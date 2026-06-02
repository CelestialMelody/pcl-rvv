/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2012, Willow Garage, Inc.
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
 * $Id$
 *
 */

#ifndef PCL_FILTERS_IMPL_PASSTHROUGH_HPP_
#define PCL_FILTERS_IMPL_PASSTHROUGH_HPP_

#include <pcl/filters/passthrough.h>

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

inline constexpr std::size_t kPassThroughIndicesMinPoints = 64;

template <typename T>
using PassThroughScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT, typename = void>
struct PassThroughXYZCompatible : std::false_type {};

template <typename PointT>
struct PassThroughXYZCompatible<
    PointT,
    std::void_t<decltype(std::declval<PointT>().x),
                decltype(std::declval<PointT>().y),
                decltype(std::declval<PointT>().z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<PassThroughScalar<decltype(std::declval<PointT>().x)>, float> &&
      std::is_same_v<PassThroughScalar<decltype(std::declval<PointT>().y)>, float> &&
      std::is_same_v<PassThroughScalar<decltype(std::declval<PointT>().z)>, float>> {};

template <typename PointT>
inline constexpr bool kPassThroughXYZCompatible = PassThroughXYZCompatible<PointT>::value;

#endif

} // namespace pcl

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::PassThrough<PointT>::applyFilterIndicesStd (Indices &indices)
{
  // The arrays to be used
  indices.resize (indices_->size ());
  removed_indices_->resize (indices_->size ());
  int oii = 0, rii = 0;  // oii = output indices iterator, rii = removed indices iterator

  // Has a field name been specified?
  if (filter_field_name_.empty ())
  {
    // Only filter for non-finite entries then
    for (const auto ii : *indices_)  // ii = input index
    {
      // Non-finite entries are always passed to removed indices
      if (!std::isfinite ((*input_)[ii].x) ||
          !std::isfinite ((*input_)[ii].y) ||
          !std::isfinite ((*input_)[ii].z))
      {
        if (extract_removed_indices_)
          (*removed_indices_)[rii++] = ii;
        continue;
      }
      indices[oii++] = ii;
    }
  }
  else
  {
    // Attempt to get the field name's index
    std::vector<pcl::PCLPointField> fields;
    int distance_idx = pcl::getFieldIndex<PointT> (filter_field_name_, fields);
    if (distance_idx == -1)
    {
      PCL_WARN ("[pcl::%s::applyFilter] Unable to find field name in point type.\n", getClassName ().c_str ());
      indices.clear ();
      removed_indices_->clear ();
      return;
    }
    if (fields[distance_idx].datatype != pcl::PCLPointField::PointFieldTypes::FLOAT32)
    {
      PCL_ERROR ("[pcl::%s::applyFilter] PassThrough currently only works with float32 fields. To filter fields of other types see ConditionalRemoval or FunctorFilter/FunctionFilter.\n", getClassName ().c_str ());
      indices.clear ();
      removed_indices_->clear ();
      return;
    }
    if (filter_field_name_ == "rgb")
      PCL_WARN ("[pcl::%s::applyFilter] You told PassThrough to operate on the 'rgb' field. This will likely not do what you expect. Consider using ConditionalRemoval or FunctorFilter/FunctionFilter.\n", getClassName ().c_str ());
    const auto field_offset = fields[distance_idx].offset;

    // Filter for non-finite entries and the specified field limits
    for (const auto ii : *indices_)  // ii = input index
    {
      // Non-finite entries are always passed to removed indices
      if (!std::isfinite ((*input_)[ii].x) ||
          !std::isfinite ((*input_)[ii].y) ||
          !std::isfinite ((*input_)[ii].z))
      {
        if (extract_removed_indices_)
          (*removed_indices_)[rii++] = ii;
        continue;
      }

      // Get the field's value
      const auto* pt_data = reinterpret_cast<const std::uint8_t*> (&(*input_)[ii]);
      float field_value = 0;
      memcpy (&field_value, pt_data + field_offset, sizeof (float));

      // Remove NAN/INF/-INF values. We expect passthrough to output clean valid data.
      if (!std::isfinite (field_value))
      {
        if (extract_removed_indices_)
          (*removed_indices_)[rii++] = ii;
        continue;
      }

      // Outside of the field limits are passed to removed indices
      if (!negative_ && (field_value < filter_limit_min_ || field_value > filter_limit_max_))
      {
        if (extract_removed_indices_)
          (*removed_indices_)[rii++] = ii;
        continue;
      }

      // Inside of the field limits are passed to removed indices if negative was set
      if (negative_ && field_value >= filter_limit_min_ && field_value <= filter_limit_max_)
      {
        if (extract_removed_indices_)
          (*removed_indices_)[rii++] = ii;
        continue;
      }

      // Otherwise it was a normal point for output (inlier)
      indices[oii++] = ii;
    }
  }

  // Resize the output arrays
  indices.resize (oii);
  removed_indices_->resize (rii);
}

#if defined(__RVV10__)

