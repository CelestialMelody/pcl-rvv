/*
 * 本文件做什么：
 * 这些专项测试覆盖 point-to-plane LLS normal-equation（法方程）构造的
 * scalar reference（标量参考链路）和 RVV candidate（RVV 候选链路）。
 * main() 由 gtest 提供；Makefile 会分别构建 std 和 RVV 二进制。
 *
 * 阅读提示：
 * - 大规模全云 case 证明连续 PointNormal 输入可以命中 RVV staging（暂存阶段）。
 * - source-indexed（source 单侧索引）case 证明 source gather 与 target stride load 可分开审查。
 * - dual-indices（双侧索引）case 使用独立 target index stream，避免把它混同为 correspondences。
 * - correspondences（对应关系）case 证明 query/match 展开后的 indexed row 路径能处理乱序和重复匹配。
 * - small fallback（小规模回退路径）和 invalid lane（无效 lane）case 分别隔离规模 gate
 *   和 finite mask（有限值掩码），避免用一个混合 case 误代表所有 fallback。
 *
 * Closeout 测试清单：
 * - production direct：ProductionFullCloud*，覆盖真实 public full-cloud overload 下的
 *   f32 AoS layout-gated source xyz / target xyz+normal / Scalar=float dispatch，
 *   normal equation、matrix 和 invalid lane。
 * - fallback：ProductionFullCloudSmallInputFallsBackToScalar、
 *   ProductionFullCloudScalarDoubleFallbackSmoke、SmallInputFallsBackForIsolatedSizeGate。
 * - RVV-only diagnostic：FullCloudBlockReduction* 和 InvalidLaneMaskMatchesStd，保护当前
 *   production block-reduction 的局部语义合同。
 * - historical diagnostic：source-indexed、dual-indices、correspondences、trusted-dense、
 *   fused/grouped，用于解释已拒绝扩展方向，不构成 production evidence。
 */

#include "test_support_transformation_estimation_point_to_plane_lls.hpp"

#include <pcl/common/transforms.h>
#include <pcl/register_point_struct.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>

#include <gtest/gtest.h>

#include <algorithm>
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

namespace support = pcl::registration::rvv_te_pt2plane_lls_support;
namespace prod_detail = pcl::registration::detail;

namespace {

pcl::PointCloud<pcl::PointNormal>
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

Eigen::Matrix4f
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

pcl::PointCloud<pcl::PointNormal>
makeTargetCloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // target 由 PCL 公共 transform helper 生成，让测试输入保持真实 PointNormal 布局。
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, makeTransform());
  return target;
}

pcl::PointCloud<pcl::PointXYZ>
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

pcl::PointCloud<TEPTPLDoubleXYZPoint>
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

pcl::PointCloud<pcl::PointXYZINormal>
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

pcl::PointCloud<pcl::PointNormal>
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

std::pair<pcl::PointCloud<pcl::PointNormal>, pcl::PointCloud<pcl::PointNormal>>
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

pcl::Correspondences
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

pcl::Correspondences
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

pcl::Indices
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

pcl::Indices
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

pcl::Correspondences
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
pcl::PointCloud<PointT>
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

void
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

void
expectProductionEquationNear(
    const prod_detail::PointToPlaneLLSNormalEquation& actual,
    const prod_detail::PointToPlaneLLSNormalEquation& expected)
{
  const double ata_budget = std::max(1e-3, expected.ata.norm() * 1e-4);
  const double atb_budget = std::max(1e-3, expected.atb.norm() * 1e-4);
  EXPECT_EQ(actual.accepted_points, expected.accepted_points);
  EXPECT_LE((actual.ata - expected.ata).norm(), ata_budget);
  EXPECT_LE((actual.atb - expected.atb).norm(), atb_budget);
}

template <typename PointSource, typename PointTarget>
prod_detail::PointToPlaneLLSNormalEquation
buildProductionDefaultEquation(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    prod_detail::PointToPlaneLLSFullCloudStats* stats)
{
#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation eq;
  if (prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
          source, target, eq, stats))
    return eq;
#endif
  return prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, stats);
}

Eigen::Matrix4f
solveProductionEquation(prod_detail::PointToPlaneLLSNormalEquation eq)
{
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  prod_detail::solvePointToPlaneLLSNormalEquation(eq, matrix);
  return matrix;
}

} // namespace

