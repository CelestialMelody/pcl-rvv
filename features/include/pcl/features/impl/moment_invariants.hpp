/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *  Copyright (c) 2012-, Open Perception, Inc.
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

#ifndef PCL_FEATURES_IMPL_MOMENT_INVARIANTS_H_
#define PCL_FEATURES_IMPL_MOMENT_INVARIANTS_H_

#include <pcl/features/moment_invariants.h>
#include <pcl/common/centroid.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>
#include <riscv_vector.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PCL_MOMENT_INVARIANTS_RVV_NOINLINE __attribute__((noinline))
#else
#define PCL_MOMENT_INVARIANTS_RVV_NOINLINE
#endif

namespace pcl
{
namespace detail
{
  inline void
  momentInvariantsFinalize (const float mu200, const float mu020, const float mu002,
                            const float mu110, const float mu101, const float mu011,
                            float& j1, float& j2, float& j3)
  {
    j1 = mu200             + mu020               + mu002;
    j2 = mu200*mu020       + mu200*mu002         + mu020*mu002       - mu110*mu110       - mu101*mu101       - mu011*mu011;
    j3 = mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110*mu110 - mu020*mu101*mu101 - mu200*mu011*mu011;
  }

  template <typename PointT> void
  momentInvariantsIndexedMomentsStd (const pcl::PointCloud<PointT>& cloud,
                                     const pcl::Indices& indices,
                                     const Eigen::Vector4f& centroid,
                                     float& j1, float& j2, float& j3)
  {
    float mu200 = 0, mu020 = 0, mu002 = 0, mu110 = 0, mu101 = 0, mu011  = 0;

    for (const auto &index : indices)
    {
      const float dx = cloud[index].x - centroid[0];
      const float dy = cloud[index].y - centroid[1];
      const float dz = cloud[index].z - centroid[2];

      mu200 += dx * dx;
      mu020 += dy * dy;
      mu002 += dz * dz;
      mu110 += dx * dy;
      mu101 += dx * dz;
      mu011 += dy * dz;
    }

    momentInvariantsFinalize (mu200, mu020, mu002, mu110, mu101, mu011, j1, j2, j3);
  }

  template <typename PointT> void
  momentInvariantsFullMomentsStd (const pcl::PointCloud<PointT>& cloud,
                                  const Eigen::Vector4f& centroid,
                                  float& j1, float& j2, float& j3)
  {
    float mu200 = 0, mu020 = 0, mu002 = 0, mu110 = 0, mu101 = 0, mu011  = 0;

    for (const auto& point: cloud.points)
    {
      const float dx = point.x - centroid[0];
      const float dy = point.y - centroid[1];
      const float dz = point.z - centroid[2];

      mu200 += dx * dx;
      mu020 += dy * dy;
      mu002 += dz * dz;
      mu110 += dx * dy;
      mu101 += dx * dz;
      mu011 += dy * dz;
    }

    momentInvariantsFinalize (mu200, mu020, mu002, mu110, mu101, mu011, j1, j2, j3);
  }

#if defined(__RVV10__)
  inline constexpr std::size_t kMomentInvariantsMinRVVPoints = 16;

  inline float
  momentInvariantsReduceSumF32m2 (const vfloat32m2_t values, const std::size_t vl)
  {
    const vfloat32m1_t init = __riscv_vfmv_s_f_f32m1 (0.0f, 1);
    const vfloat32m1_t reduced = __riscv_vfredusum_vs_f32m2_f32m1 (values, init, vl);
    return __riscv_vfmv_f_s_f32m1_f32 (reduced);
  }

