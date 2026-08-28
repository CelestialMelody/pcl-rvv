/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2009-2010, Willow Garage, Inc.
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

#ifndef PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_CYLINDER_H_
#define PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_CYLINDER_H_

#include <pcl/sample_consensus/sac_model_cylinder.h>
#include <pcl/common/common.h> // for getAngle3D
#include <pcl/common/concatenate.h>
#include <pcl/rvv_point_load.h>

#include <cstdint>
#include <type_traits>

#if defined (__GNUC__)
#define PCL_RVV_CYLINDER_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_CYLINDER_NOINLINE
#endif

#if defined (__RVV10__)
namespace pcl
{
  namespace detail
  {
    template <typename PointNT,
              bool HasRequiredFields = pcl::rvv::RVVNormalFloatLayout<PointNT>::value>
    struct CylinderRVVNormalAoSLayout : std::false_type {};

    template <typename PointNT>
    struct CylinderRVVNormalAoSLayout<PointNT, true>
    {
      using Pod = typename pcl::traits::POD<PointNT>::type;

      static constexpr std::size_t kNormalX = pcl::rvv::RVVNormalFloatLayout<PointNT>::kNormalX;
      static constexpr std::size_t kNormalY = pcl::rvv::RVVNormalFloatLayout<PointNT>::kNormalY;
      static constexpr std::size_t kNormalZ = pcl::rvv::RVVNormalFloatLayout<PointNT>::kNormalZ;

      static constexpr bool value =
          std::is_standard_layout_v<Pod> &&
          sizeof(PointNT) == sizeof(Pod) &&
          sizeof(PointNT) % alignof(float) == 0 &&
          kNormalX % alignof(float) == 0 &&
          kNormalY % alignof(float) == 0 &&
          kNormalZ % alignof(float) == 0;
    };

    template <typename PointT, typename PointNT>
    inline constexpr bool kCylinderRVVLayoutCompatible =
        pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
        CylinderRVVNormalAoSLayout<PointNT>::value;
  } // namespace detail
} // namespace pcl
#endif

namespace pcl
{
  namespace detail
  {
    template <typename PointT, typename PointNT>
    struct CylinderDistanceTerms
    {
      double weighted_euclid = 0.0;
      double final_distance = 0.0;
    };

    template <typename PointT>
    inline double
    computeCylinderWeightedEuclidStandard (const PointT& point,
                                           const Eigen::Vector3f& line_pt,
                                           const Eigen::Vector3f& line_dir,
                                           const float radius,
                                           const double normal_distance_weight,
                                           Eigen::Vector3f& dir)
    {
      const Eigen::Vector3f pt (point.x, point.y, point.z);
      const Eigen::Vector3f diff = pt - line_pt;
      dir = diff - diff.dot (line_dir) * line_dir;
      return ((1.0 - normal_distance_weight) * std::abs (dir.norm () - radius));
    }

    template <typename PointNT>
    inline double
    computeCylinderFinalDistanceStandard (const PointNT& normal,
                                          const Eigen::Vector3f& dir,
                                          const double weighted_euclid,
                                          const double normal_distance_weight)
    {
      const Eigen::Vector3f n (normal.normal_x, normal.normal_y, normal.normal_z);
      double d_normal = std::abs (pcl::getAngle3D (n, dir));
      d_normal = (std::min) (d_normal, M_PI - d_normal);
      return (std::abs (normal_distance_weight * d_normal + weighted_euclid));
    }

