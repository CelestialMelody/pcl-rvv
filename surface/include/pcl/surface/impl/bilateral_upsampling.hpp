/*
 * Software License Agreement (BSD License)
 *
 * Point Cloud Library (PCL) - www.pointclouds.org
 * Copyright (c) 2009-2012, Willow Garage, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * * Neither the name of Willow Garage, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef PCL_SURFACE_IMPL_BILATERAL_UPSAMPLING_H_
#define PCL_SURFACE_IMPL_BILATERAL_UPSAMPLING_H_

#include <pcl/surface/bilateral_upsampling.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <pcl/console/print.h>

#include <Eigen/LU> // for inverse

#if defined(__RVV10__)
#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>

#include <cstddef>
#include <cstdint>
#include <riscv_vector.h>
#include <type_traits>
#endif

namespace pcl
{

template <typename PointInT, typename PointOutT> void
bilateralUpsamplingPerformProcessingStd (const pcl::PointCloud<PointInT>& input,
                                         pcl::PointCloud<PointOutT>& output,
                                         const int window_size,
                                         const Eigen::MatrixXf& val_exp_depth_matrix,
                                         const Eigen::VectorXf& val_exp_rgb_vector,
                                         const Eigen::Matrix3f& unprojection_matrix)
{
    output.resize (input.size ());
    float nan = std::numeric_limits<float>::quiet_NaN ();

    for (int x = 0; x < static_cast<int> (input.width); ++x)
      for (int y = 0; y < static_cast<int> (input.height); ++y)
      {
        int start_window_x = std::max (x - window_size, 0),
            start_window_y = std::max (y - window_size, 0),
            end_window_x = std::min (x + window_size, static_cast<int> (input.width)),
            end_window_y = std::min (y + window_size, static_cast<int> (input.height));

        float sum = 0.0f,
            norm_sum = 0.0f;

        for (int x_w = start_window_x; x_w < end_window_x; ++ x_w)
          for (int y_w = start_window_y; y_w < end_window_y; ++ y_w)
          {
            float val_exp_depth = val_exp_depth_matrix (static_cast<Eigen::MatrixXf::Index> (x - x_w + window_size),
                                                        static_cast<Eigen::MatrixXf::Index> (y - y_w + window_size));

            auto d_color = static_cast<Eigen::VectorXf::Index> (
                std::abs (input[y_w * input.width + x_w].r - input[y * input.width + x].r) +
                std::abs (input[y_w * input.width + x_w].g - input[y * input.width + x].g) +
                std::abs (input[y_w * input.width + x_w].b - input[y * input.width + x].b));

            float val_exp_rgb = val_exp_rgb_vector (d_color);

            if (std::isfinite (input[y_w*input.width + x_w].z))
            {
              sum += val_exp_depth * val_exp_rgb * input[y_w*input.width + x_w].z;
              norm_sum += val_exp_depth * val_exp_rgb;
            }
          }

        output[y*input.width + x].r = input[y*input.width + x].r;
        output[y*input.width + x].g = input[y*input.width + x].g;
        output[y*input.width + x].b = input[y*input.width + x].b;

        if (norm_sum != 0.0f)
        {
          float depth = sum / norm_sum;
          Eigen::Vector3f pc (static_cast<float> (x) * depth, static_cast<float> (y) * depth, depth);
          Eigen::Vector3f pw (unprojection_matrix * pc);
          output[y*input.width + x].x = pw[0];
          output[y*input.width + x].y = pw[1];
          output[y*input.width + x].z = pw[2];
        }
        else
        {
          output[y*input.width + x].x = nan;
          output[y*input.width + x].y = nan;
          output[y*input.width + x].z = nan;
        }
      }

    output.header = input.header;
    output.width = input.width;
    output.height = input.height;
}

#if defined(__RVV10__)
template <typename PointT>
inline constexpr bool kBilateralUpsamplingRgbPoint =
    std::is_same_v<PointT, pcl::PointXYZRGB> || std::is_same_v<PointT, pcl::PointXYZRGBA>;

template <typename PointInT, typename PointOutT>
inline constexpr bool kBilateralUpsamplingRVVCompatible =
    kBilateralUpsamplingRgbPoint<PointInT> && kBilateralUpsamplingRgbPoint<PointOutT> &&
    pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value &&
    pcl::rvv::RVVXYZAoSFloatLayout<PointOutT>::value;

inline float
bilateralUpsamplingReduceSumF32M2 (vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1 (0.0f, 1);
  const vfloat32m1_t sum = __riscv_vfredusum_vs_f32m2_f32m1 (values, zero, vl);
  return __riscv_vfmv_f_s_f32m1_f32 (sum);
}

template <typename PointInT, typename PointOutT> bool
bilateralUpsamplingPerformProcessingColorGatherRVV (const pcl::PointCloud<PointInT>& input,
                                                    pcl::PointCloud<PointOutT>& output,
                                                    const int window_size,
                                                    const Eigen::MatrixXf& val_exp_depth_matrix,
                                                    const Eigen::VectorXf& val_exp_rgb_vector,
                                                    const Eigen::Matrix3f& unprojection_matrix)
{
  if constexpr (!kBilateralUpsamplingRVVCompatible<PointInT, PointOutT>)
  {
    return false;
  }
  else
  {
    if (window_size <= 0 || window_size > 32 || input.empty () || input.width == 0 || input.height == 0)
      return false;

    output.resize (input.size ());
    const float nan = std::numeric_limits<float>::quiet_NaN ();
    const int width = static_cast<int> (input.width);
    const int height = static_cast<int> (input.height);
    const auto* base = input.points.data ();
    const ptrdiff_t point_stride_bytes = static_cast<ptrdiff_t> (input.width) *
                                         static_cast<ptrdiff_t> (sizeof (PointInT));
    const ptrdiff_t depth_col_stride_bytes = -static_cast<ptrdiff_t> (val_exp_depth_matrix.outerStride ()) *
                                            static_cast<ptrdiff_t> (sizeof (float));
    const float* rgb_base = val_exp_rgb_vector.data ();

    for (int x = 0; x < width; ++x)
      for (int y = 0; y < height; ++y)
      {
        const int center = y * width + x;
        const auto& center_point = input[center];
        const std::uint8_t center_r = center_point.r;
        const std::uint8_t center_g = center_point.g;
        const std::uint8_t center_b = center_point.b;
        int start_window_x = std::max (x - window_size, 0),
            start_window_y = std::max (y - window_size, 0),
            end_window_x = std::min (x + window_size, width),
            end_window_y = std::min (y + window_size, height);

        float sum = 0.0f,
            norm_sum = 0.0f;

        for (int x_w = start_window_x; x_w < end_window_x; ++ x_w)
        {
          int y_w = start_window_y;
          while (y_w < end_window_y)
          {
            constexpr std::size_t kMaxChunkLanes = 64;
            const std::size_t remaining = std::min<std::size_t> (static_cast<std::size_t> (end_window_y - y_w),
                                                                  kMaxChunkLanes);
            const std::size_t vl = __riscv_vsetvl_e32m2 (remaining);

            const auto* depth_ptr = &val_exp_depth_matrix (
                static_cast<Eigen::MatrixXf::Index> (x - x_w + window_size),
                static_cast<Eigen::MatrixXf::Index> (y - y_w + window_size));
            const vfloat32m2_t depth_w = __riscv_vlse32_v_f32m2 (depth_ptr, depth_col_stride_bytes, vl);

            const auto* r_ptr = &base[y_w * width + x_w].r;
            const auto* g_ptr = &base[y_w * width + x_w].g;
            const auto* b_ptr = &base[y_w * width + x_w].b;
            const vuint8mf2_t r8 = __riscv_vlse8_v_u8mf2 (r_ptr, point_stride_bytes, vl);
            const vuint8mf2_t g8 = __riscv_vlse8_v_u8mf2 (g_ptr, point_stride_bytes, vl);
            const vuint8mf2_t b8 = __riscv_vlse8_v_u8mf2 (b_ptr, point_stride_bytes, vl);
            const vuint16m1_t r16 = __riscv_vzext_vf2_u16m1 (r8, vl);
            const vuint16m1_t g16 = __riscv_vzext_vf2_u16m1 (g8, vl);
            const vuint16m1_t b16 = __riscv_vzext_vf2_u16m1 (b8, vl);
            const vuint16m1_t center_r_vec = __riscv_vmv_v_x_u16m1 (center_r, vl);
            const vuint16m1_t center_g_vec = __riscv_vmv_v_x_u16m1 (center_g, vl);
            const vuint16m1_t center_b_vec = __riscv_vmv_v_x_u16m1 (center_b, vl);
            const vuint16m1_t dr =
                __riscv_vsub_vv_u16m1 (__riscv_vmaxu_vv_u16m1 (r16, center_r_vec, vl),
                                       __riscv_vminu_vv_u16m1 (r16, center_r_vec, vl),
                                       vl);
            const vuint16m1_t dg =
                __riscv_vsub_vv_u16m1 (__riscv_vmaxu_vv_u16m1 (g16, center_g_vec, vl),
                                       __riscv_vminu_vv_u16m1 (g16, center_g_vec, vl),
                                       vl);
            const vuint16m1_t db =
                __riscv_vsub_vv_u16m1 (__riscv_vmaxu_vv_u16m1 (b16, center_b_vec, vl),
                                       __riscv_vminu_vv_u16m1 (b16, center_b_vec, vl),
                                       vl);
            const vuint16m1_t d_color = __riscv_vadd_vv_u16m1 (__riscv_vadd_vv_u16m1 (dr, dg, vl), db, vl);
            const vuint16m1_t color_offsets = __riscv_vsll_vx_u16m1 (d_color, 2, vl);
            const vfloat32m2_t rgb_w = __riscv_vluxei16_v_f32m2 (rgb_base, color_offsets, vl);
            const vfloat32m2_t w = __riscv_vfmul_vv_f32m2 (depth_w, rgb_w, vl);

            const auto* z_ptr = &base[y_w * width + x_w].z;
            const vfloat32m2_t z = __riscv_vlse32_v_f32m2 (z_ptr, point_stride_bytes, vl);
            vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (z, z, vl);
            finite = __riscv_vmand_mm_b16 (
                finite,
                __riscv_vmflt_vf_f32m2_b16 (
                    __riscv_vfabs_v_f32m2 (z, vl), std::numeric_limits<float>::infinity (), vl),
                vl);
            const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
            const vfloat32m2_t w_safe = __riscv_vmerge_vvm_f32m2 (zero, w, finite, vl);
            const vfloat32m2_t z_safe = __riscv_vmerge_vvm_f32m2 (zero, z, finite, vl);
            sum += bilateralUpsamplingReduceSumF32M2 (__riscv_vfmul_vv_f32m2 (w_safe, z_safe, vl), vl);
            norm_sum += bilateralUpsamplingReduceSumF32M2 (w_safe, vl);
            y_w += static_cast<int> (vl);
          }
        }

        output[center].r = center_r;
        output[center].g = center_g;
        output[center].b = center_b;

        if (norm_sum != 0.0f)
        {
          const float depth = sum / norm_sum;
          const Eigen::Vector3f pc (static_cast<float> (x) * depth, static_cast<float> (y) * depth, depth);
          const Eigen::Vector3f pw (unprojection_matrix * pc);
          output[center].x = pw[0];
          output[center].y = pw[1];
          output[center].z = pw[2];
        }
        else
        {
          output[center].x = nan;
          output[center].y = nan;
          output[center].z = nan;
        }
      }

    output.header = input.header;
    output.width = input.width;
    output.height = input.height;
    return true;
  }
}

template <typename PointInT, typename PointOutT> bool
bilateralUpsamplingPerformProcessingRVV (const pcl::PointCloud<PointInT>& input,
                                         pcl::PointCloud<PointOutT>& output,
                                         const int window_size,
                                         const Eigen::MatrixXf& val_exp_depth_matrix,
                                         const Eigen::VectorXf& val_exp_rgb_vector,
                                         const Eigen::Matrix3f& unprojection_matrix)
{
  return bilateralUpsamplingPerformProcessingColorGatherRVV<PointInT, PointOutT> (
      input, output, window_size, val_exp_depth_matrix, val_exp_rgb_vector, unprojection_matrix);
}
#endif

} // namespace pcl

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::BilateralUpsampling<PointInT, PointOutT>::process (pcl::PointCloud<PointOutT> &output)
{
  // Copy the header
  output.header = input_->header;

  if (!initCompute ())
  {
    output.width = output.height = 0;
    output.clear ();
    return;
  }

  if (input_->isOrganized () == false)
  {
    PCL_ERROR ("Input cloud is not organized.\n");
    return;
  }

  // Invert projection matrix
  unprojection_matrix_ = projection_matrix_.inverse ();

  for (int i = 0; i < 3; ++i)
  {
    for (int j = 0; j < 3; ++j)
      printf ("%f ", unprojection_matrix_(i, j));

    printf ("\n");
  }


  // Perform the actual surface reconstruction
  performProcessing (output);

  deinitCompute ();
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::BilateralUpsampling<PointInT, PointOutT>::performProcessing (PointCloudOut &output)
{
    Eigen::MatrixXf val_exp_depth_matrix;
    Eigen::VectorXf val_exp_rgb_vector;
    computeDistances (val_exp_depth_matrix, val_exp_rgb_vector);

#if defined(__RVV10__)
    // RVV is limited to the verified RGB/RGBA point layouts; all other
    // template instances keep the original scalar path.
    if (bilateralUpsamplingPerformProcessingColorGatherRVV<PointInT, PointOutT> (*input_, output, window_size_,
                                                                                  val_exp_depth_matrix,
                                                                                  val_exp_rgb_vector,
                                                                                  unprojection_matrix_))
      return;
#endif

    bilateralUpsamplingPerformProcessingStd<PointInT, PointOutT> (*input_, output, window_size_,
                                                                  val_exp_depth_matrix,
                                                                  val_exp_rgb_vector,
                                                                  unprojection_matrix_);
}


template <typename PointInT, typename PointOutT> void
pcl::BilateralUpsampling<PointInT, PointOutT>::computeDistances (Eigen::MatrixXf &val_exp_depth, Eigen::VectorXf &val_exp_rgb)
{
  val_exp_depth.resize (2*window_size_+1,2*window_size_+1);
  val_exp_rgb.resize (3*255+1);

  int j = 0;
  for (int dx = -window_size_; dx < window_size_+1; ++dx)
  {
    int i = 0;
    for (int dy = -window_size_; dy < window_size_+1; ++dy)
    {
      float val_exp = std::exp (- (dx*dx + dy*dy) / (2.0f * static_cast<float> (sigma_depth_ * sigma_depth_)));
      val_exp_depth(i,j) = val_exp;
      i++;
    }
    j++;
  }

  for (int d_color = 0; d_color < 3*255+1; d_color++)
  {
    float val_exp = std::exp (- d_color * d_color / (2.0f * sigma_color_ * sigma_color_));
    val_exp_rgb(d_color) = val_exp;
  }
}


#define PCL_INSTANTIATE_BilateralUpsampling(T,OutT) template class PCL_EXPORTS pcl::BilateralUpsampling<T,OutT>;


#endif /* PCL_SURFACE_IMPL_BILATERAL_UPSAMPLING_H_ */
