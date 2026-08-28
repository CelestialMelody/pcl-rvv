/*
 * 本文件做什么：
 * 这里保存 sac_model_circle 的测试专用候选 helper。它们模拟公开入口的对象状态
 * 和 direct indexed indices（直接索引）数据流，但不修改 production，也不能证明
 * production dispatch（生产分流）已经存在。
 *
 * Phase 020 保留旧的 `RVV 平方距离 + 标量 sqrt/store` 候选，用来解释已拒绝的
 * 实现族。Phase 050 新增 full-RVV 候选，参考 normal_plane 的 `vfwcvt + vse64`
 * dense double write-back（密集 double 写回）形态，专门验证去掉临时 buffer 和
 * 标量逐 lane 写回后是否值得进入 production integration loop（生产接入闭环）。
 * Phase 080 则只针对 selectWithinDistance 的命中点误差写回尾段做
 * RVV-vs-RVV detail A/B（两个 RVV 实现族在同一边界直接比较）：baseline
 * 是当前已采纳 production select RVV，candidate 在 vcompress 后继续用
 * `vfsqrt + vfwcvt + vse64` 写 `error_sqr_dists_`。
 */

#pragma once

#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/sample_consensus/sac_model_circle.h>

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined (__GNUC__)
#define PCL_RVV_CIRCLE_TEST_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_CIRCLE_TEST_NOINLINE
#endif

namespace pcl_rvv_test
{
namespace sac_model_circle
{

template <typename PointT>
class SampleConsensusModelCircleAccess
  : public pcl::SampleConsensusModelCircle2D<PointT>
{
  using Base = pcl::SampleConsensusModelCircle2D<PointT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::countWithinDistanceStandard;
  using Base::error_sqr_dists_;
  using Base::getDistancesToModel;
  using Base::getDistancesToModelStandard;
  using Base::selectWithinDistance;
  using Base::selectWithinDistanceStandard;
  using Base::setIndices;
#if defined (__RVV10__)
  using Base::countWithinDistanceRVV;
  using Base::getDistancesToModelRVV;
  using Base::selectWithinDistanceRVV;
#endif

  void
  getDistancesToModelScalarSqrtCandidate (const Eigen::VectorXf& model_coefficients,
                                          std::vector<double>& distances) const
  {
    if (!this->isModelValid (model_coefficients))
    {
      distances.clear ();
      return;
    }

#if defined (__RVV10__)
    if (canUseIndexedCircleRVV ())
    {
      getDistancesToModelScalarSqrtCandidateRVV (model_coefficients, distances);
      return;
    }
#endif
    this->getDistancesToModel (model_coefficients, distances);
  }

  void
  getDistancesToModelFullRVVCandidate (const Eigen::VectorXf& model_coefficients,
                                       std::vector<double>& distances) const
  {
    if (!this->isModelValid (model_coefficients))
    {
      distances.clear ();
      return;
    }

#if defined (__RVV10__)
    if (canUseIndexedCircleRVV ())
    {
      getDistancesToModelFullRVV (model_coefficients, distances);
      return;
    }
#endif
    this->getDistancesToModel (model_coefficients, distances);
  }

  void
  selectWithinDistanceFullRVVErrorTailCandidate (const Eigen::VectorXf& model_coefficients,
                                                const double threshold,
                                                pcl::Indices& inliers)
  {
    if (!this->isModelValid (model_coefficients))
    {
      inliers.clear ();
      return;
    }

#if defined (__RVV10__)
    if (canUseIndexedCircleRVV ())
    {
      selectWithinDistanceFullRVVErrorTail (model_coefficients, threshold, inliers);
      return;
    }
#endif
    this->selectWithinDistance (model_coefficients, threshold, inliers);
  }

#if defined (__RVV10__)
private:
  static constexpr bool kCanUseIndexedCircleRVVLayout =
      pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::x>::value &&
      pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::y>::value &&
      sizeof (pcl::index_t) == sizeof (std::int32_t) &&
      std::is_signed_v<pcl::index_t>;

  bool
  canUseIndexedCircleRVV () const
  {
    if constexpr (kCanUseIndexedCircleRVVLayout)
      return this->input_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ();
    return false;
  }

  PCL_RVV_CIRCLE_TEST_NOINLINE void
  getDistancesToModelScalarSqrtCandidateRVV (
      const Eigen::VectorXf& model_coefficients,
      std::vector<double>& distances) const
  {
    distances.resize (this->indices_->size ());
    const float a = model_coefficients[0];
    const float b = model_coefficients[1];
    const float r = model_coefficients[2];
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    std::vector<float> sqr_distances (__riscv_vsetvlmax_e32m2 ());

    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      const vuint32m2_t v_idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<PointT> (v_idx, vl);
      const vfloat32m2_t v_x =
          pcl::rvv_load::indexed_load_field_f32m2<PointT, pcl::fields::x> (
              points_base, v_off, vl);
      const vfloat32m2_t v_y =
          pcl::rvv_load::indexed_load_field_f32m2<PointT, pcl::fields::y> (
              points_base, v_off, vl);
      const vfloat32m2_t v_a = __riscv_vfmv_v_f_f32m2 (a, vl);
      const vfloat32m2_t v_b = __riscv_vfmv_v_f_f32m2 (b, vl);
      const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (v_x, v_a, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (v_y, v_b, vl);
      const vfloat32m2_t sqr =
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl);

      __riscv_vse32_v_f32m2 (sqr_distances.data (), sqr, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        distances[i + lane] =
            static_cast<double> (std::abs (std::sqrt (sqr_distances[lane]) - r));
      i += vl;
    }
  }

  PCL_RVV_CIRCLE_TEST_NOINLINE void
  getDistancesToModelFullRVV (const Eigen::VectorXf& model_coefficients,
                              std::vector<double>& distances) const
  {
    distances.resize (this->indices_->size ());
    const float a = model_coefficients[0];
    const float b = model_coefficients[1];
    const float r = model_coefficients[2];
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    double* const distances_ptr = distances.data ();

    for (std::size_t i = 0; i < this->indices_->size (); )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (this->indices_->size () - i);
      const vuint32m2_t v_idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<PointT> (v_idx, vl);
      const vfloat32m2_t v_x =
          pcl::rvv_load::indexed_load_field_f32m2<PointT, pcl::fields::x> (
              points_base, v_off, vl);
      const vfloat32m2_t v_y =
          pcl::rvv_load::indexed_load_field_f32m2<PointT, pcl::fields::y> (
              points_base, v_off, vl);
      const vfloat32m2_t v_a = __riscv_vfmv_v_f_f32m2 (a, vl);
      const vfloat32m2_t v_b = __riscv_vfmv_v_f_f32m2 (b, vl);
      const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (v_x, v_a, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (v_y, v_b, vl);
      const vfloat32m2_t sqr =
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl);
      const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2 (sqr, vl);
      vfloat32m2_t diff = __riscv_vfsub_vf_f32m2 (dist, r, vl);
      diff = __riscv_vfsgnjx_vv_f32m2 (diff, diff, vl);
      const vfloat64m4_t diff_double = __riscv_vfwcvt_f_f_v_f64m4 (diff, vl);
      __riscv_vse64_v_f64m4 (distances_ptr + i, diff_double, vl);
      i += vl;
    }
  }

