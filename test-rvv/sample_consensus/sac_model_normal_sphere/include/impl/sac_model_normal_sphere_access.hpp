/*
 * sac_model_normal_sphere 的测试支撑实现。
 *
 * 本文件只服务 test-rvv topic：它复刻 production 标量公式，并在 RVV 构建下提供
 * test-only candidate（测试专用候选）。这些 helper 能证明公式、load/gather
 * 和输出合同是否可行，不能证明真实 production dispatch（生产分流）已经接入。
 */

#pragma once

#include <pcl/common/common.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/sample_consensus/sac_model_normal_sphere.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined (__GNUC__)
#define PCL_RVV_NORMAL_SPHERE_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_NORMAL_SPHERE_NOINLINE
#endif

namespace pcl_rvv_normal_sphere_test_support
{

template <typename PointT, typename PointNT>
class SampleConsensusModelNormalSphereAccess
  : public pcl::SampleConsensusModelNormalSphere<PointT, PointNT>
{
  using Base = pcl::SampleConsensusModelNormalSphere<PointT, PointNT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::error_sqr_dists_;
  using Base::getDistancesToModel;
  using Base::selectWithinDistance;
  using Base::setIndices;
  using Base::setInputNormals;
  using Base::setNormalDistanceWeight;

  std::size_t
  countWithinDistanceScalarReference (const Eigen::VectorXf& model_coefficients,
                                      const double threshold) const
  {
    if (!this->normals_ || !this->isModelValid (model_coefficients))
      return 0;

    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const double distance = scalarDistanceAt (i, model_coefficients);
      if (distance < threshold)
        ++nr_p;
    }
    return nr_p;
  }

  void
  selectWithinDistanceScalarReference (const Eigen::VectorXf& model_coefficients,
                                       const double threshold,
                                       pcl::Indices& inliers)
  {
    inliers.clear ();
    this->error_sqr_dists_.clear ();
    if (!this->normals_ || !this->isModelValid (model_coefficients))
      return;

    inliers.reserve (this->indices_->size ());
    this->error_sqr_dists_.reserve (this->indices_->size ());
    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const double weighted_euclid = scalarWeightedEuclidAt (i, model_coefficients);
      if (weighted_euclid > threshold)
        continue;

      const double distance = scalarDistanceAtAfterEuclid (i, model_coefficients, weighted_euclid);
      if (distance < threshold)
      {
        inliers.push_back ((*this->indices_)[i]);
        this->error_sqr_dists_.push_back (distance);
      }
    }
  }

  void
  getDistancesToModelScalarReference (const Eigen::VectorXf& model_coefficients,
                                      std::vector<double>& distances) const
  {
    if (!this->normals_ || !this->isModelValid (model_coefficients))
    {
      distances.clear ();
      return;
    }

    distances.resize (this->indices_->size ());
    for (std::size_t i = 0; i < this->indices_->size (); ++i)
      distances[i] = scalarDistanceAt (i, model_coefficients);
  }

  std::size_t
  countWithinDistanceCandidate (const Eigen::VectorXf& model_coefficients,
                                const double threshold) const
  {
    if (!this->normals_ || !this->isModelValid (model_coefficients))
      return 0;

#if defined (__RVV10__)
    if (canUseRVVCandidate ())
      return countWithinDistanceCandidateRVV (model_coefficients, threshold);
#endif
    return countWithinDistanceScalarReference (model_coefficients, threshold);
  }

  void
  selectWithinDistanceCandidate (const Eigen::VectorXf& model_coefficients,
                                 const double threshold,
                                 pcl::Indices& inliers)
  {
    if (!this->normals_ || !this->isModelValid (model_coefficients))
    {
      inliers.clear ();
      this->error_sqr_dists_.clear ();
      return;
    }

#if defined (__RVV10__)
    if (canUseRVVCandidate ())
    {
      selectWithinDistanceCandidateRVV (model_coefficients, threshold, inliers);
      return;
    }
#endif
    selectWithinDistanceScalarReference (model_coefficients, threshold, inliers);
  }

  void
  selectWithinDistanceVCompressCandidate (const Eigen::VectorXf& model_coefficients,
                                          const double threshold,
                                          pcl::Indices& inliers)
  {
    if (!this->normals_ || !this->isModelValid (model_coefficients))
    {
      inliers.clear ();
      this->error_sqr_dists_.clear ();
      return;
    }

#if defined (__RVV10__)
    if (canUseRVVCandidate ())
    {
      selectWithinDistanceVCompressCandidateRVV (model_coefficients, threshold, inliers);
      return;
    }
#endif
    selectWithinDistanceScalarReference (model_coefficients, threshold, inliers);
  }

  void
  getDistancesToModelCandidate (const Eigen::VectorXf& model_coefficients,
                                std::vector<double>& distances) const
  {
    if (!this->normals_ || !this->isModelValid (model_coefficients))
    {
      distances.clear ();
      return;
    }

#if defined (__RVV10__)
    if (canUseRVVCandidate ())
    {
      getDistancesToModelCandidateRVV (model_coefficients, distances);
      return;
    }
#endif
    getDistancesToModelScalarReference (model_coefficients, distances);
  }