// 这个测试验证 test-rvv 的标量诊断入口是否复刻当前公开 estimator 的输出。
// RVV 构建中的 full-cloud public overload 可能命中 production block-reduction
// dispatch，因此这里按 reduction-tree 变化后的 production 预算对拍。
TEST(TransformationEstimationPointToPlaneLLS, StdDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(14, 0.22f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_full(source, target, &stats);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
#ifdef __RVV10__
  expectMatrixNear(diagnostic_matrix, public_matrix, 5e-4f);
#else
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
#endif
}

// 这个测试重建 source indices + target full-cloud 公开 overload 的数据流：source 通过
// indices 取点，target 是按 row 顺序压好的紧凑全云。如果失败，说明公开入口和诊断
// reference 的 row 枚举语义已经不一致。
TEST(TransformationEstimationPointToPlaneLLS,
     StdSourceIndicesDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target_full = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const auto target = copyIndexedCloud(target_full, source_indices);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, source_indices, target, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_source_indices(source, source_indices, target, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}

// 这个测试重建 source+target indices 公开 overload 的数据流。target 使用独立 index
// stream，证明 dual-indices diagnostic 不是把同一组 indices 复制到两侧。
TEST(TransformationEstimationPointToPlaneLLS,
     StdDualIndicesDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target = makeTargetCloud(source);
  pcl::Indices source_indices = makeIndexedRows(source.size());
  pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const std::size_t paired_rows = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(paired_rows);
  target_indices.resize(paired_rows);
  EXPECT_NE(source_indices, target_indices);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_dual_indices(source, source_indices, target, target_indices, &stats);

  EXPECT_EQ(stats.input_points, paired_rows);
  EXPECT_EQ(stats.accepted_points, paired_rows);
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}

// 这个测试重建 correspondences 公开 overload 的数据流：row 来自 index_query/index_match。
// 它只证明有效 query/match 输入下的公开入口一致性，不定义非法 index 行为。
TEST(TransformationEstimationPointToPlaneLLS,
     StdCorrespondencesDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeShuffledCorrespondences(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}

// 这个测试验证连续 PointNormal 全云输入下，RVV candidate（RVV 候选链路）与标量参考链路保持一致。
// 如果 RVV 构建没有命中 staging 或矩阵超差，说明 full-cloud 主路径证据或数值 gate 断了。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimate_std_full(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_full(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

// fused-reduction diagnostic 不走 vcompress + buffer + scalar lane tail；它改变跨 lane
// 累加树，因此使用独立误差预算，只用于判断后续优化方向是否值得继续。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudFusedReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_fused_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// grouped-reduction diagnostic 用 chunk-local vector reduction 写回标量 normal-equation，
// 目的是降低 27 个 accumulator 长期活跃带来的寄存器压力；它仍改变加法树。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudGroupedReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_grouped_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// grouped invalid-lane 诊断保护 finite mask 进入 reduction-tree 前清零的语义。
// 它解释历史 grouped 方案为何数值上可控，但不能证明 production dispatch 或性能。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudGroupedReductionInvalidLanesWithinBudget)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_grouped_reduction(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}

// scale-stress 诊断给 grouped reduction 一个高动态范围样本，避免只在温和曲面上对拍。
// grouped 已因板卡证据暂缓；这个测试保留为历史方案回归，不支撑当前 production 范围。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudGroupedReductionScaleStressWithinBudget)
{
  const auto source = makeScaleStressCloud();
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_grouped_reduction(source, target, &candidate_stats);

  const double ata_budget = std::max(1.0, std_eq.ata.norm() * 1e-4);
  const double atb_budget = std::max(1.0, std_eq.atb.norm() * 1e-4);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_LE((candidate_eq.ata - std_eq.ata).norm(), ata_budget);
  EXPECT_LE((candidate_eq.atb - std_eq.atb).norm(), atb_budget);
}

// block-reduction diagnostic 按 a/b/c/normal 四组在一个 row block 内累计 partial
// sums。它降低同时活跃 accumulator 数量，但重复 load/formula，并改变 reduction tree。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudBlockReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// block invalid-lane 诊断是当前 production block-reduction 的局部保护：NaN/Inf lane
// 必须被 mask 成零并不计入 accepted_points。它不覆盖 public dispatch gate。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudBlockReductionInvalidLanesWithinBudget)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// block scale-stress 诊断保护 reduction-tree 误差预算：高动态范围样本下 ATA/ATb
// 允许相对误差，但 accepted_points 和矩阵合同仍需对齐标量。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudBlockReductionScaleStressWithinBudget)
{
  const auto source = makeScaleStressCloud();
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &candidate_stats);

  const double ata_budget = std::max(1.0, std_eq.ata.norm() * 1e-4);
  const double atb_budget = std::max(1.0, std_eq.atb.norm() * 1e-4);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_LE((candidate_eq.ata - std_eq.ata).norm(), ata_budget);
  EXPECT_LE((candidate_eq.atb - std_eq.atb).norm(), atb_budget);
}

// fused-formula block 变体只替换 a/b/c/d 的 lane 内公式，保留 current block 的
// A/B/C/N 分组。这个探索测试证明它在现有正常样本下仍落在 normal-equation 预算内；
// 近似抵消和 scale-stress 的额外风险由后面的专门样本覆盖。
TEST(TransformationEstimationPointToPlaneLLS,
     FullCloudBlockFusedFormulaReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(20, 0.14f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_fused_formula_reduction(
          source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// fused-formula near-cancellation 样本专门把 d 公式的 dx-sx、dy-sy、dz-sz 做成“两个大项
// 相减只剩小余量”的形态，同时叠加 scale-stress 和一个无效 lane。它用来约束：
// 1) accepted_points 只统计 finite row；
// 2) ATA/ATb 和矩阵仍在预算内；
// 3) 逐点 fused 写法不会在抵消区间里比 current block 更脆弱。
TEST(TransformationEstimationPointToPlaneLLS,
     FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget)
{
  const auto [source, target] = makeNearCancellationCloudPair();

  support::AccumulationStats std_stats;
  support::AccumulationStats current_stats;
  support::AccumulationStats fused_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation current_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &current_stats);
  const support::NormalEquation fused_eq =
      support::accumulate_candidate_full_block_fused_formula_reduction(
          source, target, &fused_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f current_matrix = support::solve_normal_equation(current_eq);
  const Eigen::Matrix4f fused_matrix = support::solve_normal_equation(fused_eq);

  EXPECT_EQ(std_stats.input_points, source.size());
  EXPECT_EQ(current_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(current_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(current_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(current_stats.used_rvv);
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(current_stats.used_rvv);
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  const double current_ata_delta = (current_eq.ata - std_eq.ata).norm();
  const double current_atb_delta = (current_eq.atb - std_eq.atb).norm();
  const double fused_ata_delta = (fused_eq.ata - std_eq.ata).norm();
  const double fused_atb_delta = (fused_eq.atb - std_eq.atb).norm();
  const double ata_budget = std::max(256.0, std_eq.ata.norm() * 1e-4);
  const double atb_budget = std::max(16.0, std_eq.atb.norm() * 1e-4);
  EXPECT_LE(fused_ata_delta, ata_budget);
  EXPECT_LE(fused_atb_delta, atb_budget);
  EXPECT_LE(fused_ata_delta, std::max(1.0, current_ata_delta * 1.25));
  EXPECT_LE(fused_atb_delta, std::max(1.0, current_atb_delta * 1.25));
  expectMatrixNear(current_matrix, std_matrix, 8e-4f);
  expectMatrixNear(fused_matrix, std_matrix, 8e-4f);
}

// Public-entry-shaped check: inputs and output matrix follow the public
// full-cloud overload contract, while the RVV side still calls only the
// test-rvv block shim. This proves shape compatibility, not production dispatch.
TEST(TransformationEstimationPointToPlaneLLS,
     FullCloudBlockReductionPublicEntryShapeMatchesPublicWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f block_matrix =
      support::estimate_candidate_full_block_reduction(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, source.size());
  EXPECT_EQ(candidate_stats.accepted_points, source.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(block_matrix, public_matrix, 5e-4f);
}

// Production dispatch check: std/RVV builds both call the same public full-cloud
// overload. In the RVV build this should hit the exact PointNormal,float gate;
// the tolerance acknowledges the changed RVV reduction tree.
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudPublicOverloadMatrixMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const Eigen::Matrix4f std_matrix = solveProductionEquation(std_eq);

  EXPECT_EQ(std_stats.input_points, source.size());
  EXPECT_EQ(std_stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, std_matrix, 5e-4f);
}

// Production direct normal-equation check: 直接比较 production helper 构造出的
// accepted_points、ATA 和 ATb。它比只看 matrix 更早捕获 reduction-tree 或 mask 回归。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudNormalEquationMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(solveProductionEquation(candidate_eq), solveProductionEquation(std_eq), 5e-4f);
}

// Production direct invalid-lane check: public overload、production helper 和标量 helper
// 必须对 NaN/Inf 行作同样剔除。它不覆盖 indexed/correspondences 的 iterator 入口。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudInvalidLanesMatchStdWithinBudget)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 5e-4f);
}

// Production direct scale-stress check: 复核 full-cloud dispatch 在较大数值范围下仍落在
// production 误差预算内。失败时优先审查 block-reduction 的加法树与求解矩阵。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudScaleStressMatchesStdWithinBudget)
{
  const auto source = makeScaleStressCloud();
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 5e-4f);
}

// Generic production direct check: source 侧换成 PointXYZ，证明 RVV gate 只要求 source
// xyz 的 f32 AoS layout，不再要求 exact PointNormal。invalid lane 仍必须按同一 finite
// mask 剔除，并保持 accepted_points、ATA/ATb 和 matrix 预算。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudGenericPointXYZInvalidLanesMatchStdWithinBudget)
{
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source_normal);
  auto source = copySourceAsXYZ(source_normal);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 5e-4f);
}

// Generic public-overload matrix check: PointXYZ source + PointNormal target 是原模板标量
// 合同可编译的常见组合。本测试证明 full-cloud public overload 在泛型 RVV gate 下仍与
// 标量 normal-equation reference 对齐；QEMU correctness 不代表板卡性能结论。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudGenericPointXYZScaleStressMatchesStdWithinBudget)
{
  const auto source_normal = makeScaleStressCloud();
  const auto target = makeTargetCloud(source_normal);
  const auto source = copySourceAsXYZ(source_normal);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 5e-4f);
}

// Generic target smoke: target 使用 PointXYZINormal，证明 target gate 需要 xyz+normal
// f32 AoS layout，而不是 exact PointNormal。这个 case 仍只覆盖 full-cloud/Scalar=float。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudGenericTargetXYZINormalMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto target_normal = makeTargetCloud(source_normal);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(target_normal);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                             pcl::PointXYZINormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 5e-4f);
}

// fused production path 的 PointNormal 代表样本：用 near-cancellation + scale-stress
// 形态覆盖 a/b/c/d 的逐点公式树、accepted_points、ATA/ATb 和矩阵预算。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudPointNormalNearCancellationMatchesStdWithinBudget)
{
  const auto [source, target] = makeNearCancellationCloudPair();
  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats fused_stats;
  const auto std_eq = prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto fused_eq =
      buildProductionDefaultEquation(source, target, &fused_stats);

  EXPECT_EQ(std_stats.input_points, source.size());
  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(solveProductionEquation(fused_eq), solveProductionEquation(std_eq), 8e-4f);
}

// fused production path 的 generic source 样本：PointXYZ source + PointNormal target。
// invalid lane 必须继续用同一 finite mask 剔除，不能改变 accepted_points 或 matrix。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudGenericPointXYZInvalidLanesMatchStdWithinBudget)
{
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source_normal);
  auto source = copySourceAsXYZ(source_normal);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats fused_stats;
  const auto std_eq = prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto fused_eq =
      buildProductionDefaultEquation(source, target, &fused_stats);

  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(solveProductionEquation(fused_eq), solveProductionEquation(std_eq), 5e-4f);
}

