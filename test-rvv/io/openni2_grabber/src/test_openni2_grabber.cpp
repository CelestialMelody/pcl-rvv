/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）先保护 OpenNI2Grabber 的
 * frame-to-cloud（帧到点云）逐像素语义。测试使用 synthetic frame
 *（合成帧）数组，不依赖真实 OpenNI2 设备；它们验证测试专用
 * production-shaped diagnostic（生产形态诊断）helper 是否和源码中的
 * depth projection（深度反投影）、invalid mask（无效掩码）和 RGB overlay
 *（颜色覆盖）语义一致。
 *
 * 证据边界：
 * 这里还没有修改 `io/src/openni2_grabber.cpp`，也不证明 production
 * dispatch（生产分流）已经接入 RVV。它只为后续候选、bench（性能测试）
 * 和板卡证据建立可失败的 correctness gate（正确性验收条件）。
 */

#include "openni2_grabber.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#include <pcl/point_types.h>

namespace grabber = pcl::io::rvv_openni2_grabber_support;

namespace {

void
expectNanXYZ(const pcl::PointXYZ& point)
{
  EXPECT_TRUE(std::isnan(point.x));
  EXPECT_TRUE(std::isnan(point.y));
  EXPECT_TRUE(std::isnan(point.z));
}

void
expectNearXYZ(const pcl::PointXYZ& point,
              const float x,
              const float y,
              const float z)
{
  EXPECT_NEAR(point.x, x, 1e-6f);
  EXPECT_NEAR(point.y, y, 1e-6f);
  EXPECT_NEAR(point.z, z, 1e-6f);
}

std::uint32_t
floatBits(const float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

template <typename PointT>
void
expectSameXYZBits(const std::vector<PointT>& reference, const std::vector<PointT>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(floatBits(reference[i].x), floatBits(candidate[i].x)) << "x index=" << i;
    EXPECT_EQ(floatBits(reference[i].y), floatBits(candidate[i].y)) << "y index=" << i;
    EXPECT_EQ(floatBits(reference[i].z), floatBits(candidate[i].z)) << "z index=" << i;
  }
}

template <typename PointT>
void
expectSameRGBABits(const std::vector<PointT>& reference, const std::vector<PointT>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i)
    EXPECT_EQ(reference[i].rgba, candidate[i].rgba) << "rgba index=" << i;
}

template <typename PointT>
void
initializeTransparentCloud(std::vector<PointT>& cloud)
{
  for (auto& point : cloud) {
    point.x = point.y = point.z = std::numeric_limits<float>::quiet_NaN();
    point.r = point.g = point.b = 0;
    point.a = 255;
  }
}

template <typename PointT>
void
fillXYZFromDepthScalarForRgbTemplate(const std::uint16_t* depth,
                                     const unsigned depth_width,
                                     const unsigned height,
                                     const unsigned cloud_width,
                                     const grabber::CameraModel& camera,
                                     PointT* cloud)
{
  grabber::fillXYZRGBAFromDepthScalar(
      depth, depth_width, height, cloud_width, camera, cloud);
}

template <typename PointT>
void
fillXYZFromDepthCandidateForRgbTemplate(const std::uint16_t* depth,
                                        const unsigned depth_width,
                                        const unsigned height,
                                        const unsigned cloud_width,
                                        const grabber::CameraModel& camera,
                                        PointT* cloud)
{
  if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB>) {
    grabber::fillXYZRGBFromDepthCandidate(
        depth, depth_width, height, cloud_width, camera, cloud);
  }
  else {
    grabber::fillXYZRGBAFromDepthCandidate(
        depth, depth_width, height, cloud_width, camera, cloud);
  }
}

template <typename PointT>
void
expectRgbTemplatePointTypeMatchesScalar(const char* point_type_name)
{
  const unsigned depth_width = 17;
  const unsigned height = 5;
  const unsigned cloud_width = 34;
  std::vector<std::uint16_t> depth(static_cast<std::size_t>(depth_width) * height);
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(cloud_width) * height * 3);

  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < depth_width; ++x) {
      const auto index = static_cast<std::size_t>(y) * depth_width + x;
      depth[index] = static_cast<std::uint16_t>(900 + ((x * 47 + y * 19) % 3000));
      if ((x + y) % 11 == 0)
        depth[index] = 0;
      if ((x * 3 + y) % 13 == 0)
        depth[index] = 2047;
      if ((x + y * 7) % 17 == 0)
        depth[index] = 65535;
    }
    for (unsigned x = 0; x < cloud_width; ++x) {
      const auto index = (static_cast<std::size_t>(y) * cloud_width + x) * 3;
      rgb[index] = static_cast<std::uint8_t>((x * 5 + y * 3) & 0xff);
      rgb[index + 1] = static_cast<std::uint8_t>((x * 11 + y) & 0xff);
      rgb[index + 2] = static_cast<std::uint8_t>((x + y * 23) & 0xff);
    }
  }

  const grabber::CameraModel camera{
      525.0f, 525.0f, 8.0f, 2.0f, 2047u, 65535u};
  std::vector<PointT> reference(static_cast<std::size_t>(cloud_width) * height);
  std::vector<PointT> candidate(reference.size());
  initializeTransparentCloud(reference);
  initializeTransparentCloud(candidate);

  fillXYZFromDepthScalarForRgbTemplate(
      depth.data(), depth_width, height, cloud_width, camera, reference.data());
  fillXYZFromDepthCandidateForRgbTemplate(
      depth.data(), depth_width, height, cloud_width, camera, candidate.data());
  grabber::fillRGBOverlayScalar(rgb.data(), cloud_width, height, cloud_width, reference.data());
  grabber::fillRGBOverlayCandidate(rgb.data(), cloud_width, height, cloud_width, candidate.data());

  SCOPED_TRACE(point_type_name);
  expectSameXYZBits(reference, candidate);
  expectSameRGBABits(reference, candidate);
}

} // namespace

