#pragma once

/*
 * 本文件做什么：
 * 这个内部头文件承载 color_coding 的 component diagnostic（组件诊断）
 * 实现。reference helper 复刻当前 production color coder 的整数 byte
 * 语义；candidate helper 是后续 RVV 实验入口。
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::octree::rvv_color_coding_support {

struct ColorPoint {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  std::uint32_t rgba = 0;
};

struct EncodedColorData {
  std::vector<char> average;
  std::vector<char> differential;
};

inline std::uint32_t
packColor(const std::uint8_t red,
          const std::uint8_t green,
          const std::uint8_t blue,
          const std::uint8_t alpha = 0)
{
  return (static_cast<std::uint32_t>(red) << 0) |
         (static_cast<std::uint32_t>(green) << 8) |
         (static_cast<std::uint32_t>(blue) << 16) |
         (static_cast<std::uint32_t>(alpha) << 24);
}

inline std::uint32_t
loadColor(const ColorPoint& point, const unsigned char rgba_offset)
{
  std::uint32_t color = 0;
  std::memcpy(&color, reinterpret_cast<const char*>(&point) + rgba_offset, sizeof(color));
  return color;
}

template <typename PointT>
inline std::uint32_t
loadPointColor(const PointT& point, const unsigned char rgba_offset)
{
  std::uint32_t color = 0;
  std::memcpy(&color, reinterpret_cast<const char*>(&point) + rgba_offset, sizeof(color));
  return color;
}

inline void
storeColor(ColorPoint& point, const unsigned char rgba_offset, const std::uint32_t color)
{
  std::memcpy(reinterpret_cast<char*>(&point) + rgba_offset, &color, sizeof(color));
}

template <typename PointT>
inline void
storePointColor(PointT& point, const unsigned char rgba_offset, const std::uint32_t color)
{
  std::memcpy(reinterpret_cast<char*>(&point) + rgba_offset, &color, sizeof(color));
}

inline void
appendAverageBytes(EncodedColorData& encoded,
                   std::uint32_t red,
                   std::uint32_t green,
                   std::uint32_t blue,
                   const unsigned char color_bit_reduction)
{
  red >>= color_bit_reduction;
  green >>= color_bit_reduction;
  blue >>= color_bit_reduction;
  encoded.average.push_back(static_cast<char>(red));
  encoded.average.push_back(static_cast<char>(green));
  encoded.average.push_back(static_cast<char>(blue));
}

inline void
sumIndexedColorsScalar(const std::vector<ColorPoint>& cloud,
                       const std::vector<std::uint32_t>& indices,
                       const unsigned char rgba_offset,
                       std::uint32_t& red,
                       std::uint32_t& green,
                       std::uint32_t& blue)
{
  red = 0;
  green = 0;
  blue = 0;
  for (const auto idx : indices) {
    const std::uint32_t color = loadColor(cloud[idx], rgba_offset);
    red += (color >> 0) & 0xffu;
    green += (color >> 8) & 0xffu;
    blue += (color >> 16) & 0xffu;
  }
}

template <typename PointT>
inline void
sumIndexedPointColorsScalar(const std::vector<PointT>& cloud,
                            const std::vector<std::uint32_t>& indices,
                            const unsigned char rgba_offset,
                            std::uint32_t& red,
                            std::uint32_t& green,
                            std::uint32_t& blue)
{
  red = 0;
  green = 0;
  blue = 0;
  for (const auto idx : indices) {
    const std::uint32_t color = loadPointColor(cloud[idx], rgba_offset);
    red += (color >> 0) & 0xffu;
    green += (color >> 8) & 0xffu;
    blue += (color >> 16) & 0xffu;
  }
}

#if defined(__RVV10__)
inline std::uint32_t
reduceU32(const vuint32m2_t values, const std::size_t vl)
{
  const vuint32m1_t zero = __riscv_vmv_v_x_u32m1(0, 1);
  const vuint32m1_t reduced = __riscv_vredsum_vs_u32m2_u32m1(values, zero, vl);
  return __riscv_vmv_x_s_u32m1_u32(reduced);
}

inline void
sumIndexedColorsRVV(const std::vector<ColorPoint>& cloud,
                    const std::vector<std::uint32_t>& indices,
                    const unsigned char rgba_offset,
                    std::uint32_t& red,
                    std::uint32_t& green,
                    std::uint32_t& blue)
{
  red = 0;
  green = 0;
  blue = 0;
  const auto* base =
      reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const char*>(cloud.data()));
  const auto stride = static_cast<std::uint32_t>(sizeof(ColorPoint));
  const auto offset = static_cast<std::uint32_t>(rgba_offset);

  for (std::size_t i = 0; i < indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const vuint32m2_t idx = __riscv_vle32_v_u32m2(indices.data() + i, vl);
    const vuint32m2_t byte_offsets =
        __riscv_vadd_vx_u32m2(__riscv_vmul_vx_u32m2(idx, stride, vl), offset, vl);
    const vuint32m2_t color = __riscv_vluxei32_v_u32m2(base, byte_offsets, vl);
    red += reduceU32(__riscv_vand_vx_u32m2(color, 0xffu, vl), vl);
    green += reduceU32(
        __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(color, 8, vl), 0xffu, vl), vl);
    blue += reduceU32(
        __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(color, 16, vl), 0xffu, vl), vl);
    i += vl;
  }
}

template <typename PointT>
inline void
sumIndexedPointColorsRVV(const std::vector<PointT>& cloud,
                         const std::vector<std::uint32_t>& indices,
                         const unsigned char rgba_offset,
                         std::uint32_t& red,
                         std::uint32_t& green,
                         std::uint32_t& blue)
{
  red = 0;
  green = 0;
  blue = 0;
  const auto* base =
      reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const char*>(cloud.data()));
  const auto stride = static_cast<std::uint32_t>(sizeof(PointT));
  const auto offset = static_cast<std::uint32_t>(rgba_offset);

  for (std::size_t i = 0; i < indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const vuint32m2_t idx = __riscv_vle32_v_u32m2(indices.data() + i, vl);
    const vuint32m2_t byte_offsets =
        __riscv_vadd_vx_u32m2(__riscv_vmul_vx_u32m2(idx, stride, vl), offset, vl);
    const vuint32m2_t color = __riscv_vluxei32_v_u32m2(base, byte_offsets, vl);
    red += reduceU32(__riscv_vand_vx_u32m2(color, 0xffu, vl), vl);
    green += reduceU32(
        __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(color, 8, vl), 0xffu, vl), vl);
    blue += reduceU32(
        __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(color, 16, vl), 0xffu, vl), vl);
    i += vl;
  }
}
#endif

inline void
encodeAverageOfPointsReference(const std::vector<ColorPoint>& cloud,
                               const std::vector<std::uint32_t>& indices,
                               const unsigned char rgba_offset,
                               const unsigned char color_bit_reduction,
                               EncodedColorData& encoded)
{
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedColorsScalar(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
}

inline void
encodePointsReference(const std::vector<ColorPoint>& cloud,
                      const std::vector<std::uint32_t>& indices,
                      const unsigned char rgba_offset,
                      const unsigned char color_bit_reduction,
                      EncodedColorData& encoded)
{
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedColorsScalar(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
    for (const auto idx : indices) {
      const std::uint32_t color = loadColor(cloud[idx], rgba_offset);
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(red) ^
                             static_cast<std::uint8_t>((color >> 0) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(green) ^
                             static_cast<std::uint8_t>((color >> 8) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(blue) ^
                             static_cast<std::uint8_t>((color >> 16) & 0xffu)) >>
                            color_bit_reduction));
    }
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
}

inline void
decodePointsReference(const EncodedColorData& encoded,
                      std::size_t& average_cursor,
                      std::size_t& diff_cursor,
                      std::vector<ColorPoint>& output,
                      const std::size_t begin_idx,
                      const std::size_t end_idx,
                      const unsigned char rgba_offset,
                      const unsigned char color_bit_reduction)
{
  const auto point_count = end_idx - begin_idx;
  auto avg_red = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_green = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_blue = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  avg_red = static_cast<std::uint8_t>(avg_red << color_bit_reduction);
  avg_green = static_cast<std::uint8_t>(avg_green << color_bit_reduction);
  avg_blue = static_cast<std::uint8_t>(avg_blue << color_bit_reduction);

  for (std::size_t i = 0; i < point_count; ++i) {
    std::uint32_t color = 0;
    if (point_count > 1) {
      auto diff_red = static_cast<std::uint8_t>(encoded.differential[diff_cursor++]);
      auto diff_green = static_cast<std::uint8_t>(encoded.differential[diff_cursor++]);
      auto diff_blue = static_cast<std::uint8_t>(encoded.differential[diff_cursor++]);
      diff_red = static_cast<std::uint8_t>(diff_red << color_bit_reduction);
      diff_green = static_cast<std::uint8_t>(diff_green << color_bit_reduction);
      diff_blue = static_cast<std::uint8_t>(diff_blue << color_bit_reduction);
      color = packColor(avg_red ^ diff_red, avg_green ^ diff_green, avg_blue ^ diff_blue);
    }
    else {
      color = packColor(avg_red, avg_green, avg_blue);
    }
    storeColor(output[begin_idx + i], rgba_offset, color);
  }
}

inline void
setDefaultColorReference(std::vector<ColorPoint>& output,
                         const std::size_t begin_idx,
                         const std::size_t end_idx,
                         const unsigned char rgba_offset)
{
  for (std::size_t i = begin_idx; i < end_idx; ++i)
    storeColor(output[i], rgba_offset, packColor(255, 255, 255));
}

inline bool
encodeAverageOfPointsCandidate(const std::vector<ColorPoint>& cloud,
                               const std::vector<std::uint32_t>& indices,
                               const unsigned char rgba_offset,
                               const unsigned char color_bit_reduction,
                               EncodedColorData& encoded)
{
#if defined(__RVV10__)
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedColorsRVV(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
  return true;
#else
  encodeAverageOfPointsReference(cloud, indices, rgba_offset, color_bit_reduction, encoded);
  return true;
#endif
}

inline bool
encodePointsCandidate(const std::vector<ColorPoint>& cloud,
                      const std::vector<std::uint32_t>& indices,
                      const unsigned char rgba_offset,
                      const unsigned char color_bit_reduction,
                      EncodedColorData& encoded)
{
#if defined(__RVV10__)
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedColorsRVV(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
    for (const auto idx : indices) {
      const std::uint32_t color = loadColor(cloud[idx], rgba_offset);
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(red) ^
                             static_cast<std::uint8_t>((color >> 0) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(green) ^
                             static_cast<std::uint8_t>((color >> 8) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(blue) ^
                             static_cast<std::uint8_t>((color >> 16) & 0xffu)) >>
                            color_bit_reduction));
    }
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
  return true;
#else
  encodePointsReference(cloud, indices, rgba_offset, color_bit_reduction, encoded);
  return true;
#endif
}

inline bool
decodePointsCandidate(const EncodedColorData& encoded,
                      std::size_t& average_cursor,
                      std::size_t& diff_cursor,
                      std::vector<ColorPoint>& output,
                      const std::size_t begin_idx,
                      const std::size_t end_idx,
                      const unsigned char rgba_offset,
                      const unsigned char color_bit_reduction)
{
#if defined(__RVV10__)
  const auto point_count = end_idx - begin_idx;
  auto avg_red = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_green = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_blue = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  avg_red = static_cast<std::uint8_t>(avg_red << color_bit_reduction);
  avg_green = static_cast<std::uint8_t>(avg_green << color_bit_reduction);
  avg_blue = static_cast<std::uint8_t>(avg_blue << color_bit_reduction);

  if (point_count == 1) {
    storeColor(output[begin_idx], rgba_offset, packColor(avg_red, avg_green, avg_blue));
    return true;
  }

  auto* first =
      reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(output.data() + begin_idx) +
                                       rgba_offset);
  const auto stride = static_cast<std::ptrdiff_t>(sizeof(ColorPoint));
  for (std::size_t i = 0; i < point_count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(point_count - i);
    const auto* diff_bytes =
        reinterpret_cast<const std::uint8_t*>(encoded.differential.data() + diff_cursor);
    const vuint8mf2_t diff_red8 = __riscv_vlse8_v_u8mf2(diff_bytes + 0, 3, vl);
    const vuint8mf2_t diff_green8 = __riscv_vlse8_v_u8mf2(diff_bytes + 1, 3, vl);
    const vuint8mf2_t diff_blue8 = __riscv_vlse8_v_u8mf2(diff_bytes + 2, 3, vl);
    const vuint32m2_t red =
        __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_red8, color_bit_reduction, vl),
                                vl);
    const vuint32m2_t green =
        __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_green8, color_bit_reduction, vl),
                                vl);
    const vuint32m2_t blue =
        __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_blue8, color_bit_reduction, vl),
                                vl);
    const vuint32m2_t color =
        __riscv_vor_vv_u32m2(__riscv_vor_vv_u32m2(
                                 __riscv_vxor_vx_u32m2(red, avg_red, vl),
                                 __riscv_vsll_vx_u32m2(
                                     __riscv_vxor_vx_u32m2(green, avg_green, vl), 8, vl),
                                 vl),
                             __riscv_vsll_vx_u32m2(
                                 __riscv_vxor_vx_u32m2(blue, avg_blue, vl), 16, vl),
                             vl);
    __riscv_vsse32_v_u32m2(first + i * (sizeof(ColorPoint) / sizeof(std::uint32_t)),
                           stride,
                           color,
                           vl);
    diff_cursor += vl * 3;
    i += vl;
  }
  return true;
#else
  decodePointsReference(encoded,
                        average_cursor,
                        diff_cursor,
                        output,
                        begin_idx,
                        end_idx,
                        rgba_offset,
                        color_bit_reduction);
  return true;
#endif
}

inline bool
setDefaultColorCandidate(std::vector<ColorPoint>& output,
                         const std::size_t begin_idx,
                         const std::size_t end_idx,
                         const unsigned char rgba_offset)
{
#if defined(__RVV10__)
  auto* first =
      reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(output.data() + begin_idx) +
                                       rgba_offset);
  const auto stride = static_cast<std::ptrdiff_t>(sizeof(ColorPoint));
  const auto white = packColor(255, 255, 255);
  const auto point_count = end_idx - begin_idx;
  for (std::size_t i = 0; i < point_count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(point_count - i);
    __riscv_vsse32_v_u32m2(first + i * (sizeof(ColorPoint) / sizeof(std::uint32_t)),
                           stride,
                           __riscv_vmv_v_x_u32m2(white, vl),
                           vl);
    i += vl;
  }
  return true;
#else
  setDefaultColorReference(output, begin_idx, end_idx, rgba_offset);
  return true;
#endif
}

template <typename PointT>
inline void
encodePointsReferencePointVector(const std::vector<PointT>& cloud,
                                 const std::vector<std::uint32_t>& indices,
                                 const unsigned char rgba_offset,
                                 const unsigned char color_bit_reduction,
                                 EncodedColorData& encoded)
{
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedPointColorsScalar(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
    for (const auto idx : indices) {
      const std::uint32_t color = loadPointColor(cloud[idx], rgba_offset);
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(red) ^
                             static_cast<std::uint8_t>((color >> 0) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(green) ^
                             static_cast<std::uint8_t>((color >> 8) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(blue) ^
                             static_cast<std::uint8_t>((color >> 16) & 0xffu)) >>
                            color_bit_reduction));
    }
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
}

template <typename PointT>
inline void
encodeAverageOfPointsReferencePointVector(const std::vector<PointT>& cloud,
                                          const std::vector<std::uint32_t>& indices,
                                          const unsigned char rgba_offset,
                                          const unsigned char color_bit_reduction,
                                          EncodedColorData& encoded)
{
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedPointColorsScalar(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
}

template <typename PointT>
inline bool
encodeAverageOfPointsCandidatePointVector(const std::vector<PointT>& cloud,
                                          const std::vector<std::uint32_t>& indices,
                                          const unsigned char rgba_offset,
                                          const unsigned char color_bit_reduction,
                                          EncodedColorData& encoded)
{
#if defined(__RVV10__)
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedPointColorsRVV(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
  return true;
#else
  encodeAverageOfPointsReferencePointVector(
      cloud, indices, rgba_offset, color_bit_reduction, encoded);
  return true;
#endif
}

template <typename PointT>
inline bool
encodePointsCandidatePointVector(const std::vector<PointT>& cloud,
                                 const std::vector<std::uint32_t>& indices,
                                 const unsigned char rgba_offset,
                                 const unsigned char color_bit_reduction,
                                 EncodedColorData& encoded)
{
#if defined(__RVV10__)
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
  sumIndexedPointColorsRVV(cloud, indices, rgba_offset, red, green, blue);
  const auto len = static_cast<std::uint32_t>(indices.size());
  if (len > 1) {
    red /= len;
    green /= len;
    blue /= len;
    for (const auto idx : indices) {
      const std::uint32_t color = loadPointColor(cloud[idx], rgba_offset);
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(red) ^
                             static_cast<std::uint8_t>((color >> 0) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(green) ^
                             static_cast<std::uint8_t>((color >> 8) & 0xffu)) >>
                            color_bit_reduction));
      encoded.differential.push_back(
          static_cast<char>((static_cast<std::uint8_t>(blue) ^
                             static_cast<std::uint8_t>((color >> 16) & 0xffu)) >>
                            color_bit_reduction));
    }
  }
  appendAverageBytes(encoded, red, green, blue, color_bit_reduction);
  return true;
#else
  encodePointsReferencePointVector(cloud, indices, rgba_offset, color_bit_reduction, encoded);
  return true;
#endif
}

template <typename PointT>
inline void
decodePointsReferencePointVector(const EncodedColorData& encoded,
                                 std::size_t& average_cursor,
                                 std::size_t& diff_cursor,
                                 std::vector<PointT>& output,
                                 const std::size_t begin_idx,
                                 const std::size_t end_idx,
                                 const unsigned char rgba_offset,
                                 const unsigned char color_bit_reduction)
{
  const auto point_count = end_idx - begin_idx;
  auto avg_red = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_green = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_blue = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  avg_red = static_cast<std::uint8_t>(avg_red << color_bit_reduction);
  avg_green = static_cast<std::uint8_t>(avg_green << color_bit_reduction);
  avg_blue = static_cast<std::uint8_t>(avg_blue << color_bit_reduction);

  for (std::size_t i = 0; i < point_count; ++i) {
    std::uint32_t color = 0;
    if (point_count > 1) {
      auto diff_red = static_cast<std::uint8_t>(encoded.differential[diff_cursor++]);
      auto diff_green = static_cast<std::uint8_t>(encoded.differential[diff_cursor++]);
      auto diff_blue = static_cast<std::uint8_t>(encoded.differential[diff_cursor++]);
      diff_red = static_cast<std::uint8_t>(diff_red << color_bit_reduction);
      diff_green = static_cast<std::uint8_t>(diff_green << color_bit_reduction);
      diff_blue = static_cast<std::uint8_t>(diff_blue << color_bit_reduction);
      color = packColor(avg_red ^ diff_red, avg_green ^ diff_green, avg_blue ^ diff_blue);
    }
    else {
      color = packColor(avg_red, avg_green, avg_blue);
    }
    storePointColor(output[begin_idx + i], rgba_offset, color);
  }
}

template <typename PointT>
inline bool
decodePointsCandidatePointVector(const EncodedColorData& encoded,
                                 std::size_t& average_cursor,
                                 std::size_t& diff_cursor,
                                 std::vector<PointT>& output,
                                 const std::size_t begin_idx,
                                 const std::size_t end_idx,
                                 const unsigned char rgba_offset,
                                 const unsigned char color_bit_reduction)
{
#if defined(__RVV10__)
  const auto point_count = end_idx - begin_idx;
  auto avg_red = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_green = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_blue = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  avg_red = static_cast<std::uint8_t>(avg_red << color_bit_reduction);
  avg_green = static_cast<std::uint8_t>(avg_green << color_bit_reduction);
  avg_blue = static_cast<std::uint8_t>(avg_blue << color_bit_reduction);

  if (point_count == 1) {
    storePointColor(output[begin_idx], rgba_offset, packColor(avg_red, avg_green, avg_blue));
    return true;
  }

  auto* first =
      reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(output.data() + begin_idx) +
                                       rgba_offset);
  const auto stride = static_cast<std::ptrdiff_t>(sizeof(PointT));
  for (std::size_t i = 0; i < point_count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(point_count - i);
    const auto* diff_bytes =
        reinterpret_cast<const std::uint8_t*>(encoded.differential.data() + diff_cursor);
    const vuint8mf2_t diff_red8 = __riscv_vlse8_v_u8mf2(diff_bytes + 0, 3, vl);
    const vuint8mf2_t diff_green8 = __riscv_vlse8_v_u8mf2(diff_bytes + 1, 3, vl);
    const vuint8mf2_t diff_blue8 = __riscv_vlse8_v_u8mf2(diff_bytes + 2, 3, vl);
    const vuint32m2_t red =
        __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_red8, color_bit_reduction, vl),
                                vl);
    const vuint32m2_t green =
        __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_green8, color_bit_reduction, vl),
                                vl);
    const vuint32m2_t blue =
        __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_blue8, color_bit_reduction, vl),
                                vl);
    const vuint32m2_t color =
        __riscv_vor_vv_u32m2(__riscv_vor_vv_u32m2(
                                 __riscv_vxor_vx_u32m2(red, avg_red, vl),
                                 __riscv_vsll_vx_u32m2(
                                     __riscv_vxor_vx_u32m2(green, avg_green, vl), 8, vl),
                                 vl),
                             __riscv_vsll_vx_u32m2(
                                 __riscv_vxor_vx_u32m2(blue, avg_blue, vl), 16, vl),
                             vl);
    __riscv_vsse32_v_u32m2(first + i * (sizeof(PointT) / sizeof(std::uint32_t)),
                           stride,
                           color,
                           vl);
    diff_cursor += vl * 3;
    i += vl;
  }
  return true;
#else
  decodePointsReferencePointVector(encoded,
                                   average_cursor,
                                   diff_cursor,
                                   output,
                                   begin_idx,
                                   end_idx,
                                   rgba_offset,
                                   color_bit_reduction);
  return true;
#endif
}

template <typename PointT>
inline bool
decodePointsCandidatePointVectorStagedStore(const EncodedColorData& encoded,
                                            std::size_t& average_cursor,
                                            std::size_t& diff_cursor,
                                            std::vector<PointT>& output,
                                            const std::size_t begin_idx,
                                            const std::size_t end_idx,
                                            const unsigned char rgba_offset,
                                            const unsigned char color_bit_reduction,
                                            std::vector<std::uint32_t>& scratch_colors)
{
#if defined(__RVV10__)
  const auto point_count = end_idx - begin_idx;
  scratch_colors.resize(point_count);
  auto avg_red = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_green = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  auto avg_blue = static_cast<std::uint8_t>(encoded.average[average_cursor++]);
  avg_red = static_cast<std::uint8_t>(avg_red << color_bit_reduction);
  avg_green = static_cast<std::uint8_t>(avg_green << color_bit_reduction);
  avg_blue = static_cast<std::uint8_t>(avg_blue << color_bit_reduction);

  if (point_count == 1) {
    scratch_colors[0] = packColor(avg_red, avg_green, avg_blue);
  }
  else {
    for (std::size_t i = 0; i < point_count;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(point_count - i);
      const auto* diff_bytes =
          reinterpret_cast<const std::uint8_t*>(encoded.differential.data() + diff_cursor);
      const vuint8mf2_t diff_red8 = __riscv_vlse8_v_u8mf2(diff_bytes + 0, 3, vl);
      const vuint8mf2_t diff_green8 = __riscv_vlse8_v_u8mf2(diff_bytes + 1, 3, vl);
      const vuint8mf2_t diff_blue8 = __riscv_vlse8_v_u8mf2(diff_bytes + 2, 3, vl);
      const vuint32m2_t red =
          __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_red8, color_bit_reduction, vl),
                                  vl);
      const vuint32m2_t green =
          __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_green8, color_bit_reduction, vl),
                                  vl);
      const vuint32m2_t blue =
          __riscv_vzext_vf4_u32m2(__riscv_vsll_vx_u8mf2(diff_blue8, color_bit_reduction, vl),
                                  vl);
      const vuint32m2_t color =
          __riscv_vor_vv_u32m2(__riscv_vor_vv_u32m2(
                                   __riscv_vxor_vx_u32m2(red, avg_red, vl),
                                   __riscv_vsll_vx_u32m2(
                                       __riscv_vxor_vx_u32m2(green, avg_green, vl), 8, vl),
                                   vl),
                               __riscv_vsll_vx_u32m2(
                                   __riscv_vxor_vx_u32m2(blue, avg_blue, vl), 16, vl),
                               vl);
      __riscv_vse32_v_u32m2(scratch_colors.data() + i, color, vl);
      diff_cursor += vl * 3;
      i += vl;
    }
  }

  for (std::size_t i = 0; i < point_count; ++i)
    storePointColor(output[begin_idx + i], rgba_offset, scratch_colors[i]);
  return true;
#else
  (void)scratch_colors;
  decodePointsReferencePointVector(encoded,
                                   average_cursor,
                                   diff_cursor,
                                   output,
                                   begin_idx,
                                   end_idx,
                                   rgba_offset,
                                   color_bit_reduction);
  return true;
#endif
}

template <typename PointT>
inline void
setDefaultColorReferencePointVector(std::vector<PointT>& output,
                                    const std::size_t begin_idx,
                                    const std::size_t end_idx,
                                    const unsigned char rgba_offset)
{
  for (std::size_t i = begin_idx; i < end_idx; ++i)
    storePointColor(output[i], rgba_offset, packColor(255, 255, 255));
}

template <typename PointT>
inline bool
setDefaultColorCandidatePointVector(std::vector<PointT>& output,
                                    const std::size_t begin_idx,
                                    const std::size_t end_idx,
                                    const unsigned char rgba_offset)
{
#if defined(__RVV10__)
  auto* first =
      reinterpret_cast<std::uint32_t*>(reinterpret_cast<char*>(output.data() + begin_idx) +
                                       rgba_offset);
  const auto stride = static_cast<std::ptrdiff_t>(sizeof(PointT));
  const auto white = packColor(255, 255, 255);
  const auto point_count = end_idx - begin_idx;
  for (std::size_t i = 0; i < point_count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(point_count - i);
    __riscv_vsse32_v_u32m2(first + i * (sizeof(PointT) / sizeof(std::uint32_t)),
                           stride,
                           __riscv_vmv_v_x_u32m2(white, vl),
                           vl);
    i += vl;
  }
  return true;
#else
  setDefaultColorReferencePointVector(output, begin_idx, end_idx, rgba_offset);
  return true;
#endif
}

} // namespace pcl::octree::rvv_color_coding_support
