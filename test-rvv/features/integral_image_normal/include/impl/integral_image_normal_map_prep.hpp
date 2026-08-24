#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::integral_image_normal
{
namespace detail
{
constexpr std::ptrdiff_t kFloatBytes = static_cast<std::ptrdiff_t>(sizeof(float));
constexpr std::ptrdiff_t kXYZPadPointStrideBytes = 4 * kFloatBytes;

inline bool
isFiniteFloat(float value)
{
  return std::isfinite(value);
}

inline void
buildDepthChangeMapScalar(const float* z,
                          std::size_t width,
                          std::size_t height,
                          float max_depth_change_factor,
                          unsigned char* depth_change_map)
{
  std::fill_n(depth_change_map, width * height, static_cast<unsigned char>(255));
  if (width == 0 || height == 0)
    return;

  for (std::size_t ri = 0; ri + 1 < height; ++ri)
  {
    for (std::size_t ci = 0; ci + 1 < width; ++ci)
    {
      const std::size_t index = ri * width + ci;
      const float depth = z[index];
      const float depth_r = z[index + 1];
      const float depth_d = z[index + width];
      const float threshold = max_depth_change_factor * (std::abs(depth) + 1.0f) * 2.0f;

      if (std::fabs(depth - depth_r) > threshold || !isFiniteFloat(depth) || !isFiniteFloat(depth_r))
      {
        depth_change_map[index] = 0;
        depth_change_map[index + 1] = 0;
      }
      if (std::fabs(depth - depth_d) > threshold || !isFiniteFloat(depth) || !isFiniteFloat(depth_d))
      {
        depth_change_map[index] = 0;
        depth_change_map[index + width] = 0;
      }
    }
  }
}

template <typename PointT>
inline void
buildAverage3DGradientDiffBuffersScalar(const PointT* points,
                                        std::size_t width,
                                        std::size_t height,
                                        float* diff_x,
                                        float* diff_y)
{
  const std::size_t output_count = width * height * 4;
  std::fill_n(diff_x, output_count, 0.0f);
  std::fill_n(diff_y, output_count, 0.0f);
  if (width < 3 || height < 3)
    return;

  for (std::size_t row = 1; row + 1 < height; ++row)
  {
    for (std::size_t col = 1; col + 1 < width; ++col)
    {
      const PointT& left = points[row * width + (col - 1)];
      const PointT& right = points[row * width + (col + 1)];
      const PointT& up = points[(row - 1) * width + col];
      const PointT& down = points[(row + 1) * width + col];
      const std::size_t out = (row * width + col) * 4;

      diff_x[out + 0] = right.x - left.x;
      diff_x[out + 1] = right.y - left.y;
      diff_x[out + 2] = right.z - left.z;

      diff_y[out + 0] = down.x - up.x;
      diff_y[out + 1] = down.y - up.y;
      diff_y[out + 2] = down.z - up.z;
    }
  }
}

#ifdef __RVV10__
inline vbool32_t
finiteMask(vfloat32m1_t value, std::size_t vl)
{
  const vfloat32m1_t abs_value = __riscv_vfabs_v_f32m1(value, vl);
  return __riscv_vmfle_vf_f32m1_b32(abs_value, std::numeric_limits<float>::max(), vl);
}
#endif
} // namespace detail

inline void
buildDepthChangeMapRVV(const float* z,
                       std::size_t width,
                       std::size_t height,
                       float max_depth_change_factor,
                       unsigned char* depth_change_map)
{
#ifdef __RVV10__
  std::fill_n(depth_change_map, width * height, static_cast<unsigned char>(255));
  if (width == 0 || height == 0)
    return;

  const float threshold_scale = max_depth_change_factor * 2.0f;
  for (std::size_t ri = 0; ri + 1 < height; ++ri)
  {
    const std::size_t row = ri * width;
    std::size_t ci = 0;
    while (ci + 1 < width)
    {
      const std::size_t remaining = width - 1 - ci;
      const std::size_t vl = __riscv_vsetvl_e32m1(remaining);
      const std::size_t index = row + ci;
      const vuint8mf4_t zero_u8 = __riscv_vmv_v_x_u8mf4(0, vl);

      const vfloat32m1_t depth = __riscv_vle32_v_f32m1(z + index, vl);
      const vfloat32m1_t depth_r = __riscv_vle32_v_f32m1(z + index + 1, vl);
      const vfloat32m1_t depth_d = __riscv_vle32_v_f32m1(z + index + width, vl);
      const vfloat32m1_t threshold =
          __riscv_vfmul_vf_f32m1(__riscv_vfadd_vf_f32m1(__riscv_vfabs_v_f32m1(depth, vl), 1.0f, vl),
                                  threshold_scale,
                                  vl);

      const vbool32_t finite_depth = detail::finiteMask(depth, vl);
      const vbool32_t finite_r = detail::finiteMask(depth_r, vl);
      const vbool32_t finite_d = detail::finiteMask(depth_d, vl);
      const vbool32_t invalid_r =
          __riscv_vmnot_m_b32(__riscv_vmand_mm_b32(finite_depth, finite_r, vl), vl);
      const vbool32_t invalid_d =
          __riscv_vmnot_m_b32(__riscv_vmand_mm_b32(finite_depth, finite_d, vl), vl);
      const vbool32_t edge_r =
          __riscv_vmor_mm_b32(__riscv_vmfgt_vv_f32m1_b32(__riscv_vfabs_v_f32m1(__riscv_vfsub_vv_f32m1(depth, depth_r, vl), vl),
                                                         threshold,
                                                         vl),
                              invalid_r,
                              vl);
      const vbool32_t edge_d =
          __riscv_vmor_mm_b32(__riscv_vmfgt_vv_f32m1_b32(__riscv_vfabs_v_f32m1(__riscv_vfsub_vv_f32m1(depth, depth_d, vl), vl),
                                                         threshold,
                                                         vl),
                              invalid_d,
                              vl);
      const vbool32_t edge_any = __riscv_vmor_mm_b32(edge_r, edge_d, vl);

      __riscv_vse8_v_u8mf4_m(edge_any, depth_change_map + index, zero_u8, vl);
      __riscv_vse8_v_u8mf4_m(edge_r, depth_change_map + index + 1, zero_u8, vl);
      __riscv_vse8_v_u8mf4_m(edge_d, depth_change_map + index + width, zero_u8, vl);

      ci += vl;
    }
  }
#else
  detail::buildDepthChangeMapScalar(z, width, height, max_depth_change_factor, depth_change_map);
#endif
}

inline void
initializeDistanceMapRVV(const unsigned char* depth_change_map,
                         std::size_t count,
                         float far_distance,
                         float* distance_map)
{
#ifdef __RVV10__
  std::size_t i = 0;
  while (i < count)
  {
    const std::size_t vl = __riscv_vsetvl_e8mf4(count - i);
    const vuint8mf4_t map = __riscv_vle8_v_u8mf4(depth_change_map + i, vl);
    const vbool32_t is_zero = __riscv_vmseq_vx_u8mf4_b32(map, 0, vl);
    const vfloat32m1_t far_values = __riscv_vfmv_v_f_f32m1(far_distance, vl);
    const vfloat32m1_t zero_values = __riscv_vfmv_v_f_f32m1(0.0f, vl);
    const vfloat32m1_t selected = __riscv_vmerge_vvm_f32m1(far_values, zero_values, is_zero, vl);
    __riscv_vse32_v_f32m1(distance_map + i, selected, vl);
    i += vl;
  }
#else
  for (std::size_t i = 0; i < count; ++i)
    distance_map[i] = depth_change_map[i] == 0 ? 0.0f : far_distance;
#endif
}

template <typename PointT>
inline void
buildAverage3DGradientDiffBuffersRVV(const PointT* points,
                                     std::size_t width,
                                     std::size_t height,
                                     float* diff_x,
                                     float* diff_y)
{
  static_assert(sizeof(PointT) == 4 * sizeof(float),
                "This diagnostic helper only covers PointXYZ-like test points with 4-float stride.");
#ifdef __RVV10__
  const std::size_t output_count = width * height * 4;
  std::fill_n(diff_x, output_count, 0.0f);
  std::fill_n(diff_y, output_count, 0.0f);
  if (width < 3 || height < 3)
    return;

  const auto* xyz = reinterpret_cast<const float*>(points);
  for (std::size_t row = 1; row + 1 < height; ++row)
  {
    std::size_t col = 1;
    while (col + 1 < width)
    {
      const std::size_t remaining = width - 1 - col;
      const std::size_t vl = __riscv_vsetvl_e32m1(remaining);
      const std::size_t center = row * width + col;
      const float* left = xyz + ((row * width + (col - 1)) * 4);
      const float* right = xyz + ((row * width + (col + 1)) * 4);
      const float* up = xyz + (((row - 1) * width + col) * 4);
      const float* down = xyz + (((row + 1) * width + col) * 4);
      float* out_x = diff_x + center * 4;
      float* out_y = diff_y + center * 4;

      const vfloat32m1_t left_x =
          __riscv_vlse32_v_f32m1(left + 0, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t right_x =
          __riscv_vlse32_v_f32m1(right + 0, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t left_y =
          __riscv_vlse32_v_f32m1(left + 1, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t right_y =
          __riscv_vlse32_v_f32m1(right + 1, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t left_z =
          __riscv_vlse32_v_f32m1(left + 2, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t right_z =
          __riscv_vlse32_v_f32m1(right + 2, detail::kXYZPadPointStrideBytes, vl);

      __riscv_vsse32_v_f32m1(out_x + 0,
                             detail::kXYZPadPointStrideBytes,
                             __riscv_vfsub_vv_f32m1(right_x, left_x, vl),
                             vl);
      __riscv_vsse32_v_f32m1(out_x + 1,
                             detail::kXYZPadPointStrideBytes,
                             __riscv_vfsub_vv_f32m1(right_y, left_y, vl),
                             vl);
      __riscv_vsse32_v_f32m1(out_x + 2,
                             detail::kXYZPadPointStrideBytes,
                             __riscv_vfsub_vv_f32m1(right_z, left_z, vl),
                             vl);

      const vfloat32m1_t up_x =
          __riscv_vlse32_v_f32m1(up + 0, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t down_x =
          __riscv_vlse32_v_f32m1(down + 0, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t up_y =
          __riscv_vlse32_v_f32m1(up + 1, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t down_y =
          __riscv_vlse32_v_f32m1(down + 1, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t up_z =
          __riscv_vlse32_v_f32m1(up + 2, detail::kXYZPadPointStrideBytes, vl);
      const vfloat32m1_t down_z =
          __riscv_vlse32_v_f32m1(down + 2, detail::kXYZPadPointStrideBytes, vl);

      __riscv_vsse32_v_f32m1(out_y + 0,
                             detail::kXYZPadPointStrideBytes,
                             __riscv_vfsub_vv_f32m1(down_x, up_x, vl),
                             vl);
      __riscv_vsse32_v_f32m1(out_y + 1,
                             detail::kXYZPadPointStrideBytes,
                             __riscv_vfsub_vv_f32m1(down_y, up_y, vl),
                             vl);
      __riscv_vsse32_v_f32m1(out_y + 2,
                             detail::kXYZPadPointStrideBytes,
                             __riscv_vfsub_vv_f32m1(down_z, up_z, vl),
                             vl);

      col += vl;
    }
  }
#else
  detail::buildAverage3DGradientDiffBuffersScalar(points, width, height, diff_x, diff_y);
#endif
}
} // namespace pcl::features::rvv_test::integral_image_normal
