#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::test::linemod_rvv
{

struct ScoreSummary
{
  std::uint16_t max_value{0};
  std::size_t max_index{0};
  std::size_t count_above_threshold{0};
};

inline void
makeEnergyMaskSet (const std::size_t bin_index, std::uint8_t masks[4])
{
  const auto base_bit = static_cast<std::uint8_t> (0x1);
  masks[0] = static_cast<std::uint8_t> (base_bit << bin_index);
  masks[1] = static_cast<std::uint8_t> (
      masks[0] | (base_bit << ((bin_index + 1) % 8)) | (base_bit << ((bin_index + 7) % 8)));
  masks[2] = static_cast<std::uint8_t> (
      masks[1] | (base_bit << ((bin_index + 2) % 8)) | (base_bit << ((bin_index + 6) % 8)));
  masks[3] = static_cast<std::uint8_t> (
      masks[2] | (base_bit << ((bin_index + 3) % 8)) | (base_bit << ((bin_index + 5) % 8)));
}

// 标量参考链路复刻 `detectTemplates` 默认分支的 energy map generation
// （能量图生成）语义。输出按 bin-major（先 bin 后像素）布局保存，每个元素是
// `val0..val3` 四个 mask 命中的次数，因此取值范围固定为 0..4。
inline void
buildEnergyMapsStd (const std::uint8_t* quantized_data,
                    const std::size_t map_size,
                    const std::size_t nr_bins,
                    std::uint8_t* energy_maps)
{
  for (std::size_t bin_index = 0; bin_index < nr_bins; ++bin_index)
  {
    std::uint8_t masks[4];
    makeEnergyMaskSet (bin_index, masks);
    std::uint8_t* energy_map = energy_maps + bin_index * map_size;
    for (std::size_t index = 0; index < map_size; ++index)
    {
      const std::uint8_t quantized = quantized_data[index];
      std::uint8_t energy = 0;
      energy = static_cast<std::uint8_t> (energy + ((masks[0] & quantized) != 0));
      energy = static_cast<std::uint8_t> (energy + ((masks[1] & quantized) != 0));
      energy = static_cast<std::uint8_t> (energy + ((masks[2] & quantized) != 0));
      energy = static_cast<std::uint8_t> (energy + ((masks[3] & quantized) != 0));
      energy_map[index] = energy;
    }
  }
}

// Phase 030 的 RVV 候选只覆盖默认合并 energy map。每个 VL chunk（可变向量长度分块）
// 先加载一段 quantized byte，再对四个 mask 分别做命中判断；masked add（带掩码加法）
// 让命中的向量通道加 1，未命中的通道保持原值。
inline void
buildEnergyMapsRVV (const std::uint8_t* quantized_data,
                    const std::size_t map_size,
                    const std::size_t nr_bins,
                    std::uint8_t* energy_maps)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  for (std::size_t bin_index = 0; bin_index < nr_bins; ++bin_index)
  {
    std::uint8_t masks[4];
    makeEnergyMaskSet (bin_index, masks);
    std::uint8_t* energy_map = energy_maps + bin_index * map_size;
    for (std::size_t index = 0; index < map_size;)
    {
      const std::size_t vl = __riscv_vsetvl_e8m1 (map_size - index);
      const vuint8m1_t quantized = __riscv_vle8_v_u8m1 (quantized_data + index, vl);
      vuint8m1_t energy = __riscv_vmv_v_x_u8m1 (0, vl);
      for (const std::uint8_t mask_value : masks)
      {
        const vuint8m1_t masked = __riscv_vand_vx_u8m1 (quantized, mask_value, vl);
        const vbool8_t hit = __riscv_vmsne_vx_u8m1_b8 (masked, 0, vl);
        const vuint8m1_t increment = __riscv_vmerge_vxm_u8m1 (
            __riscv_vmv_v_x_u8m1 (0, vl), 1, hit, vl);
        energy = __riscv_vadd_vv_u8m1 (energy, increment, vl);
      }
      __riscv_vse8_v_u8m1 (energy_map + index, energy, vl);
      index += vl;
    }
  }
#else
  buildEnergyMapsStd (quantized_data, map_size, nr_bins, energy_maps);
#endif
}

// 标量参考链路复刻 `EnergyMaps -> LinearizedMaps` 的 8x8 offset copy
// （偏移拷贝）语义。输出由 64 张连续小图拼接而成，第
// `map_row * step_size + map_col` 张小图对应生产里的 `maps(map_col,map_row)`。
inline void
linearizeEnergyMapStd (const std::uint8_t* energy_map,
                       const std::size_t width,
                       const std::size_t height,
                       const std::size_t step_size,
                       std::uint8_t* linearized_maps)
{
  const std::size_t lin_width = width / step_size;
  const std::size_t lin_height = height / step_size;
  const std::size_t linearized_map_size = lin_width * lin_height;
  for (std::size_t map_row = 0; map_row < step_size; ++map_row)
  {
    for (std::size_t map_col = 0; map_col < step_size; ++map_col)
    {
      std::uint8_t* linearized_map =
          linearized_maps + (map_row * step_size + map_col) * linearized_map_size;
      for (std::size_t row_index = 0; row_index < lin_height; ++row_index)
      {
        const std::size_t source_row = row_index * step_size + map_row;
        for (std::size_t col_index = 0; col_index < lin_width; ++col_index)
        {
          const std::size_t source_col = col_index * step_size + map_col;
          linearized_map[row_index * lin_width + col_index] =
              energy_map[source_row * width + source_col];
        }
      }
    }
  }
}

