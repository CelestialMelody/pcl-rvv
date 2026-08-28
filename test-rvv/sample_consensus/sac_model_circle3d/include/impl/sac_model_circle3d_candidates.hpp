/*
 * 本文件做什么：
 * 这里会保存 SampleConsensusModelCircle3D 的测试专用候选 helper。Phase 000
 * 先把 public-like count/select（形态接近公开入口但仍是测试专用包装）
 * 和 test-only RVV candidate（仅测试使用的 RVV 候选）放在同一派生类里对拍。
 *
 * 当前 RED 阶段只声明访问层，候选实现随后补入；这样测试能先证明缺失的是
 * 本 topic 的诊断入口，而不是生产源码。
 */

#pragma once

#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/sample_consensus/sac_model_circle3d.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#if defined (__GNUC__)
#define PCL_RVV_CIRCLE3D_TEST_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_CIRCLE3D_TEST_NOINLINE
#endif

namespace pcl_rvv_test
{
namespace sac_model_circle3d
{

template <typename PointT>
class SampleConsensusModelCircle3DAccess
  : public pcl::SampleConsensusModelCircle3D<PointT>
{
  using Base = pcl::SampleConsensusModelCircle3D<PointT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::error_sqr_dists_;
  using Base::getDistancesToModel;
  using Base::selectWithinDistance;
  using Base::setIndices;

  std::size_t
  countWithinDistanceProjectionCandidate (const Eigen::VectorXf& model_coefficients,
                                          double threshold) const;

  void
  selectWithinDistanceProjectionCandidate (const Eigen::VectorXf& model_coefficients,
                                           double threshold,
                                           pcl::Indices& inliers);

private:
  static double
  circle3DSquaredDistance (const PointT& point,
                           const Eigen::VectorXf& model_coefficients)
  {
    const double dx = static_cast<double> (point.x) - model_coefficients[0];
    const double dy = static_cast<double> (point.y) - model_coefficients[1];
    const double dz = static_cast<double> (point.z) - model_coefficients[2];
    const double nx = model_coefficients[4];
    const double ny = model_coefficients[5];
    const double nz = model_coefficients[6];
    const double r = model_coefficients[3];
    const double n_norm2 = nx * nx + ny * ny + nz * nz;
    const double dot = dx * nx + dy * ny + dz * nz;
    const double scale = dot / n_norm2;
    const double px = dx - scale * nx;
    const double py = dy - scale * ny;
    const double pz = dz - scale * nz;
    const double radial_norm = std::sqrt (px * px + py * py + pz * pz);
    const double radial_error = radial_norm - r;
    return (dot * dot) / n_norm2 + radial_error * radial_error;
  }

  bool
  hasDegenerateProjection (const Eigen::VectorXf& model_coefficients) const
  {
    const double nx = model_coefficients[4];
    const double ny = model_coefficients[5];
    const double nz = model_coefficients[6];
    const double n_norm2 = nx * nx + ny * ny + nz * nz;
    if (n_norm2 <= std::numeric_limits<double>::epsilon ())
      return true;

    for (const auto index : *this->indices_)
    {
      const auto& point = (*this->input_)[index];
      const double dx = static_cast<double> (point.x) - model_coefficients[0];
      const double dy = static_cast<double> (point.y) - model_coefficients[1];
      const double dz = static_cast<double> (point.z) - model_coefficients[2];
      const double dot = dx * nx + dy * ny + dz * nz;
      const double scale = dot / n_norm2;
      const double px = dx - scale * nx;
      const double py = dy - scale * ny;
      const double pz = dz - scale * nz;
      if (px * px + py * py + pz * pz <= 1e-20)
        return true;
    }
    return false;
  }

#if defined (__RVV10__)
  static constexpr bool kCanUseIndexedCircle3DLayout =
      pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::x>::value &&
      pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::y>::value &&
      pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::z>::value &&
      sizeof (pcl::index_t) == sizeof (std::int32_t) &&
      std::is_signed_v<pcl::index_t>;

  bool
  canUseProjectionRVV (const Eigen::VectorXf& model_coefficients) const
  {
    if constexpr (kCanUseIndexedCircle3DLayout)
      return this->input_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () &&
             !hasDegenerateProjection (model_coefficients);
    return false;
  }

  PCL_RVV_CIRCLE3D_TEST_NOINLINE std::size_t
  countWithinDistanceProjectionRVV (const Eigen::VectorXf& model_coefficients,
                                    double threshold) const
  {
    const float cx = model_coefficients[0];
    const float cy = model_coefficients[1];
    const float cz = model_coefficients[2];
    const float r = model_coefficients[3];
    const float nx = model_coefficients[4];
    const float ny = model_coefficients[5];
    const float nz = model_coefficients[6];
    const float n_norm2 = nx * nx + ny * ny + nz * nz;
    const float inv_n_norm2 = 1.0f / n_norm2;
    const float squared_threshold = static_cast<float> (threshold * threshold);
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();

    std::size_t count = 0;
    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      const vuint32m2_t v_idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<PointT> (v_idx, vl);
      vfloat32m2_t vx;
      vfloat32m2_t vy;
      vfloat32m2_t vz;
      pcl::rvv_load::indexed_load3_fields_f32m2<
          PointT,
          pcl::traits::offset<PointT, pcl::fields::x>::value,
          pcl::traits::offset<PointT, pcl::fields::y>::value,
          pcl::traits::offset<PointT, pcl::fields::z>::value> (
          points_base, v_off, vl, vx, vy, vz);

      const vfloat32m2_t dx = __riscv_vfsub_vf_f32m2 (vx, cx, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vf_f32m2 (vy, cy, vl);
      const vfloat32m2_t dz = __riscv_vfsub_vf_f32m2 (vz, cz, vl);
      vfloat32m2_t dot = __riscv_vfmul_vf_f32m2 (dx, nx, vl);
      dot = __riscv_vfmacc_vf_f32m2 (dot, ny, dy, vl);
      dot = __riscv_vfmacc_vf_f32m2 (dot, nz, dz, vl);
      const vfloat32m2_t scale = __riscv_vfmul_vf_f32m2 (dot, inv_n_norm2, vl);
      const vfloat32m2_t px = __riscv_vfnmsac_vf_f32m2 (dx, nx, scale, vl);
      const vfloat32m2_t py = __riscv_vfnmsac_vf_f32m2 (dy, ny, scale, vl);
      const vfloat32m2_t pz = __riscv_vfnmsac_vf_f32m2 (dz, nz, scale, vl);
      vfloat32m2_t radial2 = __riscv_vfmul_vv_f32m2 (px, px, vl);
      radial2 = __riscv_vfmacc_vv_f32m2 (radial2, py, py, vl);
      radial2 = __riscv_vfmacc_vv_f32m2 (radial2, pz, pz, vl);
      const vfloat32m2_t radial = __riscv_vfsqrt_v_f32m2 (radial2, vl);
      const vfloat32m2_t radial_error = __riscv_vfsub_vf_f32m2 (radial, r, vl);
      vfloat32m2_t sqr = __riscv_vfmul_vv_f32m2 (radial_error, radial_error, vl);
      const vfloat32m2_t plane = __riscv_vfmul_vf_f32m2 (
          __riscv_vfmul_vv_f32m2 (dot, dot, vl), inv_n_norm2, vl);
      sqr = __riscv_vfadd_vv_f32m2 (sqr, plane, vl);
      const vbool16_t inliers_mask = __riscv_vmflt_vf_f32m2_b16 (sqr, squared_threshold, vl);
      count += __riscv_vcpop_m_b16 (inliers_mask, vl);
      i += vl;
    }
    return count;
  }

  PCL_RVV_CIRCLE3D_TEST_NOINLINE void
  selectWithinDistanceProjectionRVV (const Eigen::VectorXf& model_coefficients,
                                     double threshold,
                                     pcl::Indices& inliers)
  {
    const std::size_t total_n = this->indices_->size ();
    inliers.resize (total_n);
    this->error_sqr_dists_.resize (total_n);

    const float cx = model_coefficients[0];
    const float cy = model_coefficients[1];
    const float cz = model_coefficients[2];
    const float r = model_coefficients[3];
    const float nx = model_coefficients[4];
    const float ny = model_coefficients[5];
    const float nz = model_coefficients[6];
    const float n_norm2 = nx * nx + ny * ny + nz * nz;
    const float inv_n_norm2 = 1.0f / n_norm2;
    const float squared_threshold = static_cast<float> (threshold * threshold);
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    double* const error_ptr = this->error_sqr_dists_.data ();

    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      const vuint32m2_t v_idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<PointT> (v_idx, vl);
      vfloat32m2_t vx;
      vfloat32m2_t vy;
      vfloat32m2_t vz;
      pcl::rvv_load::indexed_load3_fields_f32m2<
          PointT,
          pcl::traits::offset<PointT, pcl::fields::x>::value,
          pcl::traits::offset<PointT, pcl::fields::y>::value,
          pcl::traits::offset<PointT, pcl::fields::z>::value> (
          points_base, v_off, vl, vx, vy, vz);

      const vfloat32m2_t dx = __riscv_vfsub_vf_f32m2 (vx, cx, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vf_f32m2 (vy, cy, vl);
      const vfloat32m2_t dz = __riscv_vfsub_vf_f32m2 (vz, cz, vl);
      vfloat32m2_t dot = __riscv_vfmul_vf_f32m2 (dx, nx, vl);
      dot = __riscv_vfmacc_vf_f32m2 (dot, ny, dy, vl);
      dot = __riscv_vfmacc_vf_f32m2 (dot, nz, dz, vl);
      const vfloat32m2_t scale = __riscv_vfmul_vf_f32m2 (dot, inv_n_norm2, vl);
      const vfloat32m2_t px = __riscv_vfnmsac_vf_f32m2 (dx, nx, scale, vl);
      const vfloat32m2_t py = __riscv_vfnmsac_vf_f32m2 (dy, ny, scale, vl);
      const vfloat32m2_t pz = __riscv_vfnmsac_vf_f32m2 (dz, nz, scale, vl);
      vfloat32m2_t radial2 = __riscv_vfmul_vv_f32m2 (px, px, vl);
      radial2 = __riscv_vfmacc_vv_f32m2 (radial2, py, py, vl);
      radial2 = __riscv_vfmacc_vv_f32m2 (radial2, pz, pz, vl);
      const vfloat32m2_t radial = __riscv_vfsqrt_v_f32m2 (radial2, vl);
      const vfloat32m2_t radial_error = __riscv_vfsub_vf_f32m2 (radial, r, vl);
      vfloat32m2_t sqr = __riscv_vfmul_vv_f32m2 (radial_error, radial_error, vl);
      const vfloat32m2_t plane = __riscv_vfmul_vf_f32m2 (
          __riscv_vfmul_vv_f32m2 (dot, dot, vl), inv_n_norm2, vl);
      sqr = __riscv_vfadd_vv_f32m2 (sqr, plane, vl);
      const vbool16_t inliers_mask = __riscv_vmflt_vf_f32m2_b16 (sqr, squared_threshold, vl);
      const std::size_t active_count = __riscv_vcpop_m_b16 (inliers_mask, vl);

      if (active_count > 0)
      {
        const vuint32m2_t compressed_idx =
            __riscv_vcompress_vm_u32m2 (v_idx, inliers_mask, vl);
        const vint32m2_t compressed_idx_i32 =
            __riscv_vreinterpret_v_u32m2_i32m2 (compressed_idx);
        __riscv_vse32_v_i32m2 (
            reinterpret_cast<std::int32_t*> (inliers.data () + nr_p),
            compressed_idx_i32,
            active_count);

        const vfloat32m2_t compressed_sqr =
            __riscv_vcompress_vm_f32m2 (sqr, inliers_mask, vl);
        const vfloat64m4_t sqr_double = __riscv_vfwcvt_f_f_v_f64m4 (
            compressed_sqr, active_count);
        __riscv_vse64_v_f64m4 (error_ptr + nr_p, sqr_double, active_count);
        nr_p += active_count;
      }

      i += vl;
    }
    inliers.resize (nr_p);
    this->error_sqr_dists_.resize (nr_p);
  }
#endif
};

template <typename PointT>
std::size_t
SampleConsensusModelCircle3DAccess<PointT>::countWithinDistanceProjectionCandidate (
    const Eigen::VectorXf& model_coefficients,
    const double threshold) const
{
  if (!this->isModelValid (model_coefficients))
    return 0;

#if defined (__RVV10__)
  if (canUseProjectionRVV (model_coefficients))
    return countWithinDistanceProjectionRVV (model_coefficients, threshold);
#endif
  return this->countWithinDistance (model_coefficients, threshold);
}

template <typename PointT>
void
SampleConsensusModelCircle3DAccess<PointT>::selectWithinDistanceProjectionCandidate (
    const Eigen::VectorXf& model_coefficients,
    const double threshold,
    pcl::Indices& inliers)
{
  if (!this->isModelValid (model_coefficients))
  {
    inliers.clear ();
    return;
  }

#if defined (__RVV10__)
  if (canUseProjectionRVV (model_coefficients))
  {
    selectWithinDistanceProjectionRVV (model_coefficients, threshold, inliers);
    return;
  }
#endif
  this->selectWithinDistance (model_coefficients, threshold, inliers);
}

} // namespace sac_model_circle3d
} // namespace pcl_rvv_test

#undef PCL_RVV_CIRCLE3D_TEST_NOINLINE
