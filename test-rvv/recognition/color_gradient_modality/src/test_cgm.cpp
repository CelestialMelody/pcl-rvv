/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 color_gradient_modality topic 的
 * Sobel+quantize production-shaped diagnostic（生产形态诊断）是否与标量同构
 * 链路一致。RVV build 必须命中 RVV candidate（候选链路）；否则测试失败，
 * 防止纯标量 fallback 被误写成 RVV 证据。
 */

#define PCL_RVV_CGM_TEST_HOOK
#include "cgm.h"

#include <pcl/recognition/color_gradient_modality.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

namespace cgm = pcl::recognition::rvv_test::color_gradient_modality;

namespace {

enum class ProductionPathHook
{
  None = 0,
  Scalar = 1,
  Rvv = 2
};

#if defined(__RVV10__) && !defined(PCL_RVV_CGM_TEST_HOOK_ACTIVE)
extern "C" __attribute__((weak)) void
pcl_rvv_cgm_reset_test_hook()
{}

extern "C" __attribute__((weak)) int
pcl_rvv_cgm_last_test_hook()
{
  return static_cast<int>(ProductionPathHook::None);
}
#endif

void
resetProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_CGM_TEST_HOOK_ACTIVE)
  pcl::detail::pcl_rvv_cgm_reset_test_hook();
#else
  pcl_rvv_cgm_reset_test_hook();
#endif
#endif
}

void
setForceScalarProductionHook(const bool enabled)
{
#if defined(__RVV10__)
#if defined(PCL_RVV_CGM_TEST_HOOK_ACTIVE)
  pcl::detail::pcl_rvv_cgm_set_force_scalar_test_hook(enabled ? 1 : 0);
#endif
#else
  (void)enabled;
#endif
}

int
lastProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_CGM_TEST_HOOK_ACTIVE)
  return pcl::detail::pcl_rvv_cgm_last_test_hook();
#else
  return pcl_rvv_cgm_last_test_hook();
#endif
#else
  return static_cast<int>(ProductionPathHook::None);
#endif
}

std::vector<pcl::RGB>
makePattern(const std::size_t width, const std::size_t height)
{
  std::vector<pcl::RGB> image(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      const auto r = static_cast<std::uint8_t>((11 * x + 7 * y + 5 * ((x + y) & 3)) & 0xff);
      const auto g = static_cast<std::uint8_t>((3 * x + 17 * y + 13 * ((x ^ y) & 7)) & 0xff);
      const auto b = static_cast<std::uint8_t>((23 * x + 19 * y + 29 * ((x + 2 * y) & 5)) & 0xff);
      image[y * width + x] = pcl::RGB(r, g, b);
    }
  }

  // 手工放一个低幅值区域，确保阈值 mask（阈值掩码）能把量化值压成 0。
  image[2 * width + 2] = pcl::RGB(42, 42, 42);
  image[2 * width + 3] = pcl::RGB(42, 42, 42);
  image[3 * width + 2] = pcl::RGB(42, 42, 42);
  image[3 * width + 3] = pcl::RGB(42, 42, 42);
  return image;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr
makeProductionCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
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

std::size_t
countNonZero(const pcl::QuantizedMap& map)
{
  std::size_t count = 0;
  const auto size = map.getWidth() * map.getHeight();
  const auto* data = map.getData();
  for (std::size_t i = 0; i < size; ++i)
    count += data[i] != 0 ? 1u : 0u;
  return count;
}

std::vector<std::uint8_t>
copyMap(const pcl::QuantizedMap& map)
{
  const auto size = map.getWidth() * map.getHeight();
  const auto* data = map.getData();
  return std::vector<std::uint8_t>(data, data + size);
}