  PCL_RVV_CIRCLE_TEST_NOINLINE void
  selectWithinDistanceFullRVVErrorTail (const Eigen::VectorXf& model_coefficients,
                                        const double threshold,
                                        pcl::Indices& inliers)
  {
    const std::size_t total_n = this->indices_->size ();
    inliers.resize (total_n);
    this->error_sqr_dists_.resize (total_n);

    const float a = model_coefficients[0];
    const float b = model_coefficients[1];
    const float r = model_coefficients[2];
    const float threshold_f = static_cast<float> (threshold);
    const float sqr_inner_radius = (r <= threshold_f)
                                    ? 0.0f
                                    : (r - threshold_f) * (r - threshold_f);
    const float sqr_outer_radius = (r + threshold_f) * (r + threshold_f);

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
      const vfloat32m2_t v_x =
          pcl::rvv_load::indexed_load_field_f32m2<PointT, pcl::fields::x> (
              points_base, v_off, vl);
      const vfloat32m2_t v_y =
          pcl::rvv_load::indexed_load_field_f32m2<PointT, pcl::fields::y> (
              points_base, v_off, vl);
      const vfloat32m2_t v_a = __riscv_vfmv_v_f_f32m2 (a, vl);
      const vfloat32m2_t v_b = __riscv_vfmv_v_f_f32m2 (b, vl);
      const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (v_x, v_a, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (v_y, v_b, vl);
      const vfloat32m2_t sqr =
          __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl);

      const vbool16_t mask_inner = __riscv_vmfge_vf_f32m2_b16 (sqr, sqr_inner_radius, vl);
      const vbool16_t mask_outer = __riscv_vmfle_vf_f32m2_b16 (sqr, sqr_outer_radius, vl);
      const vbool16_t inliers_mask = __riscv_vmand_mm_b16 (mask_inner, mask_outer, vl);
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
        const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2 (compressed_sqr, active_count);
        vfloat32m2_t diff = __riscv_vfsub_vf_f32m2 (dist, r, active_count);
        diff = __riscv_vfsgnjx_vv_f32m2 (diff, diff, active_count);
        const vfloat64m4_t diff_double = __riscv_vfwcvt_f_f_v_f64m4 (diff, active_count);
        __riscv_vse64_v_f64m4 (error_ptr + nr_p, diff_double, active_count);
        nr_p += active_count;
      }

      i += vl;
    }

    inliers.resize (nr_p);
    this->error_sqr_dists_.resize (nr_p);
  }
#endif
};

} // namespace sac_model_circle
} // namespace pcl_rvv_test

#undef PCL_RVV_CIRCLE_TEST_NOINLINE
