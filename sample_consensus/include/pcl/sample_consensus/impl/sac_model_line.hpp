/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2009, Willow Garage, Inc.
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

#ifndef PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_LINE_H_
#define PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_LINE_H_

#include <pcl/sample_consensus/sac_model_line.h>
#include <pcl/common/centroid.h>
#include <pcl/common/concatenate.h>
#include <pcl/common/eigen.h> // for eigen33

#if defined (__RVV10__)
#include <pcl/rvv_point_load.h>
#include <cstdint>
#include <type_traits>
#endif

//////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::SampleConsensusModelLine<PointT>::isSampleGood (const Indices &samples) const
{
  if (samples.size () != sample_size_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelLine::isSampleGood] Wrong number of samples (is %lu, should be %lu)!\n", samples.size (), sample_size_);
    return (false);
  }

  // Make sure that the two sample points are not identical
  if (
      std::abs ((*input_)[samples[0]].x - (*input_)[samples[1]].x) <= std::numeric_limits<float>::epsilon ()
    &&
      std::abs ((*input_)[samples[0]].y - (*input_)[samples[1]].y) <= std::numeric_limits<float>::epsilon ()
    &&
      std::abs ((*input_)[samples[0]].z - (*input_)[samples[1]].z) <= std::numeric_limits<float>::epsilon ())
  {
    PCL_ERROR ("[pcl::SampleConsensusModelLine::isSampleGood] The two sample points are (almost) identical!\n");
    return (false);
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::SampleConsensusModelLine<PointT>::computeModelCoefficients (
      const Indices &samples, Eigen::VectorXf &model_coefficients) const
{
  // Make sure that the samples are valid
  if (!isSampleGood (samples))
  {
    PCL_ERROR ("[pcl::SampleConsensusModelLine::computeModelCoefficients] Invalid set of samples given!\n");
    return (false);
  }

  model_coefficients.resize (model_size_);
  model_coefficients[0] = (*input_)[samples[0]].x;
  model_coefficients[1] = (*input_)[samples[0]].y;
  model_coefficients[2] = (*input_)[samples[0]].z;

  model_coefficients[3] = (*input_)[samples[1]].x - model_coefficients[0];
  model_coefficients[4] = (*input_)[samples[1]].y - model_coefficients[1];
  model_coefficients[5] = (*input_)[samples[1]].z - model_coefficients[2];

  // This precondition should hold if the samples have been found to be good
  assert (model_coefficients.template tail<3> ().squaredNorm () > 0.0f);

  model_coefficients.template tail<3> ().normalize ();
  PCL_DEBUG ("[pcl::SampleConsensusModelLine::computeModelCoefficients] Model is (%g,%g,%g,%g,%g,%g).\n",
             model_coefficients[0], model_coefficients[1], model_coefficients[2],
             model_coefficients[3], model_coefficients[4], model_coefficients[5]);
  return (true);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::getDistancesToModel (
      const Eigen::VectorXf &model_coefficients, std::vector<double> &distances) const
{
  // Needs a valid set of model coefficients
  if (!isModelValid (model_coefficients))
  {
    return;
  }

#if defined (__RVV10__)
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
                sizeof (pcl::index_t) == sizeof (std::int32_t) &&
                std::is_signed_v<pcl::index_t>)
  {
    getDistancesToModelRVV (model_coefficients, distances);
    return;
  }
#endif

  getDistancesToModelStandard (model_coefficients, distances);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::getDistancesToModelStandard (
      const Eigen::VectorXf &model_coefficients, std::vector<double> &distances) const
{
  distances.resize (indices_->size ());

  // Obtain the line point and direction
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0);
  line_dir.normalize ();

  // Iterate through the 3d points and calculate the distances from them to the line
  for (std::size_t i = 0; i < indices_->size (); ++i)
  {
    // Calculate the distance from the point to the line
    // D = ||(P2-P1) x (P1-P0)|| / ||P2-P1|| = norm (cross (p2-p1, p2-p0)) / norm(p2-p1)
    // Need to estimate sqrt here to keep MSAC and friends general
    distances[i] = sqrt ((line_pt - (*input_)[(*indices_)[i]].getVector4fMap ()).cross3 (line_dir).squaredNorm ());
  }
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::selectWithinDistance (
      const Eigen::VectorXf &model_coefficients, const double threshold, Indices &inliers)
{
  // Needs a valid set of model coefficients
  if (!isModelValid (model_coefficients))
    return;

#if defined (__RVV10__)
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
                sizeof (pcl::index_t) == sizeof (std::int32_t) &&
                std::is_signed_v<pcl::index_t>)
  {
    selectWithinDistanceRVV (model_coefficients, threshold, inliers);
    return;
  }
#endif

  selectWithinDistanceStandard (model_coefficients, threshold, inliers);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::selectWithinDistanceStandard (
      const Eigen::VectorXf &model_coefficients, const double threshold, Indices &inliers)
{
  double sqr_threshold = threshold * threshold;

  inliers.clear ();
  error_sqr_dists_.clear ();
  inliers.reserve (indices_->size ());
  error_sqr_dists_.reserve (indices_->size ());

  // Obtain the line point and direction
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0);
  line_dir.normalize ();

  // Iterate through the 3d points and calculate the distances from them to the line
  for (std::size_t i = 0; i < indices_->size (); ++i)
  {
    // Calculate the distance from the point to the line
    // D = ||(P2-P1) x (P1-P0)|| / ||P2-P1|| = norm (cross (p2-p1, p2-p0)) / norm(p2-p1)
    double sqr_distance = (line_pt - (*input_)[(*indices_)[i]].getVector4fMap ()).cross3 (line_dir).squaredNorm ();

    if (sqr_distance < sqr_threshold)
    {
      // Returns the indices of the points whose squared distances are smaller than the threshold
      inliers.push_back ((*indices_)[i]);
      error_sqr_dists_.push_back (sqr_distance);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> std::size_t
pcl::SampleConsensusModelLine<PointT>::countWithinDistance (
      const Eigen::VectorXf &model_coefficients, const double threshold) const
{
  // Needs a valid set of model coefficients
  if (!isModelValid (model_coefficients))
    return (0);

#if defined (__RVV10__)
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
                sizeof (pcl::index_t) == sizeof (std::int32_t) &&
                std::is_signed_v<pcl::index_t>)
  {
    return countWithinDistanceRVV (model_coefficients, threshold);
  }
#endif

  return countWithinDistanceStandard (model_coefficients, threshold);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> std::size_t
pcl::SampleConsensusModelLine<PointT>::countWithinDistanceStandard (
      const Eigen::VectorXf &model_coefficients, const double threshold) const
{
  double sqr_threshold = threshold * threshold;

  std::size_t nr_p = 0;

  // Obtain the line point and direction
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0.0f);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0.0f);
  line_dir.normalize ();

  // Iterate through the 3d points and calculate the distances from them to the line
  for (std::size_t i = 0; i < indices_->size (); ++i)
  {
    // Calculate the distance from the point to the line
    // D = ||(P2-P1) x (P1-P0)|| / ||P2-P1|| = norm (cross (p2-p1, p2-p0)) / norm(p2-p1)
    double sqr_distance = (line_pt - (*input_)[(*indices_)[i]].getVector4fMap ()).cross3 (line_dir).squaredNorm ();

    if (sqr_distance < sqr_threshold)
      nr_p++;
  }
  return (nr_p);
}

#if defined (__RVV10__)
//////////////////////////////////////////////////////////////////////////
template <typename PointT> std::size_t
pcl::SampleConsensusModelLine<PointT>::countWithinDistanceRVV (
      const Eigen::VectorXf &model_coefficients, const double threshold) const
{
  const std::size_t total_n = indices_->size ();
  if (input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
    return countWithinDistanceStandard (model_coefficients, threshold);

  Eigen::Vector3f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5]);
  line_dir.normalize ();

  const float px = model_coefficients[0];
  const float py = model_coefficients[1];
  const float pz = model_coefficients[2];
  const float dx_line = line_dir.x ();
  const float dy_line = line_dir.y ();
  const float dz_line = line_dir.z ();
  const float sqr_threshold = static_cast<float> (threshold * threshold);
  const std::uint8_t* const points_base =
      reinterpret_cast<const std::uint8_t*> (input_->points.data ());
  const pcl::index_t* const indices_ptr = indices_->data ();
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;

  std::size_t nr_p = 0;
  for (std::size_t i = 0; i < total_n; )
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
    const vuint32m2_t idx =
        __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
    vfloat32m2_t x_vec;
    vfloat32m2_t y_vec;
    vfloat32m2_t z_vec;
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
    pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
        points_base, offsets, vl, x_vec, y_vec, z_vec);

    const vfloat32m2_t vx = __riscv_vfsub_vf_f32m2 (x_vec, px, vl);
    const vfloat32m2_t vy = __riscv_vfsub_vf_f32m2 (y_vec, py, vl);
    const vfloat32m2_t vz = __riscv_vfsub_vf_f32m2 (z_vec, pz, vl);
    const vfloat32m2_t cross_x =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vy, dz_line, vl),
                                __riscv_vfmul_vf_f32m2 (vz, dy_line, vl),
                                vl);
    const vfloat32m2_t cross_y =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vz, dx_line, vl),
                                __riscv_vfmul_vf_f32m2 (vx, dz_line, vl),
                                vl);
    const vfloat32m2_t cross_z =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vx, dy_line, vl),
                                __riscv_vfmul_vf_f32m2 (vy, dx_line, vl),
                                vl);
    const vfloat32m2_t sqr =
        __riscv_vfmacc_vv_f32m2 (
            __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (cross_x, cross_x, vl),
                                      cross_y,
                                      cross_y,
                                      vl),
            cross_z,
            cross_z,
            vl);
    const vbool16_t inlier_mask = __riscv_vmflt_vf_f32m2_b16 (sqr, sqr_threshold, vl);
    nr_p += __riscv_vcpop_m_b16 (inlier_mask, vl);
    i += vl;
  }

  return nr_p;
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::selectWithinDistanceRVV (
      const Eigen::VectorXf &model_coefficients, const double threshold, Indices &inliers)
{
  const std::size_t total_n = indices_->size ();
  if (input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
  {
    selectWithinDistanceStandard (model_coefficients, threshold, inliers);
    return;
  }

  inliers.resize (total_n);
  error_sqr_dists_.resize (total_n);

  Eigen::Vector3f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5]);
  line_dir.normalize ();

  const float px = model_coefficients[0];
  const float py = model_coefficients[1];
  const float pz = model_coefficients[2];
  const float dx_line = line_dir.x ();
  const float dy_line = line_dir.y ();
  const float dz_line = line_dir.z ();
  const float sqr_threshold = static_cast<float> (threshold * threshold);
  const std::uint8_t* const points_base =
      reinterpret_cast<const std::uint8_t*> (input_->points.data ());
  const pcl::index_t* const indices_ptr = indices_->data ();
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
  double* const dists_out_ptr = error_sqr_dists_.data ();

  std::size_t nr_p = 0;
  for (std::size_t i = 0; i < total_n; )
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
    const vuint32m2_t idx =
        __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
    vfloat32m2_t x_vec;
    vfloat32m2_t y_vec;
    vfloat32m2_t z_vec;
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
    pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
        points_base, offsets, vl, x_vec, y_vec, z_vec);

    const vfloat32m2_t vx = __riscv_vfsub_vf_f32m2 (x_vec, px, vl);
    const vfloat32m2_t vy = __riscv_vfsub_vf_f32m2 (y_vec, py, vl);
    const vfloat32m2_t vz = __riscv_vfsub_vf_f32m2 (z_vec, pz, vl);
    const vfloat32m2_t cross_x =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vy, dz_line, vl),
                                __riscv_vfmul_vf_f32m2 (vz, dy_line, vl),
                                vl);
    const vfloat32m2_t cross_y =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vz, dx_line, vl),
                                __riscv_vfmul_vf_f32m2 (vx, dz_line, vl),
                                vl);
    const vfloat32m2_t cross_z =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vx, dy_line, vl),
                                __riscv_vfmul_vf_f32m2 (vy, dx_line, vl),
                                vl);
    const vfloat32m2_t sqr =
        __riscv_vfmacc_vv_f32m2 (
            __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (cross_x, cross_x, vl),
                                      cross_y,
                                      cross_y,
                                      vl),
            cross_z,
            cross_z,
            vl);
    const vbool16_t inlier_mask = __riscv_vmflt_vf_f32m2_b16 (sqr, sqr_threshold, vl);
    const std::size_t active_count = __riscv_vcpop_m_b16 (inlier_mask, vl);

    if (active_count > 0)
    {
      const vuint32m2_t compressed_idx =
          __riscv_vcompress_vm_u32m2 (idx, inlier_mask, vl);
      const vint32m2_t compressed_idx_i32 =
          __riscv_vreinterpret_v_u32m2_i32m2 (compressed_idx);
      __riscv_vse32_v_i32m2 (
          reinterpret_cast<std::int32_t*> (inliers.data () + nr_p),
          compressed_idx_i32,
          active_count);

      const vfloat32m2_t compressed_sqr =
          __riscv_vcompress_vm_f32m2 (sqr, inlier_mask, vl);
      const vfloat64m4_t compressed_sqr_d =
          __riscv_vfwcvt_f_f_v_f64m4 (compressed_sqr, active_count);
      __riscv_vse64_v_f64m4 (dists_out_ptr + nr_p, compressed_sqr_d, active_count);
      nr_p += active_count;
    }

    i += vl;
  }

  inliers.resize (nr_p);
  error_sqr_dists_.resize (nr_p);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::getDistancesToModelRVV (
      const Eigen::VectorXf &model_coefficients, std::vector<double> &distances) const
{
  const std::size_t total_n = indices_->size ();
  if (input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
  {
    getDistancesToModelStandard (model_coefficients, distances);
    return;
  }

  distances.resize (total_n);

  Eigen::Vector3f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5]);
  line_dir.normalize ();

  const float px = model_coefficients[0];
  const float py = model_coefficients[1];
  const float pz = model_coefficients[2];
  const float dx_line = line_dir.x ();
  const float dy_line = line_dir.y ();
  const float dz_line = line_dir.z ();
  const std::uint8_t* const points_base =
      reinterpret_cast<const std::uint8_t*> (input_->points.data ());
  const pcl::index_t* const indices_ptr = indices_->data ();
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
  double* const distances_ptr = distances.data ();

  for (std::size_t i = 0; i < total_n; )
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
    const vuint32m2_t idx =
        __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
    vfloat32m2_t x_vec;
    vfloat32m2_t y_vec;
    vfloat32m2_t z_vec;
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
    pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
        points_base, offsets, vl, x_vec, y_vec, z_vec);

    const vfloat32m2_t vx = __riscv_vfsub_vf_f32m2 (x_vec, px, vl);
    const vfloat32m2_t vy = __riscv_vfsub_vf_f32m2 (y_vec, py, vl);
    const vfloat32m2_t vz = __riscv_vfsub_vf_f32m2 (z_vec, pz, vl);
    const vfloat32m2_t cross_x =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vy, dz_line, vl),
                                __riscv_vfmul_vf_f32m2 (vz, dy_line, vl),
                                vl);
    const vfloat32m2_t cross_y =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vz, dx_line, vl),
                                __riscv_vfmul_vf_f32m2 (vx, dz_line, vl),
                                vl);
    const vfloat32m2_t cross_z =
        __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vx, dy_line, vl),
                                __riscv_vfmul_vf_f32m2 (vy, dx_line, vl),
                                vl);
    const vfloat32m2_t sqr =
        __riscv_vfmacc_vv_f32m2 (
            __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (cross_x, cross_x, vl),
                                      cross_y,
                                      cross_y,
                                      vl),
            cross_z,
            cross_z,
            vl);
    const vfloat32m2_t distance = __riscv_vfsqrt_v_f32m2 (sqr, vl);
    const vfloat64m4_t distance_d = __riscv_vfwcvt_f_f_v_f64m4 (distance, vl);
    __riscv_vse64_v_f64m4 (distances_ptr + i, distance_d, vl);
    i += vl;
  }
}
#endif // __RVV10__

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::optimizeModelCoefficients (
      const Indices &inliers, const Eigen::VectorXf &model_coefficients, Eigen::VectorXf &optimized_coefficients) const
{
  // Needs a valid set of model coefficients
  if (!isModelValid (model_coefficients))
  {
    optimized_coefficients = model_coefficients;
    return;
  }

  // Need more than the minimum sample size to make a difference
  if (inliers.size () <= sample_size_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelLine::optimizeModelCoefficients] Not enough inliers to refine/optimize the model's coefficients (%lu)! Returning the same coefficients.\n", inliers.size ());
    optimized_coefficients = model_coefficients;
    return;
  }

  optimized_coefficients.resize (model_size_);

  // Compute the 3x3 covariance matrix
  Eigen::Vector4f centroid;
  if (0 == compute3DCentroid (*input_, inliers, centroid))
  {
    PCL_WARN ("[pcl::SampleConsensusModelLine::optimizeModelCoefficients] compute3DCentroid failed (returned 0) because there are no valid inliers.\n");
    optimized_coefficients = model_coefficients;
    return;
  }
  Eigen::Matrix3f covariance_matrix;
  computeCovarianceMatrix (*input_, inliers, centroid, covariance_matrix);
  optimized_coefficients[0] = centroid[0];
  optimized_coefficients[1] = centroid[1];
  optimized_coefficients[2] = centroid[2];

  // Extract the eigenvalues and eigenvectors
  EIGEN_ALIGN16 Eigen::Vector3f eigen_values;
  EIGEN_ALIGN16 Eigen::Vector3f eigen_vector;
  pcl::eigen33 (covariance_matrix, eigen_values);
  pcl::computeCorrespondingEigenVector (covariance_matrix, eigen_values [2], eigen_vector);
  //pcl::eigen33 (covariance_matrix, eigen_vectors, eigen_values);

  optimized_coefficients.template tail<3> ().matrix () = eigen_vector;
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::SampleConsensusModelLine<PointT>::projectPoints (
      const Indices &inliers, const Eigen::VectorXf &model_coefficients, PointCloud &projected_points, bool copy_data_fields) const
{
  // Needs a valid model coefficients
  if (!isModelValid (model_coefficients))
  {
    PCL_ERROR ("[pcl::SampleConsensusModelLine::projectPoints] Given model is invalid!\n");
    return;
  }

  // Obtain the line point and direction
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0.0f);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0.0f);

  projected_points.header = input_->header;
  projected_points.is_dense = input_->is_dense;

  // Copy all the data fields from the input cloud to the projected one?
  if (copy_data_fields)
  {
    // Allocate enough space and copy the basics
    projected_points.resize (input_->size ());
    projected_points.width    = input_->width;
    projected_points.height   = input_->height;

    using FieldList = typename pcl::traits::fieldList<PointT>::type;
    // Iterate over each point
    for (std::size_t i = 0; i < projected_points.size (); ++i)
      // Iterate over each dimension
      pcl::for_each_type <FieldList> (NdConcatenateFunctor <PointT, PointT> ((*input_)[i], projected_points[i]));

    // Iterate through the 3d points and calculate the distances from them to the line
    for (const auto &inlier : inliers)
    {
      Eigen::Vector4f pt ((*input_)[inlier].x, (*input_)[inlier].y, (*input_)[inlier].z, 0.0f);
      // double k = (DOT_PROD_3D (points[i], p21) - dotA_B) / dotB_B;
      float k = (pt.dot (line_dir) - line_pt.dot (line_dir)) / line_dir.dot (line_dir);

      Eigen::Vector4f pp = line_pt + k * line_dir;
      // Calculate the projection of the point on the line (pointProj = A + k * B)
      projected_points[inlier].x = pp[0];
      projected_points[inlier].y = pp[1];
      projected_points[inlier].z = pp[2];
    }
  }
  else
  {
    // Allocate enough space and copy the basics
    projected_points.resize (inliers.size ());
    projected_points.width    = inliers.size ();
    projected_points.height   = 1;

    using FieldList = typename pcl::traits::fieldList<PointT>::type;
    // Iterate over each point
    for (std::size_t i = 0; i < inliers.size (); ++i)
      // Iterate over each dimension
      pcl::for_each_type <FieldList> (NdConcatenateFunctor <PointT, PointT> ((*input_)[inliers[i]], projected_points[i]));

    // Iterate through the 3d points and calculate the distances from them to the line
    for (std::size_t i = 0; i < inliers.size (); ++i)
    {
      Eigen::Vector4f pt ((*input_)[inliers[i]].x, (*input_)[inliers[i]].y, (*input_)[inliers[i]].z, 0.0f);
      // double k = (DOT_PROD_3D (points[i], p21) - dotA_B) / dotB_B;
      float k = (pt.dot (line_dir) - line_pt.dot (line_dir)) / line_dir.dot (line_dir);

      Eigen::Vector4f pp = line_pt + k * line_dir;
      // Calculate the projection of the point on the line (pointProj = A + k * B)
      projected_points[i].x = pp[0];
      projected_points[i].y = pp[1];
      projected_points[i].z = pp[2];
    }
  }
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::SampleConsensusModelLine<PointT>::doSamplesVerifyModel (
      const std::set<index_t> &indices, const Eigen::VectorXf &model_coefficients, const double threshold) const
{
  // Needs a valid set of model coefficients
  if (!isModelValid (model_coefficients))
    return (false);

  // Obtain the line point and direction
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0.0f);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0.0f);
  line_dir.normalize ();

  double sqr_threshold = threshold * threshold;
  // Iterate through the 3d points and calculate the distances from them to the line
  for (const auto &index : indices)
  {
    // Calculate the distance from the point to the line
    // D = ||(P2-P1) x (P1-P0)|| / ||P2-P1|| = norm (cross (p2-p1, p2-p0)) / norm(p2-p1)
    if ((line_pt - (*input_)[index].getVector4fMap ()).cross3 (line_dir).squaredNorm () > sqr_threshold)
      return (false);
  }

  return (true);
}

#define PCL_INSTANTIATE_SampleConsensusModelLine(T) template class PCL_EXPORTS pcl::SampleConsensusModelLine<T>;

#endif    // PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_LINE_H_
