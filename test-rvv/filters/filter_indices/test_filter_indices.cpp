#include <pcl/filters/filter.h>
#include <pcl/filters/filter_indices.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace {

using PointCloudXYZ = pcl::PointCloud<pcl::PointXYZ>;

PointCloudXYZ
makeXYZCloud(bool dense = false)
{
  PointCloudXYZ cloud;
  cloud.width = 8;
  cloud.height = 1;
  cloud.is_dense = dense;
  cloud.points.resize(cloud.width * cloud.height);

  cloud[0] = pcl::PointXYZ(1.0f, 2.0f, 3.0f);
  cloud[1] = pcl::PointXYZ(std::numeric_limits<float>::quiet_NaN(), 2.0f, 3.0f);
  cloud[2] = pcl::PointXYZ(4.0f, 5.0f, 6.0f);
  cloud[3] = pcl::PointXYZ(7.0f, std::numeric_limits<float>::infinity(), 9.0f);
  cloud[4] = pcl::PointXYZ(-1.0f, -2.0f, -3.0f);
  cloud[5] = pcl::PointXYZ(1.0f, 2.0f, -std::numeric_limits<float>::infinity());
  cloud[6] = pcl::PointXYZ(0.0f, 0.0f, 0.0f);
  cloud[7] = pcl::PointXYZ(9.0f, 8.0f, 7.0f);
  return cloud;
}

PointCloudXYZ
makeLargeXYZCloud()
{
  PointCloudXYZ cloud;
  cloud.width = 257;
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.points.resize(cloud.width);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    cloud[i] = pcl::PointXYZ(static_cast<float>(i), static_cast<float>(i + 1), static_cast<float>(i + 2));
    if (i % 17 == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    if (i % 29 == 0)
      cloud[i].z = std::numeric_limits<float>::infinity();
  }
  return cloud;
}

pcl::PointCloud<pcl::PointNormal>
makeNormalCloud()
{
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.width = 7;
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.points.resize(cloud.width * cloud.height);

  for (std::size_t i = 0; i < cloud.size(); ++i) {
    cloud[i].x = static_cast<float>(i);
    cloud[i].y = static_cast<float>(i + 1);
    cloud[i].z = static_cast<float>(i + 2);
    cloud[i].normal_x = 1.0f;
    cloud[i].normal_y = 0.0f;
    cloud[i].normal_z = 0.0f;
    cloud[i].curvature = 0.1f;
  }
  cloud[1].normal_x = std::numeric_limits<float>::quiet_NaN();
  cloud[3].normal_z = std::numeric_limits<float>::infinity();
  cloud[4].x = std::numeric_limits<float>::quiet_NaN();
  return cloud;
}

