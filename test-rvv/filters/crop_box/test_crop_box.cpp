#include <pcl/filters/crop_box.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

using PointCloudXYZ = pcl::PointCloud<pcl::PointXYZ>;

PointCloudXYZ
makeSmallCloud()
{
  PointCloudXYZ cloud;
  cloud.width = 9;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(cloud.width);

  cloud[0] = pcl::PointXYZ(-1.0f, 0.0f, 0.0f);
  cloud[1] = pcl::PointXYZ(-0.5f, 0.0f, 0.0f);
  cloud[2] = pcl::PointXYZ(0.0f, -0.5f, 0.0f);
  cloud[3] = pcl::PointXYZ(0.0f, 0.0f, -0.5f);
  cloud[4] = pcl::PointXYZ(0.5f, 0.5f, 0.5f);
  cloud[5] = pcl::PointXYZ(1.0f, 0.0f, 0.0f);
  cloud[6] = pcl::PointXYZ(1.25f, 0.0f, 0.0f);
  cloud[7] = pcl::PointXYZ(0.0f, -1.25f, 0.0f);
  cloud[8] = pcl::PointXYZ(0.0f, 0.0f, 1.25f);
  return cloud;
}

PointCloudXYZ
makeLargeCloud(std::size_t n, bool dense)
{
  PointCloudXYZ cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = dense;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const float x = static_cast<float>(static_cast<int>(i % 211) - 105) / 70.0f;
    const float y = static_cast<float>(static_cast<int>((i * 7) % 223) - 111) / 80.0f;
    const float z = static_cast<float>(static_cast<int>((i * 13) % 227) - 113) / 90.0f;
    cloud[i] = pcl::PointXYZ(x, y, z);
    if (!dense && i % 97 == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    if (!dense && i % 131 == 0)
      cloud[i].z = std::numeric_limits<float>::infinity();
  }
  return cloud;
}

pcl::Indices
expectedCrop(const PointCloudXYZ& cloud,
             const pcl::Indices* subset,
             const Eigen::Vector4f& min_pt,
             const Eigen::Vector4f& max_pt,
             bool negative)
{
  pcl::Indices expected;
  const std::size_t n = subset ? subset->size() : cloud.size();
  expected.reserve(n);
  for (std::size_t pos = 0; pos < n; ++pos) {
    const int index = subset ? (*subset)[pos] : static_cast<int>(pos);
    const auto& p = cloud[index];
    if (!cloud.is_dense && (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)))
      continue;
    const bool inside = p.x >= min_pt[0] && p.y >= min_pt[1] && p.z >= min_pt[2] &&
                        p.x <= max_pt[0] && p.y <= max_pt[1] && p.z <= max_pt[2];
    if ((!negative && inside) || (negative && !inside))
      expected.push_back(index);
  }
  return expected;
}

pcl::Indices
expectedRemoved(const PointCloudXYZ& cloud,
                const pcl::Indices* subset,
                const Eigen::Vector4f& min_pt,
                const Eigen::Vector4f& max_pt,
                bool negative)
{
  pcl::Indices removed;
  const std::size_t n = subset ? subset->size() : cloud.size();
  removed.reserve(n);
  for (std::size_t pos = 0; pos < n; ++pos) {
    const int index = subset ? (*subset)[pos] : static_cast<int>(pos);
    const auto& p = cloud[index];
    if (!cloud.is_dense && (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)))
      continue;
    const bool inside = p.x >= min_pt[0] && p.y >= min_pt[1] && p.z >= min_pt[2] &&
                        p.x <= max_pt[0] && p.y <= max_pt[1] && p.z <= max_pt[2];
    if ((negative && inside) || (!negative && !inside))
      removed.push_back(index);
  }
  return removed;
}

} // namespace

TEST(CropBoxRVV, SmallCloudMatchesScalarFormula)
{
  const auto cloud = makeSmallCloud();
  const Eigen::Vector4f min_pt(-0.5f, -0.5f, -0.5f, 1.0f);
  const Eigen::Vector4f max_pt(0.5f, 0.5f, 0.5f, 1.0f);

  pcl::CropBox<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setMin(min_pt);
  filter.setMax(max_pt);

  pcl::Indices indices;
  filter.filter(indices);
  EXPECT_EQ(indices, expectedCrop(cloud, nullptr, min_pt, max_pt, false));
  EXPECT_EQ(*filter.getRemovedIndices(), expectedRemoved(cloud, nullptr, min_pt, max_pt, false));
}