// fused production path 的 generic target 样本：PointXYZINormal target 证明 target gate 仍
// 只要求 xyz+normal 的 f32 AoS 语义，不要求 exact PointNormal。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudPointXYZToPointXYZINormalScaleStressMatchesStdWithinBudget)
{
  const auto source_normal = makeScaleStressCloud();
  const auto target_normal = makeTargetCloud(source_normal);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(target_normal);

  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSFullCloudStats fused_stats;
  const auto std_eq = prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);
  const auto fused_eq =
      buildProductionDefaultEquation(source, target, &fused_stats);

  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(solveProductionEquation(fused_eq), solveProductionEquation(std_eq), 5e-4f);
}

// fused production path 的 fallback smoke：小规模输入必须继续走标量。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.2f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats stats;
#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation rvv_eq;
  EXPECT_FALSE(prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
      source, target, rvv_eq, &stats));
  EXPECT_FALSE(stats.used_rvv);
#endif
  const auto std_eq = prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 1e-3f);
}

// fused production path 的 fallback smoke：layout gate 失败时也必须回到标量。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudLayoutGateFailureFallsBackToScalar)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source_normal);
  const auto source = copySourceAsDoubleXYZ(source_normal);
  static_assert(!pcl::rvv::RVVXYZAoSFloatLayout<TEPTPLDoubleXYZPoint>::value);

  pcl::registration::TransformationEstimationPointToPlaneLLS<TEPTPLDoubleXYZPoint,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats fused_stats;
  prod_detail::PointToPlaneLLSFullCloudStats std_stats;
  const auto fused_eq = buildProductionDefaultEquation(source, target, &fused_stats);
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &std_stats);

