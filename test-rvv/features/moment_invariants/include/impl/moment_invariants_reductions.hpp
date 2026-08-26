/*
 * 本文件做什么：
 * 这里保存 MomentInvariantsEstimation 的 test-only reference（测试专用参考链路）。
 * 它复刻 production helper 中 centroid（质心）之后的中心矩累加和 j1/j2/j3
 * 组合公式，供后续 RVV candidate（候选实现）做 same-chain（同构链路）对拍。
 *
 * 证据边界：
 * 本文件里的 RVV helper 是测试专用诊断实现。生产源码中的 RVV path（RVV 链路）
 * 位于 `features/include/pcl/features/impl/moment_invariants.hpp`，两者分开保留，
 * 方便 reviewer 区分 test-only evidence（测试专用证据）和 production direct evidence
 * （真实生产路径证据）。
 */

#pragma once

#include <pcl/common/centroid.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <cstddef>
#include <cstdint>
#include <limits>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::moment_invariants {

struct MomentSummary {
  float mu200 = 0.0f;
  float mu020 = 0.0f;
  float mu002 = 0.0f;
  float mu110 = 0.0f;
  float mu101 = 0.0f;
  float mu011 = 0.0f;
  float j1 = 0.0f;
  float j2 = 0.0f;
  float j3 = 0.0f;
};

inline void
finalizeMomentSummary(MomentSummary& summary)
{
  summary.j1 = summary.mu200 + summary.mu020 + summary.mu002;
  summary.j2 = summary.mu200 * summary.mu020 + summary.mu200 * summary.mu002 +
               summary.mu020 * summary.mu002 - summary.mu110 * summary.mu110 -
               summary.mu101 * summary.mu101 - summary.mu011 * summary.mu011;
  summary.j3 = summary.mu200 * summary.mu020 * summary.mu002 +
               2.0f * summary.mu110 * summary.mu101 * summary.mu011 -
               summary.mu002 * summary.mu110 * summary.mu110 -
               summary.mu020 * summary.mu101 * summary.mu101 -
               summary.mu200 * summary.mu011 * summary.mu011;
}

inline Eigen::Vector4f
computeCentroidStd(const pcl::PointXYZ* points, const pcl::Indices& indices)
{
  Eigen::Vector4f centroid = Eigen::Vector4f::Zero();
  for (const int index : indices) {
    const pcl::PointXYZ& point = points[static_cast<std::size_t>(index)];
    centroid[0] += point.x;
    centroid[1] += point.y;
    centroid[2] += point.z;
  }
  if (!indices.empty())
    centroid /= static_cast<float>(indices.size());
  return centroid;
}

inline Eigen::Vector4f
computeCentroidStd(const pcl::PointXYZ* points, const std::size_t count)
{
  Eigen::Vector4f centroid = Eigen::Vector4f::Zero();
  for (std::size_t i = 0; i < count; ++i) {
    centroid[0] += points[i].x;
    centroid[1] += points[i].y;
    centroid[2] += points[i].z;
  }
  if (count != 0)
    centroid /= static_cast<float>(count);
  return centroid;
}

inline MomentSummary
computeMomentSummaryFromCentroidStd(const pcl::PointXYZ* points,
                                    const pcl::Indices& indices,
                                    const Eigen::Vector4f& centroid)
{
  MomentSummary summary;
  for (const int index : indices) {
    const pcl::PointXYZ& point = points[static_cast<std::size_t>(index)];
    const float dx = point.x - centroid[0];
    const float dy = point.y - centroid[1];
    const float dz = point.z - centroid[2];
    summary.mu200 += dx * dx;
    summary.mu020 += dy * dy;
    summary.mu002 += dz * dz;
    summary.mu110 += dx * dy;
    summary.mu101 += dx * dz;
    summary.mu011 += dy * dz;
  }
  finalizeMomentSummary(summary);
  return summary;
}

inline MomentSummary
computeMomentSummaryFromCentroidStd(const pcl::PointXYZ* points,
                                    const std::size_t count,
                                    const Eigen::Vector4f& centroid)
{
  MomentSummary summary;
  for (std::size_t i = 0; i < count; ++i) {
    const pcl::PointXYZ& point = points[i];
    const float dx = point.x - centroid[0];
    const float dy = point.y - centroid[1];
    const float dz = point.z - centroid[2];
    summary.mu200 += dx * dx;
    summary.mu020 += dy * dy;
    summary.mu002 += dz * dz;
    summary.mu110 += dx * dy;
    summary.mu101 += dx * dz;
    summary.mu011 += dy * dz;
  }
  finalizeMomentSummary(summary);
  return summary;
}

