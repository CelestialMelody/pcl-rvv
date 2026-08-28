/*
 * 本文件提供 sac_model_sphere topic 的测试/bench 共享 access wrapper。
 * 它暴露 production Standard/RVV helper 供 correctness 对拍，并保留测试专用
 * select/getDistances candidate 作为诊断和历史证据入口。
 */

#pragma once

#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/sample_consensus/sac_model_sphere.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined (__GNUC__)
#define PCL_RVV_SPHERE_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_SPHERE_NOINLINE
#endif

namespace pcl_rvv_sphere_test_support
{

template <typename PointT>
class SampleConsensusModelSphereAccess
  : public pcl::SampleConsensusModelSphere<PointT>
{
  using Base = pcl::SampleConsensusModelSphere<PointT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::countWithinDistanceStandard;
  using Base::error_sqr_dists_;
  using Base::getDistancesToModel;
  using Base::selectWithinDistance;
  using Base::selectWithinDistanceStandard;
  using Base::setIndices;
#if defined (__RVV10__)
  using Base::countWithinDistanceRVV;
  using Base::selectWithinDistanceRVV;
#endif

  void
  getDistancesToModelCandidate (const Eigen::VectorXf& model_coefficients,
                                std::vector<double>& distances) const
  {
    if (!this->isModelValid (model_coefficients))
    {
      distances.clear ();
      return;
    }

#if defined (__RVV10__)
    if constexpr (pcl::rvv::RVVXYZFloatLayout<PointT>::value)
    {
      getDistancesToModelCandidateRVV (model_coefficients, distances);
      return;
    }
#endif
    getDistancesToModelCandidateScalar (model_coefficients, distances);
  }

  void
  selectWithinDistanceCandidate (const Eigen::VectorXf& model_coefficients,
                                 const double threshold,
                                 pcl::Indices& inliers)
  {
    if (!this->isModelValid (model_coefficients))
    {
      inliers.clear ();
      return;
    }

#if defined (__RVV10__)
    if constexpr (pcl::rvv::RVVXYZFloatLayout<PointT>::value)
    {
      selectWithinDistanceCandidateRVV (model_coefficients, threshold, inliers);
      return;
    }
#endif
    selectWithinDistanceCandidateScalar (model_coefficients, threshold, inliers);
  }

#if defined (__RVV10__)
  void
  selectWithinDistanceVCompressCandidate (const Eigen::VectorXf& model_coefficients,
                                          const double threshold,
                                          pcl::Indices& inliers)
  {
    if (!this->isModelValid (model_coefficients))
    {
      inliers.clear ();
      return;
    }

    selectWithinDistanceVCompressCandidateRVV (model_coefficients, threshold, inliers);
  }
#endif

private:
  void
  getDistancesToModelCandidateScalar (const Eigen::VectorXf& model_coefficients,
                                      std::vector<double>& distances) const
  {
    distances.resize (this->indices_->size ());
    const Eigen::Vector3f center (model_coefficients[0], model_coefficients[1], model_coefficients[2]);
    for (std::size_t i = 0; i < this->indices_->size (); ++i)
      distances[i] = std::abs (((*this->input_)[(*this->indices_)[i]].getVector3fMap () - center).norm () - model_coefficients[3]);
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

    const float radius = model_coefficients[3];
    const float threshold_f = static_cast<float> (threshold);
    const float sqr_inner_radius = (radius <= threshold_f ? 0.0f : (radius - threshold_f) * (radius - threshold_f));
    const float sqr_outer_radius = (radius + threshold_f) * (radius + threshold_f);
    const Eigen::Vector3f center (model_coefficients[0], model_coefficients[1], model_coefficients[2]);

    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const float sqr_dist = ((*this->input_)[(*this->indices_)[i]].getVector3fMap () - center).squaredNorm ();
      if ((sqr_dist <= sqr_outer_radius) && (sqr_dist >= sqr_inner_radius))
      {
        inliers.push_back ((*this->indices_)[i]);
        this->error_sqr_dists_.push_back (static_cast<double> (std::abs (std::sqrt (sqr_dist) - radius)));
      }
    }
  }

#if defined (__RVV10__)
  vfloat32m2_t
  sqrDistancesRVV (const vfloat32m2_t& x_vec,
                   const vfloat32m2_t& y_vec,
                   const vfloat32m2_t& z_vec,
                   const Eigen::VectorXf& model_coefficients,
                   const std::size_t vl) const
  {
    const vfloat32m2_t xc = __riscv_vfmv_v_f_f32m2 (model_coefficients[0], vl);
    const vfloat32m2_t yc = __riscv_vfmv_v_f_f32m2 (model_coefficients[1], vl);
    const vfloat32m2_t zc = __riscv_vfmv_v_f_f32m2 (model_coefficients[2], vl);
    const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (x_vec, xc, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (y_vec, yc, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2 (z_vec, zc, vl);
    return __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl),
        dz,
        dz,
        vl);
  }

