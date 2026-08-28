#pragma once

/*
 * 本文件做什么：
 * 这里放 color_gradient_modality 首阶段的 scalar reference（标量参考链路）
 * 和 RVV candidate（RVV 候选链路）。两条链路都只计算 production 中
 * computeMaxColorGradientsSobel() + quantizeColorGradients() 的内区像素，
 * 边框像素保持 0。
 *
 * 证据边界：
 * RVV candidate 是 production-shaped diagnostic（生产形态诊断）：输入、阈值
 * 和量化公式模拟 production，但还没有接入 ColorGradientModality public entry
 *（公开入口）。它能证明 Sobel+quantize 子链路的正确性、反汇编和板卡性能信号，
 * 不能替代真实 production evidence（生产证据）。
 */

#include <pcl/point_types.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/common.h>
#include <riscv_vector.h>
#endif

namespace pcl::recognition::rvv_test::color_gradient_modality
{
enum class ExecutionPath
{
  ScalarFallback,
  RvvSobelQuantize,
  RvvDominantFilter,
  RvvFullChain,
  RvvFullStencilChain
};

struct GradientCell
{
  float magnitude = 0.0f;
  float angle_degrees = 0.0f;
  std::uint8_t quantized = 0;
};

inline std::uint8_t
quantizeGradient(const float magnitude, const float angle_degrees, const float magnitude_threshold)
{
  if (magnitude < magnitude_threshold)
    return 0;

  constexpr float angle_scale = 16.0f / 360.0f;
  const float angle = 11.25f + angle_degrees + 180.0f;
  const int quantized_value = static_cast<int>(angle * angle_scale) & 7;
  return static_cast<std::uint8_t>(quantized_value + 1);
}

inline void
computeSobelQuantizedScalar(const pcl::RGB* input,
                            std::size_t width,
                            std::size_t height,
                            float magnitude_threshold,
                            std::vector<GradientCell>& output)
{
  output.assign(width * height, {});
  if (width < 3 || height < 3)
    return;

  const float pi = std::tanf(1.0f) * 2.0f;
  for (std::size_t row = 1; row + 1 < height; ++row) {
    for (std::size_t col = 1; col + 1 < width; ++col) {
      const auto at = [&](const int dx, const int dy) -> const pcl::RGB& {
        return input[(row + static_cast<std::size_t>(dy)) * width + col + static_cast<std::size_t>(dx)];
      };

      const int r7 = static_cast<int>(at(-1, -1).r);
      const int g7 = static_cast<int>(at(-1, -1).g);
      const int b7 = static_cast<int>(at(-1, -1).b);
      const int r8 = static_cast<int>(at(0, -1).r);
      const int g8 = static_cast<int>(at(0, -1).g);
      const int b8 = static_cast<int>(at(0, -1).b);
      const int r9 = static_cast<int>(at(1, -1).r);
      const int g9 = static_cast<int>(at(1, -1).g);
      const int b9 = static_cast<int>(at(1, -1).b);
      const int r4 = static_cast<int>(at(-1, 0).r);
      const int g4 = static_cast<int>(at(-1, 0).g);
      const int b4 = static_cast<int>(at(-1, 0).b);
      const int r6 = static_cast<int>(at(1, 0).r);
      const int g6 = static_cast<int>(at(1, 0).g);
      const int b6 = static_cast<int>(at(1, 0).b);
      const int r1 = static_cast<int>(at(-1, 1).r);
      const int g1 = static_cast<int>(at(-1, 1).g);
      const int b1 = static_cast<int>(at(-1, 1).b);
      const int r2 = static_cast<int>(at(0, 1).r);
      const int g2 = static_cast<int>(at(0, 1).g);
      const int b2 = static_cast<int>(at(0, 1).b);
      const int r3 = static_cast<int>(at(1, 1).r);
      const int g3 = static_cast<int>(at(1, 1).g);
      const int b3 = static_cast<int>(at(1, 1).b);

      const int r_dx = r9 + 2 * r6 + r3 - (r7 + 2 * r4 + r1);
      const int r_dy = r1 + 2 * r2 + r3 - (r7 + 2 * r8 + r9);
      const int g_dx = g9 + 2 * g6 + g3 - (g7 + 2 * g4 + g1);
      const int g_dy = g1 + 2 * g2 + g3 - (g7 + 2 * g8 + g9);
      const int b_dx = b9 + 2 * b6 + b3 - (b7 + 2 * b4 + b1);
      const int b_dy = b1 + 2 * b2 + b3 - (b7 + 2 * b8 + b9);

      const int sqr_mag_r = r_dx * r_dx + r_dy * r_dy;
      const int sqr_mag_g = g_dx * g_dx + g_dy * g_dy;
      const int sqr_mag_b = b_dx * b_dx + b_dy * b_dy;

      int dx = b_dx;
      int dy = b_dy;
      int sqr_mag = sqr_mag_b;
      if (sqr_mag_r > sqr_mag_g && sqr_mag_r > sqr_mag_b) {
        dx = r_dx;
        dy = r_dy;
        sqr_mag = sqr_mag_r;
      }
      else if (sqr_mag_g > sqr_mag_b) {
        dx = g_dx;
        dy = g_dy;
        sqr_mag = sqr_mag_g;
      }

      GradientCell cell;
      cell.magnitude = std::sqrt(static_cast<float>(sqr_mag));
      cell.angle_degrees = std::atan2(static_cast<float>(dy), static_cast<float>(dx)) * 180.0f / pi;
      if (cell.angle_degrees < -180.0f)
        cell.angle_degrees += 360.0f;
      if (cell.angle_degrees >= 180.0f)
        cell.angle_degrees -= 360.0f;
      assert(cell.angle_degrees >= -180.0f && cell.angle_degrees <= 180.0f);
      cell.quantized = quantizeGradient(cell.magnitude, cell.angle_degrees, magnitude_threshold);
      output[row * width + col] = cell;
    }
  }
}

inline ExecutionPath
computeSobelQuantizedCandidate(const pcl::RGB* input,
                               std::size_t width,
                               std::size_t height,
                               float magnitude_threshold,
                               std::vector<GradientCell>& output)
{
#if defined(__RVV10__)
  output.assign(width * height, {});
  if (width < 3 || height < 3)
    return ExecutionPath::RvvSobelQuantize;

  std::vector<float> selected_dx(width * height, 0.0f);
  std::vector<float> selected_dy(width * height, 0.0f);
  std::vector<float> selected_sqr_mag(width * height, 0.0f);

  for (std::size_t row = 1; row + 1 < height; ++row) {
    for (std::size_t col = 1; col + 1 < width; ++col) {
      const auto at = [&](const int dx, const int dy) -> const pcl::RGB& {
        return input[(row + static_cast<std::size_t>(dy)) * width + col + static_cast<std::size_t>(dx)];
      };

      const int r7 = static_cast<int>(at(-1, -1).r);
      const int g7 = static_cast<int>(at(-1, -1).g);
      const int b7 = static_cast<int>(at(-1, -1).b);
      const int r8 = static_cast<int>(at(0, -1).r);
      const int g8 = static_cast<int>(at(0, -1).g);
      const int b8 = static_cast<int>(at(0, -1).b);
      const int r9 = static_cast<int>(at(1, -1).r);
      const int g9 = static_cast<int>(at(1, -1).g);
      const int b9 = static_cast<int>(at(1, -1).b);
      const int r4 = static_cast<int>(at(-1, 0).r);
      const int g4 = static_cast<int>(at(-1, 0).g);
      const int b4 = static_cast<int>(at(-1, 0).b);
      const int r6 = static_cast<int>(at(1, 0).r);
      const int g6 = static_cast<int>(at(1, 0).g);
      const int b6 = static_cast<int>(at(1, 0).b);
      const int r1 = static_cast<int>(at(-1, 1).r);
      const int g1 = static_cast<int>(at(-1, 1).g);
      const int b1 = static_cast<int>(at(-1, 1).b);
      const int r2 = static_cast<int>(at(0, 1).r);
      const int g2 = static_cast<int>(at(0, 1).g);
      const int b2 = static_cast<int>(at(0, 1).b);
      const int r3 = static_cast<int>(at(1, 1).r);
      const int g3 = static_cast<int>(at(1, 1).g);
      const int b3 = static_cast<int>(at(1, 1).b);

      const int r_dx = r9 + 2 * r6 + r3 - (r7 + 2 * r4 + r1);
      const int r_dy = r1 + 2 * r2 + r3 - (r7 + 2 * r8 + r9);
      const int g_dx = g9 + 2 * g6 + g3 - (g7 + 2 * g4 + g1);
      const int g_dy = g1 + 2 * g2 + g3 - (g7 + 2 * g8 + g9);
      const int b_dx = b9 + 2 * b6 + b3 - (b7 + 2 * b4 + b1);
      const int b_dy = b1 + 2 * b2 + b3 - (b7 + 2 * b8 + b9);

      const int sqr_mag_r = r_dx * r_dx + r_dy * r_dy;
      const int sqr_mag_g = g_dx * g_dx + g_dy * g_dy;
      const int sqr_mag_b = b_dx * b_dx + b_dy * b_dy;

      int dx = b_dx;
      int dy = b_dy;
      int sqr_mag = sqr_mag_b;
      if (sqr_mag_r > sqr_mag_g && sqr_mag_r > sqr_mag_b) {
        dx = r_dx;
        dy = r_dy;
        sqr_mag = sqr_mag_r;
      }
      else if (sqr_mag_g > sqr_mag_b) {
        dx = g_dx;
        dy = g_dy;
        sqr_mag = sqr_mag_g;
      }

      const auto index = row * width + col;
      selected_dx[index] = static_cast<float>(dx);
      selected_dy[index] = static_cast<float>(dy);
      selected_sqr_mag[index] = static_cast<float>(sqr_mag);
    }
  }

  constexpr float radians_to_degrees = 180.0f / (std::tanf(1.0f) * 2.0f);
  constexpr float angle_scale = 16.0f / 360.0f;
  const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<float> magnitudes(max_vl);
  std::vector<float> angles(max_vl);
  std::vector<std::int32_t> quantized_values(max_vl);
  for (std::size_t row = 1; row + 1 < height; ++row) {
    const std::size_t begin = row * width + 1;
    const std::size_t count = width - 2;
    for (std::size_t offset = 0; offset < count;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(count - offset);
      const std::size_t index = begin + offset;

      const vfloat32m2_t dx = __riscv_vle32_v_f32m2(selected_dx.data() + index, vl);
      const vfloat32m2_t dy = __riscv_vle32_v_f32m2(selected_dy.data() + index, vl);
      const vfloat32m2_t sqr_mag = __riscv_vle32_v_f32m2(selected_sqr_mag.data() + index, vl);
      const vfloat32m2_t magnitude = __riscv_vfsqrt_v_f32m2(sqr_mag, vl);
      vfloat32m2_t angle = pcl::atan2_RVV_f32m2(dy, dx, vl);
      angle = __riscv_vfmul_vf_f32m2(angle, radians_to_degrees, vl);

      const vbool16_t angle_lt_low = __riscv_vmflt_vf_f32m2_b16(angle, -180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2(
          angle, __riscv_vfadd_vf_f32m2(angle, 360.0f, vl), angle_lt_low, vl);
      const vbool16_t angle_ge_high = __riscv_vmfge_vf_f32m2_b16(angle, 180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2(
          angle, __riscv_vfsub_vf_f32m2(angle, 360.0f, vl), angle_ge_high, vl);

      vfloat32m2_t quantized_angle = __riscv_vfadd_vf_f32m2(angle, 191.25f, vl);
      quantized_angle = __riscv_vfmul_vf_f32m2(quantized_angle, angle_scale, vl);
      vint32m2_t quantized = __riscv_vfcvt_rtz_x_f_v_i32m2(quantized_angle, vl);
      quantized = __riscv_vand_vx_i32m2(quantized, 7, vl);
      quantized = __riscv_vadd_vx_i32m2(quantized, 1, vl);

      const vbool16_t below_threshold = __riscv_vmflt_vf_f32m2_b16(magnitude, magnitude_threshold, vl);
      quantized = __riscv_vmerge_vxm_i32m2(quantized, 0, below_threshold, vl);

      __riscv_vse32_v_f32m2(magnitudes.data(), magnitude, vl);
      __riscv_vse32_v_f32m2(angles.data(), angle, vl);
      __riscv_vse32_v_i32m2(quantized_values.data(), quantized, vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        auto& cell = output[index + lane];
        cell.magnitude = magnitudes[lane];
        cell.angle_degrees = angles[lane];
        cell.quantized = static_cast<std::uint8_t>(quantized_values[lane]);
      }

      offset += vl;
    }
  }
  return ExecutionPath::RvvSobelQuantize;
#else
  computeSobelQuantizedScalar(input, width, height, magnitude_threshold, output);
  return ExecutionPath::ScalarFallback;
#endif
}

inline void
filterQuantizedGradientsScalar(const std::uint8_t* input,
                               std::size_t width,
                               std::size_t height,
                               std::vector<std::uint8_t>& output)
{
  output.assign(width * height, 0);
  if (width < 3 || height < 3)
    return;

  for (std::size_t row = 1; row + 1 < height; ++row) {
    for (std::size_t col = 1; col + 1 < width; ++col) {
      std::uint8_t histogram[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
      for (std::size_t dy = row - 1; dy <= row + 1; ++dy) {
        const std::uint8_t* row_ptr = input + dy * width + col - 1;
        assert(row_ptr[0] < 9 && row_ptr[1] < 9 && row_ptr[2] < 9);
        ++histogram[row_ptr[0]];
        ++histogram[row_ptr[1]];
        ++histogram[row_ptr[2]];
      }

      std::uint8_t max_hist_value = 0;
      int max_hist_index = -1;
      for (int bin = 1; bin <= 8; ++bin) {
        if (max_hist_value < histogram[bin]) {
          max_hist_index = bin - 1;
          max_hist_value = histogram[bin];
        }
      }

      if (max_hist_index != -1 && max_hist_value >= 5)
        output[row * width + col] = static_cast<std::uint8_t>(1u << max_hist_index);
    }
  }
}

inline ExecutionPath
filterQuantizedGradientsCandidate(const std::uint8_t* input,
                                  std::size_t width,
                                  std::size_t height,
                                  std::vector<std::uint8_t>& output)
{
#if defined(__RVV10__)
  output.assign(width * height, 0);
  if (width < 3 || height < 3)
    return ExecutionPath::RvvDominantFilter;

  for (std::size_t row = 1; row + 1 < height; ++row) {
    const std::size_t count = width - 2;
    for (std::size_t offset = 0; offset < count;) {
      const std::size_t vl = __riscv_vsetvl_e8m2(count - offset);
      const std::size_t col = 1 + offset;
      const std::uint8_t* prev = input + (row - 1) * width + col - 1;
      const std::uint8_t* curr = input + row * width + col - 1;
      const std::uint8_t* next = input + (row + 1) * width + col - 1;

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

      for (int bin = 1; bin <= 8; ++bin) {
        vuint8m2_t bin_count = __riscv_vmv_v_x_u8m2(0, vl);
        const auto add_match = [&](const vuint8m2_t values, vuint8m2_t counts) {
          const vbool4_t match = __riscv_vmseq_vx_u8m2_b4(values, static_cast<unsigned long>(bin), vl);
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
        output_bits =
            __riscv_vmerge_vxm_u8m2(output_bits, static_cast<unsigned long>(1u << (bin - 1)), new_max, vl);
      }

      const vbool4_t below_threshold = __riscv_vmsltu_vx_u8m2_b4(max_count, 5, vl);
      output_bits = __riscv_vmerge_vxm_u8m2(output_bits, 0, below_threshold, vl);
      __riscv_vse8_v_u8m2(output.data() + row * width + col, output_bits, vl);
      offset += vl;
    }
  }
  return ExecutionPath::RvvDominantFilter;
#else
  filterQuantizedGradientsScalar(input, width, height, output);
  return ExecutionPath::ScalarFallback;
#endif
}

inline void
computeSobelQuantizedFilteredScalar(const pcl::RGB* input,
                                    std::size_t width,
                                    std::size_t height,
                                    float magnitude_threshold,
                                    std::vector<std::uint8_t>& output);

inline ExecutionPath
computeSobelQuantizedStencilCandidate(const pcl::RGB* input,
                                      std::size_t width,
                                      std::size_t height,
                                      float magnitude_threshold,
                                      std::vector<std::uint8_t>& output)
{
#if defined(__RVV10__)
  std::vector<std::uint8_t> quantized(width * height, 0);
  if (width < 3 || height < 3) {
    output.assign(width * height, 0);
    return ExecutionPath::RvvFullStencilChain;
  }

  constexpr float radians_to_degrees = 180.0f / (std::tanf(1.0f) * 2.0f);
  constexpr float negative_half_pi_degrees = -1.57079632679489661923f * radians_to_degrees;
  constexpr float angle_scale = 16.0f / 360.0f;

  const auto* input_bytes = reinterpret_cast<const std::uint8_t*>(input);
  const auto* first_point = reinterpret_cast<const std::uint8_t*>(&input[0]);
  const std::ptrdiff_t b_offset = reinterpret_cast<const std::uint8_t*>(&input[0].b) - first_point;
  const std::ptrdiff_t g_offset = reinterpret_cast<const std::uint8_t*>(&input[0].g) - first_point;
  const std::ptrdiff_t r_offset = reinterpret_cast<const std::uint8_t*>(&input[0].r) - first_point;
  const std::ptrdiff_t point_stride = static_cast<std::ptrdiff_t>(sizeof(pcl::RGB));

  const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<std::int32_t> quantized_values(max_vl);

  const auto load_channel = [input_bytes, width, point_stride](
                                const std::size_t row,
                                const std::size_t col,
                                const std::ptrdiff_t channel_offset,
                                const std::size_t vl) {
    const auto* ptr =
        input_bytes + (row * width + col) * sizeof(pcl::RGB) + channel_offset;
    const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2(ptr, point_stride, vl);
    const vuint16m1_t u16 = __riscv_vzext_vf2_u16m1(u8, vl);
    return __riscv_vreinterpret_v_u32m2_i32m2(__riscv_vzext_vf2_u32m2(u16, vl));
  };

  const auto sobel_dx = [](const vint32m2_t p7,
                           const vint32m2_t p4,
                           const vint32m2_t p1,
                           const vint32m2_t p9,
                           const vint32m2_t p6,
                           const vint32m2_t p3,
                           const std::size_t vl) {
    const vint32m2_t right =
        __riscv_vadd_vv_i32m2(__riscv_vadd_vv_i32m2(p9, __riscv_vmul_vx_i32m2(p6, 2, vl), vl), p3, vl);
    const vint32m2_t left =
        __riscv_vadd_vv_i32m2(__riscv_vadd_vv_i32m2(p7, __riscv_vmul_vx_i32m2(p4, 2, vl), vl), p1, vl);
    return __riscv_vsub_vv_i32m2(right, left, vl);
  };

  const auto sobel_dy = [](const vint32m2_t p7,
                           const vint32m2_t p8,
                           const vint32m2_t p9,
                           const vint32m2_t p1,
                           const vint32m2_t p2,
                           const vint32m2_t p3,
                           const std::size_t vl) {
    const vint32m2_t down =
        __riscv_vadd_vv_i32m2(__riscv_vadd_vv_i32m2(p1, __riscv_vmul_vx_i32m2(p2, 2, vl), vl), p3, vl);
    const vint32m2_t up =
        __riscv_vadd_vv_i32m2(__riscv_vadd_vv_i32m2(p7, __riscv_vmul_vx_i32m2(p8, 2, vl), vl), p9, vl);
    return __riscv_vsub_vv_i32m2(down, up, vl);
  };

  const auto sqr_magnitude = [](const vint32m2_t dx, const vint32m2_t dy, const std::size_t vl) {
    return __riscv_vadd_vv_i32m2(__riscv_vmul_vv_i32m2(dx, dx, vl),
                                 __riscv_vmul_vv_i32m2(dy, dy, vl),
                                 vl);
  };

  for (std::size_t row = 1; row + 1 < height; ++row) {
    const std::size_t count = width - 2;
    for (std::size_t offset = 0; offset < count;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(count - offset);
      const std::size_t col = 1 + offset;

      const auto compute_channel = [&](const std::ptrdiff_t channel_offset,
                                       vint32m2_t& dx,
                                       vint32m2_t& dy,
                                       vint32m2_t& sqr_mag) {
        const vint32m2_t p7 = load_channel(row - 1, col - 1, channel_offset, vl);
        const vint32m2_t p8 = load_channel(row - 1, col, channel_offset, vl);
        const vint32m2_t p9 = load_channel(row - 1, col + 1, channel_offset, vl);
        const vint32m2_t p4 = load_channel(row, col - 1, channel_offset, vl);
        const vint32m2_t p6 = load_channel(row, col + 1, channel_offset, vl);
        const vint32m2_t p1 = load_channel(row + 1, col - 1, channel_offset, vl);
        const vint32m2_t p2 = load_channel(row + 1, col, channel_offset, vl);
        const vint32m2_t p3 = load_channel(row + 1, col + 1, channel_offset, vl);
        dx = sobel_dx(p7, p4, p1, p9, p6, p3, vl);
        dy = sobel_dy(p7, p8, p9, p1, p2, p3, vl);
        sqr_mag = sqr_magnitude(dx, dy, vl);
      };

      vint32m2_t r_dx;
      vint32m2_t r_dy;
      vint32m2_t sqr_mag_r;
      vint32m2_t g_dx;
      vint32m2_t g_dy;
      vint32m2_t sqr_mag_g;
      vint32m2_t b_dx;
      vint32m2_t b_dy;
      vint32m2_t sqr_mag_b;
      compute_channel(r_offset, r_dx, r_dy, sqr_mag_r);
      compute_channel(g_offset, g_dx, g_dy, sqr_mag_g);
      compute_channel(b_offset, b_dx, b_dy, sqr_mag_b);

      const vbool16_t r_selected =
          __riscv_vmand_mm_b16(__riscv_vmsgt_vv_i32m2_b16(sqr_mag_r, sqr_mag_g, vl),
                               __riscv_vmsgt_vv_i32m2_b16(sqr_mag_r, sqr_mag_b, vl),
                               vl);
      const vbool16_t g_selected =
          __riscv_vmand_mm_b16(__riscv_vmnot_m_b16(r_selected, vl),
                               __riscv_vmsgt_vv_i32m2_b16(sqr_mag_g, sqr_mag_b, vl),
                               vl);

      vint32m2_t selected_dx = b_dx;
      vint32m2_t selected_dy = b_dy;
      vint32m2_t selected_sqr_mag = sqr_mag_b;
      selected_dx = __riscv_vmerge_vvm_i32m2(selected_dx, g_dx, g_selected, vl);
      selected_dy = __riscv_vmerge_vvm_i32m2(selected_dy, g_dy, g_selected, vl);
      selected_sqr_mag = __riscv_vmerge_vvm_i32m2(selected_sqr_mag, sqr_mag_g, g_selected, vl);
      selected_dx = __riscv_vmerge_vvm_i32m2(selected_dx, r_dx, r_selected, vl);
      selected_dy = __riscv_vmerge_vvm_i32m2(selected_dy, r_dy, r_selected, vl);
      selected_sqr_mag = __riscv_vmerge_vvm_i32m2(selected_sqr_mag, sqr_mag_r, r_selected, vl);

      const vfloat32m2_t dx = __riscv_vfcvt_f_x_v_f32m2(selected_dx, vl);
      const vfloat32m2_t dy = __riscv_vfcvt_f_x_v_f32m2(selected_dy, vl);
      const vfloat32m2_t sqr_mag = __riscv_vfcvt_f_x_v_f32m2(selected_sqr_mag, vl);
      const vfloat32m2_t magnitude = __riscv_vfsqrt_v_f32m2(sqr_mag, vl);
      vfloat32m2_t angle = pcl::atan2_RVV_f32m2(dy, dx, vl);
      angle = __riscv_vfmul_vf_f32m2(angle, radians_to_degrees, vl);
      const vbool16_t negative_y_axis =
          __riscv_vmand_mm_b16(__riscv_vmfeq_vf_f32m2_b16(dx, 0.0f, vl),
                               __riscv_vmflt_vf_f32m2_b16(dy, 0.0f, vl),
                               vl);
      angle = __riscv_vmerge_vvm_f32m2(
          angle, __riscv_vfmv_v_f_f32m2(negative_half_pi_degrees, vl), negative_y_axis, vl);

      const vbool16_t angle_lt_low = __riscv_vmflt_vf_f32m2_b16(angle, -180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2(
          angle, __riscv_vfadd_vf_f32m2(angle, 360.0f, vl), angle_lt_low, vl);
      const vbool16_t angle_ge_high = __riscv_vmfge_vf_f32m2_b16(angle, 180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2(
          angle, __riscv_vfsub_vf_f32m2(angle, 360.0f, vl), angle_ge_high, vl);

      vfloat32m2_t quantized_angle = __riscv_vfadd_vf_f32m2(angle, 191.25f, vl);
      quantized_angle = __riscv_vfmul_vf_f32m2(quantized_angle, angle_scale, vl);
      vint32m2_t quantized_bins = __riscv_vfcvt_rtz_x_f_v_i32m2(quantized_angle, vl);
      quantized_bins = __riscv_vand_vx_i32m2(quantized_bins, 7, vl);
      quantized_bins = __riscv_vadd_vx_i32m2(quantized_bins, 1, vl);

      const vbool16_t below_threshold =
          __riscv_vmflt_vf_f32m2_b16(magnitude, magnitude_threshold, vl);
      quantized_bins = __riscv_vmerge_vxm_i32m2(quantized_bins, 0, below_threshold, vl);
      __riscv_vse32_v_i32m2(quantized_values.data(), quantized_bins, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        quantized[row * width + col + lane] =
            static_cast<std::uint8_t>(quantized_values[lane]);

      offset += vl;
    }
  }

  filterQuantizedGradientsCandidate(quantized.data(), width, height, output);
  return ExecutionPath::RvvFullStencilChain;
#else
  computeSobelQuantizedFilteredScalar(input, width, height, magnitude_threshold, output);
  return ExecutionPath::ScalarFallback;
#endif
}

inline void
computeSobelQuantizedFilteredScalar(const pcl::RGB* input,
                                    std::size_t width,
                                    std::size_t height,
                                    float magnitude_threshold,
                                    std::vector<std::uint8_t>& output)
{
  std::vector<GradientCell> gradients;
  computeSobelQuantizedScalar(input, width, height, magnitude_threshold, gradients);

  std::vector<std::uint8_t> quantized(width * height, 0);
  for (std::size_t i = 0; i < gradients.size(); ++i)
    quantized[i] = gradients[i].quantized;

  filterQuantizedGradientsScalar(quantized.data(), width, height, output);
}

inline ExecutionPath
computeSobelQuantizedFilteredCandidate(const pcl::RGB* input,
                                       std::size_t width,
                                       std::size_t height,
                                       float magnitude_threshold,
                                       std::vector<std::uint8_t>& output)
{
#if defined(__RVV10__)
  std::vector<GradientCell> gradients;
  computeSobelQuantizedCandidate(input, width, height, magnitude_threshold, gradients);

  std::vector<std::uint8_t> quantized(width * height, 0);
  for (std::size_t i = 0; i < gradients.size(); ++i)
    quantized[i] = gradients[i].quantized;

  filterQuantizedGradientsCandidate(quantized.data(), width, height, output);
  return ExecutionPath::RvvFullChain;
#else
  computeSobelQuantizedFilteredScalar(input, width, height, magnitude_threshold, output);
  return ExecutionPath::ScalarFallback;
#endif
}
} // namespace pcl::recognition::rvv_test::color_gradient_modality
