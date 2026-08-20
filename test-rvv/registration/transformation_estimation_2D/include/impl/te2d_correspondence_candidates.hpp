/*
 * 本文件做什么：
 * 本文件负责 correspondence row source 的 direct-gather、chunked staging 和 materialize candidates。
 *
 * correspondence 当前不接 production；这些 helper 只保留测试支撑和负向证据路径。
 */

#pragma once

#include "te2d_ordered_candidates.hpp"
#include "te2d_row_sources.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DCorrespondenceDirectStd(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  Fused2DAccumulation acc;
  const std::size_t n = correspondences.size();
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (const auto& correspondence : correspondences) {
    const auto& source_point =
        source[static_cast<std::size_t>(correspondence.index_query)];
    const auto& target_point =
        target[static_cast<std::size_t>(correspondence.index_match)];
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

  for (const auto& correspondence : correspondences) {
    const auto& source_point =
        source[static_cast<std::size_t>(correspondence.index_query)];
    const auto& target_point =
        target[static_cast<std::size_t>(correspondence.index_match)];
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
accumulateFused2DCorrespondenceDirectRVV(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  using TargetPod = typename TargetLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "direct correspondence RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::Correspondence::index_query) == sizeof(std::int32_t),
                "direct correspondence RVV candidate expects 32-bit correspondence indices");

  Fused2DAccumulation acc;
  const std::size_t n = correspondences.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
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
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
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

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DCorrespondenceChunkedXYZStagingRVV(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  using TargetPod = typename TargetLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "chunked correspondence RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::Correspondence::index_query) == sizeof(std::int32_t),
                "chunked correspondence RVV candidate expects 32-bit indices");

  Fused2DAccumulation acc;
  const std::size_t n = correspondences.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
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
  constexpr std::size_t kChunkRows = 256;
  alignas(64) float staged_sx[kChunkRows];
  alignas(64) float staged_sy[kChunkRows];
  alignas(64) float staged_sz[kChunkRows];
  alignas(64) float staged_tx[kChunkRows];
  alignas(64) float staged_ty[kChunkRows];
  alignas(64) float staged_tz[kChunkRows];

  for (std::size_t chunk_begin = 0; chunk_begin < n; chunk_begin += kChunkRows) {
    const std::size_t chunk_count = std::min(kChunkRows, n - chunk_begin);
    std::size_t staged = 0;
    while (staged < chunk_count) {
      const std::size_t global = chunk_begin + staged;
      const std::size_t vl = __riscv_vsetvl_e32m2(chunk_count - staged);
      const auto* query_i32 =
          reinterpret_cast<const std::int32_t*>(query_base + global * corr_stride);
      const auto* match_i32 =
          reinterpret_cast<const std::int32_t*>(match_base + global * corr_stride);
      const vint32m2_t v_query_i32 =
          __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
      const vint32m2_t v_match_i32 =
          __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
      const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
          __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
      const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
          __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
      vfloat32m2_t sx, sy, sz, tx, ty, tz;
      pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                         SourceLayout::kX,
                                         SourceLayout::kY,
                                         SourceLayout::kZ>(
          source_base, v_query_off, vl, sx, sy, sz);
      pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                         TargetLayout::kX,
                                         TargetLayout::kY,
                                         TargetLayout::kZ>(
          target_base, v_match_off, vl, tx, ty, tz);
      __riscv_vse32_v_f32m2(staged_sx + staged, sx, vl);
      __riscv_vse32_v_f32m2(staged_sy + staged, sy, vl);
      __riscv_vse32_v_f32m2(staged_sz + staged, sz, vl);
      __riscv_vse32_v_f32m2(staged_tx + staged, tx, vl);
      __riscv_vse32_v_f32m2(staged_ty + staged, ty, vl);
      __riscv_vse32_v_f32m2(staged_tz + staged, tz, vl);
      staged += vl;
    }

    staged = 0;
    while (staged < chunk_count) {
      const std::size_t vl = __riscv_vsetvl_e32m2(chunk_count - staged);
      const vfloat32m2_t sx = __riscv_vle32_v_f32m2(staged_sx + staged, vl);
      const vfloat32m2_t sy = __riscv_vle32_v_f32m2(staged_sy + staged, vl);
      const vfloat32m2_t tx = __riscv_vle32_v_f32m2(staged_tx + staged, vl);
      const vfloat32m2_t ty = __riscv_vle32_v_f32m2(staged_ty + staged, vl);
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
      staged += vl;
    }
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
estimateFused2DCorrespondenceCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeCorrespondencePair(source,
                                     target,
                                     correspondences,
                                     materialized_source,
                                     materialized_target)) {
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
estimateFused2DCorrespondenceDirectGatherCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = correspondences.size();
    stats->source_finite_points =
        countFiniteCorrespondenceQueryRows(source, correspondences);
    stats->target_finite_points =
        countFiniteCorrespondenceMatchRows(target, correspondences);
  }

  const bool valid_dense = isValidDenseCorrespondencePair(source, target, correspondences);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? correspondences.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (correspondences.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DCorrespondenceDirectRVV(source, target, correspondences));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DCorrespondenceDirectStd(source, target, correspondences));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = correspondences.size();
    stats->source_finite_points =
        countFiniteCorrespondenceQueryRows(source, correspondences);
    stats->target_finite_points =
        countFiniteCorrespondenceMatchRows(target, correspondences);
  }

  const bool valid_dense = isValidDenseCorrespondencePair(source, target, correspondences);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? correspondences.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (correspondences.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DCorrespondenceChunkedXYZStagingRVV(
              source, target, correspondences));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DCorrespondenceDirectStd(source, target, correspondences));
}

} // namespace pcl::registration::rvv_te2d_support