private:
  double
  scalarWeightedEuclidAt (const std::size_t i, const Eigen::VectorXf& model_coefficients) const
  {
    const PointT& pt = (*this->input_)[(*this->indices_)[i]];
    const Eigen::Vector4f center (model_coefficients[0],
                                  model_coefficients[1],
                                  model_coefficients[2],
                                  0.0f);
    const Eigen::Vector4f p (pt.x, pt.y, pt.z, 0.0f);
    const Eigen::Vector4f n_dir = p - center;
    return (1.0 - this->normal_distance_weight_) *
           std::abs (n_dir.norm () - model_coefficients[3]);
  }

  double
  scalarDistanceAtAfterEuclid (const std::size_t i,
                               const Eigen::VectorXf& model_coefficients,
                               const double weighted_euclid) const
  {
    const PointT& pt = (*this->input_)[(*this->indices_)[i]];
    const PointNT& nt = (*this->normals_)[(*this->indices_)[i]];
    const Eigen::Vector4f center (model_coefficients[0],
                                  model_coefficients[1],
                                  model_coefficients[2],
                                  0.0f);
    const Eigen::Vector4f p (pt.x, pt.y, pt.z, 0.0f);
    const Eigen::Vector4f n_dir = p - center;
    const Eigen::Vector4f n (nt.normal[0], nt.normal[1], nt.normal[2], 0.0f);

    double d_normal = std::abs (pcl::getAngle3D (n, n_dir));
    d_normal = (std::min) (d_normal, M_PI - d_normal);
    return std::abs (this->normal_distance_weight_ * d_normal + weighted_euclid);
  }

  double
  scalarDistanceAt (const std::size_t i, const Eigen::VectorXf& model_coefficients) const
  {
    return scalarDistanceAtAfterEuclid (
        i, model_coefficients, scalarWeightedEuclidAt (i, model_coefficients));
  }

