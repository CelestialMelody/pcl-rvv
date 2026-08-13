/*
 * 本文件做什么：
 * 这是 TEPTPL gtest 专用 fixture、断言和 production bridge helper。多个
 * test source 共同 include 它，以便拆分 TEST body 后仍复用同一批确定性输入、
 * row source 构造和 normal-equation 对拍逻辑。
 *
 * 证据边界：
 * 这些 helper 只服务配置解析出的 test-rvv correctness（正确性）证据；它们不进入
 * bench 计时边界，也不改变 production dispatch（生产分流）或 fallback（回退路径）。
 */

#pragma once

#include "teptpl_candidates.hpp"

#include <pcl/common/transforms.h>
#include <pcl/register_point_struct.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>
#include <pcl/rvv_point_traits.h>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

struct TEPTPLDoubleXYZPoint {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

POINT_CLOUD_REGISTER_POINT_STRUCT(TEPTPLDoubleXYZPoint,
                                  (double, x, x)
                                  (double, y, y)
                                  (double, z, z))

namespace pcl::registration::rvv_te_pt2plane_lls_test {

namespace support = pcl::registration::rvv_te_pt2plane_lls_support;
namespace prod_detail = pcl::registration::detail;

inline pcl::PointCloud<pcl::PointNormal>
makeSurfaceCloud(const int grid_radius, const float step)
{
  // 生成上游 registration API 测试同类的二次曲面；法线来自解析偏导。
  // 这个曲面避免完全共面退化，让 6x6 normal-equation 更稳定。
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.height = 1;
  cloud.is_dense = true;
  for (int ix = -grid_radius; ix <= grid_radius; ++ix) {
    for (int iy = -grid_radius; iy <= grid_radius; ++iy) {
      const float x = static_cast<float>(ix) * step;
      const float y = static_cast<float>(iy) * step;
      pcl::PointNormal point;
      point.x = x;
      point.y = y;
      point.z = 0.1f * x * x + 0.2f * x * y - 0.3f * y + 1.0f;
      point.normal_x = -0.2f * x - 0.2f * y;
      point.normal_y = -0.2f * x + 0.3f;
      point.normal_z = 1.0f;
      const float norm = std::sqrt(point.normal_x * point.normal_x +
                                   point.normal_y * point.normal_y +
                                   point.normal_z * point.normal_z);
      point.normal_x /= norm;
      point.normal_y /= norm;
      point.normal_z /= norm;
      cloud.push_back(point);
    }
  }
  cloud.width = cloud.size();
  return cloud;
}

inline Eigen::Matrix4f
makeTransform()
{
  // 这个矩阵沿用上游 registration API 测试的温和刚体变换，便于和公开 estimator 对齐。
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform.row(0) << 0.9938f, 0.0988f, 0.0517f, 0.1000f;
  transform.row(1) << -0.0997f, 0.9949f, 0.0149f, -0.2000f;
  transform.row(2) << -0.0500f, -0.0200f, 0.9986f, 0.3000f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

inline pcl::PointCloud<pcl::PointNormal>
makeTargetCloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // target 由 PCL 公共 transform helper 生成，让测试输入保持真实 PointNormal 布局。
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, makeTransform());
  return target;
}

inline pcl::PointCloud<pcl::PointXYZ>
copySourceAsXYZ(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // PointXYZ source 覆盖“source 只需要 xyz”的泛型字段合同；target normal 仍来自 target 点型。
  pcl::PointCloud<pcl::PointXYZ> xyz;
  xyz.height = 1;
  xyz.is_dense = source.is_dense;
  xyz.reserve(source.size());
  for (const auto& point : source) {
    pcl::PointXYZ copy;
    copy.x = point.x;
    copy.y = point.y;
    copy.z = point.z;
    xyz.push_back(copy);
  }
  xyz.width = xyz.size();
  return xyz;
}

inline pcl::PointCloud<TEPTPLDoubleXYZPoint>
copySourceAsDoubleXYZ(const pcl::PointCloud<pcl::PointNormal>& source)
{
  pcl::PointCloud<TEPTPLDoubleXYZPoint> xyz;
  xyz.height = 1;
  xyz.is_dense = source.is_dense;
  xyz.reserve(source.size());
  for (const auto& point : source) {
    TEPTPLDoubleXYZPoint copy;
    copy.x = point.x;
    copy.y = point.y;
    copy.z = point.z;
    xyz.push_back(copy);
  }
  xyz.width = xyz.size();
  return xyz;
}

inline pcl::PointCloud<pcl::PointXYZINormal>
copyTargetAsXYZINormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  // PointXYZINormal target 覆盖“target 需要 xyz+normal，但不要求 exact PointNormal”的 gate。
  pcl::PointCloud<pcl::PointXYZINormal> copy_cloud;
  copy_cloud.height = 1;
  copy_cloud.is_dense = target.is_dense;
  copy_cloud.reserve(target.size());
  for (const auto& point : target) {
    pcl::PointXYZINormal copy;
    copy.x = point.x;
    copy.y = point.y;
    copy.z = point.z;
    copy.normal_x = point.normal_x;
    copy.normal_y = point.normal_y;
    copy.normal_z = point.normal_z;
    copy.intensity = 0.5f;
    copy.curvature = 0.0f;
    copy_cloud.push_back(copy);
  }
  copy_cloud.width = copy_cloud.size();
  return copy_cloud;
}

