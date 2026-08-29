/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
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

#pragma once

#include <pcl/pcl_base.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <pcl/recognition/dot_modality.h>
#include <pcl/recognition/point_types.h>
#include <pcl/recognition/quantized_map.h>

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/common.h>
#endif


namespace pcl
{
  template <typename PointInT>
  class ColorGradientDOTModality
    : public DOTModality, public PCLBase<PointInT>
  {
    protected:
      using PCLBase<PointInT>::input_;

    struct Candidate
    {
      GradientXY gradient;

      int x;
      int y;

      bool operator< (const Candidate & rhs)
      {
        return (gradient.magnitude > rhs.gradient.magnitude);
      }
    };

    public:
      using PointCloudIn = pcl::PointCloud<PointInT>;

      ColorGradientDOTModality (std::size_t bin_size);

      virtual ~ColorGradientDOTModality () = default;

      inline void
      setGradientMagnitudeThreshold (const float threshold)
      {
        gradient_magnitude_threshold_ = threshold;
      }

      //inline QuantizedMap &
      //getDominantQuantizedMap ()
      //{
      //  return (dominant_quantized_color_gradients_);
      //}

      inline QuantizedMap &
      getDominantQuantizedMap ()
      {
        return (dominant_quantized_color_gradients_);
      }

      QuantizedMap
      computeInvariantQuantizedMap (const MaskMap & mask,
                                   const RegionXY & region);

      /** \brief Provide a pointer to the input dataset (overwrites the PCLBase::setInputCloud method)
        * \param cloud the const boost shared pointer to a PointCloud message
        */
      virtual void
      setInputCloud (const typename PointCloudIn::ConstPtr & cloud)
      {
        input_ = cloud;
        //processInputData ();
      }

      virtual void
      processInputData ();

    protected:

      void
      computeMaxColorGradients ();

#if defined(__RVV10__) && defined(__riscv_vector)
      void
      computeMaxColorGradientsRVV ();
#endif

      void
      computeDominantQuantizedGradients ();

      //void
      //computeInvariantQuantizedGradients ();

    private:
      std::size_t bin_size_;

      float gradient_magnitude_threshold_;
      pcl::PointCloud<pcl::GradientXY> color_gradients_;

      pcl::QuantizedMap dominant_quantized_color_gradients_;
      //pcl::QuantizedMap invariant_quantized_color_gradients_;

  };

}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT>
pcl::ColorGradientDOTModality<PointInT>::
ColorGradientDOTModality (const std::size_t bin_size)
  : bin_size_ (bin_size), gradient_magnitude_threshold_ (80.0f), color_gradients_ (), dominant_quantized_color_gradients_ ()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT>
