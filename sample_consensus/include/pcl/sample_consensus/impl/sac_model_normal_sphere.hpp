/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2009-2012, Willow Garage, Inc.
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
 * $Id: sac_model_normal_sphere.hpp schrandt $
 *
 */

#ifndef PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_NORMAL_SPHERE_H_
#define PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_NORMAL_SPHERE_H_

#include <pcl/sample_consensus/sac_model_normal_sphere.h>
#include <pcl/common/common.h> // for getAngle3D
#include <pcl/rvv_point_load.h>

#include <cstdint>
#include <limits>
#include <type_traits>

#if defined (__GNUC__)
#define PCL_RVV_NORMAL_SPHERE_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_NORMAL_SPHERE_NOINLINE
#endif

namespace pcl
{
  namespace detail
  {
    template <typename PointT>
    inline double
    computeNormalSphereWeightedEuclidStandard (const PointT& point,
                                               const Eigen::Vector4f& center,
                                               const float radius,
                                               const double normal_distance_weight,
                                               Eigen::Vector4f& dir)
    {
      const Eigen::Vector4f pt (point.x, point.y, point.z, 0.0f);
      dir = pt - center;
      return ((1.0 - normal_distance_weight) * std::abs (dir.norm () - radius));
    }

    template <typename PointNT>
    inline double
    computeNormalSphereFinalDistanceStandard (const PointNT& normal,
                                              const Eigen::Vector4f& dir,
                                              const double weighted_euclid,
                                              const double normal_distance_weight)
    {
      const Eigen::Vector4f n (normal.normal[0], normal.normal[1], normal.normal[2], 0.0f);
      double d_normal = std::abs (pcl::getAngle3D (n, dir));
      d_normal = (std::min) (d_normal, M_PI - d_normal);
      return (std::abs (normal_distance_weight * d_normal + weighted_euclid));
    }

    template <typename PointT, typename PointNT>
    inline void
    selectWithinDistanceStandardNormalSphere (const pcl::PointCloud<PointT>& input,
                                              const pcl::PointCloud<PointNT>& normals,
                                              const pcl::Indices& indices,
                                              const Eigen::VectorXf& model_coefficients,
                                              const double threshold,
                                              const double normal_distance_weight,
                                              pcl::Indices& inliers,
                                              std::vector<double>& error_sqr_dists)
    {
      inliers.clear ();
      error_sqr_dists.clear ();
      inliers.reserve (indices.size ());
      error_sqr_dists.reserve (indices.size ());

      Eigen::Vector4f center = model_coefficients;
      center[3] = 0.0f;
      const float radius = model_coefficients[3];
      for (std::size_t i = 0; i < indices.size (); ++i)
      {
        const pcl::index_t index = indices[i];
        Eigen::Vector4f dir;
        const double weighted_euclid = computeNormalSphereWeightedEuclidStandard (
            input[index], center, radius, normal_distance_weight, dir);
        if (weighted_euclid > threshold)
          continue;
        const double distance = computeNormalSphereFinalDistanceStandard (
            normals[index], dir, weighted_euclid, normal_distance_weight);
        if (distance < threshold)
        {
          inliers.push_back (index);
          error_sqr_dists.push_back (distance);
        }
      }
    }

    template <typename PointT, typename PointNT>
    inline std::size_t
    countWithinDistanceStandardNormalSphere (const pcl::PointCloud<PointT>& input,
                                             const pcl::PointCloud<PointNT>& normals,
                                             const pcl::Indices& indices,
                                             const Eigen::VectorXf& model_coefficients,
                                             const double threshold,
                                             const double normal_distance_weight)
    {
      std::size_t nr_p = 0;
      Eigen::Vector4f center = model_coefficients;
      center[3] = 0.0f;
      const float radius = model_coefficients[3];
      for (std::size_t i = 0; i < indices.size (); ++i)
      {
        const pcl::index_t index = indices[i];
        Eigen::Vector4f dir;
        const double weighted_euclid = computeNormalSphereWeightedEuclidStandard (
            input[index], center, radius, normal_distance_weight, dir);
        if (weighted_euclid > threshold)
          continue;
        const double distance = computeNormalSphereFinalDistanceStandard (
            normals[index], dir, weighted_euclid, normal_distance_weight);
        if (distance < threshold)
          ++nr_p;
      }
      return (nr_p);
    }