void
expectSameGradients(const pcl::PointCloud<pcl::GradientXY>& reference,
                    const pcl::PointCloud<pcl::GradientXY>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  ASSERT_EQ(reference.width, candidate.width);
  ASSERT_EQ(reference.height, candidate.height);
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_NEAR(reference[i].magnitude, candidate[i].magnitude, 1.0e-4f) << "index=" << i;
    EXPECT_NEAR(reference[i].angle, candidate[i].angle, 1.0e-2f)
        << "index=" << i << " ref_xy=(" << reference[i].x << "," << reference[i].y
        << ") cand_xy=(" << candidate[i].x << "," << candidate[i].y
        << ") ref_mag=" << reference[i].magnitude << " cand_mag=" << candidate[i].magnitude
        << " ref_angle=" << reference[i].angle << " cand_angle=" << candidate[i].angle;
  }
}

void
expectSameFeatures(const std::vector<pcl::QuantizedMultiModFeature>& reference,
                   const std::vector<pcl::QuantizedMultiModFeature>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_TRUE(reference[i].compareForEquality(candidate[i]))
        << "index=" << i << " ref=(" << reference[i].x << "," << reference[i].y << ","
        << static_cast<int>(reference[i].quantized_value) << ") cand=(" << candidate[i].x
        << "," << candidate[i].y << "," << static_cast<int>(candidate[i].quantized_value)
        << ")";
  }
}

void
extractMaskedFeatures(pcl::ColorGradientModality<pcl::PointXYZRGB>& modality,
                      const std::size_t width,
                      const std::size_t height,
                      std::vector<pcl::QuantizedMultiModFeature>& features)
{
  pcl::MaskMap mask(width, height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x)
      mask.set(x, y);
  }
  modality.extractFeatures(mask, 32, 3, features);
}

void
expectSameCells(const std::vector<cgm::GradientCell>& reference,
                const std::vector<cgm::GradientCell>& candidate,
                const std::size_t width,
                const std::size_t height)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      const auto index = y * width + x;
      EXPECT_EQ(reference[index].quantized, candidate[index].quantized) << "x=" << x << " y=" << y;
      EXPECT_NEAR(reference[index].magnitude, candidate[index].magnitude, 1.0e-4f)
          << "x=" << x << " y=" << y;
      EXPECT_NEAR(reference[index].angle_degrees, candidate[index].angle_degrees, 1.0e-2f)
          << "x=" << x << " y=" << y;
    }
  }
}

} // namespace

TEST(ColorGradientModalityDiagnostic, ScalarSobelQuantizeKeepsBorderZero)
{
  const std::size_t width = 17;
  const std::size_t height = 11;
  const auto image = makePattern(width, height);
  std::vector<cgm::GradientCell> reference;

  cgm::computeSobelQuantizedScalar(image.data(), width, height, 10.0f, reference);

  for (std::size_t x = 0; x < width; ++x) {
    EXPECT_EQ(reference[x].quantized, 0);
    EXPECT_EQ(reference[(height - 1) * width + x].quantized, 0);
  }
  for (std::size_t y = 0; y < height; ++y) {
    EXPECT_EQ(reference[y * width].quantized, 0);
    EXPECT_EQ(reference[y * width + width - 1].quantized, 0);
  }
}

TEST(ColorGradientModalityDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarQuantization)
{
  const std::size_t width = 65;
  const std::size_t height = 19;
  const auto image = makePattern(width, height);
  std::vector<cgm::GradientCell> reference;
  std::vector<cgm::GradientCell> candidate;

  cgm::computeSobelQuantizedScalar(image.data(), width, height, 10.0f, reference);
  const auto path = cgm::computeSobelQuantizedCandidate(image.data(), width, height, 10.0f, candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, cgm::ExecutionPath::RvvSobelQuantize);
#else
  EXPECT_EQ(path, cgm::ExecutionPath::ScalarFallback);
#endif
  expectSameCells(reference, candidate, width, height);
}

