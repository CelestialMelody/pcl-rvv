#pragma once

/*
 * 本文件做什么：
 * 这里放 debayer topic 的测试专用 reference（参考链路）和 RVV candidate
 *（候选链路）。首阶段只覆盖 DeBayer::debayerBilinear() 的 full-size
 * 内区 2x2 Bayer stencil（邻域模板）计算，边界行列和 edge-aware
 * 分支留给后续 phase。
 *
 * 证据边界：
 * 本文件不属于 production（生产源码），也不改变 pcl::io::DeBayer 的公开行为。
 * 它只能证明这个局部 stencil candidate 的 correctness（正确性）、反汇编和板卡
 * 性能信号，不能证明 production dispatch（生产分流）已经接入 RVV。
 */

#include <cstddef>
#include <cstdint>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_debayer_support {

enum class ExecutionPath
{
  ScalarFallback,
  RvvBilinearInterior
};

inline std::uint8_t
avg2(const std::uint8_t a, const std::uint8_t b)
{
  return static_cast<std::uint8_t>((static_cast<int>(a) + static_cast<int>(b)) >> 1);
}

inline std::uint8_t
avg4(const std::uint8_t a, const std::uint8_t b, const std::uint8_t c, const std::uint8_t d)
{
  return static_cast<std::uint8_t>(
      (static_cast<int>(a) + static_cast<int>(b) + static_cast<int>(c) + static_cast<int>(d)) >> 2);
}

inline unsigned
normalizeBayerLineStep(const unsigned width, const int bayer_line_step)
{
  return bayer_line_step == 0 ? width : static_cast<unsigned>(bayer_line_step);
}

inline unsigned
normalizeBayerLineStep2(const unsigned width, const int bayer_line_step2)
{
  return bayer_line_step2 == 0 ? (width << 1) : static_cast<unsigned>(bayer_line_step2);
}

inline unsigned
normalizeRgbLineStep(const unsigned width, const unsigned rgb_line_step)
{
  return rgb_line_step == 0 ? width * 3 : rgb_line_step;
}

inline void
debayerBilinearInteriorScalar(const std::uint8_t* bayer,
                              std::uint8_t* rgb,
                              const unsigned width,
                              const unsigned height,
                              int bayer_line_step,
                              int bayer_line_step2,
                              unsigned rgb_line_step)
{
  const unsigned line_step = normalizeBayerLineStep(width, bayer_line_step);
  const unsigned line_step2 = normalizeBayerLineStep2(width, bayer_line_step2);
  const unsigned out_step = normalizeRgbLineStep(width, rgb_line_step);

  for (unsigned y = 2; y < height - 2; y += 2) {
    for (unsigned x = 2; x < width - 2; x += 2) {
      const auto* p = bayer + static_cast<std::size_t>(y) * line_step + x;
      auto* top = rgb + static_cast<std::size_t>(y) * out_step + x * 3;
      auto* bottom = top + out_step;

      top[0] = avg2(p[1], p[-1]);
      top[1] = p[0];
      top[2] = avg2(p[line_step], p[-static_cast<int>(line_step)]);

      top[3] = p[1];
      top[4] = avg4(p[0], p[2], p[line_step + 1], p[1 - static_cast<int>(line_step)]);
      top[5] = avg4(p[-static_cast<int>(line_step)],
                    p[2 - static_cast<int>(line_step)],
                    p[line_step],
                    p[line_step + 2]);

      bottom[0] = avg4(p[1], p[line_step2 + 1], p[-1], p[line_step2 - 1]);
      bottom[1] = avg4(p[0], p[line_step2], p[line_step - 1], p[line_step + 1]);
      bottom[2] = p[line_step];

      bottom[3] = avg2(p[1], p[line_step2 + 1]);
      bottom[4] = p[line_step + 1];
      bottom[5] = avg2(p[line_step], p[line_step + 2]);
    }
  }
}

#if defined(__RVV10__)
inline vuint16m1_t
widenU8(const vuint8mf2_t value, const std::size_t vl)
{
  return __riscv_vzext_vf2_u16m1(value, vl);
}

inline vuint8mf2_t
avg2U8(const vuint8mf2_t a, const vuint8mf2_t b, const std::size_t vl)
{
  vuint16m1_t sum = __riscv_vadd_vv_u16m1(widenU8(a, vl), widenU8(b, vl), vl);
  sum = __riscv_vsrl_vx_u16m1(sum, 1, vl);
  return __riscv_vncvt_x_x_w_u8mf2(sum, vl);
}

inline vuint8mf2_t
avg4U8(const vuint8mf2_t a,
       const vuint8mf2_t b,
       const vuint8mf2_t c,
       const vuint8mf2_t d,
       const std::size_t vl)
{
  vuint16m1_t sum = __riscv_vadd_vv_u16m1(widenU8(a, vl), widenU8(b, vl), vl);
  sum = __riscv_vadd_vv_u16m1(sum, widenU8(c, vl), vl);
  sum = __riscv_vadd_vv_u16m1(sum, widenU8(d, vl), vl);
  sum = __riscv_vsrl_vx_u16m1(sum, 2, vl);
  return __riscv_vncvt_x_x_w_u8mf2(sum, vl);
}