    template <typename PointT, typename PointNT>
    inline void
    getDistancesToModelStandardNormalSphere (const pcl::PointCloud<PointT>& input,
                                             const pcl::PointCloud<PointNT>& normals,
                                             const pcl::Indices& indices,
                                             const Eigen::VectorXf& model_coefficients,
                                             const double normal_distance_weight,
                                             std::vector<double>& distances)
    {
      distances.resize (indices.size ());
      Eigen::Vector4f center = model_coefficients;
      center[3] = 0.0f;
      const float radius = model_coefficients[3];
      for (std::size_t i = 0; i < indices.size (); ++i)
      {
        const pcl::index_t index = indices[i];
        Eigen::Vector4f dir;
        const double weighted_euclid = computeNormalSphereWeightedEuclidStandard (
            input[index], center, radius, normal_distance_weight, dir);
        distances[i] = computeNormalSphereFinalDistanceStandard (
            normals[index], dir, weighted_euclid, normal_distance_weight);
      }
    }

#if defined (__RVV10__)
    template <typename PointT, typename PointNT>
    struct RVVNormalSphereLayoutGate
      : std::bool_constant<pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
                           std::is_same_v<PointNT, pcl::Normal> &&
                           sizeof (pcl::index_t) == sizeof (std::int32_t) &&
                           std::is_signed_v<pcl::index_t>>
    {};

    inline vfloat32m2_t
    computeNormalSphereDistanceRVV (const vfloat32m2_t& x_vec,
                                    const vfloat32m2_t& y_vec,
                                    const vfloat32m2_t& z_vec,
                                    const vfloat32m2_t& nx_vec,
                                    const vfloat32m2_t& ny_vec,
                                    const vfloat32m2_t& nz_vec,
                                    const float cx,
                                    const float cy,
                                    const float cz,
                                    const float radius,
                                    const float euclid_weight,
                                    const float normal_weight,
                                    const float threshold,
                                    const std::size_t vl)
    {
      const vfloat32m2_t dx = __riscv_vfsub_vf_f32m2 (x_vec, cx, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vf_f32m2 (y_vec, cy, vl);
      const vfloat32m2_t dz = __riscv_vfsub_vf_f32m2 (z_vec, cz, vl);
      const vfloat32m2_t dir_sqr =
          __riscv_vfmacc_vv_f32m2 (
              __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl),
                                       dy,
                                       dy,
                                       vl),
              dz,
              dz,
              vl);
      const vfloat32m2_t dir_norm = __riscv_vfsqrt_v_f32m2 (dir_sqr, vl);

      const vfloat32m2_t normal_sqr =
          __riscv_vfmacc_vv_f32m2 (
              __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (nx_vec, nx_vec, vl),
                                       ny_vec,
                                       ny_vec,
                                       vl),
              nz_vec,
              nz_vec,
              vl);
      const vfloat32m2_t normal_norm = __riscv_vfsqrt_v_f32m2 (normal_sqr, vl);

      const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
      const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
      const vfloat32m2_t tiny = __riscv_vfmv_v_f_f32m2 (1.0e-20f, vl);
      const vbool16_t dir_nonzero = __riscv_vmfgt_vf_f32m2_b16 (dir_sqr, 0.0f, vl);
      const vbool16_t normal_nonzero = __riscv_vmfgt_vf_f32m2_b16 (normal_sqr, 0.0f, vl);
      const vfloat32m2_t inv_dir =
          __riscv_vmerge_vvm_f32m2 (
              zero,
              __riscv_vfdiv_vv_f32m2 (
                  one, __riscv_vfmax_vv_f32m2 (dir_norm, tiny, vl), vl),
              dir_nonzero,
              vl);
      const vfloat32m2_t inv_normal =
          __riscv_vmerge_vvm_f32m2 (
              zero,
              __riscv_vfdiv_vv_f32m2 (
                  one, __riscv_vfmax_vv_f32m2 (normal_norm, tiny, vl), vl),
              normal_nonzero,
              vl);

      const vfloat32m2_t dir_x = __riscv_vfmul_vv_f32m2 (dx, inv_dir, vl);
      const vfloat32m2_t dir_y = __riscv_vfmul_vv_f32m2 (dy, inv_dir, vl);
      const vfloat32m2_t dir_z = __riscv_vfmul_vv_f32m2 (dz, inv_dir, vl);
      const vfloat32m2_t normal_x = __riscv_vfmul_vv_f32m2 (nx_vec, inv_normal, vl);
      const vfloat32m2_t normal_y = __riscv_vfmul_vv_f32m2 (ny_vec, inv_normal, vl);
      const vfloat32m2_t normal_z = __riscv_vfmul_vv_f32m2 (nz_vec, inv_normal, vl);

