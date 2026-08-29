/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 color_gradient_dot_modality topic 的
 * dominant quantized map（主方向量化图）是否与标量参考链路一致，并故意让
 * RVV build 命中尚未接通的 candidate path，避免把纯标量 fallback 误写成 RVV 证据。
 */

#include "cgdm.h"

#include <pcl/recognition/color_gradient_dot_modality.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cgdm = pcl::recognition::rvv_test::color_gradient_dot_modality;

namespace
{
std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGB>>
makeProductionCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      auto& point = (*cloud)(x, y);
      point.x = static_cast<float>(x);
      point.y = static_cast<float>(y);
      point.z = 0.1f * static_cast<float>((x + y) & 7);
      point.r = static_cast<std::uint8_t>((5 * x + 13 * y + 17) & 0xff);
      point.g = static_cast<std::uint8_t>((19 * x + 7 * y + 23) & 0xff);
      point.b = static_cast<std::uint8_t>((11 * x + 29 * y + 31) & 0xff);
    }
  }
  return cloud;
}

std::vector<std::uint8_t>
copyMap(const pcl::QuantizedMap& map)
{
  const auto size = map.getWidth() * map.getHeight();
  return std::vector<std::uint8_t>(map.getData(), map.getData() + size);
}
} // namespace

TEST(ColorGradientDotModalityDiagnostic, ScalarReferenceProducesDominantBits)
{
  auto cloud = makeProductionCloud(19, 13);
  cgdm::DominantMapArtifacts artifacts;

  cgdm::computeDominantMapScalar(
      cloud->points.data(), cloud->width, cloud->height, 4, 20.0f, artifacts);

  EXPECT_EQ(artifacts.dominant_map.getWidth(), cloud->width / 4);
  EXPECT_EQ(artifacts.dominant_map.getHeight(), cloud->height / 4);
  EXPECT_FALSE(copyMap(artifacts.dominant_map).empty());
}

TEST(ColorGradientDotModalityDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarReference)
{
  auto cloud = makeProductionCloud(37, 25);
  cgdm::DominantMapArtifacts reference;
  cgdm::DominantMapArtifacts candidate;

  cgdm::computeDominantMapScalar(
      cloud->points.data(), cloud->width, cloud->height, 4, 20.0f, reference);
  const auto path =
      cgdm::computeDominantMapCandidate(cloud->points.data(),
                                        cloud->width,
                                        cloud->height,
                                        4,
                                        20.0f,
                                        candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, cgdm::ExecutionPath::RvvGradientDominant);
#else
  EXPECT_EQ(path, cgdm::ExecutionPath::ScalarFallback);
#endif
  EXPECT_EQ(copyMap(reference.dominant_map), copyMap(candidate.dominant_map));
}

TEST(ColorGradientDotModalityProductionDirect, ProcessInputDataMatchesScalarReferenceMap)
{
  auto cloud = makeProductionCloud(37, 25);
  pcl::ColorGradientDOTModality<pcl::PointXYZRGB> modality(4);
  modality.setGradientMagnitudeThreshold(20.0f);
  modality.setInputCloud(cloud);
  modality.processInputData();

  cgdm::DominantMapArtifacts reference;
  cgdm::computeDominantMapScalar(
      cloud->points.data(), cloud->width, cloud->height, 4, 20.0f, reference);

  EXPECT_EQ(copyMap(reference.dominant_map), copyMap(modality.getDominantQuantizedMap()));
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