inline MomentSummary
computeMomentSummaryStd(const pcl::PointXYZ* points, const pcl::Indices& indices)
{
  const Eigen::Vector4f centroid = computeCentroidStd(points, indices);
  return computeMomentSummaryFromCentroidStd(points, indices, centroid);
}

inline MomentSummary
computeMomentSummaryStd(const pcl::PointXYZ* points, const std::size_t count)
{
  const Eigen::Vector4f centroid = computeCentroidStd(points, count);
  return computeMomentSummaryFromCentroidStd(points, count, centroid);
}

#ifdef __RVV10__
inline float
reduceSumF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t init = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  const vfloat32m1_t reduced = __riscv_vfredusum_vs_f32m2_f32m1(values, init, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline void
accumulateMomentChunk(MomentSummary& summary,
                      const vfloat32m2_t vx,
                      const vfloat32m2_t vy,
                      const vfloat32m2_t vz,
                      const Eigen::Vector4f& centroid,
                      const std::size_t vl)
{
  const vfloat32m2_t cx = __riscv_vfmv_v_f_f32m2(centroid[0], vl);
  const vfloat32m2_t cy = __riscv_vfmv_v_f_f32m2(centroid[1], vl);
  const vfloat32m2_t cz = __riscv_vfmv_v_f_f32m2(centroid[2], vl);
  const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(vx, cx, vl);
  const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(vy, cy, vl);
  const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(vz, cz, vl);

  summary.mu200 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), vl);
  summary.mu020 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dy, dy, vl), vl);
  summary.mu002 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dz, dz, vl), vl);
  summary.mu110 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dx, dy, vl), vl);
  summary.mu101 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dx, dz, vl), vl);
  summary.mu011 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(dy, dz, vl), vl);
}
#endif

inline MomentSummary
computeMomentSummaryFromCentroidRVV(const pcl::PointXYZ* points,
                                    const pcl::Indices& indices,
                                    const Eigen::Vector4f& centroid)
{
#ifndef __RVV10__
  return computeMomentSummaryFromCentroidStd(points, indices, centroid);
#else
  MomentSummary summary;
  const auto* base = reinterpret_cast<const std::uint8_t*>(points);
  const auto* x_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, x));
  const auto* y_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, y));
  const auto* z_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, z));

  std::size_t offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const vuint32m2_t v_index =
        __riscv_vle32_v_u32m2(reinterpret_cast<const std::uint32_t*>(indices.data() + offset), vl);
    const vuint32m2_t v_byte_offset = __riscv_vmul_vx_u32m2(v_index, sizeof(pcl::PointXYZ), vl);
    const vfloat32m2_t vx = __riscv_vluxei32_v_f32m2(x_base, v_byte_offset, vl);
    const vfloat32m2_t vy = __riscv_vluxei32_v_f32m2(y_base, v_byte_offset, vl);
    const vfloat32m2_t vz = __riscv_vluxei32_v_f32m2(z_base, v_byte_offset, vl);
    accumulateMomentChunk(summary, vx, vy, vz, centroid, vl);
    offset += vl;
  }
  finalizeMomentSummary(summary);
  return summary;
#endif
}

inline MomentSummary
computeMomentSummaryFromCentroidRVV(const pcl::PointXYZ* points,
                                    const std::size_t count,
                                    const Eigen::Vector4f& centroid)
{
#ifndef __RVV10__
  return computeMomentSummaryFromCentroidStd(points, count, centroid);
#else
  MomentSummary summary;
  const auto* base = reinterpret_cast<const std::uint8_t*>(points);
  const auto* x_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, x));
  const auto* y_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, y));
  const auto* z_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, z));

  std::size_t offset = 0;
  while (offset < count) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - offset);
    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(x_base + offset * 4, sizeof(pcl::PointXYZ), vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(y_base + offset * 4, sizeof(pcl::PointXYZ), vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(z_base + offset * 4, sizeof(pcl::PointXYZ), vl);
    accumulateMomentChunk(summary, vx, vy, vz, centroid, vl);
    offset += vl;
  }
  finalizeMomentSummary(summary);
  return summary;
#endif
}

inline MomentSummary
computeMomentSummaryRVV(const pcl::PointXYZ* points, const pcl::Indices& indices)
{
  const Eigen::Vector4f centroid = computeCentroidStd(points, indices);
  return computeMomentSummaryFromCentroidRVV(points, indices, centroid);
}

inline MomentSummary
computeMomentSummaryRVV(const pcl::PointXYZ* points, const std::size_t count)
{
  const Eigen::Vector4f centroid = computeCentroidStd(points, count);
  return computeMomentSummaryFromCentroidRVV(points, count, centroid);
}

} // namespace pcl::features::rvv_test::moment_invariants