      vfloat32m2_t weighted_euclid = __riscv_vfsub_vf_f32m2 (dir_norm, radius, vl);
      weighted_euclid = __riscv_vfsgnjx_vv_f32m2 (weighted_euclid, weighted_euclid, vl);
      weighted_euclid = __riscv_vfmul_vf_f32m2 (weighted_euclid, euclid_weight, vl);

      const vfloat32m2_t d_normal =
          pcl::getAcuteAngle3DRVV_f32m2 (
              normal_x, normal_y, normal_z, dir_x, dir_y, dir_z, vl);
      vfloat32m2_t distance =
          __riscv_vfmacc_vf_f32m2 (weighted_euclid, normal_weight, d_normal, vl);
      distance = __riscv_vfsgnjx_vv_f32m2 (distance, distance, vl);
      const vbool16_t euclid_mask =
          __riscv_vmfle_vf_f32m2_b16 (weighted_euclid, threshold, vl);
      distance = __riscv_vmerge_vvm_f32m2 (
          __riscv_vfmv_v_f_f32m2 (threshold, vl), distance, euclid_mask, vl);
      return distance;
    }

    template <typename PointT, typename PointNT>
    inline bool
    canUseRVVNormalSphere (const pcl::PointCloud<PointT>& input,
                           const pcl::PointCloud<PointNT>& normals,
                           const pcl::Indices& indices)
    {
      if constexpr (!RVVNormalSphereLayoutGate<PointT, PointNT>::value)
      {
        return false;
      }
      else
      {
        return indices.size () >= 16 &&
               input.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () &&
               normals.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> () &&
               normals.size () >= input.size ();
      }
    }

    template <typename PointT, typename PointNT>
    inline void
    loadIndexedNormalSpherePointAndNormal (const pcl::PointCloud<PointT>& input,
                                           const pcl::PointCloud<PointNT>& normals,
                                           const pcl::Indices& indices,
                                           const std::size_t begin,
                                           const std::size_t vl,
                                           vuint32m2_t& idx,
                                           vfloat32m2_t& x_vec,
                                           vfloat32m2_t& y_vec,
                                           vfloat32m2_t& z_vec,
                                           vfloat32m2_t& nx_vec,
                                           vfloat32m2_t& ny_vec,
                                           vfloat32m2_t& nz_vec)
    {
      const pcl::index_t* const indices_ptr = indices.data ();
      const std::uint8_t* const points_base =
          reinterpret_cast<const std::uint8_t*> (input.points.data ());
      const std::uint8_t* const normals_base =
          reinterpret_cast<const std::uint8_t*> (normals.points.data ());
      using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;

      idx = __riscv_vle32_v_u32m2 (
          reinterpret_cast<const std::uint32_t*> (indices_ptr + begin), vl);
      const vuint32m2_t point_offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
      const vuint32m2_t normal_offsets = pcl::rvv_load::byte_offsets_u32m2<PointNT> (idx, vl);
      pcl::rvv_load::indexed_load3_f32m2<
          PointT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
          points_base, point_offsets, vl, x_vec, y_vec, z_vec);
      nx_vec = pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_x> (
          normals_base, normal_offsets, vl);
      ny_vec = pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_y> (
          normals_base, normal_offsets, vl);
      nz_vec = pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_z> (
          normals_base, normal_offsets, vl);
    }

    template <typename PointT, typename PointNT>
    PCL_RVV_NORMAL_SPHERE_NOINLINE bool
    countWithinDistanceRVVNormalSphere (const pcl::PointCloud<PointT>& input,
                                        const pcl::PointCloud<PointNT>& normals,
                                        const pcl::Indices& indices,
                                        const Eigen::VectorXf& model_coefficients,
                                        const double threshold,
                                        const double normal_distance_weight,
                                        std::size_t& nr_p)
    {
      if constexpr (!RVVNormalSphereLayoutGate<PointT, PointNT>::value)
      {
        return false;
      }
      else if (!canUseRVVNormalSphere (input, normals, indices))
        return false;

      const float cx = model_coefficients[0];
      const float cy = model_coefficients[1];
      const float cz = model_coefficients[2];
      const float radius = model_coefficients[3];
      const float threshold_f = static_cast<float> (threshold);
      const float normal_weight = static_cast<float> (normal_distance_weight);
      const float euclid_weight = 1.0f - normal_weight;

      nr_p = 0;
      for (std::size_t i = 0; i < indices.size (); )
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (indices.size () - i);
        vuint32m2_t idx;
        vfloat32m2_t x_vec, y_vec, z_vec, nx_vec, ny_vec, nz_vec;
        loadIndexedNormalSpherePointAndNormal (
            input, normals, indices, i, vl, idx, x_vec, y_vec, z_vec, nx_vec, ny_vec, nz_vec);
        const vfloat32m2_t distance =
            computeNormalSphereDistanceRVV (x_vec, y_vec, z_vec,
                                            nx_vec, ny_vec, nz_vec,
                                            cx, cy, cz, radius,
                                            euclid_weight, normal_weight, threshold_f, vl);
        const vbool16_t inlier_mask =
            __riscv_vmflt_vf_f32m2_b16 (distance, threshold_f, vl);
        nr_p += __riscv_vcpop_m_b16 (inlier_mask, vl);
        i += vl;
      }
      return true;
    }

    template <typename PointT, typename PointNT>
    PCL_RVV_NORMAL_SPHERE_NOINLINE bool
    selectWithinDistanceRVVNormalSphere (const pcl::PointCloud<PointT>& input,
                                         const pcl::PointCloud<PointNT>& normals,
                                         const pcl::Indices& indices,
                                         const Eigen::VectorXf& model_coefficients,
                                         const double threshold,
                                         const double normal_distance_weight,
                                         pcl::Indices& inliers,
                                         std::vector<double>& error_sqr_dists)
    {
      if constexpr (!RVVNormalSphereLayoutGate<PointT, PointNT>::value)
      {
        return false;
      }
      else if (!canUseRVVNormalSphere (input, normals, indices))
        return false;

      const std::size_t total_n = indices.size ();
      inliers.resize (total_n);
      error_sqr_dists.resize (total_n);

      const float cx = model_coefficients[0];
      const float cy = model_coefficients[1];
      const float cz = model_coefficients[2];
      const float radius = model_coefficients[3];
      const float threshold_f = static_cast<float> (threshold);
      const float normal_weight = static_cast<float> (normal_distance_weight);
      const float euclid_weight = 1.0f - normal_weight;

      std::size_t nr_p = 0;
      for (std::size_t i = 0; i < total_n; )
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
        vuint32m2_t idx;
        vfloat32m2_t x_vec, y_vec, z_vec, nx_vec, ny_vec, nz_vec;
        loadIndexedNormalSpherePointAndNormal (
            input, normals, indices, i, vl, idx, x_vec, y_vec, z_vec, nx_vec, ny_vec, nz_vec);
        const vfloat32m2_t distance =
            computeNormalSphereDistanceRVV (x_vec, y_vec, z_vec,
                                            nx_vec, ny_vec, nz_vec,
                                            cx, cy, cz, radius,
                                            euclid_weight, normal_weight, threshold_f, vl);
        const vbool16_t inlier_mask =
            __riscv_vmflt_vf_f32m2_b16 (distance, threshold_f, vl);
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

          const vfloat32m2_t compressed_distance =
              __riscv_vcompress_vm_f32m2 (distance, inlier_mask, vl);
          const vfloat64m4_t distance64 =
              __riscv_vfwcvt_f_f_v_f64m4 (compressed_distance, active_count);
          __riscv_vse64_v_f64m4 (error_sqr_dists.data () + nr_p,
                                 distance64,
                                 active_count);
          nr_p += active_count;
        }

        i += vl;
      }

      inliers.resize (nr_p);
      error_sqr_dists.resize (nr_p);
      return true;
    }

    template <typename PointT, typename PointNT>
    PCL_RVV_NORMAL_SPHERE_NOINLINE bool
    getDistancesToModelRVVNormalSphere (const pcl::PointCloud<PointT>& input,
                                        const pcl::PointCloud<PointNT>& normals,
                                        const pcl::Indices& indices,
                                        const Eigen::VectorXf& model_coefficients,
                                        const double normal_distance_weight,
                                        std::vector<double>& distances)
    {
      if constexpr (!RVVNormalSphereLayoutGate<PointT, PointNT>::value)
      {
        return false;
      }
      else if (!canUseRVVNormalSphere (input, normals, indices))
        return false;

      distances.resize (indices.size ());
      const float cx = model_coefficients[0];
      const float cy = model_coefficients[1];
      const float cz = model_coefficients[2];
      const float radius = model_coefficients[3];
      const float normal_weight = static_cast<float> (normal_distance_weight);
      const float euclid_weight = 1.0f - normal_weight;

      for (std::size_t i = 0; i < indices.size (); )
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (indices.size () - i);
        vuint32m2_t idx;
        vfloat32m2_t x_vec, y_vec, z_vec, nx_vec, ny_vec, nz_vec;
        loadIndexedNormalSpherePointAndNormal (
            input, normals, indices, i, vl, idx, x_vec, y_vec, z_vec, nx_vec, ny_vec, nz_vec);
        const vfloat32m2_t distance =
            computeNormalSphereDistanceRVV (x_vec, y_vec, z_vec,
                                            nx_vec, ny_vec, nz_vec,
                                            cx, cy, cz, radius,
                                            euclid_weight,
                                            normal_weight,
                                            std::numeric_limits<float>::infinity (),
                                            vl);
        const vfloat64m4_t distance64 = __riscv_vfwcvt_f_f_v_f64m4 (distance, vl);
        __riscv_vse64_v_f64m4 (distances.data () + i, distance64, vl);
        i += vl;
      }
      return true;
    }
