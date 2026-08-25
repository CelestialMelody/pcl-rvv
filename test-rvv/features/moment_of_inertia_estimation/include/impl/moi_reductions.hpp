/*
 * 本文件做什么：
 * 这里保存 MomentOfInertiaEstimation 逐点规约的 diagnostic（诊断）helper。
 * Std helper（标量参考链路）复刻 production 中 mean/AABB、covariance、单轴
 * moment of inertia（惯性矩）和 OBB extrema（有向包围盒极值）的计算口径。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv 证据，不修改 production，也不证明 public
 * compute() 已经命中 RVV dispatch（生产分流）。
 */

#pragma once

#include <pcl/point_types.h>

#include <Eigen/Core>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::moi {

struct ReductionSummary {
  std::uint32_t count = 0;
  std::array<float, 3> mean{{0.0f, 0.0f, 0.0f}};
  std::array<float, 3> aabb_min{{
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max()}};
  std::array<float, 3> aabb_max{{
      -std::numeric_limits<float>::max(),
      -std::numeric_limits<float>::max(),
      -std::numeric_limits<float>::max()}};
  std::array<float, 6> covariance{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
  float moment = 0.0f;
  std::array<float, 3> obb_min{{
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max()}};
  std::array<float, 3> obb_max{{
      -std::numeric_limits<float>::max(),
      -std::numeric_limits<float>::max(),
      -std::numeric_limits<float>::max()}};
};

using Covariance6 = std::array<float, 6>;

inline void
updateMinMax(const float x,
             const float y,
             const float z,
             std::array<float, 3>& min_values,
             std::array<float, 3>& max_values)
{
  min_values[0] = std::min(min_values[0], x);
  min_values[1] = std::min(min_values[1], y);
  min_values[2] = std::min(min_values[2], z);
  max_values[0] = std::max(max_values[0], x);
  max_values[1] = std::max(max_values[1], y);
  max_values[2] = std::max(max_values[2], z);
}

inline ReductionSummary
computeReductionSummaryStd(const pcl::PointXYZ* points,
                           const std::uint32_t* indices,
                           const std::size_t count,
                           const Eigen::Vector3f& inertia_axis,
                           const Eigen::Vector3f& major_axis,
                           const Eigen::Vector3f& middle_axis,
                           const Eigen::Vector3f& minor_axis,
                           const float point_mass)
{
  ReductionSummary summary;
  summary.count = static_cast<std::uint32_t>(count);

  for (std::size_t i = 0; i < count; ++i) {
    const pcl::PointXYZ& point = points[indices[i]];
    summary.mean[0] += point.x;
    summary.mean[1] += point.y;
    summary.mean[2] += point.z;
    updateMinMax(point.x, point.y, point.z, summary.aabb_min, summary.aabb_max);
  }

  const float divisor = count == 0 ? 1.0f : static_cast<float>(count);
  summary.mean[0] /= divisor;
  summary.mean[1] /= divisor;
  summary.mean[2] /= divisor;

  const float factor = 1.0f / static_cast<float>((count > 1) ? (count - 1) : 1);
  for (std::size_t i = 0; i < count; ++i) {
    const pcl::PointXYZ& point = points[indices[i]];
    const float dx = point.x - summary.mean[0];
    const float dy = point.y - summary.mean[1];
    const float dz = point.z - summary.mean[2];

    summary.covariance[0] += dx * dx;
    summary.covariance[1] += dx * dy;
    summary.covariance[2] += dx * dz;
    summary.covariance[3] += dy * dy;
    summary.covariance[4] += dy * dz;
    summary.covariance[5] += dz * dz;

    const float vx = summary.mean[0] - point.x;
    const float vy = summary.mean[1] - point.y;
    const float vz = summary.mean[2] - point.z;
    const float cx = vy * inertia_axis.z() - vz * inertia_axis.y();
    const float cy = vz * inertia_axis.x() - vx * inertia_axis.z();
    const float cz = vx * inertia_axis.y() - vy * inertia_axis.x();
    summary.moment += cx * cx + cy * cy + cz * cz;

    const float ox = dx * major_axis.x() + dy * major_axis.y() + dz * major_axis.z();
    const float oy = dx * middle_axis.x() + dy * middle_axis.y() + dz * middle_axis.z();
    const float oz = dx * minor_axis.x() + dy * minor_axis.y() + dz * minor_axis.z();
    updateMinMax(ox, oy, oz, summary.obb_min, summary.obb_max);
  }

  for (float& value : summary.covariance)
    value *= factor;
  summary.moment *= point_mass;
  return summary;
}

inline Covariance6
computeProjectedCovarianceStd(const pcl::PointXYZ* points,
                              const std::uint32_t* indices,
                              const std::size_t count,
                              const Eigen::Vector3f& mean,
                              const Eigen::Vector3f& normal)
{
  Covariance6 covariance{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
  const float factor = 1.0f / static_cast<float>((count > 1) ? (count - 1) : 1);
  for (std::size_t i = 0; i < count; ++i) {
    const pcl::PointXYZ& point = points[indices[i]];
    const float dx = point.x - mean.x();
    const float dy = point.y - mean.y();
    const float dz = point.z - mean.z();
    const float dot = dx * normal.x() + dy * normal.y() + dz * normal.z();
    const float px = dx - dot * normal.x();
    const float py = dy - dot * normal.y();
    const float pz = dz - dot * normal.z();

    covariance[0] += px * px;
    covariance[1] += px * py;
    covariance[2] += px * pz;
    covariance[3] += py * py;
    covariance[4] += py * pz;
    covariance[5] += pz * pz;
  }
  for (float& value : covariance)
    value *= factor;
  return covariance;
}

#ifdef __RVV10__
inline float
reduceSumF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t init = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  const vfloat32m1_t reduced = __riscv_vfredusum_vs_f32m2_f32m1(values, init, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline float
reduceMinF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t init = __riscv_vfmv_s_f_f32m1(std::numeric_limits<float>::max(), 1);
  const vfloat32m1_t reduced = __riscv_vfredmin_vs_f32m2_f32m1(values, init, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline float
reduceMaxF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t init = __riscv_vfmv_s_f_f32m1(-std::numeric_limits<float>::max(), 1);
  const vfloat32m1_t reduced = __riscv_vfredmax_vs_f32m2_f32m1(values, init, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline void
loadIndexedXYZ(const pcl::PointXYZ* points,
               const std::uint32_t* indices,
               const std::size_t offset,
               const std::size_t vl,
               vfloat32m2_t& vx,
               vfloat32m2_t& vy,
               vfloat32m2_t& vz)
{
  const vuint32m2_t v_index = __riscv_vle32_v_u32m2(indices + offset, vl);
  const vuint32m2_t v_byte_offset = __riscv_vmul_vx_u32m2(v_index, sizeof(pcl::PointXYZ), vl);
  const auto* base = reinterpret_cast<const std::uint8_t*>(points);
  const auto* x_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, x));
  const auto* y_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, y));
  const auto* z_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, z));
  vx = __riscv_vluxei32_v_f32m2(x_base, v_byte_offset, vl);
  vy = __riscv_vluxei32_v_f32m2(y_base, v_byte_offset, vl);
  vz = __riscv_vluxei32_v_f32m2(z_base, v_byte_offset, vl);
}
#endif

