/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 integral image normal（积分图法线）
 * map-preparation diagnostic（地图预处理诊断）候选是否复刻 production
 * `computeFeature()` 开头的 depth-change map 和 distance-map initialization 语义。
 *
 * 证据边界：
 * 本文件只覆盖 pre-production diagnostic（生产前诊断）helper。它不修改 production
 * 头文件，也不证明真实 `IntegralImageNormalEstimation::computeFeature()` 已经命中 RVV。
 */

#include "integral_image_normal.h"

#include <pcl/features/integral_image_normal.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace iin = pcl::features::rvv_test::integral_image_normal;

namespace
{
struct XYZPadPoint
{
  float x;
  float y;
  float z;
  float pad;
};

std::vector<float>
makePlaneZ(std::size_t width, std::size_t height)
{
  std::vector<float> z(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
      z[y * width + x] = 1.0f + 0.001f * static_cast<float>(x) + 0.002f * static_cast<float>(y);
  }
  return z;
}

std::vector<unsigned char>
buildDepthChangeMapReference(const std::vector<float>& z,
                             std::size_t width,
                             std::size_t height,
                             float max_depth_change_factor)
{
  std::vector<unsigned char> map(z.size(), 255);
  for (std::size_t ri = 0; ri < height - 1; ++ri)
  {
    for (std::size_t ci = 0; ci < width - 1; ++ci)
    {
      const std::size_t index = ri * width + ci;
      const float depth = z[index];
      const float depth_r = z[index + 1];
      const float depth_d = z[index + width];
      const float threshold = max_depth_change_factor * (std::abs(depth) + 1.0f) * 2.0f;

      if (std::fabs(depth - depth_r) > threshold || !std::isfinite(depth) || !std::isfinite(depth_r))
      {
        map[index] = 0;
        map[index + 1] = 0;
      }
      if (std::fabs(depth - depth_d) > threshold || !std::isfinite(depth) || !std::isfinite(depth_d))
      {
        map[index] = 0;
        map[index + width] = 0;
      }
    }
  }
  return map;
}

std::vector<float>
initializeDistanceMapReference(const std::vector<unsigned char>& depth_change_map,
                               float far_distance)
{
  std::vector<float> distance(depth_change_map.size());
  for (std::size_t i = 0; i < depth_change_map.size(); ++i)
    distance[i] = depth_change_map[i] == 0 ? 0.0f : far_distance;
  return distance;
}

std::vector<float>
applyDistanceTransformReference(const std::vector<unsigned char>& depth_change_map,
                                std::size_t width,
                                std::size_t height)
{
  std::vector<float> distance =
      initializeDistanceMapReference(depth_change_map, static_cast<float>(width + height));

  float* previous_row = distance.data();
  float* current_row = previous_row + width;
  for (std::size_t ri = 1; ri < height; ++ri)
  {
    for (std::size_t ci = 1; ci < width; ++ci)
    {
      const float up_left = previous_row[ci - 1] + 1.4f;
      const float up = previous_row[ci] + 1.0f;
      const float up_right = previous_row[ci + 1] + 1.4f;
      const float left = current_row[ci - 1] + 1.0f;
      const float center = current_row[ci];
      const float min_value = std::min(std::min(up_left, up), std::min(left, up_right));
      if (min_value < center)
        current_row[ci] = min_value;
    }
    previous_row = current_row;
    current_row += width;
  }

  float* next_row = distance.data() + width * (height - 1);
  current_row = next_row - width;
  for (int ri = static_cast<int>(height) - 2; ri >= 0; --ri)
  {
    for (int ci = static_cast<int>(width) - 2; ci >= 0; --ci)
    {
      const float lower_left = next_row[ci - 1] + 1.4f;
      const float lower = next_row[ci] + 1.0f;
      const float lower_right = next_row[ci + 1] + 1.4f;
      const float right = current_row[ci + 1] + 1.0f;
      const float center = current_row[ci];
      const float min_value = std::min(std::min(lower_left, lower), std::min(right, lower_right));
      if (min_value < center)
        current_row[ci] = min_value;
    }
    next_row = current_row;
    current_row -= width;
  }

  return distance;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeOrganizedPointXYZCloud(std::size_t width, std::size_t height)
{
  auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>);
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = false;
  cloud->points.resize(width * height);
  const std::vector<float> z = makePlaneZ(width, height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      pcl::PointXYZ& point = cloud->points[y * width + x];
      point.x = static_cast<float>(x) * 0.01f;
      point.y = static_cast<float>(y) * 0.02f;
      point.z = z[y * width + x];
    }
  }