// Phase 040 的 RVV 候选只替代上面的规则拷贝核。每个 `(map_col,map_row)`
// 的每一行都从源 energy map 按 `step_size` 字节跨步加载，再连续写入对应
// linearized row；对象分配、`LinearizedMaps::getOffsetMap` 和后续打分不在本
// helper 的证据边界内。
inline void
linearizeEnergyMapRVV (const std::uint8_t* energy_map,
                       const std::size_t width,
                       const std::size_t height,
                       const std::size_t step_size,
                       std::uint8_t* linearized_maps)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  const std::size_t lin_width = width / step_size;
  const std::size_t lin_height = height / step_size;
  const std::size_t linearized_map_size = lin_width * lin_height;
  const auto stride_bytes = static_cast<std::ptrdiff_t> (step_size);
  for (std::size_t map_row = 0; map_row < step_size; ++map_row)
  {
    for (std::size_t map_col = 0; map_col < step_size; ++map_col)
    {
      std::uint8_t* linearized_map =
          linearized_maps + (map_row * step_size + map_col) * linearized_map_size;
      for (std::size_t row_index = 0; row_index < lin_height; ++row_index)
      {
        const std::uint8_t* source_row =
            energy_map + (row_index * step_size + map_row) * width + map_col;
        std::uint8_t* target_row = linearized_map + row_index * lin_width;
        for (std::size_t col_index = 0; col_index < lin_width;)
        {
          const std::size_t vl = __riscv_vsetvl_e8m1 (lin_width - col_index);
          const vuint8m1_t values =
              __riscv_vlse8_v_u8m1 (source_row + col_index * step_size, stride_bytes, vl);
          __riscv_vse8_v_u8m1 (target_row + col_index, values, vl);
          col_index += vl;
        }
      }
    }
  }
#else
  linearizeEnergyMapStd (energy_map, width, height, step_size, linearized_maps);
#endif
}

// Phase 050 的 full-chain split 需要把 8 个 bin-major energy maps 一次性
// 线性化。这个 wrapper 只固定 bin 拼接顺序；单张 map 的 offset copy 语义仍由
// `linearizeEnergyMapStd` 覆盖，便于 reviewer 把多 bin layout 和单 bin kernel 分开审。
inline void
linearizeEnergyMapsStd (const std::uint8_t* energy_maps,
                        const std::size_t width,
                        const std::size_t height,
                        const std::size_t nr_bins,
                        const std::size_t step_size,
                        std::uint8_t* linearized_maps)
{
  const std::size_t energy_map_size = width * height;
  const std::size_t lin_width = width / step_size;
  const std::size_t lin_height = height / step_size;
  const std::size_t linearized_per_bin = step_size * step_size * lin_width * lin_height;
  for (std::size_t bin_index = 0; bin_index < nr_bins; ++bin_index)
  {
    linearizeEnergyMapStd (energy_maps + bin_index * energy_map_size,
                           width,
                           height,
                           step_size,
                           linearized_maps + bin_index * linearized_per_bin);
  }
}

// RVV 侧同样只改变每张 energy map 的规则拷贝核，不改变 bin-major 拼接顺序。
// 非 RVV 构建自然回到标量 wrapper，使同一测试源能同时验证 Std/RVV 二进制。
inline void
linearizeEnergyMapsRVV (const std::uint8_t* energy_maps,
                        const std::size_t width,
                        const std::size_t height,
                        const std::size_t nr_bins,
                        const std::size_t step_size,
                        std::uint8_t* linearized_maps)
{
  const std::size_t energy_map_size = width * height;
  const std::size_t lin_width = width / step_size;
  const std::size_t lin_height = height / step_size;
  const std::size_t linearized_per_bin = step_size * step_size * lin_width * lin_height;
  for (std::size_t bin_index = 0; bin_index < nr_bins; ++bin_index)
  {
    linearizeEnergyMapRVV (energy_maps + bin_index * energy_map_size,
                           width,
                           height,
                           step_size,
                           linearized_maps + bin_index * linearized_per_bin);
  }
}

// 标量参考链路复刻 `linemod.cpp` 中 score_sums[mem_index] += data[mem_index]
// 的核心语义。调用者直接传入已经由 LinearizedMaps::getOffsetMap 形态抽象出的
// 连续 score maps，因此本 helper 只证明累加核，不证明 production dispatch。
inline void
accumulateScoreMapsStd (const std::uint8_t* const* maps,
                        const std::size_t nr_maps,
                        const std::size_t mem_size,
                        std::uint16_t* score_sums)
{
  std::fill_n (score_sums, mem_size, static_cast<std::uint16_t> (0));
  for (std::size_t map_index = 0; map_index < nr_maps; ++map_index)
  {
    const std::uint8_t* data = maps[map_index];
    for (std::size_t mem_index = 0; mem_index < mem_size; ++mem_index)
      score_sums[mem_index] = static_cast<std::uint16_t> (score_sums[mem_index] + data[mem_index]);
  }
}

