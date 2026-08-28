/*
 * 本文件做什么：
 * 这里放 SampleConsensusModelCylinder 的测试专用 diagnostic（诊断）helper。
 * 它继承 production class（生产类）来复用真实 input_ / indices_ / normals_
 * 状态，但只服务 test-rvv correctness（正确性）和 bench（性能测试）。
 * 生产接入后，本 helper 保留为 historical diagnostic（历史诊断）和
 * production direct（真实生产路径）对照，不替代公开入口证据。
 */

#pragma once

#include <pcl/common/common.h>
#include <pcl/rvv_point_load.h>
#include <pcl/sample_consensus/sac_model_cylinder.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined (__GNUC__)
#define PCL_RVV_CYLINDER_DIAGNOSTIC_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_CYLINDER_DIAGNOSTIC_NOINLINE
#endif

namespace pcl_rvv_test {

#if defined (__RVV10__)
namespace detail {

template <typename PointNT, bool HasRequiredFields = pcl::rvv::RVVNormalFloatLayout<PointNT>::value>
struct CylinderRVVNormalAoSLayout : std::false_type {};

template <typename PointNT>
struct CylinderRVVNormalAoSLayout<PointNT, true>
{
  using Pod = typename pcl::traits::POD<PointNT>::type;

  static constexpr std::size_t kNormalX =
      pcl::rvv::RVVNormalFloatLayout<PointNT>::kNormalX;
  static constexpr std::size_t kNormalY =
      pcl::rvv::RVVNormalFloatLayout<PointNT>::kNormalY;
  static constexpr std::size_t kNormalZ =
      pcl::rvv::RVVNormalFloatLayout<PointNT>::kNormalZ;

  static constexpr bool value =
      std::is_standard_layout_v<Pod> && sizeof (PointNT) == sizeof (Pod) &&
      sizeof (PointNT) % alignof (float) == 0 &&
      kNormalX % alignof (float) == 0 &&
      kNormalY % alignof (float) == 0 &&
      kNormalZ % alignof (float) == 0;
};

template <typename PointT, typename PointNT>
inline constexpr bool kCylinderRVVLayoutCompatible =
    pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
    CylinderRVVNormalAoSLayout<PointNT>::value;

} // namespace detail
#endif

template <typename PointT, typename PointNT>
class SampleConsensusModelCylinderDiagnostic
  : public pcl::SampleConsensusModelCylinder<PointT, PointNT>
{
  using Base = pcl::SampleConsensusModelCylinder<PointT, PointNT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::error_sqr_dists_;
  using Base::selectWithinDistance;
  using Base::setIndices;
  using Base::setInputNormals;
  using Base::setNormalDistanceWeight;

  std::size_t
  countWithinDistanceCandidate (const Eigen::VectorXf& model_coefficients,
                                const double threshold) const
  {
    if (!this->isModelValid (model_coefficients))
      return 0;

#if defined (__RVV10__)
    if constexpr (detail::kCylinderRVVLayoutCompatible<PointT, PointNT>)
      return countWithinDistanceCandidateRVV (model_coefficients, threshold);
#endif

    return countWithinDistanceCandidateScalar (model_coefficients, threshold);
  }

  void
  selectWithinDistanceCandidate (const Eigen::VectorXf& model_coefficients,
                                 const double threshold,
                                 pcl::Indices& inliers)
  {
    if (!this->isModelValid (model_coefficients))
    {
      inliers.clear ();
      this->error_sqr_dists_.clear ();
      return;
    }

#if defined (__RVV10__)
    if constexpr (detail::kCylinderRVVLayoutCompatible<PointT, PointNT>)
    {
      if (sizeof (pcl::index_t) == sizeof (std::int32_t) &&
          std::is_signed_v<pcl::index_t>)
      {
        selectWithinDistanceCandidateRVV (model_coefficients, threshold, inliers);
        return;
      }
    }
#endif

    selectWithinDistanceCandidateScalar (model_coefficients, threshold, inliers);
  }

private:
  struct CylinderDistanceTerms
  {
    double weighted_euclid = 0.0;
    double final_distance = 0.0;
  };

  CylinderDistanceTerms
  computeDistanceTermsScalar (const PointT& point,
                              const PointNT& normal,
                              const Eigen::Vector3f& line_pt,
                              const Eigen::Vector3f& line_dir,
                              const float radius) const
  {
    const Eigen::Vector3f pt (point.x, point.y, point.z);
    const Eigen::Vector3f diff = pt - line_pt;
    const Eigen::Vector3f dir = diff - diff.dot (line_dir) * line_dir;
    const double weighted_euclid =
        (1.0 - this->normal_distance_weight_) * std::abs (dir.norm () - radius);

    const Eigen::Vector3f n (normal.normal_x, normal.normal_y, normal.normal_z);
    double d_normal = std::abs (pcl::getAngle3D (n, dir));
    d_normal = (std::min) (d_normal, M_PI - d_normal);

    return {weighted_euclid,
            std::abs (this->normal_distance_weight_ * d_normal + weighted_euclid)};
  }

  std::size_t
  countWithinDistanceCandidateScalar (const Eigen::VectorXf& model_coefficients,
                                      const double threshold) const
  {
    Eigen::Vector3f line_pt (model_coefficients[0],
                             model_coefficients[1],
                             model_coefficients[2]);
    Eigen::Vector3f line_dir (model_coefficients[3],
                              model_coefficients[4],
                              model_coefficients[5]);
    line_dir.normalize ();
    const float radius = model_coefficients[6];

    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const pcl::index_t index = (*this->indices_)[i];
      const CylinderDistanceTerms terms = computeDistanceTermsScalar (
          (*this->input_)[index], (*this->normals_)[index], line_pt, line_dir, radius);
      if (terms.weighted_euclid > threshold)
        continue;
      if (terms.final_distance < threshold)
        ++nr_p;
    }
    return nr_p;
  }

