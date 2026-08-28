#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "impl/linemod_template_scoring_production_direct.hpp"

namespace
{

std::vector<pcl::LINEMODDetection>
runMatchTemplates (const std::size_t mem_size, const std::size_t nr_features)
{
  const std::size_t step_size = 8;
  const std::size_t lin_width = pcl::test::linemod_rvv::chooseProductionLinWidth (mem_size);
  const std::size_t lin_height = mem_size / lin_width;
  pcl::test::linemod_rvv::FixedQuantizableModality modality (lin_width * step_size, lin_height * step_size);
  std::vector<pcl::QuantizableModality*> modalities{&modality};
  auto linemod = pcl::test::linemod_rvv::makeProductionLinemod (nr_features);
  std::vector<pcl::LINEMODDetection> detections;
  linemod.matchTemplates (modalities, detections);
  return detections;
}

std::vector<pcl::LINEMODDetection>
runDetectTemplates (const std::size_t mem_size, const std::size_t nr_features)
{
  const std::size_t step_size = 8;
  const std::size_t lin_width = pcl::test::linemod_rvv::chooseProductionLinWidth (mem_size);
  const std::size_t lin_height = mem_size / lin_width;
  pcl::test::linemod_rvv::FixedQuantizableModality modality (lin_width * step_size, lin_height * step_size);
  std::vector<pcl::QuantizableModality*> modalities{&modality};
  auto linemod = pcl::test::linemod_rvv::makeProductionLinemod (nr_features);
  std::vector<pcl::LINEMODDetection> detections;
  linemod.detectTemplates (modalities, detections);
  return detections;
}

std::vector<pcl::LINEMODDetection>
runSemiScaleDetectTemplates (const std::size_t mem_size, const std::size_t nr_features)
{
  const std::size_t step_size = 8;
  const std::size_t lin_width = pcl::test::linemod_rvv::chooseProductionLinWidth (mem_size);
  const std::size_t lin_height = mem_size / lin_width;
  pcl::test::linemod_rvv::FixedQuantizableModality modality (lin_width * step_size, lin_height * step_size);
  std::vector<pcl::QuantizableModality*> modalities{&modality};
  auto linemod = pcl::test::linemod_rvv::makeProductionLinemod (nr_features);
  std::vector<pcl::LINEMODDetection> detections;
  linemod.detectTemplatesSemiScaleInvariant (modalities, detections, 1.0f, 2.0f, 2.0f);
  return detections;
}

} // namespace

TEST (LINEMODTemplateScoringProductionDirect, MatchTemplatesProducesStableDetection)
{
  const auto detections = runMatchTemplates (257, 17);

  ASSERT_EQ (detections.size (), 1u);
  EXPECT_GE (detections[0].x, 0);
  EXPECT_GE (detections[0].y, 0);
  EXPECT_EQ (detections[0].template_id, 0);
  EXPECT_GT (detections[0].score, 0.0f);
  EXPECT_NE (pcl::test::linemod_rvv::checksumDetections (detections), 0u);
}

TEST (LINEMODTemplateScoringProductionDirect, DetectTemplatesKeepsDefaultEntrySemantics)
{
  const auto detections = runDetectTemplates (257, 17);

  ASSERT_FALSE (detections.empty ());
  for (const auto& detection : detections)
  {
    EXPECT_GE (detection.x, 0);
    EXPECT_GE (detection.y, 0);
    EXPECT_EQ (detection.template_id, 0);
    EXPECT_GT (detection.score, 0.0f);
  }
  EXPECT_NE (pcl::test::linemod_rvv::checksumDetections (detections), 0u);
}

TEST (LINEMODTemplateScoringProductionDirect, SemiScaleDetectTemplatesKeepsScaleSemantics)
{
  const auto detections = runSemiScaleDetectTemplates (257, 17);

  ASSERT_FALSE (detections.empty ());
  bool saw_scale_one = false;
  bool saw_scale_two = false;
  for (const auto& detection : detections)
  {
    EXPECT_GE (detection.x, 0);
    EXPECT_GE (detection.y, 0);
    EXPECT_EQ (detection.template_id, 0);
    EXPECT_GT (detection.score, 0.0f);
    EXPECT_TRUE (detection.scale == 1.0f || detection.scale == 2.0f);
    saw_scale_one = saw_scale_one || detection.scale == 1.0f;
    saw_scale_two = saw_scale_two || detection.scale == 2.0f;
  }
  EXPECT_TRUE (saw_scale_one);
  EXPECT_TRUE (saw_scale_two);
  EXPECT_NE (pcl::test::linemod_rvv::checksumDetections (detections), 0u);
}