TEST(CropBoxRVV, LargeDenseKeepsInsideRangeInOrder)
{
  const auto cloud = makeLargeCloud(257, true);
  const Eigen::Vector4f min_pt(-0.75f, -0.5f, -0.4f, 1.0f);
  const Eigen::Vector4f max_pt(0.8f, 0.65f, 0.7f, 1.0f);

  pcl::CropBox<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setMin(min_pt);
  filter.setMax(max_pt);

  pcl::Indices indices;
  filter.filter(indices);
  EXPECT_EQ(indices, expectedCrop(cloud, nullptr, min_pt, max_pt, false));
  EXPECT_EQ(*filter.getRemovedIndices(), expectedRemoved(cloud, nullptr, min_pt, max_pt, false));
}

TEST(CropBoxRVV, LargeDenseNegativeKeepsOutsideRange)
{
  const auto cloud = makeLargeCloud(257, true);
  const Eigen::Vector4f min_pt(-0.5f, -0.4f, -0.3f, 1.0f);
  const Eigen::Vector4f max_pt(0.5f, 0.4f, 0.3f, 1.0f);

  pcl::CropBox<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setMin(min_pt);
  filter.setMax(max_pt);
  filter.setNegative(true);

  pcl::Indices indices;
  filter.filter(indices);
  EXPECT_EQ(indices, expectedCrop(cloud, nullptr, min_pt, max_pt, true));
  EXPECT_EQ(*filter.getRemovedIndices(), expectedRemoved(cloud, nullptr, min_pt, max_pt, true));
}

TEST(CropBoxRVV, CloudOutputUsesFilteredIndices)
{
  const auto cloud = makeLargeCloud(257, true);
  const Eigen::Vector4f min_pt(-0.75f, -0.5f, -0.4f, 1.0f);
  const Eigen::Vector4f max_pt(0.8f, 0.65f, 0.7f, 1.0f);

  pcl::CropBox<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setMin(min_pt);
  filter.setMax(max_pt);

  PointCloudXYZ out;
  filter.filter(out);

  const auto expected = expectedCrop(cloud, nullptr, min_pt, max_pt, false);
  ASSERT_EQ(out.size(), expected.size());
  EXPECT_EQ(out.width, expected.size());
  EXPECT_EQ(out.height, 1u);
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_FLOAT_EQ(out[i].x, cloud[expected[i]].x);
}

TEST(CropBoxRVV, ExplicitIndicesFallbackPreservesSubsetValues)
{
  const auto cloud = makeLargeCloud(257, true);
  auto subset = pcl::make_shared<pcl::Indices>();
  *subset = {3, 5, 7, 97, 111, 131, 173, 199, 201};
  const Eigen::Vector4f min_pt(-0.75f, -0.5f, -0.4f, 1.0f);
  const Eigen::Vector4f max_pt(0.8f, 0.65f, 0.7f, 1.0f);

  pcl::CropBox<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setIndices(subset);
  filter.setMin(min_pt);
  filter.setMax(max_pt);

  pcl::Indices indices;
  filter.filter(indices);
  EXPECT_EQ(indices, expectedCrop(cloud, subset.get(), min_pt, max_pt, false));
  EXPECT_EQ(*filter.getRemovedIndices(), expectedRemoved(cloud, subset.get(), min_pt, max_pt, false));
}

TEST(CropBoxRVV, NonDenseFallbackSkipsInvalidPoints)
{
  const auto cloud = makeLargeCloud(257, false);
  const Eigen::Vector4f min_pt(-0.75f, -0.5f, -0.4f, 1.0f);
  const Eigen::Vector4f max_pt(0.8f, 0.65f, 0.7f, 1.0f);

  pcl::CropBox<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setMin(min_pt);
  filter.setMax(max_pt);

  pcl::Indices indices;
  filter.filter(indices);
  EXPECT_EQ(indices, expectedCrop(cloud, nullptr, min_pt, max_pt, false));
  EXPECT_EQ(*filter.getRemovedIndices(), expectedRemoved(cloud, nullptr, min_pt, max_pt, false));
}

TEST(CropBoxRVV, TranslationFallbackPreservesTransformedSemantics)
{
  const auto cloud = makeSmallCloud();
  pcl::CropBox<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setMin(Eigen::Vector4f(-0.1f, -0.1f, -0.1f, 1.0f));
  filter.setMax(Eigen::Vector4f(0.1f, 0.1f, 0.1f, 1.0f));
  filter.setTranslation(Eigen::Vector3f(0.5f, 0.5f, 0.5f));

  pcl::Indices indices;
  filter.filter(indices);

  EXPECT_EQ(indices, (pcl::Indices{4}));
  ASSERT_EQ(filter.getRemovedIndices()->size(), cloud.size() - 1);
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
