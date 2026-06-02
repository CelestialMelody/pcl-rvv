#include <pcl/filters/passthrough.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

using PointCloudXYZI = pcl::PointCloud<pcl::PointXYZI>;

PointCloudXYZI
makeSmallCloud()
{
  PointCloudXYZI cloud;
  cloud.width = 9;
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.points.resize(cloud.width);

  cloud[0].x = 0.0f; cloud[0].y = 1.0f; cloud[0].z = 2.0f; cloud[0].intensity = -1.0f;
  cloud[1].x = 1.0f; cloud[1].y = 1.0f; cloud[1].z = 2.0f; cloud[1].intensity = 0.0f;
  cloud[2].x = 2.0f; cloud[2].y = 1.0f; cloud[2].z = 2.0f; cloud[2].intensity = 0.5f;
  cloud[3].x = 3.0f; cloud[3].y = 1.0f; cloud[3].z = 2.0f; cloud[3].intensity = 1.0f;
  cloud[4].x = 4.0f; cloud[4].y = 1.0f; cloud[4].z = 2.0f; cloud[4].intensity = 1.5f;
  cloud[5].x = std::numeric_limits<float>::quiet_NaN(); cloud[5].y = 1.0f; cloud[5].z = 2.0f; cloud[5].intensity = 0.5f;
  cloud[6].x = 6.0f; cloud[6].y = 1.0f; cloud[6].z = std::numeric_limits<float>::infinity(); cloud[6].intensity = 0.5f;
  cloud[7].x = 7.0f; cloud[7].y = 1.0f; cloud[7].z = 2.0f; cloud[7].intensity = std::numeric_limits<float>::quiet_NaN();
  cloud[8].x = 8.0f; cloud[8].y = 1.0f; cloud[8].z = 2.0f; cloud[8].intensity = 2.0f;
  return cloud;
}

PointCloudXYZI
makeLargeCloud(std::size_t n)
{
  PointCloudXYZI cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(i % 4096);
    cloud[i].y = static_cast<float>((i + 3) % 4096);
    cloud[i].z = static_cast<float>((i + 7) % 4096);
    cloud[i].intensity = static_cast<float>((static_cast<int>(i % 200) - 100)) / 50.0f;
    if (i % 97 == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    if (i % 131 == 0)
      cloud[i].z = std::numeric_limits<float>::infinity();
    if (i % 173 == 0)
      cloud[i].intensity = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

std::vector<int>
expectedPassThrough(const PointCloudXYZI& cloud, float min_value, float max_value, bool negative)
{
  std::vector<int> expected;
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const auto& p = cloud[i];
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
      continue;
    if (!std::isfinite(p.intensity))
      continue;
    const bool in_range = p.intensity >= min_value && p.intensity <= max_value;
    if ((!negative && in_range) || (negative && !in_range))
      expected.push_back(static_cast<int>(i));
  }
  return expected;
}

} // namespace

TEST(PassThroughRVV, SmallCloudMatchesScalarFormula)
{
  const auto cloud = makeSmallCloud();
  pcl::Indices indices;
  pcl::PassThrough<pcl::PointXYZI> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(0.0f, 1.0f);
  filter.filter(indices);

  const auto expected = expectedPassThrough(cloud, 0.0f, 1.0f, false);
  EXPECT_EQ(indices, expected);
  ASSERT_EQ(filter.getRemovedIndices()->size(), cloud.size() - expected.size());
  EXPECT_EQ((*filter.getRemovedIndices())[0], 0);
  EXPECT_EQ((*filter.getRemovedIndices())[3], 6);
}

TEST(PassThroughRVV, LargePositiveKeepsInclusiveRangeInOrder)
{
  const auto cloud = makeLargeCloud(257);
  pcl::Indices indices;
  pcl::PassThrough<pcl::PointXYZI> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.5f, 0.75f);
  filter.filter(indices);

  const auto expected = expectedPassThrough(cloud, -0.5f, 0.75f, false);
  EXPECT_EQ(indices, expected);
  ASSERT_EQ(filter.getRemovedIndices()->size(), cloud.size() - expected.size());
  EXPECT_EQ((*filter.getRemovedIndices())[0], 0);
}

TEST(PassThroughRVV, LargeNegativeKeepsOutsideRange)
{
  const auto cloud = makeLargeCloud(257);
  pcl::Indices indices;
  pcl::PassThrough<pcl::PointXYZI> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.25f, 0.25f);
  filter.setNegative(true);
  filter.filter(indices);

  const auto expected = expectedPassThrough(cloud, -0.25f, 0.25f, true);
  EXPECT_EQ(indices, expected);
  ASSERT_EQ(filter.getRemovedIndices()->size(), cloud.size() - expected.size());
}

TEST(PassThroughRVV, ExplicitIndicesFallbackPreservesSubsetValues)
{
  const auto cloud = makeLargeCloud(257);
  auto subset = pcl::make_shared<pcl::Indices>();
  *subset = {3, 5, 7, 97, 111, 131, 173, 199, 201};

  pcl::Indices indices;
  pcl::PassThrough<pcl::PointXYZI> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setIndices(subset);
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.5f, 0.5f);
  filter.filter(indices);

  pcl::Indices expected;
  pcl::Indices removed;
  for (const int idx : *subset) {
    const auto& p = cloud[idx];
    bool keep = std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
                std::isfinite(p.intensity) && p.intensity >= -0.5f && p.intensity <= 0.5f;
    (keep ? expected : removed).push_back(idx);
  }
  EXPECT_EQ(indices, expected);
  EXPECT_EQ(*filter.getRemovedIndices(), removed);
}

TEST(PassThroughRVV, EmptyFieldOnlyFiltersFiniteXYZ)
{
  const auto cloud = makeSmallCloud();
  pcl::Indices indices;
  pcl::PassThrough<pcl::PointXYZI> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.filter(indices);

  EXPECT_EQ(indices, (pcl::Indices{0, 1, 2, 3, 4, 7, 8}));
  EXPECT_EQ(*filter.getRemovedIndices(), (pcl::Indices{5, 6}));
}

TEST(PassThroughRVV, CloudOutputUsesFilteredIndices)
{
  const auto cloud = makeLargeCloud(257);
  PointCloudXYZI out;
  pcl::PassThrough<pcl::PointXYZI> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.5f, 0.75f);
  filter.filter(out);

  const auto expected = expectedPassThrough(cloud, -0.5f, 0.75f, false);
  ASSERT_EQ(out.size(), expected.size());
  EXPECT_EQ(out.width, expected.size());
  EXPECT_EQ(out.height, 1u);
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_FLOAT_EQ(out[i].intensity, cloud[expected[i]].intensity);
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
