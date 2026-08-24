/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）保护 ONIGrabber depth-only
 * frame-to-cloud（帧到点云）语义。测试使用 synthetic depth frame
 *（合成深度帧），不依赖真实 ONI 文件或 OpenNI runtime；它们验证测试
 * 专用 production-shaped diagnostic（生产形态诊断）helper 是否和源码中的
 * depth projection（深度反投影）及 invalid mask（无效掩码）一致。
 *
 * 证据边界：
 * 这些 diagnostic tests（诊断测试）不直接命中 production dispatch
 *（生产分流）。它们为候选、bench（性能测试）和板卡证据建立可失败的
 * correctness gate（正确性验收条件）；production-detail（生产内部边界）
 * 证据由 `oni_grabber_production_detail_test.cpp` 覆盖。
 */

#include "oni_grabber.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace grabber = pcl::io::rvv_oni_grabber_support;

namespace {

std::uint32_t
floatBits(const float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void
expectNanXYZ(const pcl::PointXYZ& point)
{
  EXPECT_TRUE(std::isnan(point.x));
  EXPECT_TRUE(std::isnan(point.y));
  EXPECT_TRUE(std::isnan(point.z));
}

void
expectNearXYZ(const pcl::PointXYZ& point, const float x, const float y, const float z)
{
  EXPECT_NEAR(point.x, x, 1e-6f);
  EXPECT_NEAR(point.y, y, 1e-6f);
  EXPECT_NEAR(point.z, z, 1e-6f);
}

void
expectSameXYZBits(const std::vector<pcl::PointXYZ>& reference,
                  const std::vector<pcl::PointXYZ>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(floatBits(reference[i].x), floatBits(candidate[i].x)) << "x index=" << i;
    EXPECT_EQ(floatBits(reference[i].y), floatBits(candidate[i].y)) << "y index=" << i;
    EXPECT_EQ(floatBits(reference[i].z), floatBits(candidate[i].z)) << "z index=" << i;
  }
}

} // namespace

TEST(ONIGrabberDiagnostic, DepthXYZMatchesProductionFormulaAndInvalidMask)
{
  const unsigned width = 4;
  const unsigned height = 2;
  const std::vector<std::uint16_t> depth = {
      1000, 0, 2000, 2047,
      3000, 65535, 4000, 5000};
  std::vector<pcl::PointXYZ> cloud(static_cast<std::size_t>(width) * height);

  const grabber::CameraModel camera{0.002f, 2, 1, 2047u, 65535u};

  grabber::fillXYZCloudCandidate(depth.data(), width, height, camera, cloud.data());

  expectNearXYZ(cloud[0], -0.004f, -0.002f, 1.0f);
  expectNanXYZ(cloud[1]);
  expectNearXYZ(cloud[2], 0.0f, -0.004f, 2.0f);
  expectNanXYZ(cloud[3]);
  expectNearXYZ(cloud[4], -0.012f, 0.0f, 3.0f);
  expectNanXYZ(cloud[5]);
  expectNearXYZ(cloud[6], 0.0f, 0.0f, 4.0f);
  expectNearXYZ(cloud[7], 0.010f, 0.0f, 5.0f);
}

TEST(ONIGrabberDiagnostic, CandidateMatchesScalarReferenceBitwiseForOddDepthValues)
{
  const unsigned width = 18;
  const unsigned height = 6;
  std::vector<std::uint16_t> depth(static_cast<std::size_t>(width) * height);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      depth[index] = static_cast<std::uint16_t>(700 + ((x * 37 + y * 53) % 5000));
      if ((x + y * 3) % 11 == 0)
        depth[index] = 0;
      if ((x * 5 + y) % 17 == 0)
        depth[index] = 2047;
      if ((x + y * 7) % 19 == 0)
        depth[index] = 65535;
    }
  }

  const grabber::CameraModel camera{1.0f / 525.0f, static_cast<int>(width >> 1),
                                    static_cast<int>(height >> 1), 2047u, 65535u};
  std::vector<pcl::PointXYZ> reference(depth.size());
  std::vector<pcl::PointXYZ> candidate(depth.size());

  grabber::fillXYZCloudScalar(depth.data(), width, height, camera, reference.data());
  grabber::fillXYZCloudCandidate(depth.data(), width, height, camera, candidate.data());

  expectSameXYZBits(reference, candidate);
}
