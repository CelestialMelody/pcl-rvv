/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 surface_normal_modality topic 的
 * depth-to-normal / quantize production-shaped diagnostic（生产形态诊断）和
 * 真实 `SurfaceNormalModality::processInputData()` production direct（真实生产路径）
 * 是否与标量同构链路一致。RVV build 必须命中 RVV candidate（候选链路），并在
 * Phase 020 之后命中 filter RVV hook；否则测试失败，避免把纯标量 fallback（回退路径）
 * 或上一阶段 RVV 命中误写成当前 filter 证据。
 */

#include "snm.h"

#define PCL_RVV_SNM_TEST_HOOK
#include <pcl/recognition/surface_normal_modality.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace snm = pcl::recognition::rvv_test::surface_normal_modality;

namespace {

enum class ProductionPathHook
{
  None = 0,
  Scalar = 1,
  Rvv = 2,
  FilterRvv = 3,
  SpreadRvv = 4
};

#if defined(__RVV10__) && !defined(PCL_RVV_SNM_TEST_HOOK_ACTIVE)
extern "C" __attribute__((weak)) void
pcl_rvv_snm_reset_test_hook()
{}

extern "C" __attribute__((weak)) int
pcl_rvv_snm_last_test_hook()
{
  return static_cast<int>(ProductionPathHook::None);
}
#endif

void
resetProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_SNM_TEST_HOOK_ACTIVE)
  pcl::detail::pcl_rvv_snm_reset_test_hook();
#else
  pcl_rvv_snm_reset_test_hook();
#endif
#endif
}

void
setForceScalarProductionHook(const bool enabled)
{
#if defined(__RVV10__)
#if defined(PCL_RVV_SNM_TEST_HOOK_ACTIVE)
  pcl::detail::pcl_rvv_snm_set_force_scalar_test_hook(enabled ? 1 : 0);
#endif
#else
  (void)enabled;
#endif
}

int
lastProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_SNM_TEST_HOOK_ACTIVE)
  return pcl::detail::pcl_rvv_snm_last_test_hook();
#else
  return pcl_rvv_snm_last_test_hook();
#endif
#else
  return static_cast<int>(ProductionPathHook::None);
#endif
}

std::vector<pcl::PointXYZ>
makeDepthCloud(const std::size_t width, const std::size_t height)
{
  std::vector<pcl::PointXYZ> cloud(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      auto& point = cloud[y * width + x];
      point.x = static_cast<float>(x);
      point.y = static_cast<float>(y);
      point.z = 0.62f + 0.0009f * static_cast<float>(x) + 0.0017f * static_cast<float>(y);
    }
  }

  cloud[6 * width + 6].z = std::numeric_limits<float>::quiet_NaN();
  cloud[7 * width + 8].z = 2.25f;
  cloud[10 * width + 11].z += 0.09f;
  return cloud;
}

pcl::PointCloud<pcl::PointXYZRGBA>::Ptr
makeProductionCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGBA>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      auto& point = (*cloud)(x, y);
      point.x = static_cast<float>(x);
      point.y = static_cast<float>(y);
      point.z = 0.62f + 0.0009f * static_cast<float>(x) + 0.0017f * static_cast<float>(y);
      point.r = static_cast<std::uint8_t>((5 * x + 13 * y + 17) & 0xff);
      point.g = static_cast<std::uint8_t>((19 * x + 7 * y + 23) & 0xff);
      point.b = static_cast<std::uint8_t>((11 * x + 29 * y + 31) & 0xff);
      point.a = 255;
    }
  }
  (*cloud)(6, 6).z = std::numeric_limits<float>::quiet_NaN();
  (*cloud)(8, 7).z = 2.25f;
  (*cloud)(11, 10).z += 0.09f;
  return cloud;
}

void
expectSameCells(const std::vector<snm::NormalCell>& reference,
                const std::vector<snm::NormalCell>& candidate,
                const std::size_t width,
                const std::size_t height)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      const auto index = y * width + x;
      EXPECT_EQ(reference[index].quantized, candidate[index].quantized) << "x=" << x << " y=" << y;
      EXPECT_NEAR(reference[index].angle_degrees, candidate[index].angle_degrees, 1.0e-4f)
          << "x=" << x << " y=" << y;
    }
  }
}

std::vector<std::uint8_t>
copyMap(const pcl::QuantizedMap& map)
{
  const auto size = map.getWidth() * map.getHeight();
  const auto* data = map.getData();
  return std::vector<std::uint8_t>(data, data + size);
}

void
expectSameOrientationMap(const pcl::LINEMOD_OrientationMap& reference,
                         const pcl::LINEMOD_OrientationMap& candidate)
{
  ASSERT_EQ(reference.getWidth(), candidate.getWidth());
  ASSERT_EQ(reference.getHeight(), candidate.getHeight());
  for (std::size_t y = 0; y < reference.getHeight(); ++y) {
    for (std::size_t x = 0; x < reference.getWidth(); ++x)
      EXPECT_NEAR(reference(x, y), candidate(x, y), 1.0e-4f) << "x=" << x << " y=" << y;
  }
}

} // namespace

