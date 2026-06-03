#include <pcl/filters/fast_bilateral.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeOrganizedCloud(std::uint32_t width, std::uint32_t height, bool inject_non_finite)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = !inject_non_finite;
  cloud->points.resize(static_cast<std::size_t>(width) * height);

  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::size_t idx = static_cast<std::size_t>(y) * width + x;
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      const float wave = 0.03f * std::sin(fx * 0.11f) + 0.02f * std::cos(fy * 0.07f);
      (*cloud)[idx].x = fx * 0.01f;
      (*cloud)[idx].y = fy * 0.01f;
      (*cloud)[idx].z = 0.8f + 0.002f * fx + 0.003f * fy + wave;
    }
  }

  if (inject_non_finite && cloud->size() > 80) {
    (*cloud)[5].z = std::numeric_limits<float>::quiet_NaN();
    (*cloud)[17].z = std::numeric_limits<float>::infinity();
    (*cloud)[79].z = -std::numeric_limits<float>::infinity();
  }

  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
runFilter(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& cloud, float sigma_s, float sigma_r)
{
  pcl::FastBilateralFilter<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud);
  filter.setSigmaS(sigma_s);
  filter.setSigmaR(sigma_r);

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);
  return output;
}

void
expectFiniteZ(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  for (const auto& point : cloud)
    EXPECT_TRUE(std::isfinite(point.z));
}

} // namespace

TEST(FastBilateralRVV, LargeOrganizedCloudProducesFiniteSmoothedDepth)
{
  const auto cloud = makeOrganizedCloud(96, 72, false);
  const auto output = runFilter(cloud, 5.0f, 0.05f);

  ASSERT_EQ(output.size(), cloud->size());
  EXPECT_EQ(output.width, cloud->width);
  EXPECT_EQ(output.height, cloud->height);
  expectFiniteZ(output);

  EXPECT_NEAR(output(20, 15).x, (*cloud)(20, 15).x, 1e-6f);
  EXPECT_NEAR(output(20, 15).y, (*cloud)(20, 15).y, 1e-6f);
  EXPECT_GT(output(20, 15).z, 0.0f);
  EXPECT_NEAR(output(40, 32).z, output(41, 32).z, 0.08f);
}

TEST(FastBilateralRVV, NonFiniteDepthIsReplacedBeforeFiltering)
{
  const auto cloud = makeOrganizedCloud(80, 64, true);
  const auto output = runFilter(cloud, 4.0f, 0.04f);

  ASSERT_EQ(output.size(), cloud->size());
  expectFiniteZ(output);
  EXPECT_TRUE(std::isfinite(output[5].z));
  EXPECT_TRUE(std::isfinite(output[17].z));
  EXPECT_TRUE(std::isfinite(output[79].z));
}

TEST(FastBilateralRVV, SmallCloudFallbackKeepsOrganizedOutput)
{
  const auto cloud = makeOrganizedCloud(7, 5, true);
  const auto output = runFilter(cloud, 3.0f, 0.05f);

  ASSERT_EQ(output.size(), cloud->size());
  EXPECT_EQ(output.width, cloud->width);
  EXPECT_EQ(output.height, cloud->height);
  expectFiniteZ(output);
}

TEST(FastBilateralRVV, NonOrganizedInputReturnsEmptyOutput)
{
  auto cloud = makeOrganizedCloud(32, 2, false);
  cloud->height = 1;
  cloud->width = static_cast<std::uint32_t>(cloud->size());

  pcl::FastBilateralFilter<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud);
  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  EXPECT_TRUE(output.empty());
}
