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

#ifndef PCL_FILTERS_IMPL_VOXEL_GRID_H_
#define PCL_FILTERS_IMPL_VOXEL_GRID_H_

#include <limits>
#include <cstdint>
#include <cstring>

#if defined(__RVV10__)
#include <cstdint>
#include <type_traits>
#include <riscv_vector.h>
#endif
#include <pcl/common/centroid.h>
#include <pcl/common/common.h>
#include <pcl/common/io.h>
#include <pcl/filters/voxel_grid.h>
#include  <boost/sort/spreadsort/integer_sort.hpp>

namespace pcl
{

template <typename T>
inline void
getMinMax3DStd(const pcl::PCLPointCloud2ConstPtr& cloud,
               int x_idx,
               int y_idx,
               int z_idx,
               Eigen::Matrix<T, 4, 1>& min_pt,
               Eigen::Matrix<T, 4, 1>& max_pt)
{
  T min_x = std::numeric_limits<T>::max();
  T min_y = std::numeric_limits<T>::max();
  T min_z = std::numeric_limits<T>::max();
  T max_x = std::numeric_limits<T>::lowest();
  T max_y = std::numeric_limits<T>::lowest();
  T max_z = std::numeric_limits<T>::lowest();

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  const std::size_t nr_points = cloud->width * cloud->height;

  const std::uint8_t* data_ptr = cloud->data.data();

  T x, y, z;

  auto update_min_max = [&](const T& x, const T& y, const T& z) {
    if (x < min_x)
      min_x = x;
    if (y < min_y)
      min_y = y;
    if (z < min_z)
      min_z = z;
    if (x > max_x)
      max_x = x;
    if (y > max_y)
      max_y = y;
    if (z > max_z)
      max_z = z;
  };

  // If dense, no need to check for NaNs
  if (cloud->is_dense)
  {
    for (std::size_t i = 0; i < nr_points; ++i)
    {
      std::memcpy(&x, data_ptr + x_off, sizeof(T));
      std::memcpy(&y, data_ptr + y_off, sizeof(T));
      std::memcpy(&z, data_ptr + z_off, sizeof(T));

      data_ptr += pt_step;

      update_min_max(x, y, z);
    }
  }
  else
  {
    for (std::size_t i = 0; i < nr_points; ++i)
    {
      std::memcpy(&x, data_ptr + x_off, sizeof(T));
      std::memcpy(&y, data_ptr + y_off, sizeof(T));
      std::memcpy(&z, data_ptr + z_off, sizeof(T));

      data_ptr += pt_step;

      // Check if the point is invalid
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        continue;

      update_min_max(x, y, z);
    }
  }

  min_pt << min_x, min_y, min_z, 0;
  max_pt << max_x, max_y, max_z, 0;
}

#if defined(__RVV10__)

inline constexpr std::size_t kVoxelGridMinMaxRvvMinPoints = 64;

inline bool
getMinMax3DFloatDenseRVV(const pcl::PCLPointCloud2ConstPtr& cloud,
                         int x_idx,
                         int y_idx,
                         int z_idx,
                         Eigen::Vector4f& min_pt,
                         Eigen::Vector4f& max_pt)
{
  if (!cloud || !cloud->is_dense)
    return false;

  if (cloud->fields[x_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[y_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[z_idx].datatype != pcl::PCLPointField::FLOAT32)
    return false;

  const std::size_t nr_points = cloud->width * cloud->height;
  if (nr_points < kVoxelGridMinMaxRvvMinPoints)
    return false;

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  if (pt_step == 0 || x_off + sizeof(float) > pt_step || y_off + sizeof(float) > pt_step ||
      z_off + sizeof(float) > pt_step)
    return false;

  const auto* base = cloud->data.data();
  // RVV only uses byte-strided access described by PCLPointCloud2 metadata; paths
  // that need NaN filtering, non-float fields, or invalid offsets stay on Std.
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float min_z = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();
  float max_z = std::numeric_limits<float>::lowest();

  std::size_t i = 0;
  while (i < nr_points)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    const auto* chunk = base + i * pt_step;
    const auto* x_ptr = reinterpret_cast<const float*>(chunk + x_off);
    const auto* y_ptr = reinterpret_cast<const float*>(chunk + y_off);
    const auto* z_ptr = reinterpret_cast<const float*>(chunk + z_off);
    const auto stride = static_cast<ptrdiff_t>(pt_step);

    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(x_ptr, stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(y_ptr, stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(z_ptr, stride, vl);

    min_x = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vx, __riscv_vfmv_s_f_f32m1(min_x, 1), vl));
    min_y = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vy, __riscv_vfmv_s_f_f32m1(min_y, 1), vl));
    min_z = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vz, __riscv_vfmv_s_f_f32m1(min_z, 1), vl));
    max_x = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vx, __riscv_vfmv_s_f_f32m1(max_x, 1), vl));
    max_y = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vy, __riscv_vfmv_s_f_f32m1(max_y, 1), vl));
    max_z = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vz, __riscv_vfmv_s_f_f32m1(max_z, 1), vl));

    i += vl;
  }

  min_pt << min_x, min_y, min_z, 0.0f;
  max_pt << max_x, max_y, max_z, 0.0f;
  return true;
}

