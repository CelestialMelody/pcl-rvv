#include <pcl/filters/pyramid.h>
#include <pcl/filters/impl/pyramid.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace {

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePointXYZCloud(const int width, const int height, const bool dense)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = dense;
  cloud->points.resize(static_cast<std::size_t>(width * height));
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      auto& p = cloud->at(c, r);
      p.x = 0.25f * static_cast<float>(c) + 0.10f * static_cast<float>(r);
      p.y = -0.15f * static_cast<float>(c) + 0.20f * static_cast<float>(r);
      p.z = 0.05f * static_cast<float>((c * 7 + r * 11) % 23);
    }
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr
makePointXYZICloud(const int width, const int height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(static_cast<std::size_t>(width * height));
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      auto& p = cloud->at(c, r);
      p.x = 0.12f * static_cast<float>(c);
      p.y = 0.07f * static_cast<float>(r);
      p.z = 0.03f * static_cast<float>(c + r);
      p.intensity = static_cast<float>((c + r) % 9);
    }
  }
  return cloud;
}

template <typename PointT>
std::vector<typename pcl::PointCloud<PointT>::Ptr>
runPyramid(const typename pcl::PointCloud<PointT>::ConstPtr& input, const int levels, const bool large)
{
  pcl::filters::Pyramid<PointT> pyramid(levels);
  pyramid.setInputCloud(input);
  pyramid.setLargeSmoothingKernel(large);
  pyramid.setDistanceThreshold(std::numeric_limits<float>::infinity());
  std::vector<typename pcl::PointCloud<PointT>::Ptr> output;
  pyramid.compute(output);
  return output;
}

std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>
runPointXYZPyramidWithThreads(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& input,
                              const int levels,
                              const unsigned int threads)
{
  pcl::filters::Pyramid<pcl::PointXYZ> pyramid(levels);
  pyramid.setInputCloud(input);
  pyramid.setLargeSmoothingKernel(false);
  pyramid.setDistanceThreshold(std::numeric_limits<float>::infinity());
  pyramid.setNumberOfThreads(threads);
  std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> output;
  pyramid.compute(output);
  return output;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
referenceLevel(const pcl::PointCloud<pcl::PointXYZ>& previous, const bool large)
{
  const float small[3] = {0.25f, 0.5f, 0.25f};
  const float big[5] = {1.0f / 16.0f, 0.25f, 0.375f, 0.25f, 1.0f / 16.0f};
  const float* k = large ? big : small;
  const int kernel_size = large ? 5 : 3;
  const int center = kernel_size / 2;
  auto next = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(previous.width / 2, previous.height / 2);

  for (int r = 0; r < static_cast<int>(next->height); ++r) {
    for (int c = 0; c < static_cast<int>(next->width); ++c) {
      auto& out = next->at(c, r);
      out.x = out.y = out.z = 0.0f;
      for (int m = 0; m < kernel_size; ++m) {
        const int mm = kernel_size - 1 - m;
        int rr = 2 * r + (m - center);
        if (rr < 0)
          rr = 0;
        if (rr >= static_cast<int>(previous.height))
          rr = static_cast<int>(previous.height) - 1;
        for (int n = 0; n < kernel_size; ++n) {
          const int nn = kernel_size - 1 - n;
          int cc = 2 * c + (n - center);
          if (cc < 0)
            cc = 0;
          if (cc >= static_cast<int>(previous.width))
            cc = static_cast<int>(previous.width) - 1;
          const float weight = k[mm] * k[nn];
          const auto& in = previous.at(cc, rr);
          out.x += in.x * weight;
          out.y += in.y * weight;
          out.z += in.z * weight;
        }
      }
    }
  }
  return next;
}

std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>
referencePyramid(const pcl::PointCloud<pcl::PointXYZ>& input, const int levels, const bool large)
{
  std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> output;
  output.resize(static_cast<std::size_t>(levels + 1));
  output[0] = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(input);
  for (int l = 1; l <= levels; ++l)
    output[static_cast<std::size_t>(l)] = referenceLevel(*output[static_cast<std::size_t>(l - 1)], large);
  return output;
}

template <typename PointT>
void
expectXYZNear(const pcl::PointCloud<PointT>& a, const pcl::PointCloud<PointT>& b)
{
  ASSERT_EQ(a.width, b.width);
  ASSERT_EQ(a.height, b.height);
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    EXPECT_NEAR(a[i].x, b[i].x, 1e-5f) << i;
    EXPECT_NEAR(a[i].y, b[i].y, 1e-5f) << i;
    EXPECT_NEAR(a[i].z, b[i].z, 1e-5f) << i;
  }
}

} // namespace

