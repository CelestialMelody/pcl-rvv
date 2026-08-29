#pragma once

/*
 * 本文件做什么：
 * 这里把 TrajkovicKeypoint3D 的 FOUR_CORNERS / EIGHT_CORNERS response map
 *（四邻域 / 八邻域响应图）拆成测试专用 helper。Std helper（标量辅助函数）
 * 复刻 production 公式；RVV helper（RISC-V 向量辅助函数）只替换逐像素 normal
 * stencil（法线模板）计算，供 correctness（正确性）、反汇编和板卡诊断使用。
 *
 * 证据边界：
 * 本文件不接入 production dispatch（生产分流），也不覆盖 normal estimation
 * 或 non-max suppression（非极大值抑制）。后续若进入 production integration
 * loop（生产接入闭环），必须在真实 `detectKeypoints()` 入口重新验证。
 */

#include <pcl/common/point_tests.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::keypoints::rvv_test::trajkovic_3d
{
struct ResponseConfig
{
  std::size_t width = 0;
  std::size_t height = 0;
  int half_window = 1;
  float first_threshold = 0.0f;
};

inline bool
finitePoint(const pcl::PointXYZ& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool
finiteNormal(const pcl::Normal& normal)
{
  return std::isfinite(normal.normal_x) && std::isfinite(normal.normal_y) && std::isfinite(normal.normal_z);
}

inline const pcl::Normal&
normalOrNull(const pcl::Normal* normals, std::size_t index, int& count)
{
  static const pcl::Normal null_normal;
  const pcl::Normal& normal = normals[index];
  if (!finiteNormal(normal))
    return null_normal;
  ++count;
  return normal;
}

inline float
normalsDiff(const pcl::Normal& a, const pcl::Normal& b)
{
  const double nx = a.normal_x;
  const double ny = a.normal_y;
  const double nz = a.normal_z;
  const double mx = b.normal_x;
  const double my = b.normal_y;
  const double mz = b.normal_z;
  return static_cast<float>(1.0 - (nx * mx + ny * my + nz * mz));
}

inline float
squaredNormalsDiff(const pcl::Normal& a, const pcl::Normal& b)
{
  const float diff = normalsDiff(a, b);
  return diff * diff;
}

inline float
fourCornersResponseScalarAt(const pcl::PointXYZ* points,
                            const pcl::Normal* normals,
                            const ResponseConfig& config,
                            std::size_t row,
                            std::size_t col)
{
  const std::size_t center_index = row * config.width + col;
  if (!finitePoint(points[center_index]))
    return 0.0f;

  const pcl::Normal& center = normals[center_index];
  if (!finiteNormal(center))
    return 0.0f;

  int count = 0;
  const auto step = static_cast<std::size_t>(config.half_window);
  const pcl::Normal& up = normalOrNull(normals, (row - step) * config.width + col, count);
  const pcl::Normal& down = normalOrNull(normals, (row + step) * config.width + col, count);
  const pcl::Normal& left = normalOrNull(normals, row * config.width + (col - step), count);
  const pcl::Normal& right = normalOrNull(normals, row * config.width + (col + step), count);
  if (!count)
    return 0.0f;

  float sn1 = squaredNormalsDiff(up, center);
  float sn2 = squaredNormalsDiff(down, center);
  const float r1 = sn1 + sn2;
  const float r2 = squaredNormalsDiff(right, center) + squaredNormalsDiff(left, center);
  const float d = std::min(r1, r2);
  if (d < config.first_threshold)
    return 0.0f;

  sn1 = std::sqrt(sn1);
  sn2 = std::sqrt(sn2);
  float b1 = normalsDiff(right, up) * sn1;
  b1 += normalsDiff(left, down) * sn2;
  float b2 = normalsDiff(right, down) * sn2;
  b2 += normalsDiff(left, up) * sn1;
  const float b = std::min(b1, b2);
  const float a = r2 - r1 - 2.0f * b;

  return ((b < 0.0f) && ((b + a) > 0.0f)) ? r1 - ((b * b) / a) : d;
}

inline float
eightCornersResponseScalarAt(const pcl::PointXYZ* points,
                             const pcl::Normal* normals,
                             const ResponseConfig& config,
                             std::size_t row,
                             std::size_t col)
{
  const std::size_t center_index = row * config.width + col;
  if (!finitePoint(points[center_index]))
    return 0.0f;

  const pcl::Normal& center = normals[center_index];
  if (!finiteNormal(center))
    return 0.0f;

  int count = 0;
  const auto step = static_cast<std::size_t>(config.half_window);
  const pcl::Normal& up = normalOrNull(normals, (row - step) * config.width + col, count);
  const pcl::Normal& down = normalOrNull(normals, (row + step) * config.width + col, count);
  const pcl::Normal& left = normalOrNull(normals, row * config.width + (col - step), count);
  const pcl::Normal& right = normalOrNull(normals, row * config.width + (col + step), count);
  const pcl::Normal& upleft = normalOrNull(normals, (row - step) * config.width + (col - step), count);
  const pcl::Normal& upright = normalOrNull(normals, (row - step) * config.width + (col + step), count);
  const pcl::Normal& downleft = normalOrNull(normals, (row + step) * config.width + (col - step), count);
  const pcl::Normal& downright = normalOrNull(normals, (row + step) * config.width + (col + step), count);
  if (!count)
    return 0.0f;

  const float up_center = normalsDiff(up, center);
  const float down_center = normalsDiff(down, center);
  const float left_center = normalsDiff(left, center);
  const float right_center = normalsDiff(right, center);
  const float upright_center = normalsDiff(upright, center);
  const float downleft_center = normalsDiff(downleft, center);
  const float downright_center = normalsDiff(downright, center);
  const float upleft_center = normalsDiff(upleft, center);

  const float r0 = squaredNormalsDiff(up, center) + squaredNormalsDiff(down, center);
  const float r1 = squaredNormalsDiff(upright, center) + squaredNormalsDiff(downleft, center);
  const float r2 = squaredNormalsDiff(right, center) + squaredNormalsDiff(left, center);
  const float r3 = squaredNormalsDiff(downright, center) + squaredNormalsDiff(upleft, center);
  const float d = std::min(std::min(r0, r1), std::min(r2, r3));
  if (d < config.first_threshold)
    return 0.0f;

  float b[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float a[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float sum_ab[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  b[0] = normalsDiff(upright, up) * up_center;
  b[0] += normalsDiff(downleft, down) * down_center;
  b[1] = normalsDiff(right, upright) * upright_center;
  b[1] += normalsDiff(left, downleft) * downleft_center;
  b[2] = normalsDiff(downright, right) * downright_center;
  b[2] += normalsDiff(upleft, left) * upleft_center;
  b[3] = normalsDiff(down, downright) * downright_center;
  b[3] += normalsDiff(up, upleft) * upleft_center;
  a[0] = r1 - r0 - b[0] - b[0];
  a[1] = r2 - r1 - b[1] - b[1];
  a[2] = r3 - r2 - b[2] - b[2];
  a[3] = r0 - r3 - b[3] - b[3];
  sum_ab[0] = a[0] + b[0];
  sum_ab[1] = a[1] + b[1];
  sum_ab[2] = a[2] + b[2];
  sum_ab[3] = a[3] + b[3];
  if ((*std::max_element(std::begin(b), std::end(b)) < 0.0f) &&
      (*std::min_element(std::begin(sum_ab), std::end(sum_ab)) > 0.0f))
  {
    const float d_values[4] = {
        b[0] * b[0] / a[0], b[1] * b[1] / a[1], b[2] * b[2] / a[2], b[3] * b[3] / a[3]};
    return *std::min_element(std::begin(d_values), std::end(d_values));
  }
  return d;
}

inline void
clearResponse(float* response, std::size_t size)
{
  std::fill(response, response + size, 0.0f);
}

inline void
computeFourCornersResponseStd(const pcl::PointXYZ* points,
                              const pcl::Normal* normals,
                              const ResponseConfig& config,
                              float* response)
{
  clearResponse(response, config.width * config.height);
  if (config.width == 0 || config.height == 0 || config.half_window <= 0)
    return;

  const auto step = static_cast<std::size_t>(config.half_window);
  if (config.width <= 2 * step || config.height <= 2 * step)
    return;

  for (std::size_t row = step; row < config.height - step; ++row)
  {
    for (std::size_t col = step; col < config.width - step; ++col)
      response[row * config.width + col] = fourCornersResponseScalarAt(points, normals, config, row, col);
  }
}

inline void
computeEightCornersResponseStd(const pcl::PointXYZ* points,
                               const pcl::Normal* normals,
                               const ResponseConfig& config,
                               float* response)
{
  clearResponse(response, config.width * config.height);
  if (config.width == 0 || config.height == 0 || config.half_window <= 0)
    return;

  const auto step = static_cast<std::size_t>(config.half_window);
  if (config.width <= 2 * step || config.height <= 2 * step)
    return;

  for (std::size_t row = step; row < config.height - step; ++row)
  {
    for (std::size_t col = step; col < config.width - step; ++col)
      response[row * config.width + col] = eightCornersResponseScalarAt(points, normals, config, row, col);
  }
}

#if defined(__RVV10__)
#define PCL_TRAJKOVIC_3D_RVV_NOINLINE __attribute__((noinline))

inline vbool32_t
finiteF32(vfloat32m1_t value, std::size_t vl)
{
  vbool32_t finite = __riscv_vmfeq_vv_f32m1_b32(value, value, vl);
  finite = __riscv_vmand_mm_b32(
      finite,
      __riscv_vmflt_vf_f32m1_b32(__riscv_vfabs_v_f32m1(value, vl), std::numeric_limits<float>::infinity(), vl),
      vl);
  return finite;
}

inline vbool32_t
pointFiniteMask(vfloat32m1_t x, vfloat32m1_t y, vfloat32m1_t z, std::size_t vl)
{
  vbool32_t finite = finiteF32(x, vl);
  finite = __riscv_vmand_mm_b32(finite, finiteF32(y, vl), vl);
  finite = __riscv_vmand_mm_b32(finite, finiteF32(z, vl), vl);
  return finite;
}

inline vbool32_t
normalFiniteMask(vfloat32m1_t nx, vfloat32m1_t ny, vfloat32m1_t nz, std::size_t vl)
{
  return pointFiniteMask(nx, ny, nz, vl);
}

inline void
applyNullNormalForInvalidMask(vbool32_t finite,
                              std::size_t vl,
                              vfloat32m1_t& nx,
                              vfloat32m1_t& ny,
                              vfloat32m1_t& nz)
{
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, finite, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, finite, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, finite, vl);
}

inline vfloat32m1_t
normalDiff(vfloat32m1_t ax,
           vfloat32m1_t ay,
           vfloat32m1_t az,
           vfloat32m1_t bx,
           vfloat32m1_t by,
           vfloat32m1_t bz,
           std::size_t vl)
{
  vfloat32m1_t dot = __riscv_vfmul_vv_f32m1(ax, bx, vl);
  dot = __riscv_vfmacc_vv_f32m1(dot, ay, by, vl);
  dot = __riscv_vfmacc_vv_f32m1(dot, az, bz, vl);
  return __riscv_vfrsub_vf_f32m1(dot, 1.0f, vl);
}

inline void
loadNormalFields(const pcl::Normal* base,
                 std::size_t offset,
                 std::size_t vl,
                 vfloat32m1_t& nx,
                 vfloat32m1_t& ny,
                 vfloat32m1_t& nz)
{
  static_assert(std::is_standard_layout_v<pcl::Normal>, "pcl::Normal must support field-offset RVV loads");
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(base + offset);
  nx = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + offsetof(pcl::Normal, normal_x)),
                              static_cast<std::ptrdiff_t>(sizeof(pcl::Normal)),
                              vl);
  ny = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + offsetof(pcl::Normal, normal_y)),
                              static_cast<std::ptrdiff_t>(sizeof(pcl::Normal)),
                              vl);
  nz = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + offsetof(pcl::Normal, normal_z)),
                              static_cast<std::ptrdiff_t>(sizeof(pcl::Normal)),
                              vl);
}

