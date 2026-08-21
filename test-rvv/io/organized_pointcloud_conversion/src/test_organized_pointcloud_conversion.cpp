/*
 * 本文件做什么：
 * 这里先验证 organized_pointcloud_conversion 首阶段 test-only RVV candidate
 * （测试专用 RVV 候选）是否保持 PointXYZ 点云到 disparity image（视差图）
 * 的公开转换语义。测试覆盖有限点、NaN / infinity invalid point（无效点）
 * 和非 VL 整除长度，后续 phase 再扩展到 RGB / mono、decode 和 production direct。
 *
 * 证据边界：
 * 这些 TEST 是 correctness（正确性）证据。它们不证明生产 dispatch（分流逻辑），
 * 也不证明真实性能；性能只由板卡 benchmark（性能测试）和 Evidence Doctor
 * （证据体检）解释。
 */

#include "organized_pointcloud_conversion.h"

#include <pcl/compression/organized_pointcloud_conversion.h>
#include <pcl/compression/impl/organized_pointcloud_compression_analysis.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/test/gtest.h>

#include <cstdint>
#include <limits>
#include <cmath>
#include <sstream>
#include <vector>

namespace opc = pcl::io::rvv_test::organized_pointcloud_conversion;

namespace {

constexpr float kFocalLength = 525.0f;
constexpr float kDisparityShift = 2.0f;
constexpr float kDisparityScale = 0.5f;

pcl::PointCloud<pcl::PointXYZ>
makeLiteralCloud()
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 4;
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.resize(4);

  cloud[0].x = 0.0f;
  cloud[0].y = 0.0f;
  cloud[0].z = 1.0f;

  cloud[1].x = 1.0f;
  cloud[1].y = -1.0f;
  cloud[1].z = 2.0f;

  cloud[2].x = std::numeric_limits<float>::quiet_NaN();
  cloud[2].y = 2.0f;
  cloud[2].z = 3.0f;

  cloud[3].x = 3.0f;
  cloud[3].y = 4.0f;
  cloud[3].z = std::numeric_limits<float>::infinity();

  return cloud;
}

}  // namespace

TEST(OrganizedPointCloudConversionRVV, PointXYZCloudToDisparityMatchesHandCheckedLiterals)
{
  const auto cloud = makeLiteralCloud();
  std::vector<std::uint16_t> disparity;

  opc::convertPointXYZCloudToDisparityDiagnostic(
      cloud, kFocalLength, kDisparityShift, kDisparityScale, disparity);

  const std::vector<std::uint16_t> expected = {
      1054u,  // 525 / (0.5 * 1.0) + 2.0 / 0.5
      529u,   // 525 / (0.5 * 2.0) + 2.0 / 0.5
      0u,     // x is NaN, so pcl::isFinite(point) is false
      0u,     // z is infinity, so pcl::isFinite(point) is false
  };

  EXPECT_EQ(disparity, expected);
}

TEST(OrganizedPointCloudConversionRVV, PointXYZDiagnosticMatchesPclScalarPathForMixedCloud)
{
  const auto cloud = opc::makePointXYZCloud(257, opc::InvalidPattern::mixed_invalid);

  std::vector<std::uint16_t> scalar_disparity;
  std::vector<std::uint8_t> unused_color;
  pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(
      cloud,
      kFocalLength,
      kDisparityShift,
      kDisparityScale,
      false,
      scalar_disparity,
      unused_color);

  std::vector<std::uint16_t> diagnostic_disparity;
  opc::convertPointXYZCloudToDisparityDiagnostic(cloud,
                                                 kFocalLength,
                                                 kDisparityShift,
                                                 kDisparityScale,
                                                 diagnostic_disparity);

  EXPECT_EQ(diagnostic_disparity, scalar_disparity);
  EXPECT_EQ(opc::checksumDisparity(diagnostic_disparity),
            opc::checksumDisparity(scalar_disparity));
}

TEST(OrganizedPointCloudConversionRVV, PointXYZDiagnosticHandlesNonMultipleOfVectorLength)
{
  const auto cloud = opc::makePointXYZCloud(1027, opc::InvalidPattern::finite_only);

  std::vector<std::uint16_t> scalar_disparity;
  std::vector<std::uint8_t> unused_color;
  pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(
      cloud,
      kFocalLength,
      kDisparityShift,
      kDisparityScale,
      false,
      scalar_disparity,
      unused_color);

  std::vector<std::uint16_t> diagnostic_disparity;
  opc::convertPointXYZCloudToDisparityDiagnostic(cloud,
                                                 kFocalLength,
                                                 kDisparityShift,
                                                 kDisparityScale,
                                                 diagnostic_disparity);

  EXPECT_EQ(diagnostic_disparity, scalar_disparity);
}

