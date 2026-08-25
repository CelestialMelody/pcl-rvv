/*
 * SHOT interpolation geometry staging diagnostic helper.
 *
 * 本文件只服务 Phase 050 的 test-only component ablation（测试专用组件消融）。
 * 它复刻 `interpolateSingleChannel` / `interpolateDoubleChannel` 开头的 indexed
 * surface gather（按索引读取 surface 点）、中心点差值、local reference frame
 * 三轴投影和 distance staging（距离暂存）。它不更新 histogram，也不覆盖
 * `acos` / `atan2` 后续插值语义。
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_shot {

struct InterpolationBinSelectionLane {
  std::int32_t desc_index;
  std::int32_t step_index;
  std::int32_t adjacent_index;
  std::uint8_t valid;
};

inline double
zeroTinyInterpolationComponent(const double value)
{
  return std::abs(value) < 1e-30 ? 0.0 : value;
}

inline bool
isValidInterpolationBinSelectionLane(const double distance, const double bin_distance)
{
  return std::isfinite(bin_distance) && std::isfinite(distance) && std::abs(distance) >= 1e-15;
}

inline InterpolationBinSelectionLane
selectInterpolationBinScalarLane(const double x_input,
                                 const double y_input,
                                 const double z_input,
                                 const double distance,
                                 const double bin_distance,
                                 const int nr_bins,
                                 const double radius1_2)
{
  if (!isValidInterpolationBinSelectionLane(distance, bin_distance))
    return {-1, -1, -1, 0u};

  const double x = zeroTinyInterpolationComponent(x_input);
  const double y = zeroTinyInterpolationComponent(y_input);
  const double z = zeroTinyInterpolationComponent(z_input);

  const unsigned char bit4 = ((y > 0.0) || ((y == 0.0) && (x < 0.0))) ? 1u : 0u;
  const bool bit3_condition = (x > 0.0) || ((x == 0.0) && (y > 0.0));
  const auto bit3 = static_cast<unsigned char>(bit3_condition ? !bit4 : bit4);

  int desc_index = (static_cast<int>(bit4) << 3) + (static_cast<int>(bit3) << 2);
  desc_index = desc_index << 1;

  if ((x * y > 0.0) || (x == 0.0))
    desc_index += (std::abs(x) >= std::abs(y)) ? 0 : 4;
  else
    desc_index += (std::abs(x) > std::abs(y)) ? 4 : 0;

  desc_index += z > 0.0 ? 1 : 0;
  desc_index += (distance > radius1_2) ? 2 : 0;

  const int step_index = static_cast<int>(std::floor(bin_distance + 0.5));
  const int volume_index = desc_index * (nr_bins + 1);
  const double residual = bin_distance - static_cast<double>(step_index);
  const int adjacent_bin =
      residual > 0.0 ? ((step_index + 1) % nr_bins) : ((step_index - 1 + nr_bins) % nr_bins);

  return {desc_index, step_index, volume_index + adjacent_bin, 1u};
}

inline void
computeInterpolationBinSelectionScalar(const double* x,
                                       const double* y,
                                       const double* z,
                                       const double* distance,
                                       const double* bin_distance,
                                       const std::size_t count,
                                       const int nr_bins,
                                       const double radius1_2,
                                       std::int32_t* out_desc_index,
                                       std::int32_t* out_step_index,
                                       std::int32_t* out_adjacent_index,
                                       float* out_adjacent_delta,
                                       float* out_center_weight,
                                       std::uint8_t* out_valid)
{
  for (std::size_t i = 0; i < count; ++i) {
    const InterpolationBinSelectionLane lane =
        selectInterpolationBinScalarLane(x[i], y[i], z[i], distance[i], bin_distance[i], nr_bins, radius1_2);
    out_desc_index[i] = lane.desc_index;
    out_step_index[i] = lane.step_index;
    out_adjacent_index[i] = lane.adjacent_index;
    out_valid[i] = lane.valid;

    if (lane.valid == 0u) {
      out_adjacent_delta[i] = 0.0f;
      out_center_weight[i] = 0.0f;
      continue;
    }

    const double residual = bin_distance[i] - static_cast<double>(lane.step_index);
    out_adjacent_delta[i] = static_cast<float>(std::abs(residual));
    out_center_weight[i] = static_cast<float>(1.0 - std::abs(residual));
  }
}

#if defined(__RVV10__)
inline void
computeInterpolationBinSelectionRvvKernel(const double* x,
                                          const double* y,
                                          const double* z,
                                          const double* distance,
                                          const double* bin_distance,
                                          const std::size_t count,
                                          const int nr_bins,
                                          const double radius1_2,
                                          std::int32_t* out_desc_index,
                                          std::int32_t* out_step_index,
                                          std::int32_t* out_adjacent_index,
                                          float* out_adjacent_delta,
                                          float* out_center_weight,
                                          std::uint8_t* out_valid)
{
  alignas(16) std::array<double, 256> bin_buf{};
  alignas(16) std::array<double, 256> step_buf{};
  alignas(16) std::array<double, 256> delta_buf{};
  alignas(16) std::array<double, 256> center_buf{};

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e64m4(count - i);
    for (std::size_t lane_index = 0; lane_index < vl; ++lane_index) {
      const std::size_t offset = i + lane_index;
      const InterpolationBinSelectionLane lane = selectInterpolationBinScalarLane(x[offset],
                                                                                  y[offset],
                                                                                  z[offset],
                                                                                  distance[offset],
                                                                                  bin_distance[offset],
                                                                                  nr_bins,
                                                                                  radius1_2);
      out_desc_index[offset] = lane.desc_index;
      out_step_index[offset] = lane.step_index;
      out_adjacent_index[offset] = lane.adjacent_index;
      out_valid[offset] = lane.valid;
      bin_buf[lane_index] = lane.valid != 0u ? bin_distance[offset] : 0.0;
      step_buf[lane_index] = lane.valid != 0u ? static_cast<double>(lane.step_index) : 0.0;
    }

    const vfloat64m4_t bin = __riscv_vle64_v_f64m4(bin_buf.data(), vl);
    const vfloat64m4_t step = __riscv_vle64_v_f64m4(step_buf.data(), vl);
    const vfloat64m4_t residual = __riscv_vfsub_vv_f64m4(bin, step, vl);
    const vfloat64m4_t delta = __riscv_vfabs_v_f64m4(residual, vl);
    const vfloat64m4_t center = __riscv_vfrsub_vf_f64m4(delta, 1.0, vl);

    __riscv_vse64_v_f64m4(delta_buf.data(), delta, vl);
    __riscv_vse64_v_f64m4(center_buf.data(), center, vl);

    for (std::size_t lane_index = 0; lane_index < vl; ++lane_index) {
      const std::size_t offset = i + lane_index;
      if (out_valid[offset] == 0u) {
        out_adjacent_delta[offset] = 0.0f;
        out_center_weight[offset] = 0.0f;
      } else {
        out_adjacent_delta[offset] = static_cast<float>(delta_buf[lane_index]);
        out_center_weight[offset] = static_cast<float>(center_buf[lane_index]);
      }
    }
    i += vl;
  }
}
#endif

inline void
computeInterpolationBinSelectionRVV(const double* x,
                                    const double* y,
                                    const double* z,
                                    const double* distance,
                                    const double* bin_distance,
                                    const std::size_t count,
                                    const int nr_bins,
                                    const double radius1_2,
                                    std::int32_t* out_desc_index,
                                    std::int32_t* out_step_index,
                                    std::int32_t* out_adjacent_index,
                                    float* out_adjacent_delta,
                                    float* out_center_weight,
                                    std::uint8_t* out_valid)
{
#if defined(__RVV10__)
  computeInterpolationBinSelectionRvvKernel(x,
                                            y,
                                            z,
                                            distance,
                                            bin_distance,
                                            count,
                                            nr_bins,
                                            radius1_2,
                                            out_desc_index,
                                            out_step_index,
                                            out_adjacent_index,
                                            out_adjacent_delta,
                                            out_center_weight,
                                            out_valid);
#else
  computeInterpolationBinSelectionScalar(x,
                                         y,
                                         z,
                                         distance,
                                         bin_distance,
                                         count,
                                         nr_bins,
                                         radius1_2,
                                         out_desc_index,
                                         out_step_index,
                                         out_adjacent_index,
                                         out_adjacent_delta,
                                         out_center_weight,
                                         out_valid);
#endif
}

inline void
computeInterpolationGeometryIndexedScalar(const pcl::PointCloud<pcl::PointXYZ>& surface,
                                          const pcl::Indices& indices,
                                          const float* sqr_dists,
                                          const double* bin_distance,
                                          const float central[3],
                                          const float frame_x[3],
                                          const float frame_y[3],
                                          const float frame_z[3],
                                          double* out_x,
                                          double* out_y,
                                          double* out_z,
                                          double* out_distance,
                                          std::uint8_t* out_valid)
{
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const pcl::PointXYZ& point = surface[static_cast<std::size_t>(indices[i])];
    const double dx = static_cast<double>(point.x) - static_cast<double>(central[0]);
    const double dy = static_cast<double>(point.y) - static_cast<double>(central[1]);
    const double dz = static_cast<double>(point.z) - static_cast<double>(central[2]);

    double x = dx * static_cast<double>(frame_x[0]) + dy * static_cast<double>(frame_x[1]) +
               dz * static_cast<double>(frame_x[2]);
    double y = dx * static_cast<double>(frame_y[0]) + dy * static_cast<double>(frame_y[1]) +
               dz * static_cast<double>(frame_y[2]);
    double z = dx * static_cast<double>(frame_z[0]) + dy * static_cast<double>(frame_z[1]) +
               dz * static_cast<double>(frame_z[2]);

    if (std::abs(x) < 1e-30)
      x = 0.0;
    if (std::abs(y) < 1e-30)
      y = 0.0;
    if (std::abs(z) < 1e-30)
      z = 0.0;

    const double distance = std::sqrt(static_cast<double>(sqr_dists[i]));
    out_x[i] = x;
    out_y[i] = y;
    out_z[i] = z;
    out_distance[i] = distance;
    out_valid[i] =
        (std::isfinite(bin_distance[i]) && !std::isnan(distance) && distance != 0.0) ? 1u : 0u;
  }
}

#if defined(__RVV10__)
inline void
computeInterpolationGeometryIndexedRvvKernel(const pcl::PointCloud<pcl::PointXYZ>& surface,
                                             const pcl::Indices& indices,
                                             const float* sqr_dists,
                                             const double* bin_distance,
                                             const float central[3],
                                             const float frame_x[3],
                                             const float frame_y[3],
                                             const float frame_z[3],
                                             double* out_x,
                                             double* out_y,
                                             double* out_z,
                                             double* out_distance,
                                             std::uint8_t* out_valid)
{
  const auto* base = reinterpret_cast<const std::uint8_t*>(surface.points.data());
  alignas(16) std::array<std::uint32_t, 256> offset_buf{};
  const vfloat64m4_t zeros_template = __riscv_vfmv_v_f_f64m4(0.0, __riscv_vsetvlmax_e64m4());

  for (std::size_t i = 0; i < indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      offset_buf[lane] =
          static_cast<std::uint32_t>(indices[i + lane]) * static_cast<std::uint32_t>(sizeof(pcl::PointXYZ));
      out_valid[i + lane] = (std::isfinite(bin_distance[i + lane]) && sqr_dists[i + lane] > 0.0f) ? 1u : 0u;
    }

    const vuint32m2_t offsets = __riscv_vle32_v_u32m2(offset_buf.data(), vl);
    const vfloat32m2_t px = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, x)), offsets, vl);
    const vfloat32m2_t py = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, y)), offsets, vl);
    const vfloat32m2_t pz = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, z)), offsets, vl);

    const vfloat64m4_t dx =
        __riscv_vfsub_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(px, vl), central[0], vl);
    const vfloat64m4_t dy =
        __riscv_vfsub_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(py, vl), central[1], vl);
    const vfloat64m4_t dz =
        __riscv_vfsub_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(pz, vl), central[2], vl);

    vfloat64m4_t x = __riscv_vfmul_vf_f64m4(dx, frame_x[0], vl);
    x = __riscv_vfmacc_vf_f64m4(x, frame_x[1], dy, vl);
    x = __riscv_vfmacc_vf_f64m4(x, frame_x[2], dz, vl);

    vfloat64m4_t y = __riscv_vfmul_vf_f64m4(dx, frame_y[0], vl);
    y = __riscv_vfmacc_vf_f64m4(y, frame_y[1], dy, vl);
    y = __riscv_vfmacc_vf_f64m4(y, frame_y[2], dz, vl);

    vfloat64m4_t z = __riscv_vfmul_vf_f64m4(dx, frame_z[0], vl);
    z = __riscv_vfmacc_vf_f64m4(z, frame_z[1], dy, vl);
    z = __riscv_vfmacc_vf_f64m4(z, frame_z[2], dz, vl);

    const vbool16_t small_x = __riscv_vmflt_vf_f64m4_b16(__riscv_vfabs_v_f64m4(x, vl), 1e-30, vl);
    const vbool16_t small_y = __riscv_vmflt_vf_f64m4_b16(__riscv_vfabs_v_f64m4(y, vl), 1e-30, vl);
    const vbool16_t small_z = __riscv_vmflt_vf_f64m4_b16(__riscv_vfabs_v_f64m4(z, vl), 1e-30, vl);
    x = __riscv_vmerge_vvm_f64m4(x, zeros_template, small_x, vl);
    y = __riscv_vmerge_vvm_f64m4(y, zeros_template, small_y, vl);
    z = __riscv_vmerge_vvm_f64m4(z, zeros_template, small_z, vl);

    const vfloat64m4_t distance =
        __riscv_vfsqrt_v_f64m4(__riscv_vfwcvt_f_f_v_f64m4(__riscv_vle32_v_f32m2(sqr_dists + i, vl), vl), vl);

    __riscv_vse64_v_f64m4(out_x + i, x, vl);
    __riscv_vse64_v_f64m4(out_y + i, y, vl);
    __riscv_vse64_v_f64m4(out_z + i, z, vl);
    __riscv_vse64_v_f64m4(out_distance + i, distance, vl);
    i += vl;
  }
}
#endif

inline void
computeInterpolationGeometryIndexedRVV(const pcl::PointCloud<pcl::PointXYZ>& surface,
                                       const pcl::Indices& indices,
                                       const float* sqr_dists,
                                       const double* bin_distance,
                                       const float central[3],
                                       const float frame_x[3],
                                       const float frame_y[3],
                                       const float frame_z[3],
                                       double* out_x,
                                       double* out_y,
                                       double* out_z,
                                       double* out_distance,
                                       std::uint8_t* out_valid)
{
#if defined(__RVV10__)
  computeInterpolationGeometryIndexedRvvKernel(surface,
                                               indices,
                                               sqr_dists,
                                               bin_distance,
                                               central,
                                               frame_x,
                                               frame_y,
                                               frame_z,
                                               out_x,
                                               out_y,
                                               out_z,
                                               out_distance,
                                               out_valid);
#else
  computeInterpolationGeometryIndexedScalar(surface,
                                            indices,
                                            sqr_dists,
                                            bin_distance,
                                            central,
                                            frame_x,
                                            frame_y,
                                            frame_z,
                                            out_x,
                                            out_y,
                                            out_z,
                                            out_distance,
                                            out_valid);
#endif
}

} // namespace pcl_rvv_shot
