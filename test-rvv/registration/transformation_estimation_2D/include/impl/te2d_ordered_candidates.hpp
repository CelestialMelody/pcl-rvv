/*
 * 本文件做什么：
 * 本文件负责 ordered-cloud-pair 的标量 / RVV fused 2D candidate 和求解入口。
 *
 * 该 candidate 属于测试支撑；只有真实 production direct 证据才能证明 production dispatch。
 */

#pragma once

#include "te2d_layout_helpers.hpp"
#include "te2d_public_wrappers.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DStd(const pcl::PointCloud<PointSource>& source,
                     const pcl::PointCloud<PointTarget>& target)
{
  Fused2DAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (std::size_t i = 0; i < n; ++i) {
    source_sum[0] += source[i].x;
    source_sum[1] += source[i].y;
    target_sum[0] += target[i].x;
    target_sum[1] += target[i].y;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = source_sum[0] * inv_n;
  acc.source_centroid[1] = source_sum[1] * inv_n;
  acc.target_centroid[0] = target_sum[0] * inv_n;
  acc.target_centroid[1] = target_sum[1] * inv_n;

  for (std::size_t i = 0; i < n; ++i) {
    const float sx = source[i].x - acc.source_centroid[0];
    const float sy = source[i].y - acc.source_centroid[1];
    const float tx = target[i].x - acc.target_centroid[0];
    const float ty = target[i].y - acc.target_centroid[1];
    acc.correlation[0] += sx * tx;
    acc.correlation[1] += sx * ty;
    acc.correlation[2] += sy * tx;
    acc.correlation[3] += sy * ty;
  }
  return acc;
}


#ifdef __RVV10__
inline float
reduceSum(vfloat32m2_t value, const std::size_t vlmax)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return __riscv_vfmv_f_s_f32m1_f32(
      __riscv_vfredosum_vs_f32m2_f32m1(value, zero, vlmax));
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DRVV(const pcl::PointCloud<PointSource>& source,
                     const pcl::PointCloud<PointTarget>& target)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  static_assert(SourceLayout::value && TargetLayout::value,
                "RVV fused candidate requires traits-gated xyz AoS layouts");

  Fused2DAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero;
  vfloat32m2_t stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = reduceSum(ssx, vlmax) * inv_n;
  acc.source_centroid[1] = reduceSum(ssy, vlmax) * inv_n;
  acc.target_centroid[0] = reduceSum(stx, vlmax) * inv_n;
  acc.target_centroid[1] = reduceSum(sty, vlmax) * inv_n;

  vfloat32m2_t sx_tx = zero, sx_ty = zero, sy_tx = zero, sy_ty = zero;
  i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    const vfloat32m2_t source_x = __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t source_y = __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t target_x = __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t target_y = __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
    sx_tx = __riscv_vfmacc_vv_f32m2_tu(sx_tx, source_x, target_x, vl);
    sx_ty = __riscv_vfmacc_vv_f32m2_tu(sx_ty, source_x, target_y, vl);
    sy_tx = __riscv_vfmacc_vv_f32m2_tu(sy_tx, source_y, target_x, vl);
    sy_ty = __riscv_vfmacc_vv_f32m2_tu(sy_ty, source_y, target_y, vl);
    i += vl;
  }

  acc.correlation[0] = reduceSum(sx_tx, vlmax);
  acc.correlation[1] = reduceSum(sx_ty, vlmax);
  acc.correlation[2] = reduceSum(sy_tx, vlmax);
  acc.correlation[3] = reduceSum(sy_ty, vlmax);
  return acc;
}

#endif

inline Eigen::Matrix4f
solveTransform2DFromAccumulation(const Fused2DAccumulation& acc)
{
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  if (acc.count == 0)
    return transform;

  const float src_cx = acc.source_centroid[0];
  const float src_cy = acc.source_centroid[1];
  const float tgt_cx = acc.target_centroid[0];
  const float tgt_cy = acc.target_centroid[1];

  const float h00 = acc.correlation[0];
  const float h01 = acc.correlation[1];
  const float h10 = acc.correlation[2];
  const float h11 = acc.correlation[3];
  const float angle = std::atan2(h01 - h10, h00 + h11);
  const float c = std::cos(angle);
  const float s = std::sin(angle);

  transform(0, 0) = c;
  transform(0, 1) = -s;
  transform(1, 0) = s;
  transform(1, 1) = c;
  transform(0, 3) = tgt_cx - (c * src_cx - s * src_cy);
  transform(1, 3) = tgt_cy - (s * src_cx + c * src_cy);
  transform(2, 3) = 0.0f;
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DStd(const pcl::PointCloud<PointSource>& source,
                   const pcl::PointCloud<PointTarget>& target,
                   CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
    stats->source_finite_points = countFinitePoints(source);
    stats->target_finite_points = countFinitePoints(target);
    stats->dense_finite_input = isDenseFiniteOrderedPair(source, target);
    stats->used_fallback = true;
  }
  return solveTransform2DFromAccumulation(accumulateFused2DStd(source, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DCandidate(const pcl::PointCloud<PointSource>& source,
                         const pcl::PointCloud<PointTarget>& target,
                         CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = std::min(source.size(), target.size());
    stats->source_finite_points = countFinitePoints(source);
    stats->target_finite_points = countFinitePoints(target);
  }

  if (source.size() != target.size() || source.empty()) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

  const bool dense_finite = isDenseFiniteOrderedPair(source, target);
  if (stats) {
    stats->dense_finite_input = dense_finite;
    stats->accepted_points = dense_finite ? source.size() : 0;
  }

  if (!dense_finite) {
    if (stats)
      stats->used_fallback = true;
    return estimatePublic2D(source, target);
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(accumulateFused2DRVV(source, target));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateFused2DStd(source, target);
}

} // namespace pcl::registration::rvv_te2d_support
