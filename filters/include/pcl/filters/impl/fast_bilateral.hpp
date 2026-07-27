/*
 * Software License Agreement (BSD License)
 *
 * Point Cloud Library (PCL) - www.pointclouds.org
 * Copyright (c) 2012-, Open Perception, Inc.
 * Copyright (c) 2004, Sylvain Paris and Francois Sillion

 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * * Neither the name of the copyright holder(s) nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * $Id$
 *
 */

#ifndef PCL_FILTERS_IMPL_FAST_BILATERAL_HPP_
#define PCL_FILTERS_IMPL_FAST_BILATERAL_HPP_

#include <pcl/common/io.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>

#include <cstdint>
#include <riscv_vector.h>
#include <type_traits>
#include <utility>
#endif

namespace pcl
{

template <typename PointT> bool
fastBilateralComputeBaseRangeStd (const pcl::PointCloud<PointT>& output,
                                  float& base_min,
                                  float& base_max)
{
  base_max = -std::numeric_limits<float>::max ();
  base_min = std::numeric_limits<float>::max ();
  bool found_finite = false;
  for (const auto& pt: output)
  {
    if (std::isfinite (pt.z))
    {
      base_max = std::max<float> (pt.z, base_max);
      base_min = std::min<float> (pt.z, base_min);
      found_finite = true;
    }
  }
  return found_finite;
}

template <typename PointT> void
fastBilateralReplaceNonFiniteZStd (pcl::PointCloud<PointT>& output,
                                   const float base_max)
{
  for (auto& pt: output)
  {
    if (!std::isfinite (pt.z))
    {
      pt.z = base_max;
    }
  }
}

#if defined(__RVV10__)

inline constexpr std::size_t kFastBilateralZMinPoints = 64;

template <typename T>
using FastBilateralScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT, typename = void>
struct FastBilateralZCompatible : std::false_type {};

template <typename PointT>
struct FastBilateralZCompatible<
    PointT,
    std::void_t<decltype(std::declval<PointT>().z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<FastBilateralScalar<decltype(std::declval<PointT>().z)>, float>> {};

template <typename PointT>
inline constexpr bool kFastBilateralZCompatible = FastBilateralZCompatible<PointT>::value;

template <typename PointT> bool
fastBilateralComputeBaseRangeRVV (const pcl::PointCloud<PointT>& output,
                                  float& base_min,
                                  float& base_max,
                                  bool& found_finite)
{
  const std::size_t n = output.size ();
  if (n < pcl::kFastBilateralZMinPoints)
    return false;

  float min_z = std::numeric_limits<float>::max ();
  float max_z = -std::numeric_limits<float>::max ();
  std::size_t finite_count = 0;

  const auto* base = reinterpret_cast<const std::uint8_t*> (output.data ());
  std::size_t i = 0;

  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const auto* z_ptr = reinterpret_cast<const float*> (base + i * sizeof (PointT) + offsetof (PointT, z));
    const vfloat32m2_t vz = pcl::rvv_load::strided_load_f32m2<sizeof (PointT)> (z_ptr, vl);

    // FastBilateral only needs the finite z range before the lattice pass.  The
    // AoS stride load plus finite mask preserves the scalar rule that NaN/Inf
    // are ignored and later replaced by base_max.
    vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (vz, vz, vl);
    finite = __riscv_vmand_mm_b16 (
        finite,
        __riscv_vmflt_vf_f32m2_b16 (
            __riscv_vfabs_v_f32m2 (vz, vl), std::numeric_limits<float>::infinity (), vl),
        vl);

    finite_count += __riscv_vcpop_m_b16 (finite, vl);
    min_z = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1_m (
        finite, vz, __riscv_vfmv_s_f_f32m1 (min_z, 1), vl));
    max_z = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1_m (
        finite, vz, __riscv_vfmv_s_f_f32m1 (max_z, 1), vl));

    i += vl;
  }

  found_finite = finite_count != 0;
  if (found_finite)
  {
    base_min = min_z;
    base_max = max_z;
  }
  return true;
}

