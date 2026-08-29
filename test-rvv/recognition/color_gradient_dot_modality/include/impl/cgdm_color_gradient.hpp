#pragma once

/*
 * 本文件做什么：
 * 这里放 color_gradient_dot_modality 首阶段的 scalar reference（标量参考链路）
 * 和 RVV candidate（RVV 候选链路）。两条链路都只复刻 production 中
 * computeMaxColorGradients() + computeDominantQuantizedGradients() 的主流程：
 * 先生成逐像素 GradientXY，再把每个 bin_size x bin_size 区域压成 dominant map。
 *
 * 证据边界：
 * RVV candidate 当前仍是 test-only stub（测试专用占位），用于把 Phase 000 的
 * RED 测试卡住，提醒 worker 还没有真正把 RVV 路径接上。它可以证明 helper 语义、
 * correctness 和后续 asm / board 的证据边界，但不能替代 production evidence（生产证据）。
 */

#include <pcl/recognition/color_gradient_dot_modality.h>
#include <pcl/recognition/dotmod.h>
#include <pcl/recognition/mask_map.h>
#include <pcl/recognition/region_xy.h>
#include <pcl/common/common.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pcl::recognition::rvv_test::color_gradient_dot_modality
{
enum class ExecutionPath
{
  ScalarFallback = 0,
  RvvGradientDominant = 1,
  RvvInvariantMap = 2
};

struct DominantMapArtifacts
{
  std::vector<pcl::GradientXY> gradients;
  pcl::QuantizedMap dominant_map;
};

inline void
computeMaxColorGradientsScalar(const pcl::PointXYZRGB* input,
                               const std::size_t width,
                               const std::size_t height,
                               std::vector<pcl::GradientXY>& gradients)
{
  gradients.assign(width * height, {});
  if (width < 3 || height < 3)
    return;

  constexpr float pi = std::tan(1.0f) * 4.0f;
  for (std::size_t row = 0; row + 2 < height; ++row)
  {
    for (std::size_t col = 0; col + 2 < width; ++col)
    {
      const auto at = [&](const std::size_t dx, const std::size_t dy) -> const pcl::PointXYZRGB& {
        return input[(row + dy) * width + col + dx];
      };

      const int r0 = static_cast<int>(at(0, 0).r);
      const int g0 = static_cast<int>(at(0, 0).g);
      const int b0 = static_cast<int>(at(0, 0).b);
      const int rc = static_cast<int>(at(2, 0).r);
      const int gc = static_cast<int>(at(2, 0).g);
      const int bc = static_cast<int>(at(2, 0).b);
      const int rr = static_cast<int>(at(0, 2).r);
      const int gr = static_cast<int>(at(0, 2).g);
      const int br = static_cast<int>(at(0, 2).b);

      const int r_dx = rc - r0;
      const int g_dx = gc - g0;
      const int b_dx = bc - b0;
      const int r_dy = rr - r0;
      const int g_dy = gr - g0;
      const int b_dy = br - b0;

      const int sqr_mag_r = r_dx * r_dx + r_dy * r_dy;
      const int sqr_mag_g = g_dx * g_dx + g_dy * g_dy;
      const int sqr_mag_b = b_dx * b_dx + b_dy * b_dy;

      pcl::GradientXY gradient;
      gradient.x = static_cast<float>(col);
      gradient.y = static_cast<float>(row);
      if (sqr_mag_r > sqr_mag_g && sqr_mag_r > sqr_mag_b)
      {
        gradient.magnitude = std::sqrt(static_cast<float>(sqr_mag_r));
        gradient.angle = std::atan2(static_cast<float>(r_dy), static_cast<float>(r_dx)) * 180.0f / pi;
      }
      else if (sqr_mag_g > sqr_mag_b)
      {
        gradient.magnitude = std::sqrt(static_cast<float>(sqr_mag_g));
        gradient.angle = std::atan2(static_cast<float>(g_dy), static_cast<float>(g_dx)) * 180.0f / pi;
      }
      else
      {
        gradient.magnitude = std::sqrt(static_cast<float>(sqr_mag_b));
        gradient.angle = std::atan2(static_cast<float>(b_dy), static_cast<float>(b_dx)) * 180.0f / pi;
      }

      assert(gradient.angle >= -180.0f && gradient.angle <= 180.0f);
      gradients[(row + 1) * width + (col + 1)] = gradient;
    }
  }
}

#if defined(__RVV10__) && defined(__riscv_vector)
inline void
computeMaxColorGradientsRvv(const pcl::PointXYZRGB* input,
                            const std::size_t width,
                            const std::size_t height,
                            std::vector<pcl::GradientXY>& gradients)
{
  gradients.assign(width * height, {});
  if (width < 3 || height < 3)
    return;

  constexpr float pi = std::tan(1.0f) * 4.0f;
  constexpr float radians_to_degrees = 180.0f / pi;
  constexpr float negative_half_pi_degrees = -1.57079632679489661923f * radians_to_degrees;
  const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<float> magnitudes(max_vl);
  std::vector<float> angles(max_vl);
  const auto* input_bytes = reinterpret_cast<const std::uint8_t*>(input);
  const auto* first_point = reinterpret_cast<const std::uint8_t*>(&input[0]);
  const std::ptrdiff_t r_offset = reinterpret_cast<const std::uint8_t*>(&input[0].r) - first_point;
  const std::ptrdiff_t g_offset = reinterpret_cast<const std::uint8_t*>(&input[0].g) - first_point;
  const std::ptrdiff_t b_offset = reinterpret_cast<const std::uint8_t*>(&input[0].b) - first_point;
  const std::ptrdiff_t point_stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZRGB));

  const auto load_channel = [input_bytes, width, point_stride](
                                const std::size_t row,
                                const std::size_t col,
                                const std::ptrdiff_t channel_offset,
                                const std::size_t vl) {
    const auto* ptr = input_bytes + (row * width + col) * sizeof(pcl::PointXYZRGB) + channel_offset;
    const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2(ptr, point_stride, vl);
    const vuint16m1_t u16 = __riscv_vzext_vf2_u16m1(u8, vl);
    return __riscv_vreinterpret_v_u32m2_i32m2(__riscv_vzext_vf2_u32m2(u16, vl));
  };

  const auto compute_channel = [&load_channel](const std::size_t row,
                                               const std::size_t col,
                                               const std::ptrdiff_t channel_offset,
                                               const std::size_t vl,
                                               vint32m2_t& dx,
                                               vint32m2_t& dy,
                                               vint32m2_t& sqr_mag) {
    const vint32m2_t center = load_channel(row, col, channel_offset, vl);
    dx = __riscv_vsub_vv_i32m2(load_channel(row, col + 2, channel_offset, vl), center, vl);
    dy = __riscv_vsub_vv_i32m2(load_channel(row + 2, col, channel_offset, vl), center, vl);
    sqr_mag = __riscv_vadd_vv_i32m2(
        __riscv_vmul_vv_i32m2(dx, dx, vl), __riscv_vmul_vv_i32m2(dy, dy, vl), vl);
  };

  for (std::size_t row = 0; row + 2 < height; ++row)
  {
    for (std::size_t col = 0; col + 2 < width;)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2(width - 2 - col);

      vint32m2_t vr_dx;
      vint32m2_t vr_dy;
      vint32m2_t vr_sqr_mag;
      vint32m2_t vg_dx;
      vint32m2_t vg_dy;
      vint32m2_t vg_sqr_mag;
      vint32m2_t vb_dx;
      vint32m2_t vb_dy;
      vint32m2_t vb_sqr_mag;
      compute_channel(row, col, r_offset, vl, vr_dx, vr_dy, vr_sqr_mag);
      compute_channel(row, col, g_offset, vl, vg_dx, vg_dy, vg_sqr_mag);
      compute_channel(row, col, b_offset, vl, vb_dx, vb_dy, vb_sqr_mag);

      const vbool16_t r_selected =
          __riscv_vmand_mm_b16(__riscv_vmsgt_vv_i32m2_b16(vr_sqr_mag, vg_sqr_mag, vl),
                               __riscv_vmsgt_vv_i32m2_b16(vr_sqr_mag, vb_sqr_mag, vl),
                               vl);
      const vbool16_t g_selected =
          __riscv_vmand_mm_b16(__riscv_vmnot_m_b16(r_selected, vl),
                               __riscv_vmsgt_vv_i32m2_b16(vg_sqr_mag, vb_sqr_mag, vl),
                               vl);

      vint32m2_t selected_dx = vb_dx;
      vint32m2_t selected_dy = vb_dy;
      vint32m2_t selected_sqr_mag = vb_sqr_mag;
      selected_dx = __riscv_vmerge_vvm_i32m2(selected_dx, vg_dx, g_selected, vl);
      selected_dy = __riscv_vmerge_vvm_i32m2(selected_dy, vg_dy, g_selected, vl);
      selected_sqr_mag = __riscv_vmerge_vvm_i32m2(selected_sqr_mag, vg_sqr_mag, g_selected, vl);
      selected_dx = __riscv_vmerge_vvm_i32m2(selected_dx, vr_dx, r_selected, vl);
      selected_dy = __riscv_vmerge_vvm_i32m2(selected_dy, vr_dy, r_selected, vl);
      selected_sqr_mag = __riscv_vmerge_vvm_i32m2(selected_sqr_mag, vr_sqr_mag, r_selected, vl);

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
      angle = __riscv_vmerge_vvm_f32m2(angle,
                                       __riscv_vfmv_v_f_f32m2(negative_half_pi_degrees, vl),
                                       negative_y_axis,
                                       vl);

      const vbool16_t angle_lt_low = __riscv_vmflt_vf_f32m2_b16(angle, -180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2(angle, __riscv_vfadd_vf_f32m2(angle, 360.0f, vl), angle_lt_low, vl);
      const vbool16_t angle_ge_high = __riscv_vmfge_vf_f32m2_b16(angle, 180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2(angle, __riscv_vfsub_vf_f32m2(angle, 360.0f, vl), angle_ge_high, vl);

      __riscv_vse32_v_f32m2(magnitudes.data(), magnitude, vl);
      __riscv_vse32_v_f32m2(angles.data(), angle, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
      {
        const std::size_t out_index = (row + 1) * width + (col + lane + 1);
        pcl::GradientXY gradient;
        gradient.x = static_cast<float>(col + lane);
        gradient.y = static_cast<float>(row);
        gradient.magnitude = magnitudes[lane];
        gradient.angle = angles[lane];
        gradients[out_index] = gradient;
      }

      col += vl;
    }
  }
}
#endif

inline void
computeDominantQuantizedGradientsScalar(const std::vector<pcl::GradientXY>& gradients,
                                        const std::size_t width,
                                        const std::size_t height,
                                        const std::size_t bin_size,
                                        const float gradient_magnitude_threshold,
                                        pcl::QuantizedMap& dominant_map)
{
  const std::size_t output_width = width / bin_size;
  const std::size_t output_height = height / bin_size;
  dominant_map.resize(output_width, output_height);
  std::fill_n(dominant_map.getData(), output_width * output_height, 0);

  constexpr std::size_t num_gradient_bins = 7;
  constexpr float divisor = 180.0f / (static_cast<float>(num_gradient_bins) - 1.0f);

  unsigned char* peak_pointer = dominant_map.getData();
  for (std::size_t row_bin_index = 0; row_bin_index < output_height; ++row_bin_index)
  {
    for (std::size_t col_bin_index = 0; col_bin_index < output_width; ++col_bin_index)
    {
      const std::size_t x_position = col_bin_index * bin_size;
      const std::size_t y_position = row_bin_index * bin_size;

      float max_gradient = 0.0f;
      std::size_t max_gradient_pos_x = 0;
      std::size_t max_gradient_pos_y = 0;
      for (std::size_t row_sub_index = 0; row_sub_index < bin_size; ++row_sub_index)
      {
        for (std::size_t col_sub_index = 0; col_sub_index < bin_size; ++col_sub_index)
        {
          const float magnitude =
              gradients[(col_sub_index + x_position) + width * (row_sub_index + y_position)].magnitude;
          if (magnitude > max_gradient)
          {
            max_gradient = magnitude;
            max_gradient_pos_x = col_sub_index;
            max_gradient_pos_y = row_sub_index;
          }
        }
      }

      if (max_gradient >= gradient_magnitude_threshold)
      {
        const std::size_t angle =
            static_cast<std::size_t>(180 + gradients[(max_gradient_pos_x + x_position) +
                                                     width * (max_gradient_pos_y + y_position)]
                                            .angle +
                                     0.5f);
        const std::size_t bin_index =
            static_cast<std::size_t>((angle >= 180 ? angle - 180 : angle) / divisor);
        *peak_pointer |= static_cast<unsigned char>(1u << bin_index);
      }

      if (*peak_pointer == 0)
        *peak_pointer |= static_cast<unsigned char>(1u << 7);
      ++peak_pointer;
    }
  }
}

inline void
computeDominantMapScalar(const pcl::PointXYZRGB* input,
                         const std::size_t width,
                         const std::size_t height,
                         const std::size_t bin_size,
                         const float gradient_magnitude_threshold,
                         DominantMapArtifacts& output)
{
  output.gradients.clear();
  computeMaxColorGradientsScalar(input, width, height, output.gradients);
  computeDominantQuantizedGradientsScalar(
      output.gradients, width, height, bin_size, gradient_magnitude_threshold, output.dominant_map);
}

inline ExecutionPath
computeDominantMapCandidate(const pcl::PointXYZRGB* input,
                            const std::size_t width,
                            const std::size_t height,
                            const std::size_t bin_size,
                            const float gradient_magnitude_threshold,
                            DominantMapArtifacts& output)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  computeMaxColorGradientsRvv(input, width, height, output.gradients);
  computeDominantQuantizedGradientsScalar(
      output.gradients, width, height, bin_size, gradient_magnitude_threshold, output.dominant_map);
  return ExecutionPath::RvvGradientDominant;
#else
  computeDominantMapScalar(input, width, height, bin_size, gradient_magnitude_threshold, output);
  return ExecutionPath::ScalarFallback;
#endif
}

inline ExecutionPath
computeInvariantQuantizedMapCandidate(const pcl::ColorGradientDOTModality<pcl::PointXYZRGB>&,
                                      const pcl::MaskMap&,
                                      const pcl::RegionXY&,
                                      pcl::QuantizedMap&)
{
  return ExecutionPath::ScalarFallback;
}
} // namespace pcl::recognition::rvv_test::color_gradient_dot_modality