TEST(SurfaceNormalModalityDiagnostic, ScalarReferenceKeepsOuterBandZero)
{
  const std::size_t width = 31;
  const std::size_t height = 23;
  const auto cloud = makeDepthCloud(width, height);
  std::vector<snm::NormalCell> reference;

  snm::computeDepthNormalQuantizedScalar(cloud.data(), width, height, reference);

  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      if (x < 5 || y < 5 || x >= width - 6 || y >= height - 6)
        EXPECT_EQ(reference[y * width + x].quantized, 0) << "x=" << x << " y=" << y;
    }
  }
}

TEST(SurfaceNormalModalityDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarQuantization)
{
  const std::size_t width = 65;
  const std::size_t height = 37;
  const auto cloud = makeDepthCloud(width, height);
  std::vector<snm::NormalCell> reference;
  std::vector<snm::NormalCell> candidate;

  snm::computeDepthNormalQuantizedScalar(cloud.data(), width, height, reference);
  const auto path = snm::computeDepthNormalQuantizedCandidate(cloud.data(), width, height, candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, snm::ExecutionPath::RvvDepthQuantize);
#else
  EXPECT_EQ(path, snm::ExecutionPath::ScalarFallback);
#endif
  expectSameCells(reference, candidate, width, height);
}

TEST(SurfaceNormalModalityProductionDirect, PublicEntryHitsRvvPathAndMatchesForcedScalar)
{
  constexpr std::size_t width = 65;
  constexpr std::size_t height = 37;
  auto cloud = makeProductionCloud(width, height);

  pcl::SurfaceNormalModality<pcl::PointXYZRGBA> scalar_modality;
  scalar_modality.setInputCloud(cloud);
  resetProductionHook();
  setForceScalarProductionHook(true);
  scalar_modality.processInputData();
  const auto scalar_hook = lastProductionHook();
  const auto scalar_quantized = copyMap(scalar_modality.getQuantizedMap());
  const auto scalar_spreaded = copyMap(scalar_modality.getSpreadedQuantizedMap());

  pcl::SurfaceNormalModality<pcl::PointXYZRGBA> rvv_modality;
  rvv_modality.setInputCloud(cloud);
  resetProductionHook();
  setForceScalarProductionHook(false);
  rvv_modality.processInputData();
  const auto rvv_hook = lastProductionHook();

#if defined(__RVV10__)
  EXPECT_EQ(scalar_hook, static_cast<int>(ProductionPathHook::Scalar));
  EXPECT_EQ(rvv_hook, static_cast<int>(ProductionPathHook::SpreadRvv));
#else
  EXPECT_EQ(scalar_hook, static_cast<int>(ProductionPathHook::None));
  EXPECT_EQ(rvv_hook, static_cast<int>(ProductionPathHook::None));
#endif
  EXPECT_EQ(copyMap(rvv_modality.getQuantizedMap()), scalar_quantized);
  EXPECT_EQ(copyMap(rvv_modality.getSpreadedQuantizedMap()), scalar_spreaded);
  expectSameOrientationMap(scalar_modality.getOrientationMap(), rvv_modality.getOrientationMap());
}

TEST(SurfaceNormalModalityProductionDirect, NonDefaultSpreadSizeKeepsScalarSpreadPath)
{
  constexpr std::size_t width = 65;
  constexpr std::size_t height = 37;
  auto cloud = makeProductionCloud(width, height);

  pcl::SurfaceNormalModality<pcl::PointXYZRGBA> scalar_modality;
  scalar_modality.setInputCloud(cloud);
  scalar_modality.setSpreadingSize(6);
  resetProductionHook();
  setForceScalarProductionHook(true);
  scalar_modality.processInputData();
  const auto scalar_spreaded = copyMap(scalar_modality.getSpreadedQuantizedMap());

  pcl::SurfaceNormalModality<pcl::PointXYZRGBA> rvv_modality;
  rvv_modality.setInputCloud(cloud);
  rvv_modality.setSpreadingSize(6);
  resetProductionHook();
  setForceScalarProductionHook(false);
  rvv_modality.processInputData();
  const auto rvv_hook = lastProductionHook();

#if defined(__RVV10__)
  EXPECT_EQ(rvv_hook, static_cast<int>(ProductionPathHook::FilterRvv));
#else
  EXPECT_EQ(rvv_hook, static_cast<int>(ProductionPathHook::None));
#endif
  EXPECT_EQ(copyMap(rvv_modality.getSpreadedQuantizedMap()), scalar_spreaded);
}

TEST(SurfaceNormalModalityProductionDirect, SmallOrganizedInputFallsBackToScalar)
{
  constexpr std::size_t width = 9;
  constexpr std::size_t height = 9;
  auto cloud = makeProductionCloud(width, height);

  pcl::SurfaceNormalModality<pcl::PointXYZRGBA> modality;
  modality.setInputCloud(cloud);
  resetProductionHook();
  setForceScalarProductionHook(false);
  modality.processInputData();

#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Scalar));
#else
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::None));
#endif
}