#endif
  } // namespace detail
} // namespace pcl

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelNormalSphere<PointT, PointNT>::selectWithinDistance (
      const Eigen::VectorXf &model_coefficients, const double threshold, Indices &inliers)
{
  if (!normals_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelNormalSphere::selectWithinDistance] No input dataset containing normals was given! Use setInputNormals\n");
    inliers.clear ();
    return;
  }

  // Check if the model is valid given the user constraints
  if (!isModelValid (model_coefficients))
  {
    inliers.clear ();
    return;
  }

#if defined (__RVV10__)
  if (pcl::detail::selectWithinDistanceRVVNormalSphere<PointT, PointNT> (
          *input_,
          *normals_,
          *indices_,
          model_coefficients,
          threshold,
          normal_distance_weight_,
          inliers,
          error_sqr_dists_))
  {
    return;
  }
#endif

  pcl::detail::selectWithinDistanceStandardNormalSphere<PointT, PointNT> (
      *input_,
      *normals_,
      *indices_,
      model_coefficients,
      threshold,
      normal_distance_weight_,
      inliers,
      error_sqr_dists_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> std::size_t
pcl::SampleConsensusModelNormalSphere<PointT, PointNT>::countWithinDistance (
      const Eigen::VectorXf &model_coefficients,  const double threshold) const
{
  if (!normals_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelNormalSphere::getDistancesToModel] No input dataset containing normals was given! Use setInputNormals\n");
    return (0);
  }

  // Check if the model is valid given the user constraints
  if (!isModelValid (model_coefficients))
    return(0);


#if defined (__RVV10__)
  std::size_t nr_p = 0;
  if (pcl::detail::countWithinDistanceRVVNormalSphere<PointT, PointNT> (
          *input_,
          *normals_,
          *indices_,
          model_coefficients,
          threshold,
          normal_distance_weight_,
          nr_p))
  {
    return (nr_p);
  }
#endif

  return (pcl::detail::countWithinDistanceStandardNormalSphere<PointT, PointNT> (
      *input_,
      *normals_,
      *indices_,
      model_coefficients,
      threshold,
      normal_distance_weight_));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelNormalSphere<PointT, PointNT>::getDistancesToModel (
      const Eigen::VectorXf &model_coefficients, std::vector<double> &distances) const
{
  if (!normals_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelNormalSphere::getDistancesToModel] No input dataset containing normals was given! Use setInputNormals\n");
    return;
  }

  // Check if the model is valid given the user constraints
  if (!isModelValid (model_coefficients))
  {
    distances.clear ();
    return;
  }

#if defined (__RVV10__)
  if (pcl::detail::getDistancesToModelRVVNormalSphere<PointT, PointNT> (
          *input_,
          *normals_,
          *indices_,
          model_coefficients,
          normal_distance_weight_,
          distances))
  {
    return;
  }
#endif

  pcl::detail::getDistancesToModelStandardNormalSphere<PointT, PointNT> (
      *input_,
      *normals_,
      *indices_,
      model_coefficients,
      normal_distance_weight_,
      distances);
}

#define PCL_INSTANTIATE_SampleConsensusModelNormalSphere(PointT, PointNT) template class PCL_EXPORTS pcl::SampleConsensusModelNormalSphere<PointT, PointNT>;

#undef PCL_RVV_NORMAL_SPHERE_NOINLINE

#endif    // PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_NORMAL_SPHERE_H_