  cloud->points[3 * width + 5].z = 4.0f;
  cloud->points[4 * width + 5].z = 0.5f;
  cloud->points[6 * width + 8].z = std::numeric_limits<float>::quiet_NaN();
  cloud->points[2 * width + 12].z = std::numeric_limits<float>::infinity();
  return cloud;
}

std::vector<float>
extractZ(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::vector<float> z(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i)
    z[i] = cloud.points[i].z;
  return z;
}

std::vector<XYZPadPoint>
makeXYZPadCloud(std::size_t width, std::size_t height)
{
  std::vector<XYZPadPoint> points(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      points[y * width + x] = {
          0.25f * fx + 0.5f * fy,
          -0.125f * fx + 0.75f * fy,
          1.0f + 0.03125f * fx * fx + 0.0625f * fy,
          -99.0f};
    }
  }
  return points;
}

void
buildAverage3DGradientDiffReference(const std::vector<XYZPadPoint>& points,
                                    std::size_t width,
                                    std::size_t height,
                                    std::vector<float>& diff_x,
                                    std::vector<float>& diff_y)
{
  diff_x.assign(width * height * 4, 0.0f);
  diff_y.assign(width * height * 4, 0.0f);
  if (width < 3 || height < 3)
    return;

  for (std::size_t row = 1; row + 1 < height; ++row)
  {
    for (std::size_t col = 1; col + 1 < width; ++col)
    {
      const XYZPadPoint& left = points[row * width + (col - 1)];
      const XYZPadPoint& right = points[row * width + (col + 1)];
      const XYZPadPoint& up = points[(row - 1) * width + col];
      const XYZPadPoint& down = points[(row + 1) * width + col];
      const std::size_t out = (row * width + col) * 4;

      diff_x[out + 0] = right.x - left.x;
      diff_x[out + 1] = right.y - left.y;
      diff_x[out + 2] = right.z - left.z;

      diff_y[out + 0] = down.x - up.x;
      diff_y[out + 1] = down.y - up.y;
      diff_y[out + 2] = down.z - up.z;
    }
  }
}
} // namespace

TEST(IntegralImageNormalMapPrepRVV, MatchesScalarDepthChangeMapForEdgesAndNonFiniteDepth)
{
  constexpr std::size_t width = 19;
  constexpr std::size_t height = 11;
  constexpr float max_depth_change_factor = 20.0f * 0.001f;

  std::vector<float> z = makePlaneZ(width, height);
  z[3 * width + 5] = 4.0f;
  z[4 * width + 5] = 0.5f;
  z[6 * width + 8] = std::numeric_limits<float>::quiet_NaN();
  z[2 * width + 12] = std::numeric_limits<float>::infinity();

  const std::vector<unsigned char> expected =
      buildDepthChangeMapReference(z, width, height, max_depth_change_factor);
  std::vector<unsigned char> actual(z.size(), 17);

  iin::buildDepthChangeMapRVV(z.data(), width, height, max_depth_change_factor, actual.data());

  EXPECT_EQ(actual, expected);
}