template <typename PointT> bool
fastBilateralReplaceNonFiniteZRVV (pcl::PointCloud<PointT>& output,
                                   const float base_max)
{
  const std::size_t n = output.size ();
  if (n < pcl::kFastBilateralZMinPoints)
    return false;

  auto* base = reinterpret_cast<std::uint8_t*> (output.data ());
  std::size_t i = 0;

  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    auto* z_ptr = reinterpret_cast<float*> (base + i * sizeof (PointT) + offsetof (PointT, z));
    const vfloat32m2_t vz = pcl::rvv_load::strided_load_f32m2<sizeof (PointT)> (z_ptr, vl);

    vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (vz, vz, vl);
    finite = __riscv_vmand_mm_b16 (
        finite,
        __riscv_vmflt_vf_f32m2_b16 (
            __riscv_vfabs_v_f32m2 (vz, vl), std::numeric_limits<float>::infinity (), vl),
        vl);
    const vbool16_t replace = __riscv_vmnot_m_b16 (finite, vl);
    const vfloat32m2_t vmax = __riscv_vfmv_v_f_f32m2 (base_max, vl);
    pcl::rvv_store::masked_strided_store_f32m2<sizeof (PointT)> (replace, z_ptr, vmax, vl);

    i += vl;
  }
  return true;
}

#endif

template <typename PointT> void
FastBilateralFilter<PointT>::applyFilter (PointCloud &output)
{
  if (!input_->isOrganized ())
  {
    PCL_ERROR ("[pcl::FastBilateralFilter] Input cloud needs to be organized.\n");
    return;
  }

  copyPointCloud (*input_, output);
  float base_max = -std::numeric_limits<float>::max (),
        base_min = std::numeric_limits<float>::max ();
  bool found_finite = false;
#if defined(__RVV10__)
  if constexpr (pcl::kFastBilateralZCompatible<PointT>)
  {
    if (!pcl::fastBilateralComputeBaseRangeRVV<PointT> (output, base_min, base_max, found_finite))
      found_finite = pcl::fastBilateralComputeBaseRangeStd<PointT> (output, base_min, base_max);
  }
  else
  {
    found_finite = pcl::fastBilateralComputeBaseRangeStd<PointT> (output, base_min, base_max);
  }
#else
  found_finite = pcl::fastBilateralComputeBaseRangeStd<PointT> (output, base_min, base_max);
#endif
  if (!found_finite)
  {
    PCL_WARN ("[pcl::FastBilateralFilter] Given an empty cloud. Doing nothing.\n");
    return;
  }

#if defined(__RVV10__)
  if constexpr (pcl::kFastBilateralZCompatible<PointT>)
  {
    if (!pcl::fastBilateralReplaceNonFiniteZRVV<PointT> (output, base_max))
      pcl::fastBilateralReplaceNonFiniteZStd<PointT> (output, base_max);
  }
  else
  {
    pcl::fastBilateralReplaceNonFiniteZStd<PointT> (output, base_max);
  }
#else
  pcl::fastBilateralReplaceNonFiniteZStd<PointT> (output, base_max);
#endif

  const float base_delta = base_max - base_min;

  const std::size_t padding_xy = 2;
  const std::size_t padding_z  = 2;

  const std::size_t small_width  = static_cast<std::size_t> (static_cast<float> (input_->width  - 1) / sigma_s_) + 1 + 2 * padding_xy;
  const std::size_t small_height = static_cast<std::size_t> (static_cast<float> (input_->height - 1) / sigma_s_) + 1 + 2 * padding_xy;
  const std::size_t small_depth  = static_cast<std::size_t> (base_delta / sigma_r_)   + 1 + 2 * padding_z;


  Array3D data (small_width, small_height, small_depth);
  for (std::size_t x = 0; x < input_->width; ++x)
  {
    const std::size_t small_x = static_cast<std::size_t> (static_cast<float> (x) / sigma_s_ + 0.5f) + padding_xy;
    for (std::size_t y = 0; y < input_->height; ++y)
    {
      const float z = output (x,y).z - base_min;

      const std::size_t small_y = static_cast<std::size_t> (static_cast<float> (y) / sigma_s_ + 0.5f) + padding_xy;
      const std::size_t small_z = static_cast<std::size_t> (static_cast<float> (z) / sigma_r_ + 0.5f) + padding_z;

      Eigen::Vector2f& d = data (small_x, small_y, small_z);
      d[0] += output (x,y).z;
      d[1] += 1.0f;
    }
  }


  std::vector<long int> offset (3);
  offset[0] = &(data (1,0,0)) - &(data (0,0,0));
  offset[1] = &(data (0,1,0)) - &(data (0,0,0));
  offset[2] = &(data (0,0,1)) - &(data (0,0,0));

  Array3D buffer (small_width, small_height, small_depth);

  for (std::size_t dim = 0; dim < 3; ++dim)
  {
    const long int off = offset[dim];
    for (std::size_t n_iter = 0; n_iter < 2; ++n_iter)
    {
      std::swap (buffer, data);
      for(std::size_t x = 1; x < small_width - 1; ++x)
        for(std::size_t y = 1; y < small_height - 1; ++y)
        {
          Eigen::Vector2f* d_ptr = &(data (x,y,1));
          Eigen::Vector2f* b_ptr = &(buffer (x,y,1));

          for(std::size_t z = 1; z < small_depth - 1; ++z, ++d_ptr, ++b_ptr)
            *d_ptr = (*(b_ptr - off) + *(b_ptr + off) + 2.0 * (*b_ptr)) / 4.0;
        }
    }
  }

  if (early_division_)
  {
    for (auto d = data.begin (); d != data.end (); ++d)
      *d /= ((*d)[0] != 0) ? (*d)[1] : 1;

    for (std::size_t x = 0; x < input_->width; x++)
      for (std::size_t y = 0; y < input_->height; y++)
      {
        const float z = output (x,y).z - base_min;
        const Eigen::Vector2f D = data.trilinear_interpolation (static_cast<float> (x) / sigma_s_ + padding_xy,
                                                                static_cast<float> (y) / sigma_s_ + padding_xy,
                                                                z / sigma_r_ + padding_z);
        output(x,y).z = D[0];
      }
  }
  else
  {
    for (std::size_t x = 0; x < input_->width; ++x)
      for (std::size_t y = 0; y < input_->height; ++y)
      {
        const float z = output (x,y).z - base_min;
        const Eigen::Vector2f D = data.trilinear_interpolation (static_cast<float> (x) / sigma_s_ + padding_xy,
                                                                static_cast<float> (y) / sigma_s_ + padding_xy,
                                                                z / sigma_r_ + padding_z);
        output (x,y).z = D[0] / D[1];
      }
  }
}