#if defined (__RVV10__)
  static constexpr bool
  layoutCompatible ()
  {
    return pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
           pcl::rvv::RVVNormalFloatLayout<PointNT>::value &&
           std::is_standard_layout_v<PointNT> &&
           sizeof (PointNT) % alignof (float) == 0;
  }

  bool
  canUseRVVCandidate () const
  {
    if constexpr (!layoutCompatible ())
      return false;
    return this->input_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () &&
           this->normals_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ();
  }

  PCL_RVV_NORMAL_SPHERE_NOINLINE vfloat32m2_t
  distancesRVV (const Eigen::VectorXf& model_coefficients,
                const vfloat32m2_t& px,
                const vfloat32m2_t& py,
                const vfloat32m2_t& pz,
                const vfloat32m2_t& nx,
                const vfloat32m2_t& ny,
                const vfloat32m2_t& nz,
                const std::size_t vl) const
  {
    const vfloat32m2_t cx = __riscv_vfmv_v_f_f32m2 (model_coefficients[0], vl);
    const vfloat32m2_t cy = __riscv_vfmv_v_f_f32m2 (model_coefficients[1], vl);
    const vfloat32m2_t cz = __riscv_vfmv_v_f_f32m2 (model_coefficients[2], vl);
    const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (px, cx, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (py, cy, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2 (pz, cz, vl);

    const vfloat32m2_t dir_sqr = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl),
        dz,
        dz,
        vl);
    const vfloat32m2_t dir_norm = __riscv_vfsqrt_v_f32m2 (dir_sqr, vl);

    const vfloat32m2_t normal_sqr = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (nx, nx, vl), ny, ny, vl),
        nz,
        nz,
        vl);
    const vfloat32m2_t normal_norm = __riscv_vfsqrt_v_f32m2 (normal_sqr, vl);

    const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
    const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
    const vfloat32m2_t tiny = __riscv_vfmv_v_f_f32m2 (1.0e-20f, vl);
    const vbool16_t dir_nonzero = __riscv_vmfgt_vf_f32m2_b16 (dir_sqr, 0.0f, vl);
    const vbool16_t normal_nonzero = __riscv_vmfgt_vf_f32m2_b16 (normal_sqr, 0.0f, vl);
    const vfloat32m2_t inv_dir =
        __riscv_vmerge_vvm_f32m2 (zero,
                                  __riscv_vfdiv_vv_f32m2 (one, __riscv_vfmax_vv_f32m2 (dir_norm, tiny, vl), vl),
                                  dir_nonzero,
                                  vl);
    const vfloat32m2_t inv_normal =
        __riscv_vmerge_vvm_f32m2 (zero,
                                  __riscv_vfdiv_vv_f32m2 (one, __riscv_vfmax_vv_f32m2 (normal_norm, tiny, vl), vl),
                                  normal_nonzero,
                                  vl);

    const vfloat32m2_t dir_x = __riscv_vfmul_vv_f32m2 (dx, inv_dir, vl);
    const vfloat32m2_t dir_y = __riscv_vfmul_vv_f32m2 (dy, inv_dir, vl);
    const vfloat32m2_t dir_z = __riscv_vfmul_vv_f32m2 (dz, inv_dir, vl);
    const vfloat32m2_t normal_x = __riscv_vfmul_vv_f32m2 (nx, inv_normal, vl);
    const vfloat32m2_t normal_y = __riscv_vfmul_vv_f32m2 (ny, inv_normal, vl);
    const vfloat32m2_t normal_z = __riscv_vfmul_vv_f32m2 (nz, inv_normal, vl);

    vfloat32m2_t weighted_euclid =
        __riscv_vfsub_vf_f32m2 (dir_norm, model_coefficients[3], vl);
    weighted_euclid = __riscv_vfsgnjx_vv_f32m2 (weighted_euclid, weighted_euclid, vl);
    weighted_euclid = __riscv_vfmul_vf_f32m2 (
        weighted_euclid, static_cast<float> (1.0 - this->normal_distance_weight_), vl);

    const vfloat32m2_t d_normal = pcl::getAcuteAngle3DRVV_f32m2 (
        normal_x, normal_y, normal_z, dir_x, dir_y, dir_z, vl);
    vfloat32m2_t distance = __riscv_vfmacc_vf_f32m2 (
        weighted_euclid, static_cast<float> (this->normal_distance_weight_), d_normal, vl);
    distance = __riscv_vfsgnjx_vv_f32m2 (distance, distance, vl);
    return distance;
  }

  void
  loadIndexedPointAndNormal (const std::size_t begin,
                             const std::size_t vl,
                             vuint32m2_t& idx,
                             vfloat32m2_t& px,
                             vfloat32m2_t& py,
                             vfloat32m2_t& pz,
                             vfloat32m2_t& nx,
                             vfloat32m2_t& ny,
                             vfloat32m2_t& nz) const
  {
    static_assert (std::is_same_v<pcl::index_t, std::int32_t>,
                   "normal-sphere candidate stores and loads indices as signed 32-bit values.");

    const pcl::index_t* const indices_ptr = this->indices_->data ();
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const std::uint8_t* const normals_base =
        reinterpret_cast<const std::uint8_t*> (this->normals_->points.data ());
    using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;

    idx = __riscv_vle32_v_u32m2 (
        reinterpret_cast<const std::uint32_t*> (indices_ptr + begin), vl);
    const vuint32m2_t point_offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
    const vuint32m2_t normal_offsets = pcl::rvv_load::byte_offsets_u32m2<PointNT> (idx, vl);

    pcl::rvv_load::indexed_load3_fields_f32m2<
        PointT, PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
        points_base, point_offsets, vl, px, py, pz);
    nx = pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_x> (
        normals_base, normal_offsets, vl);
    ny = pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_y> (
        normals_base, normal_offsets, vl);
    nz = pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_z> (
        normals_base, normal_offsets, vl);
  }

  std::size_t
  countWithinDistanceCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                   const double threshold) const
  {
    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      vuint32m2_t idx;
      vfloat32m2_t px, py, pz, nx, ny, nz;
      loadIndexedPointAndNormal (i, vl, idx, px, py, pz, nx, ny, nz);
      const vfloat32m2_t distance = distancesRVV (
          model_coefficients, px, py, pz, nx, ny, nz, vl);
      const vbool16_t inlier_mask =
          __riscv_vmflt_vf_f32m2_b16 (distance, static_cast<float> (threshold), vl);
      nr_p += __riscv_vcpop_m_b16 (inlier_mask, vl);
      i += vl;
    }
    return nr_p;
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
    std::vector<float> distances (__riscv_vsetvlmax_e32m2 ());

    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      vuint32m2_t idx;
      vfloat32m2_t px, py, pz, nx, ny, nz;
      loadIndexedPointAndNormal (i, vl, idx, px, py, pz, nx, ny, nz);
      const vfloat32m2_t distance = distancesRVV (
          model_coefficients, px, py, pz, nx, ny, nz, vl);
      __riscv_vse32_v_f32m2 (distances.data (), distance, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
      {
        if (distances[lane] < threshold)
        {
          inliers.push_back ((*this->indices_)[i + lane]);
          this->error_sqr_dists_.push_back (static_cast<double> (distances[lane]));
        }
      }
      i += vl;
    }
  }

  PCL_RVV_NORMAL_SPHERE_NOINLINE void
  selectWithinDistanceVCompressCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                             const double threshold,
                                             pcl::Indices& inliers)
  {
    static_assert (std::is_same_v<pcl::index_t, std::int32_t>,
                   "normal-sphere vcompress candidate stores indices through signed 32-bit RVV stores.");

    const std::size_t total_n = this->indices_->size ();
    inliers.resize (total_n);
    this->error_sqr_dists_.resize (total_n);

    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      vuint32m2_t idx;
      vfloat32m2_t px, py, pz, nx, ny, nz;
      loadIndexedPointAndNormal (i, vl, idx, px, py, pz, nx, ny, nz);
      const vfloat32m2_t distance = distancesRVV (
          model_coefficients, px, py, pz, nx, ny, nz, vl);
      const vbool16_t inlier_mask =
          __riscv_vmflt_vf_f32m2_b16 (distance, static_cast<float> (threshold), vl);
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
        const vfloat64m4_t distance_d =
            __riscv_vfwcvt_f_f_v_f64m4 (compressed_distance, active_count);
        __riscv_vse64_v_f64m4 (this->error_sqr_dists_.data () + nr_p,
                               distance_d,
                               active_count);
        nr_p += active_count;
      }

      i += vl;
    }

    inliers.resize (nr_p);
    this->error_sqr_dists_.resize (nr_p);
  }

  void
  getDistancesToModelCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                   std::vector<double>& distances) const
  {
    distances.resize (this->indices_->size ());
    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      vuint32m2_t idx;
      vfloat32m2_t px, py, pz, nx, ny, nz;
      loadIndexedPointAndNormal (i, vl, idx, px, py, pz, nx, ny, nz);
      const vfloat32m2_t distance = distancesRVV (
          model_coefficients, px, py, pz, nx, ny, nz, vl);
      const vfloat64m4_t distance_d = __riscv_vfwcvt_f_f_v_f64m4 (distance, vl);
      __riscv_vse64_v_f64m4 (distances.data () + i, distance_d, vl);
      i += vl;
    }
  }
#endif
};

}  // namespace pcl_rvv_normal_sphere_test_support

#undef PCL_RVV_NORMAL_SPHERE_NOINLINE
