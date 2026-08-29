/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2013-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef PCL_TRAJKOVIC_KEYPOINT_2D_IMPL_H_
#define PCL_TRAJKOVIC_KEYPOINT_2D_IMPL_H_

#include <pcl/point_types.h>

#if defined(__RVV10__)
#include <pcl/common/intensity.h>
#include <pcl/rvv_point_traits.h>
#include <riscv_vector.h>
#include <type_traits>
#endif

namespace pcl
{

#if defined(__RVV10__)
namespace detail
{
template <typename PointInT, typename IntensityT>
inline constexpr bool kTrajkovic2DPointXYZIRVVGated =
    std::is_same_v<PointInT, pcl::PointXYZI> &&
    std::is_same_v<IntensityT, pcl::common::IntensityFieldAccessor<PointInT>> &&
    pcl::rvv::RVVFloatFieldLayout<PointInT, pcl::fields::intensity>::value;

template <typename PointInT>
inline vfloat32m2_t
trajkovic2DLoadIntensityRVV (const PointInT* points, const std::size_t offset, const std::size_t vl)
{
  constexpr std::size_t kIntensityOffset =
      pcl::traits::offset<PointInT, pcl::fields::intensity>::value;
  const auto* base = reinterpret_cast<const std::uint8_t*> (points + offset);
  const auto* intensity = reinterpret_cast<const float*> (base + kIntensityOffset);
  return __riscv_vlse32_v_f32m2 (intensity, static_cast<std::ptrdiff_t> (sizeof (PointInT)), vl);
}

inline vfloat32m2_t
trajkovic2DSquareRVV (const vfloat32m2_t value, const std::size_t vl)
{
  return __riscv_vfmul_vv_f32m2 (value, value, vl);
}

template <typename PointInT>
inline float
trajkovic2DIntensityAt (const PointInT* points, const int width, const int x, const int y)
{
  constexpr std::size_t kIntensityOffset =
      pcl::traits::offset<PointInT, pcl::fields::intensity>::value;
  const auto* base = reinterpret_cast<const std::uint8_t*> (points + y * width + x);
  return *reinterpret_cast<const float*> (base + kIntensityOffset);
}

template <typename PointInT>
inline bool
trajkovic2DScalarResponseAt (const PointInT* points,
                             const int width,
                             const int x,
                             const int y,
                             const float first_threshold,
                             const bool eight_corners,
                             float& value)
{
  const float center = trajkovic2DIntensityAt (points, width, x, y);
  const float up = trajkovic2DIntensityAt (points, width, x, y - 1);
  const float down = trajkovic2DIntensityAt (points, width, x, y + 1);
  const float left = trajkovic2DIntensityAt (points, width, x - 1, y);
  const float right = trajkovic2DIntensityAt (points, width, x + 1, y);

  const float up_center = up - center;
  float r0 = up_center * up_center;
  const float down_center = down - center;
  r0 += down_center * down_center;
  const float right_center = right - center;
  float r2 = right_center * right_center;
  const float left_center = left - center;
  r2 += left_center * left_center;

  if (!eight_corners)
  {
    const float d = std::min (r0, r2);
    if (d < first_threshold)
      return false;

    float b1 = (right - up) * up_center;
    b1 += (left - down) * down_center;
    float b2 = (right - down) * down_center;
    b2 += (left - up) * up_center;
    const float b = std::min (b1, b2);
    const float a = r2 - r0 - 2 * b;
    value = ((b < 0) && ((b + a) > 0)) ? r0 - ((b * b) / a) : d;
    return true;
  }

  const float upleft = trajkovic2DIntensityAt (points, width, x - 1, y - 1);
  const float upright = trajkovic2DIntensityAt (points, width, x + 1, y - 1);
  const float downleft = trajkovic2DIntensityAt (points, width, x - 1, y + 1);
  const float downright = trajkovic2DIntensityAt (points, width, x + 1, y + 1);
  const float upright_center = upright - center;
  float r1 = upright_center * upright_center;
  const float downleft_center = downleft - center;
  r1 += downleft_center * downleft_center;
  const float downright_center = downright - center;
  float r3 = downright_center * downright_center;
  const float upleft_center = upleft - center;
  r3 += upleft_center * upleft_center;
  const float r_values[4] = {r0, r1, r2, r3};
  const float d = *(std::min_element (std::begin (r_values), std::end (r_values)));
  if (d < first_threshold)
    return false;

  float b[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float a[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float sum_ab[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  b[0] = (upright - up) * up_center;
  b[0] += (downleft - down) * down_center;
  b[1] = (right - upright) * upright_center;
  b[1] += (left - downleft) * downleft_center;
  b[2] = (downright - right) * downright_center;
  b[2] += (upleft - left) * upleft_center;
  b[3] = (down - downright) * downright_center;
  b[3] += (up - upleft) * upleft_center;
  a[0] = r1 - r0 - b[0] - b[0];
  a[1] = r2 - r1 - b[1] - b[1];
  a[2] = r3 - r2 - b[2] - b[2];
  a[3] = r0 - r3 - b[3] - b[3];
  sum_ab[0] = a[0] + b[0];
  sum_ab[1] = a[1] + b[1];
  sum_ab[2] = a[2] + b[2];
  sum_ab[3] = a[3] + b[3];
  if ((*std::max_element (std::begin (b), std::end (b)) < 0) &&
      (*std::min_element (std::begin (sum_ab), std::end (sum_ab)) > 0))
  {
    const float d_values[4] = {
        b[0] * b[0] / a[0], b[1] * b[1] / a[1], b[2] * b[2] / a[2], b[3] * b[3] / a[3]};
    value = *(std::min_element (std::begin (d_values), std::end (d_values)));
  }
  else
    value = d;
  return true;
}

template <typename PointInT>
inline void
trajkovic2DResponseGridRVV (const PointInT* points,
                            float* response,
                            const int width,
                            const int half_window_size,
                            const int w,
                            const int h,
                            const float first_threshold,
                            const float second_threshold,
                            const bool eight_corners)
{
  if (half_window_size != 1)
    return;

  for (int j = half_window_size; j < h; ++j)
  {
    int i = half_window_size;
    while (i < w)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (w - i));
      const std::size_t center_index = static_cast<std::size_t> (j * width + i);

      const vfloat32m2_t center = trajkovic2DLoadIntensityRVV (points, center_index, vl);
      const vfloat32m2_t up =
          trajkovic2DLoadIntensityRVV (points, center_index - static_cast<std::size_t> (width), vl);
      const vfloat32m2_t down =
          trajkovic2DLoadIntensityRVV (points, center_index + static_cast<std::size_t> (width), vl);
      const vfloat32m2_t left = trajkovic2DLoadIntensityRVV (points, center_index - 1, vl);
      const vfloat32m2_t right = trajkovic2DLoadIntensityRVV (points, center_index + 1, vl);

      const vfloat32m2_t up_center = __riscv_vfsub_vv_f32m2 (up, center, vl);
      const vfloat32m2_t down_center = __riscv_vfsub_vv_f32m2 (down, center, vl);
      const vfloat32m2_t left_center = __riscv_vfsub_vv_f32m2 (left, center, vl);
      const vfloat32m2_t right_center = __riscv_vfsub_vv_f32m2 (right, center, vl);

      const vfloat32m2_t r0 =
          __riscv_vfadd_vv_f32m2 (trajkovic2DSquareRVV (up_center, vl),
                                  trajkovic2DSquareRVV (down_center, vl),
                                  vl);
      const vfloat32m2_t r2 =
          __riscv_vfadd_vv_f32m2 (trajkovic2DSquareRVV (right_center, vl),
                                  trajkovic2DSquareRVV (left_center, vl),
                                  vl);
      vfloat32m2_t response_value = __riscv_vfmin_vv_f32m2 (r0, r2, vl);
      vfloat32m2_t threshold_value = response_value;

      if (eight_corners)
      {
        const vfloat32m2_t upleft =
            trajkovic2DLoadIntensityRVV (points, center_index - static_cast<std::size_t> (width) - 1, vl);
        const vfloat32m2_t upright =
            trajkovic2DLoadIntensityRVV (points, center_index - static_cast<std::size_t> (width) + 1, vl);
        const vfloat32m2_t downleft =
            trajkovic2DLoadIntensityRVV (points, center_index + static_cast<std::size_t> (width) - 1, vl);
        const vfloat32m2_t downright =
            trajkovic2DLoadIntensityRVV (points, center_index + static_cast<std::size_t> (width) + 1, vl);

        const vfloat32m2_t upleft_center = __riscv_vfsub_vv_f32m2 (upleft, center, vl);
        const vfloat32m2_t upright_center = __riscv_vfsub_vv_f32m2 (upright, center, vl);
        const vfloat32m2_t downleft_center = __riscv_vfsub_vv_f32m2 (downleft, center, vl);
        const vfloat32m2_t downright_center = __riscv_vfsub_vv_f32m2 (downright, center, vl);
        const vfloat32m2_t r1 =
            __riscv_vfadd_vv_f32m2 (trajkovic2DSquareRVV (upright_center, vl),
                                    trajkovic2DSquareRVV (downleft_center, vl),
                                    vl);
        const vfloat32m2_t r3 =
            __riscv_vfadd_vv_f32m2 (trajkovic2DSquareRVV (downright_center, vl),
                                    trajkovic2DSquareRVV (upleft_center, vl),
                                    vl);
        response_value =
            __riscv_vfmin_vv_f32m2 (__riscv_vfmin_vv_f32m2 (response_value, r1, vl), r3, vl);
        threshold_value = response_value;

        const vfloat32m2_t b0 =
            __riscv_vfadd_vv_f32m2 (__riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (upright, up, vl), up_center, vl),
                                    __riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (downleft, down, vl), down_center, vl),
                                    vl);
        const vfloat32m2_t b1 =
            __riscv_vfadd_vv_f32m2 (__riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (right, upright, vl), upright_center, vl),
                                    __riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (left, downleft, vl), downleft_center, vl),
                                    vl);
        const vfloat32m2_t b2 =
            __riscv_vfadd_vv_f32m2 (__riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (downright, right, vl), downright_center, vl),
                                    __riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (upleft, left, vl), upleft_center, vl),
                                    vl);
        const vfloat32m2_t b3 =
            __riscv_vfadd_vv_f32m2 (__riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (down, downright, vl), downright_center, vl),
                                    __riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (up, upleft, vl), upleft_center, vl),
                                    vl);
        const vfloat32m2_t two_b0 = __riscv_vfadd_vv_f32m2 (b0, b0, vl);
        const vfloat32m2_t two_b1 = __riscv_vfadd_vv_f32m2 (b1, b1, vl);
        const vfloat32m2_t two_b2 = __riscv_vfadd_vv_f32m2 (b2, b2, vl);
        const vfloat32m2_t two_b3 = __riscv_vfadd_vv_f32m2 (b3, b3, vl);
        const vfloat32m2_t a0 =
            __riscv_vfsub_vv_f32m2 (__riscv_vfsub_vv_f32m2 (r1, r0, vl), two_b0, vl);
        const vfloat32m2_t a1 =
            __riscv_vfsub_vv_f32m2 (__riscv_vfsub_vv_f32m2 (r2, r1, vl), two_b1, vl);
        const vfloat32m2_t a2 =
            __riscv_vfsub_vv_f32m2 (__riscv_vfsub_vv_f32m2 (r3, r2, vl), two_b2, vl);
        const vfloat32m2_t a3 =
            __riscv_vfsub_vv_f32m2 (__riscv_vfsub_vv_f32m2 (r0, r3, vl), two_b3, vl);
        vbool16_t use_d =
            __riscv_vmflt_vf_f32m2_b16 (__riscv_vfmax_vv_f32m2 (__riscv_vfmax_vv_f32m2 (b0, b1, vl),
                                                               __riscv_vfmax_vv_f32m2 (b2, b3, vl),
                                                               vl),
                                        0.0f,
                                        vl);
        const vfloat32m2_t sum01_min =
            __riscv_vfmin_vv_f32m2 (__riscv_vfadd_vv_f32m2 (a0, b0, vl),
                                    __riscv_vfadd_vv_f32m2 (a1, b1, vl),
                                    vl);
        const vfloat32m2_t sum23_min =
            __riscv_vfmin_vv_f32m2 (__riscv_vfadd_vv_f32m2 (a2, b2, vl),
                                    __riscv_vfadd_vv_f32m2 (a3, b3, vl),
                                    vl);
        use_d = __riscv_vmand_mm_b16 (
            use_d, __riscv_vmfgt_vf_f32m2_b16 (__riscv_vfmin_vv_f32m2 (sum01_min, sum23_min, vl), 0.0f, vl), vl);
        const vfloat32m2_t d0 =
            __riscv_vfdiv_vv_f32m2 (__riscv_vfmul_vv_f32m2 (b0, b0, vl), a0, vl);
        const vfloat32m2_t d1 =
            __riscv_vfdiv_vv_f32m2 (__riscv_vfmul_vv_f32m2 (b1, b1, vl), a1, vl);
        const vfloat32m2_t d2 =
            __riscv_vfdiv_vv_f32m2 (__riscv_vfmul_vv_f32m2 (b2, b2, vl), a2, vl);
        const vfloat32m2_t d3 =
            __riscv_vfdiv_vv_f32m2 (__riscv_vfmul_vv_f32m2 (b3, b3, vl), a3, vl);
        const vfloat32m2_t d_min =
            __riscv_vfmin_vv_f32m2 (__riscv_vfmin_vv_f32m2 (d0, d1, vl),
                                    __riscv_vfmin_vv_f32m2 (d2, d3, vl),
                                    vl);
        response_value = __riscv_vmerge_vvm_f32m2 (response_value, d_min, use_d, vl);
      }
      else
      {
        const vfloat32m2_t b1 =
            __riscv_vfadd_vv_f32m2 (__riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (right, up, vl), up_center, vl),
                                    __riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (left, down, vl), down_center, vl),
                                    vl);
        const vfloat32m2_t b2 =
            __riscv_vfadd_vv_f32m2 (__riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (right, down, vl), down_center, vl),
                                    __riscv_vfmul_vv_f32m2 (__riscv_vfsub_vv_f32m2 (left, up, vl), up_center, vl),
                                    vl);
        const vfloat32m2_t b = __riscv_vfmin_vv_f32m2 (b1, b2, vl);
        const vfloat32m2_t a =
            __riscv_vfsub_vv_f32m2 (__riscv_vfsub_vv_f32m2 (r2, r0, vl), __riscv_vfadd_vv_f32m2 (b, b, vl), vl);
        const vbool16_t use_adjusted = __riscv_vmand_mm_b16 (
            __riscv_vmflt_vf_f32m2_b16 (b, 0.0f, vl),
            __riscv_vmfgt_vf_f32m2_b16 (__riscv_vfadd_vv_f32m2 (b, a, vl), 0.0f, vl),
            vl);
        const vfloat32m2_t adjusted =
            __riscv_vfsub_vv_f32m2 (r0,
                                    __riscv_vfdiv_vv_f32m2 (__riscv_vfmul_vv_f32m2 (b, b, vl), a, vl),
                                    vl);
        response_value = __riscv_vmerge_vvm_f32m2 (response_value, adjusted, use_adjusted, vl);
      }

      const vbool16_t keep = __riscv_vmfge_vf_f32m2_b16 (threshold_value, first_threshold, vl);
      __riscv_vse32_v_f32m2_m (keep, response + center_index, response_value, vl);
      i += static_cast<int> (vl);
    }
  }

  (void) second_threshold;
  const float reconcile_threshold = first_threshold - std::max (1.0f, std::abs (first_threshold)) * 1e-3f;
  // NMS 排序对 float response 的细小差异很敏感；所有通过 first-threshold 的点用标量公式复核，
  // 保持 public output 与原路径一致，低响应点仍由 RVV 快速筛掉。
  for (int j = half_window_size; j < h; ++j)
  {
    for (int i = half_window_size; i < w; ++i)
    {
      const std::size_t index = static_cast<std::size_t> (j * width + i);
      if (response[index] < reconcile_threshold)
        continue;

      float scalar_response = 0.0f;
      response[index] = trajkovic2DScalarResponseAt (
                            points, width, i, j, first_threshold, eight_corners, scalar_response)
                            ? scalar_response
                            : 0.0f;
    }
  }
}
} // namespace detail
#endif

