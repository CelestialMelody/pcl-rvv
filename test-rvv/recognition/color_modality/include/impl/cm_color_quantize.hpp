#pragma once

/*
 * 本文件做什么：
 * 这里放 ColorModality 首阶段的 scalar reference（标量参考链路）和 RVV
 * candidate（RVV 候选链路）。标量参考复刻 production 的 quantizeColors()、
 * filterQuantizedColors() 和固定 spreading_size=8 的 spreadQuantizedMap()。
 *
 * 证据边界：
 * 当前 candidate 同时服务 production-shaped diagnostic（生产形态诊断）和
 * production direct（真实生产路径）对拍，用来证明 RGB extrema 分类与 3x3 filter 子链路。
 * computeDistanceMap() 与 extractFeatures() 有顺序依赖和 list/sort 状态，本阶段
 * 明确不覆盖。
 */

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::recognition::rvv_test::color_modality
{
enum class ExecutionPath
{
  ScalarFallback,
  RvvQuantizeFilter
};

struct ColorArtifacts
{
  std::vector<std::uint8_t> quantized;
  std::vector<std::uint8_t> filtered;
  std::vector<std::uint8_t> spreaded;
};

inline std::uint8_t
quantizeColorOnRGBExtremaScalar(const std::uint8_t r_value,
                                const std::uint8_t g_value,
                                const std::uint8_t b_value)
{
  const float r = static_cast<float>(r_value);
  const float g = static_cast<float>(g_value);
  const float b = static_cast<float>(b_value);
  const float r_inv = 255.0f - r;
  const float g_inv = 255.0f - g;
  const float b_inv = 255.0f - b;

  const float dist_0 = (r * r + g * g + b * b) * 2.0f;
  const float dist_1 = r * r + g * g + b_inv * b_inv;
  const float dist_2 = r * r + g_inv * g_inv + b * b;
  const float dist_3 = r * r + g_inv * g_inv + b_inv * b_inv;
  const float dist_4 = r_inv * r_inv + g * g + b * b;
  const float dist_5 = r_inv * r_inv + g * g + b_inv * b_inv;
  const float dist_6 = r_inv * r_inv + g_inv * g_inv + b * b;
  const float dist_7 = (r_inv * r_inv + g_inv * g_inv + b_inv * b_inv) * 1.5f;

  const float min_dist = std::min(std::min(std::min(dist_0, dist_1), std::min(dist_2, dist_3)),
                                  std::min(std::min(dist_4, dist_5), std::min(dist_6, dist_7)));
  if (min_dist == dist_0)
    return 0;
  if (min_dist == dist_1)
    return 1;
  if (min_dist == dist_2)
    return 2;
  if (min_dist == dist_3)
    return 3;
  if (min_dist == dist_4)
    return 4;
  if (min_dist == dist_5)
    return 5;
  if (min_dist == dist_6)
    return 6;
  return 7;
}

inline void
quantizeColorsScalar(const pcl::PointXYZRGB* input,
                     const std::size_t width,
                     const std::size_t height,
                     std::vector<std::uint8_t>& output)
{
  output.assign(width * height, 0);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const auto& point = input[y * width + x];
      output[y * width + x] = quantizeColorOnRGBExtremaScalar(point.r, point.g, point.b);
    }
  }
}

inline void
filterQuantizedColorsScalar(const std::uint8_t* input,
                            const std::size_t width,
                            const std::size_t height,
                            std::vector<std::uint8_t>& output)
{
  output.assign(width * height, 0);
  if (width < 3 || height < 3)
    return;

  for (std::size_t y = 1; y + 1 < height; ++y)
  {
    for (std::size_t x = 1; x + 1 < width; ++x)
    {
      std::uint8_t histogram[8] = {};
      for (std::size_t dy = y - 1; dy <= y + 1; ++dy)
        for (std::size_t dx = x - 1; dx <= x + 1; ++dx)
          ++histogram[input[dy * width + dx]];

      std::uint8_t max_hist_value = 0;
      int max_hist_index = -1;
      for (int bin = 0; bin < 8; ++bin)
      {
        if (max_hist_value < histogram[bin])
        {
          max_hist_index = bin;
          max_hist_value = histogram[bin];
        }
      }
      output[y * width + x] = static_cast<std::uint8_t>(1u << max_hist_index);
    }
  }
}

inline void
spreadQuantizedMapScalar(const std::uint8_t* input,
                         const std::size_t width,
                         const std::size_t height,
                         const std::size_t spreading_size,
                         std::vector<std::uint8_t>& output)
{
  const std::size_t half_spreading_size = spreading_size / 2;
  std::vector<std::uint8_t> tmp(width * height, 0);
  output.assign(width * height, 0);
  if (height <= spreading_size + 1 || width <= spreading_size + 1)
    return;

  for (std::size_t y = 0; y < height - spreading_size - 1; ++y)
  {
    for (std::size_t x = 0; x < width - spreading_size - 1; ++x)
    {
      std::uint8_t value = 0;
      const std::uint8_t* ptr = input + y * width + x;
      for (std::size_t i = 0; i < spreading_size; ++i, ++ptr)
        value |= *ptr;
      tmp[y * width + x + half_spreading_size] = value;
    }
  }

  for (std::size_t y = 0; y < height - spreading_size - 1; ++y)
  {
    for (std::size_t x = 0; x < width - spreading_size - 1; ++x)
    {
      std::uint8_t value = 0;
      const std::uint8_t* ptr = tmp.data() + y * width + x;
      for (std::size_t i = 0; i < spreading_size; ++i, ptr += width)
        value |= *ptr;
      output[(y + half_spreading_size) * width + x] = value;
    }
  }
}

