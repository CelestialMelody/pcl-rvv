#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "dotmod_template_matching.h"

namespace
{

std::vector<std::uint8_t>
makeImageMap (const std::size_t width, const std::size_t height)
{
  std::vector<std::uint8_t> values (width * height);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
      values[row * width + col] =
          static_cast<std::uint8_t> (((row * 17 + col * 29 + (row ^ col)) & 0x7f) | 0x01);
  }
  return values;
}

std::vector<std::uint8_t>
makeTemplateMap (const std::size_t width, const std::size_t height)
{
  std::vector<std::uint8_t> values (width * height);
  for (std::size_t index = 0; index < values.size (); ++index)
    values[index] = static_cast<std::uint8_t> (1u << (index % 7));
  return values;
}

std::vector<std::vector<std::uint8_t>>
makeModalityMaps (const std::size_t width, const std::size_t height, const std::size_t nr_modalities)
{
  std::vector<std::vector<std::uint8_t>> maps;
  maps.reserve (nr_modalities);
  for (std::size_t modality = 0; modality < nr_modalities; ++modality)
  {
    auto map = makeImageMap (width, height);
    for (std::size_t index = 0; index < map.size (); ++index)
      map[index] = static_cast<std::uint8_t> ((map[index] << (modality % 3)) | (1u << (index % 8)));
    maps.push_back (std::move (map));
  }
  return maps;
}

std::vector<std::vector<std::vector<std::uint8_t>>>
makeTemplateBank (const std::size_t window_width,
                  const std::size_t window_height,
                  const std::size_t nr_templates,
                  const std::size_t nr_modalities)
{
  std::vector<std::vector<std::vector<std::uint8_t>>> templates;
  templates.reserve (nr_templates);
  for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
  {
    std::vector<std::vector<std::uint8_t>> modalities;
    modalities.reserve (nr_modalities);
    for (std::size_t modality = 0; modality < nr_modalities; ++modality)
    {
      auto templ = makeTemplateMap (window_width, window_height);
      for (std::size_t index = 0; index < templ.size (); ++index)
      {
        const auto shift = static_cast<unsigned> ((index + template_index + modality) % 8);
        templ[index] = static_cast<std::uint8_t> (1u << shift);
      }
      modalities.push_back (std::move (templ));
    }
    templates.push_back (std::move (modalities));
  }
  return templates;
}

} // namespace

TEST (DOTMODTemplateMatching, DirectWindowScoreMatchesSubMapBaselineWithTail)
{
  // Phase 000 固定 DOTMOD 的核心 score 语义：原 production 先为每个滑窗位置
  // 构造 `QuantizedMap::getSubMap()`，再按 row-major（按行连续）顺序检查
  // `image_data & template_data` 是否非零。这里的 direct window（直接窗口读取）
  // 不构造 submap，但必须得到同一计数，并覆盖非典型窗口宽度带来的 VL tail。
  const std::size_t image_width = 37;
  const std::size_t image_height = 19;
  const std::size_t window_width = 11;
  const std::size_t window_height = 7;
  const std::size_t window_x = 13;
  const std::size_t window_y = 5;
  const auto image = makeImageMap (image_width, image_height);
  const auto templ = makeTemplateMap (window_width, window_height);

  const auto expected = pcl::test::dotmod_rvv::scoreWindowViaSubMapStd (
      image.data (), image_width, window_x, window_y, window_width, window_height, templ.data ());
  const auto actual = pcl::test::dotmod_rvv::scoreWindowDirectRVV (
      image.data (), image_width, window_x, window_y, window_width, window_height, templ.data ());

  EXPECT_EQ (actual, expected);
}

TEST (DOTMODTemplateMatching, FullDetectTemplatesShapePreservesDetectionOrderAndScores)
{
  // Phase 010 把 Phase 000 的 direct window（直接窗口读取）计数放回完整
  // `detectTemplates()` 形态。这个测试固定 row/col/template 的输出顺序、
  // 多 modality（多模态）累加和 threshold（阈值）后的 score，避免后续
  // 只验证单窗口 helper 而漏掉真实检测链路的容器语义。
  const std::size_t image_width = 19;
  const std::size_t image_height = 13;
  const std::size_t window_width = 7;
  const std::size_t window_height = 5;
  const std::size_t nr_templates = 3;
  const std::size_t nr_modalities = 2;
  const float threshold = 0.90f;
  const auto maps = makeModalityMaps (image_width, image_height, nr_modalities);
  const auto templates = makeTemplateBank (window_width, window_height, nr_templates, nr_modalities);

  const auto expected = pcl::test::dotmod_rvv::detectTemplatesViaSubMapStd (
      maps, image_width, window_width, window_height, templates, threshold);
  const auto actual = pcl::test::dotmod_rvv::detectTemplatesDirectRVV (
      maps, image_width, window_width, window_height, templates, threshold);

  ASSERT_EQ (actual.size (), expected.size ());
  ASSERT_GT (actual.size (), 0u);
  for (std::size_t index = 0; index < expected.size (); ++index)
  {
    EXPECT_EQ (actual[index].bin_x, expected[index].bin_x);
    EXPECT_EQ (actual[index].bin_y, expected[index].bin_y);
    EXPECT_EQ (actual[index].template_id, expected[index].template_id);
    EXPECT_FLOAT_EQ (actual[index].score, expected[index].score);
  }
}

TEST (DOTMODTemplateMatching, FullDetectTemplatesShapeUsesStrictThreshold)
{
  // Production 使用严格大于号：response 等于 threshold 时不能输出 detection。
  // 这里构造一个全命中窗口，让 score 恰好为 1.0，锁住 `>` 而不是 `>=`。
  const std::size_t image_width = 6;
  const std::size_t image_height = 5;
  const std::size_t window_width = 3;
  const std::size_t window_height = 2;
  const std::vector<std::vector<std::uint8_t>> maps (1, std::vector<std::uint8_t> (image_width * image_height, 0xff));
  const std::vector<std::vector<std::vector<std::uint8_t>>> templates (
      1, std::vector<std::vector<std::uint8_t>> (1, std::vector<std::uint8_t> (window_width * window_height, 0x01)));

  const auto detections = pcl::test::dotmod_rvv::detectTemplatesDirectRVV (
      maps, image_width, window_width, window_height, templates, 1.0f);

  EXPECT_TRUE (detections.empty ());
}

TEST (DOTMODTemplateMatching, DirectWindowScorePreservesZeroMaskAndOffset)
{
  // 这个 case 把空模板 byte、空 image byte 和非零 bit 交错放在窗口里，
  // 用来证明候选路径没有把“任意非零 byte”误写成命中，而是保持
  // production 的 bitwise AND（按位与）语义。
  const std::size_t image_width = 16;
  const std::size_t image_height = 10;
  const std::size_t window_width = 8;
  const std::size_t window_height = 5;
  const std::size_t window_x = 3;
  const std::size_t window_y = 2;
  auto image = makeImageMap (image_width, image_height);
  auto templ = makeTemplateMap (window_width, window_height);
  image[(window_y + 1) * image_width + window_x + 2] = 0;
  templ[1 * window_width + 2] = 0xff;
  templ[3 * window_width + 4] = 0;

  const auto expected = pcl::test::dotmod_rvv::scoreWindowViaSubMapStd (
      image.data (), image_width, window_x, window_y, window_width, window_height, templ.data ());
  const auto actual = pcl::test::dotmod_rvv::scoreWindowDirectRVV (
      image.data (), image_width, window_x, window_y, window_width, window_height, templ.data ());

  EXPECT_EQ (actual, expected);
}
