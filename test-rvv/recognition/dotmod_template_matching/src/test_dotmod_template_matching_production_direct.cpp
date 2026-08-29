#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "impl/dotmod_template_matching_production_direct.hpp"

namespace
{

std::vector<pcl::DOTMODDetection>
runProductionDetectTemplates (const std::size_t image_width,
                              const std::size_t image_height,
                              const std::size_t template_width,
                              const std::size_t template_height,
                              const std::size_t nr_templates,
                              const std::size_t nr_modalities,
                              const float threshold,
                              const std::size_t bin_size)
{
  auto modalities_storage = pcl::test::dotmod_rvv::makeFixedModalities (image_width, image_height, nr_modalities);
  auto modalities = pcl::test::dotmod_rvv::modalityPointers (modalities_storage);
  auto masks_storage = pcl::test::dotmod_rvv::makeFullMasks (image_width, image_height, nr_modalities);
  auto masks = pcl::test::dotmod_rvv::maskPointers (masks_storage);
  auto dotmod = pcl::test::dotmod_rvv::makeProductionDOTMOD (
      template_width, template_height, modalities, masks, nr_templates);

  std::vector<pcl::DOTMODDetection> detections;
  dotmod.detectTemplates (modalities, threshold, detections, bin_size);
  return detections;
}

} // namespace

TEST (DOTMODTemplateMatchingProductionDirect, DetectTemplatesProducesStableDetections)
{
  // 真实入口 smoke（小型验证）通过 createAndAddTemplate() 和 detectTemplates()
  // 进入 production 源码。若 RVV 路径改变输出顺序、template_id 或 score，
  // Std/RVV 对拍和 checksum 都会变化。
  const auto detections = runProductionDetectTemplates (37, 29, 12, 8, 4, 2, 0.55f, 4);

  ASSERT_FALSE (detections.empty ());
  for (const auto& detection : detections)
  {
    EXPECT_LT (detection.bin_x, 37u);
    EXPECT_LT (detection.bin_y, 29u);
    EXPECT_LT (detection.template_id, 4u);
    EXPECT_GT (detection.score, 0.55f);
  }
  EXPECT_NE (pcl::test::dotmod_rvv::checksumDetections (detections), 0u);
}

TEST (DOTMODTemplateMatchingProductionDirect, DetectTemplatesKeepsStrictThreshold)
{
  // score 等于 threshold 时不输出 detection；这个测试锁住 production 中的
  // `>` 语义，防止生产接入时把阈值边界顺手改成 `>=`。
  const auto detections = runProductionDetectTemplates (16, 12, 8, 4, 2, 1, 1.0f, 4);

  EXPECT_TRUE (detections.empty ());
}
