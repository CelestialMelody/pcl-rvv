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
 * $Id$
 *
 */

#pragma once

#include <pcl/pcl_exports.h> // for PCL_EXPORTS
#include <pcl/common/point_tests.h> // for pcl::isFinite
#include <pcl/filters/filter.h>

#if defined(__RVV10__)
#include <pcl/common/rvv_point_traits.h>

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
removeNaNFromPointCloudStd(const pcl::PointCloud<PointT> &cloud_in,
                           pcl::PointCloud<PointT> &cloud_out,
                           Indices &index)
{
  // If the clouds are not the same, prepare the output
  if (&cloud_in != &cloud_out)
  {
    cloud_out.header = cloud_in.header;
    cloud_out.resize (cloud_in.size ());
    cloud_out.sensor_origin_ = cloud_in.sensor_origin_;
    cloud_out.sensor_orientation_ = cloud_in.sensor_orientation_;
  }
  // Reserve enough space for the indices
  index.resize (cloud_in.size ());

  // If the data is dense, we don't need to check for NaN
  if (cloud_in.is_dense)
  {
    // Simply copy the data
    cloud_out = cloud_in;
    for (std::size_t j = 0; j < cloud_out.size (); ++j)
      index[j] = j;
  }
  else
  {
    std::size_t j = 0;
    for (std::size_t i = 0; i < cloud_in.size (); ++i)
    {
      if (!std::isfinite (cloud_in[i].x) ||
          !std::isfinite (cloud_in[i].y) ||
          !std::isfinite (cloud_in[i].z))
        continue;
      cloud_out[j] = cloud_in[i];
      index[j] = i;
      j++;
    }
    if (j != cloud_in.size ())
    {
      // Resize to the correct size
      cloud_out.resize (j);
      index.resize (j);
    }

    cloud_out.height = 1;
    cloud_out.width  = static_cast<std::uint32_t>(j);

    // Removing bad points => dense (note: 'dense' doesn't mean 'organized')
    cloud_out.is_dense = true;
  }
}

template <typename PointT>
inline void
removeNaNNormalsFromPointCloudStd(const pcl::PointCloud<PointT> &cloud_in,
                                  pcl::PointCloud<PointT> &cloud_out,
                                  Indices &index)
{
  // If the clouds are not the same, prepare the output
  if (&cloud_in != &cloud_out)
  {
    cloud_out.header = cloud_in.header;
    cloud_out.resize (cloud_in.size ());
    cloud_out.sensor_origin_ = cloud_in.sensor_origin_;
    cloud_out.sensor_orientation_ = cloud_in.sensor_orientation_;
  }
  // Reserve enough space for the indices
  index.resize (cloud_in.size ());
  std::size_t j = 0;

  // Assume cloud is dense
  cloud_out.is_dense = true;

  for (std::size_t i = 0; i < cloud_in.size (); ++i)
  {
    if (!std::isfinite (cloud_in[i].normal_x) ||
        !std::isfinite (cloud_in[i].normal_y) ||
        !std::isfinite (cloud_in[i].normal_z))
      continue;
    if (cloud_out.is_dense && !pcl::isFinite(cloud_in[i]))
      cloud_out.is_dense = false;
    cloud_out[j] = cloud_in[i];
    index[j] = i;
    j++;
  }
  if (j != cloud_in.size ())
  {
    // Resize to the correct size
    cloud_out.resize (j);
    index.resize (j);
  }

  cloud_out.height = 1;
  cloud_out.width  = j;
}

#if defined(__RVV10__)

inline constexpr std::size_t kRemoveNaNCloudMinPoints = 64;

template <typename PointT>
inline constexpr bool kFilterXYZCompatible =
    pcl::rvv::kRVVXYZPointCompatible<PointT>;

template <typename PointT, typename = void>
struct FilterNormalCompatible : std::false_type {};

template <typename PointT>
struct FilterNormalCompatible<
    PointT,
    std::void_t<decltype(std::declval<PointT>().normal_x),
                decltype(std::declval<PointT>().normal_y),
                decltype(std::declval<PointT>().normal_z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<pcl::rvv::RVVFieldScalar<decltype(std::declval<PointT>().normal_x)>, float> &&
      std::is_same_v<pcl::rvv::RVVFieldScalar<decltype(std::declval<PointT>().normal_y)>, float> &&
      std::is_same_v<pcl::rvv::RVVFieldScalar<decltype(std::declval<PointT>().normal_z)>, float>> {};

template <typename PointT>
inline constexpr bool kFilterNormalCompatible = FilterNormalCompatible<PointT>::value;

inline vbool16_t
finiteMask3F32M2(vfloat32m2_t v0, vfloat32m2_t v1, vfloat32m2_t v2, std::size_t vl)
{
  vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(v0, v0, vl);
  finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(v1, v1, vl), vl);
  finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(v2, v2, vl), vl);
  finite = __riscv_vmand_mm_b16(finite, __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(v0, vl), std::numeric_limits<float>::infinity(), vl), vl);
  finite = __riscv_vmand_mm_b16(finite, __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(v1, vl), std::numeric_limits<float>::infinity(), vl), vl);
  finite = __riscv_vmand_mm_b16(finite, __riscv_vmflt_vf_f32m2_b16(__riscv_vfabs_v_f32m2(v2, vl), std::numeric_limits<float>::infinity(), vl), vl);
  return finite;
}