TEST(OpenNI2GrabberDiagnostic, DepthXYZMatchesProductionFormulaAndInvalidMask)
{
  const unsigned width = 4;
  const unsigned height = 2;
  const std::vector<std::uint16_t> depth = {
      1000, 0, 2000, 2047,
      3000, 65535, 4000, 5000};
  std::vector<pcl::PointXYZ> cloud(static_cast<std::size_t>(width) * height);

  const grabber::CameraModel camera{
      500.0f, 250.0f, 1.5f, 0.5f, 2047u, 65535u};

  grabber::fillXYZCloudCandidate(depth.data(), width, height, camera, cloud.data());

  expectNearXYZ(cloud[0], -0.003f, -0.002f, 1.0f);
  expectNanXYZ(cloud[1]);
  expectNearXYZ(cloud[2], 0.002f, -0.004f, 2.0f);
  expectNanXYZ(cloud[3]);
  expectNearXYZ(cloud[4], -0.009f, 0.006f, 3.0f);
  expectNanXYZ(cloud[5]);
  expectNearXYZ(cloud[6], 0.004f, 0.008f, 4.0f);
  expectNearXYZ(cloud[7], 0.015f, 0.010f, 5.0f);
}

TEST(OpenNI2GrabberDiagnostic, RGBOverlayUsesProductionAlphaAndPackedOrder)
{
  const unsigned width = 3;
  const unsigned height = 1;
  const std::vector<std::uint8_t> rgb = {
      10, 20, 30,
      40, 50, 60,
      70, 80, 90};
  std::vector<pcl::PointXYZRGBA> cloud(static_cast<std::size_t>(width) * height);

  for (auto& point : cloud) {
    point.x = point.y = point.z = std::numeric_limits<float>::quiet_NaN();
    point.r = point.g = point.b = 0;
    point.a = 255;
  }

  grabber::fillRGBOverlayCandidate(rgb.data(), width, height, width, cloud.data());

  EXPECT_EQ(cloud[0].r, 10);
  EXPECT_EQ(cloud[0].g, 20);
  EXPECT_EQ(cloud[0].b, 30);
  EXPECT_EQ(cloud[0].a, 255);
  EXPECT_EQ(cloud[1].r, 40);
  EXPECT_EQ(cloud[1].g, 50);
  EXPECT_EQ(cloud[1].b, 60);
  EXPECT_EQ(cloud[1].a, 255);
  EXPECT_EQ(cloud[2].r, 70);
  EXPECT_EQ(cloud[2].g, 80);
  EXPECT_EQ(cloud[2].b, 90);
  EXPECT_EQ(cloud[2].a, 255);
}

TEST(OpenNI2GrabberDiagnostic, MismatchedDepthWidthKeepsUnmappedRGBSlotsTransparent)
{
  const unsigned depth_width = 2;
  const unsigned height = 1;
  const unsigned cloud_width = 4;
  const std::vector<std::uint16_t> depth = {1000, 2000};
  std::vector<pcl::PointXYZRGBA> cloud(static_cast<std::size_t>(cloud_width) * height);

  for (auto& point : cloud) {
    point.x = point.y = point.z = std::numeric_limits<float>::quiet_NaN();
    point.r = point.g = point.b = 0;
    point.a = 255;
  }

  const grabber::CameraModel camera{
      500.0f, 250.0f, 0.5f, 0.0f, 2047u, 65535u};

  grabber::fillXYZRGBAFromDepthCandidate(
      depth.data(), depth_width, height, cloud_width, camera, cloud.data());

  EXPECT_NEAR(cloud[0].x, -0.001f, 1e-6f);
  EXPECT_NEAR(cloud[0].y, 0.0f, 1e-6f);
  EXPECT_NEAR(cloud[0].z, 1.0f, 1e-6f);
  EXPECT_TRUE(std::isnan(cloud[1].x));
  EXPECT_EQ(cloud[1].r, 0);
  EXPECT_EQ(cloud[1].a, 255);
  EXPECT_NEAR(cloud[2].x, 0.002f, 1e-6f);
  EXPECT_NEAR(cloud[2].y, 0.0f, 1e-6f);
  EXPECT_NEAR(cloud[2].z, 2.0f, 1e-6f);
  EXPECT_TRUE(std::isnan(cloud[3].x));
  EXPECT_EQ(cloud[3].r, 0);
  EXPECT_EQ(cloud[3].a, 255);
}

