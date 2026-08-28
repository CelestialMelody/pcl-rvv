#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include "linemod_template_scoring.h"

namespace
{

std::vector<std::vector<std::uint8_t>>
makeScoreMaps (const std::size_t nr_maps, const std::size_t mem_size)
{
  std::vector<std::vector<std::uint8_t>> maps (nr_maps, std::vector<std::uint8_t> (mem_size));
  for (std::size_t map_index = 0; map_index < nr_maps; ++map_index)
  {
    for (std::size_t value_index = 0; value_index < mem_size; ++value_index)
    {
      maps[map_index][value_index] = static_cast<std::uint8_t> ((map_index * 3 + value_index * 5 + 1) % 5);
    }
  }
  return maps;
}

std::vector<const std::uint8_t*>
mapPointers (const std::vector<std::vector<std::uint8_t>>& maps)
{
  std::vector<const std::uint8_t*> pointers;
  pointers.reserve (maps.size ());
  for (const auto& map : maps)
    pointers.push_back (map.data ());
  return pointers;
}

std::uint64_t
checksum (const std::vector<std::uint16_t>& values)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto value : values)
  {
    result ^= static_cast<std::uint64_t> (value);
    result *= 1099511628211ull;
  }
  return result;
}

std::uint64_t
checksumBytes (const std::vector<std::uint8_t>& values)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto value : values)
  {
    result ^= static_cast<std::uint64_t> (value);
    result *= 1099511628211ull;
  }
  return result;
}

} // namespace

TEST (LINEMODTemplateScoring, ScoreAccumulationMatchesScalarReferenceWithTail)
{
  // 这个 case 刻意使用非 8、16 或典型 VLEN 整数倍的 mem_size，验证 tail（尾段）
  // 仍按 production 标量语义累加，而不是只验证整向量主体。
  const std::size_t mem_size = 257;
  const auto maps = makeScoreMaps (17, mem_size);
  const auto pointers = mapPointers (maps);

  std::vector<std::uint16_t> expected (mem_size, 0);
  std::vector<std::uint16_t> actual (mem_size, 0);

  pcl::test::linemod_rvv::accumulateScoreMapsStd (pointers.data (), pointers.size (), mem_size, expected.data ());
  pcl::test::linemod_rvv::accumulateScoreMapsRVV (pointers.data (), pointers.size (), mem_size, actual.data ());

  EXPECT_EQ (actual, expected);
  EXPECT_NE (checksum (actual), 0u);
}

TEST (LINEMODTemplateScoring, ScoreSummaryPreservesMaxIndexTieBreak)
{
  // `matchTemplates` 的 max scan 使用严格大于比较；分数相同时保留最早位置。
  // 这个测试把该 tie-break（并列分数处理）固定下来，避免后续 RVV 规约改变检测位置。
  const std::size_t mem_size = 65;
  auto maps = makeScoreMaps (9, mem_size);
  for (auto& map : maps)
  {
    map[7] = 4;
    map[23] = 4;
  }
  const auto pointers = mapPointers (maps);

  std::vector<std::uint16_t> scores (mem_size, 0);
  pcl::test::linemod_rvv::accumulateScoreMapsRVV (pointers.data (), pointers.size (), mem_size, scores.data ());
  const auto summary = pcl::test::linemod_rvv::summarizeScores (scores.data (), scores.size (), 12);

  EXPECT_EQ (summary.max_index, 7u);
  EXPECT_EQ (summary.max_value, scores[7]);
  EXPECT_GT (summary.count_above_threshold, 0u);
}

TEST (LINEMODTemplateScoring, ScoreScanKeepsThresholdStrictAndCandidateOrder)
{
  // Phase 010 的入口先固定 detection scan 的 caller-visible 语义：
  // threshold 使用 strict greater-than（严格大于），候选 index 按 mem_index 递增，
  // 最大值并列时仍保留最早位置。
  const std::vector<std::uint16_t> scores = {
      7, 13, 12, 40, 13, 1, 39, 40, 0, 13, 14, 2, 40, 3, 4, 41, 5};
  const std::uint16_t raw_threshold = 13;
  const std::vector<std::size_t> expected_candidates = {3, 6, 7, 10, 12, 15};

  std::vector<std::size_t> expected_indices;
  std::vector<std::size_t> actual_indices;
  const auto expected = pcl::test::linemod_rvv::scanScoresStd (
      scores.data (), scores.size (), raw_threshold, expected_indices);
  const auto actual = pcl::test::linemod_rvv::scanScoresRVV (
      scores.data (), scores.size (), raw_threshold, actual_indices);

  EXPECT_EQ (actual.max_value, expected.max_value);
  EXPECT_EQ (actual.max_index, expected.max_index);
  EXPECT_EQ (actual.count_above_threshold, expected.count_above_threshold);
  EXPECT_EQ (actual_indices, expected_indices);
  EXPECT_EQ (actual_indices, expected_candidates);
  EXPECT_EQ (actual.max_value, 41u);
  EXPECT_EQ (actual.max_index, 15u);
}

