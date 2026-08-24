/*
 * 本文件做什么：
 * 这是 point_cloud_image_extractors 的首阶段 correctness（正确性）测试。
 * Std build 使用标量参考链路；RVV build 在 __RVV10__ 下会走测试专用
 * RVV candidate（RVV 候选）。这些测试只证明 diagnostic helper（诊断
 * helper）与生产标量语义一致，不证明 production dispatch（生产分流）
 * 已经存在。
 */

#define PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK
#include "pcie.h"

#include <pcl/test/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace pcie = pcl::io::rvv_test::point_cloud_image_extractors;

namespace {

enum class ProductionPathHook {
  None = 0,
  RgbScalar = 1,
  RgbRvv = 2,
  ScalingScalar = 3,
  ScalingRvv = 4,
  LabelScalar = 5,
  LabelRvv = 6,
};

#if defined(__RVV10__) && !defined(PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK_ACTIVE)
extern "C" __attribute__((weak)) void
pcl_rvv_point_cloud_image_extractors_reset_test_hook()
{}

extern "C" __attribute__((weak)) int
pcl_rvv_point_cloud_image_extractors_last_test_hook()
{
  return static_cast<int>(ProductionPathHook::None);
}
#endif

void
resetProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK_ACTIVE)
  pcl::io::detail::pcl_rvv_point_cloud_image_extractors_reset_test_hook();
#else
  pcl_rvv_point_cloud_image_extractors_reset_test_hook();
#endif
#endif
}

int
lastProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK_ACTIVE)
  return pcl::io::detail::pcl_rvv_point_cloud_image_extractors_last_test_hook();
#else
  return pcl_rvv_point_cloud_image_extractors_last_test_hook();
#endif
#else
  return static_cast<int>(ProductionPathHook::None);
#endif
}

void
expectSameImage(const pcl::PCLImage& expected, const pcl::PCLImage& actual)
{
  EXPECT_EQ(expected.encoding, actual.encoding);
  EXPECT_EQ(expected.width, actual.width);
  EXPECT_EQ(expected.height, actual.height);
  EXPECT_EQ(expected.step, actual.step);
  EXPECT_EQ(expected.data, actual.data);
}

template <typename PointT>
void
expectRgbProductionMatchesScalar(const pcl::PointCloud<PointT>& cloud)
{
  pcl::PCLImage expected;
  pcl::PCLImage actual;
  pcl::io::PointCloudImageExtractorFromRGBField<PointT> extractor;
  ASSERT_TRUE(extractor.extract(cloud, expected));
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(extractor.extract(cloud, actual));
  expectSameImage(expected, actual);
}

template <typename PointT>
void
expectIntensityProductionMatchesScalar(const pcl::PointCloud<PointT>& cloud,
                                       typename pcl::io::PointCloudImageExtractorWithScaling<
                                           PointT>::ScalingMethod method)
{
  pcl::PCLImage expected;
  pcl::PCLImage actual;
  pcl::io::PointCloudImageExtractorFromIntensityField<PointT> extractor(method);
  ASSERT_TRUE(extractor.extract(cloud, expected));
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(extractor.extract(cloud, actual));
  expectSameImage(expected, actual);
}

void
expectLabelMono16ProductionMatchesScalar(const pcl::PointCloud<pcl::PointXYZL>& cloud)
{
  pcl::PCLImage expected;
  pcl::PCLImage actual;
  pcl::io::PointCloudImageExtractorFromLabelField<pcl::PointXYZL> extractor;
  extractor.setColorMode(extractor.COLORS_MONO);
  ASSERT_TRUE(extractor.extract(cloud, expected));
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(extractor.extract(cloud, actual));
  expectSameImage(expected, actual);
}

} // namespace

TEST(PointCloudImageExtractorsDiagnostic, RgbAndRgbaUnpackMatchScalarReference)
{
  const auto rgb_cloud = pcie::makeRgbCloud<pcl::PointXYZRGB>(17, 13);
  const auto rgba_cloud = pcie::makeRgbCloud<pcl::PointXYZRGBA>(19, 11);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;

  ASSERT_TRUE(pcie::extractRgbScalar(rgb_cloud, expected));
  ASSERT_TRUE(pcie::extractRgbCandidate(rgb_cloud, actual));
  EXPECT_EQ(expected, actual);

  ASSERT_TRUE(pcie::extractRgbScalar(rgba_cloud, expected));
  ASSERT_TRUE(pcie::extractRgbCandidate(rgba_cloud, actual));
  EXPECT_EQ(expected, actual);
}