inline ReductionSummary
computeReductionSummaryRVV(const pcl::PointXYZ* points,
                           const std::uint32_t* indices,
                           const std::size_t count,
                           const Eigen::Vector3f& inertia_axis,
                           const Eigen::Vector3f& major_axis,
                           const Eigen::Vector3f& middle_axis,
                           const Eigen::Vector3f& minor_axis,
                           const float point_mass)
{
#ifndef __RVV10__
  return computeReductionSummaryStd(
      points, indices, count, inertia_axis, major_axis, middle_axis, minor_axis, point_mass);
#else
  ReductionSummary summary;
  summary.count = static_cast<std::uint32_t>(count);

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    loadIndexedXYZ(points, indices, i, vl, vx, vy, vz);

    summary.mean[0] += reduceSumF32m2(vx, vl);
    summary.mean[1] += reduceSumF32m2(vy, vl);
    summary.mean[2] += reduceSumF32m2(vz, vl);
    summary.aabb_min[0] = std::min(summary.aabb_min[0], reduceMinF32m2(vx, vl));
    summary.aabb_min[1] = std::min(summary.aabb_min[1], reduceMinF32m2(vy, vl));
    summary.aabb_min[2] = std::min(summary.aabb_min[2], reduceMinF32m2(vz, vl));
    summary.aabb_max[0] = std::max(summary.aabb_max[0], reduceMaxF32m2(vx, vl));
    summary.aabb_max[1] = std::max(summary.aabb_max[1], reduceMaxF32m2(vy, vl));
    summary.aabb_max[2] = std::max(summary.aabb_max[2], reduceMaxF32m2(vz, vl));
    i += vl;
  }

  const float divisor = count == 0 ? 1.0f : static_cast<float>(count);
  summary.mean[0] /= divisor;
  summary.mean[1] /= divisor;
  summary.mean[2] /= divisor;

  const float factor = 1.0f / static_cast<float>((count > 1) ? (count - 1) : 1);
  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    vfloat32m2_t px;
    vfloat32m2_t py;
    vfloat32m2_t pz;
    loadIndexedXYZ(points, indices, i, vl, px, py, pz);

    const vfloat32m2_t dx =
        __riscv_vfsub_vf_f32m2(px, summary.mean[0], vl);
    const vfloat32m2_t dy =
        __riscv_vfsub_vf_f32m2(py, summary.mean[1], vl);
    const vfloat32m2_t dz =
        __riscv_vfsub_vf_f32m2(pz, summary.mean[2], vl);

    summary.covariance[0] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), vl);
    summary.covariance[1] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dx, dy, vl), vl);
    summary.covariance[2] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dx, dz, vl), vl);
    summary.covariance[3] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dy, dy, vl), vl);
    summary.covariance[4] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dy, dz, vl), vl);
    summary.covariance[5] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dz, dz, vl), vl);

    const vfloat32m2_t vx = __riscv_vfsub_vv_f32m2(
        __riscv_vfmv_v_f_f32m2(summary.mean[0], vl), px, vl);
    const vfloat32m2_t vy = __riscv_vfsub_vv_f32m2(
        __riscv_vfmv_v_f_f32m2(summary.mean[1], vl), py, vl);
    const vfloat32m2_t vz = __riscv_vfsub_vv_f32m2(
        __riscv_vfmv_v_f_f32m2(summary.mean[2], vl), pz, vl);
    const vfloat32m2_t cx = __riscv_vfsub_vv_f32m2(
        __riscv_vfmul_vf_f32m2(vy, inertia_axis.z(), vl),
        __riscv_vfmul_vf_f32m2(vz, inertia_axis.y(), vl),
        vl);
    const vfloat32m2_t cy = __riscv_vfsub_vv_f32m2(
        __riscv_vfmul_vf_f32m2(vz, inertia_axis.x(), vl),
        __riscv_vfmul_vf_f32m2(vx, inertia_axis.z(), vl),
        vl);
    const vfloat32m2_t cz = __riscv_vfsub_vv_f32m2(
        __riscv_vfmul_vf_f32m2(vx, inertia_axis.y(), vl),
        __riscv_vfmul_vf_f32m2(vy, inertia_axis.x(), vl),
        vl);
    vfloat32m2_t moment = __riscv_vfmul_vv_f32m2(cx, cx, vl);
    moment = __riscv_vfmacc_vv_f32m2(moment, cy, cy, vl);
    moment = __riscv_vfmacc_vv_f32m2(moment, cz, cz, vl);
    summary.moment += reduceSumF32m2(moment, vl);

    vfloat32m2_t ox = __riscv_vfmul_vf_f32m2(dx, major_axis.x(), vl);
    ox = __riscv_vfmacc_vf_f32m2(ox, major_axis.y(), dy, vl);
    ox = __riscv_vfmacc_vf_f32m2(ox, major_axis.z(), dz, vl);
    vfloat32m2_t oy = __riscv_vfmul_vf_f32m2(dx, middle_axis.x(), vl);
    oy = __riscv_vfmacc_vf_f32m2(oy, middle_axis.y(), dy, vl);
    oy = __riscv_vfmacc_vf_f32m2(oy, middle_axis.z(), dz, vl);
    vfloat32m2_t oz = __riscv_vfmul_vf_f32m2(dx, minor_axis.x(), vl);
    oz = __riscv_vfmacc_vf_f32m2(oz, minor_axis.y(), dy, vl);
    oz = __riscv_vfmacc_vf_f32m2(oz, minor_axis.z(), dz, vl);

    summary.obb_min[0] = std::min(summary.obb_min[0], reduceMinF32m2(ox, vl));
    summary.obb_min[1] = std::min(summary.obb_min[1], reduceMinF32m2(oy, vl));
    summary.obb_min[2] = std::min(summary.obb_min[2], reduceMinF32m2(oz, vl));
    summary.obb_max[0] = std::max(summary.obb_max[0], reduceMaxF32m2(ox, vl));
    summary.obb_max[1] = std::max(summary.obb_max[1], reduceMaxF32m2(oy, vl));
    summary.obb_max[2] = std::max(summary.obb_max[2], reduceMaxF32m2(oz, vl));

    i += vl;
  }

  for (float& value : summary.covariance)
    value *= factor;
  summary.moment *= point_mass;
  return summary;