inline pcl::PointCloud<pcl::PointNormal>
makeScaleStressCloud()
{
  auto cloud = makeSurfaceCloud(18, 0.09f);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const float scale = (i % 7 == 0) ? 8.0f : ((i % 5 == 0) ? 0.125f : 1.0f);
    cloud[i].x *= scale;
    cloud[i].y *= (i % 3 == 0) ? -scale : scale;
    cloud[i].z = cloud[i].z * scale + static_cast<float>(i % 11) * 0.015f;
    const float nx = cloud[i].normal_x + 0.001f * static_cast<float>(i % 13);
    const float ny = cloud[i].normal_y - 0.0015f * static_cast<float>(i % 17);
    const float nz = cloud[i].normal_z;
    const float norm = std::sqrt(nx * nx + ny * ny + nz * nz);
    cloud[i].normal_x = nx / norm;
    cloud[i].normal_y = ny / norm;
    cloud[i].normal_z = nz / norm;
  }
  return cloud;
}

inline std::pair<pcl::PointCloud<pcl::PointNormal>, pcl::PointCloud<pcl::PointNormal>>
makeNearCancellationCloudPair()
{
  // 这个样本把 source/target 坐标推到较大的绝对值，同时只保留很小的相对位移。
  // current formula 需要先算大项再相减，fused-formula 则先算差值再乘法，因此这类
  // 点能专门约束 d 公式里 dx-sx / dy-sy / dz-sz 的抵消误差。
  auto source = makeScaleStressCloud();
  auto target = source;
  constexpr float kBaseX = 4096.0f;
  constexpr float kBaseY = -2048.0f;
  constexpr float kBaseZ = 1024.0f;
  constexpr float kSmallDx = 0.03125f;
  constexpr float kSmallDy = -0.046875f;
  constexpr float kSmallDz = 0.0625f;
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float scale = (i % 8 == 0) ? 64.0f : ((i % 5 == 0) ? 8.0f : 1.0f);
    const float jitter = static_cast<float>(static_cast<int>(i % 7) - 3) * 0.00390625f;
    source[i].x = kBaseX + source[i].x * scale + jitter;
    source[i].y = kBaseY + source[i].y * scale - jitter * 0.5f;
    source[i].z = kBaseZ + source[i].z * scale + jitter * 0.25f;
    target[i] = source[i];
    const float drift = static_cast<float>(static_cast<int>(i % 9) - 4) * 0.0009765625f;
    target[i].x += kSmallDx + drift;
    target[i].y += kSmallDy - drift * 0.5f;
    target[i].z += kSmallDz + drift * 0.25f;
  }
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[11].normal_y = std::numeric_limits<float>::infinity();
  return {std::move(source), std::move(target)};
}

inline pcl::Correspondences
makeShuffledCorrespondences(const std::size_t n)
{
  // 乱序和重复 index 是 correspondences gather path（对应关系索引读取路径）的语义重点。
  // 这里仍保持 source/target 成对来自同一个变换，只改变扫描顺序并重复少量样本。
  pcl::Correspondences correspondences;
  correspondences.reserve(n + n / 11);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  for (std::size_t i = 1; i < n; i += 4)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  for (std::size_t i = 7; i < n; i += 37)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  return correspondences;
}

inline pcl::Correspondences
makeLocalOffsetCorrespondences(const std::size_t n)
{
  // local-offset（局部偏移）保持 row 顺序和重复样本，但让 target match
  // 偏离 query。它比 same-index 更接近真实 ICP 中 query/match 不完全相同的情况，
  // 但仍保留局部性，因此不是最坏随机分布。
  pcl::Correspondences correspondences;
  correspondences.reserve(n + n / 11);
  if (n == 0)
    return correspondences;
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>((i + 17) % n), 0.0f);
  for (std::size_t i = 1; i < n; i += 4)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>((i + 31) % n), 0.0f);
  for (std::size_t i = 7; i < n; i += 37)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>((i + 5) % n), 0.0f);
  return correspondences;
}

