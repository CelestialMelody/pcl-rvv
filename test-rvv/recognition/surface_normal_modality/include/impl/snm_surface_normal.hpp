#pragma once

/*
 * 本文件做什么：
 * 这里放 surface_normal_modality 首阶段的 scalar reference（标量参考链路）
 * 和 RVV candidate（RVV 候选链路）。两条链路都只覆盖 production
 * computeAndQuantizeSurfaceNormals2() 的核心形态：organized depth cloud（有宽高的
 * 深度点云）先转换成毫米深度，再在 5 像素半径的 8 邻域上做双边累加、法线方向量化。
 *
 * 证据边界：
 * RVV candidate 在第一版故意只返回 ScalarFallback，用于 TDD RED（先失败测试）。
 * 后续 green（通过测试）步骤会在 __RVV10__ 构建中替换为实际 RVV 路径。
 */

#include <pcl/point_types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/common.h>
#include <riscv_vector.h>
#endif

namespace pcl::recognition::rvv_test::surface_normal_modality
{
enum class ExecutionPath
{
  ScalarFallback,
  RvvDepthQuantize
};

struct NormalCell
{
  float angle_degrees = 0.0f;
  std::uint8_t quantized = 0;
};

inline void
accumulateBilateralScalar(const long delta, const long i, const long j, long* a, long* b)
{
  constexpr int difference_threshold = 50;
  const long f = std::labs(delta) < difference_threshold ? 1 : 0;
  const long fi = f * i;
  const long fj = f * j;
  a[0] += fi * i;
  a[1] += fi * j;
  a[3] += fj * j;
  b[0] += fi * delta;
  b[1] += fj * delta;
}

inline void
depthToMillimetersScalar(const pcl::PointXYZ* input,
                         const std::size_t width,
                         const std::size_t height,
                         std::vector<std::uint16_t>& depth_mm)
{
  depth_mm.assign(width * height, 0);
  for (std::size_t i = 0; i < width * height; ++i) {
    const float z = input[i].z;
    depth_mm[i] = std::isfinite(z) ? static_cast<std::uint16_t>(z * 1000.0f) : 0;
  }
}

inline void
computeDepthNormalQuantizedScalar(const pcl::PointXYZ* input,
                                  const std::size_t width,
                                  const std::size_t height,
                                  std::vector<NormalCell>& output)
{
  output.assign(width * height, {});
  std::vector<std::uint16_t> depth_mm;
  depthToMillimetersScalar(input, width, height, depth_mm);
  if (width < 12 || height < 12)
    return;

  constexpr int radius = 5;
  constexpr int distance_threshold = 2000;
  const int offsets_i[] = {-radius, 0, radius, -radius, radius, -radius, 0, radius};
  const int offsets_j[] = {-radius, -radius, -radius, 0, 0, radius, radius, radius};
  const int offsets[] = { offsets_i[0] + offsets_j[0] * static_cast<int>(width),
                          offsets_i[1] + offsets_j[1] * static_cast<int>(width),
                          offsets_i[2] + offsets_j[2] * static_cast<int>(width),
                          offsets_i[3] + offsets_j[3] * static_cast<int>(width),
                          offsets_i[4] + offsets_j[4] * static_cast<int>(width),
                          offsets_i[5] + offsets_j[5] * static_cast<int>(width),
                          offsets_i[6] + offsets_j[6] * static_cast<int>(width),
                          offsets_i[7] + offsets_j[7] * static_cast<int>(width) };

  for (int y = radius; y < static_cast<int>(height) - radius - 1; ++y) {
    const std::uint16_t* line = depth_mm.data() + y * width + radius;
    for (int x = radius; x < static_cast<int>(width) - radius - 1; ++x, ++line) {
      const long d = line[0];
      NormalCell cell;
      if (d < distance_threshold) {
        long a[4] = {0, 0, 0, 0};
        long b[2] = {0, 0};
        for (int k = 0; k < 8; ++k)
          accumulateBilateralScalar(line[offsets[k]] - d, offsets_i[k], offsets_j[k], a, b);

        const long det = a[0] * a[3] - a[1] * a[1];
        const long ddx = a[3] * b[0] - a[1] * b[1];
        const long ddy = -a[1] * b[0] + a[0] * b[1];
        float nx = static_cast<float>(1150 * ddx);
        float ny = static_cast<float>(1150 * ddy);
        float nz = static_cast<float>(-det * d);
        const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length > 0.0f) {
          const float inv_length = 1.0f / length;
          nx *= inv_length;
          ny *= inv_length;
          nz *= inv_length;
          (void)nz;

          float angle = 11.25f + std::atan2(ny, nx) * 180.0f / 3.14f;
          if (angle < 0.0f)
            angle += 360.0f;
          if (angle >= 360.0f)
            angle -= 360.0f;
          const int bin_index = static_cast<int>(angle * 8.0f / 360.0f);
          cell.angle_degrees = angle;
          cell.quantized = static_cast<std::uint8_t>(bin_index == 0 ? 1 : bin_index + 1);
        }
      }
      output[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = cell;
    }
  }
}

inline ExecutionPath
computeDepthNormalQuantizedCandidate(const pcl::PointXYZ* input,
                                     const std::size_t width,
                                     const std::size_t height,
                                     std::vector<NormalCell>& output)
{
#if defined(__RVV10__)
  output.assign(width * height, {});
  std::vector<std::uint16_t> depth_mm;
  depthToMillimetersScalar(input, width, height, depth_mm);
  if (width < 12 || height < 12)
    return ExecutionPath::RvvDepthQuantize;

  constexpr int radius = 5;
  constexpr int distance_threshold = 2000;
  const int offsets_i[] = {-radius, 0, radius, -radius, radius, -radius, 0, radius};
  const int offsets_j[] = {-radius, -radius, -radius, 0, 0, radius, radius, radius};
  const int offsets[] = { offsets_i[0] + offsets_j[0] * static_cast<int>(width),
                          offsets_i[1] + offsets_j[1] * static_cast<int>(width),
                          offsets_i[2] + offsets_j[2] * static_cast<int>(width),
                          offsets_i[3] + offsets_j[3] * static_cast<int>(width),
                          offsets_i[4] + offsets_j[4] * static_cast<int>(width),
                          offsets_i[5] + offsets_j[5] * static_cast<int>(width),
                          offsets_i[6] + offsets_j[6] * static_cast<int>(width),
                          offsets_i[7] + offsets_j[7] * static_cast<int>(width) };

  const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<std::int32_t> center_depth(max_vl);
  std::vector<std::int32_t> det_values(max_vl);
  std::vector<std::int32_t> ddx_values(max_vl);
  std::vector<std::int32_t> ddy_values(max_vl);

  const auto load_depth_i32 = [](const std::uint16_t* ptr, const std::size_t vl) {
    const vuint16m1_t u16 = __riscv_vle16_v_u16m1(ptr, vl);
    return __riscv_vreinterpret_v_u32m2_i32m2(__riscv_vzext_vf2_u32m2(u16, vl));
  };

  const auto add_neighbor = [](const vint32m2_t neighbor,
                               const vint32m2_t center,
                               const int i,
                               const int j,
                               vint32m2_t a0,
                               vint32m2_t a1,
                               vint32m2_t a3,
                               vint32m2_t b0,
                               vint32m2_t b1,
                               const std::size_t vl,
                               vint32m2_t& out_a0,
                               vint32m2_t& out_a1,
                               vint32m2_t& out_a3,
                               vint32m2_t& out_b0,
                               vint32m2_t& out_b1) {
    const vint32m2_t delta = __riscv_vsub_vv_i32m2(neighbor, center, vl);
    const vint32m2_t abs_delta = __riscv_vmax_vv_i32m2(delta, __riscv_vneg_v_i32m2(delta, vl), vl);
    const vbool16_t keep = __riscv_vmslt_vx_i32m2_b16(abs_delta, 50, vl);
    const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
    const vint32m2_t kept_delta = __riscv_vmerge_vvm_i32m2(zero, delta, keep, vl);
    const vint32m2_t fi = __riscv_vmerge_vxm_i32m2(zero, i, keep, vl);
    const vint32m2_t fj = __riscv_vmerge_vxm_i32m2(zero, j, keep, vl);

    out_a0 = __riscv_vadd_vv_i32m2(a0, __riscv_vmul_vx_i32m2(fi, i, vl), vl);
    out_a1 = __riscv_vadd_vv_i32m2(a1, __riscv_vmul_vx_i32m2(fi, j, vl), vl);
    out_a3 = __riscv_vadd_vv_i32m2(a3, __riscv_vmul_vx_i32m2(fj, j, vl), vl);
    out_b0 = __riscv_vadd_vv_i32m2(b0, __riscv_vmul_vx_i32m2(kept_delta, i, vl), vl);
    out_b1 = __riscv_vadd_vv_i32m2(b1, __riscv_vmul_vx_i32m2(kept_delta, j, vl), vl);
  };

  for (int y = radius; y < static_cast<int>(height) - radius - 1; ++y) {
    const std::size_t count = width - 2 * radius - 1;
    for (std::size_t offset = 0; offset < count;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(count - offset);
      const std::size_t x = radius + offset;
      const auto* line = depth_mm.data() + static_cast<std::size_t>(y) * width + x;

      const vint32m2_t d = load_depth_i32(line, vl);
      vint32m2_t a0 = __riscv_vmv_v_x_i32m2(0, vl);
      vint32m2_t a1 = __riscv_vmv_v_x_i32m2(0, vl);
      vint32m2_t a3 = __riscv_vmv_v_x_i32m2(0, vl);
      vint32m2_t b0 = __riscv_vmv_v_x_i32m2(0, vl);
      vint32m2_t b1 = __riscv_vmv_v_x_i32m2(0, vl);
      for (int k = 0; k < 8; ++k) {
        const vint32m2_t neighbor = load_depth_i32(line + offsets[k], vl);
        vint32m2_t next_a0;
        vint32m2_t next_a1;
        vint32m2_t next_a3;
        vint32m2_t next_b0;
        vint32m2_t next_b1;
        add_neighbor(neighbor,
                     d,
                     offsets_i[k],
                     offsets_j[k],
                     a0,
                     a1,
                     a3,
                     b0,
                     b1,
                     vl,
                     next_a0,
                     next_a1,
                     next_a3,
                     next_b0,
                     next_b1);
        a0 = next_a0;
        a1 = next_a1;
        a3 = next_a3;
        b0 = next_b0;
        b1 = next_b1;
      }

      const vint32m2_t det =
          __riscv_vsub_vv_i32m2(__riscv_vmul_vv_i32m2(a0, a3, vl),
                                __riscv_vmul_vv_i32m2(a1, a1, vl),
                                vl);
      const vint32m2_t ddx =
          __riscv_vsub_vv_i32m2(__riscv_vmul_vv_i32m2(a3, b0, vl),
                                __riscv_vmul_vv_i32m2(a1, b1, vl),
                                vl);
      const vint32m2_t ddy =
          __riscv_vadd_vv_i32m2(__riscv_vneg_v_i32m2(__riscv_vmul_vv_i32m2(a1, b0, vl), vl),
                                __riscv_vmul_vv_i32m2(a0, b1, vl),
                                vl);

      __riscv_vse32_v_i32m2(center_depth.data(), d, vl);
      __riscv_vse32_v_i32m2(det_values.data(), det, vl);
      __riscv_vse32_v_i32m2(ddx_values.data(), ddx, vl);
      __riscv_vse32_v_i32m2(ddy_values.data(), ddy, vl);

      for (std::size_t lane = 0; lane < vl; ++lane) {
        NormalCell cell;
        const long depth = center_depth[lane];
        if (depth < distance_threshold) {
          float nx = static_cast<float>(1150L * static_cast<long>(ddx_values[lane]));
          float ny = static_cast<float>(1150L * static_cast<long>(ddy_values[lane]));
          float nz = static_cast<float>(-static_cast<long>(det_values[lane]) * depth);
          const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
          if (length > 0.0f) {
            const float inv_length = 1.0f / length;
            nx *= inv_length;
            ny *= inv_length;
            nz *= inv_length;
            (void)nz;

            float angle = 11.25f + std::atan2(ny, nx) * 180.0f / 3.14f;
            if (angle < 0.0f)
              angle += 360.0f;
            if (angle >= 360.0f)
              angle -= 360.0f;
            const int bin_index = static_cast<int>(angle * 8.0f / 360.0f);
            cell.angle_degrees = angle;
            cell.quantized = static_cast<std::uint8_t>(bin_index == 0 ? 1 : bin_index + 1);
          }
        }
        output[static_cast<std::size_t>(y) * width + x + lane] = cell;
      }

      offset += vl;
    }
  }
  return ExecutionPath::RvvDepthQuantize;
#else
  computeDepthNormalQuantizedScalar(input, width, height, output);
  return ExecutionPath::ScalarFallback;
#endif
}

} // namespace pcl::recognition::rvv_test::surface_normal_modality