    template <typename PointT, typename PointNT>
    inline std::size_t
    countWithinDistanceStandardCylinder (const pcl::PointCloud<PointT>& input,
                                         const pcl::PointCloud<PointNT>& normals,
                                         const pcl::Indices& indices,
                                         const Eigen::VectorXf& model_coefficients,
                                         const double threshold,
                                         const double normal_distance_weight)
    {
      std::size_t nr_p = 0;

      Eigen::Vector3f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2]);
      Eigen::Vector3f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5]);
      line_dir.normalize ();
      const float radius = model_coefficients[6];

      for (std::size_t i = 0; i < indices.size (); ++i)
      {
        const pcl::index_t index = indices[i];
        Eigen::Vector3f dir;
        const double weighted_euclid = computeCylinderWeightedEuclidStandard (
            input[index], line_pt, line_dir, radius, normal_distance_weight, dir);
        if (weighted_euclid > threshold)
          continue;
        const double distance = computeCylinderFinalDistanceStandard (
            normals[index], dir, weighted_euclid, normal_distance_weight);
        if (distance < threshold)
          nr_p++;
      }
      return (nr_p);
    }

    template <typename PointT, typename PointNT>
    inline void
    selectWithinDistanceStandardCylinder (const pcl::PointCloud<PointT>& input,
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

      Eigen::Vector3f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2]);
      Eigen::Vector3f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5]);
      line_dir.normalize ();
      const float radius = model_coefficients[6];

      for (std::size_t i = 0; i < indices.size (); ++i)
      {
        const pcl::index_t index = indices[i];
        Eigen::Vector3f dir;
        const double weighted_euclid = computeCylinderWeightedEuclidStandard (
            input[index], line_pt, line_dir, radius, normal_distance_weight, dir);
        if (weighted_euclid > threshold)
          continue;
        const double distance = computeCylinderFinalDistanceStandard (
            normals[index], dir, weighted_euclid, normal_distance_weight);
        if (distance < threshold)
        {
          inliers.push_back (index);
          error_sqr_dists.push_back (distance);
        }
      }
    }

    template <typename PointT, typename PointNT>
    inline void
    getDistancesToModelStandardCylinder (const pcl::PointCloud<PointT>& input,
                                         const pcl::PointCloud<PointNT>& normals,
                                         const pcl::Indices& indices,
                                         const Eigen::VectorXf& model_coefficients,
                                         const double normal_distance_weight,
                                         std::vector<double>& distances)
    {
      distances.resize (indices.size ());

      Eigen::Vector3f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2]);
      Eigen::Vector3f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5]);
      line_dir.normalize ();
      const float radius = model_coefficients[6];

      for (std::size_t i = 0; i < indices.size (); ++i)
      {
        const pcl::index_t index = indices[i];
        Eigen::Vector3f dir;
        const double weighted_euclid = computeCylinderWeightedEuclidStandard (
            input[index], line_pt, line_dir, radius, normal_distance_weight, dir);
        distances[i] = computeCylinderFinalDistanceStandard (
            normals[index], dir, weighted_euclid, normal_distance_weight);
      }
    }

