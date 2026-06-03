#include <pcl/filters/voxel_grid_covariance.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

using CloudXYZ = pcl::PointCloud<pcl::PointXYZ>;

CloudXYZ
makeClusteredCloud(std::size_t points_per_axis, bool dense)
{
  CloudXYZ cloud;
  const std::size_t n = points_per_axis * points_per_axis * points_per_axis * 4;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = dense;
  cloud.points.resize(n);

  std::size_t k = 0;
  for (std::size_t z = 0; z < points_per_axis; ++z)
    for (std::size_t y = 0; y < points_per_axis; ++y)
      for (std::size_t x = 0; x < points_per_axis; ++x)
      {
        const float bx = (static_cast<float>(x) - static_cast<float>(points_per_axis / 2)) * 0.12f;
        const float by = (static_cast<float>(y) - static_cast<float>(points_per_axis / 2)) * 0.11f;
        const float bz = (static_cast<float>(z) - static_cast<float>(points_per_axis / 2)) * 0.10f;
        cloud[k++] = pcl::PointXYZ(bx - 0.011f, by - 0.009f, bz - 0.007f);
        cloud[k++] = pcl::PointXYZ(bx + 0.013f, by - 0.004f, bz + 0.006f);
        cloud[k++] = pcl::PointXYZ(bx - 0.006f, by + 0.012f, bz + 0.004f);
        cloud[k++] = pcl::PointXYZ(bx + 0.008f, by + 0.007f, bz - 0.005f);
      }

  if (!dense)
  {
    for (std::size_t i = 0; i < cloud.size(); i += 211)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t i = 97; i < cloud.size(); i += 257)
      cloud[i].z = std::numeric_limits<float>::infinity();
  }
  return cloud;
}

CloudXYZ
runFilter(const CloudXYZ& input,
          bool save_leaf_layout = false,
          bool searchable = false,
          const std::string& field_name = std::string())
{
  pcl::VoxelGridCovariance<pcl::PointXYZ> grid;
  grid.setLeafSize(0.12f, 0.11f, 0.10f);
  grid.setMinPointPerVoxel(3);
  grid.setSaveLeafLayout(save_leaf_layout);
  if (!field_name.empty())
  {
    grid.setFilterFieldName(field_name);
    grid.setFilterLimits(-0.28f, 0.28f);
  }
  grid.setInputCloud(input.makeShared());
  CloudXYZ output;
  grid.filter(output, searchable);
  return output;
}

void
expectCloudNear(const CloudXYZ& actual, const CloudXYZ& expected)
{
  ASSERT_EQ(actual.size(), expected.size());
  EXPECT_EQ(actual.width, expected.width);
  EXPECT_EQ(actual.height, expected.height);
  EXPECT_EQ(actual.is_dense, expected.is_dense);
  for (std::size_t i = 0; i < actual.size(); ++i)
  {
    EXPECT_NEAR(actual[i].x, expected[i].x, 1e-5f) << "i=" << i;
    EXPECT_NEAR(actual[i].y, expected[i].y, 1e-5f) << "i=" << i;
    EXPECT_NEAR(actual[i].z, expected[i].z, 1e-5f) << "i=" << i;
  }
}

} // namespace

TEST(VoxelGridCovarianceRVV, DenseNegativeCoordinatesMatchScalarOutput)
{
  const auto cloud = makeClusteredCloud(5, true);
  const auto output = runFilter(cloud);

  ASSERT_FALSE(output.empty());
  EXPECT_LT(output.front().x, 0.0f);
  EXPECT_GT(output.back().x, -1.0f);
}

TEST(VoxelGridCovarianceRVV, SingleNegativeVoxelMatchesHandComputedCentroid)
{
  CloudXYZ cloud;
  cloud.width = 80;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(cloud.width);
  float sx = 0.0f;
  float sy = 0.0f;
  float sz = 0.0f;
  for (std::size_t i = 0; i < cloud.size(); ++i)
  {
    const float dx = static_cast<float>(static_cast<int>(i % 5) - 2) * 0.001f;
    const float dy = static_cast<float>(static_cast<int>(i % 7) - 3) * 0.0007f;
    const float dz = static_cast<float>(static_cast<int>(i % 3) - 1) * 0.0009f;
    cloud[i] = pcl::PointXYZ(-0.086f + dx, -0.077f + dy, -0.070f + dz);
    sx += cloud[i].x;
    sy += cloud[i].y;
    sz += cloud[i].z;
  }

  const auto output = runFilter(cloud);

  ASSERT_EQ(output.size(), 1u);
  EXPECT_NEAR(output[0].x, sx / static_cast<float>(cloud.size()), 1e-6f);
  EXPECT_NEAR(output[0].y, sy / static_cast<float>(cloud.size()), 1e-6f);
  EXPECT_NEAR(output[0].z, sz / static_cast<float>(cloud.size()), 1e-6f);
}

TEST(VoxelGridCovarianceRVV, SaveLeafLayoutAndSearchableRemainUsable)
{
  const auto cloud = makeClusteredCloud(5, true);
  pcl::VoxelGridCovariance<pcl::PointXYZ> grid;
  grid.setLeafSize(0.12f, 0.11f, 0.10f);
  grid.setMinPointPerVoxel(3);
  grid.setSaveLeafLayout(true);
  grid.setInputCloud(cloud.makeShared());

  CloudXYZ output;
  grid.filter(output, true);

  ASSERT_FALSE(output.empty());
  EXPECT_NE(grid.getCentroidIndex(output.front()), -1);

  std::vector<pcl::VoxelGridCovariance<pcl::PointXYZ>::LeafConstPtr> leaves;
  std::vector<float> distances;
  grid.nearestKSearch(output.front(), 1, leaves, distances);
  EXPECT_EQ(leaves.size(), 1u);
  EXPECT_GE(leaves.front()->getPointCount(), 3);
}

TEST(VoxelGridCovarianceRVV, DistanceFieldFallbackKeepsFilterSemantics)
{
  const auto cloud = makeClusteredCloud(5, true);
  const auto output = runFilter(cloud, false, false, "z");

  pcl::VoxelGridCovariance<pcl::PointXYZ> grid;
  grid.setLeafSize(0.12f, 0.11f, 0.10f);
  grid.setMinPointPerVoxel(3);
  grid.setFilterFieldName("z");
  grid.setFilterLimits(-0.28f, 0.28f);
  grid.setInputCloud(cloud.makeShared());
  CloudXYZ expected;
  grid.filter(expected);

  expectCloudNear(output, expected);
}

TEST(VoxelGridCovarianceRVV, NonDenseFallbackSkipsInvalidPoints)
{
  const auto cloud = makeClusteredCloud(6, false);
  const auto output = runFilter(cloud);
  const auto expected = runFilter(cloud);
  expectCloudNear(output, expected);
}

TEST(VoxelGridCovarianceRVV, SmallInputFallbackWorks)
{
  CloudXYZ cloud;
  cloud.width = 4;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points = {
      pcl::PointXYZ(-0.01f, 0.00f, 0.00f),
      pcl::PointXYZ( 0.01f, 0.00f, 0.00f),
      pcl::PointXYZ( 0.00f, 0.02f, 0.00f),
      pcl::PointXYZ( 0.00f, 0.00f, 0.02f)};

  const auto output = runFilter(cloud);
  const auto expected = runFilter(cloud);
  expectCloudNear(output, expected);
}