TEST(OpenNI2GrabberDiagnostic, IRPointCloudClearsColorStorageAndCopiesIntensity)
{
  const unsigned width = 3;
  const unsigned height = 1;
  const std::vector<std::uint16_t> depth = {1000, 0, 3000};
  const std::vector<std::uint16_t> ir = {111, 222, 333};
  std::vector<pcl::PointXYZI> cloud(static_cast<std::size_t>(width) * height);

  const grabber::CameraModel camera{
      500.0f, 250.0f, 1.0f, 0.0f, 2047u, 65535u};

  grabber::fillXYZICloudCandidate(depth.data(), ir.data(), width, height, camera, cloud.data());

  EXPECT_NEAR(cloud[0].x, -0.002f, 1e-6f);
  EXPECT_NEAR(cloud[0].z, 1.0f, 1e-6f);
  EXPECT_FLOAT_EQ(cloud[0].intensity, 111.0f);
  expectNanXYZ(pcl::PointXYZ{cloud[1].x, cloud[1].y, cloud[1].z});
  EXPECT_FLOAT_EQ(cloud[1].intensity, 222.0f);
  EXPECT_NEAR(cloud[2].x, 0.006f, 1e-6f);
  EXPECT_NEAR(cloud[2].z, 3.0f, 1e-6f);
  EXPECT_FLOAT_EQ(cloud[2].intensity, 333.0f);

  // `data_c[0]` 与 `intensity` 是同一个 union（共用存储）字段；
  // production 先清零 `data_c` 再写 intensity，因此最终只能要求
  // padding lane 仍为 0，不能要求 `data_c[0]` 保持 0。
  for (const auto& point : cloud) {
    EXPECT_EQ(point.data_c[1], 0);
    EXPECT_EQ(point.data_c[2], 0);
    EXPECT_EQ(point.data_c[3], 0);
  }
}

TEST(OpenNI2GrabberDiagnostic, CandidateMatchesScalarBitwiseOnLargeFrame)
{
  const unsigned width = 37;
  const unsigned height = 11;
  std::vector<std::uint16_t> depth(static_cast<std::size_t>(width) * height);
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      depth[index] = static_cast<std::uint16_t>(700 + ((x * 19 + y * 31) % 4000));
      if ((x + y * 5) % 23 == 0)
        depth[index] = 0;
      if ((x * 7 + y) % 29 == 0)
        depth[index] = 2047;
      if ((x + y * 11) % 31 == 0)
        depth[index] = 65535;
      rgb[index * 3] = static_cast<std::uint8_t>((x * 13 + y) & 0xff);
      rgb[index * 3 + 1] = static_cast<std::uint8_t>((x + y * 17) & 0xff);
      rgb[index * 3 + 2] = static_cast<std::uint8_t>((x * 3 + y * 5) & 0xff);
    }
  }

  const grabber::CameraModel camera{
      525.0f, 525.0f, 18.0f, 5.0f, 2047u, 65535u};
  std::vector<pcl::PointXYZ> xyz_ref(depth.size());
  std::vector<pcl::PointXYZ> xyz_candidate(depth.size());
  std::vector<pcl::PointXYZRGBA> rgba_ref(depth.size());
  std::vector<pcl::PointXYZRGBA> rgba_candidate(depth.size());

  grabber::fillXYZCloudScalar(depth.data(), width, height, camera, xyz_ref.data());
  grabber::fillXYZCloudCandidate(depth.data(), width, height, camera, xyz_candidate.data());
  expectSameXYZBits(xyz_ref, xyz_candidate);

  grabber::fillXYZRGBAFromDepthScalar(
      depth.data(), width, height, width, camera, rgba_ref.data());
  grabber::fillXYZRGBAFromDepthCandidate(
      depth.data(), width, height, width, camera, rgba_candidate.data());
  grabber::fillRGBOverlayScalar(rgb.data(), width, height, width, rgba_ref.data());
  grabber::fillRGBOverlayCandidate(rgb.data(), width, height, width, rgba_candidate.data());
  expectSameXYZBits(rgba_ref, rgba_candidate);
  for (std::size_t i = 0; i < rgba_ref.size(); ++i)
    EXPECT_EQ(rgba_ref[i].rgba, rgba_candidate[i].rgba) << "rgba index=" << i;
}

TEST(OpenNI2GrabberDiagnostic, RGBTemplatePointTypesMatchScalarBitwise)
{
  expectRgbTemplatePointTypeMatchesScalar<pcl::PointXYZRGB>("PointXYZRGB");
  expectRgbTemplatePointTypeMatchesScalar<pcl::PointXYZRGBA>("PointXYZRGBA");
}