template <typename PointT> bool
pcl::PassThrough<PointT>::applyFilterIndicesRVV (Indices &indices, std::size_t field_offset)
{
  const std::size_t n = indices_->size ();
  if (!fake_indices_ ||
      n < pcl::kPassThroughIndicesMinPoints ||
      n > static_cast<std::size_t> (std::numeric_limits<int>::max ()) ||
      field_offset % alignof (float) != 0 ||
      !input_)
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
    const vfloat32m2_t vf = __riscv_vlse32_v_f32m2 (reinterpret_cast<const float*> (chunk + field_offset), stride, vl);

    // This path only covers identity input indices.  AoS fields are read with
    // byte stride, and vcompress preserves scalar scan order inside each VL chunk.
    vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (vx, vx, vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (vy, vy, vl), vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (vz, vz, vl), vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (vf, vf, vl), vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmflt_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (vx, vl), std::numeric_limits<float>::infinity (), vl), vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmflt_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (vy, vl), std::numeric_limits<float>::infinity (), vl), vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmflt_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (vz, vl), std::numeric_limits<float>::infinity (), vl), vl);
    finite = __riscv_vmand_mm_b16 (finite, __riscv_vmflt_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (vf, vl), std::numeric_limits<float>::infinity (), vl), vl);

    vbool16_t in_range = __riscv_vmnot_m_b16 (__riscv_vmflt_vf_f32m2_b16 (vf, filter_limit_min_, vl), vl);
    in_range = __riscv_vmand_mm_b16 (in_range, __riscv_vmnot_m_b16 (__riscv_vmfgt_vf_f32m2_b16 (vf, filter_limit_max_, vl), vl), vl);

    vbool16_t keep = negative_ ? __riscv_vmnot_m_b16 (in_range, vl) : in_range;
    keep = __riscv_vmand_mm_b16 (keep, finite, vl);
    const vbool16_t drop = __riscv_vmnot_m_b16 (keep, vl);

    const vuint32m2_t local = __riscv_vid_v_u32m2 (vl);
    const vuint32m2_t source = __riscv_vadd_vx_u32m2 (local, static_cast<std::uint32_t> (i), vl);
    const vint32m2_t source_i32 = __riscv_vreinterpret_v_u32m2_i32m2 (source);

    const vint32m2_t kept_i32 = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16 (keep, vl);
    __riscv_vse32_v_i32m2 (out + kept, kept_i32, keep_count);
    kept += keep_count;

    if (extract_removed_indices_)
    {
      const vint32m2_t removed_i32 = __riscv_vcompress_vm_i32m2 (source_i32, drop, vl);
      const std::size_t drop_count = __riscv_vcpop_m_b16 (drop, vl);
      __riscv_vse32_v_i32m2 (removed + dropped, removed_i32, drop_count);
      dropped += drop_count;
    }

    i += vl;
  }

  indices.resize (kept);
  removed_indices_->resize (extract_removed_indices_ ? dropped : 0);
  return true;
}

#endif

template <typename PointT> void
pcl::PassThrough<PointT>::applyFilterIndices (Indices &indices)
{
#if defined(__RVV10__)
  if constexpr (pcl::kPassThroughXYZCompatible<PointT>)
  {
    if (!filter_field_name_.empty () &&
        fake_indices_ &&
        indices_->size () >= pcl::kPassThroughIndicesMinPoints &&
        indices_->size () <= static_cast<std::size_t> (std::numeric_limits<int>::max ()) &&
        input_)
    {
      std::vector<pcl::PCLPointField> fields;
      int distance_idx = pcl::getFieldIndex<PointT> (filter_field_name_, fields);
      if (distance_idx == -1)
      {
        PCL_WARN ("[pcl::%s::applyFilter] Unable to find field name in point type.\n", getClassName ().c_str ());
        indices.clear ();
        removed_indices_->clear ();
        return;
      }
      if (fields[distance_idx].datatype != pcl::PCLPointField::PointFieldTypes::FLOAT32)
      {
        PCL_ERROR ("[pcl::%s::applyFilter] PassThrough currently only works with float32 fields. To filter fields of other types see ConditionalRemoval or FunctorFilter/FunctionFilter.\n", getClassName ().c_str ());
        indices.clear ();
        removed_indices_->clear ();
        return;
      }
      if (filter_field_name_ == "rgb")
        PCL_WARN ("[pcl::%s::applyFilter] You told PassThrough to operate on the 'rgb' field. This will likely not do what you expect. Consider using ConditionalRemoval or FunctorFilter/FunctionFilter.\n", getClassName ().c_str ());

      if (applyFilterIndicesRVV (indices, fields[distance_idx].offset))
        return;
    }
  }
#endif

  applyFilterIndicesStd (indices);
}

#define PCL_INSTANTIATE_PassThrough(T) template class PCL_EXPORTS pcl::PassThrough<T>;

#endif  // PCL_FILTERS_IMPL_PASSTHROUGH_HPP_
