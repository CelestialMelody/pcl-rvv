#include "approximate_voxel_grid_diag.hpp"

#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace {

using Diag = pcl_rvv_filters_approximate_voxel_grid::LeafHash;

template <typename PointT>
class ApproximateVoxelGridStdProbe : public pcl::ApproximateVoxelGrid<PointT>
{
public:
  using PointCloud = pcl::PointCloud<PointT>;

  void
  runStd(const typename PointCloud::ConstPtr& input,
         PointCloud& output,
         float lx,
         float ly,
         float lz,
         bool downsample_all_data = true)
  {
    this->setLeafSize(lx, ly, lz);
    this->setInputCloud(input);
    this->setDownsampleAllData(downsample_all_data);
    this->applyFilterStd(output);
  }
};

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

template <typename PointT>
pcl::PointCloud<PointT>
makeGenericCloud(std::size_t n, bool with_invalid = false)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    auto& point = cloud[i];
    point.x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.0031f;
    point.y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.0029f;
    point.z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.0027f;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>) {
      point.intensity = static_cast<float>((i * 5) % 101) * 0.25f;
    }
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB> ||
                       std::is_same_v<PointT, pcl::PointXYZRGBA>) {
      point.r = static_cast<std::uint8_t>((17 * i) & 0xffu);
      point.g = static_cast<std::uint8_t>((29 * i) & 0xffu);
      point.b = static_cast<std::uint8_t>((43 * i) & 0xffu);
      if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
        point.a = static_cast<std::uint8_t>((59 * i) & 0xffu);
    }
  }
  if (with_invalid) {
    cloud[3].x = std::numeric_limits<float>::quiet_NaN();
    cloud[67].y = std::numeric_limits<float>::infinity();
    cloud[129].z = -std::numeric_limits<float>::infinity();
  }
  return cloud;
}

template <typename PointT>
void
expectSameXYZ(const pcl::PointCloud<PointT>& expected, const pcl::PointCloud<PointT>& actual)
{
  ASSERT_EQ(actual.size(), expected.size());
  EXPECT_EQ(actual.width, expected.width);
  EXPECT_EQ(actual.height, expected.height);
  EXPECT_EQ(actual.is_dense, expected.is_dense);
  for (std::size_t i = 0; i < actual.size(); ++i) {
    EXPECT_FLOAT_EQ(actual[i].x, expected[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(actual[i].y, expected[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(actual[i].z, expected[i].z) << "i=" << i;
  }
}

void
expectSameIntensity(const pcl::PointCloud<pcl::PointXYZI>& expected,
                    const pcl::PointCloud<pcl::PointXYZI>& actual)
{
  expectSameXYZ(expected, actual);
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_FLOAT_EQ(actual[i].intensity, expected[i].intensity) << "i=" << i;
}

void
expectSameRGB(const pcl::PointCloud<pcl::PointXYZRGB>& expected,
              const pcl::PointCloud<pcl::PointXYZRGB>& actual)
{
  expectSameXYZ(expected, actual);
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_EQ(actual[i].rgba, expected[i].rgba) << "i=" << i;
}

void
expectSameRGBA(const pcl::PointCloud<pcl::PointXYZRGBA>& expected,
               const pcl::PointCloud<pcl::PointXYZRGBA>& actual)
{
  expectSameXYZ(expected, actual);
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_EQ(actual[i].rgba, expected[i].rgba) << "i=" << i;
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

TEST(ApproximateVoxelGridDiag, LeafHashGateCoversGenericXYZPointTypes)
{
  const auto small_cloud = makeCloud(16);
  const auto xyzi_cloud = makeGenericCloud<pcl::PointXYZI>(128);

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

TEST(ApproximateVoxelGridDiag, ProductionGenericPointXYZIMatchesScalar)
{
  const auto cloud = makeGenericCloud<pcl::PointXYZI>(2048, true);
  const float lx = 0.05f, ly = 0.06f, lz = 0.07f;

  ApproximateVoxelGridStdProbe<pcl::PointXYZI> probe;
  pcl::PointCloud<pcl::PointXYZI> expected;
  probe.runStd(cloud.makeShared(), expected, lx, ly, lz, true);

  pcl::ApproximateVoxelGrid<pcl::PointXYZI> filter;
  filter.setLeafSize(lx, ly, lz);
  filter.setInputCloud(cloud.makeShared());
  filter.setDownsampleAllData(true);
  pcl::PointCloud<pcl::PointXYZI> actual;
  filter.filter(actual);

  expectSameIntensity(expected, actual);
}

TEST(ApproximateVoxelGridDiag, ProductionGenericPointXYZIWithoutAllDataMatchesScalar)
{
  const auto cloud = makeGenericCloud<pcl::PointXYZI>(2048, true);
  const float lx = 0.05f, ly = 0.06f, lz = 0.07f;

  ApproximateVoxelGridStdProbe<pcl::PointXYZI> probe;
  pcl::PointCloud<pcl::PointXYZI> expected;
  probe.runStd(cloud.makeShared(), expected, lx, ly, lz, false);

  pcl::ApproximateVoxelGrid<pcl::PointXYZI> filter;
  filter.setLeafSize(lx, ly, lz);
  filter.setInputCloud(cloud.makeShared());
  filter.setDownsampleAllData(false);
  pcl::PointCloud<pcl::PointXYZI> actual;
  filter.filter(actual);

  expectSameIntensity(expected, actual);
}

TEST(ApproximateVoxelGridDiag, ProductionGenericPointXYZRGBMatchesScalar)
{
  const auto cloud = makeGenericCloud<pcl::PointXYZRGB>(2048, true);
  const float lx = 0.05f, ly = 0.06f, lz = 0.07f;

  ApproximateVoxelGridStdProbe<pcl::PointXYZRGB> probe;
  pcl::PointCloud<pcl::PointXYZRGB> expected;
  probe.runStd(cloud.makeShared(), expected, lx, ly, lz, true);

  pcl::ApproximateVoxelGrid<pcl::PointXYZRGB> filter;
  filter.setLeafSize(lx, ly, lz);
  filter.setInputCloud(cloud.makeShared());
  filter.setDownsampleAllData(true);
  pcl::PointCloud<pcl::PointXYZRGB> actual;
  filter.filter(actual);

  expectSameRGB(expected, actual);
}

TEST(ApproximateVoxelGridDiag, ProductionGenericPointXYZRGBAMatchesScalar)
{
  const auto cloud = makeGenericCloud<pcl::PointXYZRGBA>(2048, true);
  const float lx = 0.05f, ly = 0.06f, lz = 0.07f;

  ApproximateVoxelGridStdProbe<pcl::PointXYZRGBA> probe;
  pcl::PointCloud<pcl::PointXYZRGBA> expected;
  probe.runStd(cloud.makeShared(), expected, lx, ly, lz, true);

  pcl::ApproximateVoxelGrid<pcl::PointXYZRGBA> filter;
  filter.setLeafSize(lx, ly, lz);
  filter.setInputCloud(cloud.makeShared());
  filter.setDownsampleAllData(true);
  pcl::PointCloud<pcl::PointXYZRGBA> actual;
  filter.filter(actual);

  expectSameRGBA(expected, actual);
}
