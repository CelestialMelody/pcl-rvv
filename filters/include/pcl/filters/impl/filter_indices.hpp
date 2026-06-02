/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
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
 * $Id: filter.hpp 1800 2011-07-15 11:45:31Z marton $
 *
 */

#ifndef PCL_FILTERS_IMPL_FILTER_INDICES_H_
#define PCL_FILTERS_IMPL_FILTER_INDICES_H_

#include <pcl/filters/filter_indices.h>
#include <pcl/point_types.h>             // for PointXYZ

#if defined(__RVV10__)
#include <cstdint>
#include <limits>
#include <riscv_vector.h>
#include <type_traits>
#include <utility>
#endif

namespace pcl
{

template <typename PointT>
inline void
removeNaNFromPointCloudIndicesStd(const pcl::PointCloud<PointT> &cloud_in, Indices &index)
{
  // Reserve enough space for the indices
  index.resize (cloud_in.size ());

  // If the data is dense, we don't need to check for NaN
  if (cloud_in.is_dense)
  {
    for (int j = 0; j < static_cast<int> (cloud_in.size ()); ++j)
      index[j] = j;
  }
  else
  {
    int j = 0;
    for (int i = 0; i < static_cast<int> (cloud_in.size ()); ++i)
    {
      if (!std::isfinite (cloud_in[i].x) ||
          !std::isfinite (cloud_in[i].y) ||
          !std::isfinite (cloud_in[i].z))
        continue;
      index[j] = i;
      j++;
    }
    if (j != static_cast<int> (cloud_in.size ()))
    {
      // Resize to the correct size
      index.resize (j);
    }
  }
}

#if defined(__RVV10__)

inline constexpr std::size_t kRemoveNaNIndicesMinPoints = 64;

template <typename T>
using FilterIndicesScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT, typename = void>
struct FilterIndicesXYZCompatible : std::false_type {};

template <typename PointT>
struct FilterIndicesXYZCompatible<
    PointT,
    std::void_t<decltype(std::declval<PointT>().x),
                decltype(std::declval<PointT>().y),
                decltype(std::declval<PointT>().z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<FilterIndicesScalar<decltype(std::declval<PointT>().x)>, float> &&
      std::is_same_v<FilterIndicesScalar<decltype(std::declval<PointT>().y)>, float> &&
      std::is_same_v<FilterIndicesScalar<decltype(std::declval<PointT>().z)>, float>> {};

template <typename PointT>
inline constexpr bool kFilterIndicesXYZCompatible = FilterIndicesXYZCompatible<PointT>::value;

template <typename PointT>
inline bool
removeNaNFromPointCloudIndicesRVV(const pcl::PointCloud<PointT> &cloud_in, Indices &index)
{
  const std::size_t n = cloud_in.size();
  if (cloud_in.is_dense || n < kRemoveNaNIndicesMinPoints || n > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return false;

  index.resize (n);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud_in.data());
  int* out = index.data();
  std::size_t kept = 0;
  std::size_t i = 0;

  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* chunk = base + i * sizeof(PointT);
    const auto stride = static_cast<ptrdiff_t>(sizeof(PointT));

    const auto* x_ptr = reinterpret_cast<const float*>(chunk + offsetof(PointT, x));
    const auto* y_ptr = reinterpret_cast<const float*>(chunk + offsetof(PointT, y));
    const auto* z_ptr = reinterpret_cast<const float*>(chunk + offsetof(PointT, z));
    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(x_ptr, stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(y_ptr, stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(z_ptr, stride, vl);

    // NaN fails x == x; +/-Inf is rejected by abs(v) < Inf. vcompress keeps the
    // scalar scan order while each VL chunk writes only finite source indices.
    vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(vx, vx, vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(vy, vy, vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(vz, vz, vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), std::numeric_limits<float>::infinity(), vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), std::numeric_limits<float>::infinity(), vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), std::numeric_limits<float>::infinity(), vl), vl);

    const vuint32m2_t local = __riscv_vid_v_u32m2(vl);
    const vuint32m2_t source = __riscv_vadd_vx_u32m2(local, static_cast<std::uint32_t>(i), vl);
    const vint32m2_t source_i32 = __riscv_vreinterpret_v_u32m2_i32m2(source);
    const vint32m2_t compact = __riscv_vcompress_vm_i32m2(source_i32, finite, vl);
    const std::size_t count = __riscv_vcpop_m_b16(finite, vl);
    __riscv_vse32_v_i32m2(out + kept, compact, count);

    kept += count;
    i += vl;
  }

  index.resize (kept);
  return true;
}

#endif

} // namespace pcl

template <typename PointT> void
pcl::removeNaNFromPointCloud (const pcl::PointCloud<PointT> &cloud_in,
                              Indices &index)
{
#if defined(__RVV10__)
  if constexpr (pcl::kFilterIndicesXYZCompatible<PointT>)
    if (pcl::removeNaNFromPointCloudIndicesRVV (cloud_in, index))
      return;
#endif
  pcl::removeNaNFromPointCloudIndicesStd (cloud_in, index);
}

template<typename PointT> void
pcl::FilterIndices<PointT>::applyFilter (PointCloud &output)
{
  Indices indices;
  if (keep_organized_)
  {
    if (!extract_removed_indices_)
    {
      PCL_WARN ("[pcl::FilterIndices<PointT>::applyFilter] extract_removed_indices_ was set to 'true' to keep the point cloud organized.\n");
      extract_removed_indices_ = true;
    }
    applyFilter (indices);

    output = *input_;

    // To preserve legacy behavior, only coordinates xyz are filtered.
    // Copying a PointXYZ initialized with the user_filter_value_ into a generic
    // PointT, ensures only the xyz coordinates, if they exist at destination,
    // are overwritten.
    const PointXYZ ufv (user_filter_value_, user_filter_value_, user_filter_value_);
    for (const auto ri : *removed_indices_)  // ri = removed index
      copyPoint(ufv, output[ri]);
    if (!std::isfinite (user_filter_value_))
      output.is_dense = false;
  }
  else
  {
    applyFilter (indices);
    pcl::copyPointCloud (*input_, indices, output);
  }
}


#define PCL_INSTANTIATE_removeNanFromPointCloud(T) template PCL_EXPORTS void pcl::removeNaNFromPointCloud<T>(const pcl::PointCloud<T>&, Indices&);
#define PCL_INSTANTIATE_FilterIndices(T) template class PCL_EXPORTS  pcl::FilterIndices<T>;

#endif    // PCL_FILTERS_IMPL_FILTER_INDICES_H_