#endif
}

inline Covariance6
computeProjectedCovarianceRVV(const pcl::PointXYZ* points,
                              const std::uint32_t* indices,
                              const std::size_t count,
                              const Eigen::Vector3f& mean,
                              const Eigen::Vector3f& normal)
{
#ifndef __RVV10__
  return computeProjectedCovarianceStd(points, indices, count, mean, normal);
#else
  Covariance6 covariance{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
  const float factor = 1.0f / static_cast<float>((count > 1) ? (count - 1) : 1);
  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    vfloat32m2_t px0;
    vfloat32m2_t py0;
    vfloat32m2_t pz0;
    loadIndexedXYZ(points, indices, i, vl, px0, py0, pz0);

    const vfloat32m2_t dx = __riscv_vfsub_vf_f32m2(px0, mean.x(), vl);
    const vfloat32m2_t dy = __riscv_vfsub_vf_f32m2(py0, mean.y(), vl);
    const vfloat32m2_t dz = __riscv_vfsub_vf_f32m2(pz0, mean.z(), vl);

    vfloat32m2_t dot = __riscv_vfmul_vf_f32m2(dx, normal.x(), vl);
    dot = __riscv_vfmacc_vf_f32m2(dot, normal.y(), dy, vl);
    dot = __riscv_vfmacc_vf_f32m2(dot, normal.z(), dz, vl);

    const vfloat32m2_t px = __riscv_vfnmsac_vf_f32m2(dx, normal.x(), dot, vl);
    const vfloat32m2_t py = __riscv_vfnmsac_vf_f32m2(dy, normal.y(), dot, vl);
    const vfloat32m2_t pz = __riscv_vfnmsac_vf_f32m2(dz, normal.z(), dot, vl);

    covariance[0] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(px, px, vl), vl);
    covariance[1] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(px, py, vl), vl);
    covariance[2] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(px, pz, vl), vl);
    covariance[3] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(py, py, vl), vl);
    covariance[4] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(py, pz, vl), vl);
    covariance[5] += reduceSumF32m2(__riscv_vfmul_vv_f32m2(pz, pz, vl), vl);
    i += vl;
  }
  for (float& value : covariance)
    value *= factor;
  return covariance;
#endif
}

} // namespace pcl::features::rvv_test::moi