TEST(OrganizedPointCloudConversionRVV, AnalyzeOrganizedCloudMatchesScalarFiniteCloud)
{
  const auto cloud =
      opc::makeOrganizedPointXYZCloud(32, 24, opc::InvalidPattern::finite_only);

  float scalar_max_depth = 0.0f;
  float scalar_focal_length = 0.0f;
  opc::analyzeOrganizedCloudReference(cloud, scalar_max_depth, scalar_focal_length);

  float diagnostic_max_depth = 0.0f;
  float diagnostic_focal_length = 0.0f;
  opc::analyzeOrganizedCloudDiagnostic(
      cloud, diagnostic_max_depth, diagnostic_focal_length);

  EXPECT_EQ(opc::checksumAnalyzeResult(diagnostic_max_depth, diagnostic_focal_length),
            opc::checksumAnalyzeResult(scalar_max_depth, scalar_focal_length));
}

TEST(OrganizedPointCloudConversionRVV, AnalyzeOrganizedCloudMatchesScalarMixedInvalidCloud)
{
  const auto cloud =
      opc::makeOrganizedPointXYZCloud(32, 24, opc::InvalidPattern::mixed_invalid);

  float scalar_max_depth = 0.0f;
  float scalar_focal_length = 0.0f;
  opc::analyzeOrganizedCloudReference(cloud, scalar_max_depth, scalar_focal_length);

  float diagnostic_max_depth = 0.0f;
  float diagnostic_focal_length = 0.0f;
  opc::analyzeOrganizedCloudDiagnostic(
      cloud, diagnostic_max_depth, diagnostic_focal_length);

  EXPECT_EQ(opc::checksumAnalyzeResult(diagnostic_max_depth, diagnostic_focal_length),
            opc::checksumAnalyzeResult(scalar_max_depth, scalar_focal_length));
}

TEST(OrganizedPointCloudConversionRVV, ProductionAnalyzeDetailMatchesStdFinitePointXYZCloud)
{
  const auto cloud =
      opc::makeOrganizedPointXYZCloud(64, 48, opc::InvalidPattern::finite_only);

  float std_max_depth = 0.0f;
  float std_focal_length = 0.0f;
  pcl::io::organized_compression_detail::analyzeOrganizedCloudStd(
      cloud, std_max_depth, std_focal_length);

  float dispatch_max_depth = 0.0f;
  float dispatch_focal_length = 0.0f;
  pcl::io::organized_compression_detail::analyzeOrganizedCloud(
      cloud, dispatch_max_depth, dispatch_focal_length);

  EXPECT_EQ(opc::checksumAnalyzeResult(dispatch_max_depth, dispatch_focal_length),
            opc::checksumAnalyzeResult(std_max_depth, std_focal_length));
}

TEST(OrganizedPointCloudConversionRVV, ProductionAnalyzeDetailMatchesStdMixedInvalidPointXYZICloud)
{
  auto cloud = opc::makePointXYZICloud(64u * 48u, opc::InvalidPattern::mixed_invalid);
  cloud.width = 64;
  cloud.height = 48;

  float std_max_depth = 0.0f;
  float std_focal_length = 0.0f;
  pcl::io::organized_compression_detail::analyzeOrganizedCloudStd(
      cloud, std_max_depth, std_focal_length);

  float dispatch_max_depth = 0.0f;
  float dispatch_focal_length = 0.0f;
  pcl::io::organized_compression_detail::analyzeOrganizedCloud(
      cloud, dispatch_max_depth, dispatch_focal_length);

  EXPECT_EQ(opc::checksumAnalyzeResult(dispatch_max_depth, dispatch_focal_length),
            opc::checksumAnalyzeResult(std_max_depth, std_focal_length));
}