// RVV（RISC-V Vector，可变长度向量扩展）候选只替代上面的逐元素累加。
// `u8mf2 -> u16m1` 的 zero-extension（零扩展）让每个向量通道和标量
// `unsigned char` 到 `unsigned short` 的提升一致；尾段由 vsetvl 处理。
inline void
accumulateScoreMapsRVV (const std::uint8_t* const* maps,
                       const std::size_t nr_maps,
                       const std::size_t mem_size,
                       std::uint16_t* score_sums)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  std::fill_n (score_sums, mem_size, static_cast<std::uint16_t> (0));
  for (std::size_t map_index = 0; map_index < nr_maps; ++map_index)
  {
    const std::uint8_t* data = maps[map_index];
    for (std::size_t mem_index = 0; mem_index < mem_size;)
    {
      const std::size_t vl = __riscv_vsetvl_e16m1 (mem_size - mem_index);
      const vuint16m1_t sums = __riscv_vle16_v_u16m1 (score_sums + mem_index, vl);
      const vuint8mf2_t bytes = __riscv_vle8_v_u8mf2 (data + mem_index, vl);
      const vuint16m1_t widened = __riscv_vzext_vf2_u16m1 (bytes, vl);
      __riscv_vse16_v_u16m1 (score_sums + mem_index,
                             __riscv_vadd_vv_u16m1 (sums, widened, vl),
                             vl);
      mem_index += vl;
    }
  }
#else
  accumulateScoreMapsStd (maps, nr_maps, mem_size, score_sums);
#endif
}

// `detectTemplates` 的最大值扫描使用严格大于，所以并列最大值必须保留
// 最早 mem_index。这里先用标量 summary 固定输出语义；threshold scan 的 RVV
// 候选放到后续 phase。
inline ScoreSummary
summarizeScores (const std::uint16_t* score_sums,
                 const std::size_t mem_size,
                 const std::uint16_t raw_threshold)
{
  ScoreSummary summary;
  for (std::size_t mem_index = 0; mem_index < mem_size; ++mem_index)
  {
    const std::uint16_t score = score_sums[mem_index];
    if (score > summary.max_value)
    {
      summary.max_value = score;
      summary.max_index = mem_index;
    }
    if (score > raw_threshold)
      ++summary.count_above_threshold;
  }
  return summary;
}

// Phase 010 的标量 reference 复刻 `detectTemplates` 在 NMS 之前的线性扫描：
// threshold 使用 strict `>`，候选 index 按 mem_index 保序写入；max 扫描也使用
// strict `>`，因此并列最大值保留最早位置。
inline ScoreSummary
scanScoresStd (const std::uint16_t* score_sums,
               const std::size_t mem_size,
               const std::uint16_t raw_threshold,
               std::vector<std::size_t>& candidate_indices)
{
  candidate_indices.clear ();
  ScoreSummary summary;
  for (std::size_t mem_index = 0; mem_index < mem_size; ++mem_index)
  {
    const std::uint16_t score = score_sums[mem_index];
    if (score > summary.max_value)
    {
      summary.max_value = score;
      summary.max_index = mem_index;
    }
    if (score > raw_threshold)
    {
      ++summary.count_above_threshold;
      candidate_indices.push_back (mem_index);
    }
  }
  return summary;
}

// RVV 候选先只向量化 threshold count（阈值计数），max / index append 保留标量。
// 这样能隔离 compare + mask + vcpop 的收益，同时避免过早引入 vcompress buffer
// 对 detection 输出顺序造成额外风险。
inline ScoreSummary
scanScoresRVV (const std::uint16_t* score_sums,
               const std::size_t mem_size,
               const std::uint16_t raw_threshold,
               std::vector<std::size_t>& candidate_indices)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  candidate_indices.clear ();
  ScoreSummary summary;
  for (std::size_t mem_index = 0; mem_index < mem_size;)
  {
    const std::size_t vl = __riscv_vsetvl_e16m1 (mem_size - mem_index);
    const vuint16m1_t scores = __riscv_vle16_v_u16m1 (score_sums + mem_index, vl);
    const vbool16_t above = __riscv_vmsgtu_vx_u16m1_b16 (scores, raw_threshold, vl);
    summary.count_above_threshold += __riscv_vcpop_m_b16 (above, vl);
    mem_index += vl;
  }
  for (std::size_t mem_index = 0; mem_index < mem_size; ++mem_index)
  {
    const std::uint16_t score = score_sums[mem_index];
    if (score > summary.max_value)
    {
      summary.max_value = score;
      summary.max_index = mem_index;
    }
    if (score > raw_threshold)
      candidate_indices.push_back (mem_index);
  }
  return summary;
#else
  return scanScoresStd (score_sums, mem_size, raw_threshold, candidate_indices);
#endif
}

} // namespace pcl::test::linemod_rvv