pcl::PointCloud<pcl::PointNormal>
makeLargeNormalCloud()
{
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.width = 257;
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.points.resize(cloud.width);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    cloud[i].x = static_cast<float>(i);
    cloud[i].y = static_cast<float>(i + 1);
    cloud[i].z = static_cast<float>(i + 2);
    cloud[i].normal_x = 1.0f;
    cloud[i].normal_y = 0.0f;
    cloud[i].normal_z = 0.0f;
    cloud[i].curvature = 0.1f;
    if (i % 19 == 0)
      cloud[i].normal_y = std::numeric_limits<float>::quiet_NaN();
    if (i % 31 == 0)
      cloud[i].normal_z = std::numeric_limits<float>::infinity();
    if (i % 43 == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

template <typename T>
void
expectIndicesEq(const std::vector<T>& actual, std::initializer_list<int> expected)
{
  ASSERT_EQ(actual.size(), expected.size());
  std::size_t i = 0;
  for (const int value : expected)
    EXPECT_EQ(actual[i++], value);
}

} // namespace

TEST(FilterIndicesRVV, RemoveNaNIndicesOnlyKeepsFiniteXYZInOrder)
{
  const auto cloud = makeXYZCloud();
  pcl::Indices indices;
  pcl::removeNaNFromPointCloud(cloud, indices);

  expectIndicesEq(indices, {0, 2, 4, 6, 7});
}

TEST(FilterIndicesRVV, DenseIndicesOnlyKeepsLegacyIdentityMapping)
{
  const auto cloud = makeXYZCloud(true);
  pcl::Indices indices;
  pcl::removeNaNFromPointCloud(cloud, indices);

  expectIndicesEq(indices, {0, 1, 2, 3, 4, 5, 6, 7});
}

TEST(FilterIndicesRVV, RemoveNaNCloudCompressesPointsAndIndices)
{
  const auto cloud = makeXYZCloud();
  PointCloudXYZ out;
  pcl::Indices indices;
  pcl::removeNaNFromPointCloud(cloud, out, indices);

  expectIndicesEq(indices, {0, 2, 4, 6, 7});
  ASSERT_EQ(out.size(), indices.size());
  EXPECT_EQ(out.width, indices.size());
  EXPECT_EQ(out.height, 1u);
  EXPECT_TRUE(out.is_dense);
  for (std::size_t i = 0; i < indices.size(); ++i) {
    EXPECT_FLOAT_EQ(out[i].x, cloud[indices[i]].x);
    EXPECT_FLOAT_EQ(out[i].y, cloud[indices[i]].y);
    EXPECT_FLOAT_EQ(out[i].z, cloud[indices[i]].z);
  }
}

TEST(FilterIndicesRVV, RemoveNaNCloudSupportsInPlace)
{
  auto cloud = makeXYZCloud();
  pcl::Indices indices;
  pcl::removeNaNFromPointCloud(cloud, cloud, indices);

  expectIndicesEq(indices, {0, 2, 4, 6, 7});
  ASSERT_EQ(cloud.size(), indices.size());
  EXPECT_FLOAT_EQ(cloud[0].x, 1.0f);
  EXPECT_FLOAT_EQ(cloud[1].x, 4.0f);
  EXPECT_FLOAT_EQ(cloud[2].x, -1.0f);
  EXPECT_FLOAT_EQ(cloud[3].x, 0.0f);
  EXPECT_FLOAT_EQ(cloud[4].x, 9.0f);
}

TEST(FilterIndicesRVV, LargeCloudPathMatchesScalarFormula)
{
  const auto cloud = makeLargeXYZCloud();
  PointCloudXYZ out;
  pcl::Indices indices;
  pcl::removeNaNFromPointCloud(cloud, out, indices);

  pcl::Indices expected;
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    if (std::isfinite(cloud[i].x) && std::isfinite(cloud[i].y) && std::isfinite(cloud[i].z))
      expected.push_back(static_cast<int>(i));
  }
  EXPECT_EQ(indices, expected);
  ASSERT_EQ(out.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_FLOAT_EQ(out[i].x, cloud[expected[i]].x);
}

TEST(FilterIndicesRVV, RemoveNaNNormalsKeepsNormalFiniteAndDenseFlag)
{
  const auto cloud = makeNormalCloud();
  pcl::PointCloud<pcl::PointNormal> out;
  pcl::Indices indices;
  pcl::removeNaNNormalsFromPointCloud(cloud, out, indices);

  expectIndicesEq(indices, {0, 2, 4, 5, 6});
  ASSERT_EQ(out.size(), indices.size());
  EXPECT_FALSE(out.is_dense);
  EXPECT_TRUE(std::isnan(out[2].x));
  EXPECT_FLOAT_EQ(out[0].normal_x, 1.0f);
}

TEST(FilterIndicesRVV, LargeNormalPathMatchesScalarFormula)
{
  const auto cloud = makeLargeNormalCloud();
  pcl::PointCloud<pcl::PointNormal> out;
  pcl::Indices indices;
  pcl::removeNaNNormalsFromPointCloud(cloud, out, indices);

  pcl::Indices expected;
  bool expected_dense = true;
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    if (!std::isfinite(cloud[i].normal_x) ||
        !std::isfinite(cloud[i].normal_y) ||
        !std::isfinite(cloud[i].normal_z))
      continue;
    expected.push_back(static_cast<int>(i));
    if (!pcl::isFinite(cloud[i]))
      expected_dense = false;
  }
  EXPECT_EQ(indices, expected);
  ASSERT_EQ(out.size(), expected.size());
  EXPECT_EQ(out.is_dense, expected_dense);
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_FLOAT_EQ(out[i].normal_z, cloud[expected[i]].normal_z);
}

int
main(int argc, char** argv)
{
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