TEST(OrganizedPointCloudConversionRVV, PointXYZRGBCloudToDisparityRgbMatchesHandCheckedLiterals)
{
  pcl::PointCloud<pcl::PointXYZRGB> cloud;
  cloud.width = 3;
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.resize(3);

  cloud[0].x = 0.0f;
  cloud[0].y = 0.0f;
  cloud[0].z = 1.0f;
  cloud[0].r = 10;
  cloud[0].g = 20;
  cloud[0].b = 30;

  cloud[1].x = 1.0f;
  cloud[1].y = -1.0f;
  cloud[1].z = 2.0f;
  cloud[1].r = 40;
  cloud[1].g = 50;
  cloud[1].b = 60;

  cloud[2].x = std::numeric_limits<float>::quiet_NaN();
  cloud[2].y = 2.0f;
  cloud[2].z = 3.0f;
  cloud[2].r = 70;
  cloud[2].g = 80;
  cloud[2].b = 90;

  std::vector<std::uint16_t> disparity;
  std::vector<std::uint8_t> rgb;
  opc::convertPointXYZRGBCloudToDisparityColorDiagnostic(
      cloud, kFocalLength, kDisparityShift, kDisparityScale, false, disparity, rgb);

  const std::vector<std::uint16_t> expected_disparity = {1054u, 529u, 0u};
  const std::vector<std::uint8_t> expected_rgb = {10u, 20u, 30u, 40u, 50u, 60u, 0u, 0u, 0u};
  EXPECT_EQ(disparity, expected_disparity);
  EXPECT_EQ(rgb, expected_rgb);
}

TEST(OrganizedPointCloudConversionRVV, PointXYZRGBCloudToDisparityMonoMatchesPclScalarPath)
{
  const auto cloud = opc::makePointXYZRGBCloud(259, opc::InvalidPattern::mixed_invalid);

  std::vector<std::uint16_t> scalar_disparity;
  std::vector<std::uint8_t> scalar_mono;
  pcl::io::OrganizedConversion<pcl::PointXYZRGB>::convert(cloud,
                                                          kFocalLength,
                                                          kDisparityShift,
                                                          kDisparityScale,
                                                          true,
                                                          scalar_disparity,
                                                          scalar_mono);

  std::vector<std::uint16_t> diagnostic_disparity;
  std::vector<std::uint8_t> diagnostic_mono;
  opc::convertPointXYZRGBCloudToDisparityColorDiagnostic(cloud,
                                                         kFocalLength,
                                                         kDisparityShift,
                                                         kDisparityScale,
                                                         true,
                                                         diagnostic_disparity,
                                                         diagnostic_mono);

  EXPECT_EQ(diagnostic_disparity, scalar_disparity);
  EXPECT_EQ(diagnostic_mono, scalar_mono);
  EXPECT_EQ(opc::checksumDisparity(diagnostic_disparity),
            opc::checksumDisparity(scalar_disparity));
}

TEST(OrganizedPointCloudConversionRVV, PointXYZRGBFusedColorDiagnosticMatchesPclScalarPath)
{
  const auto cloud = opc::makePointXYZRGBCloud(263, opc::InvalidPattern::mixed_invalid);

  std::vector<std::uint16_t> scalar_disparity;
  std::vector<std::uint8_t> scalar_rgb;
  pcl::io::OrganizedConversion<pcl::PointXYZRGB>::convert(cloud,
                                                          kFocalLength,
                                                          kDisparityShift,
                                                          kDisparityScale,
                                                          false,
                                                          scalar_disparity,
                                                          scalar_rgb);

  std::vector<std::uint16_t> diagnostic_disparity;
  std::vector<std::uint8_t> diagnostic_rgb;
  opc::convertPointXYZRGBCloudToDisparityColorFusedDiagnostic(cloud,
                                                              kFocalLength,
                                                              kDisparityShift,
                                                              kDisparityScale,
                                                              false,
                                                              diagnostic_disparity,
                                                              diagnostic_rgb);

  EXPECT_EQ(diagnostic_disparity, scalar_disparity);
  EXPECT_EQ(diagnostic_rgb, scalar_rgb);
  EXPECT_EQ(opc::checksumBytes(diagnostic_rgb), opc::checksumBytes(scalar_rgb));
}

