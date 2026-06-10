#include "approximate_voxel_grid_diag.hpp"

#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <limits>

namespace {

using Diag = pcl_rvv_filters_approximate_voxel_grid::LeafHash;

pcl::PointCloud<pcl::PointXYZ>
makeCloud(std::size_t n, bool with_invalid = false)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.0031f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.0029f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.0027f;
  }
  if (with_invalid) {
    cloud[3].x = std::numeric_limits<float>::quiet_NaN();
    cloud[67].y = std::numeric_limits<float>::infinity();
    cloud[129].z = -std::numeric_limits<float>::infinity();
  }
  return cloud;
}

void
expectSameLeaves(const std::vector<Diag>& actual, const std::vector<Diag>& expected)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    EXPECT_EQ(actual[i], expected[i]) << "leaf i=" << i;
  }
}

} // namespace

TEST(ApproximateVoxelGridDiag, ScalarFormulaCoversNegativeFloorAndHash)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 4;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points = {pcl::PointXYZ(0.49f, 0.50f, 1.01f),
                  pcl::PointXYZ(-0.01f, -0.50f, -1.01f),
                  pcl::PointXYZ(-1.00f, 1.99f, -0.001f),
                  pcl::PointXYZ(2.00f, -2.00f, 0.00f)};
  const Eigen::Array3f inverse_leaf(2.0f, 2.0f, 2.0f);

  const auto leaves =
      pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesStd(cloud, inverse_leaf, 512);

  ASSERT_EQ(leaves.size(), 4u);
  EXPECT_EQ(leaves[0].ix, 0);
  EXPECT_EQ(leaves[0].iy, 1);
  EXPECT_EQ(leaves[0].iz, 2);
  EXPECT_EQ(leaves[1].ix, -1);
  EXPECT_EQ(leaves[1].iy, -1);
  EXPECT_EQ(leaves[1].iz, -3);
  EXPECT_EQ(leaves[2].ix, -2);
  EXPECT_EQ(leaves[2].iy, 3);
  EXPECT_EQ(leaves[2].iz, -1);
}

TEST(ApproximateVoxelGridDiag, RVVLeafHashMatchesScalar)
{
  const auto cloud = makeCloud(1024);
  const Eigen::Array3f inverse_leaf(4.0f, 3.0f, 5.0f);
  const auto expected =
      pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesStd(cloud, inverse_leaf, 512);

#if defined(__RVV10__)
  std::vector<Diag> actual;
  ASSERT_TRUE(pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesRVV(
      cloud, inverse_leaf, 512, actual));
  expectSameLeaves(actual, expected);
#else
  EXPECT_FALSE(expected.empty());
#endif
}

TEST(ApproximateVoxelGridDiag, RVVFiniteMaskSkipsInvalidXYZLikeScalar)
{
  const auto cloud = makeCloud(1024, true);
  const Eigen::Array3f inverse_leaf(4.0f, 3.0f, 5.0f);
  const auto expected =
      pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesStd(cloud, inverse_leaf, 512);

#if defined(__RVV10__)
  std::vector<Diag> actual;
  ASSERT_TRUE(pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesRVV(
      cloud, inverse_leaf, 512, actual));
  expectSameLeaves(actual, expected);
#else
  EXPECT_EQ(expected.size(), cloud.size() - 3);
#endif
}

TEST(ApproximateVoxelGridDiag, FullPointXYZDiagnosticMatchesScalar)
{
  const auto cloud = makeCloud(4096, true);
  const Eigen::Array3f inverse_leaf(4.0f, 3.0f, 5.0f);
  const auto expected =
      pcl_rvv_filters_approximate_voxel_grid::approximateVoxelGridPointXYZStd(cloud, inverse_leaf, 512);

#if defined(__RVV10__)
  pcl::PointCloud<pcl::PointXYZ> actual;
  ASSERT_TRUE(pcl_rvv_filters_approximate_voxel_grid::approximateVoxelGridPointXYZRVV(
      cloud, inverse_leaf, 512, actual));
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    EXPECT_FLOAT_EQ(actual[i].x, expected[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(actual[i].y, expected[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(actual[i].z, expected[i].z) << "i=" << i;
  }
#else
  EXPECT_FALSE(expected.empty());
#endif
}

TEST(ApproximateVoxelGridDiag, FallbackCasesRemainScalar)
{
  const auto small_cloud = makeCloud(16);
  const auto xyzi_cloud = [] {
    pcl::PointCloud<pcl::PointXYZI> cloud;
    cloud.width = 128;
    cloud.height = 1;
    cloud.is_dense = true;
    cloud.points.resize(128);
    for (std::size_t i = 0; i < cloud.size(); ++i) {
      cloud[i].x = static_cast<float>(i) * 0.01f;
      cloud[i].y = static_cast<float>(i) * -0.02f;
      cloud[i].z = static_cast<float>(i) * 0.03f;
      cloud[i].intensity = static_cast<float>(i % 7);
    }
    return cloud;
  }();

#if defined(__RVV10__)
  const Eigen::Array3f inverse_leaf(4.0f, 3.0f, 5.0f);
  std::vector<Diag> actual;
  EXPECT_FALSE(pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesRVV(
      small_cloud, inverse_leaf, 512, actual));
  EXPECT_TRUE(pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesRVV(
      xyzi_cloud, inverse_leaf, 512, actual));
#else
  EXPECT_EQ(small_cloud.size(), 16u);
  EXPECT_EQ(xyzi_cloud.size(), 128u);
#endif
}

TEST(ApproximateVoxelGridDiag, ProductionApproximateVoxelGridStillRuns)
{
  const auto cloud = makeCloud(2048, true);
  const Eigen::Array3f inverse_leaf(1.0f / 0.05f, 1.0f / 0.06f, 1.0f / 0.07f);
  const auto expected =
      pcl_rvv_filters_approximate_voxel_grid::approximateVoxelGridPointXYZStd(cloud, inverse_leaf, 512);

  pcl::ApproximateVoxelGrid<pcl::PointXYZ> filter;
  filter.setLeafSize(0.05f, 0.06f, 0.07f);
  filter.setInputCloud(cloud.makeShared());

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  ASSERT_FALSE(output.empty());
  EXPECT_TRUE(output.is_dense);
  EXPECT_EQ(output.height, 1u);
  ASSERT_EQ(output.size(), expected.size());
  for (std::size_t i = 0; i < output.size(); ++i) {
    EXPECT_FLOAT_EQ(output[i].x, expected[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].y, expected[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].z, expected[i].z) << "i=" << i;
  }
}
