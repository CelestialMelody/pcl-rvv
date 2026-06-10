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

#ifndef PCL_FILTERS_BILATERAL_IMPL_H_
#define PCL_FILTERS_BILATERAL_IMPL_H_

#include <pcl/filters/bilateral.h>
#include <pcl/common/common.h>
#include <pcl/search/auto.h> // for autoSelectMethod
#include <pcl/common/point_tests.h> // for isXYZFinite
#include <pcl/common/rvv_point_load.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl
{

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> double
computePointWeightStd (const typename pcl::PointCloud<PointT>::ConstPtr& input,
                       const int pid,
                       const pcl::Indices &indices,
                       const std::vector<float> &distances,
                       const double sigma_s,
                       const double sigma_r)
{
  double BF = 0, W = 0;

  // For each neighbor
  for (std::size_t n_id = 0; n_id < indices.size (); ++n_id)
  {
    int id = indices[n_id];
    // Compute the difference in intensity
    double intensity_dist = std::abs ((*input)[pid].intensity - (*input)[id].intensity);

    // Compute the Gaussian intensity weights both in Euclidean and in intensity space
    double dist = std::sqrt (distances[n_id]);
    double weight = std::exp (- (dist * dist)/(2 * sigma_s * sigma_s)) *
                    std::exp (- (intensity_dist * intensity_dist)/(2 * sigma_r * sigma_r));

    // Calculate the bilateral filter response
    BF += weight * (*input)[id].intensity;
    W += weight;
  }
  return (BF / W);
}

#ifdef __RVV10__
//////////////////////////////////////////////////////////////////////////////////////////////
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("no-tree-vectorize")
#endif
template <typename PointT> double
computePointWeightRVV (const typename pcl::PointCloud<PointT>::ConstPtr& input,
                       const int pid,
                       const pcl::Indices &indices,
                       const std::vector<float> &distances,
                       const double sigma_s,
                       const double sigma_r)
{
  // The RVV path only covers radiusSearch neighbor lists where each distance
  // entry still lines up with one non-negative point index.  Short lists keep
  // the original scalar path to avoid paying vector setup and stack staging.
  if (indices.size () < 16 || indices.size () != distances.size ())
    return pcl::computePointWeightStd<PointT> (input, pid, indices, distances, sigma_s, sigma_r);

  const auto* base_u8 = reinterpret_cast<const std::uint8_t*> (input->points.data ());
  const auto* raw_indices = reinterpret_cast<const std::uint32_t*> (indices.data ());
  const float center_intensity = (*input)[pid].intensity;
  const float spatial_scale = static_cast<float> (-1.0 / (2.0 * sigma_s * sigma_s));
  const float intensity_scale = static_cast<float> (-1.0 / (2.0 * sigma_r * sigma_r));
  double BF = 0.0;
  double W = 0.0;

  constexpr std::size_t kMaxChunkLanes = 256;
  alignas(64) float weights[kMaxChunkLanes];
  alignas(64) float contribs[kMaxChunkLanes];

  std::size_t offset = 0;
  while (offset < indices.size ())
  {
    const std::size_t remaining = std::min<std::size_t> (indices.size () - offset, kMaxChunkLanes);
    const std::size_t vl = __riscv_vsetvl_e32m2 (remaining);
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2 (raw_indices + offset, vl);
    // Neighbor ids are produced by radiusSearch, so intensity is a gathered
    // PointXYZI field load rather than a contiguous array load.
    const vuint32m2_t v_point_offsets =
        pcl::rvv_load::byte_offsets_u32m2<PointT> (v_ids, vl);
    const vfloat32m2_t v_intensity =
        pcl::rvv_load::gather_load_f32m2<PointT, offsetof(PointT, intensity)> (
            base_u8, v_point_offsets, vl);
    const vfloat32m2_t v_squared = __riscv_vle32_v_f32m2 (distances.data () + offset, vl);
    const vfloat32m2_t v_center = __riscv_vfmv_v_f_f32m2 (center_intensity, vl);
    const vfloat32m2_t v_delta = __riscv_vfsub_vv_f32m2 (v_center, v_intensity, vl);
    // The scalar path computes sqrt(d2) and kernel() immediately squares it.
    // Using d2 directly keeps the same Gaussian argument while avoiding that
    // staging-only round trip; the approximation boundary is only common expf.
    const vfloat32m2_t v_spatial_arg = __riscv_vfmul_vf_f32m2 (v_squared, spatial_scale, vl);
    const vfloat32m2_t v_delta2 = __riscv_vfmul_vv_f32m2 (v_delta, v_delta, vl);
    const vfloat32m2_t v_intensity_arg = __riscv_vfmul_vf_f32m2 (v_delta2, intensity_scale, vl);
    const vfloat32m2_t v_weight =
        __riscv_vfmul_vv_f32m2 (pcl::expf_RVV_f32m2 (v_spatial_arg, vl),
                                pcl::expf_RVV_f32m2 (v_intensity_arg, vl),
                                vl);
    const vfloat32m2_t v_contrib = __riscv_vfmul_vv_f32m2 (v_weight, v_intensity, vl);

    __riscv_vse32_v_f32m2 (weights, v_weight, vl);
    __riscv_vse32_v_f32m2 (contribs, v_contrib, vl);
    // Preserve radiusSearch neighbor order; this avoids adding a vector
    // reduction-order difference on top of the accepted float exp approximation.
    for (std::size_t lane = 0; lane < vl; ++lane)
    {
      BF += static_cast<double> (contribs[lane]);
      W += static_cast<double> (weights[lane]);
    }
    offset += vl;
  }
  return (BF / W);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
#endif

} // namespace pcl

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> double
pcl::BilateralFilter<PointT>::computePointWeight (const int pid,
                                                  const Indices &indices,
                                                  const std::vector<float> &distances)
{
#if defined(__RVV10__)
  // Production coverage is intentionally limited to PointXYZI, the point type
  // whose intensity field layout is fixed for the RVV gather helper.
  if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
    return (pcl::computePointWeightRVV<PointT> (input_, pid, indices, distances, sigma_s_, sigma_r_));
#endif
  return (pcl::computePointWeightStd<PointT> (input_, pid, indices, distances, sigma_s_, sigma_r_));
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::BilateralFilter<PointT>::applyFilter (PointCloud &output)
{
  // Check if sigma_s has been given by the user
  if (sigma_s_ == 0)
  {
    PCL_ERROR ("[pcl::BilateralFilter::applyFilter] Need a sigma_s value given before continuing.\n");
    return;
  }
  // In case a search method has not been given, initialize it using some defaults
  if (!tree_)
  {
    tree_.reset (pcl::search::autoSelectMethod<PointT>(input_, false, pcl::search::Purpose::radius_search));
  }
  else
  {
    tree_->setInputCloud (input_);
  }

  Indices k_indices;
  std::vector<float> k_distances;

  // Copy the input data into the output
  output = *input_;

  // For all the indices given (equal to the entire cloud if none given)
  for (const auto& idx : (*indices_))
  {
    if (input_->is_dense || pcl::isXYZFinite((*input_)[idx]))
    {
      // Perform a radius search to find the nearest neighbors
      tree_->radiusSearch (idx, sigma_s_ * 2, k_indices, k_distances);

      // Overwrite the intensity value with the computed average
      output[idx].intensity = static_cast<float> (computePointWeight (idx, k_indices, k_distances));
    }
  }
}
 
#define PCL_INSTANTIATE_BilateralFilter(T) template class PCL_EXPORTS pcl::BilateralFilter<T>;

#endif // PCL_FILTERS_BILATERAL_IMPL_H_