inline void
computeColorArtifactsScalar(const pcl::PointXYZRGB* input,
                            const std::size_t width,
                            const std::size_t height,
                            ColorArtifacts& output)
{
  quantizeColorsScalar(input, width, height, output.quantized);
  filterQuantizedColorsScalar(output.quantized.data(), width, height, output.filtered);
  spreadQuantizedMapScalar(output.filtered.data(), width, height, 8, output.spreaded);
}

inline ExecutionPath
computeColorArtifactsCandidate(const pcl::PointXYZRGB* input,
                               const std::size_t width,
                               const std::size_t height,
                               ColorArtifacts& output)
{
#if defined(__RVV10__)
  quantizeColorsScalar(input, width, height, output.quantized);
  output.filtered.assign(width * height, 0);
  if (width >= 3 && height >= 3)
  {
    for (std::size_t y = 1; y + 1 < height; ++y)
    {
      const std::size_t count = width - 2;
      for (std::size_t offset = 0; offset < count;)
      {
        const std::size_t vl = __riscv_vsetvl_e8m2(count - offset);
        const std::size_t x = 1 + offset;
        const std::uint8_t* prev = output.quantized.data() + (y - 1) * width + x - 1;
        const std::uint8_t* curr = output.quantized.data() + y * width + x - 1;
        const std::uint8_t* next = output.quantized.data() + (y + 1) * width + x - 1;

        const vuint8m2_t p0 = __riscv_vle8_v_u8m2(prev, vl);
        const vuint8m2_t p1 = __riscv_vle8_v_u8m2(prev + 1, vl);
        const vuint8m2_t p2 = __riscv_vle8_v_u8m2(prev + 2, vl);
        const vuint8m2_t c0 = __riscv_vle8_v_u8m2(curr, vl);
        const vuint8m2_t c1 = __riscv_vle8_v_u8m2(curr + 1, vl);
        const vuint8m2_t c2 = __riscv_vle8_v_u8m2(curr + 2, vl);
        const vuint8m2_t n0 = __riscv_vle8_v_u8m2(next, vl);
        const vuint8m2_t n1 = __riscv_vle8_v_u8m2(next + 1, vl);
        const vuint8m2_t n2 = __riscv_vle8_v_u8m2(next + 2, vl);

        vuint8m2_t max_count = __riscv_vmv_v_x_u8m2(0, vl);
        vuint8m2_t output_bits = __riscv_vmv_v_x_u8m2(0, vl);
        for (int bin = 0; bin < 8; ++bin)
        {
          vuint8m2_t bin_count = __riscv_vmv_v_x_u8m2(0, vl);
          const auto add_match = [&](const vuint8m2_t values, vuint8m2_t counts) {
            const vbool4_t match =
                __riscv_vmseq_vx_u8m2_b4(values, static_cast<unsigned long>(bin), vl);
            const vuint8m2_t incremented = __riscv_vadd_vx_u8m2(counts, 1, vl);
            return __riscv_vmerge_vvm_u8m2(counts, incremented, match, vl);
          };
          bin_count = add_match(p0, bin_count);
          bin_count = add_match(p1, bin_count);
          bin_count = add_match(p2, bin_count);
          bin_count = add_match(c0, bin_count);
          bin_count = add_match(c1, bin_count);
          bin_count = add_match(c2, bin_count);
          bin_count = add_match(n0, bin_count);
          bin_count = add_match(n1, bin_count);
          bin_count = add_match(n2, bin_count);

          const vbool4_t new_max = __riscv_vmsgtu_vv_u8m2_b4(bin_count, max_count, vl);
          max_count = __riscv_vmerge_vvm_u8m2(max_count, bin_count, new_max, vl);
          output_bits = __riscv_vmerge_vxm_u8m2(
              output_bits, static_cast<unsigned long>(1u << bin), new_max, vl);
        }
        __riscv_vse8_v_u8m2(output.filtered.data() + y * width + x, output_bits, vl);
        offset += vl;
      }
    }
  }
  spreadQuantizedMapScalar(output.filtered.data(), width, height, 8, output.spreaded);
  return ExecutionPath::RvvQuantizeFilter;
#else
  computeColorArtifactsScalar(input, width, height, output);
  return ExecutionPath::ScalarFallback;
#endif
}
} // namespace pcl::recognition::rvv_test::color_modality
