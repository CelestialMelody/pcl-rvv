/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 color_modality topic 的
 * quantize+filter production-shaped diagnostic（生产形态诊断）是否与标量参考
 * 链路一致。RVV build 必须命中 RVV candidate；否则测试失败，防止纯标量
 * fallback 被误写成 RVV 证据。
 */

#define PCL_RVV_CM_TEST_HOOK
#include "cm.h"

#include <pcl/recognition/color_modality.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cm = pcl::recognition::rvv_test::color_modality;

namespace
{
enum class ProductionPathHook
{
  None = 0,
  Scalar = 1,
  Rvv = 2
};

#if defined(__RVV10__) && !defined(PCL_RVV_CM_TEST_HOOK_ACTIVE)
extern "C" __attribute__((weak)) void
pcl_rvv_cm_reset_test_hook()
{}

extern "C" __attribute__((weak)) int
pcl_rvv_cm_last_test_hook()
{
  return static_cast<int>(ProductionPathHook::None);
}
#endif

void
resetProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_CM_TEST_HOOK_ACTIVE)
  pcl::detail::pcl_rvv_cm_reset_test_hook();
#else
  pcl_rvv_cm_reset_test_hook();
#endif
#endif
}

void
setForceScalarProductionHook(const bool enabled)
{
#if defined(__RVV10__)
#if defined(PCL_RVV_CM_TEST_HOOK_ACTIVE)
  pcl::detail::pcl_rvv_cm_set_force_scalar_test_hook(enabled ? 1 : 0);
#else
  (void)enabled;
#endif
#else
  (void)enabled;
#endif
}

int
lastProductionHook()
{
#if defined(__RVV10__)
#if defined(PCL_RVV_CM_TEST_HOOK_ACTIVE)
  return pcl::detail::pcl_rvv_cm_last_test_hook();
#else
  return pcl_rvv_cm_last_test_hook();
#endif
#else
  return static_cast<int>(ProductionPathHook::None);
#endif
}

class TestColorModality : public pcl::ColorModality<pcl::PointXYZRGB>
{
  public:
    void
    extractAllFeatures(const pcl::MaskMap& mask,
                       std::size_t nr_features,
                       std::size_t modality_index,
                       std::vector<pcl::QuantizedMultiModFeature>& features) const override
    {
      extractFeatures(mask, nr_features, modality_index, features);
    }
};

pcl::PointCloud<pcl::PointXYZRGB>::Ptr
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
} // namespace

TEST(ColorModalityDiagnostic, ScalarReferenceKeepsExpectedMapSizes)
{
  const auto cloud = makeProductionCloud(23, 17);
  cm::ColorArtifacts artifacts;

  cm::computeColorArtifactsScalar(cloud->points.data(), cloud->width, cloud->height, artifacts);

  EXPECT_EQ(artifacts.quantized.size(), cloud->width * cloud->height);
  EXPECT_EQ(artifacts.filtered.size(), cloud->width * cloud->height);
  EXPECT_EQ(artifacts.spreaded.size(), cloud->width * cloud->height);
}

TEST(ColorModalityDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarReference)
{
  const auto cloud = makeProductionCloud(65, 37);
  cm::ColorArtifacts reference;
  cm::ColorArtifacts candidate;

  cm::computeColorArtifactsScalar(cloud->points.data(), cloud->width, cloud->height, reference);
  const auto path =
      cm::computeColorArtifactsCandidate(cloud->points.data(), cloud->width, cloud->height, candidate);

#if defined(__RVV10__)
  EXPECT_EQ(path, cm::ExecutionPath::RvvQuantizeFilter);
#else
  EXPECT_EQ(path, cm::ExecutionPath::ScalarFallback);
#endif
  EXPECT_EQ(reference.quantized, candidate.quantized);
  EXPECT_EQ(reference.filtered, candidate.filtered);
  EXPECT_EQ(reference.spreaded, candidate.spreaded);
}

TEST(ColorModalityProductionDirect, ProcessInputDataProducesUsableMaps)
{
  const auto cloud = makeProductionCloud(64, 48);
  TestColorModality modality;
  modality.setInputCloud(cloud);
  modality.processInputData();

  EXPECT_EQ(modality.getQuantizedMap().getWidth(), cloud->width);
  EXPECT_EQ(modality.getQuantizedMap().getHeight(), cloud->height);
  EXPECT_EQ(modality.getSpreadedQuantizedMap().getWidth(), cloud->width);
  EXPECT_EQ(modality.getSpreadedQuantizedMap().getHeight(), cloud->height);
  EXPECT_GT(countNonZero(modality.getQuantizedMap()), 0u);
  EXPECT_GT(countNonZero(modality.getSpreadedQuantizedMap()), 0u);
}

TEST(ColorModalityProductionDirect, RvvBuildHitsProcessInputDataFilterPipeline)
{
  const auto cloud = makeProductionCloud(65, 49);
  TestColorModality modality;
  modality.setInputCloud(cloud);

  resetProductionHook();
  modality.processInputData();

#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Rvv));
#else
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::None));
#endif
}

TEST(ColorModalityProductionDirect, RvvProcessInputDataMatchesForcedScalarPipeline)
{
  const auto cloud = makeProductionCloud(129, 97);

  TestColorModality scalar_modality;
  scalar_modality.setInputCloud(cloud);
  resetProductionHook();
  setForceScalarProductionHook(true);
  scalar_modality.processInputData();
#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Scalar));
#endif
  const auto scalar_quantized = copyMap(scalar_modality.getQuantizedMap());
  const auto scalar_spreaded = copyMap(scalar_modality.getSpreadedQuantizedMap());

  TestColorModality rvv_modality;
  rvv_modality.setInputCloud(cloud);
  resetProductionHook();
  rvv_modality.processInputData();
#if defined(__RVV10__)
  EXPECT_EQ(lastProductionHook(), static_cast<int>(ProductionPathHook::Rvv));
#endif

  EXPECT_EQ(scalar_quantized, copyMap(rvv_modality.getQuantizedMap()));
  EXPECT_EQ(scalar_spreaded, copyMap(rvv_modality.getSpreadedQuantizedMap()));
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