template <typename PointT> std::size_t
FastBilateralFilter<PointT>::Array3D::clamp (const std::size_t min_value,
                                             const std::size_t max_value,
                                             const std::size_t x)
{
  if (x >= min_value && x <= max_value)
  {
    return x;
  }
  if (x < min_value)
  {
    return (min_value);
  }
  return (max_value);
}


template <typename PointT> Eigen::Vector2f
FastBilateralFilter<PointT>::Array3D::trilinear_interpolation (const float x,
                                                               const float y,
                                                               const float z)
{
  const std::size_t x_index  = clamp (0, x_dim_ - 1, static_cast<std::size_t> (x));
  const std::size_t xx_index = clamp (0, x_dim_ - 1, x_index + 1);

  const std::size_t y_index  = clamp (0, y_dim_ - 1, static_cast<std::size_t> (y));
  const std::size_t yy_index = clamp (0, y_dim_ - 1, y_index + 1);

  const std::size_t z_index  = clamp (0, z_dim_ - 1, static_cast<std::size_t> (z));
  const std::size_t zz_index = clamp (0, z_dim_ - 1, z_index + 1);

  const float x_alpha = x - static_cast<float> (x_index);
  const float y_alpha = y - static_cast<float> (y_index);
  const float z_alpha = z - static_cast<float> (z_index);

  return
      (1.0f-x_alpha) * (1.0f-y_alpha) * (1.0f-z_alpha) * (*this)(x_index, y_index, z_index) +
      x_alpha        * (1.0f-y_alpha) * (1.0f-z_alpha) * (*this)(xx_index, y_index, z_index) +
      (1.0f-x_alpha) * y_alpha        * (1.0f-z_alpha) * (*this)(x_index, yy_index, z_index) +
      x_alpha        * y_alpha        * (1.0f-z_alpha) * (*this)(xx_index, yy_index, z_index) +
      (1.0f-x_alpha) * (1.0f-y_alpha) * z_alpha        * (*this)(x_index, y_index, zz_index) +
      x_alpha        * (1.0f-y_alpha) * z_alpha        * (*this)(xx_index, y_index, zz_index) +
      (1.0f-x_alpha) * y_alpha        * z_alpha        * (*this)(x_index, yy_index, zz_index) +
      x_alpha        * y_alpha        * z_alpha        * (*this)(xx_index, yy_index, zz_index);
}

} // namespace pcl

#endif /* PCL_FILTERS_IMPL_FAST_BILATERAL_HPP_ */
