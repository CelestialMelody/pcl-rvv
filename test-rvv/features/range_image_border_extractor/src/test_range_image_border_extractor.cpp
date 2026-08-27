/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 RangeImageBorderExtractor 的
 * score-update（分数传播）测试专用 RVV candidate 是否和标量参考链路一致。
 * score-update 只覆盖连续 float 分数图像，不接触 LocalSurface 指针数组、
 * shadow border（阴影边界）状态机或 production dispatch（生产分流）。
 */

#include "range_image_border_extractor.h"
#include "impl/range_image_border_extractor_range_fixture.hpp"

#include <pcl/test/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace ribe = pcl::features::rvv_test::range_image_border_extractor;

namespace
{
std::vector<float>
makeScoreImage(const std::size_t width, const std::size_t height)
{
  std::vector<float> scores(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const float base = static_cast<float>((x * 17 + y * 11) % 23) / 24.0f;
      scores[y * width + x] = ((x + 2 * y) % 5 == 0) ? -0.35f * base : base;
    }
  }
  scores[2 * width + 3] = 0.86f;
  scores[2 * width + 4] = 0.77f;
  scores[3 * width + 3] = -0.42f;
  scores[4 * width + 5] = 0.04f;
  return scores;
}

void
expectScoresNear(const std::vector<float>& actual, const std::vector<float>& expected)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], 1.0e-6f) << "index=" << i;
}
} // namespace

TEST(RangeImageBorderExtractorScoreUpdate, RvvMatchesScalarForInteriorBoundaryAndSignGates)
{
  /*
   * 这个测试会抓三类真实风险：内部像素固定 8 邻域公式写错、图像边缘的
   * weight_sum（权重和）处理错、以及邻域平均值和中心分数符号相反时没有保留原值。
   */
  const std::size_t width = 17;
  const std::size_t height = 9;
  const float minimum_border_probability = 0.8f;
  const std::vector<float> scores = makeScoreImage(width, height);

  const std::vector<float> expected = ribe::updateScoresStd(scores, width, height, minimum_border_probability);
  const std::vector<float> actual = ribe::updateScoresRVV(scores, width, height, minimum_border_probability);

  expectScoresNear(actual, expected);
}

TEST(RangeImageBorderExtractorScoreUpdate, RvvMatchesScalarForFourDirectionScoreImages)
{
  /*
   * Production 中 score-update 会连续作用在 left/right/top/bottom 四张分数图。
   * 这个测试只验证四图打包路径没有错连方向或遗漏某一张图，不证明分数图如何由 RangeImage 生成。
   */
  const std::size_t width = 19;
  const std::size_t height = 11;
  const float minimum_border_probability = 0.72f;
  ribe::ScoreImageSet scores;
  scores.left = makeScoreImage(width, height);
  scores.right = makeScoreImage(width, height);
  scores.top = makeScoreImage(width, height);
  scores.bottom = makeScoreImage(width, height);
  for (std::size_t i = 0; i < scores.left.size(); ++i)
  {
    scores.right[i] = scores.right[i] * 0.75f - 0.10f;
    scores.top[i] = -scores.top[i] * 0.45f;
    scores.bottom[i] = scores.bottom[i] * 1.10f;
  }

  const ribe::ScoreImageSet expected =
      ribe::updateScoreImageSetStd(scores, width, height, minimum_border_probability);
  const ribe::ScoreImageSet actual =
      ribe::updateScoreImageSetRVV(scores, width, height, minimum_border_probability);

  expectScoresNear(actual.left, expected.left);
  expectScoresNear(actual.right, expected.right);
  expectScoresNear(actual.top, expected.top);
  expectScoresNear(actual.bottom, expected.bottom);
}

TEST(RangeImageBorderExtractorScoreGeneration, RangeImageFixtureProducesStableFourDirectionScores)
{
  /*
   * Phase 020 开始触碰真实 RangeImageBorderExtractor 的 score generation
   * 边界。这个测试不比较 RVV 加速；它证明夹具能稳定触发 production
   * extractBorderScoreImages()，避免后续 bench 只在 synthetic score image 上转圈。
   */
  const pcl::RangeImage range_image = ribe::makeRangeImageFixture(96, 72);
  const float minimum_border_probability = 0.80f;

  const ribe::ScoreImageSet scores_a =
      ribe::extractProductionScoreImages(range_image, minimum_border_probability);
  const ribe::ScoreImageSet scores_b =
      ribe::extractProductionScoreImages(range_image, minimum_border_probability);

  const std::size_t expected_size =
      static_cast<std::size_t>(range_image.width) * static_cast<std::size_t>(range_image.height);
  ASSERT_EQ(scores_a.left.size(), expected_size);
  ASSERT_EQ(scores_a.right.size(), expected_size);
  ASSERT_EQ(scores_a.top.size(), expected_size);
  ASSERT_EQ(scores_a.bottom.size(), expected_size);
  EXPECT_GT(ribe::countMeaningfulScores(scores_a), 0u);
  EXPECT_NEAR(ribe::checksumScoreImageSet(scores_a), ribe::checksumScoreImageSet(scores_b), 1.0e-4);

  expectScoresNear(scores_a.left, scores_b.left);
  expectScoresNear(scores_a.right, scores_b.right);
  expectScoresNear(scores_a.top, scores_b.top);
  expectScoresNear(scores_a.bottom, scores_b.bottom);
}
