/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 MomentInvariantsEstimation 中
 * centroid（质心）之后的三项 moment invariant（矩不变量）累加是否能由
 * test-only RVV candidate（仅测试使用的 RVV 候选）复刻。测试覆盖连续点云、
 * 非连续 indices（索引）和 tail（向量尾段），用于防止 RVV gather（离散加载）
 * 读错字段或漏算尾部样本。
 *
 * 证据边界：
 * production direct（真实生产路径）测试证明 public computeFeature（公开计算入口）
 * 的输出语义和 fallback（回退路径）。性能收益仍由板卡 repeated summary（重复板卡摘要）
 * 单独证明，QEMU（仿真器）测试不作为性能结论。
 */

#include "moment_invariants.h"

#include <pcl/features/moment_invariants.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/test/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace mi = pcl::features::rvv_test::moment_invariants;

namespace {

struct MomentTolerance {
  float j1_abs;
  float j1_rel;
  float j2_abs;
  float j2_rel;
  float j3_abs;
  float j3_rel;
};

constexpr MomentTolerance kReferenceTolerance{1.0e-3f, 5.0e-6f, 2.0e-2f, 5.0e-6f, 2.0e-1f, 2.0e-5f};
constexpr MomentTolerance kProductionDirectTolerance{1.0e-2f, 2.0e-5f, 2.0e-1f, 2.0e-5f, 1.0e2f, 2.0e-3f};

std::vector<pcl::PointXYZ>
makeCloud(const std::size_t count)
{
  std::vector<pcl::PointXYZ> cloud(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float f = static_cast<float>(i);
    cloud[i].x = 0.0625f * f - 3.0f;
    cloud[i].y = 0.25f * static_cast<float>((i * 5) % 23) - 2.5f;
    cloud[i].z = 1.25f + 0.015625f * f * f - 0.375f * static_cast<float>(i % 7);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
makePointCloud(const std::size_t count)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  const std::vector<pcl::PointXYZ> points = makeCloud(count);
  cloud.points.assign(points.begin(), points.end());
  cloud.width = static_cast<std::uint32_t>(cloud.points.size());
  cloud.height = 1;
  cloud.is_dense = true;
  return cloud;
}

template <typename PointT>
pcl::PointCloud<PointT>
makeTypedPointCloud(const std::size_t count)
{
  pcl::PointCloud<PointT> cloud;
  cloud.points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float f = static_cast<float>(i);
    cloud.points[i].x = 0.0625f * f - 3.0f;
    cloud.points[i].y = 0.25f * static_cast<float>((i * 5) % 23) - 2.5f;
    cloud.points[i].z = 1.25f + 0.015625f * f * f - 0.375f * static_cast<float>(i % 7);
  }
  cloud.width = static_cast<std::uint32_t>(cloud.points.size());
  cloud.height = 1;
  cloud.is_dense = true;
  return cloud;
}

pcl::Indices
makeSparseIndices()
{
  pcl::Indices indices;
  for (int i = 3; i < 131; i += 3)
    indices.push_back(i);
  indices.push_back(0);
  indices.push_back(127);
  indices.push_back(42);
  indices.push_back(128);
  return indices;
}

std::vector<int>
makeQueryIndices(const std::size_t count, const std::size_t step)
{
  std::vector<int> indices;
  for (std::size_t i = 0; i < count; i += step)
    indices.push_back(static_cast<int>(i));
  return indices;
}

void
expectClose(const float actual, const float expected, const float absolute_tolerance, const float relative_tolerance)
{
  const float tolerance = std::max(absolute_tolerance, std::fabs(expected) * relative_tolerance);
  EXPECT_NEAR(actual, expected, tolerance);
}

void
expectNear(const mi::MomentSummary& actual,
           const mi::MomentSummary& expected,
           const float absolute_tolerance,
           const float relative_tolerance)
{
  expectClose(actual.j1, expected.j1, absolute_tolerance, relative_tolerance);
  expectClose(actual.j2, expected.j2, absolute_tolerance, relative_tolerance);
  expectClose(actual.j3, expected.j3, absolute_tolerance, relative_tolerance);
  expectClose(actual.mu200, expected.mu200, absolute_tolerance, relative_tolerance);
  expectClose(actual.mu020, expected.mu020, absolute_tolerance, relative_tolerance);
  expectClose(actual.mu002, expected.mu002, absolute_tolerance, relative_tolerance);
  expectClose(actual.mu110, expected.mu110, absolute_tolerance, relative_tolerance);
  expectClose(actual.mu101, expected.mu101, absolute_tolerance, relative_tolerance);
  expectClose(actual.mu011, expected.mu011, absolute_tolerance, relative_tolerance);
}

void
expectMomentClose(const pcl::MomentInvariants& actual,
                  const mi::MomentSummary& expected,
                  const MomentTolerance& tolerance)
{
  expectClose(actual.j1, expected.j1, tolerance.j1_abs, tolerance.j1_rel);
  expectClose(actual.j2, expected.j2, tolerance.j2_abs, tolerance.j2_rel);
  expectClose(actual.j3, expected.j3, tolerance.j3_abs, tolerance.j3_rel);
}

template <typename PointT>
mi::MomentSummary
computeTypedMomentSummaryStd(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  Eigen::Vector4f centroid = Eigen::Vector4f::Zero();
  for (const int index : indices) {
    const auto& point = cloud[static_cast<std::size_t>(index)];
    centroid[0] += point.x;
    centroid[1] += point.y;
    centroid[2] += point.z;
  }
  if (!indices.empty())
    centroid /= static_cast<float>(indices.size());

  mi::MomentSummary summary;
  for (const int index : indices) {
    const auto& point = cloud[static_cast<std::size_t>(index)];
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
  mi::finalizeMomentSummary(summary);
  return summary;
}

template <typename PointT>
void
expectProductionComputeMatchesReferenceForSparseQueries()
{
  const auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>(makeTypedPointCloud<PointT>(512));
  auto queries = pcl::make_shared<pcl::Indices>(makeQueryIndices(cloud->size(), 5));
  auto tree = pcl::make_shared<pcl::search::KdTree<PointT>>(false);
  tree->setInputCloud(cloud);

  pcl::MomentInvariantsEstimation<PointT, pcl::MomentInvariants> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(queries);
  estimator.setSearchMethod(tree);
  estimator.setKSearch(32);

  pcl::PointCloud<pcl::MomentInvariants> output;
  estimator.compute(output);

  ASSERT_EQ(output.size(), queries->size());
  ASSERT_TRUE(output.is_dense);

  pcl::Indices nn_indices(32);
  std::vector<float> nn_dists(32);
  for (std::size_t i = 0; i < queries->size(); ++i) {
    const int found = tree->nearestKSearch((*queries)[i], 32, nn_indices, nn_dists);
    ASSERT_GT(found, 0);
    nn_indices.resize(static_cast<std::size_t>(found));
    const mi::MomentSummary expected = computeTypedMomentSummaryStd(*cloud, nn_indices);
    SCOPED_TRACE(testing::Message() << "query_index=" << (*queries)[i] << ", found=" << found);
    expectMomentClose(output[i], expected, kProductionDirectTolerance);
    nn_indices.resize(32);
  }
}

} // namespace

TEST(MomentInvariantsReference, MatchesProductionHelperForSparseIndices)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makePointCloud(140);
  const pcl::Indices indices = makeSparseIndices();