inline void
loadPointFields(const pcl::PointXYZ* base,
                std::size_t offset,
                std::size_t vl,
                vfloat32m1_t& x,
                vfloat32m1_t& y,
                vfloat32m1_t& z)
{
  static_assert(std::is_standard_layout_v<pcl::PointXYZ>, "pcl::PointXYZ must support field-offset RVV loads");
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(base + offset);
  x = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + offsetof(pcl::PointXYZ, x)),
                             static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ)),
                             vl);
  y = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + offsetof(pcl::PointXYZ, y)),
                             static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ)),
                             vl);
  z = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + offsetof(pcl::PointXYZ, z)),
                             static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ)),
                             vl);
}

PCL_TRAJKOVIC_3D_RVV_NOINLINE inline void
computeFourCornersResponseRVV(const pcl::PointXYZ* points,
                              const pcl::Normal* normals,
                              const ResponseConfig& config,
                              float* response)
{
  clearResponse(response, config.width * config.height);
  if (config.width == 0 || config.height == 0 || config.half_window <= 0)
    return;

  const auto step = static_cast<std::size_t>(config.half_window);
  if (config.width <= 2 * step || config.height <= 2 * step)
    return;

  for (std::size_t row = step; row < config.height - step; ++row)
  {
    std::size_t col = step;
    for (; col < config.width - step;)
    {
      const std::size_t available = config.width - step - col;
      const std::size_t vl = __riscv_vsetvl_e32m1(available);
      const std::size_t center = row * config.width + col;
      const std::size_t up = (row - step) * config.width + col;
      const std::size_t down = (row + step) * config.width + col;
      const std::size_t left = row * config.width + (col - step);
      const std::size_t right = row * config.width + (col + step);

      vfloat32m1_t cx;
      vfloat32m1_t cy;
      vfloat32m1_t cz;
      vfloat32m1_t ux;
      vfloat32m1_t uy;
      vfloat32m1_t uz;
      vfloat32m1_t dx;
      vfloat32m1_t dy;
      vfloat32m1_t dz;
      vfloat32m1_t lx;
      vfloat32m1_t ly;
      vfloat32m1_t lz;
      vfloat32m1_t rx;
      vfloat32m1_t ry;
      vfloat32m1_t rz;
      vfloat32m1_t px;
      vfloat32m1_t py;
      vfloat32m1_t pz;
      loadPointFields(points, center, vl, px, py, pz);
      loadNormalFields(normals, center, vl, cx, cy, cz);
      loadNormalFields(normals, up, vl, ux, uy, uz);
      loadNormalFields(normals, down, vl, dx, dy, dz);
      loadNormalFields(normals, left, vl, lx, ly, lz);
      loadNormalFields(normals, right, vl, rx, ry, rz);

      const vbool32_t up_finite = normalFiniteMask(ux, uy, uz, vl);
      const vbool32_t down_finite = normalFiniteMask(dx, dy, dz, vl);
      const vbool32_t left_finite = normalFiniteMask(lx, ly, lz, vl);
      const vbool32_t right_finite = normalFiniteMask(rx, ry, rz, vl);
      applyNullNormalForInvalidMask(up_finite, vl, ux, uy, uz);
      applyNullNormalForInvalidMask(down_finite, vl, dx, dy, dz);
      applyNullNormalForInvalidMask(left_finite, vl, lx, ly, lz);
      applyNullNormalForInvalidMask(right_finite, vl, rx, ry, rz);

      vbool32_t active = __riscv_vmand_mm_b32(pointFiniteMask(px, py, pz, vl), normalFiniteMask(cx, cy, cz, vl), vl);
      vbool32_t neighbor_any = up_finite;
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, down_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, left_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, right_finite, vl);
      active = __riscv_vmand_mm_b32(active, neighbor_any, vl);

      const vfloat32m1_t up_diff = normalDiff(ux, uy, uz, cx, cy, cz, vl);
      const vfloat32m1_t down_diff = normalDiff(dx, dy, dz, cx, cy, cz, vl);
      const vfloat32m1_t right_diff = normalDiff(rx, ry, rz, cx, cy, cz, vl);
      const vfloat32m1_t left_diff = normalDiff(lx, ly, lz, cx, cy, cz, vl);
      const vfloat32m1_t sn1 = __riscv_vfmul_vv_f32m1(up_diff, up_diff, vl);
      const vfloat32m1_t sn2 = __riscv_vfmul_vv_f32m1(down_diff, down_diff, vl);
      const vfloat32m1_t r1 = __riscv_vfadd_vv_f32m1(sn1, sn2, vl);
      const vfloat32m1_t right_sq = __riscv_vfmul_vv_f32m1(right_diff, right_diff, vl);
      const vfloat32m1_t left_sq = __riscv_vfmul_vv_f32m1(left_diff, left_diff, vl);
      const vfloat32m1_t r2 = __riscv_vfadd_vv_f32m1(right_sq, left_sq, vl);
      const vfloat32m1_t d = __riscv_vfmin_vv_f32m1(r1, r2, vl);

      active = __riscv_vmand_mm_b32(active, __riscv_vmfge_vf_f32m1_b32(d, config.first_threshold, vl), vl);

      const vfloat32m1_t sn1_sqrt = __riscv_vfsqrt_v_f32m1(sn1, vl);
      const vfloat32m1_t sn2_sqrt = __riscv_vfsqrt_v_f32m1(sn2, vl);
      vfloat32m1_t b1 = __riscv_vfmul_vv_f32m1(normalDiff(rx, ry, rz, ux, uy, uz, vl), sn1_sqrt, vl);
      b1 = __riscv_vfmacc_vv_f32m1(b1, normalDiff(lx, ly, lz, dx, dy, dz, vl), sn2_sqrt, vl);
      vfloat32m1_t b2 = __riscv_vfmul_vv_f32m1(normalDiff(rx, ry, rz, dx, dy, dz, vl), sn2_sqrt, vl);
      b2 = __riscv_vfmacc_vv_f32m1(b2, normalDiff(lx, ly, lz, ux, uy, uz, vl), sn1_sqrt, vl);
      const vfloat32m1_t b = __riscv_vfmin_vv_f32m1(b1, b2, vl);
      const vfloat32m1_t a = __riscv_vfsub_vv_f32m1(
          __riscv_vfsub_vv_f32m1(r2, r1, vl),
          __riscv_vfmul_vf_f32m1(b, 2.0f, vl),
          vl);
      const vbool32_t use_quad = __riscv_vmand_mm_b32(
          __riscv_vmflt_vf_f32m1_b32(b, 0.0f, vl),
          __riscv_vmfgt_vf_f32m1_b32(__riscv_vfadd_vv_f32m1(b, a, vl), 0.0f, vl),
          vl);
      const vfloat32m1_t bb = __riscv_vfmul_vv_f32m1(b, b, vl);
      const vfloat32m1_t quad = __riscv_vfsub_vv_f32m1(r1, __riscv_vfdiv_vv_f32m1(bb, a, vl), vl);
      const vfloat32m1_t selected = __riscv_vmerge_vvm_f32m1(d, quad, use_quad, vl);
      __riscv_vse32_v_f32m1_m(active, response + center, selected, vl);
      col += vl;
    }
  }
}

