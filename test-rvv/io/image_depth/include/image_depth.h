#pragma once

/*
 * 本文件做什么：
 * 这里放 image_depth topic 的测试专用 reference（参考链路）和 RVV candidate
 *（候选链路）。它复刻 `DepthImage::fillDepthImage()` / `fillDisparityImage()`
 * 的内层像素语义，供 gtest 和 bench 对拍。
 *
 * 证据边界：
 * 本文件不属于 production（生产源码），也不改变 `pcl::io::DepthImage` 的公开
 * 行为。`__RVV10__` 关闭或当前入口不满足首轮 contiguous 条件时，candidate
 * 自然回到同一份标量参考链路。
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_image_depth_support {

enum class CandidatePath { Scalar, ContiguousRvv, DownsampleRvv };

inline bool
isIntegerDownsample(const unsigned src_width,
                    const unsigned src_height,
                    const unsigned width,
                    const unsigned height)
{
  return width > 0 && height > 0 && src_width >= width && src_height >= height &&
         src_width % width == 0 && src_height % height == 0;
}

inline unsigned
xStepFor(const unsigned src_width, const unsigned width)
{
  return src_width / width;
}

inline unsigned
yStepFor(const unsigned src_height, const unsigned height)
{
  return src_height / height;
}

inline CandidatePath
selectDepthMetersCandidatePath(const unsigned src_width,
                               const unsigned src_height,
                               const unsigned width,
                               const unsigned height)
{
#if defined(__RVV10__)
  if (src_width == width && src_height == height)
    return CandidatePath::ContiguousRvv;
  if (isIntegerDownsample(src_width, src_height, width, height) &&
      xStepFor(src_width, width) > 1)
    return CandidatePath::DownsampleRvv;
#endif
  return CandidatePath::Scalar;
}

inline CandidatePath
selectDisparityCandidatePath(const unsigned src_width,
                             const unsigned src_height,
                             const unsigned width,
                             const unsigned height)
{
  return selectDepthMetersCandidatePath(src_width, src_height, width, height);
}

inline bool
isInvalidDepthPixel(const std::uint16_t pixel,
                    const std::uint64_t no_sample_value,
                    const std::uint64_t shadow_value)
{
  return pixel == 0 || pixel == no_sample_value || pixel == shadow_value;
}

inline std::uint32_t
floatBits(const float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline unsigned
normalizeLineStep(const unsigned width, const unsigned line_step)
{
  return line_step == 0 ? width * static_cast<unsigned>(sizeof(float)) : line_step;
}

inline void
fillDepthMetersScalar(const std::uint16_t* input,
                      const unsigned src_width,
                      const unsigned src_height,
                      const std::uint64_t no_sample_value,
                      const std::uint64_t shadow_value,
                      const unsigned width,
                      const unsigned height,
                      float* output,
                      unsigned line_step)
{
  line_step = normalizeLineStep(width, line_step);
  const unsigned x_step = src_width / width;
  const unsigned y_skip = (src_height / height - 1) * src_width;
  const unsigned buffer_skip = line_step - width * static_cast<unsigned>(sizeof(float));
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  unsigned depth_idx = 0;

  for (unsigned y = 0; y < height; ++y, depth_idx += y_skip) {
    for (unsigned x = 0; x < width; ++x, depth_idx += x_step, ++output) {
      const std::uint16_t pixel = input[depth_idx];
      *output = isInvalidDepthPixel(pixel, no_sample_value, shadow_value)
                    ? bad_point
                    : static_cast<float>(pixel) * 0.001f;
    }
    if (buffer_skip > 0) {
      auto* bytes = reinterpret_cast<char*>(output);
      output = reinterpret_cast<float*>(bytes + buffer_skip);
    }
  }
}

inline void
fillDisparityScalar(const std::uint16_t* input,
                    const unsigned src_width,
                    const unsigned src_height,
                    const std::uint64_t no_sample_value,
                    const std::uint64_t shadow_value,
                    const float baseline,
                    const float focal_length,
                    const unsigned width,
                    const unsigned height,
                    float* output,
                    unsigned line_step)
{
  line_step = normalizeLineStep(width, line_step);
  const unsigned x_step = src_width / width;
  const unsigned y_skip = (src_height / height - 1) * src_width;
  const unsigned buffer_skip = line_step - width * static_cast<unsigned>(sizeof(float));
  const float constant = focal_length * baseline * 1000.0f / static_cast<float>(x_step);
  unsigned depth_idx = 0;

  for (unsigned y = 0; y < height; ++y, depth_idx += y_skip) {
    for (unsigned x = 0; x < width; ++x, depth_idx += x_step, ++output) {
      const std::uint16_t pixel = input[depth_idx];
      *output = isInvalidDepthPixel(pixel, no_sample_value, shadow_value)
                    ? 0.0f
                    : constant / static_cast<float>(pixel);
    }
    if (buffer_skip > 0) {
      auto* bytes = reinterpret_cast<char*>(output);
      output = reinterpret_cast<float*>(bytes + buffer_skip);
    }
  }
}

#if defined(__RVV10__)
inline bool
fitsU16Compare(const std::uint64_t value)
{
  return value <= std::numeric_limits<std::uint16_t>::max();
}

inline vbool8_t
invalidMask(const vuint16m2_t pixels,
            const std::uint64_t no_sample_value,
            const std::uint64_t shadow_value,
            const std::size_t vl)
{
  vbool8_t invalid = __riscv_vmseq_vx_u16m2_b8(pixels, 0, vl);
  if (fitsU16Compare(no_sample_value)) {
    invalid = __riscv_vmor_mm_b8(
        invalid,
        __riscv_vmseq_vx_u16m2_b8(
            pixels, static_cast<std::uint16_t>(no_sample_value), vl),
        vl);
  }
  if (fitsU16Compare(shadow_value)) {
    invalid = __riscv_vmor_mm_b8(
        invalid,
        __riscv_vmseq_vx_u16m2_b8(
            pixels, static_cast<std::uint16_t>(shadow_value), vl),
        vl);
  }
  return invalid;
}

inline vfloat32m4_t
pixelsToFloat(const vuint16m2_t pixels, const std::size_t vl)
{
  const vuint32m4_t widened = __riscv_vwaddu_vx_u32m4(pixels, 0, vl);
  return __riscv_vfcvt_f_xu_v_f32m4(widened, vl);
}

inline void
fillDepthMetersContiguousRVV(const std::uint16_t* input,
                             const std::uint64_t no_sample_value,
                             const std::uint64_t shadow_value,
                             const unsigned width,
                             const unsigned height,
                             float* output,
                             const unsigned line_step)
{
  const unsigned row_stride = line_step / static_cast<unsigned>(sizeof(float));
  const float bad_point = std::numeric_limits<float>::quiet_NaN();

  for (unsigned y = 0; y < height; ++y) {
    const auto* src = input + static_cast<std::size_t>(y) * width;
    auto* dst = output + static_cast<std::size_t>(y) * row_stride;
    for (unsigned x = 0; x < width;) {
      const std::size_t vl = __riscv_vsetvl_e16m2(width - x);
      const vuint16m2_t pixels = __riscv_vle16_v_u16m2(src + x, vl);
      const vbool8_t invalid = invalidMask(pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8(invalid, vl);
      const vfloat32m4_t values =
          __riscv_vfmul_vf_f32m4(pixelsToFloat(pixels, vl), 0.001f, vl);
      const vfloat32m4_t bad = __riscv_vfmv_v_f_f32m4(bad_point, vl);

      __riscv_vse32_v_f32m4(dst + x, bad, vl);
      __riscv_vse32_v_f32m4_m(valid, dst + x, values, vl);
      x += static_cast<unsigned>(vl);
    }
  }
}

inline void
fillDepthMetersDownsampleRVV(const std::uint16_t* input,
                             const unsigned src_width,
                             const unsigned src_height,
                             const std::uint64_t no_sample_value,
                             const std::uint64_t shadow_value,
                             const unsigned width,
                             const unsigned height,
                             float* output,
                             const unsigned line_step)
{
  const unsigned x_step = xStepFor(src_width, width);
  const unsigned y_step = yStepFor(src_height, height);
  const unsigned row_stride = line_step / static_cast<unsigned>(sizeof(float));
  const std::ptrdiff_t stride_bytes =
      static_cast<std::ptrdiff_t>(x_step * sizeof(std::uint16_t));
  const float bad_point = std::numeric_limits<float>::quiet_NaN();

  for (unsigned y = 0; y < height; ++y) {
    const auto* src =
        input + static_cast<std::size_t>(y) * y_step * src_width;
    auto* dst = output + static_cast<std::size_t>(y) * row_stride;
    for (unsigned x = 0; x < width;) {
      const std::size_t vl = __riscv_vsetvl_e16m2(width - x);
      const vuint16m2_t pixels =
          __riscv_vlse16_v_u16m2(src + static_cast<std::size_t>(x) * x_step,
                                 stride_bytes,
                                 vl);
      const vbool8_t invalid = invalidMask(pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8(invalid, vl);
      const vfloat32m4_t values =
          __riscv_vfmul_vf_f32m4(pixelsToFloat(pixels, vl), 0.001f, vl);
      const vfloat32m4_t bad = __riscv_vfmv_v_f_f32m4(bad_point, vl);

      __riscv_vse32_v_f32m4(dst + x, bad, vl);
      __riscv_vse32_v_f32m4_m(valid, dst + x, values, vl);
      x += static_cast<unsigned>(vl);
    }
  }
}

inline void
fillDisparityContiguousRVV(const std::uint16_t* input,
                           const std::uint64_t no_sample_value,
                           const std::uint64_t shadow_value,
                           const float constant,
                           const unsigned width,
                           const unsigned height,
                           float* output,
                           const unsigned line_step)
{
  const unsigned row_stride = line_step / static_cast<unsigned>(sizeof(float));

  for (unsigned y = 0; y < height; ++y) {
    const auto* src = input + static_cast<std::size_t>(y) * width;
    auto* dst = output + static_cast<std::size_t>(y) * row_stride;
    for (unsigned x = 0; x < width;) {
      const std::size_t vl = __riscv_vsetvl_e16m2(width - x);
      const vuint16m2_t pixels = __riscv_vle16_v_u16m2(src + x, vl);
      const vbool8_t invalid = invalidMask(pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8(invalid, vl);
      const vfloat32m4_t denom = pixelsToFloat(pixels, vl);
      const vfloat32m4_t values = __riscv_vfrdiv_vf_f32m4(denom, constant, vl);
      const vfloat32m4_t zero = __riscv_vfmv_v_f_f32m4(0.0f, vl);

      __riscv_vse32_v_f32m4(dst + x, zero, vl);
      __riscv_vse32_v_f32m4_m(valid, dst + x, values, vl);
      x += static_cast<unsigned>(vl);
    }
  }
}

inline void
fillDisparityDownsampleRVV(const std::uint16_t* input,
                           const unsigned src_width,
                           const unsigned src_height,
                           const std::uint64_t no_sample_value,
                           const std::uint64_t shadow_value,
                           const float constant,
                           const unsigned width,
                           const unsigned height,
                           float* output,
                           const unsigned line_step)
{
  const unsigned x_step = xStepFor(src_width, width);
  const unsigned y_step = yStepFor(src_height, height);
  const unsigned row_stride = line_step / static_cast<unsigned>(sizeof(float));
  const std::ptrdiff_t stride_bytes =
      static_cast<std::ptrdiff_t>(x_step * sizeof(std::uint16_t));

  for (unsigned y = 0; y < height; ++y) {
    const auto* src =
        input + static_cast<std::size_t>(y) * y_step * src_width;
    auto* dst = output + static_cast<std::size_t>(y) * row_stride;
    for (unsigned x = 0; x < width;) {
      const std::size_t vl = __riscv_vsetvl_e16m2(width - x);
      const vuint16m2_t pixels =
          __riscv_vlse16_v_u16m2(src + static_cast<std::size_t>(x) * x_step,
                                 stride_bytes,
                                 vl);
      const vbool8_t invalid = invalidMask(pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8(invalid, vl);
      const vfloat32m4_t denom = pixelsToFloat(pixels, vl);
      const vfloat32m4_t values = __riscv_vfrdiv_vf_f32m4(denom, constant, vl);
      const vfloat32m4_t zero = __riscv_vfmv_v_f_f32m4(0.0f, vl);

      __riscv_vse32_v_f32m4(dst + x, zero, vl);
      __riscv_vse32_v_f32m4_m(valid, dst + x, values, vl);
      x += static_cast<unsigned>(vl);
    }
  }
}
#endif

inline void
fillDepthMetersCandidate(const std::uint16_t* input,
                         const unsigned src_width,
                         const unsigned src_height,
                         const std::uint64_t no_sample_value,
                         const std::uint64_t shadow_value,
                         const unsigned width,
                         const unsigned height,
                         float* output,
                         unsigned line_step)
{
  line_step = normalizeLineStep(width, line_step);
#if defined(__RVV10__)
  const CandidatePath path = selectDepthMetersCandidatePath(src_width, src_height, width, height);
  if (path == CandidatePath::ContiguousRvv) {
    fillDepthMetersContiguousRVV(
        input, no_sample_value, shadow_value, width, height, output, line_step);
    return;
  }
  if (path == CandidatePath::DownsampleRvv) {
    fillDepthMetersDownsampleRVV(input,
                                 src_width,
                                 src_height,
                                 no_sample_value,
                                 shadow_value,
                                 width,
                                 height,
                                 output,
                                 line_step);
    return;
  }
#endif
  fillDepthMetersScalar(
      input, src_width, src_height, no_sample_value, shadow_value, width, height, output, line_step);
}

inline void
fillDisparityCandidate(const std::uint16_t* input,
                       const unsigned src_width,
                       const unsigned src_height,
                       const std::uint64_t no_sample_value,
                       const std::uint64_t shadow_value,
                       const float baseline,
                       const float focal_length,
                       const unsigned width,
                       const unsigned height,
                       float* output,
                       unsigned line_step)
{
  line_step = normalizeLineStep(width, line_step);
#if defined(__RVV10__)
  const CandidatePath path = selectDisparityCandidatePath(src_width, src_height, width, height);
  if (path == CandidatePath::ContiguousRvv) {
    const float constant = focal_length * baseline * 1000.0f;
    fillDisparityContiguousRVV(
        input, no_sample_value, shadow_value, constant, width, height, output, line_step);
    return;
  }
  if (path == CandidatePath::DownsampleRvv) {
    const unsigned x_step = xStepFor(src_width, width);
    const float constant =
        focal_length * baseline * 1000.0f / static_cast<float>(x_step);
    fillDisparityDownsampleRVV(input,
                               src_width,
                               src_height,
                               no_sample_value,
                               shadow_value,
                               constant,
                               width,
                               height,
                               output,
                               line_step);
    return;
  }
#endif
  fillDisparityScalar(input,
                      src_width,
                      src_height,
                      no_sample_value,
                      shadow_value,
                      baseline,
                      focal_length,
                      width,
                      height,
                      output,
                      line_step);
}

} // namespace pcl::io::rvv_image_depth_support
