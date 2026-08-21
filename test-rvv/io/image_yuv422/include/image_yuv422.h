#pragma once

/*
 * 本文件做什么：
 * 这里放 image_yuv422 topic 的测试专用 reference（参考链路）和 RVV
 * candidate（候选链路）。它复刻 `ImageYUV422::fillRGB()` /
 * `fillGrayscale()` 的内层像素语义，供 gtest 和 bench 对拍。
 *
 * 证据边界：
 * 本文件不属于 production（生产源码），也不改变 `pcl::io::ImageYUV422`
 * 的公开行为。`__RVV10__` 关闭或当前入口不满足首轮 full-size 条件时，
 * candidate 自然回到同一份标量参考链路。
 */

#include <cstddef>
#include <cstdint>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_image_yuv422_support {

inline std::uint8_t
clipByte(const int value)
{
  return static_cast<std::uint8_t>(value > 255 ? 255 : (value < 0 ? 0 : value));
}

inline unsigned
normalizeRgbLineStep(const unsigned width, const unsigned line_step)
{
  return line_step == 0 ? width * 3 : line_step;
}

inline unsigned
normalizeGrayLineStep(const unsigned width, const unsigned line_step)
{
  return line_step == 0 ? width : line_step;
}

inline void
convertPairToRgb(const std::uint8_t* yuv, std::uint8_t* rgb)
{
  const int u = static_cast<int>(yuv[0]) - 128;
  const int v = static_cast<int>(yuv[2]) - 128;
  const int r_delta = (v * 18678 + 8192) >> 14;
  const int g_delta = (v * -9519 - u * 6472 + 8192) >> 14;
  const int b_delta = (u * 33292 + 8192) >> 14;
  const int y1 = yuv[1];
  const int y2 = yuv[3];

  rgb[0] = clipByte(y1 + r_delta);
  rgb[1] = clipByte(y1 + g_delta);
  rgb[2] = clipByte(y1 + b_delta);
  rgb[3] = clipByte(y2 + r_delta);
  rgb[4] = clipByte(y2 + g_delta);
  rgb[5] = clipByte(y2 + b_delta);
}

inline void
fillRgbScalar(const std::uint8_t* yuv,
              const unsigned src_width,
              const unsigned src_height,
              const unsigned width,
              const unsigned height,
              std::uint8_t* rgb,
              unsigned rgb_line_step)
{
  rgb_line_step = normalizeRgbLineStep(width, rgb_line_step);
  const unsigned rgb_line_skip = rgb_line_step - width * 3;

  if (src_width == width && src_height == height) {
    const auto* src = yuv;
    auto* dst = rgb;
    for (unsigned y = 0; y < height; ++y, dst += rgb_line_skip) {
      for (unsigned x = 0; x < width; x += 2, dst += 6, src += 4)
        convertPairToRgb(src, dst);
    }
    return;
  }

  const unsigned yuv_step = src_width / width;
  const unsigned yuv_x_step = yuv_step << 1;
  const unsigned yuv_skip = (src_height / height - 1) * (src_width << 1);
  const auto* src = yuv;
  auto* dst = rgb;

  for (unsigned y = 0; y < src_height; y += yuv_step, src += yuv_skip, dst += rgb_line_skip) {
    for (unsigned x = 0; x < src_width; x += yuv_step, dst += 3, src += yuv_x_step) {
      const int u = static_cast<int>(src[0]) - 128;
      const int v = static_cast<int>(src[2]) - 128;
      const int yy = src[1];
      dst[0] = clipByte(yy + ((v * 18678 + 8192) >> 14));
      dst[1] = clipByte(yy + ((v * -9519 - u * 6472 + 8192) >> 14));
      dst[2] = clipByte(yy + ((u * 33292 + 8192) >> 14));
    }
  }
}

inline void
fillGrayscaleScalar(const std::uint8_t* yuv,
                    const unsigned src_width,
                    const unsigned src_height,
                    const unsigned width,
                    const unsigned height,
                    std::uint8_t* gray,
                    unsigned gray_line_step)
{
  gray_line_step = normalizeGrayLineStep(width, gray_line_step);
  const unsigned gray_line_skip = gray_line_step - width;
  const unsigned yuv_step = src_width / width;
  const unsigned yuv_x_step = yuv_step << 1;
  const unsigned yuv_skip = (src_height / height - 1) * (src_width << 1);
  auto* dst = gray;
  const auto* src = yuv + 1;

  for (unsigned y = 0; y < src_height; y += yuv_step, src += yuv_skip, dst += gray_line_skip) {
    for (unsigned x = 0; x < src_width; x += yuv_step, ++dst, src += yuv_x_step)
      *dst = *src;
  }
}