PCL_TRAJKOVIC_3D_RVV_NOINLINE inline void
computeEightCornersResponseRVV(const pcl::PointXYZ* points,
                               const pcl::Normal* normals,
                               const ResponseConfig& config,
                               float* response)
{
  clearResponse(response, config.width * config.height);
  if (config.width == 0 || config.height == 0 || config.half_window <= 0)
    return;

  const auto step = static_cast<std::size_t>(config.half_window);
  if (config.width <= 2 * step || config.height <= 2 * step)
    return;

  for (std::size_t row = step; row < config.height - step; ++row)
  {
    std::size_t col = step;
    for (; col < config.width - step;)
    {
      const std::size_t available = config.width - step - col;
      const std::size_t vl = __riscv_vsetvl_e32m1(available);
      const std::size_t center = row * config.width + col;
      const std::size_t up = (row - step) * config.width + col;
      const std::size_t down = (row + step) * config.width + col;
      const std::size_t left = row * config.width + (col - step);
      const std::size_t right = row * config.width + (col + step);
      const std::size_t upleft = (row - step) * config.width + (col - step);
      const std::size_t upright = (row - step) * config.width + (col + step);
      const std::size_t downleft = (row + step) * config.width + (col - step);
      const std::size_t downright = (row + step) * config.width + (col + step);

      vfloat32m1_t cx;
      vfloat32m1_t cy;
      vfloat32m1_t cz;
      vfloat32m1_t ux;
      vfloat32m1_t uy;
      vfloat32m1_t uz;
      vfloat32m1_t dx;
      vfloat32m1_t dy;
      vfloat32m1_t dz;
      vfloat32m1_t lx;
      vfloat32m1_t ly;
      vfloat32m1_t lz;
      vfloat32m1_t rx;
      vfloat32m1_t ry;
      vfloat32m1_t rz;
      vfloat32m1_t ulx;
      vfloat32m1_t uly;
      vfloat32m1_t ulz;
      vfloat32m1_t urx;
      vfloat32m1_t ury;
      vfloat32m1_t urz;
      vfloat32m1_t dlx;
      vfloat32m1_t dly;
      vfloat32m1_t dlz;
      vfloat32m1_t drx;
      vfloat32m1_t dry;
      vfloat32m1_t drz;
      vfloat32m1_t px;
      vfloat32m1_t py;
      vfloat32m1_t pz;
      loadPointFields(points, center, vl, px, py, pz);
      loadNormalFields(normals, center, vl, cx, cy, cz);
      loadNormalFields(normals, up, vl, ux, uy, uz);
      loadNormalFields(normals, down, vl, dx, dy, dz);
      loadNormalFields(normals, left, vl, lx, ly, lz);
      loadNormalFields(normals, right, vl, rx, ry, rz);
      loadNormalFields(normals, upleft, vl, ulx, uly, ulz);
      loadNormalFields(normals, upright, vl, urx, ury, urz);
      loadNormalFields(normals, downleft, vl, dlx, dly, dlz);
      loadNormalFields(normals, downright, vl, drx, dry, drz);

      const vbool32_t up_finite = normalFiniteMask(ux, uy, uz, vl);
      const vbool32_t down_finite = normalFiniteMask(dx, dy, dz, vl);
      const vbool32_t left_finite = normalFiniteMask(lx, ly, lz, vl);
      const vbool32_t right_finite = normalFiniteMask(rx, ry, rz, vl);
      const vbool32_t upleft_finite = normalFiniteMask(ulx, uly, ulz, vl);
      const vbool32_t upright_finite = normalFiniteMask(urx, ury, urz, vl);
      const vbool32_t downleft_finite = normalFiniteMask(dlx, dly, dlz, vl);
      const vbool32_t downright_finite = normalFiniteMask(drx, dry, drz, vl);
      applyNullNormalForInvalidMask(up_finite, vl, ux, uy, uz);
      applyNullNormalForInvalidMask(down_finite, vl, dx, dy, dz);
      applyNullNormalForInvalidMask(left_finite, vl, lx, ly, lz);
      applyNullNormalForInvalidMask(right_finite, vl, rx, ry, rz);
      applyNullNormalForInvalidMask(upleft_finite, vl, ulx, uly, ulz);
      applyNullNormalForInvalidMask(upright_finite, vl, urx, ury, urz);
      applyNullNormalForInvalidMask(downleft_finite, vl, dlx, dly, dlz);
      applyNullNormalForInvalidMask(downright_finite, vl, drx, dry, drz);

      vbool32_t active = __riscv_vmand_mm_b32(pointFiniteMask(px, py, pz, vl), normalFiniteMask(cx, cy, cz, vl), vl);
      vbool32_t neighbor_any = up_finite;
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, down_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, left_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, right_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, upleft_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, upright_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, downleft_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, downright_finite, vl);
      active = __riscv_vmand_mm_b32(active, neighbor_any, vl);

      const vfloat32m1_t up_center = normalDiff(ux, uy, uz, cx, cy, cz, vl);
      const vfloat32m1_t down_center = normalDiff(dx, dy, dz, cx, cy, cz, vl);
      const vfloat32m1_t left_center = normalDiff(lx, ly, lz, cx, cy, cz, vl);
      const vfloat32m1_t right_center = normalDiff(rx, ry, rz, cx, cy, cz, vl);
      const vfloat32m1_t upright_center = normalDiff(urx, ury, urz, cx, cy, cz, vl);
      const vfloat32m1_t downleft_center = normalDiff(dlx, dly, dlz, cx, cy, cz, vl);
      const vfloat32m1_t downright_center = normalDiff(drx, dry, drz, cx, cy, cz, vl);
      const vfloat32m1_t upleft_center = normalDiff(ulx, uly, ulz, cx, cy, cz, vl);

      const vfloat32m1_t r0 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(up_center, up_center, vl),
          __riscv_vfmul_vv_f32m1(down_center, down_center, vl),
          vl);
      const vfloat32m1_t r1 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(upright_center, upright_center, vl),
          __riscv_vfmul_vv_f32m1(downleft_center, downleft_center, vl),
          vl);
      const vfloat32m1_t r2 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(right_center, right_center, vl),
          __riscv_vfmul_vv_f32m1(left_center, left_center, vl),
          vl);
      const vfloat32m1_t r3 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(downright_center, downright_center, vl),
          __riscv_vfmul_vv_f32m1(upleft_center, upleft_center, vl),
          vl);
      const vfloat32m1_t d = __riscv_vfmin_vv_f32m1(__riscv_vfmin_vv_f32m1(r0, r1, vl),
                                                    __riscv_vfmin_vv_f32m1(r2, r3, vl),
                                                    vl);

      active = __riscv_vmand_mm_b32(active, __riscv_vmfge_vf_f32m1_b32(d, config.first_threshold, vl), vl);

      const vfloat32m1_t b0 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(normalDiff(urx, ury, urz, ux, uy, uz, vl), up_center, vl),
          __riscv_vfmul_vv_f32m1(normalDiff(dlx, dly, dlz, dx, dy, dz, vl), down_center, vl),
          vl);
      const vfloat32m1_t b1 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(normalDiff(rx, ry, rz, urx, ury, urz, vl), upright_center, vl),
          __riscv_vfmul_vv_f32m1(normalDiff(lx, ly, lz, dlx, dly, dlz, vl), downleft_center, vl),
          vl);
      const vfloat32m1_t b2 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(normalDiff(drx, dry, drz, rx, ry, rz, vl), downright_center, vl),
          __riscv_vfmul_vv_f32m1(normalDiff(ulx, uly, ulz, lx, ly, lz, vl), upleft_center, vl),
          vl);
      const vfloat32m1_t b3 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(normalDiff(dx, dy, dz, drx, dry, drz, vl), downright_center, vl),
          __riscv_vfmul_vv_f32m1(normalDiff(ux, uy, uz, ulx, uly, ulz, vl), upleft_center, vl),
          vl);
      const vfloat32m1_t a0 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r1, r0, vl),
                                                     __riscv_vfadd_vv_f32m1(b0, b0, vl),
                                                     vl);
      const vfloat32m1_t a1 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r2, r1, vl),
                                                     __riscv_vfadd_vv_f32m1(b1, b1, vl),
                                                     vl);
      const vfloat32m1_t a2 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r3, r2, vl),
                                                     __riscv_vfadd_vv_f32m1(b2, b2, vl),
                                                     vl);
      const vfloat32m1_t a3 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r0, r3, vl),
                                                     __riscv_vfadd_vv_f32m1(b3, b3, vl),
                                                     vl);
      const vbool32_t use_d = __riscv_vmand_mm_b32(
          __riscv_vmflt_vf_f32m1_b32(__riscv_vfmax_vv_f32m1(__riscv_vfmax_vv_f32m1(b0, b1, vl),
                                                            __riscv_vfmax_vv_f32m1(b2, b3, vl),
                                                            vl),
                                     0.0f,
                                     vl),
          __riscv_vmfgt_vf_f32m1_b32(__riscv_vfmin_vv_f32m1(__riscv_vfmin_vv_f32m1(__riscv_vfadd_vv_f32m1(a0, b0, vl),
                                                                                   __riscv_vfadd_vv_f32m1(a1, b1, vl),
                                                                                   vl),
                                                            __riscv_vfmin_vv_f32m1(__riscv_vfadd_vv_f32m1(a2, b2, vl),
                                                                                   __riscv_vfadd_vv_f32m1(a3, b3, vl),
                                                                                   vl),
                                                            vl),
                                       0.0f,
                                       vl),
          vl);
      const vfloat32m1_t d0 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b0, b0, vl), a0, vl);
      const vfloat32m1_t d1 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b1, b1, vl), a1, vl);
      const vfloat32m1_t d2 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b2, b2, vl), a2, vl);
      const vfloat32m1_t d3 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b3, b3, vl), a3, vl);
      const vfloat32m1_t selected = __riscv_vmerge_vvm_f32m1(d,
                                                             __riscv_vfmin_vv_f32m1(
                                                                 __riscv_vfmin_vv_f32m1(d0, d1, vl),
                                                                 __riscv_vfmin_vv_f32m1(d2, d3, vl),
                                                                 vl),
                                                             use_d,
                                                             vl);
      __riscv_vse32_v_f32m1_m(active, response + center, selected, vl);
      col += vl;
    }
  }
}

#undef PCL_TRAJKOVIC_3D_RVV_NOINLINE
#else
inline void
computeFourCornersResponseRVV(const pcl::PointXYZ* points,
                              const pcl::Normal* normals,
                              const ResponseConfig& config,
                              float* response)
{
  computeFourCornersResponseStd(points, normals, config, response);
}

inline void
computeEightCornersResponseRVV(const pcl::PointXYZ* points,
                               const pcl::Normal* normals,
                               const ResponseConfig& config,
                               float* response)
{
  computeEightCornersResponseStd(points, normals, config, response);
}
#endif

} // namespace pcl::keypoints::rvv_test::trajkovic_3d