#if defined (__RVV10__)
    inline void
    computeCylinderDistanceTermsRVV (const vfloat32m2_t& x_vec,
                                     const vfloat32m2_t& y_vec,
                                     const vfloat32m2_t& z_vec,
                                     const vfloat32m2_t& nx_vec,
                                     const vfloat32m2_t& ny_vec,
                                     const vfloat32m2_t& nz_vec,
                                     const float px,
                                     const float py,
                                     const float pz,
                                     const float dx_line,
                                     const float dy_line,
                                     const float dz_line,
                                     const float radius,
                                     const float euclid_weight,
                                     const float normal_weight,
                                     const float threshold,
                                     const std::size_t vl,
                                     vfloat32m2_t& distance,
                                     vbool16_t& euclid_mask)
    {
      const vfloat32m2_t diff_x = __riscv_vfsub_vf_f32m2 (x_vec, px, vl);
      const vfloat32m2_t diff_y = __riscv_vfsub_vf_f32m2 (y_vec, py, vl);
      const vfloat32m2_t diff_z = __riscv_vfsub_vf_f32m2 (z_vec, pz, vl);
      const vfloat32m2_t dot =
          __riscv_vfmacc_vf_f32m2 (
              __riscv_vfmacc_vf_f32m2 (__riscv_vfmul_vf_f32m2 (diff_x, dx_line, vl),
                                       dy_line,
                                       diff_y,
                                       vl),
              dz_line,
              diff_z,
              vl);

      const vfloat32m2_t dir_x = __riscv_vfsub_vv_f32m2 (
          diff_x, __riscv_vfmul_vf_f32m2 (dot, dx_line, vl), vl);
      const vfloat32m2_t dir_y = __riscv_vfsub_vv_f32m2 (
          diff_y, __riscv_vfmul_vf_f32m2 (dot, dy_line, vl), vl);
      const vfloat32m2_t dir_z = __riscv_vfsub_vv_f32m2 (
          diff_z, __riscv_vfmul_vf_f32m2 (dot, dz_line, vl), vl);

      const vfloat32m2_t dir_sqr =
          __riscv_vfmacc_vv_f32m2 (
              __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dir_x, dir_x, vl),
                                       dir_y,
                                       dir_y,
                                       vl),
              dir_z,
              dir_z,
              vl);
      const vfloat32m2_t dir_norm = __riscv_vfsqrt_v_f32m2 (dir_sqr, vl);
      vfloat32m2_t radial_delta = __riscv_vfsub_vf_f32m2 (dir_norm, radius, vl);
      radial_delta = __riscv_vfsgnjx_vv_f32m2 (radial_delta, radial_delta, vl);
      const vfloat32m2_t weighted_euclid =
          __riscv_vfmul_vf_f32m2 (radial_delta, euclid_weight, vl);
      euclid_mask = __riscv_vmfle_vf_f32m2_b16 (weighted_euclid, threshold, vl);

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
      const vfloat32m2_t inv_dir_norm = __riscv_vfrdiv_vf_f32m2 (dir_norm, 1.0f, vl);
      const vfloat32m2_t inv_normal_norm = __riscv_vfrdiv_vf_f32m2 (normal_norm, 1.0f, vl);

      const vfloat32m2_t dir_unit_x = __riscv_vfmul_vv_f32m2 (dir_x, inv_dir_norm, vl);
      const vfloat32m2_t dir_unit_y = __riscv_vfmul_vv_f32m2 (dir_y, inv_dir_norm, vl);
      const vfloat32m2_t dir_unit_z = __riscv_vfmul_vv_f32m2 (dir_z, inv_dir_norm, vl);
      const vfloat32m2_t normal_unit_x = __riscv_vfmul_vv_f32m2 (nx_vec, inv_normal_norm, vl);
      const vfloat32m2_t normal_unit_y = __riscv_vfmul_vv_f32m2 (ny_vec, inv_normal_norm, vl);
      const vfloat32m2_t normal_unit_z = __riscv_vfmul_vv_f32m2 (nz_vec, inv_normal_norm, vl);

      const vfloat32m2_t d_normal =
          pcl::getAcuteAngle3DRVV_f32m2 (normal_unit_x,
                                         normal_unit_y,
                                         normal_unit_z,
                                         dir_unit_x,
                                         dir_unit_y,
                                         dir_unit_z,
                                         vl);

      distance = __riscv_vfmacc_vf_f32m2 (weighted_euclid, normal_weight, d_normal, vl);
      distance = __riscv_vfsgnjx_vv_f32m2 (distance, distance, vl);
    }

    template <typename PointT, typename PointNT>
    PCL_RVV_CYLINDER_NOINLINE bool
    countWithinDistanceRVVCylinder (const pcl::PointCloud<PointT>& input,
                                    const pcl::PointCloud<PointNT>& normals,
                                    const pcl::Indices& indices,
                                    const Eigen::VectorXf& model_coefficients,
                                    const double threshold,
                                    const double normal_distance_weight,
                                    std::size_t& nr_p)
    {
      if constexpr (!pcl::detail::kCylinderRVVLayoutCompatible<PointT, PointNT> ||
                    sizeof (pcl::index_t) != sizeof (std::int32_t) ||
                    !std::is_signed_v<pcl::index_t>)
      {
        return false;
      }
      else
      {
        if (input.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () ||
            normals.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> () ||
            normals.size () < input.size ())
          return false;

        const std::size_t total_n = indices.size ();
        const float px = model_coefficients[0];
        const float py = model_coefficients[1];
        const float pz = model_coefficients[2];
        Eigen::Vector3f line_dir (model_coefficients[3],
                                  model_coefficients[4],
                                  model_coefficients[5]);
        line_dir.normalize ();
        const float dx_line = line_dir.x ();
        const float dy_line = line_dir.y ();
        const float dz_line = line_dir.z ();
        const float radius = model_coefficients[6];
        const float threshold_f = static_cast<float> (threshold);
        const float normal_weight = static_cast<float> (normal_distance_weight);
        const float euclid_weight = 1.0f - normal_weight;

        const std::uint8_t* const points_base =
            reinterpret_cast<const std::uint8_t*> (input.points.data ());
        const std::uint8_t* const normals_base =
            reinterpret_cast<const std::uint8_t*> (normals.points.data ());
        const pcl::index_t* const indices_ptr = indices.data ();
        using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
        using NormalLayout = pcl::detail::CylinderRVVNormalAoSLayout<PointNT>;

        nr_p = 0;
        for (std::size_t i = 0; i < total_n; )
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
          const vuint32m2_t idx =
              __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
          const vuint32m2_t point_offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
          const vuint32m2_t normal_offsets = pcl::rvv_load::byte_offsets_u32m2<PointNT> (idx, vl);

          vfloat32m2_t x_vec;
          vfloat32m2_t y_vec;
          vfloat32m2_t z_vec;
          pcl::rvv_load::indexed_load3_f32m2<PointT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
              points_base, point_offsets, vl, x_vec, y_vec, z_vec);

          vfloat32m2_t nx_vec;
          vfloat32m2_t ny_vec;
          vfloat32m2_t nz_vec;
          pcl::rvv_load::indexed_load3_fields_f32m2<
              PointNT, NormalLayout::kNormalX, NormalLayout::kNormalY, NormalLayout::kNormalZ> (
              normals_base, normal_offsets, vl, nx_vec, ny_vec, nz_vec);

          vfloat32m2_t distance;
          vbool16_t euclid_mask;
          computeCylinderDistanceTermsRVV (x_vec, y_vec, z_vec,
                                           nx_vec, ny_vec, nz_vec,
                                           px, py, pz,
                                           dx_line, dy_line, dz_line,
                                           radius, euclid_weight, normal_weight,
                                           threshold_f, vl, distance, euclid_mask);
          const vbool16_t distance_mask = __riscv_vmflt_vf_f32m2_b16 (distance, threshold_f, vl);
          const vbool16_t inlier_mask = __riscv_vmand_mm_b16 (euclid_mask, distance_mask, vl);
          nr_p += __riscv_vcpop_m_b16 (inlier_mask, vl);
          i += vl;
        }
        return true;
      }
    }

    template <typename PointT, typename PointNT>
    PCL_RVV_CYLINDER_NOINLINE bool
    selectWithinDistanceRVVCylinder (const pcl::PointCloud<PointT>& input,
                                     const pcl::PointCloud<PointNT>& normals,
                                     const pcl::Indices& indices,
                                     const Eigen::VectorXf& model_coefficients,
                                     const double threshold,
                                     const double normal_distance_weight,
                                     pcl::Indices& inliers,
                                     std::vector<double>& error_sqr_dists)
    {
      if constexpr (!pcl::detail::kCylinderRVVLayoutCompatible<PointT, PointNT> ||
                    sizeof (pcl::index_t) != sizeof (std::int32_t) ||
                    !std::is_signed_v<pcl::index_t>)
      {
        return false;
      }
      else
      {
        if (input.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () ||
            normals.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> () ||
            normals.size () < input.size ())
          return false;

        const std::size_t total_n = indices.size ();
        inliers.resize (total_n);
        error_sqr_dists.resize (total_n);

        const float px = model_coefficients[0];
        const float py = model_coefficients[1];
        const float pz = model_coefficients[2];
        Eigen::Vector3f line_dir (model_coefficients[3],
                                  model_coefficients[4],
                                  model_coefficients[5]);
        line_dir.normalize ();
        const float dx_line = line_dir.x ();
        const float dy_line = line_dir.y ();
        const float dz_line = line_dir.z ();
        const float radius = model_coefficients[6];
        const float threshold_f = static_cast<float> (threshold);
        const float normal_weight = static_cast<float> (normal_distance_weight);
        const float euclid_weight = 1.0f - normal_weight;

        const std::uint8_t* const points_base =
            reinterpret_cast<const std::uint8_t*> (input.points.data ());
        const std::uint8_t* const normals_base =
            reinterpret_cast<const std::uint8_t*> (normals.points.data ());
        const pcl::index_t* const indices_ptr = indices.data ();
        using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
        using NormalLayout = pcl::detail::CylinderRVVNormalAoSLayout<PointNT>;

        std::size_t nr_p = 0;
        for (std::size_t i = 0; i < total_n; )
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
          const vuint32m2_t idx =
              __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
          const vuint32m2_t point_offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
          const vuint32m2_t normal_offsets = pcl::rvv_load::byte_offsets_u32m2<PointNT> (idx, vl);

          vfloat32m2_t x_vec;
          vfloat32m2_t y_vec;
          vfloat32m2_t z_vec;
          pcl::rvv_load::indexed_load3_f32m2<PointT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
              points_base, point_offsets, vl, x_vec, y_vec, z_vec);

          vfloat32m2_t nx_vec;
          vfloat32m2_t ny_vec;
          vfloat32m2_t nz_vec;
          pcl::rvv_load::indexed_load3_fields_f32m2<
              PointNT, NormalLayout::kNormalX, NormalLayout::kNormalY, NormalLayout::kNormalZ> (
              normals_base, normal_offsets, vl, nx_vec, ny_vec, nz_vec);

          vfloat32m2_t distance;
          vbool16_t euclid_mask;
          computeCylinderDistanceTermsRVV (x_vec, y_vec, z_vec,
                                           nx_vec, ny_vec, nz_vec,
                                           px, py, pz,
                                           dx_line, dy_line, dz_line,
                                           radius, euclid_weight, normal_weight,
                                           threshold_f, vl, distance, euclid_mask);
          const vbool16_t distance_mask = __riscv_vmflt_vf_f32m2_b16 (distance, threshold_f, vl);
          const vbool16_t inlier_mask = __riscv_vmand_mm_b16 (euclid_mask, distance_mask, vl);
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
    }

    template <typename PointT, typename PointNT>
    PCL_RVV_CYLINDER_NOINLINE bool
    getDistancesToModelRVVCylinder (const pcl::PointCloud<PointT>& input,
                                    const pcl::PointCloud<PointNT>& normals,
                                    const pcl::Indices& indices,
                                    const Eigen::VectorXf& model_coefficients,
                                    const double normal_distance_weight,
                                    std::vector<double>& distances)
    {
      if constexpr (!pcl::detail::kCylinderRVVLayoutCompatible<PointT, PointNT> ||
                    sizeof (pcl::index_t) != sizeof (std::int32_t) ||
                    !std::is_signed_v<pcl::index_t>)
      {
        return false;
      }
      else
      {
        if (input.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () ||
            normals.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> () ||
            normals.size () < input.size ())
          return false;

        const std::size_t total_n = indices.size ();
        distances.resize (total_n);

        const float px = model_coefficients[0];
        const float py = model_coefficients[1];
        const float pz = model_coefficients[2];
        Eigen::Vector3f line_dir (model_coefficients[3],
                                  model_coefficients[4],
                                  model_coefficients[5]);
        line_dir.normalize ();
        const float dx_line = line_dir.x ();
        const float dy_line = line_dir.y ();
        const float dz_line = line_dir.z ();
        const float radius = model_coefficients[6];
        const float normal_weight = static_cast<float> (normal_distance_weight);
        const float euclid_weight = 1.0f - normal_weight;

        const std::uint8_t* const points_base =
            reinterpret_cast<const std::uint8_t*> (input.points.data ());
        const std::uint8_t* const normals_base =
            reinterpret_cast<const std::uint8_t*> (normals.points.data ());
        const pcl::index_t* const indices_ptr = indices.data ();
        using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
        using NormalLayout = pcl::detail::CylinderRVVNormalAoSLayout<PointNT>;

        for (std::size_t i = 0; i < total_n; )
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
          const vuint32m2_t idx =
              __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
          const vuint32m2_t point_offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
          const vuint32m2_t normal_offsets = pcl::rvv_load::byte_offsets_u32m2<PointNT> (idx, vl);

          vfloat32m2_t x_vec;
          vfloat32m2_t y_vec;
          vfloat32m2_t z_vec;
          pcl::rvv_load::indexed_load3_f32m2<PointT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
              points_base, point_offsets, vl, x_vec, y_vec, z_vec);

          vfloat32m2_t nx_vec;
          vfloat32m2_t ny_vec;
          vfloat32m2_t nz_vec;
          pcl::rvv_load::indexed_load3_fields_f32m2<
              PointNT, NormalLayout::kNormalX, NormalLayout::kNormalY, NormalLayout::kNormalZ> (
              normals_base, normal_offsets, vl, nx_vec, ny_vec, nz_vec);

          vfloat32m2_t distance;
          vbool16_t euclid_mask;
          computeCylinderDistanceTermsRVV (x_vec, y_vec, z_vec,
                                           nx_vec, ny_vec, nz_vec,
                                           px, py, pz,
                                           dx_line, dy_line, dz_line,
                                           radius, euclid_weight, normal_weight,
                                           std::numeric_limits<float>::infinity (),
                                           vl, distance, euclid_mask);
          (void) euclid_mask;

          const vfloat64m4_t distance64 = __riscv_vfwcvt_f_f_v_f64m4 (distance, vl);
          __riscv_vse64_v_f64m4 (distances.data () + i, distance64, vl);
          i += vl;
        }
        return true;
      }
    }