  pcl::MomentInvariantsEstimation<pcl::PointXYZ, pcl::MomentInvariants> estimator;
  float expected_j1 = 0.0f;
  float expected_j2 = 0.0f;
  float expected_j3 = 0.0f;
  estimator.computePointMomentInvariants(cloud, indices, expected_j1, expected_j2, expected_j3);

  const mi::MomentSummary reference = mi::computeMomentSummaryStd(cloud.points.data(), indices);

  expectClose(reference.j1, expected_j1, kReferenceTolerance.j1_abs, kReferenceTolerance.j1_rel);
  expectClose(reference.j2, expected_j2, kReferenceTolerance.j2_abs, kReferenceTolerance.j2_rel);
  expectClose(reference.j3, expected_j3, kReferenceTolerance.j3_abs, kReferenceTolerance.j3_rel);
}

TEST(MomentInvariantsCandidate, RVVMatchesReferenceForIndexedTail)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makePointCloud(140);
  const pcl::Indices indices = makeSparseIndices();

  const mi::MomentSummary expected = mi::computeMomentSummaryStd(cloud.points.data(), indices);
  const mi::MomentSummary actual = mi::computeMomentSummaryRVV(cloud.points.data(), indices);

  expectNear(actual, expected, 1.0e-2f, 2.0e-5f);
}