#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation rvv_eq;
  prod_detail::PointToPlaneLLSFullCloudStats rvv_stats;
  EXPECT_FALSE(prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
      source, target, rvv_eq, &rvv_stats));
  EXPECT_FALSE(rvv_stats.used_rvv);
#endif
  EXPECT_FALSE(fused_stats.used_rvv);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 5e-4f);
}

// Production fallback check: n < 64 时即使是 PointNormal/float/full-cloud 也不得进入
// RVV block path。它保护小规模输入的标量语义和 dispatch 成本边界。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.2f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSFullCloudStats stats;
#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation rvv_eq;
  EXPECT_FALSE(
      prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(source, target, rvv_eq, &stats));
  EXPECT_FALSE(stats.used_rvv);
#endif
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSFullCloudStd(source, target, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 1e-3f);
}

// Production fallback smoke: Scalar=double 仍不在本轮 RVV 范围内，必须继续走原标量
// public overload。点字段 float32 和输出 Scalar=float 是两条不同边界。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudScalarDoubleFallbackSmoke)
{
  const auto source_normal = makeSurfaceCloud(4, 0.2f);
  const auto target = makeTargetCloud(source_normal);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal,
                                                             double>
      double_estimator;
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();
  double_estimator.estimateRigidTransformation(source_normal, target, double_matrix);
  EXPECT_TRUE(double_matrix.allFinite());
#ifdef __RVV10__
  Eigen::Matrix4d rvv_matrix = Eigen::Matrix4d::Identity();
  EXPECT_FALSE(prod_detail::estimatePointToPlaneLLSFullCloudRVV(
      source_normal, target, rvv_matrix));
#endif
}