TEST(OrganizedPointCloudConversionRVV, DisparityToPointXYZDiagnosticMatchesPclScalarPath)
{
  std::vector<std::uint16_t> disparity = {1054u, 0u, 529u, 0x7ffu, 240u, 1200u, 0u, 777u};
  std::vector<std::uint8_t> unused_color;

  pcl::PointCloud<pcl::PointXYZ> scalar_cloud;
  pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(disparity,
                                                       unused_color,
                                                       false,
                                                       4,
                                                       2,
                                                       kFocalLength,
                                                       kDisparityShift,
                                                       kDisparityScale,
                                                       scalar_cloud);

  pcl::PointCloud<pcl::PointXYZ> diagnostic_cloud;
  opc::convertDisparityToPointXYZCloudDiagnostic(disparity,
                                                 4,
                                                 2,
                                                 kFocalLength,
                                                 kDisparityShift,
                                                 kDisparityScale,
                                                 diagnostic_cloud);

  ASSERT_EQ(diagnostic_cloud.size(), scalar_cloud.size());
  EXPECT_EQ(diagnostic_cloud.width, scalar_cloud.width);
  EXPECT_EQ(diagnostic_cloud.height, scalar_cloud.height);
  EXPECT_EQ(diagnostic_cloud.is_dense, scalar_cloud.is_dense);
  for (std::size_t i = 0; i < scalar_cloud.size(); ++i) {
    if (std::isnan(scalar_cloud[i].z)) {
      EXPECT_TRUE(std::isnan(diagnostic_cloud[i].x));
      EXPECT_TRUE(std::isnan(diagnostic_cloud[i].y));
      EXPECT_TRUE(std::isnan(diagnostic_cloud[i].z));
      continue;
    }
    EXPECT_FLOAT_EQ(diagnostic_cloud[i].x, scalar_cloud[i].x);
    EXPECT_FLOAT_EQ(diagnostic_cloud[i].y, scalar_cloud[i].y);
    EXPECT_FLOAT_EQ(diagnostic_cloud[i].z, scalar_cloud[i].z);
  }
}

TEST(OrganizedPointCloudConversionRVV, PointXYZIProductionDirectMatchesScalarHelper)
{
  const auto cloud = opc::makePointXYZICloud(271, opc::InvalidPattern::mixed_invalid);

  std::vector<std::uint16_t> scalar_disparity;
  pcl::io::convertCloudToDisparityStd(
      cloud, kFocalLength, kDisparityShift, kDisparityScale, scalar_disparity);

  std::vector<std::uint16_t> production_disparity;
  std::vector<std::uint8_t> unused_color;
  pcl::io::OrganizedConversion<pcl::PointXYZI>::convert(cloud,
                                                        kFocalLength,
                                                        kDisparityShift,
                                                        kDisparityScale,
                                                        false,
                                                        production_disparity,
                                                        unused_color);

  EXPECT_EQ(production_disparity, scalar_disparity);
}

TEST(OrganizedPointCloudConversionRVV, PointXYZRGBAProductionDirectMatchesScalarHelper)
{
  const auto cloud = opc::makePointXYZRGBACloud(277, opc::InvalidPattern::mixed_invalid);

  std::vector<std::uint16_t> scalar_disparity;
  std::vector<std::uint8_t> scalar_rgb;
  pcl::io::convertCloudToDisparityColorStd(cloud,
                                           kFocalLength,
                                           kDisparityShift,
                                           kDisparityScale,
                                           false,
                                           scalar_disparity,
                                           scalar_rgb);

  std::vector<std::uint16_t> production_disparity;
  std::vector<std::uint8_t> production_rgb;
  pcl::io::OrganizedConversion<pcl::PointXYZRGBA>::convert(cloud,
                                                           kFocalLength,
                                                           kDisparityShift,
                                                           kDisparityScale,
                                                           false,
                                                           production_disparity,
                                                           production_rgb);

  EXPECT_EQ(production_disparity, scalar_disparity);
  EXPECT_EQ(production_rgb, scalar_rgb);
}

TEST(OrganizedPointCloudConversionRVV, PointXYZEncodePointCloudSmokeProducesStableBytes)
{
  auto cloud = opc::makeOrganizedPointXYZCloud(64, 48, opc::InvalidPattern::finite_only);
  auto cloud_ptr = cloud.makeShared();

  std::ostringstream compressed;
  opc::encodePointCloudShaped(*cloud_ptr,
                              compressed,
                              false,
                              false,
                              1 /* Z_BEST_SPEED */);

  const std::string bytes = compressed.str();
  EXPECT_GT(bytes.size(), 0u);
  EXPECT_NE(opc::checksumStringBytes(bytes), 0u);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
