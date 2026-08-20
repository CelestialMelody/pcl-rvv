/*
 * 本文件做什么：
 * 本文件负责 source-indexed row source 的 materialize 与 direct-gather test candidates。
 *
 * 它保留 Phase 091 / 110 exact 边界和 generic 诊断边界，不能外推到未采纳 generic widening。
 */

#pragma once

#include "te2d_ordered_candidates.hpp"
#include "te2d_row_sources.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DSourceIndexedDirectStd(const pcl::PointCloud<PointSource>& source,
                                        const pcl::Indices& source_indices,
                                        const pcl::PointCloud<PointTarget>& target)
{
  Fused2DAccumulation acc;
  const std::size_t n = source_indices.size();
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (std::size_t i = 0; i < n; ++i) {
    const auto& source_point = source[static_cast<std::size_t>(source_indices[i])];
    const auto& target_point = target[i];
    source_sum[0] += source_point.x;
    source_sum[1] += source_point.y;
    target_sum[0] += target_point.x;
    target_sum[1] += target_point.y;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = source_sum[0] * inv_n;
  acc.source_centroid[1] = source_sum[1] * inv_n;
  acc.target_centroid[0] = target_sum[0] * inv_n;
  acc.target_centroid[1] = target_sum[1] * inv_n;

  for (std::size_t i = 0; i < n; ++i) {
    const auto& source_point = source[static_cast<std::size_t>(source_indices[i])];
    const auto& target_point = target[i];
    const float sx = source_point.x - acc.source_centroid[0];
    const float sy = source_point.y - acc.source_centroid[1];
    const float tx = target_point.x - acc.target_centroid[0];
    const float ty = target_point.y - acc.target_centroid[1];
    acc.correlation[0] += sx * tx;
    acc.correlation[1] += sx * ty;
    acc.correlation[2] += sy * tx;
    acc.correlation[3] += sy * ty;
  }
  return acc;
}


#ifdef __RVV10__
template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DSourceIndexedDirectRVV(const pcl::PointCloud<PointSource>& source,
                                        const pcl::Indices& source_indices,
                                        const pcl::PointCloud<PointTarget>& target)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "direct source-indexed RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "direct source-indexed RVV candidate expects 32-bit PCL indices");

  Fused2DAccumulation acc;
  const std::size_t n = source_indices.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* indices_i32 = reinterpret_cast<const std::int32_t*>(source_indices.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_idx_i32 = __riscv_vle32_v_i32m2(indices_i32 + i, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_off, vl, sx, sy, sz);
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
    const vint32m2_t v_idx_i32 = __riscv_vle32_v_i32m2(indices_i32 + i, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_off, vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    const vfloat32m2_t source_x =
        __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t source_y =
        __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t target_x =
        __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t target_y =
        __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
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

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DSourceIndexedCandidate(const pcl::PointCloud<PointSource>& source,
                                      const pcl::Indices& source_indices,
                                      const pcl::PointCloud<PointTarget>& target,
                                      CandidateStats* stats = nullptr)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeSourceIndexedPair(
          source, source_indices, target, materialized_source, materialized_target)) {
    if (stats) {
      *stats = {};
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }
  return estimateFused2DCandidate(materialized_source, materialized_target, stats);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DSourceIndexedDirectGatherCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = source_indices.size();
    stats->source_finite_points = countFiniteIndexedRows(source, source_indices);
    stats->target_finite_points = countFinitePrefixRows(target, source_indices.size());
  }

  const bool valid_dense = isValidDenseSourceIndexedPair(source, source_indices, target);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? source_indices.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source_indices.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DSourceIndexedDirectRVV(source, source_indices, target));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DSourceIndexedDirectStd(source, source_indices, target));
}

} // namespace pcl::registration::rvv_te2d_support