TEST(PointCloudImageExtractorsDiagnostic, RgbSegmentStoreCandidateMatchesScalarReference)
{
  const auto rgb_cloud = pcie::makeRgbCloud<pcl::PointXYZRGB>(31, 10);
  const auto rgba_cloud = pcie::makeRgbCloud<pcl::PointXYZRGBA>(29, 9);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;

  ASSERT_TRUE(pcie::extractRgbScalar(rgb_cloud, expected));
  ASSERT_TRUE(pcie::extractRgbSegmentStoreCandidate(rgb_cloud, actual));
  EXPECT_EQ(expected, actual);

  ASSERT_TRUE(pcie::extractRgbScalar(rgba_cloud, expected));
  ASSERT_TRUE(pcie::extractRgbSegmentStoreCandidate(rgba_cloud, actual));
  EXPECT_EQ(expected, actual);
}

TEST(PointCloudImageExtractorsDiagnostic, ScalingModesMatchScalarReference)
{
  const auto cloud = pcie::makeScalingCloud(23, 9);

  for (const auto mode : {pcie::ScalingMode::NoScaling,
                          pcie::ScalingMode::FixedFactor,
                          pcie::ScalingMode::FullRange}) {
    std::vector<std::uint16_t> expected;
    std::vector<std::uint16_t> actual;
    ASSERT_TRUE(pcie::extractScalingScalar(cloud, mode, 1234.0f, expected));
    ASSERT_TRUE(pcie::extractScalingCandidate(cloud, mode, 1234.0f, actual));
    EXPECT_EQ(expected, actual) << "mode=" << static_cast<int>(mode);
  }
}

TEST(PointCloudImageExtractorsDiagnostic, FullRangeReductionCandidateMatchesScalarReference)
{
  auto cloud = pcie::makeScalingCloud(31, 10);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    // 这个 fixture（测试输入）让 min/max 落在不同 VL chunk，并让最后一个 chunk
    // 不是满 VL，避免只验证整齐长度下的规约路径。
    cloud[i].intensity = 0.125f + static_cast<float>((i * 53 + 17) % 251) * 0.015625f;
  }
  cloud[7].intensity = 0.03125f;
  cloud[cloud.size() - 3].intensity = 8.75f;

  std::vector<std::uint16_t> expected;
  std::vector<std::uint16_t> actual;
  ASSERT_TRUE(pcie::extractScalingScalar(cloud, pcie::ScalingMode::FullRange, 1.0f, expected));
  ASSERT_TRUE(pcie::extractScalingFullRangeReductionCandidate(cloud, actual));
  EXPECT_EQ(expected, actual);
}

TEST(PointCloudImageExtractorsDiagnostic, PaintNaNsWithBlackPostPassMatchesScalarReference)
{
  auto cloud = pcie::makeScalingCloud(9, 7);
  cloud.points[3].z = std::numeric_limits<float>::quiet_NaN();
  cloud.is_dense = false;

  std::vector<std::uint16_t> scalar_image;
  std::vector<std::uint16_t> candidate_image;
  ASSERT_TRUE(pcie::extractScalingScalar(cloud, pcie::ScalingMode::FullRange, 1.0f, scalar_image));
  ASSERT_TRUE(pcie::extractScalingCandidate(cloud, pcie::ScalingMode::FullRange, 1.0f, candidate_image));

  pcie::paintNaNsWithBlackScalar(cloud, scalar_image);
  pcie::paintNaNsWithBlackCandidate(cloud, candidate_image);
  EXPECT_EQ(scalar_image, candidate_image);
}

TEST(PointCloudImageExtractorsDiagnostic, NormalFieldCandidateMatchesScalarReference)
{
  // 这个测试覆盖 normal field（法线字段）到 rgb8 的生产标量语义：
  // 三个 float 字段分别执行 `(value + 1.0) * 127` 后转成 unsigned char。
  // 它仍是 test-only diagnostic，不证明 production dispatch 已存在。
  const auto cloud = pcie::makeNormalCloud(23, 11);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  ASSERT_TRUE(pcie::extractNormalScalar(cloud, expected));
  ASSERT_TRUE(pcie::extractNormalCandidate(cloud, actual));
  EXPECT_EQ(expected, actual);
}

