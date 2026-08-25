/*
 * SHOT shape-bin component diagnostic helper.
 *
 * 本文件只服务 Phase 020 的 test-only component ablation（测试专用组件消融）。
 * 它复刻 `createBinDistanceShape` 的核心数学：normal dot frame_z、finite mask
 * （有限值掩码）、clamp（夹紧）和 shape-bin double 输出。这里使用 SoA
 * 连续输入，尚不覆盖 production 的 pcl::Normal AoS 跨步加载、indices gather
 * 或 PCL_WARN nan_counter 副作用。
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>
#include <limits>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_shot {

inline void
computeShapeBinDistanceScalar(const float* nx,
                              const float* ny,
                              const float* nz,
                              const std::size_t count,
                              const float frame_z[3],
                              const int nr_shape_bins,
                              double* out)
{
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;
  for (std::size_t i = 0; i < count; ++i) {
    if (!std::isfinite(nx[i]) || !std::isfinite(ny[i]) || !std::isfinite(nz[i])) {
      out[i] = std::numeric_limits<double>::quiet_NaN();
      continue;
    }

    double cosine_desc = static_cast<double>(nx[i]) * static_cast<double>(frame_z[0]) +
                         static_cast<double>(ny[i]) * static_cast<double>(frame_z[1]) +
                         static_cast<double>(nz[i]) * static_cast<double>(frame_z[2]);
    if (cosine_desc > 1.0)
      cosine_desc = 1.0;
    if (cosine_desc < -1.0)
      cosine_desc = -1.0;
    out[i] = (1.0 + cosine_desc) * scale;
  }
}

// 连续 AoS（结构数组）参考链路：它比 SoA helper 更接近 production 的
// `pcl::Normal` 字段布局，但仍不覆盖 production 的任意 indices gather。
inline void
computeShapeBinDistanceAoSScalar(const pcl::PointCloud<pcl::Normal>& normals,
                                 const std::size_t start,
                                 const std::size_t count,
                                 const float frame_z[3],
                                 const int nr_shape_bins,
                                 double* out)
{
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;
  for (std::size_t i = 0; i < count; ++i) {
    const pcl::Normal& normal = normals[start + i];
    if (!std::isfinite(normal.normal_x) || !std::isfinite(normal.normal_y) ||
        !std::isfinite(normal.normal_z)) {
      out[i] = std::numeric_limits<double>::quiet_NaN();
      continue;
    }

    double cosine_desc = static_cast<double>(normal.normal_x) * static_cast<double>(frame_z[0]) +
                         static_cast<double>(normal.normal_y) * static_cast<double>(frame_z[1]) +
                         static_cast<double>(normal.normal_z) * static_cast<double>(frame_z[2]);
    if (cosine_desc > 1.0)
      cosine_desc = 1.0;
    if (cosine_desc < -1.0)
      cosine_desc = -1.0;
    out[i] = (1.0 + cosine_desc) * scale;
  }
}

// Indexed gather（按索引离散加载）参考链路：保留 production 的输出顺序和
// 重复 index 语义，并返回 NaN normal 数量，供 warning side effect 边界审计。
inline unsigned
computeShapeBinDistanceIndexedScalar(const pcl::PointCloud<pcl::Normal>& normals,
                                     const pcl::Indices& indices,
                                     const float frame_z[3],
                                     const int nr_shape_bins,
                                     double* out)
{
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;
  unsigned nan_counter = 0;
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const pcl::Normal& normal = normals[static_cast<std::size_t>(indices[i])];
    if (!std::isfinite(normal.normal_x) || !std::isfinite(normal.normal_y) ||
        !std::isfinite(normal.normal_z)) {
      out[i] = std::numeric_limits<double>::quiet_NaN();
      ++nan_counter;
      continue;
    }

    double cosine_desc = static_cast<double>(normal.normal_x) * static_cast<double>(frame_z[0]) +
                         static_cast<double>(normal.normal_y) * static_cast<double>(frame_z[1]) +
                         static_cast<double>(normal.normal_z) * static_cast<double>(frame_z[2]);
    if (cosine_desc > 1.0)
      cosine_desc = 1.0;
    if (cosine_desc < -1.0)
      cosine_desc = -1.0;
    out[i] = (1.0 + cosine_desc) * scale;
  }
  return nan_counter;
}

#if defined(__RVV10__)
inline void
computeShapeBinDistanceRvvKernel(const float* nx,
                                 const float* ny,
                                 const float* nz,
                                 const std::size_t count,
                                 const float frame_z[3],
                                 const int nr_shape_bins,
                                 double* out)
{
  const float max_finite = std::numeric_limits<float>::max();
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const vfloat32m2_t vx = __riscv_vle32_v_f32m2(nx + i, vl);
    const vfloat32m2_t vy = __riscv_vle32_v_f32m2(ny + i, vl);
    const vfloat32m2_t vz = __riscv_vle32_v_f32m2(nz + i, vl);

    vbool16_t finite = __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), max_finite, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), max_finite, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), max_finite, vl), vl);

    vfloat64m4_t dot = __riscv_vfmul_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(vx, vl), frame_z[0], vl);
    dot = __riscv_vfmacc_vf_f64m4(dot, frame_z[1], __riscv_vfwcvt_f_f_v_f64m4(vy, vl), vl);
    dot = __riscv_vfmacc_vf_f64m4(dot, frame_z[2], __riscv_vfwcvt_f_f_v_f64m4(vz, vl), vl);
    dot = __riscv_vfmin_vf_f64m4(__riscv_vfmax_vf_f64m4(dot, -1.0, vl), 1.0, vl);

    vfloat64m4_t bins = __riscv_vfmul_vf_f64m4(__riscv_vfadd_vf_f64m4(dot, 1.0, vl), scale, vl);
    const vfloat64m4_t nan_values =
        __riscv_vfmv_v_f_f64m4(std::numeric_limits<double>::quiet_NaN(), vl);
    bins = __riscv_vmerge_vvm_f64m4(nan_values, bins, finite, vl);
    __riscv_vse64_v_f64m4(out + i, bins, vl);
    i += vl;
  }
}

inline void
computeShapeBinDistanceAoSRvvKernel(const pcl::PointCloud<pcl::Normal>& normals,
                                    const std::size_t start,
                                    const std::size_t count,
                                    const float frame_z[3],
                                    const int nr_shape_bins,
                                    double* out)
{
  const float max_finite = std::numeric_limits<float>::max();
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(pcl::Normal));

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const pcl::Normal* base = normals.points.data() + start + i;
    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(&base->normal_x, stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(&base->normal_y, stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(&base->normal_z, stride, vl);

    vbool16_t finite = __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), max_finite, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), max_finite, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), max_finite, vl), vl);

    vfloat64m4_t dot = __riscv_vfmul_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(vx, vl), frame_z[0], vl);
    dot = __riscv_vfmacc_vf_f64m4(dot, frame_z[1], __riscv_vfwcvt_f_f_v_f64m4(vy, vl), vl);
    dot = __riscv_vfmacc_vf_f64m4(dot, frame_z[2], __riscv_vfwcvt_f_f_v_f64m4(vz, vl), vl);
    dot = __riscv_vfmin_vf_f64m4(__riscv_vfmax_vf_f64m4(dot, -1.0, vl), 1.0, vl);

    vfloat64m4_t bins = __riscv_vfmul_vf_f64m4(__riscv_vfadd_vf_f64m4(dot, 1.0, vl), scale, vl);
    const vfloat64m4_t nan_values =
        __riscv_vfmv_v_f_f64m4(std::numeric_limits<double>::quiet_NaN(), vl);
    bins = __riscv_vmerge_vvm_f64m4(nan_values, bins, finite, vl);
    __riscv_vse64_v_f64m4(out + i, bins, vl);
    i += vl;
  }
}

inline unsigned
computeShapeBinDistanceIndexedRvvKernel(const pcl::PointCloud<pcl::Normal>& normals,
                                        const pcl::Indices& indices,
                                        const float frame_z[3],
                                        const int nr_shape_bins,
                                        double* out)
{
  const auto* base = reinterpret_cast<const std::uint8_t*>(normals.points.data());
  const float max_finite = std::numeric_limits<float>::max();
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;
  unsigned nan_counter = 0;
  alignas(16) std::array<std::uint32_t, 256> offset_buf{};

  for (std::size_t i = 0; i < indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    for (std::size_t lane = 0; lane < vl; ++lane)
      offset_buf[lane] =
          static_cast<std::uint32_t>(indices[i + lane]) * static_cast<std::uint32_t>(sizeof(pcl::Normal));

    const vuint32m2_t offsets = __riscv_vle32_v_u32m2(offset_buf.data(), vl);
    const vfloat32m2_t vx = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(base + offsetof(pcl::Normal, normal_x)), offsets, vl);
    const vfloat32m2_t vy = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(base + offsetof(pcl::Normal, normal_y)), offsets, vl);
    const vfloat32m2_t vz = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(base + offsetof(pcl::Normal, normal_z)), offsets, vl);

    vbool16_t finite = __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), max_finite, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), max_finite, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), max_finite, vl), vl);
    nan_counter += static_cast<unsigned>(vl - __riscv_vcpop_m_b16(finite, vl));

    vfloat64m4_t dot = __riscv_vfmul_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(vx, vl), frame_z[0], vl);
    dot = __riscv_vfmacc_vf_f64m4(dot, frame_z[1], __riscv_vfwcvt_f_f_v_f64m4(vy, vl), vl);
    dot = __riscv_vfmacc_vf_f64m4(dot, frame_z[2], __riscv_vfwcvt_f_f_v_f64m4(vz, vl), vl);
    dot = __riscv_vfmin_vf_f64m4(__riscv_vfmax_vf_f64m4(dot, -1.0, vl), 1.0, vl);

    vfloat64m4_t bins = __riscv_vfmul_vf_f64m4(__riscv_vfadd_vf_f64m4(dot, 1.0, vl), scale, vl);
    const vfloat64m4_t nan_values =
        __riscv_vfmv_v_f_f64m4(std::numeric_limits<double>::quiet_NaN(), vl);
    bins = __riscv_vmerge_vvm_f64m4(nan_values, bins, finite, vl);
    __riscv_vse64_v_f64m4(out + i, bins, vl);
    i += vl;
  }
  return nan_counter;
}
#endif

inline void
computeShapeBinDistanceRVV(const float* nx,
                           const float* ny,
                           const float* nz,
                           const std::size_t count,
                           const float frame_z[3],
                           const int nr_shape_bins,
                           double* out)
{
#if defined(__RVV10__)
  computeShapeBinDistanceRvvKernel(nx, ny, nz, count, frame_z, nr_shape_bins, out);
#else
  computeShapeBinDistanceScalar(nx, ny, nz, count, frame_z, nr_shape_bins, out);
#endif
}

inline void
computeShapeBinDistanceAoSRVV(const pcl::PointCloud<pcl::Normal>& normals,
                              const std::size_t start,
                              const std::size_t count,
                              const float frame_z[3],
                              const int nr_shape_bins,
                              double* out)
{
#if defined(__RVV10__)
  computeShapeBinDistanceAoSRvvKernel(normals, start, count, frame_z, nr_shape_bins, out);
#else
  computeShapeBinDistanceAoSScalar(normals, start, count, frame_z, nr_shape_bins, out);
#endif
}

inline unsigned
computeShapeBinDistanceIndexedRVV(const pcl::PointCloud<pcl::Normal>& normals,
                                  const pcl::Indices& indices,
                                  const float frame_z[3],
                                  const int nr_shape_bins,
                                  double* out)
{
#if defined(__RVV10__)
  return computeShapeBinDistanceIndexedRvvKernel(normals, indices, frame_z, nr_shape_bins, out);
#else
  return computeShapeBinDistanceIndexedScalar(normals, indices, frame_z, nr_shape_bins, out);
#endif
}

} // namespace pcl_rvv_shot