  template <typename PointT>
  PCL_MOMENT_INVARIANTS_RVV_NOINLINE bool
  momentInvariantsIndexedMomentsRVV (const pcl::PointCloud<PointT>& cloud,
                                     const pcl::Indices& indices,
                                     const Eigen::Vector4f& centroid,
                                     float& j1, float& j2, float& j3)
  {
    if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value)
    {
      return false;
    }
    else
    {
      if (indices.size () < kMomentInvariantsMinRVVPoints ||
          !cloud.is_dense ||
          cloud.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
        return false;

      using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
      using Pod = typename Layout::Pod;

      float mu200 = 0, mu020 = 0, mu002 = 0, mu110 = 0, mu101 = 0, mu011  = 0;
      const auto* base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());

      std::size_t offset = 0;
      while (offset < indices.size ())
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (indices.size () - offset);
        const vuint32m2_t v_index =
            __riscv_vreinterpret_v_i32m2_u32m2 (__riscv_vle32_v_i32m2 (indices.data () + offset, vl));
        const vuint32m2_t v_byte_offset = pcl::rvv_load::byte_offsets_u32m2<Pod> (v_index, vl);

        vfloat32m2_t vx;
        vfloat32m2_t vy;
        vfloat32m2_t vz;
        pcl::rvv_load::indexed_load3_f32m2<Pod, Layout::kX, Layout::kY, Layout::kZ> (
            base, v_byte_offset, vl, vx, vy, vz);

        const vfloat32m2_t cx = __riscv_vfmv_v_f_f32m2 (centroid[0], vl);
        const vfloat32m2_t cy = __riscv_vfmv_v_f_f32m2 (centroid[1], vl);
        const vfloat32m2_t cz = __riscv_vfmv_v_f_f32m2 (centroid[2], vl);
        const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (vx, cx, vl);
        const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (vy, cy, vl);
        const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2 (vz, cz, vl);

        mu200 += momentInvariantsReduceSumF32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), vl);
        mu020 += momentInvariantsReduceSumF32m2 (__riscv_vfmul_vv_f32m2 (dy, dy, vl), vl);
        mu002 += momentInvariantsReduceSumF32m2 (__riscv_vfmul_vv_f32m2 (dz, dz, vl), vl);
        mu110 += momentInvariantsReduceSumF32m2 (__riscv_vfmul_vv_f32m2 (dx, dy, vl), vl);
        mu101 += momentInvariantsReduceSumF32m2 (__riscv_vfmul_vv_f32m2 (dx, dz, vl), vl);
        mu011 += momentInvariantsReduceSumF32m2 (__riscv_vfmul_vv_f32m2 (dy, dz, vl), vl);

        offset += vl;
      }

      momentInvariantsFinalize (mu200, mu020, mu002, mu110, mu101, mu011, j1, j2, j3);
      return true;
    }
  }
#endif

} // namespace detail
} // namespace pcl

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::MomentInvariantsEstimation<PointInT, PointOutT>::computePointMomentInvariants (
      const pcl::PointCloud<PointInT> &cloud, const pcl::Indices &indices,
      float &j1, float &j2, float &j3)
{
  // Estimate the XYZ centroid
  compute3DCentroid (cloud, indices, xyz_centroid_);

#if defined(__RVV10__)
  // Only xyz single-float AoS point types with MomentInvariants output enter
  // this indexed RVV accumulation; all other template instances keep scalar.
  if constexpr (std::is_same_v<PointOutT, pcl::MomentInvariants>)
  {
    if (pcl::detail::momentInvariantsIndexedMomentsRVV (cloud, indices, xyz_centroid_, j1, j2, j3))
      return;
  }
#endif

  pcl::detail::momentInvariantsIndexedMomentsStd (cloud, indices, xyz_centroid_, j1, j2, j3);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::MomentInvariantsEstimation<PointInT, PointOutT>::computePointMomentInvariants (
      const pcl::PointCloud<PointInT> &cloud, float &j1, float &j2, float &j3)
{
  // Estimate the XYZ centroid
  compute3DCentroid (cloud, xyz_centroid_);

  pcl::detail::momentInvariantsFullMomentsStd (cloud, xyz_centroid_, j1, j2, j3);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::MomentInvariantsEstimation<PointInT, PointOutT>::computeFeature (PointCloudOut &output)
{
  // Allocate enough space to hold the results
  // \note This resize is irrelevant for a radiusSearch ().
  pcl::Indices nn_indices (k_);
  std::vector<float> nn_dists (k_);

  output.is_dense = true;
  // Save a few cycles by not checking every point for NaN/Inf values if the cloud is set to dense
  if (input_->is_dense)
  {
    // Iterating over the entire index vector
    for (std::size_t idx = 0; idx < indices_->size (); ++idx)
    {
      if (this->searchForNeighbors ((*indices_)[idx], search_parameter_, nn_indices, nn_dists) == 0)
      {
        output[idx].j1 = output[idx].j2 = output[idx].j3 = std::numeric_limits<float>::quiet_NaN ();
        output.is_dense = false;
        continue;
      }
     
      computePointMomentInvariants (*surface_, nn_indices,
                                    output[idx].j1, output[idx].j2, output[idx].j3);
    }
  }
  else
  {
    // Iterating over the entire index vector
    for (std::size_t idx = 0; idx < indices_->size (); ++idx)
    {
      if (!isFinite ((*input_)[(*indices_)[idx]]) ||
          this->searchForNeighbors ((*indices_)[idx], search_parameter_, nn_indices, nn_dists) == 0)
      {
        output[idx].j1 = output[idx].j2 = output[idx].j3 = std::numeric_limits<float>::quiet_NaN ();
        output.is_dense = false;
        continue;
      }

      computePointMomentInvariants (*surface_, nn_indices,
                                    output[idx].j1, output[idx].j2, output[idx].j3);
    }
  }
}

#define PCL_INSTANTIATE_MomentInvariantsEstimation(T,NT) template class PCL_EXPORTS pcl::MomentInvariantsEstimation<T,NT>;

#endif    // PCL_FEATURES_IMPL_MOMENT_INVARIANTS_H_ 
