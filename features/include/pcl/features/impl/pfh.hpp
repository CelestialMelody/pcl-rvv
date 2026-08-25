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
 */

#pragma once

#include <pcl/features/pfh.h>
#include <pcl/features/pfh_tools.h> // for computePairFeatures

#include <pcl/common/point_tests.h> // for pcl::isFinite

#include <cmath>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>

#include <riscv_vector.h>

#include <cstdint>
#include <type_traits>
#include <vector>
#endif

#if defined(__RVV10__)
namespace pcl::detail
{
template <typename PointT, bool HasNormal = pcl::traits::has_normal<PointT>::value>
struct PFHNormalAoSFloatLayout : std::false_type {};

template <typename PointT>
struct PFHNormalAoSFloatLayout<PointT, true> {
  using Pod = typename pcl::traits::POD<PointT>::type;

  static constexpr std::size_t kNX = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalX;
  static constexpr std::size_t kNY = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalY;
  static constexpr std::size_t kNZ = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalZ;

  static constexpr bool value =
      pcl::rvv::RVVNormalFloatLayout<PointT>::value &&
      std::is_standard_layout_v<Pod> && sizeof(PointT) == sizeof(Pod) &&
      sizeof(PointT) % alignof(float) == 0 && kNX % alignof(float) == 0 &&
      kNY % alignof(float) == 0 && kNZ % alignof(float) == 0;
};

template <typename PointInT, typename PointNT>
inline constexpr bool kPFHDirectAoSRVVSupportedPointTypes =
    (std::is_same_v<PointInT, pcl::PointNormal> &&
     std::is_same_v<PointNT, pcl::PointNormal>) ||
    (std::is_same_v<PointInT, pcl::PointXYZ> &&
     std::is_same_v<PointNT, pcl::Normal>);

inline int
pfhRVVBinAngularFeature (const float value, const int bins)
{
  const float scaled = static_cast<float> (bins) *
                       ((value + static_cast<float> (M_PI)) *
                        (1.0f / (2.0f * static_cast<float> (M_PI))));
  int bin = static_cast<int> (std::floor (scaled));
  if (bin < 0) return 0;
  if (bin >= bins) return bins - 1;
  return bin;
}

inline int
pfhRVVBinUnitFeature (const float value, const int bins)
{
  int bin = static_cast<int> (std::floor (static_cast<float> (bins) *
                                          ((value + 1.0f) * 0.5f)));
  if (bin < 0) return 0;
  if (bin >= bins) return bins - 1;
  return bin;
}

template <typename PointInT, typename PointNT> bool
computePointPFHSignatureDirectAoSRVV (const pcl::PointCloud<PointInT> &cloud,
                                      const pcl::PointCloud<PointNT> &normals,
                                      const pcl::Indices &indices,
                                      const int nr_split,
                                      Eigen::VectorXf &pfh_histogram)
{
  if constexpr (!kPFHDirectAoSRVVSupportedPointTypes<PointInT, PointNT> ||
                !pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value ||
                !PFHNormalAoSFloatLayout<PointNT>::value)
    return false;
  else
  {
    if (nr_split != 5 || indices.size () < 4 ||
        cloud.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointInT> () ||
        normals.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ())
      return false;

    std::vector<std::uint32_t> p1_offsets;
    std::vector<std::uint32_t> p2_offsets;
    std::vector<std::uint32_t> n1_offsets;
    std::vector<std::uint32_t> n2_offsets;
    const std::size_t pair_capacity = indices.size () * (indices.size () - 1) / 2;
    p1_offsets.reserve (pair_capacity);
    p2_offsets.reserve (pair_capacity);
    n1_offsets.reserve (pair_capacity);
    n2_offsets.reserve (pair_capacity);

    for (std::size_t i_idx = 0; i_idx < indices.size (); ++i_idx)
    {
      for (std::size_t j_idx = 0; j_idx < i_idx; ++j_idx)
      {
        const auto i_index = indices[i_idx];
        const auto j_index = indices[j_idx];
        if (i_index < 0 || j_index < 0)
          return false;

        const auto i = static_cast<std::size_t> (i_index);
        const auto j = static_cast<std::size_t> (j_index);
        if (i >= cloud.size () || j >= cloud.size () ||
            i >= normals.size () || j >= normals.size ())
          return false;

        if (!isFinite (cloud[i]) || !isFinite (cloud[j]))
          continue;

        p1_offsets.push_back (static_cast<std::uint32_t> (i * sizeof (PointInT)));
        p2_offsets.push_back (static_cast<std::uint32_t> (j * sizeof (PointInT)));
        n1_offsets.push_back (static_cast<std::uint32_t> (i * sizeof (PointNT)));
        n2_offsets.push_back (static_cast<std::uint32_t> (j * sizeof (PointNT)));
      }
    }

    pfh_histogram.setZero (nr_split * nr_split * nr_split);
    const float hist_incr =
        100.0f / static_cast<float> (indices.size () * (indices.size () - 1) / 2);

    std::vector<float> f1 (p1_offsets.size ());
    std::vector<float> f2 (p1_offsets.size ());
    std::vector<float> f3 (p1_offsets.size ());
    std::vector<std::int32_t> valid (p1_offsets.size ());

    using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointInT>;
    using NormalLayout = PFHNormalAoSFloatLayout<PointNT>;
    const auto* point_base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());
    const auto* normal_base = reinterpret_cast<const std::uint8_t*> (normals.points.data ());
    for (std::size_t offset = 0; offset < p1_offsets.size ();)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (p1_offsets.size () - offset);
      const vuint32m2_t p1 =
          __riscv_vle32_v_u32m2 (p1_offsets.data () + offset, vl);
      const vuint32m2_t p2 =
          __riscv_vle32_v_u32m2 (p2_offsets.data () + offset, vl);
      const vuint32m2_t n1 =
          __riscv_vle32_v_u32m2 (n1_offsets.data () + offset, vl);
      const vuint32m2_t n2 =
          __riscv_vle32_v_u32m2 (n2_offsets.data () + offset, vl);

      vfloat32m2_t p1x, p1y, p1z, p2x, p2y, p2z;
      vfloat32m2_t n1x, n1y, n1z, n2x, n2y, n2z;
      pcl::rvv_load::indexed_load3_fields_f32m2<PointInT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
          point_base, p1, vl, p1x, p1y, p1z);
      pcl::rvv_load::indexed_load3_fields_f32m2<PointInT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
          point_base, p2, vl, p2x, p2y, p2z);
      pcl::rvv_load::indexed_load3_fields_f32m2<PointNT, NormalLayout::kNX, NormalLayout::kNY, NormalLayout::kNZ> (
          normal_base, n1, vl, n1x, n1y, n1z);
      pcl::rvv_load::indexed_load3_fields_f32m2<PointNT, NormalLayout::kNX, NormalLayout::kNY, NormalLayout::kNZ> (
          normal_base, n2, vl, n2x, n2y, n2z);

      vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (p2x, p1x, vl);
      vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (p2y, p1y, vl);
      vfloat32m2_t dz = __riscv_vfsub_vv_f32m2 (p2z, p1z, vl);
      vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl);
      dist2 = __riscv_vfmacc_vv_f32m2 (dist2, dz, dz, vl);
      const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2 (dist2, vl);
      vbool16_t lane_valid = __riscv_vmfne_vf_f32m2_b16 (dist, 0.0f, vl);

      const vfloat32m2_t n1_dot_delta = __riscv_vfmacc_vv_f32m2 (
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n1x, dx, vl), n1y, dy, vl), n1z, dz, vl);
      const vfloat32m2_t n2_dot_delta = __riscv_vfmacc_vv_f32m2 (
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n2x, dx, vl), n2y, dy, vl), n2z, dz, vl);
      const vfloat32m2_t angle1 = __riscv_vfdiv_vv_f32m2 (n1_dot_delta, dist, vl);
      const vfloat32m2_t angle2 = __riscv_vfdiv_vv_f32m2 (n2_dot_delta, dist, vl);
      const vfloat32m2_t abs_angle1 = __riscv_vfsgnjx_vv_f32m2 (angle1, angle1, vl);
      const vfloat32m2_t abs_angle2 = __riscv_vfsgnjx_vv_f32m2 (angle2, angle2, vl);
      const vbool16_t swap = __riscv_vmflt_vv_f32m2_b16 (abs_angle1, abs_angle2, vl);

      dx = __riscv_vmerge_vvm_f32m2 (dx, __riscv_vfneg_v_f32m2 (dx, vl), swap, vl);
      dy = __riscv_vmerge_vvm_f32m2 (dy, __riscv_vfneg_v_f32m2 (dy, vl), swap, vl);
      dz = __riscv_vmerge_vvm_f32m2 (dz, __riscv_vfneg_v_f32m2 (dz, vl), swap, vl);

      const vfloat32m2_t ux = __riscv_vmerge_vvm_f32m2 (n1x, n2x, swap, vl);
      const vfloat32m2_t uy = __riscv_vmerge_vvm_f32m2 (n1y, n2y, swap, vl);
      const vfloat32m2_t uz = __riscv_vmerge_vvm_f32m2 (n1z, n2z, swap, vl);
      const vfloat32m2_t target_nx = __riscv_vmerge_vvm_f32m2 (n2x, n1x, swap, vl);
      const vfloat32m2_t target_ny = __riscv_vmerge_vvm_f32m2 (n2y, n1y, swap, vl);
      const vfloat32m2_t target_nz = __riscv_vmerge_vvm_f32m2 (n2z, n1z, swap, vl);
      const vfloat32m2_t vf3 = __riscv_vmerge_vvm_f32m2 (angle1, __riscv_vfneg_v_f32m2 (angle2, vl), swap, vl);

      vfloat32m2_t vx = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dy, uz, vl),
                                               __riscv_vfmul_vv_f32m2 (dz, uy, vl),
                                               vl);
      vfloat32m2_t vy = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dz, ux, vl),
                                               __riscv_vfmul_vv_f32m2 (dx, uz, vl),
                                               vl);
      vfloat32m2_t vz = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, uy, vl),
                                               __riscv_vfmul_vv_f32m2 (dy, ux, vl),
                                               vl);
      vfloat32m2_t vnorm2 = __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vx, vx, vl), vy, vy, vl);
      vnorm2 = __riscv_vfmacc_vv_f32m2 (vnorm2, vz, vz, vl);
      const vfloat32m2_t vnorm = __riscv_vfsqrt_v_f32m2 (vnorm2, vl);
      lane_valid = __riscv_vmand_mm_b16 (lane_valid, __riscv_vmfne_vf_f32m2_b16 (vnorm, 0.0f, vl), vl);
      vx = __riscv_vfdiv_vv_f32m2 (vx, vnorm, vl);
      vy = __riscv_vfdiv_vv_f32m2 (vy, vnorm, vl);
      vz = __riscv_vfdiv_vv_f32m2 (vz, vnorm, vl);

      const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (uy, vz, vl),
                                                     __riscv_vfmul_vv_f32m2 (uz, vy, vl),
                                                     vl);
      const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (uz, vx, vl),
                                                     __riscv_vfmul_vv_f32m2 (ux, vz, vl),
                                                     vl);
      const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (ux, vy, vl),
                                                     __riscv_vfmul_vv_f32m2 (uy, vx, vl),
                                                     vl);
      const vfloat32m2_t vf2 = __riscv_vfmacc_vv_f32m2 (
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vx, target_nx, vl), vy, target_ny, vl),
          vz,
          target_nz,
          vl);
      const vfloat32m2_t atan_y = __riscv_vfmacc_vv_f32m2 (
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (wx, target_nx, vl), wy, target_ny, vl),
          wz,
          target_nz,
          vl);
      const vfloat32m2_t atan_x = __riscv_vfmacc_vv_f32m2 (
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (ux, target_nx, vl), uy, target_ny, vl),
          uz,
          target_nz,
          vl);
      const vfloat32m2_t vf1 = pcl::atan2_RVV_f32m2 (atan_y, atan_x, vl);

      __riscv_vse32_v_f32m2 (f1.data () + offset, vf1, vl);
      __riscv_vse32_v_f32m2 (f2.data () + offset, vf2, vl);
      __riscv_vse32_v_f32m2 (f3.data () + offset, vf3, vl);
      const vint32m2_t zero = __riscv_vmv_v_x_i32m2 (0, vl);
      const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2 (zero, 1, lane_valid, vl);
      __riscv_vse32_v_i32m2 (valid.data () + offset, vvalid, vl);
      offset += vl;
    }

    for (std::size_t i = 0; i < valid.size (); ++i)
    {
      if (valid[i] == 0)
        continue;
      const int b1 = pfhRVVBinAngularFeature (f1[i], nr_split);
      const int b2 = pfhRVVBinUnitFeature (f2[i], nr_split);
      const int b3 = pfhRVVBinUnitFeature (f3[i], nr_split);
      pfh_histogram[b1 + nr_split * b2 + nr_split * nr_split * b3] += hist_incr;
    }

    return true;
  }
}
} // namespace pcl::detail
#endif