TEST(PyramidRVV, DensePointXYZSmallKernel)
{
  const auto cloud = makePointXYZCloud(32, 18, true);
  const auto output = runPyramid<pcl::PointXYZ>(cloud, 3, false);

  ASSERT_EQ(output.size(), 4u);
  EXPECT_EQ(output[1]->width, 16u);
  EXPECT_EQ(output[1]->height, 9u);
  EXPECT_EQ(output[2]->width, 8u);
  EXPECT_EQ(output[2]->height, 4u);
  EXPECT_TRUE(output[1]->is_dense);

  const auto repeat = runPyramid<pcl::PointXYZ>(cloud, 3, false);
  const auto reference = referencePyramid(*cloud, 3, false);
  expectXYZNear(*output[1], *repeat[1]);
  expectXYZNear(*output[2], *repeat[2]);
  expectXYZNear(*output[1], *reference[1]);
  expectXYZNear(*output[2], *reference[2]);
  expectXYZNear(*output[3], *reference[3]);
}

TEST(PyramidRVV, DensePointXYZLargeKernel)
{
  const auto cloud = makePointXYZCloud(40, 24, true);
  const auto output = runPyramid<pcl::PointXYZ>(cloud, 2, true);

  ASSERT_EQ(output.size(), 3u);
  EXPECT_EQ(output[1]->width, 20u);
  EXPECT_EQ(output[1]->height, 12u);
  EXPECT_EQ(output[2]->width, 10u);
  EXPECT_EQ(output[2]->height, 6u);
  EXPECT_TRUE(std::isfinite(output[1]->at(0, 0).x));
  EXPECT_TRUE(std::isfinite(output[1]->at(19, 11).z));
  const auto reference = referencePyramid(*cloud, 2, true);
  expectXYZNear(*output[1], *reference[1]);
  expectXYZNear(*output[2], *reference[2]);
}

TEST(PyramidRVV, NonDenseFallback)
{
  auto cloud = makePointXYZCloud(16, 10, false);
  cloud->at(3, 3).x = std::numeric_limits<float>::quiet_NaN();
  const auto output = runPyramid<pcl::PointXYZ>(cloud, 2, false);

  ASSERT_EQ(output.size(), 3u);
  EXPECT_EQ(output[1]->width, 8u);
  EXPECT_EQ(output[1]->height, 5u);
}

TEST(PyramidRVV, PointXYZIFallback)
{
  const auto cloud = makePointXYZICloud(24, 12);
  const auto output = runPyramid<pcl::PointXYZI>(cloud, 2, false);

  ASSERT_EQ(output.size(), 3u);
  EXPECT_EQ(output[1]->width, 12u);
  EXPECT_EQ(output[1]->height, 6u);
  EXPECT_TRUE(std::isfinite(output[1]->at(2, 2).x));
}

TEST(PyramidRVV, ExplicitThreadsFallback)
{
  const auto cloud = makePointXYZCloud(32, 18, true);
  const auto output = runPointXYZPyramidWithThreads(cloud, 2, 2);
  const auto reference = referencePyramid(*cloud, 2, false);
  ASSERT_EQ(output.size(), 3u);
  expectXYZNear(*output[1], *reference[1]);
  expectXYZNear(*output[2], *reference[2]);
}

TEST(PyramidRVV, InitFailureKeepsOutputEmpty)
{
  auto cloud = makePointXYZCloud(8, 8, true);
  cloud->height = 1;
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  pcl::filters::Pyramid<pcl::PointXYZ> pyramid(1);
  pyramid.setInputCloud(cloud);
  std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> output;
  pyramid.compute(output);
  EXPECT_TRUE(output.empty());
}