TEST(PointCloudImageExtractorsDiagnostic, LabelMono16CandidateMatchesScalarReference)
{
  // 这个测试只覆盖 label mono16（标签转 16 位灰度）分支：production 标量
  // 语义会把 uint32_t label 截断成 unsigned short。random / Glasbey RGB
  // 分支有 map/set 状态，本阶段保持不覆盖。
  const auto cloud = pcie::makeLabelCloud(29, 9);

  std::vector<std::uint16_t> expected;
  std::vector<std::uint16_t> actual;
  ASSERT_TRUE(pcie::extractLabelMono16Scalar(cloud, expected));
  ASSERT_TRUE(pcie::extractLabelMono16Candidate(cloud, actual));
  EXPECT_EQ(expected, actual);
}

TEST(PointCloudImageExtractorsDiagnostic, ProductionProbeGatePolicyMatchesPi1Plan)
{
  // 这个测试把 Phase 040 冻结的 PI2 gate（准入条件）变成可失败验收。
  // 它不证明 production 已接入，只防止后续补丁把 exact 点类型范围意外扩大。
  EXPECT_TRUE(pcie::rgbProductionProbeGate<pcl::PointXYZRGB>().accepted);
  EXPECT_TRUE(pcie::rgbProductionProbeGate<pcl::PointXYZRGBA>().accepted);
  EXPECT_FALSE(pcie::rgbProductionProbeGate<pcl::PointXYZRGBL>().accepted);
  EXPECT_FALSE(pcie::rgbProductionProbeGate<pcl::PointXYZI>().accepted);

  EXPECT_TRUE(
      pcie::scalingProductionProbeGate<pcl::PointXYZI>("intensity", pcie::ScalingMode::FullRange)
          .accepted);
  EXPECT_FALSE(
      pcie::scalingProductionProbeGate<pcl::PointXYZI>("intensity", pcie::ScalingMode::FixedFactor)
          .accepted);
  EXPECT_FALSE(pcie::scalingProductionProbeGate<pcl::PointXYZI>("z", pcie::ScalingMode::FullRange)
                   .accepted);
  EXPECT_FALSE(
      pcie::scalingProductionProbeGate<pcl::PointXYZINormal>("intensity", pcie::ScalingMode::FullRange)
          .accepted);
}

TEST(PointCloudImageExtractorsProductionDirect, RgbAndRgbaHitProductionRvvPath)
{
  // 这个测试进入真实 `PointCloudImageExtractorFromRGBField` 公开入口。RVV build
  // 除了输出对拍，还必须通过测试 hook 证明 exact RGB/RGBA gate 命中生产 RVV 分流。
  const auto rgb_cloud = pcie::makeRgbCloud<pcl::PointXYZRGB>(23, 17);
  const auto rgba_cloud = pcie::makeRgbCloud<pcl::PointXYZRGBA>(19, 13);

  expectRgbProductionMatchesScalar(rgb_cloud);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::RgbRvv),
            lastProductionHook());
#endif

  expectRgbProductionMatchesScalar(rgba_cloud);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::RgbRvv),
            lastProductionHook());
#endif
}

TEST(PointCloudImageExtractorsProductionDirect, RgbNonExactPointTypeFallsBackToScalar)
{
  // `PointXYZRGBL` 有 RGB 字段，但 Phase 040/050 没有把它纳入 PI2 exact
  // 点型范围。该测试单独保护 gate-miss fallback（回退路径）。
  auto cloud = pcie::makeRgbCloud<pcl::PointXYZRGBL>(17, 11);
  for (std::size_t i = 0; i < cloud.size(); ++i)
    cloud[i].label = static_cast<std::uint32_t>(i * 3 + 1);

  expectRgbProductionMatchesScalar(cloud);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::RgbScalar),
            lastProductionHook());
#endif
}