void
pcl::ColorGradientDOTModality<PointInT>::
processInputData ()
{
  // extract color gradients
#if defined(__RVV10__) && defined(__riscv_vector)
  computeMaxColorGradientsRVV ();
#else
  computeMaxColorGradients ();
#endif

  // compute dominant quantized gradient map
  computeDominantQuantizedGradients ();

  // compute invariant quantized gradient map
  //computeInvariantQuantizedGradients ();
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT>
void
pcl::ColorGradientDOTModality<PointInT>::
computeMaxColorGradients ()
{
  const int width = input_->width;
  const int height = input_->height;

  color_gradients_.resize (width*height);
  color_gradients_.width = width;
  color_gradients_.height = height;

  constexpr float pi = std::tan(1.0f)*4;
  for (int row_index = 0; row_index < height-2; ++row_index)
  {
    for (int col_index = 0; col_index < width-2; ++col_index)
    {
      const int index0 = row_index*width+col_index;
      const int index_c = row_index*width+col_index+2;
      const int index_r = (row_index+2)*width+col_index;

      const unsigned char r0 = (*input_)[index0].r;
      const unsigned char g0 = (*input_)[index0].g;
      const unsigned char b0 = (*input_)[index0].b;

      const unsigned char r_c = (*input_)[index_c].r;
      const unsigned char g_c = (*input_)[index_c].g;
      const unsigned char b_c = (*input_)[index_c].b;

      const unsigned char r_r = (*input_)[index_r].r;
      const unsigned char g_r = (*input_)[index_r].g;
      const unsigned char b_r = (*input_)[index_r].b;

      const float r_dx = static_cast<float> (r_c) - static_cast<float> (r0);
      const float g_dx = static_cast<float> (g_c) - static_cast<float> (g0);
      const float b_dx = static_cast<float> (b_c) - static_cast<float> (b0);

      const float r_dy = static_cast<float> (r_r) - static_cast<float> (r0);
      const float g_dy = static_cast<float> (g_r) - static_cast<float> (g0);
      const float b_dy = static_cast<float> (b_r) - static_cast<float> (b0);

      const float sqr_mag_r = r_dx*r_dx + r_dy*r_dy;
      const float sqr_mag_g = g_dx*g_dx + g_dy*g_dy;
      const float sqr_mag_b = b_dx*b_dx + b_dy*b_dy;

      GradientXY gradient;
      gradient.x = col_index;
      gradient.y = row_index;
      if (sqr_mag_r > sqr_mag_g && sqr_mag_r > sqr_mag_b)
      {
        gradient.magnitude = sqrt (sqr_mag_r);
        gradient.angle = std::atan2 (r_dy, r_dx) * 180.0f / pi;
      }
      else if (sqr_mag_g > sqr_mag_b)
      {
        gradient.magnitude = sqrt (sqr_mag_g);
        gradient.angle = std::atan2 (g_dy, g_dx) * 180.0f / pi;
      }
      else
      {
        gradient.magnitude = sqrt (sqr_mag_b);
        gradient.angle = std::atan2 (b_dy, b_dx) * 180.0f / pi;
      }

      assert (color_gradients_ (col_index+1, row_index+1).angle >= -180 &&
              color_gradients_ (col_index+1, row_index+1).angle <=  180);

      color_gradients_ (col_index+1, row_index+1) = gradient;
    }
  }

  return;
}

//////////////////////////////////////////////////////////////////////////////////////////////
#if defined(__RVV10__) && defined(__riscv_vector)
template <typename PointInT>
void
pcl::ColorGradientDOTModality<PointInT>::
computeMaxColorGradientsRVV ()
{
  const std::size_t width = input_->width;
  const std::size_t height = input_->height;

  color_gradients_.resize (width*height);
  color_gradients_.width = static_cast<std::uint32_t> (width);
  color_gradients_.height = static_cast<std::uint32_t> (height);

  if (width < 3 || height < 3)
    return;

  constexpr float pi = std::tan (1.0f) * 4.0f;
  constexpr float radians_to_degrees = 180.0f / pi;
  constexpr float negative_half_pi_degrees = -1.57079632679489661923f * radians_to_degrees;
  const std::size_t max_vl = __riscv_vsetvlmax_e32m2 ();

  std::vector<float> magnitudes (max_vl);
  std::vector<float> angles (max_vl);
  const auto* input_bytes = reinterpret_cast<const std::uint8_t*> (&(*input_)[0]);
  const auto* first_point = reinterpret_cast<const std::uint8_t*> (&(*input_)[0]);
  const std::ptrdiff_t r_offset =
      reinterpret_cast<const std::uint8_t*> (&(*input_)[0].r) - first_point;
  const std::ptrdiff_t g_offset =
      reinterpret_cast<const std::uint8_t*> (&(*input_)[0].g) - first_point;
  const std::ptrdiff_t b_offset =
      reinterpret_cast<const std::uint8_t*> (&(*input_)[0].b) - first_point;
  const std::ptrdiff_t point_stride = static_cast<std::ptrdiff_t> (sizeof (PointInT));

  const auto load_channel = [input_bytes, width, point_stride] (const std::size_t row,
                                                                const std::size_t col,
                                                                const std::ptrdiff_t channel_offset,
                                                                const std::size_t vl)
  {
    const auto* ptr = input_bytes + (row*width + col)*sizeof (PointInT) + channel_offset;
    const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2 (ptr, point_stride, vl);
    const vuint16m1_t u16 = __riscv_vzext_vf2_u16m1 (u8, vl);
    return __riscv_vreinterpret_v_u32m2_i32m2 (__riscv_vzext_vf2_u32m2 (u16, vl));
  };

  const auto compute_channel = [&load_channel] (const std::size_t row,
                                                const std::size_t col,
                                                const std::ptrdiff_t channel_offset,
                                                const std::size_t vl,
                                                vint32m2_t& dx,
                                                vint32m2_t& dy,
                                                vint32m2_t& sqr_mag)
  {
    const vint32m2_t center = load_channel (row, col, channel_offset, vl);
    dx = __riscv_vsub_vv_i32m2 (load_channel (row, col + 2, channel_offset, vl), center, vl);
    dy = __riscv_vsub_vv_i32m2 (load_channel (row + 2, col, channel_offset, vl), center, vl);
    sqr_mag = __riscv_vadd_vv_i32m2 (__riscv_vmul_vv_i32m2 (dx, dx, vl),
                                     __riscv_vmul_vv_i32m2 (dy, dy, vl),
                                     vl);
  };

  for (std::size_t row = 0; row + 2 < height; ++row)
  {
    for (std::size_t col = 0; col + 2 < width;)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (width - 2 - col);

      vint32m2_t r_dx;
      vint32m2_t r_dy;
      vint32m2_t r_sqr_mag;
      vint32m2_t g_dx;
      vint32m2_t g_dy;
      vint32m2_t g_sqr_mag;
      vint32m2_t b_dx;
      vint32m2_t b_dy;
      vint32m2_t b_sqr_mag;
      compute_channel (row, col, r_offset, vl, r_dx, r_dy, r_sqr_mag);
      compute_channel (row, col, g_offset, vl, g_dx, g_dy, g_sqr_mag);
      compute_channel (row, col, b_offset, vl, b_dx, b_dy, b_sqr_mag);

      const vbool16_t r_selected =
          __riscv_vmand_mm_b16 (__riscv_vmsgt_vv_i32m2_b16 (r_sqr_mag, g_sqr_mag, vl),
                                __riscv_vmsgt_vv_i32m2_b16 (r_sqr_mag, b_sqr_mag, vl),
                                vl);
      const vbool16_t g_selected =
          __riscv_vmand_mm_b16 (__riscv_vmnot_m_b16 (r_selected, vl),
                                __riscv_vmsgt_vv_i32m2_b16 (g_sqr_mag, b_sqr_mag, vl),
                                vl);

      vint32m2_t selected_dx = b_dx;
      vint32m2_t selected_dy = b_dy;
      vint32m2_t selected_sqr_mag = b_sqr_mag;
      selected_dx = __riscv_vmerge_vvm_i32m2 (selected_dx, g_dx, g_selected, vl);
      selected_dy = __riscv_vmerge_vvm_i32m2 (selected_dy, g_dy, g_selected, vl);
      selected_sqr_mag = __riscv_vmerge_vvm_i32m2 (selected_sqr_mag, g_sqr_mag, g_selected, vl);
      selected_dx = __riscv_vmerge_vvm_i32m2 (selected_dx, r_dx, r_selected, vl);
      selected_dy = __riscv_vmerge_vvm_i32m2 (selected_dy, r_dy, r_selected, vl);
      selected_sqr_mag = __riscv_vmerge_vvm_i32m2 (selected_sqr_mag, r_sqr_mag, r_selected, vl);

      const vfloat32m2_t dx = __riscv_vfcvt_f_x_v_f32m2 (selected_dx, vl);
      const vfloat32m2_t dy = __riscv_vfcvt_f_x_v_f32m2 (selected_dy, vl);
      const vfloat32m2_t sqr_mag = __riscv_vfcvt_f_x_v_f32m2 (selected_sqr_mag, vl);
      const vfloat32m2_t magnitude = __riscv_vfsqrt_v_f32m2 (sqr_mag, vl);
      vfloat32m2_t angle = pcl::atan2_RVV_f32m2 (dy, dx, vl);
      angle = __riscv_vfmul_vf_f32m2 (angle, radians_to_degrees, vl);
      const vbool16_t negative_y_axis =
          __riscv_vmand_mm_b16 (__riscv_vmfeq_vf_f32m2_b16 (dx, 0.0f, vl),
                                __riscv_vmflt_vf_f32m2_b16 (dy, 0.0f, vl),
                                vl);
      angle = __riscv_vmerge_vvm_f32m2 (angle,
                                        __riscv_vfmv_v_f_f32m2 (negative_half_pi_degrees, vl),
                                        negative_y_axis,
                                        vl);
      const vbool16_t angle_lt_low = __riscv_vmflt_vf_f32m2_b16 (angle, -180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2 (angle, __riscv_vfadd_vf_f32m2 (angle, 360.0f, vl), angle_lt_low, vl);
      const vbool16_t angle_ge_high = __riscv_vmfge_vf_f32m2_b16 (angle, 180.0f, vl);
      angle = __riscv_vmerge_vvm_f32m2 (angle, __riscv_vfsub_vf_f32m2 (angle, 360.0f, vl), angle_ge_high, vl);

      __riscv_vse32_v_f32m2 (magnitudes.data (), magnitude, vl);
      __riscv_vse32_v_f32m2 (angles.data (), angle, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
      {
        GradientXY gradient;
        gradient.x = static_cast<float> (col + lane);
        gradient.y = static_cast<float> (row);
        gradient.magnitude = magnitudes[lane];
        gradient.angle = angles[lane];
        color_gradients_ (col + lane + 1, row + 1) = gradient;
      }

      col += vl;
    }
  }
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT>
void
pcl::ColorGradientDOTModality<PointInT>::
computeDominantQuantizedGradients ()
{
  const std::size_t input_width = input_->width;
  const std::size_t input_height = input_->height;

  const std::size_t output_width = input_width / bin_size_;
  const std::size_t output_height = input_height / bin_size_;

  dominant_quantized_color_gradients_.resize (output_width, output_height);

  constexpr std::size_t num_gradient_bins = 7;

  constexpr float divisor = 180.0f / (num_gradient_bins - 1.0f);

  unsigned char * peak_pointer = dominant_quantized_color_gradients_.getData ();
  std::fill_n(peak_pointer, output_width*output_height, 0);

  for (std::size_t row_bin_index = 0; row_bin_index < output_height; ++row_bin_index)
  {
    for (std::size_t col_bin_index = 0; col_bin_index < output_width; ++col_bin_index)
    {
      const std::size_t x_position = col_bin_index * bin_size_;
      const std::size_t y_position = row_bin_index * bin_size_;

      float max_gradient = 0.0f;
      std::size_t max_gradient_pos_x = 0;
      std::size_t max_gradient_pos_y = 0;

      // find next location and value of maximum gradient magnitude in current region
      for (std::size_t row_sub_index = 0; row_sub_index < bin_size_; ++row_sub_index)
      {
        for (std::size_t col_sub_index = 0; col_sub_index < bin_size_; ++col_sub_index)
        {
          const float magnitude = color_gradients_ (col_sub_index + x_position, row_sub_index + y_position).magnitude;

          if (magnitude > max_gradient)
          {
            max_gradient = magnitude;
            max_gradient_pos_x = col_sub_index;
            max_gradient_pos_y = row_sub_index;
          }
        }
      }

      if (max_gradient >= gradient_magnitude_threshold_)
      {
        const std::size_t angle = static_cast<std::size_t> (180 + color_gradients_ (max_gradient_pos_x + x_position, max_gradient_pos_y + y_position).angle + 0.5f);
        const std::size_t bin_index = static_cast<std::size_t> ((angle >= 180 ? angle-180 : angle)/divisor);

        *peak_pointer |= 1 << bin_index;
      }

      if (*peak_pointer == 0)
      {
        *peak_pointer |= 1 << 7;
      }

      ++peak_pointer;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT>
pcl::QuantizedMap
pcl::ColorGradientDOTModality<PointInT>::
computeInvariantQuantizedMap (const MaskMap & mask,
                              const RegionXY & region)
{
  const std::size_t input_width = input_->width;
  const std::size_t input_height = input_->height;

  const std::size_t output_width = input_width / bin_size_;
  const std::size_t output_height = input_height / bin_size_;

  const std::size_t sub_start_x = region.x / bin_size_;
  const std::size_t sub_start_y = region.y / bin_size_;
  const std::size_t sub_width = region.width / bin_size_;
  const std::size_t sub_height = region.height / bin_size_;

  QuantizedMap map;
  map.resize (sub_width, sub_height);

  constexpr std::size_t num_gradient_bins = 7;
  constexpr std::size_t max_num_of_gradients = 7;

  const float divisor = 180.0f / (num_gradient_bins - 1.0f);

  float global_max_gradient = 0.0f;
  float local_max_gradient = 0.0f;

  unsigned char * peak_pointer = map.getData ();

  for (std::size_t row_bin_index = 0; row_bin_index < sub_height; ++row_bin_index)
  {
    for (std::size_t col_bin_index = 0; col_bin_index < sub_width; ++col_bin_index)
    {
      std::vector<std::size_t> x_coordinates;
      std::vector<std::size_t> y_coordinates;
      std::vector<float> values;

      for (int row_pixel_index = -static_cast<int> (bin_size_)/2;
           row_pixel_index <= static_cast<int> (bin_size_)/2;
           row_pixel_index += static_cast<int> (bin_size_)/2)
      {
        const std::size_t y_position = row_pixel_index + (sub_start_y + row_bin_index)*bin_size_;

        if (y_position < 0 || y_position >= input_height)
          continue;

        for (int col_pixel_index = -static_cast<int> (bin_size_)/2;
             col_pixel_index <= static_cast<int> (bin_size_)/2;
             col_pixel_index += static_cast<int> (bin_size_)/2)
        {
          const std::size_t x_position = col_pixel_index + (sub_start_x + col_bin_index)*bin_size_;
          std::size_t counter = 0;

          if (x_position < 0 || x_position >= input_width)
            continue;

          // find maximum gradient magnitude in current bin
          {
            local_max_gradient = 0.0f;
            for (std::size_t row_sub_index = 0; row_sub_index < bin_size_; ++row_sub_index)
            {
              for (std::size_t col_sub_index = 0; col_sub_index < bin_size_; ++col_sub_index)
              {
                const float magnitude = color_gradients_ (col_sub_index + x_position, row_sub_index + y_position).magnitude;

                if (magnitude > local_max_gradient)
                  local_max_gradient = magnitude;
              }
            }
          }

          if (local_max_gradient > global_max_gradient)
          {
            global_max_gradient = local_max_gradient;
          }

          // iteratively search for the largest gradients, set it to -1, search the next largest ... etc.
          while (true)
          {
            float max_gradient;
            std::size_t max_gradient_pos_x;
            std::size_t max_gradient_pos_y;

            // find next location and value of maximum gradient magnitude in current region
            {
              max_gradient = 0.0f;
              for (std::size_t row_sub_index = 0; row_sub_index < bin_size_; ++row_sub_index)
              {
                for (std::size_t col_sub_index = 0; col_sub_index < bin_size_; ++col_sub_index)
                {
                  const float magnitude = color_gradients_ (col_sub_index + x_position, row_sub_index + y_position).magnitude;

                  if (magnitude > max_gradient)
                  {
                    max_gradient = magnitude;
                    max_gradient_pos_x = col_sub_index;
                    max_gradient_pos_y = row_sub_index;
                  }
                }
              }
            }

            // TODO: really localMaxGradient and not maxGradient???
            if (local_max_gradient < gradient_magnitude_threshold_)
            {
              //*peakPointer |= 1 << (numOfGradientBins-1);
              break;
            }

            // TODO: replace gradient_magnitude_threshold_ here by a fixed ratio?
            if (/*max_gradient < (local_max_gradient * gradient_magnitude_threshold_) ||*/
                counter >= max_num_of_gradients)
            {
              break;
            }

            ++counter;

            const std::size_t angle = static_cast<std::size_t> (180 + color_gradients_ (max_gradient_pos_x + x_position, max_gradient_pos_y + y_position).angle + 0.5f);
            const std::size_t bin_index = static_cast<std::size_t> ((angle >= 180 ? angle-180 : angle)/divisor);

            *peak_pointer |= 1 << bin_index;

            x_coordinates.push_back (max_gradient_pos_x + x_position);
            y_coordinates.push_back (max_gradient_pos_y + y_position);
            values.push_back (max_gradient);

            color_gradients_ (max_gradient_pos_x + x_position, max_gradient_pos_y + y_position).magnitude = -1.0f;
          }

          // reset values which have been set to -1
          for (std::size_t value_index = 0; value_index < values.size (); ++value_index)
          {
            color_gradients_ (x_coordinates[value_index], y_coordinates[value_index]).magnitude = values[value_index];
          }

          x_coordinates.clear ();
          y_coordinates.clear ();
          values.clear ();
        }
      }

      if (*peak_pointer == 0)
      {
        *peak_pointer |= 1 << 7;
      }
      ++peak_pointer;
    }
  }

  return map;
}