#endif

} // namespace pcl

///////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void
pcl::getMinMax3D (const pcl::PCLPointCloud2ConstPtr &cloud, int x_idx, int y_idx, int z_idx, Eigen::Matrix<T, 4, 1> &min_pt, Eigen::Matrix<T, 4, 1> &max_pt)
{
  if (pcl::traits::asEnum_v<T> != cloud->fields[x_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[y_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[z_idx].datatype)
  {
      PCL_ERROR("[pcl::getMinMax3D] Type of max_pt/min_pt does not match cloud type!\n");
      return;
  }

#if defined(__RVV10__)
  if constexpr (std::is_same_v<T, float>)
  {
    Eigen::Vector4f min_f;
    Eigen::Vector4f max_f;
    if (pcl::getMinMax3DFloatDenseRVV(cloud, x_idx, y_idx, z_idx, min_f, max_f))
    {
      min_pt = min_f.template cast<T>();
      max_pt = max_f.template cast<T>();
      return;
    }
  }
#endif

  pcl::getMinMax3DStd(cloud, x_idx, y_idx, z_idx, min_pt, max_pt);
}

namespace pcl
{

template <typename T>
inline void
getMinMax3DIndicesStd(const pcl::PCLPointCloud2ConstPtr& cloud,
                       const pcl::Indices& indices,
                       int x_idx,
                       int y_idx,
                       int z_idx,
                       Eigen::Matrix<T, 4, 1>& min_pt,
                       Eigen::Matrix<T, 4, 1>& max_pt)
{
  T min_x = std::numeric_limits<T>::max();
  T min_y = std::numeric_limits<T>::max();
  T min_z = std::numeric_limits<T>::max();
  T max_x = std::numeric_limits<T>::lowest();
  T max_y = std::numeric_limits<T>::lowest();
  T max_z = std::numeric_limits<T>::lowest();

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  const std::uint8_t* data_ptr = cloud->data.data();

  T x, y, z;

  auto update_min_max = [&](const T& x, const T& y, const T& z) {
    if (x < min_x)
      min_x = x;
    if (y < min_y)
      min_y = y;
    if (z < min_z)
      min_z = z;
    if (x > max_x)
      max_x = x;
    if (y > max_y)
      max_y = y;
    if (z > max_z)
      max_z = z;
  };

  // If dense, no need to check for NaNs
  if (cloud->is_dense)
  {
    for (const auto& index : indices)
    {
      const std::uint8_t* pt_data = data_ptr + (index * pt_step);

      std::memcpy(&x, pt_data + x_off, sizeof(T));
      std::memcpy(&y, pt_data + y_off, sizeof(T));
      std::memcpy(&z, pt_data + z_off, sizeof(T));

      update_min_max(x, y, z);
    }
  }
  else
  {
    for (const auto& index : indices)
    {
      const std::uint8_t* pt_data = data_ptr + (index * pt_step);

      std::memcpy(&x, pt_data + x_off, sizeof(T));
      std::memcpy(&y, pt_data + y_off, sizeof(T));
      std::memcpy(&z, pt_data + z_off, sizeof(T));

      // Check if the point is invalid
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        continue;

      update_min_max(x, y, z);
    }
  }

  min_pt << min_x, min_y, min_z, 0;
  max_pt << max_x, max_y, max_z, 0;
}

#if defined(__RVV10__)

inline bool
getMinMax3DFloatDenseIndicesRVV(const pcl::PCLPointCloud2ConstPtr& cloud,
                                const pcl::Indices& indices,
                                int x_idx,
                                int y_idx,
                                int z_idx,
                                Eigen::Vector4f& min_pt,
                                Eigen::Vector4f& max_pt)
{
  if (!cloud || !cloud->is_dense)
    return false;

  if (cloud->fields[x_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[y_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[z_idx].datatype != pcl::PCLPointField::FLOAT32)
    return false;

  if (indices.size() < kVoxelGridMinMaxRvvMinPoints)
    return false;

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  if (pt_step == 0 || x_off + sizeof(float) > pt_step || y_off + sizeof(float) > pt_step ||
      z_off + sizeof(float) > pt_step)
    return false;

  const auto* base = reinterpret_cast<const float*>(cloud->data.data());
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float min_z = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();
  float max_z = std::numeric_limits<float>::lowest();

  const auto* indices_ptr = indices.data();
  std::size_t i = 0;
  while (i < indices.size())
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const vint32m2_t vidx = __riscv_vle32_v_i32m2(indices_ptr + i, vl);
    // Gather offsets are byte offsets from cloud->data.data(); keeping this in
    // the RVV helper preserves the public entry's Std fallback for all other layouts.
    const vuint32m2_t base_offsets = __riscv_vreinterpret_v_i32m2_u32m2(
        __riscv_vmul_vx_i32m2(vidx, static_cast<std::int32_t>(pt_step), vl));
    const vuint32m2_t x_offsets = __riscv_vadd_vx_u32m2(base_offsets, x_off, vl);
    const vuint32m2_t y_offsets = __riscv_vadd_vx_u32m2(base_offsets, y_off, vl);
    const vuint32m2_t z_offsets = __riscv_vadd_vx_u32m2(base_offsets, z_off, vl);

    const vfloat32m2_t vx = __riscv_vluxei32_v_f32m2(base, x_offsets, vl);
    const vfloat32m2_t vy = __riscv_vluxei32_v_f32m2(base, y_offsets, vl);
    const vfloat32m2_t vz = __riscv_vluxei32_v_f32m2(base, z_offsets, vl);

    min_x = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vx, __riscv_vfmv_s_f_f32m1(min_x, 1), vl));
    min_y = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vy, __riscv_vfmv_s_f_f32m1(min_y, 1), vl));
    min_z = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmin_vs_f32m2_f32m1(vz, __riscv_vfmv_s_f_f32m1(min_z, 1), vl));
    max_x = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vx, __riscv_vfmv_s_f_f32m1(max_x, 1), vl));
    max_y = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vy, __riscv_vfmv_s_f_f32m1(max_y, 1), vl));
    max_z = __riscv_vfmv_f_s_f32m1_f32(
        __riscv_vfredmax_vs_f32m2_f32m1(vz, __riscv_vfmv_s_f_f32m1(max_z, 1), vl));

    i += vl;
  }

  min_pt << min_x, min_y, min_z, 0.0f;
  max_pt << max_x, max_y, max_z, 0.0f;
  return true;
}