TEST(PointCloudImageExtractorsProductionDirect, IntensityFullRangeHitsProductionRvvPath)
{
  // 真实 intensity extractor（强度字段提取器）只在 exact `PointXYZI` 和
  // full-range scaling（全范围缩放）下允许命中 RVV。输出必须保持 `PCLImage`
  // metadata 和 mono16 字节内容一致。
  auto cloud = pcie::makeScalingCloud(29, 13);
  cloud[5].intensity = 0.03125f;
  cloud[cloud.size() - 2].intensity = 9.5f;

  expectIntensityProductionMatchesScalar(
      cloud,
      pcl::io::PointCloudImageExtractorWithScaling<pcl::PointXYZI>::SCALING_FULL_RANGE);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::ScalingRvv),
            lastProductionHook());
#endif
}

TEST(PointCloudImageExtractorsProductionDirect, ScalingGateMissesFallBackToScalar)
{
  auto cloud = pcie::makeScalingCloud(19, 7);

  expectIntensityProductionMatchesScalar(
      cloud,
      pcl::io::PointCloudImageExtractorWithScaling<pcl::PointXYZI>::SCALING_FIXED_FACTOR);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::ScalingScalar),
            lastProductionHook());
#endif

  pcl::PCLImage expected_z;
  pcl::PCLImage actual_z;
  pcl::io::PointCloudImageExtractorFromZField<pcl::PointXYZI> z_extractor(
      pcl::io::PointCloudImageExtractorWithScaling<pcl::PointXYZI>::SCALING_FULL_RANGE);
  ASSERT_TRUE(z_extractor.extract(cloud, expected_z));
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(z_extractor.extract(cloud, actual_z));
  expectSameImage(expected_z, actual_z);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::ScalingScalar),
            lastProductionHook());
#endif
}

TEST(PointCloudImageExtractorsProductionDirect, LabelMono16HitsProductionRvvPath)
{
  // 真实 label extractor（标签字段提取器）只在 exact `PointXYZL` 和
  // `COLORS_MONO` 下允许命中 RVV。输出必须保持 `mono16` metadata 和
  // uint32 -> uint16 截断语义一致。
  const auto cloud = pcie::makeLabelCloud(31, 13);

  expectLabelMono16ProductionMatchesScalar(cloud);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::LabelRvv),
            lastProductionHook());
#endif
}

TEST(PointCloudImageExtractorsProductionDirect, LabelRgbModesStayOnExistingScalarBranches)
{
  // RGB_RANDOM 使用 time-based rand，不做字节级对拍；这里保护 production gate
  // 不会把非 mono label 模式误分流到 mono16 RVV helper。
  const auto cloud = pcie::makeLabelCloud(17, 11);

  pcl::PCLImage glasbey_image;
  pcl::io::PointCloudImageExtractorFromLabelField<pcl::PointXYZL> glasbey_extractor;
  glasbey_extractor.setColorMode(glasbey_extractor.COLORS_RGB_GLASBEY);
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(glasbey_extractor.extract(cloud, glasbey_image));
  EXPECT_EQ("rgb8", glasbey_image.encoding);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::None),
            lastProductionHook());
#endif

  pcl::PCLImage random_image;
  pcl::io::PointCloudImageExtractorFromLabelField<pcl::PointXYZL> random_extractor;
  random_extractor.setColorMode(random_extractor.COLORS_RGB_RANDOM);
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(random_extractor.extract(cloud, random_image));
  EXPECT_EQ("rgb8", random_image.encoding);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::None),
            lastProductionHook());
#endif
}

TEST(PointCloudImageExtractorsProductionDirect, RvvExtractImplStillUsesBaseNaNPostPass)
{
  auto cloud = pcie::makeScalingCloud(13, 7);
  cloud.points[3].z = std::numeric_limits<float>::quiet_NaN();
  cloud.is_dense = false;

  pcl::PCLImage image;
  pcl::io::PointCloudImageExtractorFromIntensityField<pcl::PointXYZI> extractor(
      pcl::io::PointCloudImageExtractorWithScaling<pcl::PointXYZI>::SCALING_FULL_RANGE);
  extractor.setPaintNaNsWithBlack(true);
#if defined(__RVV10__)
  resetProductionHook();
#endif
  ASSERT_TRUE(extractor.extract(cloud, image));
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::ScalingRvv),
            lastProductionHook());
#endif

  ASSERT_EQ("mono16", image.encoding);
  ASSERT_GE(image.data.size(), static_cast<std::size_t>(8));
  EXPECT_EQ(0u, image.data[6]);
  EXPECT_EQ(0u, image.data[7]);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