inline void
storeRgbBlock6(std::uint8_t* base,
               const vuint8mf2_t c0,
               const vuint8mf2_t c1,
               const vuint8mf2_t c2,
               const vuint8mf2_t c3,
               const vuint8mf2_t c4,
               const vuint8mf2_t c5,
               const std::size_t vl)
{
  vuint8mf2x6_t channels = __riscv_vset_v_u8mf2_u8mf2x6(__riscv_vundefined_u8mf2x6(), 0, c0);
  channels = __riscv_vset_v_u8mf2_u8mf2x6(channels, 1, c1);
  channels = __riscv_vset_v_u8mf2_u8mf2x6(channels, 2, c2);
  channels = __riscv_vset_v_u8mf2_u8mf2x6(channels, 3, c3);
  channels = __riscv_vset_v_u8mf2_u8mf2x6(channels, 4, c4);
  channels = __riscv_vset_v_u8mf2_u8mf2x6(channels, 5, c5);
  __riscv_vsseg6e8_v_u8mf2x6(base, channels, vl);
}

inline void
debayerBilinearInteriorRVV(const std::uint8_t* bayer,
                           std::uint8_t* rgb,
                           const unsigned width,
                           const unsigned height,
                           const unsigned rgb_line_step)
{
  const unsigned pairs = (width - 4) / 2;
  for (unsigned y = 2; y < height - 2; y += 2) {
    const auto* row = bayer + static_cast<std::size_t>(y) * width;
    auto* top_row = rgb + static_cast<std::size_t>(y) * rgb_line_step;
    for (unsigned pair = 0; pair < pairs;) {
      const std::size_t vl = __riscv_vsetvl_e8mf2(pairs - pair);
      const unsigned x = 2 + pair * 2;
      const auto* p = row + x;
      auto* top = top_row + x * 3;
      auto* bottom = top + rgb_line_step;

      const vuint8mf2_t p_m1 = __riscv_vlse8_v_u8mf2(p - 1, 2, vl);
      const vuint8mf2_t p0 = __riscv_vlse8_v_u8mf2(p + 0, 2, vl);
      const vuint8mf2_t p1 = __riscv_vlse8_v_u8mf2(p + 1, 2, vl);
      const vuint8mf2_t p2 = __riscv_vlse8_v_u8mf2(p + 2, 2, vl);
      const vuint8mf2_t p_up0 = __riscv_vlse8_v_u8mf2(p - width, 2, vl);
      const vuint8mf2_t p_up1 = __riscv_vlse8_v_u8mf2(p - width + 1, 2, vl);
      const vuint8mf2_t p_up2 = __riscv_vlse8_v_u8mf2(p - width + 2, 2, vl);
      const vuint8mf2_t p_dn_m1 = __riscv_vlse8_v_u8mf2(p + width - 1, 2, vl);
      const vuint8mf2_t p_dn0 = __riscv_vlse8_v_u8mf2(p + width, 2, vl);
      const vuint8mf2_t p_dn1 = __riscv_vlse8_v_u8mf2(p + width + 1, 2, vl);
      const vuint8mf2_t p_dn2 = __riscv_vlse8_v_u8mf2(p + width + 2, 2, vl);
      const vuint8mf2_t p_dn2_m1 = __riscv_vlse8_v_u8mf2(p + width * 2 - 1, 2, vl);
      const vuint8mf2_t p_dn2_0 = __riscv_vlse8_v_u8mf2(p + width * 2, 2, vl);
      const vuint8mf2_t p_dn2_1 = __riscv_vlse8_v_u8mf2(p + width * 2 + 1, 2, vl);

      storeRgbBlock6(top,
                     avg2U8(p1, p_m1, vl),
                     p0,
                     avg2U8(p_dn0, p_up0, vl),
                     p1,
                     avg4U8(p0, p2, p_dn1, p_up1, vl),
                     avg4U8(p_up0, p_up2, p_dn0, p_dn2, vl),
                     vl);

      storeRgbBlock6(bottom,
                     avg4U8(p1, p_dn2_1, p_m1, p_dn2_m1, vl),
                     avg4U8(p0, p_dn2_0, p_dn_m1, p_dn1, vl),
                     p_dn0,
                     avg2U8(p1, p_dn2_1, vl),
                     p_dn1,
                     avg2U8(p_dn0, p_dn2, vl),
                     vl);

      pair += static_cast<unsigned>(vl);
    }
  }
}
#endif

inline ExecutionPath
debayerBilinearInteriorCandidate(const std::uint8_t* bayer,
                                 std::uint8_t* rgb,
                                 const unsigned width,
                                 const unsigned height,
                                 const int bayer_line_step,
                                 const int bayer_line_step2,
                                 const unsigned rgb_line_step)
{
#if defined(__RVV10__)
  const unsigned line_step = normalizeBayerLineStep(width, bayer_line_step);
  const unsigned line_step2 = normalizeBayerLineStep2(width, bayer_line_step2);
  const unsigned out_step = normalizeRgbLineStep(width, rgb_line_step);
  if (width >= 6 && height >= 6 && (width % 2) == 0 && (height % 2) == 0 &&
      line_step == width && line_step2 == width * 2) {
    debayerBilinearInteriorRVV(bayer, rgb, width, height, out_step);
    return ExecutionPath::RvvBilinearInterior;
  }
#endif
  debayerBilinearInteriorScalar(
      bayer, rgb, width, height, bayer_line_step, bayer_line_step2, rgb_line_step);
  return ExecutionPath::ScalarFallback;
}

} // namespace pcl::io::rvv_debayer_support