#if defined(__RVV10__)
inline vint32m2_t
u8ToI32(const vuint8mf2_t value, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1(value, vl);
  return __riscv_vwadd_vx_i32m2(__riscv_vreinterpret_v_u16m1_i16m1(widened), 0, vl);
}

inline vint32m2_t
u8OffsetToI32(const vuint8mf2_t value, const int offset, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1(value, vl);
  const vuint16m1_t shifted = __riscv_vsub_vx_u16m1(widened, offset, vl);
  return __riscv_vwadd_vx_i32m2(__riscv_vreinterpret_v_u16m1_i16m1(shifted), 0, vl);
}

inline vuint8mf2_t
clipI32ToU8(vint32m2_t value, const std::size_t vl)
{
  value = __riscv_vmax_vx_i32m2(value, 0, vl);
  value = __riscv_vmin_vx_i32m2(value, 255, vl);
  const vuint32m2_t value_u32 = __riscv_vreinterpret_v_i32m2_u32m2(value);
  const vuint16m1_t value_u16 = __riscv_vncvt_x_x_w_u16m1(value_u32, vl);
  return __riscv_vncvt_x_x_w_u8mf2(value_u16, vl);
}

inline vuint8mf2_t
rgbChannel(const vint32m2_t y, vint32m2_t delta, const std::size_t vl)
{
  delta = __riscv_vadd_vx_i32m2(delta, 8192, vl);
  delta = __riscv_vsra_vx_i32m2(delta, 14, vl);
  return clipI32ToU8(__riscv_vadd_vv_i32m2(y, delta, vl), vl);
}

inline void
storeRgbTriplet(std::uint8_t* base,
                const vuint8mf2_t r,
                const vuint8mf2_t g,
                const vuint8mf2_t b,
                const std::size_t vl)
{
  vuint8mf2x3_t triplet = __riscv_vcreate_v_u8mf2x3(r, g, b);
  __riscv_vssseg3e8_v_u8mf2x3(base, 6, triplet, vl);
}

inline void
storeRgbTripletContiguous(std::uint8_t* base,
                          const vuint8mf2_t r,
                          const vuint8mf2_t g,
                          const vuint8mf2_t b,
                          const std::size_t vl)
{
  vuint8mf2x3_t triplet = __riscv_vcreate_v_u8mf2x3(r, g, b);
  __riscv_vsseg3e8_v_u8mf2x3(base, triplet, vl);
}

inline void
fillRgbFullSizeRVV(const std::uint8_t* yuv,
                   const unsigned width,
                   const unsigned height,
                   std::uint8_t* rgb,
                   const unsigned rgb_line_step)
{
  const unsigned pairs = width / 2;
  for (unsigned row = 0; row < height; ++row) {
    const auto* src = yuv + static_cast<std::size_t>(row) * width * 2;
    auto* dst = rgb + static_cast<std::size_t>(row) * rgb_line_step;
    for (unsigned pair = 0; pair < pairs;) {
      const std::size_t vl = __riscv_vsetvl_e8mf2(pairs - pair);
      const auto* base = src + static_cast<std::size_t>(pair) * 4;
      auto* out = dst + static_cast<std::size_t>(pair) * 6;

      const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2(base + 0, 4, vl);
      const vuint8mf2_t y1_8 = __riscv_vlse8_v_u8mf2(base + 1, 4, vl);
      const vuint8mf2_t v8 = __riscv_vlse8_v_u8mf2(base + 2, 4, vl);
      const vuint8mf2_t y2_8 = __riscv_vlse8_v_u8mf2(base + 3, 4, vl);

      const vint32m2_t u = u8OffsetToI32(u8, 128, vl);
      const vint32m2_t v = u8OffsetToI32(v8, 128, vl);
      const vint32m2_t y1 = u8ToI32(y1_8, vl);
      const vint32m2_t y2 = u8ToI32(y2_8, vl);

      const vint32m2_t r_delta = __riscv_vmul_vx_i32m2(v, 18678, vl);
      vint32m2_t g_delta = __riscv_vmul_vx_i32m2(v, -9519, vl);
      g_delta = __riscv_vsub_vv_i32m2(g_delta, __riscv_vmul_vx_i32m2(u, 6472, vl), vl);
      const vint32m2_t b_delta = __riscv_vmul_vx_i32m2(u, 33292, vl);

      storeRgbTriplet(out + 0,
                      rgbChannel(y1, r_delta, vl),
                      rgbChannel(y1, g_delta, vl),
                      rgbChannel(y1, b_delta, vl),
                      vl);
      storeRgbTriplet(out + 3,
                      rgbChannel(y2, r_delta, vl),
                      rgbChannel(y2, g_delta, vl),
                      rgbChannel(y2, b_delta, vl),
                      vl);

      pair += static_cast<unsigned>(vl);
    }
  }
}