// trusted-dense diagnostic 是一个显式消融入口：它依赖 source/target 的 is_dense 合同，
// 跳过 finite mask 和 vcompress。它只用于评估未来 production 若授权 dense gate 是否
// 值得推进；当前 production 仍逐点检查 finite。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudTrustedDenseCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  ASSERT_TRUE(source.is_dense);
  ASSERT_TRUE(target.is_dense);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimate_std_full(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_full_trusted_dense(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

// 这个测试验证 test-rvv-only SourceIndexedRowSource policy：source 侧按 indices gather，
// target 侧按紧凑全云 stride load。失败说明单侧 gather 取数或 shared math pipeline
// 与标量 same-chain（同构链路）不一致。
TEST(TransformationEstimationPointToPlaneLLS, SourceIndicesCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target_full = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const auto target = copyIndexedCloud(target_full, source_indices);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_source_indices(source, source_indices, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_source_indices(source, source_indices, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// source-indexed trusted-dense 是历史消融：跳过 mask/compress 观察单侧 gather 成本。
// 当前 production 没有 dense gate，也没有 indexed dispatch；它只保留为负向证据复核。
TEST(TransformationEstimationPointToPlaneLLS,
     SourceIndicesTrustedDenseCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target_full = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const auto target = copyIndexedCloud(target_full, source_indices);
  ASSERT_TRUE(source.is_dense);
  ASSERT_TRUE(target.is_dense);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_source_indices(source, source_indices, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_source_indices_trusted_dense(
          source, source_indices, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证 dual-indices policy 使用两条独立 index stream。它不解析
// pcl::Correspondence，因此失败只能说明双侧 gather 诊断链路断了，不支持把问题
// 单因归到 correspondences 展开或 gather 某一项。
TEST(TransformationEstimationPointToPlaneLLS, DualIndicesCandidateMatchesStd)
{
  auto source = makeSurfaceCloud(32, 0.10f);
  auto target = makeTargetCloud(source);
  pcl::Indices source_indices = makeIndexedRows(source.size());
  pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const std::size_t paired_rows = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(paired_rows);
  target_indices.resize(paired_rows);

  ASSERT_FALSE(source_indices.empty());
  ASSERT_FALSE(target_indices.empty());
  ASSERT_GE(source_indices.size(), std::size_t{12});
  ASSERT_GE(target_indices.size(), std::size_t{12});
  EXPECT_NE(source_indices, target_indices);
  EXPECT_NE(std::find(source_indices.begin() + 2, source_indices.end(), source_indices[1]),
            source_indices.end());
  EXPECT_NE(std::find(target_indices.begin() + 2, target_indices.end(), target_indices[1]),
            target_indices.end());

  source[static_cast<std::size_t>(source_indices[1])].x =
      std::numeric_limits<float>::quiet_NaN();
  target[static_cast<std::size_t>(target_indices[2])].normal_z =
      std::numeric_limits<float>::infinity();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimate_std_dual_indices(
      source, source_indices, target, target_indices, &std_stats);
  const Eigen::Matrix4f candidate_matrix = support::estimate_candidate_dual_indices(
      source, source_indices, target, target_indices, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 4e-4f);
}

// 这个测试把同一条 index stream 同时用于 source 和 target。它是 correspondences
// same-index case 的公平对照：两者访问相关性接近，但 dual-indices 没有 query/match
// 展开阶段。失败说明 shared dual-index policy 自身不稳定。
TEST(TransformationEstimationPointToPlaneLLS, DualIndicesSameStreamCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices indices = makeIndexedRows(source.size());

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_dual_indices(source, indices, target, indices, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_dual_indices(source, indices, target, indices, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证 correspondences（对应关系）路径中的乱序、重复索引 gather（索引读取）不会改变求解结果。
// 如果失败，说明非连续访问语义、保序 staging 或对应关系路径的证据断了。
TEST(TransformationEstimationPointToPlaneLLS, CorrespondenceCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeShuffledCorrespondences(source.size());

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证 query/match 不相同但仍有局部性的 correspondences。它补上
// same-index 分布没有覆盖的匹配偏移，失败时说明 indexed row 语义或矩阵求解
// 对更真实的 correspondence 分布不稳。
TEST(TransformationEstimationPointToPlaneLLS,
     CorrespondenceLocalOffsetCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences =
      makeLocalOffsetCorrespondences(source.size());

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// 这个测试把 correspondences 构造成与 dual-indices independent-stream 相同的
// query/match 分布。它用于解释性能：若两者板卡结果不同，差异更可能来自
// correspondence 展开、容器布局或 baseline，而不是 index stream 本身。
TEST(TransformationEstimationPointToPlaneLLS,
     CorrespondenceIndependentStreamCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const pcl::Correspondences correspondences =
      makeCorrespondencesFromIndices(source_indices, target_indices);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// correspondence trusted-dense 是历史消融：在 independent-stream 分布上跳过
// mask/compress，帮助分离 correspondence 展开与后段有限值检查成本；它不是 production 证据。
TEST(TransformationEstimationPointToPlaneLLS,
     CorrespondenceIndependentStreamTrustedDenseCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const pcl::Correspondences correspondences =
      makeCorrespondencesFromIndices(source_indices, target_indices);
  ASSERT_TRUE(source.is_dense);
  ASSERT_TRUE(target.is_dense);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences_trusted_dense(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// 这个测试单独隔离小规模 fallback（回退路径）gate：输入太小时必须回到标量链路。
// 如果失败，说明规模阈值 gate 或 fallback 的 normal-equation 等价性断了。
TEST(TransformationEstimationPointToPlaneLLS, SmallInputFallsBackForIsolatedSizeGate)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats stats;
  const support::NormalEquation candidate =
      support::accumulate_candidate_full(source, target, &stats);
  const support::NormalEquation reference = support::accumulate_std_full(source, target);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(candidate.accepted_points, reference.accepted_points);
  EXPECT_NEAR((candidate.ata - reference.ata).norm(), 0.0, 1e-12);
  EXPECT_NEAR((candidate.atb - reference.atb).norm(), 0.0, 1e-12);
}

// 这个测试验证 NaN/Inf invalid lane（无效 lane）会被 finite mask（有限值掩码）剔除，并保持标量可见语义。
// 如果失败，说明 mask、vcompress（保序压缩）或压缩后标量 tail 的证据断了。
TEST(TransformationEstimationPointToPlaneLLS, InvalidLaneMaskMatchesStd)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}