  void
  selectWithinDistanceCandidateScalar (const Eigen::VectorXf& model_coefficients,
                                       const double threshold,
                                       pcl::Indices& inliers)
  {
    inliers.clear ();
    this->error_sqr_dists_.clear ();
    inliers.reserve (this->indices_->size ());
    this->error_sqr_dists_.reserve (this->indices_->size ());

    Eigen::Vector3f line_pt (model_coefficients[0],
                             model_coefficients[1],
                             model_coefficients[2]);
    Eigen::Vector3f line_dir (model_coefficients[3],
                              model_coefficients[4],
                              model_coefficients[5]);
    line_dir.normalize ();
    const float radius = model_coefficients[6];

    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const pcl::index_t index = (*this->indices_)[i];
      const CylinderDistanceTerms terms = computeDistanceTermsScalar (
          (*this->input_)[index], (*this->normals_)[index], line_pt, line_dir, radius);
      if (terms.weighted_euclid > threshold)
        continue;
      if (terms.final_distance < threshold)
      {
        inliers.push_back (index);
        this->error_sqr_dists_.push_back (terms.final_distance);
      }
    }
  }

#if defined (__RVV10__)
  PCL_RVV_CYLINDER_DIAGNOSTIC_NOINLINE std::size_t
  countWithinDistanceCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                   const double threshold) const
  {
    const std::size_t total_n = this->indices_->size ();
    if (this->input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () ||
        this->normals_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ())
      return countWithinDistanceCandidateScalar (model_coefficients, threshold);

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
    const float normal_weight = static_cast<float> (this->normal_distance_weight_);
    const float euclid_weight = 1.0f - normal_weight;

    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const std::uint8_t* const normals_base =
        reinterpret_cast<const std::uint8_t*> (this->normals_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    using NormalLayout = detail::CylinderRVVNormalAoSLayout<PointNT>;

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
      computeDistanceTermsRVV (x_vec, y_vec, z_vec,
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
    return nr_p;
  }

  PCL_RVV_CYLINDER_DIAGNOSTIC_NOINLINE void
  selectWithinDistanceCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                    const double threshold,
                                    pcl::Indices& inliers)
  {
    const std::size_t total_n = this->indices_->size ();
    if (this->input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () ||
        this->normals_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ())
    {
      selectWithinDistanceCandidateScalar (model_coefficients, threshold, inliers);
      return;
    }

    inliers.resize (total_n);
    this->error_sqr_dists_.resize (total_n);

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
    const float normal_weight = static_cast<float> (this->normal_distance_weight_);
    const float euclid_weight = 1.0f - normal_weight;

    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const std::uint8_t* const normals_base =
        reinterpret_cast<const std::uint8_t*> (this->normals_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    using NormalLayout = detail::CylinderRVVNormalAoSLayout<PointNT>;

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
      computeDistanceTermsRVV (x_vec, y_vec, z_vec,
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
        __riscv_vse64_v_f64m4 (this->error_sqr_dists_.data () + nr_p,
                               distance64,
                               active_count);
        nr_p += active_count;
      }

      i += vl;
    }

    inliers.resize (nr_p);
    this->error_sqr_dists_.resize (nr_p);
  }

  void
  computeDistanceTermsRVV (const vfloat32m2_t& x_vec,
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
                           vbool16_t& euclid_mask) const
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
#endif
};

} // namespace pcl_rvv_test

#undef PCL_RVV_CYLINDER_DIAGNOSTIC_NOINLINE