//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> bool
pcl::PFHEstimation<PointInT, PointNT, PointOutT>::computePairFeatures (
      const pcl::PointCloud<PointInT> &cloud, const pcl::PointCloud<PointNT> &normals,
      int p_idx, int q_idx, float &f1, float &f2, float &f3, float &f4)
{
  pcl::computePairFeatures (cloud[p_idx].getVector4fMap (), normals[p_idx].getNormalVector4fMap (),
                            cloud[q_idx].getVector4fMap (), normals[q_idx].getNormalVector4fMap (),
                            f1, f2, f3, f4);
  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> void
pcl::PFHEstimation<PointInT, PointNT, PointOutT>::computePointPFHSignature (
      const pcl::PointCloud<PointInT> &cloud, const pcl::PointCloud<PointNT> &normals,
      const pcl::Indices &indices, int nr_split, Eigen::VectorXf &pfh_histogram)
{
  int h_index, h_p;

  // Clear the resultant point histogram
  pfh_histogram.setZero ();

  // Factorization constant
  float hist_incr = 100.0f / static_cast<float> (indices.size () * (indices.size () - 1) / 2);

#if defined(__RVV10__)
  if (!use_cache_ &&
      pcl::detail::computePointPFHSignatureDirectAoSRVV (cloud, normals, indices, nr_split, pfh_histogram))
    return;
#endif

  std::pair<int, int> key;
  bool key_found = false;

  // Iterate over all the points in the neighborhood
  for (std::size_t i_idx = 0; i_idx < indices.size (); ++i_idx)
  {
    for (std::size_t j_idx = 0; j_idx < i_idx; ++j_idx)
    {
      // If the 3D points are invalid, don't bother estimating, just continue
      if (!isFinite (cloud[indices[i_idx]]) || !isFinite (cloud[indices[j_idx]]))
        continue;

      if (use_cache_)
      {
        // In order to create the key, always use the smaller index as the first key pair member
        int p1, p2;
  //      if (indices[i_idx] >= indices[j_idx])
  //      {
          p1 = indices[i_idx];
          p2 = indices[j_idx];
  //      }
  //      else
  //      {
  //        p1 = indices[j_idx];
  //        p2 = indices[i_idx];
  //      }
        key = std::pair<int, int> (p1, p2);

        // Check to see if we already estimated this pair in the global hashmap
        auto fm_it = feature_map_.find (key);
        if (fm_it != feature_map_.end ())
        {
          pfh_tuple_ = fm_it->second;
          key_found = true;
        }
        else
        {
          // Compute the pair NNi to NNj
          if (!computePairFeatures (cloud, normals, indices[i_idx], indices[j_idx],
                                    pfh_tuple_[0], pfh_tuple_[1], pfh_tuple_[2], pfh_tuple_[3]))
            continue;

          key_found = false;
        }
      }
      else
        if (!computePairFeatures (cloud, normals, indices[i_idx], indices[j_idx],
                                  pfh_tuple_[0], pfh_tuple_[1], pfh_tuple_[2], pfh_tuple_[3]))
          continue;

      // Normalize the f1, f2, f3 features and push them in the histogram
      f_index_[0] = static_cast<int> (std::floor (nr_split * ((pfh_tuple_[0] + M_PI) * d_pi_)));
      if (f_index_[0] < 0)         f_index_[0] = 0;
      if (f_index_[0] >= nr_split) f_index_[0] = nr_split - 1;

      f_index_[1] = static_cast<int> (std::floor (nr_split * ((pfh_tuple_[1] + 1.0) * 0.5)));
      if (f_index_[1] < 0)         f_index_[1] = 0;
      if (f_index_[1] >= nr_split) f_index_[1] = nr_split - 1;

      f_index_[2] = static_cast<int> (std::floor (nr_split * ((pfh_tuple_[2] + 1.0) * 0.5)));
      if (f_index_[2] < 0)         f_index_[2] = 0;
      if (f_index_[2] >= nr_split) f_index_[2] = nr_split - 1;

      // Copy into the histogram
      h_index = 0;
      h_p     = 1;
      for (const int &d : f_index_)
      {
        h_index += h_p * d;
        h_p     *= nr_split;
      }
      pfh_histogram[h_index] += hist_incr;

      if (use_cache_ && !key_found)
      {
        // Save the value in the hashmap
        feature_map_[key] = pfh_tuple_;

        // Use a maximum cache so that we don't go overboard on RAM usage
        key_list_.push (key);
        // Check to see if we need to remove an element due to exceeding max_size
        if (key_list_.size () > max_cache_size_)
        {
          // Remove the oldest element.
          feature_map_.erase (key_list_.front ());
          key_list_.pop ();
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> void
pcl::PFHEstimation<PointInT, PointNT, PointOutT>::computeFeature (PointCloudOut &output)
{
  // Clear the feature map
  feature_map_.clear ();
  std::queue<std::pair<int, int> > empty;
  std::swap (key_list_, empty);

  pfh_histogram_.setZero (static_cast<Eigen::Index>(nr_subdiv_) * nr_subdiv_ * nr_subdiv_);

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
        for (Eigen::Index d = 0; d < pfh_histogram_.size (); ++d)
          output[idx].histogram[d] = std::numeric_limits<float>::quiet_NaN ();

        output.is_dense = false;
        continue;
      }

      // Estimate the PFH signature at each patch
      computePointPFHSignature (*surface_, *normals_, nn_indices, nr_subdiv_, pfh_histogram_);

      // Copy into the resultant cloud
      for (Eigen::Index d = 0; d < pfh_histogram_.size (); ++d)
        output[idx].histogram[d] = pfh_histogram_[d];
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
        for (Eigen::Index d = 0; d < pfh_histogram_.size (); ++d)
          output[idx].histogram[d] = std::numeric_limits<float>::quiet_NaN ();

        output.is_dense = false;
        continue;
      }

      // Estimate the PFH signature at each patch
      computePointPFHSignature (*surface_, *normals_, nn_indices, nr_subdiv_, pfh_histogram_);

      // Copy into the resultant cloud
      for (Eigen::Index d = 0; d < pfh_histogram_.size (); ++d)
        output[idx].histogram[d] = pfh_histogram_[d];
    }
  }
}

#define PCL_INSTANTIATE_PFHEstimation(T,NT,OutT) template class PCL_EXPORTS pcl::PFHEstimation<T,NT,OutT>;