inline void
fillRgbDownsampleRVV(const std::uint8_t* yuv,
                     const unsigned src_width,
                     const unsigned src_height,
                     const unsigned width,
                     const unsigned height,
                     std::uint8_t* rgb,
                     const unsigned rgb_line_step)
{
  const unsigned yuv_step = src_width / width;
  const unsigned yuv_x_step = yuv_step << 1;
  for (unsigned row = 0; row < height; ++row) {
    const auto* src =
        yuv + static_cast<std::size_t>(row) * yuv_step * src_width * 2;
    auto* dst = rgb + static_cast<std::size_t>(row) * rgb_line_step;
    for (unsigned x = 0; x < width;) {
      const std::size_t vl = __riscv_vsetvl_e8mf2(width - x);
      const auto* base = src + static_cast<std::size_t>(x) * yuv_x_step;
      auto* out = dst + static_cast<std::size_t>(x) * 3;

      const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2(base + 0, yuv_x_step, vl);
      const vuint8mf2_t y8 = __riscv_vlse8_v_u8mf2(base + 1, yuv_x_step, vl);
      const vuint8mf2_t v8 = __riscv_vlse8_v_u8mf2(base + 2, yuv_x_step, vl);

      const vint32m2_t u = u8OffsetToI32(u8, 128, vl);
      const vint32m2_t v = u8OffsetToI32(v8, 128, vl);
      const vint32m2_t yy = u8ToI32(y8, vl);

      const vint32m2_t r_delta = __riscv_vmul_vx_i32m2(v, 18678, vl);
      vint32m2_t g_delta = __riscv_vmul_vx_i32m2(v, -9519, vl);
      g_delta = __riscv_vsub_vv_i32m2(g_delta, __riscv_vmul_vx_i32m2(u, 6472, vl), vl);
      const vint32m2_t b_delta = __riscv_vmul_vx_i32m2(u, 33292, vl);

      storeRgbTripletContiguous(out,
                                rgbChannel(yy, r_delta, vl),
                                rgbChannel(yy, g_delta, vl),
                                rgbChannel(yy, b_delta, vl),
                                vl);

      x += static_cast<unsigned>(vl);
    }
  }
}

#endif

inline void
fillRgbCandidate(const std::uint8_t* yuv,
                 const unsigned src_width,
                 const unsigned src_height,
                 const unsigned width,
                 const unsigned height,
                 std::uint8_t* rgb,
                 unsigned rgb_line_step)
{
  rgb_line_step = normalizeRgbLineStep(width, rgb_line_step);
#if defined(__RVV10__)
  if (src_width == width && src_height == height && (width % 2) == 0) {
    fillRgbFullSizeRVV(yuv, width, height, rgb, rgb_line_step);
    return;
  }
  if (src_width != width && src_height != height && width != 0 && height != 0 &&
      (src_width % width) == 0 && (src_height % height) == 0 &&
      ((src_width / width) % 2) == 0 && ((src_height / height) % 2) == 0) {
    fillRgbDownsampleRVV(yuv, src_width, src_height, width, height, rgb, rgb_line_step);
    return;
  }
#endif
  fillRgbScalar(yuv, src_width, src_height, width, height, rgb, rgb_line_step);
}

inline void
fillGrayscaleCandidate(const std::uint8_t* yuv,
                       const unsigned src_width,
                       const unsigned src_height,
                       const unsigned width,
                       const unsigned height,
  std::uint8_t* gray,
  unsigned gray_line_step)
{
  gray_line_step = normalizeGrayLineStep(width, gray_line_step);
  // 全尺寸灰度 RVV 诊断在板卡 smoke 中明显慢于标量路径；本候选只保留
  // RGB RVV，灰度继续走同一份参考链路，避免把负收益 byte copy 接入后续判断。
  fillGrayscaleScalar(yuv, src_width, src_height, width, height, gray, gray_line_step);
}

} // namespace pcl::io::rvv_image_yuv422_support