TEST(MomentInvariantsCandidate, RVVMatchesReferenceForFullCloudTail)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makePointCloud(137);

  const mi::MomentSummary expected = mi::computeMomentSummaryStd(cloud.points.data(), cloud.points.size());
  const mi::MomentSummary actual = mi::computeMomentSummaryRVV(cloud.points.data(), cloud.points.size());

  expectNear(actual, expected, 1.0e-2f, 2.0e-5f);
}

TEST(MomentInvariantsProductionDirect, ComputeFeatureMatchesReferenceForSparseQueries)
{
  const auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(makePointCloud(512));
  auto queries = pcl::make_shared<pcl::Indices>(makeQueryIndices(cloud->size(), 5));
  auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>(false);
  tree->setInputCloud(cloud);

  pcl::MomentInvariantsEstimation<pcl::PointXYZ, pcl::MomentInvariants> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(queries);
  estimator.setSearchMethod(tree);
  estimator.setKSearch(32);

  pcl::PointCloud<pcl::MomentInvariants> output;
  estimator.compute(output);

  ASSERT_EQ(output.size(), queries->size());
  ASSERT_TRUE(output.is_dense);

  pcl::Indices nn_indices(32);
  std::vector<float> nn_dists(32);
  for (std::size_t i = 0; i < queries->size(); ++i) {
    const int found = tree->nearestKSearch((*queries)[i], 32, nn_indices, nn_dists);
    ASSERT_GT(found, 0);
    nn_indices.resize(static_cast<std::size_t>(found));
    const mi::MomentSummary expected = mi::computeMomentSummaryStd(cloud->points.data(), nn_indices);
    SCOPED_TRACE(testing::Message() << "query_index=" << (*queries)[i] << ", found=" << found);
    // RVV vector reduction（向量规约）改变中心矩累加树；j3 又由六个中心矩三阶组合而成，
    // 小邻域 production direct case 需要比 helper-only same-chain 更宽的相对预算。
    expectMomentClose(output[i], expected, kProductionDirectTolerance);
    nn_indices.resize(32);
  }
}

TEST(MomentInvariantsProductionDirect, NonDenseInvalidQueryKeepsScalarFallback)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(makePointCloud(96));
  cloud->is_dense = false;
  (*cloud)[10].x = std::numeric_limits<float>::quiet_NaN();

  auto queries = pcl::make_shared<pcl::Indices>();
  queries->push_back(10);
  queries->push_back(20);

  auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>(false);
  tree->setInputCloud(cloud);

  pcl::MomentInvariantsEstimation<pcl::PointXYZ, pcl::MomentInvariants> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(queries);
  estimator.setSearchMethod(tree);
  estimator.setKSearch(24);

  pcl::PointCloud<pcl::MomentInvariants> output;
  estimator.compute(output);

  ASSERT_EQ(output.size(), queries->size());
  EXPECT_FALSE(output.is_dense);
  EXPECT_TRUE(std::isnan(output[0].j1));
  EXPECT_TRUE(std::isnan(output[0].j2));
  EXPECT_TRUE(std::isnan(output[0].j3));

  pcl::Indices nn_indices(24);
  std::vector<float> nn_dists(24);
  const int found = tree->nearestKSearch((*queries)[1], 24, nn_indices, nn_dists);
  ASSERT_GT(found, 0);
  nn_indices.resize(static_cast<std::size_t>(found));
  expectMomentClose(output[1], mi::computeMomentSummaryStd(cloud->points.data(), nn_indices),
                    kProductionDirectTolerance);
}

TEST(MomentInvariantsProductionDirect, PointXYZILayoutGateMatchesReference)
{
  expectProductionComputeMatchesReferenceForSparseQueries<pcl::PointXYZI>();
}

TEST(MomentInvariantsProductionDirect, PointXYZRGBLayoutGateMatchesReference)
{
  expectProductionComputeMatchesReferenceForSparseQueries<pcl::PointXYZRGB>();
}

TEST(MomentInvariantsProductionDirect, PointXYZRGBALayoutGateMatchesReference)
{
  expectProductionComputeMatchesReferenceForSparseQueries<pcl::PointXYZRGBA>();
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
