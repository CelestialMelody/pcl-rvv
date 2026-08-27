#include "min_cut_segmentation.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

pcl::PointCloud<pcl::PointXYZ>
makeCloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>((i * 17) % 4099) - 2048) * 0.013f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 29) % 4093) - 2046) * 0.017f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 43) % 4091) - 2045) * 0.011f;
  }
  return cloud;
}

std::vector<int>
makeIndices(const std::size_t n, const int stride)
{
  std::vector<int> indices;
  indices.reserve(n / static_cast<std::size_t>(stride) + 1);
  for (std::size_t i = 0; i < n; i += static_cast<std::size_t>(stride))
    indices.push_back(static_cast<int>(i));
  return indices;
}

std::vector<pcl::PointXYZ>
makeForeground(const std::size_t n)
{
  std::vector<pcl::PointXYZ> foreground(n);
  for (std::size_t i = 0; i < n; ++i) {
    foreground[i].x = static_cast<float>(static_cast<int>((i * 31) % 1021) - 510) * 0.025f;
    foreground[i].y = static_cast<float>(static_cast<int>((i * 37) % 1031) - 515) * 0.021f;
    foreground[i].z = 0.0f;
  }
  return foreground;
}

void
makeEdges(const std::size_t n, std::vector<int>& sources, std::vector<int>& targets)
{
  sources.clear();
  targets.clear();
  sources.reserve(n * 4);
  targets.reserve(n * 4);
  for (std::size_t i = 0; i < n; ++i) {
    sources.push_back(static_cast<int>(i));
    targets.push_back(static_cast<int>((i + 1) % n));
    sources.push_back(static_cast<int>(i));
    targets.push_back(static_cast<int>((i * 7 + 13) % n));
  }
}

void
expectNearVectors(const std::vector<double>& actual,
                  const std::vector<double>& expected,
                  const double tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "i=" << i;
}

} // namespace

TEST(MinCutSegmentationPotentialDiag, UnaryPotentialMatchesScalar)
{
  const auto cloud = makeCloud(4096);
  const auto indices = makeIndices(cloud.size(), 1);
  const auto foreground = makeForeground(19);
  std::vector<double> expected;
  const auto expected_summary = pcl_rvv_segmentation_min_cut::computeUnaryPotentialsStd(
      cloud, indices, foreground, 3.8003856 * 3.8003856, 0.8, &expected);

#if defined(__RVV10__)
  std::vector<double> actual;
  const auto actual_summary = pcl_rvv_segmentation_min_cut::computeUnaryPotentialsRVV(
      cloud, indices, foreground, 3.8003856 * 3.8003856, 0.8, &actual);
  expectNearVectors(actual, expected, 2e-6);
  EXPECT_NEAR(actual_summary.sink_checksum, expected_summary.sink_checksum, 1e-2);
  EXPECT_DOUBLE_EQ(actual_summary.source_checksum, expected_summary.source_checksum);
#else
  EXPECT_GT(expected_summary.sink_checksum, 0.0);
#endif
}

TEST(MinCutSegmentationPotentialDiag, BinaryPotentialMatchesScalarWithinFloatExpBudget)
{
  const auto cloud = makeCloud(4096);
  std::vector<int> sources;
  std::vector<int> targets;
  makeEdges(cloud.size(), sources, targets);
  std::vector<double> expected;
  const auto expected_summary = pcl_rvv_segmentation_min_cut::computeBinaryPotentialsStd(
      cloud, sources, targets, 16.0, &expected);

#if defined(__RVV10__)
  std::vector<double> actual;
  const auto actual_summary = pcl_rvv_segmentation_min_cut::computeBinaryPotentialsRVV(
      cloud, sources, targets, 16.0, &actual);
  expectNearVectors(actual, expected, 2e-5);
  EXPECT_NEAR(actual_summary.weight_checksum, expected_summary.weight_checksum, 2e-3);
#else
  EXPECT_GT(expected_summary.weight_checksum, 0.0);
#endif
}

TEST(MinCutSegmentationPotentialDiag, BuildGraphPotentialBatchMatchesScalarWithinFloatBudget)
{
  const auto cloud = makeCloud(768);
  const auto indices = makeIndices(cloud.size(), 1);
  const auto foreground = makeForeground(19);
  const auto expected = pcl_rvv_segmentation_min_cut::computeBuildGraphPotentialBatchStd(
      cloud, indices, foreground, 3.8003856 * 3.8003856, 0.8, 16.0, 14);

#if defined(__RVV10__)
  const auto actual = pcl_rvv_segmentation_min_cut::computeBuildGraphPotentialBatchRVV(
      cloud, indices, foreground, 3.8003856 * 3.8003856, 0.8, 16.0, 14);
  EXPECT_EQ(actual.vertex_count, expected.vertex_count);
  EXPECT_EQ(actual.edge_count, expected.edge_count);
  EXPECT_EQ(actual.unary_edge_count, expected.unary_edge_count);
  EXPECT_EQ(actual.binary_edge_count, expected.binary_edge_count);
  EXPECT_NEAR(actual.capacity_checksum, expected.capacity_checksum, 2e-1);
#else
  EXPECT_GT(expected.vertex_count, 0u);
  EXPECT_GT(expected.edge_count, 0u);
  EXPECT_GT(expected.capacity_checksum, 0.0);
#endif
}