TEST(IntegralImageNormalMapPrepRVV, HandlesTailWidthAndInitializesDistanceMap)
{
  constexpr std::size_t width = 37;
  constexpr std::size_t height = 9;
  constexpr float max_depth_change_factor = 20.0f * 0.001f;

  std::vector<float> z = makePlaneZ(width, height);
  z[1 * width + 35] = -2.0f;
  z[7 * width + 10] = std::numeric_limits<float>::quiet_NaN();

  const std::vector<unsigned char> expected_map =
      buildDepthChangeMapReference(z, width, height, max_depth_change_factor);
  const float far_distance = static_cast<float>(width + height);
  const std::vector<float> expected_distance =
      initializeDistanceMapReference(expected_map, far_distance);

  std::vector<unsigned char> actual_map(z.size(), 255);
  std::vector<float> actual_distance(z.size(), -1.0f);

  iin::buildDepthChangeMapRVV(z.data(), width, height, max_depth_change_factor, actual_map.data());
  iin::initializeDistanceMapRVV(actual_map.data(), actual_map.size(), far_distance, actual_distance.data());

  EXPECT_EQ(actual_map, expected_map);
  EXPECT_EQ(actual_distance, expected_distance);
}

TEST(IntegralImageNormalAverage3DGradientRVV, MatchesScalarDiffBuffersForInteriorAndBorders)
{
  constexpr std::size_t width = 23;
  constexpr std::size_t height = 13;
  const std::vector<XYZPadPoint> points = makeXYZPadCloud(width, height);

  std::vector<float> expected_x;
  std::vector<float> expected_y;
  buildAverage3DGradientDiffReference(points, width, height, expected_x, expected_y);

  std::vector<float> actual_x(width * height * 4, 42.0f);
  std::vector<float> actual_y(width * height * 4, -42.0f);
  iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, actual_x.data(), actual_y.data());

  EXPECT_EQ(actual_x, expected_x);
  EXPECT_EQ(actual_y, expected_y);
}

TEST(IntegralImageNormalAverage3DGradientRVV, LeavesTooSmallImagesZeroInitialized)
{
  constexpr std::size_t width = 2;
  constexpr std::size_t height = 7;
  const std::vector<XYZPadPoint> points = makeXYZPadCloud(width, height);
  const std::vector<float> expected(width * height * 4, 0.0f);

  std::vector<float> actual_x(width * height * 4, 5.0f);
  std::vector<float> actual_y(width * height * 4, 6.0f);
  iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, actual_x.data(), actual_y.data());

  EXPECT_EQ(actual_x, expected);
  EXPECT_EQ(actual_y, expected);
}

TEST(IntegralImageNormalProductionRVV, PublicComputeDistanceMapMatchesReference)
{
  constexpr std::size_t width = 37;
  constexpr std::size_t height = 19;
  constexpr float max_depth_change_factor = 20.0f * 0.001f;
  auto cloud = makeOrganizedPointXYZCloud(width, height);

  const std::vector<unsigned char> expected_map =
      buildDepthChangeMapReference(extractZ(*cloud), width, height, max_depth_change_factor);
  const std::vector<float> expected_distance =
      applyDistanceTransformReference(expected_map, width, height);

  pcl::IntegralImageNormalEstimation<pcl::PointXYZ, pcl::Normal> estimator;
  estimator.setNormalEstimationMethod(estimator.AVERAGE_DEPTH_CHANGE);
  estimator.setBorderPolicy(estimator.BORDER_POLICY_IGNORE);
  estimator.setNormalSmoothingSize(10.0f);
  estimator.setInputCloud(cloud);

  pcl::PointCloud<pcl::Normal> output;
  estimator.compute(output);

  const float* actual_distance = estimator.getDistanceMap();
  ASSERT_NE(actual_distance, nullptr);
  ASSERT_EQ(output.size(), cloud->size());
  for (std::size_t i = 0; i < expected_distance.size(); ++i)
    EXPECT_FLOAT_EQ(actual_distance[i], expected_distance[i]) << "distance map mismatch at " << i;
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
