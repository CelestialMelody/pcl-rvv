#include "extract_indices_diag.hpp"

#include <pcl/filters/extract_indices.h>

#include <gtest/gtest.h>

#include <limits>

namespace {

pcl::PointCloud<pcl::PointXYZ>
makeCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 1021) - 510) * 0.01f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 997) - 498) * 0.02f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 991) - 495) * 0.03f;
  }
  return cloud;
}

pcl::Indices
makeSelected(std::size_t n, int stride, int offset = 0)
{
  pcl::Indices selected;
  for (std::size_t i = static_cast<std::size_t>(offset); i < n; i += static_cast<std::size_t>(stride))
    selected.push_back(static_cast<int>(i));
  return selected;
}

} // namespace

TEST(ExtractIndicesDiag, BitmapComplementMatchesSetDifference)
{
  const pcl::Indices selected = {9, 1, 3, 3, 7};
  const auto expected =
      pcl_rvv_filters_extract_indices::complementSetDifferenceStd(12, selected);
  const auto actual = pcl_rvv_filters_extract_indices::complementBitmapStd(12, selected);
  EXPECT_EQ(actual, expected);
}

TEST(ExtractIndicesDiag, RVVBitmapComplementMatchesScalar)
{
  const std::size_t n = 4096;
  const auto selected = makeSelected(n, 3, 1);
  const auto expected =
      pcl_rvv_filters_extract_indices::complementSetDifferenceStd(n, selected);

#if defined(__RVV10__)
  pcl::Indices actual;
  ASSERT_TRUE(pcl_rvv_filters_extract_indices::complementBitmapRVV(n, selected, actual));
  EXPECT_EQ(actual, expected);
#else
  EXPECT_FALSE(expected.empty());
#endif
}

TEST(ExtractIndicesDiag, RVVKeepOrganizedPositiveMatchesScalar)
{
  const auto cloud = makeCloud(4096);
  const auto selected = makeSelected(cloud.size(), 4, 1);
  const auto expected = pcl_rvv_filters_extract_indices::keepOrganizedPointXYZStd(
      cloud, selected, false, std::numeric_limits<float>::quiet_NaN());

#if defined(__RVV10__)
  pcl::PointCloud<pcl::PointXYZ> actual;
  ASSERT_TRUE(pcl_rvv_filters_extract_indices::keepOrganizedPointXYZRVV(
      cloud, selected, false, std::numeric_limits<float>::quiet_NaN(), actual));
  EXPECT_EQ(pcl_rvv_filters_extract_indices::checksumCloudXYZ(actual),
            pcl_rvv_filters_extract_indices::checksumCloudXYZ(expected));
  EXPECT_FALSE(actual.is_dense);
#else
  EXPECT_FALSE(expected.is_dense);
#endif
}

TEST(ExtractIndicesDiag, RVVKeepOrganizedNegativeMatchesScalar)
{
  const auto cloud = makeCloud(4096);
  const auto selected = makeSelected(cloud.size(), 5, 2);
  const auto expected =
      pcl_rvv_filters_extract_indices::keepOrganizedPointXYZStd(cloud, selected, true, 42.0f);

#if defined(__RVV10__)
  pcl::PointCloud<pcl::PointXYZ> actual;
  ASSERT_TRUE(
      pcl_rvv_filters_extract_indices::keepOrganizedPointXYZRVV(cloud, selected, true, 42.0f, actual));
  EXPECT_EQ(pcl_rvv_filters_extract_indices::checksumCloudXYZ(actual),
            pcl_rvv_filters_extract_indices::checksumCloudXYZ(expected));
  EXPECT_TRUE(actual.is_dense);
#else
  EXPECT_TRUE(expected.is_dense);
#endif
}

TEST(ExtractIndicesDiag, RVVMainThenSmallFallbackDoesNotChangeScalar)
{
  const auto cloud = makeCloud(1024);
  const auto selected = makeSelected(cloud.size(), 3, 1);
  const auto fallback_expected =
      pcl_rvv_filters_extract_indices::complementSetDifferenceStd(16, makeSelected(16, 2));

#if defined(__RVV10__)
  pcl::Indices actual;
  ASSERT_TRUE(pcl_rvv_filters_extract_indices::complementBitmapRVV(cloud.size(), selected, actual));
  pcl::Indices small;
  EXPECT_FALSE(pcl_rvv_filters_extract_indices::complementBitmapRVV(16, makeSelected(16, 2), small));
  const auto fallback_actual =
      pcl_rvv_filters_extract_indices::complementSetDifferenceStd(16, makeSelected(16, 2));
  EXPECT_EQ(fallback_actual, fallback_expected);
#else
  EXPECT_FALSE(fallback_expected.empty());
#endif
}

TEST(ExtractIndicesDiag, ProductionExtractIndicesStillRuns)
{
  auto cloud = makeCloud(128).makeShared();
  auto selected = pcl::make_shared<pcl::Indices>(makeSelected(cloud->size(), 4, 1));

  pcl::ExtractIndices<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud);
  filter.setIndices(selected);

  pcl::Indices indices;
  filter.filter(indices);
  EXPECT_EQ(indices, *selected);

  filter.setNegative(true);
  filter.filter(indices);
  const auto expected =
      pcl_rvv_filters_extract_indices::complementSetDifferenceStd(cloud->size(), *selected);
  EXPECT_EQ(indices, expected);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