TEST(ColorGradientModalityDiagnostic, DominantFilterMatchesProductionTieBreakAndThreshold)
{
  const std::size_t width = 19;
  const std::size_t height = 13;
  std::vector<std::uint8_t> input(width * height, 0);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      input[y * width + x] = static_cast<std::uint8_t>(((x * 3 + y * 5) % 8) + 1);
    }
  }

  // 构造一个 5:4 邻域，验证超过阈值后输出 production 的 one-hot 方向。
  const std::array<std::uint8_t, 9> majority_patch = {3, 3, 3, 3, 3, 4, 4, 4, 4};
  std::size_t k = 0;
  for (std::size_t y = 4; y <= 6; ++y) {
    for (std::size_t x = 4; x <= 6; ++x) {
      input[y * width + x] = majority_patch[k++];
    }
  }

  // 构造一个最大计数为 4 的邻域，应该被 `>=5` 阈值压成 0。
  const std::array<std::uint8_t, 9> below_threshold_patch = {8, 8, 8, 8, 7, 7, 7, 6, 5};
  k = 0;
  for (std::size_t y = 7; y <= 9; ++y) {
    for (std::size_t x = 11; x <= 13; ++x) {
      input[y * width + x] = below_threshold_patch[k++];
    }
  }

  std::vector<std::uint8_t> reference;
  std::vector<std::uint8_t> candidate;
  cgm::filterQuantizedGradientsScalar(input.data(), width, height, reference);
  const auto path = cgm::filterQuantizedGradientsCandidate(input.data(), width, height, candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, cgm::ExecutionPath::RvvDominantFilter);
#else
  EXPECT_EQ(path, cgm::ExecutionPath::ScalarFallback);
#endif
  EXPECT_EQ(reference, candidate);
  EXPECT_EQ(candidate[5 * width + 5], static_cast<std::uint8_t>(1u << 2));
  EXPECT_EQ(candidate[8 * width + 12], 0);
}

TEST(ColorGradientModalityDiagnostic, FullChainSobelQuantizeFilterMatchesScalarSubchain)
{
  const std::size_t width = 67;
  const std::size_t height = 23;
  const auto image = makePattern(width, height);
  std::vector<std::uint8_t> reference;
  std::vector<std::uint8_t> candidate;

  cgm::computeSobelQuantizedFilteredScalar(image.data(), width, height, 10.0f, reference);
  const auto path =
      cgm::computeSobelQuantizedFilteredCandidate(image.data(), width, height, 10.0f, candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, cgm::ExecutionPath::RvvFullChain);
#else
  EXPECT_EQ(path, cgm::ExecutionPath::ScalarFallback);
#endif
  EXPECT_EQ(reference, candidate);
}

TEST(ColorGradientModalityDiagnostic, FullStencilCandidateMatchesScalarSubchain)
{
  const std::size_t width = 67;
  const std::size_t height = 23;
  const auto image = makePattern(width, height);
  std::vector<std::uint8_t> reference;
  std::vector<std::uint8_t> candidate;

  cgm::computeSobelQuantizedFilteredScalar(image.data(), width, height, 10.0f, reference);
  const auto path =
      cgm::computeSobelQuantizedStencilCandidate(image.data(), width, height, 10.0f, candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, cgm::ExecutionPath::RvvFullStencilChain);
#else
  EXPECT_EQ(path, cgm::ExecutionPath::ScalarFallback);
#endif
  EXPECT_EQ(reference, candidate);
}

