/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2009-2011, Willow Garage, Inc.
 *  Copyright (c) 2015, Google, Inc.
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
 *   * Neither the name of the copyright holder(s) nor the names of its
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
 * $Id: extract_indices.hpp 1897 2011-07-26 20:35:49Z rusu $
 *
 */

#ifndef PCL_FILTERS_IMPL_CROP_BOX_H_
#define PCL_FILTERS_IMPL_CROP_BOX_H_

#include <pcl/filters/crop_box.h>
#include <pcl/common/eigen.h> // for getTransformation
#include <pcl/common/point_tests.h> // for isFinite
#include <pcl/common/transforms.h> // for transformPoint

#if defined(__RVV10__)
#include <cstdint>
#include <limits>
#include <riscv_vector.h>
#include <type_traits>
#include <utility>
#endif

namespace pcl
{

#if defined(__RVV10__)

inline constexpr std::size_t kCropBoxIndicesMinPoints = 64;

template <typename T>
using CropBoxScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT, typename = void>
struct CropBoxXYZCompatible : std::false_type {};

template <typename PointT>
struct CropBoxXYZCompatible<
    PointT,
    std::void_t<decltype(std::declval<PointT>().x),
                decltype(std::declval<PointT>().y),
                decltype(std::declval<PointT>().z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<CropBoxScalar<decltype(std::declval<PointT>().x)>, float> &&
      std::is_same_v<CropBoxScalar<decltype(std::declval<PointT>().y)>, float> &&
      std::is_same_v<CropBoxScalar<decltype(std::declval<PointT>().z)>, float>> {};

template <typename PointT>
inline constexpr bool kCropBoxXYZCompatible = CropBoxXYZCompatible<PointT>::value;

#endif

} // namespace pcl

///////////////////////////////////////////////////////////////////////////////
template<typename PointT> void
pcl::CropBox<PointT>::applyFilterIndicesStd (Indices &indices)
{
  indices.resize (input_->size ());
  removed_indices_->resize (input_->size ());
  int indices_count = 0;
  int removed_indices_count = 0;

  Eigen::Affine3f transform = Eigen::Affine3f::Identity ();
  Eigen::Affine3f inverse_transform = Eigen::Affine3f::Identity ();

  if (rotation_ != Eigen::Vector3f::Zero ())
  {
    pcl::getTransformation (0, 0, 0,
                            rotation_ (0), rotation_ (1), rotation_ (2),
                            transform);
    inverse_transform = transform.inverse ();
  }

  bool transform_matrix_is_identity = transform_.matrix ().isIdentity ();
  bool translation_is_zero = (translation_ == Eigen::Vector3f::Zero ());
  bool inverse_transform_matrix_is_identity = inverse_transform.matrix ().isIdentity ();

  for (const auto index : *indices_)
  {
    if (!input_->is_dense)
      // Check if the point is invalid
      if (!isFinite ((*input_)[index]))
        continue;

    // Get local point
    PointT local_pt = (*input_)[index];

    // Transform point to world space
    if (!transform_matrix_is_identity)
      local_pt = pcl::transformPoint<PointT> (local_pt, transform_);

    if (!translation_is_zero)
    {
      local_pt.x -= translation_ (0);
      local_pt.y -= translation_ (1);
      local_pt.z -= translation_ (2);
    }

    // Transform point to local space of crop box
    if (!inverse_transform_matrix_is_identity)
      local_pt = pcl::transformPoint<PointT> (local_pt, inverse_transform);

    // If outside the cropbox
    if ( (local_pt.x < min_pt_[0] || local_pt.y < min_pt_[1] || local_pt.z < min_pt_[2]) ||
         (local_pt.x > max_pt_[0] || local_pt.y > max_pt_[1] || local_pt.z > max_pt_[2]))
    {
      if (negative_)
        indices[indices_count++] = index;
      else if (extract_removed_indices_)
        (*removed_indices_)[removed_indices_count++] = index;
    }
    // If inside the cropbox
    else
    {
      if (negative_ && extract_removed_indices_)
        (*removed_indices_)[removed_indices_count++] = index;
      else if (!negative_)
        indices[indices_count++] = index;
    }
  }
  indices.resize (indices_count);
  removed_indices_->resize (removed_indices_count);
}

#if defined(__RVV10__)

template<typename PointT> bool
pcl::CropBox<PointT>::applyFilterIndicesRVV (Indices &indices)
{
  const std::size_t n = indices_->size ();
  if (!fake_indices_ ||
      !input_ ||
      !input_->is_dense ||
      n < pcl::kCropBoxIndicesMinPoints ||
      n > static_cast<std::size_t> (std::numeric_limits<int>::max ()) ||
      rotation_ != Eigen::Vector3f::Zero () ||
      translation_ != Eigen::Vector3f::Zero () ||
      !transform_.matrix ().isIdentity ())
    return false;

  indices.resize (n);
  if (extract_removed_indices_)
    removed_indices_->resize (n);
  else
    removed_indices_->clear ();

  const auto* base = reinterpret_cast<const std::uint8_t*> (input_->data ());
  int* out = indices.data ();
  int* removed = extract_removed_indices_ ? removed_indices_->data () : nullptr;
  std::size_t kept = 0;
  std::size_t dropped = 0;
  std::size_t i = 0;

  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const auto* chunk = base + i * sizeof (PointT);
    const auto stride = static_cast<ptrdiff_t> (sizeof (PointT));

    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + offsetof (PointT, x)), stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + offsetof (PointT, y)), stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + offsetof (PointT, z)), stride, vl);

    // Dense identity CropBox has no scalar finite check.  The RVV path mirrors
    // the scalar six bound comparisons and uses vcompress to preserve index order.
    vbool16_t inside = __riscv_vmnot_m_b16 (__riscv_vmflt_vf_f32m2_b16 (vx, min_pt_[0], vl), vl);
    inside = __riscv_vmand_mm_b16 (inside, __riscv_vmnot_m_b16 (__riscv_vmflt_vf_f32m2_b16 (vy, min_pt_[1], vl), vl), vl);
    inside = __riscv_vmand_mm_b16 (inside, __riscv_vmnot_m_b16 (__riscv_vmflt_vf_f32m2_b16 (vz, min_pt_[2], vl), vl), vl);
    inside = __riscv_vmand_mm_b16 (inside, __riscv_vmnot_m_b16 (__riscv_vmfgt_vf_f32m2_b16 (vx, max_pt_[0], vl), vl), vl);
    inside = __riscv_vmand_mm_b16 (inside, __riscv_vmnot_m_b16 (__riscv_vmfgt_vf_f32m2_b16 (vy, max_pt_[1], vl), vl), vl);
    inside = __riscv_vmand_mm_b16 (inside, __riscv_vmnot_m_b16 (__riscv_vmfgt_vf_f32m2_b16 (vz, max_pt_[2], vl), vl), vl);

    const vbool16_t keep = negative_ ? __riscv_vmnot_m_b16 (inside, vl) : inside;
    const vbool16_t removed_mask = negative_ ? inside : __riscv_vmnot_m_b16 (inside, vl);

    const vuint32m2_t local = __riscv_vid_v_u32m2 (vl);
    const vuint32m2_t source = __riscv_vadd_vx_u32m2 (local, static_cast<std::uint32_t> (i), vl);
    const vint32m2_t source_i32 = __riscv_vreinterpret_v_u32m2_i32m2 (source);

    const vint32m2_t kept_i32 = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16 (keep, vl);
    __riscv_vse32_v_i32m2 (out + kept, kept_i32, keep_count);
    kept += keep_count;

    if (extract_removed_indices_)
    {
      const vint32m2_t removed_i32 = __riscv_vcompress_vm_i32m2 (source_i32, removed_mask, vl);
      const std::size_t removed_count = __riscv_vcpop_m_b16 (removed_mask, vl);
      __riscv_vse32_v_i32m2 (removed + dropped, removed_i32, removed_count);
      dropped += removed_count;
    }

    i += vl;
  }

  indices.resize (kept);
  removed_indices_->resize (extract_removed_indices_ ? dropped : 0);
  return true;
}

#endif

template<typename PointT> void
pcl::CropBox<PointT>::applyFilter (Indices &indices)
{
#if defined(__RVV10__)
  if constexpr (pcl::kCropBoxXYZCompatible<PointT>)
  {
    if (applyFilterIndicesRVV (indices))
      return;
  }
#endif

  applyFilterIndicesStd (indices);
}

#define PCL_INSTANTIATE_CropBox(T) template class PCL_EXPORTS pcl::CropBox<T>;

#endif    // PCL_FILTERS_IMPL_CROP_BOX_H_