  void
  loadIndexedXYZ (const std::size_t begin,
                  const std::size_t vl,
                  vfloat32m2_t& x_vec,
                  vfloat32m2_t& y_vec,
                  vfloat32m2_t& z_vec) const
  {
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using Layout = pcl::rvv::RVVXYZFloatLayout<PointT>;

    const vuint32m2_t idx =
        __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + begin), vl);
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
    pcl::rvv_load::indexed_load3_fields_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
        points_base, offsets, vl, x_vec, y_vec, z_vec);
  }

  void
  getDistancesToModelCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                   std::vector<double>& distances) const
  {
    distances.resize (this->indices_->size ());
    const float radius = model_coefficients[3];
    std::vector<float> sqr_distances (__riscv_vsetvlmax_e32m2 ());

    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      loadIndexedXYZ (i, vl, x_vec, y_vec, z_vec);
      const vfloat32m2_t sqr = sqrDistancesRVV (x_vec, y_vec, z_vec, model_coefficients, vl);
      __riscv_vse32_v_f32m2 (sqr_distances.data (), sqr, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        distances[i + lane] = static_cast<double> (std::abs (std::sqrt (sqr_distances[lane]) - radius));
      i += vl;
    }
  }

  void
  selectWithinDistanceCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                    const double threshold,
                                    pcl::Indices& inliers)
  {
    inliers.clear ();
    this->error_sqr_dists_.clear ();
    inliers.reserve (this->indices_->size ());
    this->error_sqr_dists_.reserve (this->indices_->size ());

    const float radius = model_coefficients[3];
    const float threshold_f = static_cast<float> (threshold);
    const float sqr_inner_radius = (radius <= threshold_f ? 0.0f : (radius - threshold_f) * (radius - threshold_f));
    const float sqr_outer_radius = (radius + threshold_f) * (radius + threshold_f);
    std::vector<float> sqr_distances (__riscv_vsetvlmax_e32m2 ());

    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      loadIndexedXYZ (i, vl, x_vec, y_vec, z_vec);
      const vfloat32m2_t sqr = sqrDistancesRVV (x_vec, y_vec, z_vec, model_coefficients, vl);
      __riscv_vse32_v_f32m2 (sqr_distances.data (), sqr, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
      {
        const float sqr_dist = sqr_distances[lane];
        if ((sqr_dist <= sqr_outer_radius) && (sqr_dist >= sqr_inner_radius))
        {
          inliers.push_back ((*this->indices_)[i + lane]);
          this->error_sqr_dists_.push_back (static_cast<double> (std::abs (std::sqrt (sqr_dist) - radius)));
        }
      }
      i += vl;
    }
  }

  PCL_RVV_SPHERE_NOINLINE void
  selectWithinDistanceVCompressCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                             const double threshold,
                                             pcl::Indices& inliers)
  {
    static_assert (std::is_same_v<pcl::index_t, std::int32_t>,
                   "vcompress candidate stores indices through a signed 32-bit RVV store.");

    const std::size_t total_n = this->indices_->size ();
    inliers.resize (total_n);
    this->error_sqr_dists_.resize (total_n);

    const float radius = model_coefficients[3];
    const float threshold_f = static_cast<float> (threshold);
    const float sqr_inner_radius = (radius <= threshold_f ? 0.0f : (radius - threshold_f) * (radius - threshold_f));
    const float sqr_outer_radius = (radius + threshold_f) * (radius + threshold_f);
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    std::vector<float> compressed_sqr_distances (__riscv_vsetvlmax_e32m2 ());

    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      const vuint32m2_t v_idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      loadIndexedXYZ (i, vl, x_vec, y_vec, z_vec);
      const vfloat32m2_t sqr = sqrDistancesRVV (x_vec, y_vec, z_vec, model_coefficients, vl);
      const vbool16_t inner_mask = __riscv_vmfge_vf_f32m2_b16 (sqr, sqr_inner_radius, vl);
      const vbool16_t outer_mask = __riscv_vmfle_vf_f32m2_b16 (sqr, sqr_outer_radius, vl);
      const vbool16_t inlier_mask = __riscv_vmand_mm_b16 (inner_mask, outer_mask, vl);
      const std::size_t active_count = __riscv_vcpop_m_b16 (inlier_mask, vl);

      if (active_count > 0)
      {
        const vuint32m2_t compressed_idx = __riscv_vcompress_vm_u32m2 (v_idx, inlier_mask, vl);
        const vint32m2_t compressed_idx_i32 =
            __riscv_vreinterpret_v_u32m2_i32m2 (compressed_idx);
        __riscv_vse32_v_i32m2 (inliers.data () + nr_p, compressed_idx_i32, active_count);

        const vfloat32m2_t compressed_sqr = __riscv_vcompress_vm_f32m2 (sqr, inlier_mask, vl);
        __riscv_vse32_v_f32m2 (compressed_sqr_distances.data (), compressed_sqr, active_count);
        for (std::size_t lane = 0; lane < active_count; ++lane)
          this->error_sqr_dists_[nr_p + lane] =
              static_cast<double> (std::abs (std::sqrt (compressed_sqr_distances[lane]) - radius));
        nr_p += active_count;
      }

      i += vl;
    }

    inliers.resize (nr_p);
    this->error_sqr_dists_.resize (nr_p);
  }
#endif
};

}  // namespace pcl_rvv_sphere_test_support

#undef PCL_RVV_SPHERE_NOINLINE
