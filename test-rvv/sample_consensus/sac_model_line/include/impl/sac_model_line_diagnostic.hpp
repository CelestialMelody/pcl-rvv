/*
 * 本文件做什么：
 * 这里放 SampleConsensusModelLine 的测试专用 diagnostic（诊断）helper。
 * 它继承 production class（生产类）来复用真实 input_ / indices_ 状态，
 * 但只服务 test-rvv correctness（正确性）和 bench（性能测试）。Phase 040 后，
 * production（生产源码）已有对应 public entry（公开入口）RVV dispatch（RVV 分流）；
 * 本 helper 继续保留，用来对照 diagnostic（诊断）候选和 production direct
 * （真实生产入口直连）证据。
 */

#pragma once

#include <pcl/rvv_point_load.h>
#include <pcl/sample_consensus/sac_model_line.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined (__GNUC__)
#define PCL_RVV_LINE_DIAGNOSTIC_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_LINE_DIAGNOSTIC_NOINLINE
#endif

namespace pcl_rvv_test {

template <typename PointT>
class SampleConsensusModelLineDiagnostic
  : public pcl::SampleConsensusModelLine<PointT>
{
  using Base = pcl::SampleConsensusModelLine<PointT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::error_sqr_dists_;
  using Base::getDistancesToModel;
  using Base::selectWithinDistance;
  using Base::setIndices;

  std::size_t
  countWithinDistanceCandidate (const Eigen::VectorXf& model_coefficients,
                                const double threshold) const
  {
    if (!this->isModelValid (model_coefficients))
      return 0;

#if defined (__RVV10__)
    if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value)
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
      return;

#if defined (__RVV10__)
    if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value &&
                  sizeof (pcl::index_t) == sizeof (std::int32_t) &&
                  std::is_signed_v<pcl::index_t>)
    {
      selectWithinDistanceCandidateRVV (model_coefficients, threshold, inliers);
      return;
    }
#endif

    selectWithinDistanceCandidateScalar (model_coefficients, threshold, inliers);
  }

  void
  getDistancesToModelCandidate (const Eigen::VectorXf& model_coefficients,
                                std::vector<double>& distances) const
  {
    if (!this->isModelValid (model_coefficients))
      return;

#if defined (__RVV10__)
    if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value)
    {
      getDistancesToModelCandidateRVV (model_coefficients, distances);
      return;
    }
#endif

    getDistancesToModelCandidateScalar (model_coefficients, distances);
  }

  void
  getDistancesToModelVFSqrtCandidate (const Eigen::VectorXf& model_coefficients,
                                      std::vector<double>& distances) const
  {
    if (!this->isModelValid (model_coefficients))
      return;

#if defined (__RVV10__)
    if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value)
    {
      getDistancesToModelVFSqrtCandidateRVV (model_coefficients, distances);
      return;
    }
#endif

    getDistancesToModelCandidateScalar (model_coefficients, distances);
  }

private:
  struct LineParameters
  {
    float px;
    float py;
    float pz;
    float dx_line;
    float dy_line;
    float dz_line;
    float sqr_threshold;
  };

  static LineParameters
  makeLineParameters (const Eigen::VectorXf& model_coefficients, const double threshold)
  {
    Eigen::Vector3f line_dir (model_coefficients[3],
                              model_coefficients[4],
                              model_coefficients[5]);
    line_dir.normalize ();
    return {model_coefficients[0],
            model_coefficients[1],
            model_coefficients[2],
            line_dir.x (),
            line_dir.y (),
            line_dir.z (),
            static_cast<float> (threshold * threshold)};
  }

  std::size_t
  countWithinDistanceCandidateScalar (const Eigen::VectorXf& model_coefficients,
                                      const double threshold) const
  {
    const double sqr_threshold = threshold * threshold;
    std::size_t count = 0;

    Eigen::Vector4f line_pt (model_coefficients[0],
                             model_coefficients[1],
                             model_coefficients[2],
                             0.0f);
    Eigen::Vector4f line_dir (model_coefficients[3],
                              model_coefficients[4],
                              model_coefficients[5],
                              0.0f);
    line_dir.normalize ();

    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const double sqr_distance =
          (line_pt - (*this->input_)[(*this->indices_)[i]].getVector4fMap ())
              .cross3 (line_dir)
              .squaredNorm ();
      if (sqr_distance < sqr_threshold)
        ++count;
    }
    return count;
  }

  void
  getDistancesToModelCandidateScalar (const Eigen::VectorXf& model_coefficients,
                                      std::vector<double>& distances) const
  {
    distances.resize (this->indices_->size ());

    Eigen::Vector4f line_pt (model_coefficients[0],
                             model_coefficients[1],
                             model_coefficients[2],
                             0.0f);
    Eigen::Vector4f line_dir (model_coefficients[3],
                              model_coefficients[4],
                              model_coefficients[5],
                              0.0f);
    line_dir.normalize ();

    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const double sqr_distance =
          (line_pt - (*this->input_)[(*this->indices_)[i]].getVector4fMap ())
              .cross3 (line_dir)
              .squaredNorm ();
      distances[i] = std::sqrt (sqr_distance);
    }
  }

  void
  selectWithinDistanceCandidateScalar (const Eigen::VectorXf& model_coefficients,
                                       const double threshold,
                                       pcl::Indices& inliers)
  {
    const double sqr_threshold = threshold * threshold;

    inliers.clear ();
    this->error_sqr_dists_.clear ();
    inliers.reserve (this->indices_->size ());
    this->error_sqr_dists_.reserve (this->indices_->size ());

    Eigen::Vector4f line_pt (model_coefficients[0],
                             model_coefficients[1],
                             model_coefficients[2],
                             0.0f);
    Eigen::Vector4f line_dir (model_coefficients[3],
                              model_coefficients[4],
                              model_coefficients[5],
                              0.0f);
    line_dir.normalize ();

    for (std::size_t i = 0; i < this->indices_->size (); ++i)
    {
      const double sqr_distance =
          (line_pt - (*this->input_)[(*this->indices_)[i]].getVector4fMap ())
              .cross3 (line_dir)
              .squaredNorm ();
      if (sqr_distance < sqr_threshold)
      {
        inliers.push_back ((*this->indices_)[i]);
        this->error_sqr_dists_.push_back (sqr_distance);
      }
    }
  }