#endif
  } // namespace detail
} // namespace pcl

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> bool
pcl::SampleConsensusModelCylinder<PointT, PointNT>::isSampleGood (const Indices &samples) const
{
  if (samples.size () != sample_size_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::isSampleGood] Wrong number of samples (is %lu, should be %lu)!\n", samples.size (), sample_size_);
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
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::isSampleGood] The two sample points are (almost) identical!\n");
    return (false);
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> bool
pcl::SampleConsensusModelCylinder<PointT, PointNT>::computeModelCoefficients (
      const Indices &samples, Eigen::VectorXf &model_coefficients) const
{
  // Make sure that the samples are valid
  if (!isSampleGood (samples))
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::computeModelCoefficients] Invalid set of samples given!\n");
    return (false);
  }

  if (!normals_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::computeModelCoefficients] No input dataset containing normals was given! Use setInputNormals\n");
    return (false);
  }

  Eigen::Vector4f p1 ((*input_)[samples[0]].x, (*input_)[samples[0]].y, (*input_)[samples[0]].z, 0.0f);
  Eigen::Vector4f p2 ((*input_)[samples[1]].x, (*input_)[samples[1]].y, (*input_)[samples[1]].z, 0.0f);

  Eigen::Vector4f n1 ((*normals_)[samples[0]].normal[0], (*normals_)[samples[0]].normal[1], (*normals_)[samples[0]].normal[2], 0.0f);
  Eigen::Vector4f n2 ((*normals_)[samples[1]].normal[0], (*normals_)[samples[1]].normal[1], (*normals_)[samples[1]].normal[2], 0.0f);
  Eigen::Vector4f w = n1 + p1 - p2;
  Eigen::Vector4f line_dir = n1.cross3 (n2);

  float b = n1.dot (n2);
  float c = n2.dot (n2);
  float d = n1.dot (w);
  float e = n2.dot (w);
  float denominator = line_dir.squaredNorm ();
  float sc;
  // Compute the line parameters of the two closest points
  if (denominator < 1e-8)          // The lines are almost parallel
  {
    sc = 0.0f;
  }
  else
  {
    sc = (b*e - c*d) / denominator;
  }

  // point_on_axis, axis_direction
  Eigen::Vector4f line_pt  = p1 + n1 + sc * n1;
  line_dir.normalize ();

  model_coefficients.resize (model_size_);
  // model_coefficients.template head<3> ()    = line_pt.template head<3> ();
  model_coefficients[0] = line_pt[0];
  model_coefficients[1] = line_pt[1];
  model_coefficients[2] = line_pt[2];
  // model_coefficients.template segment<3> (3) = line_dir.template head<3> ();
  model_coefficients[3] = line_dir[0];
  model_coefficients[4] = line_dir[1];
  model_coefficients[5] = line_dir[2];
  // cylinder radius
  model_coefficients[6] = static_cast<float> (
      0.5 * (sqrt (pcl::sqrPointToLineDistance (p1, line_pt, line_dir)) +
             sqrt (pcl::sqrPointToLineDistance (p2, line_pt, line_dir))));

  if (model_coefficients[6] > radius_max_ || model_coefficients[6] < radius_min_)
    return (false);

  PCL_DEBUG ("[pcl::SampleConsensusModelCylinder::computeModelCoefficients] Model is (%g,%g,%g,%g,%g,%g,%g).\n",
             model_coefficients[0], model_coefficients[1], model_coefficients[2], model_coefficients[3],
             model_coefficients[4], model_coefficients[5], model_coefficients[6]);
  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelCylinder<PointT, PointNT>::getDistancesToModel (
      const Eigen::VectorXf &model_coefficients, std::vector<double> &distances) const
{
  // Check if the model is valid given the user constraints
  if (!isModelValid (model_coefficients))
  {
    distances.clear ();
    return;
  }

#if defined (__RVV10__)
  if (pcl::detail::getDistancesToModelRVVCylinder<PointT, PointNT> (
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

  pcl::detail::getDistancesToModelStandardCylinder<PointT, PointNT> (
      *input_,
      *normals_,
      *indices_,
      model_coefficients,
      normal_distance_weight_,
      distances);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelCylinder<PointT, PointNT>::selectWithinDistance (
      const Eigen::VectorXf &model_coefficients, const double threshold, Indices &inliers)
{
  // Check if the model is valid given the user constraints
  if (!isModelValid (model_coefficients))
  {
    inliers.clear ();
    return;
  }

#if defined (__RVV10__)
  if (pcl::detail::selectWithinDistanceRVVCylinder<PointT, PointNT> (
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

  pcl::detail::selectWithinDistanceStandardCylinder<PointT, PointNT> (
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
pcl::SampleConsensusModelCylinder<PointT, PointNT>::countWithinDistance (
      const Eigen::VectorXf &model_coefficients, const double threshold) const
{
  // Check if the model is valid given the user constraints
  if (!isModelValid (model_coefficients))
    return (0);

#if defined (__RVV10__)
  std::size_t nr_p = 0;
  if (pcl::detail::countWithinDistanceRVVCylinder<PointT, PointNT> (
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

  return (pcl::detail::countWithinDistanceStandardCylinder<PointT, PointNT> (
      *input_,
      *normals_,
      *indices_,
      model_coefficients,
      threshold,
      normal_distance_weight_));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelCylinder<PointT, PointNT>::optimizeModelCoefficients (
      const Indices &inliers, const Eigen::VectorXf &model_coefficients, Eigen::VectorXf &optimized_coefficients) const
{
  optimized_coefficients = model_coefficients;

  // Needs a set of valid model coefficients
  if (!isModelValid (model_coefficients))
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::optimizeModelCoefficients] Given model is invalid!\n");
    return;
  }

  // Need more than the minimum sample size to make a difference
  if (inliers.size () <= sample_size_)
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder:optimizeModelCoefficients] Not enough inliers found to optimize model coefficients (%lu)! Returning the same coefficients.\n", inliers.size ());
    return;
  }

  Eigen::ArrayXf pts_x(inliers.size());
  Eigen::ArrayXf pts_y(inliers.size());
  Eigen::ArrayXf pts_z(inliers.size());
  std::size_t pos = 0;
  for(const auto& index : inliers) {
    pts_x[pos] = (*input_)[index].x;
    pts_y[pos] = (*input_)[index].y;
    pts_z[pos] = (*input_)[index].z;
    ++pos;
  }
  pcl::internal::optimizeModelCoefficientsCylinder(optimized_coefficients, pts_x, pts_y, pts_z);
  
  PCL_DEBUG ("[pcl::SampleConsensusModelCylinder::optimizeModelCoefficients] Initial solution: %g %g %g %g %g %g %g \nFinal solution: %g %g %g %g %g %g %g\n",
             model_coefficients[0], model_coefficients[1], model_coefficients[2], model_coefficients[3],
             model_coefficients[4], model_coefficients[5], model_coefficients[6], optimized_coefficients[0], optimized_coefficients[1], optimized_coefficients[2], optimized_coefficients[3], optimized_coefficients[4], optimized_coefficients[5], optimized_coefficients[6]);
    
  Eigen::Vector3f line_dir (optimized_coefficients[3], optimized_coefficients[4], optimized_coefficients[5]);
  line_dir.normalize ();
  optimized_coefficients[3] = line_dir[0];
  optimized_coefficients[4] = line_dir[1];
  optimized_coefficients[5] = line_dir[2];
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelCylinder<PointT, PointNT>::projectPoints (
      const Indices &inliers, const Eigen::VectorXf &model_coefficients, PointCloud &projected_points, bool copy_data_fields) const
{
  // Needs a valid set of model coefficients
  if (!isModelValid (model_coefficients))
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::projectPoints] Given model is invalid!\n");
    return;
  }

  projected_points.header = input_->header;
  projected_points.is_dense = input_->is_dense;

  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0.0f);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0.0f);
  float ptdotdir = line_pt.dot (line_dir);
  float dirdotdir = 1.0f / line_dir.dot (line_dir);

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

    // Iterate through the 3d points and calculate the distances from them to the cylinder
    for (const auto &inlier : inliers)
    {
      Eigen::Vector4f p ((*input_)[inlier].x,
                         (*input_)[inlier].y,
                         (*input_)[inlier].z,
                         1);

      float k = (p.dot (line_dir) - ptdotdir) * dirdotdir;

      pcl::Vector4fMap pp = projected_points[inlier].getVector4fMap ();
      pp.matrix () = line_pt + k * line_dir;

      Eigen::Vector4f dir = p - pp;
      dir[3] = 0.0f;
      dir.normalize ();

      // Calculate the projection of the point onto the cylinder
      pp += dir * model_coefficients[6];
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

    // Iterate through the 3d points and calculate the distances from them to the cylinder
    for (std::size_t i = 0; i < inliers.size (); ++i)
    {
      pcl::Vector4fMap pp = projected_points[i].getVector4fMap ();
      pcl::Vector4fMapConst p = (*input_)[inliers[i]].getVector4fMap ();

      float k = (p.dot (line_dir) - ptdotdir) * dirdotdir;
      // Calculate the projection of the point on the line
      pp.matrix () = line_pt + k * line_dir;

      Eigen::Vector4f dir = p - pp;
      dir[3] = 0.0f;
      dir.normalize ();

      // Calculate the projection of the point onto the cylinder
      pp += dir * model_coefficients[6];
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> bool
pcl::SampleConsensusModelCylinder<PointT, PointNT>::doSamplesVerifyModel (
      const std::set<index_t> &indices, const Eigen::VectorXf &model_coefficients, const double threshold) const
{
  // Needs a valid model coefficients
  if (!isModelValid (model_coefficients))
  {
    PCL_ERROR ("[pcl::SampleConsensusModelCylinder::doSamplesVerifyModel] Given model is invalid!\n");
    return (false);
  }

  for (const auto &index : indices)
  {
    // Approximate the distance from the point to the cylinder as the difference between
    // dist(point,cylinder_axis) and cylinder radius
    // @note need to revise this.
    Eigen::Vector4f pt ((*input_)[index].x, (*input_)[index].y, (*input_)[index].z, 0.0f);
    if (std::abs (pointToLineDistance (pt, model_coefficients) - model_coefficients[6]) > threshold)
      return (false);
  }

  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> double
pcl::SampleConsensusModelCylinder<PointT, PointNT>::pointToLineDistance (
      const Eigen::Vector4f &pt, const Eigen::VectorXf &model_coefficients) const
{
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0.0f);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0.0f);
  return sqrt(pcl::sqrPointToLineDistance (pt, line_pt, line_dir));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> void
pcl::SampleConsensusModelCylinder<PointT, PointNT>::projectPointToCylinder (
      const Eigen::Vector4f &pt, const Eigen::VectorXf &model_coefficients, Eigen::Vector4f &pt_proj) const
{
  Eigen::Vector4f line_pt  (model_coefficients[0], model_coefficients[1], model_coefficients[2], 0.0f);
  Eigen::Vector4f line_dir (model_coefficients[3], model_coefficients[4], model_coefficients[5], 0.0f);

  float k = (pt.dot (line_dir) - line_pt.dot (line_dir)) / line_dir.dot (line_dir);
  pt_proj = line_pt + k * line_dir;

  Eigen::Vector4f dir = pt - pt_proj;
  dir.normalize ();

  // Calculate the projection of the point onto the cylinder
  pt_proj += dir * model_coefficients[6];
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT> bool 
pcl::SampleConsensusModelCylinder<PointT, PointNT>::isModelValid (const Eigen::VectorXf &model_coefficients) const
{
  if (!SampleConsensusModel<PointT>::isModelValid (model_coefficients))
    return (false);

  // Check against template, if given
  if (eps_angle_ > 0.0)
  {
    // Obtain the cylinder direction
    const Eigen::Vector3f coeff(model_coefficients[3], model_coefficients[4], model_coefficients[5]);

    double angle_diff = std::abs (getAngle3D (axis_, coeff));
    angle_diff = (std::min) (angle_diff, M_PI - angle_diff);
    // Check whether the current cylinder model satisfies our angle threshold criterion with respect to the given axis
    if (angle_diff > eps_angle_)
    {
      PCL_DEBUG ("[pcl::SampleConsensusModelCylinder::isModelValid] Angle between cylinder direction and given axis is too large.\n");
      return (false);
    }
  }

  if (radius_min_ != -std::numeric_limits<double>::max() && model_coefficients[6] < radius_min_)
  {
    PCL_DEBUG ("[pcl::SampleConsensusModelCylinder::isModelValid] Radius is too small: should be larger than %g, but is %g.\n",
               radius_min_, model_coefficients[6]);
    return (false);
  }
  if (radius_max_ != std::numeric_limits<double>::max() && model_coefficients[6] > radius_max_)
  {
    PCL_DEBUG ("[pcl::SampleConsensusModelCylinder::isModelValid] Radius is too big: should be smaller than %g, but is %g.\n",
               radius_max_, model_coefficients[6]);
    return (false);
  }

  return (true);
}

#define PCL_INSTANTIATE_SampleConsensusModelCylinder(PointT, PointNT)	template class PCL_EXPORTS pcl::SampleConsensusModelCylinder<PointT, PointNT>;

#undef PCL_RVV_CYLINDER_NOINLINE

#endif    // PCL_SAMPLE_CONSENSUS_IMPL_SAC_MODEL_CYLINDER_H_