#endif

} // namespace pcl

///////////////////////////////////////////////////////////////////////////////////////////
template <typename T> void
pcl::getMinMax3D (const pcl::PCLPointCloud2ConstPtr &cloud, const pcl::Indices &indices, int x_idx, int y_idx, int z_idx,
                 Eigen::Matrix<T, 4, 1> &min_pt, Eigen::Matrix<T, 4, 1> &max_pt)
{
  if (pcl::traits::asEnum_v<T> != cloud->fields[x_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[y_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[z_idx].datatype)
  {
    PCL_ERROR("[pcl::getMinMax3D] Type of max_pt/min_pt does not match cloud type!\n");
    return;
  }

#if defined(__RVV10__)
  if constexpr (std::is_same_v<T, float>)
  {
    Eigen::Vector4f min_f;
    Eigen::Vector4f max_f;
    if (pcl::getMinMax3DFloatDenseIndicesRVV(cloud, indices, x_idx, y_idx, z_idx, min_f, max_f))
    {
      min_pt = min_f.template cast<T>();
      max_pt = max_f.template cast<T>();
      return;
    }
  }
#endif

  pcl::getMinMax3DIndicesStd(cloud, indices, x_idx, y_idx, z_idx, min_pt, max_pt);

}

namespace pcl
{

template <typename T, typename D>
inline void
getMinMax3DDistanceStd(const pcl::PCLPointCloud2ConstPtr& cloud,
                       int x_idx,
                       int y_idx,
                       int z_idx,
                       int distance_idx,
                       D min_distance,
                       D max_distance,
                       Eigen::Matrix<T, 4, 1>& min_pt,
                       Eigen::Matrix<T, 4, 1>& max_pt,
                       bool limit_negative)
{
  T min_x = std::numeric_limits<T>::max();
  T min_y = std::numeric_limits<T>::max();
  T min_z = std::numeric_limits<T>::max();
  T max_x = std::numeric_limits<T>::lowest();
  T max_y = std::numeric_limits<T>::lowest();
  T max_z = std::numeric_limits<T>::lowest();

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  const std::uint8_t* data_ptr = cloud->data.data();
  const std::size_t nr_points = cloud->width * cloud->height;

  const std::uint32_t distance_off = cloud->fields[distance_idx].offset;

  T x, y, z;
  D distance_value;

  auto update_min_max = [&](const T& x, const T& y, const T& z) {
    if (x < min_x)
      min_x = x;
    if (y < min_y)
      min_y = y;
    if (z < min_z)
      min_z = z;
    if (x > max_x)
      max_x = x;
    if (y > max_y)
      max_y = y;
    if (z > max_z)
      max_z = z;
  };

  // If dense, no need to check for NaNs
  if (cloud->is_dense)
  {
    for (std::size_t cp = 0; cp < nr_points; ++cp)
    {
      std::memcpy(&distance_value, data_ptr + distance_off, sizeof(D));

      if (limit_negative == (distance_value < max_distance && distance_value > min_distance))
      {
        data_ptr += pt_step;
        continue;
      }

      std::memcpy(&x, data_ptr + x_off, sizeof(T));
      std::memcpy(&y, data_ptr + y_off, sizeof(T));
      std::memcpy(&z, data_ptr + z_off, sizeof(T));

      data_ptr += pt_step;

      update_min_max(x, y, z);
    }
  }
  else
  {
    for (std::size_t cp = 0; cp < nr_points; ++cp)
    {
      std::memcpy(&distance_value, data_ptr + distance_off, sizeof(D));

      if (limit_negative == (distance_value < max_distance && distance_value > min_distance))
      {
        data_ptr += pt_step;
        continue;
      }

      std::memcpy(&x, data_ptr + x_off, sizeof(T));
      std::memcpy(&y, data_ptr + y_off, sizeof(T));
      std::memcpy(&z, data_ptr + z_off, sizeof(T));

      data_ptr += pt_step;

      // Check if the point is invalid
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        continue;

      update_min_max(x, y, z);
    }
  }

  min_pt << min_x, min_y, min_z, 0;
  max_pt << max_x, max_y, max_z, 0;
}

#if defined(__RVV10__)

inline bool
getMinMax3DFloatDenseDistanceRVV(const pcl::PCLPointCloud2ConstPtr& cloud,
                                 int x_idx,
                                 int y_idx,
                                 int z_idx,
                                 int distance_idx,
                                 float min_distance,
                                 float max_distance,
                                 Eigen::Vector4f& min_pt,
                                 Eigen::Vector4f& max_pt,
                                 bool limit_negative)
{
  if (!cloud || !cloud->is_dense)
    return false;

  if (cloud->fields[x_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[y_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[z_idx].datatype != pcl::PCLPointField::FLOAT32 ||
      cloud->fields[distance_idx].datatype != pcl::PCLPointField::FLOAT32)
    return false;

  const std::size_t nr_points = cloud->width * cloud->height;
  if (nr_points < kVoxelGridMinMaxRvvMinPoints)
    return false;

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t distance_off = cloud->fields[distance_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  if (pt_step == 0 || x_off + sizeof(float) > pt_step || y_off + sizeof(float) > pt_step ||
      z_off + sizeof(float) > pt_step || distance_off + sizeof(float) > pt_step)
    return false;

  const auto* base = cloud->data.data();
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float min_z = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();
  float max_z = std::numeric_limits<float>::lowest();

  std::size_t i = 0;
  while (i < nr_points)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    const auto* chunk = base + i * pt_step;
    const auto* x_ptr = reinterpret_cast<const float*>(chunk + x_off);
    const auto* y_ptr = reinterpret_cast<const float*>(chunk + y_off);
    const auto* z_ptr = reinterpret_cast<const float*>(chunk + z_off);
    const auto* distance_ptr = reinterpret_cast<const float*>(chunk + distance_off);
    const auto stride = static_cast<ptrdiff_t>(pt_step);

    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(x_ptr, stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(y_ptr, stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(z_ptr, stride, vl);
    const vfloat32m2_t vd = __riscv_vlse32_v_f32m2(distance_ptr, stride, vl);

    // Match the scalar condition exactly: limit_negative excludes points inside
    // (min_distance, max_distance), otherwise only inside points participate.
    const vbool16_t gt_min = __riscv_vmfgt_vf_f32m2_b16(vd, min_distance, vl);
    const vbool16_t lt_max = __riscv_vmflt_vf_f32m2_b16(vd, max_distance, vl);
    const vbool16_t inside = __riscv_vmand_mm_b16(gt_min, lt_max, vl);
    const vbool16_t include = limit_negative ? __riscv_vmnot_m_b16(inside, vl) : inside;

    min_x = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1_m(
        include, vx, __riscv_vfmv_s_f_f32m1(min_x, 1), vl));
    min_y = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1_m(
        include, vy, __riscv_vfmv_s_f_f32m1(min_y, 1), vl));
    min_z = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1_m(
        include, vz, __riscv_vfmv_s_f_f32m1(min_z, 1), vl));
    max_x = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1_m(
        include, vx, __riscv_vfmv_s_f_f32m1(max_x, 1), vl));
    max_y = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1_m(
        include, vy, __riscv_vfmv_s_f_f32m1(max_y, 1), vl));
    max_z = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1_m(
        include, vz, __riscv_vfmv_s_f_f32m1(max_z, 1), vl));

    i += vl;
  }

  min_pt << min_x, min_y, min_z, 0.0f;
  max_pt << max_x, max_y, max_z, 0.0f;
  return true;
}

#endif

} // namespace pcl