template <typename PointInT, typename PointOutT, typename IntensityT> bool
TrajkovicKeypoint2D<PointInT, PointOutT, IntensityT>::initCompute ()
{
  if (!PCLBase<PointInT>::initCompute ())
    return (false);

  keypoints_indices_.reset (new pcl::PointIndices);
  keypoints_indices_->indices.reserve (input_->size ());

  if (!input_->isOrganized ())
  {
    PCL_ERROR ("[pcl::%s::initCompute] %s doesn't support non organized clouds!\n", name_.c_str ());
    return (false);
  }

  if (indices_->size () != input_->size ())
  {
    PCL_ERROR ("[pcl::%s::initCompute] %s doesn't support setting indices!\n", name_.c_str ());
    return (false);
  }

  if ((window_size_%2) == 0)
  {
    PCL_ERROR ("[pcl::%s::initCompute] Window size must be odd!\n", name_.c_str ());
    return (false);
  }

  if (window_size_ < 3)
  {
    PCL_ERROR ("[pcl::%s::initCompute] Window size must be >= 3x3!\n", name_.c_str ());
    return (false);
  }

  half_window_size_ = window_size_ / 2;

  return (true);
}


template <typename PointInT, typename PointOutT, typename IntensityT> void
TrajkovicKeypoint2D<PointInT, PointOutT, IntensityT>::detectKeypoints (PointCloudOut &output)
{
  response_.reset (new pcl::PointCloud<float> (input_->width, input_->height));
  const int w = static_cast<int> (input_->width) - half_window_size_;
  const int h = static_cast<int> (input_->height) - half_window_size_;

#if defined(__RVV10__)
  if constexpr (detail::kTrajkovic2DPointXYZIRVVGated<PointInT, IntensityT>)
  {
    if (window_size_ == 3 &&
        method_ != pcl::TrajkovicKeypoint2D<PointInT, PointOutT, IntensityT>::FOUR_CORNERS)
    {
      detail::trajkovic2DResponseGridRVV (input_->points.data (),
                                          response_->points.data (),
                                          static_cast<int> (input_->width),
                                          half_window_size_,
                                          w,
                                          h,
                                          first_threshold_,
                                          second_threshold_,
                                          method_ != pcl::TrajkovicKeypoint2D<PointInT, PointOutT, IntensityT>::FOUR_CORNERS);
      goto trajkovic_2d_response_grid_complete;
    }
  }
#endif

  if (method_ == pcl::TrajkovicKeypoint2D<PointInT, PointOutT, IntensityT>::FOUR_CORNERS)
  {
#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(h, w) \
  num_threads(threads_)
#endif
    for(int j = half_window_size_; j < h; ++j)
    {
      for(int i = half_window_size_; i < w; ++i)
      {
        float center = intensity_ ((*input_) (i,j));
        float up = intensity_ ((*input_) (i, j-half_window_size_));
        float down = intensity_ ((*input_) (i, j+half_window_size_));
        float left = intensity_ ((*input_) (i-half_window_size_, j));
        float right = intensity_ ((*input_) (i+half_window_size_, j));

        float up_center = up - center;
        float r1 = up_center * up_center;
        float down_center = down - center;
        r1+= down_center * down_center;

        float right_center = right - center;
        float r2 = right_center * right_center;
        float left_center = left - center;
        r2+= left_center * left_center;

        float d = std::min (r1, r2);

        if (d < first_threshold_)
          continue;

        float b1 = (right - up) * up_center;
        b1+= (left - down) * down_center;
        float b2 = (right - down) * down_center;
        b2+= (left - up) * up_center;
        float B = std::min (b1, b2);
        float A = r2 - r1 - 2*B;

        (*response_) (i,j) = ((B < 0) && ((B + A) > 0)) ? r1 - ((B*B)/A) : d;
      }
    }
  }
  else
  {
#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(h, w) \
  num_threads(threads_)
#endif
    for(int j = half_window_size_; j < h; ++j)
    {
      for(int i = half_window_size_; i < w; ++i)
      {
        float center = intensity_ ((*input_) (i,j));
        float up = intensity_ ((*input_) (i, j-half_window_size_));
        float down = intensity_ ((*input_) (i, j+half_window_size_));
        float left = intensity_ ((*input_) (i-half_window_size_, j));
        float right = intensity_ ((*input_) (i+half_window_size_, j));
        float upleft = intensity_ ((*input_) (i-half_window_size_, j-half_window_size_));
        float upright = intensity_ ((*input_) (i+half_window_size_, j-half_window_size_));
        float downleft = intensity_ ((*input_) (i-half_window_size_, j+half_window_size_));
        float downright = intensity_ ((*input_) (i+half_window_size_, j+half_window_size_));
        std::vector<float> r (4,0);

        float up_center = up - center;
        r[0] = up_center * up_center;
        float down_center = down - center;
        r[0]+= down_center * down_center;

        float upright_center = upright - center;
        r[1] = upright_center * upright_center;
        float downleft_center = downleft - center;
        r[1]+= downleft_center * downleft_center;

        float right_center = right - center;
        r[2] = right_center * right_center;
        float left_center = left - center;
        r[2]+= left_center * left_center;

        float downright_center = downright - center;
        r[3] = downright_center * downright_center;
        float upleft_center = upleft - center;
        r[3]+= upleft_center * upleft_center;

        float d = *(std::min_element (r.begin (), r.end ()));

        if (d < first_threshold_)
          continue;

        std::vector<float> B (4,0);
        std::vector<float> A (4,0);
        std::vector<float> sumAB (4,0);
        B[0] = (upright - up) * up_center;
        B[0]+= (downleft - down) * down_center;
        B[1] = (right - upright) * upright_center;
        B[1]+= (left - downleft) * downleft_center;
        B[2] = (downright - right) * downright_center;
        B[2]+= (upleft - left) * upleft_center;
        B[3] = (down - downright) * downright_center;
        B[3]+= (up - upleft) * upleft_center;
        A[0] = r[1] - r[0] - B[0] - B[0];
        A[1] = r[2] - r[1] - B[1] - B[1];
        A[2] = r[3] - r[2] - B[2] - B[2];
        A[3] = r[0] - r[3] - B[3] - B[3];
        sumAB[0] = A[0] + B[0];
        sumAB[1] = A[1] + B[1];
        sumAB[2] = A[2] + B[2];
        sumAB[3] = A[3] + B[3];
        if ((*std::max_element (B.begin (), B.end ()) < 0) &&
            (*std::min_element (sumAB.begin (), sumAB.end ()) > 0))
        {
          std::vector<float> D (4,0);
          D[0] = B[0] * B[0] / A[0];
          D[1] = B[1] * B[1] / A[1];
          D[2] = B[2] * B[2] / A[2];
          D[3] = B[3] * B[3] / A[3];
          (*response_) (i,j) = *(std::min (D.begin (), D.end ()));
        }
        else
          (*response_) (i,j) = d;
      }
    }
  }

#if defined(__RVV10__)
trajkovic_2d_response_grid_complete:
#endif

  // Non maximas suppression
  pcl::Indices indices = *indices_;
  std::sort (indices.begin (), indices.end (), [this] (int p1, int p2) { return greaterCornernessAtIndices (p1, p2); });

  output.clear ();
  output.reserve (input_->size ());

  std::vector<bool> occupency_map (indices.size (), false);
  const int width (input_->width);
  const int height (input_->height);

#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  shared(indices, occupency_map, output) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(height, indices, occupency_map, output, width) \
  num_threads(threads_)
#endif
  // Disable lint since this 'for' is part of the pragma
  // NOLINTNEXTLINE(modernize-loop-convert)
  for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t> (indices.size ()); ++i)
  {
    int idx = indices[i];
    if (((*response_)[idx] < second_threshold_) || occupency_map[idx])
      continue;

    PointOutT p;
    p.getVector3fMap () = (*input_)[idx].getVector3fMap ();
    p.intensity = response_->points [idx];

#pragma omp critical
    {
      output.push_back (p);
      keypoints_indices_->indices.push_back (idx);
    }

    const int x = idx % width;
    const int y = idx / width;
    const int u_end = std::min (width, x + half_window_size_);
    const int v_end = std::min (height, y + half_window_size_);
    for(int v = std::max (0, y - half_window_size_); v < v_end; ++v)
      for(int u = std::max (0, x - half_window_size_); u < u_end; ++u)
        occupency_map[v*width + u] = true;
  }

  output.height = 1;
  output.width = output.size();
  // we don not change the denseness
  output.is_dense = input_->is_dense;
}

} // namespace pcl

#define PCL_INSTANTIATE_TrajkovicKeypoint2D(T,U,I) template class PCL_EXPORTS pcl::TrajkovicKeypoint2D<T,U,I>;

#endif