template <typename PointT, std::size_t kF0Off, std::size_t kF1Off, std::size_t kF2Off>
inline bool
removeNaNFromPointCloudMask(const pcl::PointCloud<PointT> &cloud_in,
                            pcl::PointCloud<PointT> &cloud_out,
                            Indices &index,
                            bool check_xyz_for_dense_flag)
{
  const std::size_t n = cloud_in.size ();
  if (n < kRemoveNaNCloudMinPoints || n > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return false;

  if (&cloud_in != &cloud_out)
  {
    cloud_out.header = cloud_in.header;
    cloud_out.resize (n);
    cloud_out.sensor_origin_ = cloud_in.sensor_origin_;
    cloud_out.sensor_orientation_ = cloud_in.sensor_orientation_;
  }
  index.resize (n);
  cloud_out.is_dense = true;

  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud_in.data());
  static_assert(sizeof(PointT) % alignof(float) == 0, "Point stride must be aligned for RVV float loads.");
  std::size_t kept = 0;
  std::size_t i = 0;

  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* chunk = base + i * sizeof(PointT);
    const auto stride = static_cast<ptrdiff_t>(sizeof(PointT));

    const vfloat32m2_t v0 = __riscv_vlse32_v_f32m2(reinterpret_cast<const float*>(chunk + kF0Off), stride, vl);
    const vfloat32m2_t v1 = __riscv_vlse32_v_f32m2(reinterpret_cast<const float*>(chunk + kF1Off), stride, vl);
    const vfloat32m2_t v2 = __riscv_vlse32_v_f32m2(reinterpret_cast<const float*>(chunk + kF2Off), stride, vl);
    vbool16_t finite = pcl::finiteMask3F32M2(v0, v1, v2, vl);

    const vuint32m2_t local = __riscv_vid_v_u32m2(vl);
    const vuint32m2_t source = __riscv_vadd_vx_u32m2(local, static_cast<std::uint32_t>(i), vl);
    const vint32m2_t source_i32 = __riscv_vreinterpret_v_u32m2_i32m2(source);
    const vint32m2_t compact = __riscv_vcompress_vm_i32m2(source_i32, finite, vl);
    const std::size_t count = __riscv_vcpop_m_b16(finite, vl);
    __riscv_vse32_v_i32m2(index.data() + kept, compact, count);

    // PointT copy remains scalar because arbitrary point structs can carry non-float
    // fields; RVV is used for finite-mask and ordered index compaction only.
    const std::size_t chunk_out = kept;
    for (std::size_t k = 0; k < count; ++k)
    {
      const int src = index[chunk_out + k];
      if (check_xyz_for_dense_flag && cloud_out.is_dense && !pcl::isFinite(cloud_in[src]))
        cloud_out.is_dense = false;
      cloud_out[kept] = cloud_in[src];
      index[kept] = src;
      ++kept;
    }

    i += vl;
  }

  if (kept != n)
  {
    cloud_out.resize (kept);
    index.resize (kept);
  }
  cloud_out.height = 1;
  cloud_out.width = static_cast<std::uint32_t> (kept);
  return true;
}

template <typename PointT>
inline bool
removeNaNFromPointCloudRVV(const pcl::PointCloud<PointT> &cloud_in,
                           pcl::PointCloud<PointT> &cloud_out,
                           Indices &index)
{
  if (cloud_in.is_dense)
    return false;
  return pcl::removeNaNFromPointCloudMask<PointT, offsetof(PointT, x), offsetof(PointT, y), offsetof(PointT, z)> (
      cloud_in, cloud_out, index, false);
}

template <typename PointT>
inline bool
removeNaNNormalsFromPointCloudRVV(const pcl::PointCloud<PointT> &cloud_in,
                                  pcl::PointCloud<PointT> &cloud_out,
                                  Indices &index)
{
  return pcl::removeNaNFromPointCloudMask<PointT,
                                          offsetof(PointT, normal_x),
                                          offsetof(PointT, normal_y),
                                          offsetof(PointT, normal_z)> (
      cloud_in, cloud_out, index, true);
}

#endif

} // namespace pcl

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::removeNaNFromPointCloud (const pcl::PointCloud<PointT> &cloud_in,
                              pcl::PointCloud<PointT> &cloud_out,
                              Indices &index)
{
#if defined(__RVV10__)
  if constexpr (pcl::kFilterXYZCompatible<PointT>)
    if (pcl::removeNaNFromPointCloudRVV (cloud_in, cloud_out, index))
      return;
#endif
  pcl::removeNaNFromPointCloudStd (cloud_in, cloud_out, index);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::removeNaNNormalsFromPointCloud (const pcl::PointCloud<PointT> &cloud_in,
                                     pcl::PointCloud<PointT> &cloud_out,
                                     Indices &index)
{
  // The RVV mask prototype is kept above as the evaluated candidate, but the
  // public normal path stays scalar: board data showed scalar point copies and
  // the extra xyz dense check dominate the vectorized normal finite mask.
  pcl::removeNaNNormalsFromPointCloudStd (cloud_in, cloud_out, index);
}


#define PCL_INSTANTIATE_removeNaNFromPointCloud(T) template PCL_EXPORTS void pcl::removeNaNFromPointCloud<T>(const pcl::PointCloud<T>&, pcl::PointCloud<T>&, Indices&);
#define PCL_INSTANTIATE_removeNaNNormalsFromPointCloud(T) template PCL_EXPORTS void pcl::removeNaNNormalsFromPointCloud<T>(const pcl::PointCloud<T>&, pcl::PointCloud<T>&, Indices&);