TEST (LINEMODTemplateScoring, EnergyMapGenerationMatchesDefaultDetectTemplatesSemantics)
{
  // Phase 030 固定 `detectTemplates` 默认 energy map generation（能量图生成）语义：
  // 每个 bin 对同一个 quantized byte 检查 4 个逐步扩大的邻域 mask，输出值是 0..4
  // 的命中次数。本 case 同时覆盖所有 bit pattern、空 bit、全 bit 和非整向量 tail。
  const std::vector<std::uint8_t> quantized = {
      0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
      0x03, 0x0c, 0x30, 0xc0, 0x81, 0xff, 0x55, 0xaa, 0x7e,
      0xbd, 0x42, 0x18, 0xe7, 0x99, 0x24, 0xdb, 0x66, 0x5a,
      0xa5, 0xf0, 0x0f, 0x11, 0x88, 0x44, 0x22, 0x77, 0xee,
      0x13, 0x31, 0x8c, 0xc8, 0x6d, 0xd6, 0x2b, 0xb2, 0xfe};
  const std::size_t nr_bins = 8;
  std::vector<std::uint8_t> expected (quantized.size () * nr_bins, 0);
  std::vector<std::uint8_t> actual (quantized.size () * nr_bins, 0);

  pcl::test::linemod_rvv::buildEnergyMapsStd (quantized.data (), quantized.size (), nr_bins, expected.data ());
  pcl::test::linemod_rvv::buildEnergyMapsRVV (quantized.data (), quantized.size (), nr_bins, actual.data ());

  EXPECT_EQ (actual, expected);
  EXPECT_EQ (actual[0], 0u);
  EXPECT_EQ (actual[14], 4u);
  EXPECT_EQ (checksumBytes (actual), checksumBytes (expected));
}

TEST (LINEMODTemplateScoring, LinearizedMapCopyMatchesDefaultDetectTemplatesLayout)
{
  // Phase 040 固定 `EnergyMaps -> LinearizedMaps` 的 8x8 offset copy（偏移拷贝）
  // 语义。源 energy map 是一张普通 row-major（按行连续）字节图；目标按
  // `LinearizedMaps::operator()(map_col,map_row)` 的顺序保存 64 张连续小图。
  // width 选择 264，让每个目标行有 33 个元素，能覆盖非整 VL tail。
  const std::size_t width = 264;
  const std::size_t height = 40;
  const std::size_t step_size = 8;
  const std::size_t lin_width = width / step_size;
  const std::size_t lin_height = height / step_size;
  std::vector<std::uint8_t> energy_map (width * height, 0);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
      energy_map[row * width + col] = static_cast<std::uint8_t> ((row * 17 + col * 5 + (row ^ col)) & 0xff);
  }

  std::vector<std::uint8_t> expected (step_size * step_size * lin_width * lin_height, 0);
  std::vector<std::uint8_t> actual (expected.size (), 0);

  pcl::test::linemod_rvv::linearizeEnergyMapStd (
      energy_map.data (), width, height, step_size, expected.data ());
  pcl::test::linemod_rvv::linearizeEnergyMapRVV (
      energy_map.data (), width, height, step_size, actual.data ());

  EXPECT_EQ (actual, expected);
  const std::size_t map_offset = (3 * step_size + 5) * lin_width * lin_height;
  EXPECT_EQ (actual[map_offset + 2 * lin_width + 7],
             energy_map[(2 * step_size + 3) * width + (7 * step_size + 5)]);
  EXPECT_EQ (checksumBytes (actual), checksumBytes (expected));
}

TEST (LINEMODTemplateScoring, LinearizedMapsCopyMatchesAllBinsLayout)
{
  // Phase 050 的 full-chain split（完整链路分阶段计时）需要一次性线性化
  // 8 个 energy maps。本测试固定 `bin -> map_row -> map_col -> row -> col`
  // 的拼接顺序，避免 bench helper 只验证了单 bin 而在多 bin layout 上错位。
  const std::size_t nr_bins = 8;
  const std::size_t width = 136;
  const std::size_t height = 24;
  const std::size_t step_size = 8;
  const std::size_t lin_width = width / step_size;
  const std::size_t lin_height = height / step_size;
  const std::size_t energy_map_size = width * height;
  const std::size_t linearized_per_bin = step_size * step_size * lin_width * lin_height;
  std::vector<std::uint8_t> energy_maps (nr_bins * energy_map_size, 0);
  for (std::size_t bin = 0; bin < nr_bins; ++bin)
  {
    for (std::size_t index = 0; index < energy_map_size; ++index)
      energy_maps[bin * energy_map_size + index] =
          static_cast<std::uint8_t> ((bin * 29 + index * 7 + (index >> 2)) & 0xff);
  }

  std::vector<std::uint8_t> expected (nr_bins * linearized_per_bin, 0);
  std::vector<std::uint8_t> actual (expected.size (), 0);

  pcl::test::linemod_rvv::linearizeEnergyMapsStd (
      energy_maps.data (), width, height, nr_bins, step_size, expected.data ());
  pcl::test::linemod_rvv::linearizeEnergyMapsRVV (
      energy_maps.data (), width, height, nr_bins, step_size, actual.data ());

  EXPECT_EQ (actual, expected);
  const std::size_t bin = 6;
  const std::size_t map_row = 1;
  const std::size_t map_col = 7;
  const std::size_t row_index = 2;
  const std::size_t col_index = 13;
  const std::size_t linearized_offset =
      bin * linearized_per_bin
      + (map_row * step_size + map_col) * lin_width * lin_height
      + row_index * lin_width + col_index;
  const std::size_t energy_offset =
      bin * energy_map_size + (row_index * step_size + map_row) * width + (col_index * step_size + map_col);
  EXPECT_EQ (actual[linearized_offset], energy_maps[energy_offset]);
  EXPECT_EQ (checksumBytes (actual), checksumBytes (expected));
}
