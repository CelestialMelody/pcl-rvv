/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2026
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
 */

#pragma once

#include <pcl/common/point_tests.h>
#include <pcl/point_cloud.h>
#include <pcl/rvv_point_traits.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl
{
namespace io
{
namespace organized_compression_detail
{

template<typename PointT> inline void
analyzeOrganizedCloudStd(const pcl::PointCloud<PointT>& cloud_arg,
                         float& maxDepth_arg,
                         float& focalLength_arg)
{
  const std::size_t width = cloud_arg.width;
  const std::size_t height = cloud_arg.height;

  const int centerX = static_cast<int> (width / 2);
  const int centerY = static_cast<int> (height / 2);

  assert ((width>1) && (height>1));
  assert (width*height == cloud_arg.size ());

  float maxDepth = 0;
  float focalLength = 0;

  std::size_t it = 0;
  for (int y = -centerY; y < centerY; ++y )
    for (int x = -centerX; x < centerX; ++x )
    {
      const PointT& point = cloud_arg[it++];

      if (pcl::isFinite (point))
      {
        if (maxDepth < point.z)
        {
          maxDepth = point.z;
          focalLength = 2.0f / (point.x / (static_cast<float> (x) * point.z) + point.y / (static_cast<float> (y) * point.z));
        }
      }
    }

  maxDepth_arg = maxDepth;
  focalLength_arg = focalLength;
}

#ifdef __RVV10__
inline bool
rvvAnalyzeClassIsFinite(const std::uint32_t cls)
{
  constexpr std::uint32_t not_finite_bits =
      (1u << 0u) | (1u << 7u) | (1u << 8u) | (1u << 9u);
  return (cls & not_finite_bits) == 0u;
}

template<typename PointT> inline bool
analyzeOrganizedCloudRVV(const pcl::PointCloud<PointT>& cloud_arg,
                         float& maxDepth_arg,
                         float& focalLength_arg)
{
  if constexpr (!pcl::rvv::kRVVXYZAoSPointCompatible<PointT>)
  {
    return false;
  }
  else
  {
    const std::size_t width = cloud_arg.width;
    const std::size_t height = cloud_arg.height;
    if (width <= 1 || height <= 1 || width*height != cloud_arg.size ())
      return false;

    const int centerX = static_cast<int> (width / 2);
    const int centerY = static_cast<int> (height / 2);
    const std::size_t scan_width = static_cast<std::size_t> (centerX) * 2u;
    const std::size_t scan_height = static_cast<std::size_t> (centerY) * 2u;
    const std::size_t scan_size = scan_width * scan_height;

    float maxDepth = 0;
    float focalLength = 0;
    if (scan_size == 0)
    {
      maxDepth_arg = maxDepth;
      focalLength_arg = focalLength;
      return true;
    }

    const std::size_t vlmax = __riscv_vsetvlmax_e32m2 ();
    std::vector<float> z_values (vlmax, 0.0f);
    std::vector<std::uint32_t> class_x (vlmax, 0);
    std::vector<std::uint32_t> class_y (vlmax, 0);
    std::vector<std::uint32_t> class_z (vlmax, 0);

    const auto* base = reinterpret_cast<const std::uint8_t*> (cloud_arg.points.data ());

    std::size_t max_index = 0;
    bool found = false;
    std::size_t i = 0;
    while (i < scan_size)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (scan_size - i);
      vfloat32m2_t x;
      vfloat32m2_t y;
      vfloat32m2_t z;
      pcl::rvv_load::strided_load3_f32m2<sizeof(PointT),
                                         pcl::rvv::RVVXYZAoSFloatLayout<PointT>::kX,
                                         pcl::rvv::RVVXYZAoSFloatLayout<PointT>::kY,
                                         pcl::rvv::RVVXYZAoSFloatLayout<PointT>::kZ>(
          base + i * sizeof(PointT), vl, x, y, z);

      __riscv_vse32_v_f32m2 (z_values.data (), z, vl);
      __riscv_vse32_v_u32m2 (class_x.data (), __riscv_vfclass_v_u32m2 (x, vl), vl);
      __riscv_vse32_v_u32m2 (class_y.data (), __riscv_vfclass_v_u32m2 (y, vl), vl);
      __riscv_vse32_v_u32m2 (class_z.data (), __riscv_vfclass_v_u32m2 (z, vl), vl);

      for (std::size_t lane = 0; lane < vl; ++lane)
      {
        if (rvvAnalyzeClassIsFinite (class_x[lane]) &&
            rvvAnalyzeClassIsFinite (class_y[lane]) &&
            rvvAnalyzeClassIsFinite (class_z[lane]) &&
            (!found || maxDepth < z_values[lane]))
        {
          found = true;
          maxDepth = z_values[lane];
          max_index = i + lane;
        }
      }
      i += vl;
    }

    if (found)
    {
      const int x = static_cast<int> (max_index % scan_width) - centerX;
      const int y = static_cast<int> (max_index / scan_width) - centerY;
      const PointT& point = cloud_arg[max_index];
      focalLength = 2.0f / (point.x / (static_cast<float> (x) * point.z) + point.y / (static_cast<float> (y) * point.z));
    }

    maxDepth_arg = maxDepth;
    focalLength_arg = focalLength;
    return true;
  }
}
#endif

template<typename PointT> inline void
analyzeOrganizedCloud(const pcl::PointCloud<PointT>& cloud_arg,
                      float& maxDepth_arg,
                      float& focalLength_arg)
{
  assert ((cloud_arg.width>1) && (cloud_arg.height>1));
  assert (static_cast<std::size_t> (cloud_arg.width) * cloud_arg.height == cloud_arg.size ());

#ifdef __RVV10__
  if (analyzeOrganizedCloudRVV (cloud_arg, maxDepth_arg, focalLength_arg))
    return;
#endif

  analyzeOrganizedCloudStd (cloud_arg, maxDepth_arg, focalLength_arg);
}

} // namespace organized_compression_detail
} // namespace io
} // namespace pcl
