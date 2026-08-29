/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 BRISK 2D scale-space downsample
 * （尺度空间下采样）路径在非 x86/RISC-V 构建中不再落到空实现。测试使用真实
 * `pcl::keypoints::brisk::Layer` 构造派生层，并把结果与 topic-local
 * reference（参考链路）逐字节对拍。
 *
 * 证据边界：
 * 本文件主要覆盖 `halfsample` / `twothirdsample` 这两个 downsample helper。
 * 它不证明 AGAST score（角点评分）或完整 keypoint refinement（关键点细化）
 * 已经加速；完整公开入口收益必须由后续 production direct（真实生产路径）
 * 和 board bench（板卡性能测试）证明。
 */

#include "brisk_2d.h"

#include <pcl/keypoints/brisk_2d.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace brisk = pcl::keypoints::rvv_test::brisk_2d;

namespace
{
void
expectSameBytes(const std::vector<std::uint8_t>& reference,
                const std::vector<unsigned char>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i)
    ASSERT_EQ(reference[i], candidate[i]) << "index=" << i;
}
} // namespace

TEST(Brisk2DDownsample, HalfSampleLayerMatchesScalarReferenceForEvenAndTailWidths)
{
  for (const auto shape : {std::pair<int, int>{32, 24}, {34, 26}, {641, 481}})
  {
    const int width = shape.first;
    const int height = shape.second;
    const auto image = brisk::makeSyntheticImage(width, height);

    std::vector<std::uint8_t> reference;
    brisk::halfsampleReference(image, width, height, reference);
    std::vector<std::uint8_t> candidate;
    brisk::halfsampleCandidate(image, width, height, candidate);

    pcl::keypoints::brisk::Layer source(image, width, height);
    pcl::keypoints::brisk::Layer derived(
        source, pcl::keypoints::brisk::Layer::CommonParams::HALFSAMPLE);

    EXPECT_EQ(derived.getImageWidth(), width / 2);
    EXPECT_EQ(derived.getImageHeight(), height / 2);
    expectSameBytes(reference, derived.getImage());
    expectSameBytes(reference, candidate);
  }
}

TEST(Brisk2DDownsample, TwoThirdSampleLayerMatchesScalarReferenceForBlockAndTailWidths)
{
  for (const auto shape : {std::pair<int, int>{30, 24}, {33, 27}, {640, 480}})
  {
    const int width = shape.first;
    const int height = shape.second;
    const auto image = brisk::makeSyntheticImage(width, height);

    std::vector<std::uint8_t> reference;
    brisk::twothirdsampleReference(image, width, height, reference);
    std::vector<std::uint8_t> candidate;
    brisk::twothirdsampleCandidate(image, width, height, candidate);

    pcl::keypoints::brisk::Layer source(image, width, height);
    pcl::keypoints::brisk::Layer derived(
        source, pcl::keypoints::brisk::Layer::CommonParams::TWOTHIRDSAMPLE);

    EXPECT_EQ(derived.getImageWidth(), 2 * width / 3);
    EXPECT_EQ(derived.getImageHeight(), 2 * height / 3);
    expectSameBytes(reference, derived.getImage());
    expectSameBytes(reference, candidate);
  }
}

TEST(Brisk2DDownsample, ScaleSpaceConstructPyramidProducesNonZeroDerivedImages)
{
  const int width = 80;
  const int height = 60;
  const auto image = brisk::makeSyntheticImage(width, height);

  pcl::keypoints::brisk::ScaleSpace scale_space(2);
  scale_space.constructPyramid(image, width, height);

  pcl::keypoints::brisk::Layer half_source(image, width, height);
  pcl::keypoints::brisk::Layer half(
      half_source, pcl::keypoints::brisk::Layer::CommonParams::HALFSAMPLE);
  EXPECT_NE(brisk::sampledChecksum(half.getImage()), 0u);
}

TEST(Brisk2DDownsample, PublicComputeRunsOnSyntheticOrganizedCloud)
{
  const auto cloud = brisk::makeSyntheticOrganizedCloud(160, 120);

  pcl::BriskKeypoint2D<pcl::PointXYZRGBA> detector;
  detector.setThreshold(60);
  detector.setOctaves(4);
  detector.setRemoveInvalid3DKeypoints(false);
  detector.setInputCloud(cloud);

  pcl::PointCloud<pcl::PointWithScale> keypoints;
  detector.compute(keypoints);

  EXPECT_EQ(keypoints.width, keypoints.size());
  EXPECT_EQ(keypoints.height, 1u);
  EXPECT_FALSE(keypoints.is_dense);
  ASSERT_GT(keypoints.size(), 0u);
  EXPECT_NE(brisk::keypointChecksum(keypoints), 0u);
  for (const auto& keypoint : keypoints)
  {
    EXPECT_TRUE(std::isfinite(keypoint.x));
    EXPECT_TRUE(std::isfinite(keypoint.y));
    EXPECT_TRUE(std::isfinite(keypoint.scale));
    EXPECT_GE(keypoint.x, 0.0f);
    EXPECT_GE(keypoint.y, 0.0f);
    EXPECT_LT(keypoint.x, static_cast<float>(cloud->width));
    EXPECT_LT(keypoint.y, static_cast<float>(cloud->height));
  }
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