#if defined (__RVV10__)
  PCL_RVV_LINE_DIAGNOSTIC_NOINLINE std::size_t
  countWithinDistanceCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                   const double threshold) const
  {
    const std::size_t total_n = this->indices_->size ();
    if (this->input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
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
    const float sqr_threshold = static_cast<float> (threshold * threshold);

    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;

    std::size_t count = 0;
    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      const vuint32m2_t idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
          points_base, offsets, vl, x_vec, y_vec, z_vec);

      const vfloat32m2_t px_vec = __riscv_vfmv_v_f_f32m2 (px, vl);
      const vfloat32m2_t py_vec = __riscv_vfmv_v_f_f32m2 (py, vl);
      const vfloat32m2_t pz_vec = __riscv_vfmv_v_f_f32m2 (pz, vl);
      const vfloat32m2_t vx = __riscv_vfsub_vv_f32m2 (px_vec, x_vec, vl);
      const vfloat32m2_t vy = __riscv_vfsub_vv_f32m2 (py_vec, y_vec, vl);
      const vfloat32m2_t vz = __riscv_vfsub_vv_f32m2 (pz_vec, z_vec, vl);

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
      count += __riscv_vcpop_m_b16 (inlier_mask, vl);
      i += vl;
    }

    return count;
  }

  PCL_RVV_LINE_DIAGNOSTIC_NOINLINE void
  selectWithinDistanceCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                    const double threshold,
                                    pcl::Indices& inliers)
  {
    const std::size_t total_n = this->indices_->size ();
    if (this->input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
    {
      selectWithinDistanceCandidateScalar (model_coefficients, threshold, inliers);
      return;
    }

    inliers.resize (total_n);
    this->error_sqr_dists_.resize (total_n);

    const LineParameters params = makeLineParameters (model_coefficients, threshold);
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    std::vector<float> compressed_sqr_distances (__riscv_vsetvlmax_e32m2 ());

    std::size_t nr_p = 0;
    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      const vuint32m2_t idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
          points_base, offsets, vl, x_vec, y_vec, z_vec);

      const vfloat32m2_t vx =
          __riscv_vfsub_vf_f32m2 (x_vec, params.px, vl);
      const vfloat32m2_t vy =
          __riscv_vfsub_vf_f32m2 (y_vec, params.py, vl);
      const vfloat32m2_t vz =
          __riscv_vfsub_vf_f32m2 (z_vec, params.pz, vl);

      const vfloat32m2_t cross_x =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vy, params.dz_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vz, params.dy_line, vl),
                                  vl);
      const vfloat32m2_t cross_y =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vz, params.dx_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vx, params.dz_line, vl),
                                  vl);
      const vfloat32m2_t cross_z =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vx, params.dy_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vy, params.dx_line, vl),
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
      const vbool16_t inlier_mask =
          __riscv_vmflt_vf_f32m2_b16 (sqr, params.sqr_threshold, vl);
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
        __riscv_vse32_v_f32m2 (compressed_sqr_distances.data (),
                               compressed_sqr,
                               active_count);

        // `vcompress`（向量压缩）保留本 chunk 内命中 lane（向量通道）的
        // 相对顺序；这里只把命中的平方距离转成 double 写回，模拟 production
        // select 的 inliers 与 error_sqr_dists_ 一一对应关系。
        for (std::size_t lane = 0; lane < active_count; ++lane)
          this->error_sqr_dists_[nr_p + lane] =
              static_cast<double> (compressed_sqr_distances[lane]);
        nr_p += active_count;
      }

      i += vl;
    }

    inliers.resize (nr_p);
    this->error_sqr_dists_.resize (nr_p);
  }

  PCL_RVV_LINE_DIAGNOSTIC_NOINLINE void
  getDistancesToModelCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                   std::vector<double>& distances) const
  {
    const std::size_t total_n = this->indices_->size ();
    if (this->input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
    {
      getDistancesToModelCandidateScalar (model_coefficients, distances);
      return;
    }

    distances.resize (total_n);

    const LineParameters params = makeLineParameters (model_coefficients, 0.0);
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    std::vector<float> sqr_distances (__riscv_vsetvlmax_e32m2 ());

    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      const vuint32m2_t idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
          points_base, offsets, vl, x_vec, y_vec, z_vec);

      const vfloat32m2_t vx =
          __riscv_vfsub_vf_f32m2 (x_vec, params.px, vl);
      const vfloat32m2_t vy =
          __riscv_vfsub_vf_f32m2 (y_vec, params.py, vl);
      const vfloat32m2_t vz =
          __riscv_vfsub_vf_f32m2 (z_vec, params.pz, vl);

      const vfloat32m2_t cross_x =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vy, params.dz_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vz, params.dy_line, vl),
                                  vl);
      const vfloat32m2_t cross_y =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vz, params.dx_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vx, params.dz_line, vl),
                                  vl);
      const vfloat32m2_t cross_z =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vx, params.dy_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vy, params.dx_line, vl),
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
      __riscv_vse32_v_f32m2 (sqr_distances.data (), sqr, vl);

      // 当前阶段只让 RVV 接管 indexed gather（离散加载）和平方距离公式。
      // sqrt（平方根）和 double 写回保持标量，便于单独判断这段后处理是否稀释收益。
      for (std::size_t lane = 0; lane < vl; ++lane)
        distances[i + lane] = std::sqrt (static_cast<double> (sqr_distances[lane]));
      i += vl;
    }
  }

  PCL_RVV_LINE_DIAGNOSTIC_NOINLINE void
  getDistancesToModelVFSqrtCandidateRVV (const Eigen::VectorXf& model_coefficients,
                                         std::vector<double>& distances) const
  {
    const std::size_t total_n = this->indices_->size ();
    if (this->input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> ())
    {
      getDistancesToModelCandidateScalar (model_coefficients, distances);
      return;
    }

    distances.resize (total_n);

    const LineParameters params = makeLineParameters (model_coefficients, 0.0);
    const std::uint8_t* const points_base =
        reinterpret_cast<const std::uint8_t*> (this->input_->points.data ());
    const pcl::index_t* const indices_ptr = this->indices_->data ();
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    std::vector<float> chunk_distances (__riscv_vsetvlmax_e32m2 ());

    for (std::size_t i = 0; i < total_n; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (total_n - i);
      const vuint32m2_t idx =
          __riscv_vle32_v_u32m2 (reinterpret_cast<const std::uint32_t*> (indices_ptr + i), vl);
      const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointT> (idx, vl);
      vfloat32m2_t x_vec;
      vfloat32m2_t y_vec;
      vfloat32m2_t z_vec;
      pcl::rvv_load::indexed_load3_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ> (
          points_base, offsets, vl, x_vec, y_vec, z_vec);

      const vfloat32m2_t vx =
          __riscv_vfsub_vf_f32m2 (x_vec, params.px, vl);
      const vfloat32m2_t vy =
          __riscv_vfsub_vf_f32m2 (y_vec, params.py, vl);
      const vfloat32m2_t vz =
          __riscv_vfsub_vf_f32m2 (z_vec, params.pz, vl);

      const vfloat32m2_t cross_x =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vy, params.dz_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vz, params.dy_line, vl),
                                  vl);
      const vfloat32m2_t cross_y =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vz, params.dx_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vx, params.dz_line, vl),
                                  vl);
      const vfloat32m2_t cross_z =
          __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vf_f32m2 (vx, params.dy_line, vl),
                                  __riscv_vfmul_vf_f32m2 (vy, params.dx_line, vl),
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
      __riscv_vse32_v_f32m2 (chunk_distances.data (), distance, vl);

      // Phase 030 只把 sqrt（平方根）移入 RVV chunk。最终 double 写回仍保留
      // 标量转换，以便和 Phase 020 的 scalar-sqrt 形状做同边界对照。
      for (std::size_t lane = 0; lane < vl; ++lane)
        distances[i + lane] = static_cast<double> (chunk_distances[lane]);
      i += vl;
    }
  }
#endif
};

} // namespace pcl_rvv_test

#undef PCL_RVV_LINE_DIAGNOSTIC_NOINLINE