TEST(ColorGradientModalityProductionDirect, ProcessInputDataProducesUsableMapsAndFeatures)
{
  auto cloud = makeProductionCloud(64, 48);
  pcl::ColorGradientModality<pcl::PointXYZRGB> modality;
  modality.setGradientMagnitudeThreshold(10.0f);
  modality.setGradientMagnitudeThresholdForFeatureExtraction(20.0f);
  modality.setSpreadingSize(8);
  modality.setInputCloud(cloud);
  modality.processInputData();

  EXPECT_EQ(modality.getQuantizedMap().getWidth(), cloud->width);
  EXPECT_EQ(modality.getQuantizedMap().getHeight(), cloud->height);
  EXPECT_EQ(modality.getSpreadedQuantizedMap().getWidth(), cloud->width);
  EXPECT_EQ(modality.getSpreadedQuantizedMap().getHeight(), cloud->height);
  EXPECT_EQ(modality.getMaxColorGradients().width, cloud->width);
  EXPECT_EQ(modality.getMaxColorGradients().height, cloud->height);
  EXPECT_GT(countNonZero(modality.getQuantizedMap()), 0u);
  EXPECT_GT(countNonZero(modality.getSpreadedQuantizedMap()), 0u);

  pcl::MaskMap mask(cloud->width, cloud->height);
  for (std::size_t y = 0; y < cloud->height; ++y) {
    for (std::size_t x = 0; x < cloud->width; ++x)
      mask.set(x, y);
  }
  std::vector<pcl::QuantizedMultiModFeature> features;
  modality.extractFeatures(mask, 16, 0, features);
  EXPECT_GT(features.size(), 0u);
}

TEST(ColorGradientModalityProductionDirect, RvvBuildHitsProcessInputDataPipeline)
{
  auto cloud = makeProductionCloud(65, 49);
  pcl::ColorGradientModality<pcl::PointXYZRGB> modality;
  modality.setGradientMagnitudeThreshold(10.0f);
  modality.setInputCloud(cloud);

  resetProductionHook();
  modality.processInputData();

#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Rvv));
#else
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::None));
#endif
}

// 这个测试验证真实 public entry（公开入口）在同一个 RVV build 中，RVV 分流和
// 强制标量 fallback（回退路径）生成相同的 quantized map、spread map 和梯度。
// 它比单纯 path-hit 更强：失败说明生产接入的输出语义已经和原链路分叉。
TEST(ColorGradientModalityProductionDirect, RvvProcessInputDataMatchesForcedScalarPipeline)
{
  auto cloud = makeProductionCloud(129, 97);

  pcl::ColorGradientModality<pcl::PointXYZRGB> scalar_modality;
  scalar_modality.setGradientMagnitudeThreshold(10.0f);
  scalar_modality.setGradientMagnitudeThresholdForFeatureExtraction(20.0f);
  scalar_modality.setSpreadingSize(8);
  scalar_modality.setInputCloud(cloud);

  resetProductionHook();
  setForceScalarProductionHook(true);
  scalar_modality.processInputData();
#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Scalar));
#endif
  const auto scalar_quantized = copyMap(scalar_modality.getQuantizedMap());
  const auto scalar_spreaded = copyMap(scalar_modality.getSpreadedQuantizedMap());
  const auto scalar_gradients = scalar_modality.getMaxColorGradients();
  std::vector<pcl::QuantizedMultiModFeature> scalar_features;
  extractMaskedFeatures(scalar_modality, cloud->width, cloud->height, scalar_features);

  pcl::ColorGradientModality<pcl::PointXYZRGB> rvv_modality;
  rvv_modality.setGradientMagnitudeThreshold(10.0f);
  rvv_modality.setGradientMagnitudeThresholdForFeatureExtraction(20.0f);
  rvv_modality.setSpreadingSize(8);
  rvv_modality.setInputCloud(cloud);

  resetProductionHook();
  rvv_modality.processInputData();
#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Rvv));
#endif
  EXPECT_EQ(scalar_quantized, copyMap(rvv_modality.getQuantizedMap()));
  EXPECT_EQ(scalar_spreaded, copyMap(rvv_modality.getSpreadedQuantizedMap()));
  expectSameGradients(scalar_gradients, rvv_modality.getMaxColorGradients());
  std::vector<pcl::QuantizedMultiModFeature> rvv_features;
  extractMaskedFeatures(rvv_modality, cloud->width, cloud->height, rvv_features);
  expectSameFeatures(scalar_features, rvv_features);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