///////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename D> void
pcl::getMinMax3D (const pcl::PCLPointCloud2ConstPtr &cloud, int x_idx, int y_idx, int z_idx,
                 const std::string &distance_field_name, D min_distance, D max_distance,
                 Eigen::Matrix<T, 4, 1> &min_pt, Eigen::Matrix<T, 4, 1> &max_pt, bool limit_negative)
{
  if (pcl::traits::asEnum_v<T> != cloud->fields[x_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[y_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[z_idx].datatype)
  {
    PCL_ERROR("[pcl::getMinMax3D] Type of max_pt/min_pt does not match cloud type!\n");
    return;
  }

  int distance_idx = pcl::getFieldIndex (*cloud, distance_field_name);

  if (distance_idx < 0)
  {
    PCL_ERROR("[pcl::getMinMax3D] The specified distance field name is not found in the cloud!\n");
    return;
  }

  if (cloud->fields[distance_idx].datatype != pcl::traits::asEnum_v<D>)
  {
    PCL_ERROR ("[pcl::getMinMax3D] min_distance/max_distance are incorrect type!\n");
    return;
  }

#if defined(__RVV10__)
  if constexpr (std::is_same_v<T, float> && std::is_same_v<D, float>)
  {
    Eigen::Vector4f min_f;
    Eigen::Vector4f max_f;
    if (pcl::getMinMax3DFloatDenseDistanceRVV(cloud,
                                              x_idx,
                                              y_idx,
                                              z_idx,
                                              distance_idx,
                                              min_distance,
                                              max_distance,
                                              min_f,
                                              max_f,
                                              limit_negative))
    {
      min_pt = min_f.template cast<T>();
      max_pt = max_f.template cast<T>();
      return;
    }
  }
#endif

  pcl::getMinMax3DDistanceStd(cloud, x_idx, y_idx, z_idx, distance_idx, min_distance, max_distance, min_pt, max_pt, limit_negative);

}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename D> void
pcl::getMinMax3D (const pcl::PCLPointCloud2ConstPtr &cloud, const pcl::Indices &indices, int x_idx, int y_idx, int z_idx, const std::string &distance_field_name, D min_distance, D max_distance,
                Eigen::Matrix<T, 4, 1> &min_pt, Eigen::Matrix<T, 4, 1> &max_pt, bool limit_negative)
{
  if (pcl::traits::asEnum_v<T> != cloud->fields[x_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[y_idx].datatype ||
      pcl::traits::asEnum_v<T> != cloud->fields[z_idx].datatype)
  {
    PCL_ERROR("[pcl::getMinMax3D] Type of max_pt/min_pt does not match cloud type!\n");
    return;
  }

  int distance_idx = pcl::getFieldIndex (*cloud, distance_field_name);

  if (distance_idx < 0)
  {
    PCL_ERROR("[pcl::getMinMax3D] The specified distance field name is not found in the cloud!\n");
    return;
  }

  if (cloud->fields[distance_idx].datatype != pcl::traits::asEnum_v<D>)
  {
    PCL_ERROR ("[pcl::getMinMax3D] min_distance/max_distance are incorrect type!\n");
    return;
  }

  T min_x = std::numeric_limits<T>::max();
  T min_y = std::numeric_limits<T>::max();
  T min_z = std::numeric_limits<T>::max();
  T max_x = std::numeric_limits<T>::lowest();
  T max_y = std::numeric_limits<T>::lowest();
  T max_z = std::numeric_limits<T>::lowest();

  const std::uint32_t x_off = cloud->fields[x_idx].offset;
  const std::uint32_t y_off = cloud->fields[y_idx].offset;
  const std::uint32_t z_off = cloud->fields[z_idx].offset;
  const std::uint32_t pt_step = cloud->point_step;
  const std::uint8_t* data_ptr = cloud->data.data();

  const std::uint32_t distance_off = cloud->fields[distance_idx].offset;

  T x, y, z;
  D distance_value;

  auto update_min_max = [&](const T& x, const T& y, const T& z) {
    if (x < min_x)
      min_x = x;
    if (y < min_y)
      min_y = y;
    if (z < min_z)
      min_z = z;
    if (x > max_x)
      max_x = x;
    if (y > max_y)
      max_y = y;
    if (z > max_z)
      max_z = z;
  };

  // If dense, no need to check for NaNs
  if(cloud->is_dense)
  {
    for (const auto& index : indices)
    {
      const std::uint8_t* pt_data = data_ptr + (index * pt_step);

      std::memcpy(&distance_value, pt_data + distance_off, sizeof(D));

      if (limit_negative == (distance_value < max_distance && distance_value > min_distance))
      {
        continue;
      }

      std::memcpy(&x, pt_data + x_off, sizeof(T));
      std::memcpy(&y, pt_data + y_off, sizeof(T));
      std::memcpy(&z, pt_data + z_off, sizeof(T));

      update_min_max(x, y, z);
    }
  }
  else
  {
    for (const auto& index : indices)
    {
      const std::uint8_t* pt_data = data_ptr + (index * pt_step);

      std::memcpy(&distance_value, pt_data + distance_off, sizeof(D));

      if (limit_negative == (distance_value < max_distance && distance_value > min_distance))
      {
        continue;
      }

      std::memcpy(&x, pt_data + x_off, sizeof(T));
      std::memcpy(&y, pt_data + y_off, sizeof(T));
      std::memcpy(&z, pt_data + z_off, sizeof(T));

      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        continue;

      update_min_max(x, y, z);
    }
  }
  min_pt << min_x, min_y, min_z, 0;
  max_pt << max_x, max_y, max_z, 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::getMinMax3D (const typename pcl::PointCloud<PointT>::ConstPtr &cloud,
                  const std::string &distance_field_name, float min_distance, float max_distance,
                  Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt, bool limit_negative)
{
  Eigen::Array4f min_p, max_p;
  min_p.setConstant (std::numeric_limits<float>::max());
  max_p.setConstant (std::numeric_limits<float>::lowest());

  // Get the fields list and the distance field index
  std::vector<pcl::PCLPointField> fields;
  int distance_idx = pcl::getFieldIndex<PointT> (distance_field_name, fields);
  if (distance_idx < 0 || fields.empty()) {
    PCL_ERROR ("[pcl::getMinMax3D] Could not find field with name '%s'!\n", distance_field_name.c_str());
    return;
  }
  const auto field_offset = fields[distance_idx].offset;

  float distance_value;
  // If dense, no need to check for NaNs
  if (cloud->is_dense)
  {
    for (const auto& point: *cloud)
    {
      // Get the distance value
      const auto* pt_data = reinterpret_cast<const std::uint8_t*> (&point);
      memcpy (&distance_value, pt_data + field_offset, sizeof (float));

      if (limit_negative)
      {
        // Use a threshold for cutting out points which inside the interval
        if ((distance_value < max_distance) && (distance_value > min_distance))
          continue;
      }
      else
      {
        // Use a threshold for cutting out points which are too close/far away
        if ((distance_value > max_distance) || (distance_value < min_distance))
          continue;
      }
      // Create the point structure and get the min/max
      pcl::Array4fMapConst pt = point.getArray4fMap ();
      min_p = min_p.min (pt);
      max_p = max_p.max (pt);
    }
  }
  else
  {
    for (const auto& point: *cloud)
    {
      // Get the distance value
      const auto* pt_data = reinterpret_cast<const std::uint8_t*> (&point);
      memcpy (&distance_value, pt_data + field_offset, sizeof (float));

      if (limit_negative)
      {
        // Use a threshold for cutting out points which inside the interval
        if ((distance_value < max_distance) && (distance_value > min_distance))
          continue;
      }
      else
      {
        // Use a threshold for cutting out points which are too close/far away
        if ((distance_value > max_distance) || (distance_value < min_distance))
          continue;
      }

      // Check if the point is invalid
      if (!isXYZFinite (point))
        continue;
      // Create the point structure and get the min/max
      pcl::Array4fMapConst pt = point.getArray4fMap ();
      min_p = min_p.min (pt);
      max_p = max_p.max (pt);
    }
  }
  min_pt = min_p;
  max_pt = max_p;
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::getMinMax3D (const typename pcl::PointCloud<PointT>::ConstPtr &cloud,
                  const Indices &indices,
                  const std::string &distance_field_name, float min_distance, float max_distance,
                  Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt, bool limit_negative)
{
  Eigen::Array4f min_p, max_p;
  min_p.setConstant (std::numeric_limits<float>::max());
  max_p.setConstant (std::numeric_limits<float>::lowest());

  // Get the fields list and the distance field index
  std::vector<pcl::PCLPointField> fields;
  int distance_idx = pcl::getFieldIndex<PointT> (distance_field_name, fields);
  if (distance_idx < 0 || fields.empty()) {
    PCL_ERROR ("[pcl::getMinMax3D] Could not find field with name '%s'!\n", distance_field_name.c_str());
    return;
  }
  const auto field_offset = fields[distance_idx].offset;

  float distance_value;
  // If dense, no need to check for NaNs
  if (cloud->is_dense)
  {
    for (const auto &index : indices)
    {
      // Get the distance value
      const auto* pt_data = reinterpret_cast<const std::uint8_t*> (&(*cloud)[index]);
      memcpy (&distance_value, pt_data + field_offset, sizeof (float));

      if (limit_negative)
      {
        // Use a threshold for cutting out points which inside the interval
        if ((distance_value < max_distance) && (distance_value > min_distance))
          continue;
      }
      else
      {
        // Use a threshold for cutting out points which are too close/far away
        if ((distance_value > max_distance) || (distance_value < min_distance))
          continue;
      }
      // Create the point structure and get the min/max
      pcl::Array4fMapConst pt = (*cloud)[index].getArray4fMap ();
      min_p = min_p.min (pt);
      max_p = max_p.max (pt);
    }
  }
  else
  {
    for (const auto &index : indices)
    {
      // Get the distance value
      const auto* pt_data = reinterpret_cast<const std::uint8_t*> (&(*cloud)[index]);
      memcpy (&distance_value, pt_data + field_offset, sizeof (float));

      if (limit_negative)
      {
        // Use a threshold for cutting out points which inside the interval
        if ((distance_value < max_distance) && (distance_value > min_distance))
          continue;
      }
      else
      {
        // Use a threshold for cutting out points which are too close/far away
        if ((distance_value > max_distance) || (distance_value < min_distance))
          continue;
      }

      // Check if the point is invalid
      if (!std::isfinite ((*cloud)[index].x) ||
          !std::isfinite ((*cloud)[index].y) ||
          !std::isfinite ((*cloud)[index].z))
        continue;
      // Create the point structure and get the min/max
      pcl::Array4fMapConst pt = (*cloud)[index].getArray4fMap ();
      min_p = min_p.min (pt);
      max_p = max_p.max (pt);
    }
  }
  min_pt = min_p;
  max_pt = max_p;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::VoxelGrid<PointT>::applyFilter (PointCloud &output)
{
  // Has the input dataset been set already?
  if (!input_)
  {
    PCL_WARN ("[pcl::%s::applyFilter] No input dataset given!\n", getClassName ().c_str ());
    output.width = output.height = 0;
    output.clear ();
    return;
  }

  // Copy the header (and thus the frame_id) + allocate enough space for points
  output.height       = 1;                    // downsampling breaks the organized structure
  output.is_dense     = true;                 // we filter out invalid points

  Eigen::Vector4f min_p, max_p;
  // Get the minimum and maximum dimensions
  if (!filter_field_name_.empty ()) // If we don't want to process the entire cloud...
    getMinMax3D<PointT> (input_, *indices_, filter_field_name_, static_cast<float> (filter_limit_min_), static_cast<float> (filter_limit_max_), min_p, max_p, filter_limit_negative_);
  else
    getMinMax3D<PointT> (*input_, *indices_, min_p, max_p);

  // Check that the leaf size is not too small, given the size of the data
  std::int64_t dx = static_cast<std::int64_t>((max_p[0] - min_p[0]) * inverse_leaf_size_[0])+1;
  std::int64_t dy = static_cast<std::int64_t>((max_p[1] - min_p[1]) * inverse_leaf_size_[1])+1;
  std::int64_t dz = static_cast<std::int64_t>((max_p[2] - min_p[2]) * inverse_leaf_size_[2])+1;

  if ((dx*dy*dz) > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
  {
    PCL_WARN("[pcl::%s::applyFilter] Leaf size is too small for the input dataset. Integer indices would overflow.\n", getClassName().c_str());
    output = *input_;
    return;
  }

  // Compute the minimum and maximum bounding box values
  min_b_[0] = static_cast<int> (std::floor (min_p[0] * inverse_leaf_size_[0]));
  max_b_[0] = static_cast<int> (std::floor (max_p[0] * inverse_leaf_size_[0]));
  min_b_[1] = static_cast<int> (std::floor (min_p[1] * inverse_leaf_size_[1]));
  max_b_[1] = static_cast<int> (std::floor (max_p[1] * inverse_leaf_size_[1]));
  min_b_[2] = static_cast<int> (std::floor (min_p[2] * inverse_leaf_size_[2]));
  max_b_[2] = static_cast<int> (std::floor (max_p[2] * inverse_leaf_size_[2]));

  // Compute the number of divisions needed along all axis
  div_b_ = max_b_ - min_b_ + Eigen::Vector4i::Ones ();
  div_b_[3] = 0;

  // Set up the division multiplier
  divb_mul_ = Eigen::Vector4i (1, div_b_[0], div_b_[0] * div_b_[1], 0);

  // Storage for mapping leaf and pointcloud indexes
  std::vector<internal::cloud_point_index_idx> index_vector;
  index_vector.reserve (indices_->size ());

  // If we don't want to process the entire cloud, but rather filter points far away from the viewpoint first...
  if (!filter_field_name_.empty ())
  {
    // Get the distance field index
    std::vector<pcl::PCLPointField> fields;
    int distance_idx = pcl::getFieldIndex<PointT> (filter_field_name_, fields);
    if (distance_idx == -1) {
      PCL_ERROR ("[pcl::%s::applyFilter] Invalid filter field name (%s).\n", getClassName ().c_str (), filter_field_name_.c_str());
      return;
    }
    const auto field_offset = fields[distance_idx].offset;

    // First pass: go over all points and insert them into the index_vector vector
    // with calculated idx. Points with the same idx value will contribute to the
    // same point of resulting CloudPoint
    for (const auto& index : (*indices_))
    {
      if (!input_->is_dense)
        // Check if the point is invalid
        if (!isXYZFinite ((*input_)[index]))
          continue;

      // Get the distance value
      const auto* pt_data = reinterpret_cast<const std::uint8_t*> (&(*input_)[index]);
      float distance_value = 0;
      memcpy (&distance_value, pt_data + field_offset, sizeof (float));

      if (filter_limit_negative_)
      {
        // Use a threshold for cutting out points which inside the interval
        if ((distance_value < filter_limit_max_) && (distance_value > filter_limit_min_))
          continue;
      }
      else
      {
        // Use a threshold for cutting out points which are too close/far away
        if ((distance_value > filter_limit_max_) || (distance_value < filter_limit_min_))
          continue;
      }

      int ijk0 = static_cast<int> (std::floor ((*input_)[index].x * inverse_leaf_size_[0]) - static_cast<float> (min_b_[0]));
      int ijk1 = static_cast<int> (std::floor ((*input_)[index].y * inverse_leaf_size_[1]) - static_cast<float> (min_b_[1]));
      int ijk2 = static_cast<int> (std::floor ((*input_)[index].z * inverse_leaf_size_[2]) - static_cast<float> (min_b_[2]));

      // Compute the centroid leaf index
      int idx = ijk0 * divb_mul_[0] + ijk1 * divb_mul_[1] + ijk2 * divb_mul_[2];
      index_vector.emplace_back(static_cast<unsigned int> (idx), index);
    }
  }
  // No distance filtering, process all data
  else
  {
    // First pass: go over all points and insert them into the index_vector vector
    // with calculated idx. Points with the same idx value will contribute to the
    // same point of resulting CloudPoint
    for (const auto& index : (*indices_))
    {
      if (!input_->is_dense)
        // Check if the point is invalid
        if (!isXYZFinite ((*input_)[index]))
          continue;

      int ijk0 = static_cast<int> (std::floor ((*input_)[index].x * inverse_leaf_size_[0]) - static_cast<float> (min_b_[0]));
      int ijk1 = static_cast<int> (std::floor ((*input_)[index].y * inverse_leaf_size_[1]) - static_cast<float> (min_b_[1]));
      int ijk2 = static_cast<int> (std::floor ((*input_)[index].z * inverse_leaf_size_[2]) - static_cast<float> (min_b_[2]));

      // Compute the centroid leaf index
      int idx = ijk0 * divb_mul_[0] + ijk1 * divb_mul_[1] + ijk2 * divb_mul_[2];
      index_vector.emplace_back(static_cast<unsigned int> (idx), index);
    }
  }

  // Second pass: sort the index_vector vector using value representing target cell as index
  // in effect all points belonging to the same output cell will be next to each other
  auto rightshift_func = [](const internal::cloud_point_index_idx &x, const unsigned offset) { return x.idx >> offset; };
  boost::sort::spreadsort::integer_sort(index_vector.begin(), index_vector.end(), rightshift_func);

  // Third pass: count output cells
  // we need to skip all the same, adjacent idx values
  unsigned int total = 0;
  unsigned int index = 0;
  // first_and_last_indices_vector[i] represents the index in index_vector of the first point in
  // index_vector belonging to the voxel which corresponds to the i-th output point,
  // and of the first point not belonging to.
  std::vector<std::pair<unsigned int, unsigned int> > first_and_last_indices_vector;
  // Worst case size
  first_and_last_indices_vector.reserve (index_vector.size ());
  while (index < index_vector.size ())
  {
    unsigned int i = index + 1;
    while (i < index_vector.size () && index_vector[i].idx == index_vector[index].idx)
      ++i;
    if (i - index >= min_points_per_voxel_)
    {
      ++total;
      first_and_last_indices_vector.emplace_back(index, i);
    }
    index = i;
  }

  // Fourth pass: compute centroids, insert them into their final position
  output.resize (total);
  if (save_leaf_layout_)
  {
    try
    {
      // Resizing won't reset old elements to -1.  If leaf_layout_ has been used previously, it needs to be re-initialized to -1
      std::uint32_t new_layout_size = div_b_[0]*div_b_[1]*div_b_[2];
      //This is the number of elements that need to be re-initialized to -1
      std::uint32_t reinit_size = std::min (static_cast<unsigned int> (new_layout_size), static_cast<unsigned int> (leaf_layout_.size()));
      for (std::uint32_t i = 0; i < reinit_size; i++)
      {
        leaf_layout_[i] = -1;
      }
      leaf_layout_.resize (new_layout_size, -1);
    }
    catch (std::bad_alloc&)
    {
      throw PCLException("VoxelGrid bin size is too low; impossible to allocate memory for layout",
        "voxel_grid.hpp", "applyFilter");
    }
    catch (std::length_error&)
    {
      throw PCLException("VoxelGrid bin size is too low; impossible to allocate memory for layout",
        "voxel_grid.hpp", "applyFilter");
    }
  }

  index = 0;
  for (const auto &cp : first_and_last_indices_vector)
  {
    // calculate centroid - sum values from all input points, that have the same idx value in index_vector array
  	unsigned int first_index = cp.first;
  	unsigned int last_index = cp.second;

    // index is centroid final position in resulting PointCloud
    if (save_leaf_layout_)
      leaf_layout_[index_vector[first_index].idx] = index;

    //Limit downsampling to coords
    if (!downsample_all_data_)
    {
      Eigen::Vector4f centroid (Eigen::Vector4f::Zero ());

      for (unsigned int li = first_index; li < last_index; ++li)
        centroid += (*input_)[index_vector[li].cloud_point_index].getVector4fMap ();

      centroid /= static_cast<float> (last_index - first_index);
      output[index].getVector4fMap () = centroid;
    }
    else
    {
      CentroidPoint<PointT> centroid;

      // fill in the accumulator with leaf points
      for (unsigned int li = first_index; li < last_index; ++li)
        centroid.add ((*input_)[index_vector[li].cloud_point_index]);

      centroid.get (output[index]);
    }

    ++index;
  }
  output.width = output.size ();
}

#define PCL_INSTANTIATE_VoxelGrid(T) template class PCL_EXPORTS pcl::VoxelGrid<T>;
#define PCL_INSTANTIATE_getMinMax3D(T) template PCL_EXPORTS void pcl::getMinMax3D<T> (const pcl::PointCloud<T>::ConstPtr &, const std::string &, float, float, Eigen::Vector4f &, Eigen::Vector4f &, bool);

#endif    // PCL_FILTERS_IMPL_VOXEL_GRID_H_