inline pcl::Indices
makeIndexedRows(const std::size_t n)
{
  // source 侧有效 index stream：非连续、含重复，但不包含非法索引。
  // indexed diagnostic 的合同是 valid-index-only（只覆盖有效索引）。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  if (n > 10) {
    indices.push_back(10);
    indices.push_back(2);
  }
  for (std::size_t i = 3; i < n; i += 5)
    indices.push_back(static_cast<int>(i));
  for (std::size_t i = 11; i < n; i += 41)
    indices.push_back(static_cast<int>(i));
  return indices;
}

inline pcl::Indices
makeIndependentTargetIndexedRows(const std::size_t n)
{
  // target 侧使用另一条独立 index stream。它和 source stream 不相同，确保 dual-indices
  // 测试真的覆盖两条 index 流，而不是偶然退化成 source-indexed 或 correspondences。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 1; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  if (n > 11) {
    indices.push_back(11);
    indices.push_back(3);
  }
  for (std::size_t i = 4; i < n; i += 5)
    indices.push_back(static_cast<int>(i));
  for (std::size_t i = 17; i < n; i += 37)
    indices.push_back(static_cast<int>(i));
  return indices;
}

inline pcl::Correspondences
makeCorrespondencesFromIndices(const pcl::Indices& source_indices,
                               const pcl::Indices& target_indices)
{
  // independent-stream correspondences（独立索引流对应关系）复用 dual-indices 的两条
  // index stream，但通过 pcl::Correspondence 承载。它让 bench 能公平比较：
  // 同样的 query/match 分布下，多出的 correspondence 展开成本有多大。
  pcl::Correspondences correspondences;
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  correspondences.reserve(n);
  for (std::size_t row = 0; row < n; ++row)
    correspondences.emplace_back(source_indices[row], target_indices[row], 0.0f);
  return correspondences;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
copyIndexedCloud(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  // 公开 source indices + target full-cloud overload 要求 target 行数等于 indices 行数。
  // 这里在测试准备阶段压出紧凑 target，确保被测 candidate 内只观察 source gather。
  pcl::PointCloud<PointT> subset;
  subset.height = 1;
  subset.is_dense = cloud.is_dense;
  subset.reserve(indices.size());
  for (const int index : indices)
    subset.push_back(cloud[static_cast<std::size_t>(index)]);
  subset.width = subset.size();
  return subset;
}

inline void
expectMatrixNear(const Eigen::Matrix4f& actual,
                 const Eigen::Matrix4f& expected,
                 const float tolerance)
{
  // 测试矩阵逐元素报错，失败时能直接定位是旋转还是平移项偏离。
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(actual(row, col), expected(row, col), tolerance)
          << "row=" << row << " col=" << col;
}

inline void
expectProductionEquationNear(
    const support::NormalEquation& actual,
    const support::NormalEquation& expected)
{
  const double ata_budget = std::max(1e-3, expected.ata.norm() * 1e-4);
  const double atb_budget = std::max(1e-3, expected.atb.norm() * 1e-4);
  EXPECT_EQ(actual.accepted_points, expected.accepted_points);
  EXPECT_LE((actual.ata - expected.ata).norm(), ata_budget);
  EXPECT_LE((actual.atb - expected.atb).norm(), atb_budget);
}

inline support::NormalEquation
toSupportEquation(const prod_detail::PointToPlaneLLSNormalEquation& production_eq)
{
  support::NormalEquation eq;
  eq.ata = production_eq.ata;
  eq.atb = production_eq.atb;
  eq.accepted_points = production_eq.accepted_points;
  return eq;
}

inline void
copyProductionStats(const prod_detail::PointToPlaneLLSFullCloudStats& production_stats,
                    support::AccumulationStats* stats)
{
  if (!stats)
    return;
  stats->input_points = production_stats.input_points;
  stats->accepted_points = production_stats.accepted_points;
  stats->used_rvv = production_stats.used_rvv;
}

template <typename PointSource, typename PointTarget>
inline support::NormalEquation
buildProductionDefaultEquation(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    support::AccumulationStats* stats)
{
#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation production_eq;
  prod_detail::PointToPlaneLLSFullCloudStats production_stats;
  if (prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
          source, target, production_eq, &production_stats)) {
    copyProductionStats(production_stats, stats);
    return toSupportEquation(production_eq);
  }
#endif
  return support::accumulate_std_full(source, target, stats);
}

inline Eigen::Matrix4f
solveTestEquation(support::NormalEquation eq)
{
  return support::solve_normal_equation(eq);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_test
