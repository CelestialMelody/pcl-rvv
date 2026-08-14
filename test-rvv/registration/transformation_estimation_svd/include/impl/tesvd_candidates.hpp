/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_svd 的标量 reference（参考链路）和 test-only
 * RVV candidate（RVV 候选链路）。fixture（夹具）/样本构造和共享统计结构已拆到
 * `tesvd_support.hpp`，本文件专注 row source reference、fallback gate 和 RVV 累加形状。
 *
 * 证据边界：
 * Phase 030 额外加入 source-indexed-cloud-pair（源索引点云对）candidate：它只把
 * source 侧改成 indexed gather（按 indices 离散加载），target 侧仍按同下标连续读取。
 * 这些 helper 仍是 test-only diagnostic（测试专用诊断）代码；production direct 结论必须
 * 继续由真实 public dispatch、fallback、反汇编和板卡证据闭合。
 */

#pragma once

#include "tesvd_support.hpp"

#include <pcl/common/eigen.h>
#include <pcl/correspondence.h>
#include <pcl/common/transforms.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/transformation_estimation_svd.h>
#include <pcl/types.h>

#include <Eigen/SVD>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_tesvd_support {

template <typename PointT>
inline bool
indicesInRange(const pcl::PointCloud<PointT>& source, const pcl::Indices& indices)
{
  return std::all_of(indices.begin(), indices.end(), [&](const pcl::index_t index) {
    return index >= 0 && static_cast<std::size_t>(index) < source.size();
  });
}

template <typename PointT>
inline bool
sourceIndicesInRange(const pcl::PointCloud<PointT>& source, const pcl::Indices& indices)
{
  return indicesInRange(source, indices);
}

inline pcl::Correspondences
makeCorrespondences(const pcl::Indices& source_indices, const pcl::Indices& target_indices)
{
  pcl::Correspondences correspondences;
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    correspondences.emplace_back(source_indices[i], target_indices[i], 0.0f);
  }
  return correspondences;
}

template <typename PointSource, typename PointTarget>
inline bool
correspondencesInRange(const pcl::PointCloud<PointSource>& source,
                       const pcl::PointCloud<PointTarget>& target,
                       const pcl::Correspondences& correspondences)
{
  for (const auto& corr : correspondences) {
    if (corr.index_query < 0 || corr.index_match < 0)
      return false;
    if (static_cast<std::size_t>(corr.index_query) >= source.size() ||
        static_cast<std::size_t>(corr.index_match) >= target.size()) {
      return false;
    }
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedStd(const pcl::PointCloud<PointSource>& source,
                   const pcl::PointCloud<PointTarget>& target)
{
  FusedAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  for (std::size_t i = 0; i < n; ++i) {
    const float sx = source[i].x;
    const float sy = source[i].y;
    const float sz = source[i].z;
    const float tx = target[i].x;
    const float ty = target[i].y;
    const float tz = target[i].z;
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.target_source_sum[0] += tx * sx;
    acc.target_source_sum[1] += tx * sy;
    acc.target_source_sum[2] += tx * sz;
    acc.target_source_sum[3] += ty * sx;
    acc.target_source_sum[4] += ty * sy;
    acc.target_source_sum[5] += ty * sz;
    acc.target_source_sum[6] += tz * sx;
    acc.target_source_sum[7] += tz * sy;
    acc.target_source_sum[8] += tz * sz;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedSourceIndexedStd(const pcl::PointCloud<PointSource>& source,
                                const pcl::Indices& indices,
                                const pcl::PointCloud<PointTarget>& target)
{
  FusedAccumulation acc;
  if (indices.size() != target.size() || !sourceIndicesInRange(source, indices))
    return acc;

  acc.count = indices.size();
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const auto& sp = source[static_cast<std::size_t>(indices[i])];
    const auto& tp = target[i];
    const float sx = sp.x;
    const float sy = sp.y;
    const float sz = sp.z;
    const float tx = tp.x;
    const float ty = tp.y;
    const float tz = tp.z;
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.target_source_sum[0] += tx * sx;
    acc.target_source_sum[1] += tx * sy;
    acc.target_source_sum[2] += tx * sz;
    acc.target_source_sum[3] += ty * sx;
    acc.target_source_sum[4] += ty * sy;
    acc.target_source_sum[5] += ty * sz;
    acc.target_source_sum[6] += tz * sx;
    acc.target_source_sum[7] += tz * sy;
    acc.target_source_sum[8] += tz * sz;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedDualIndicesStd(const pcl::PointCloud<PointSource>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<PointTarget>& target,
                              const pcl::Indices& target_indices)
{
  FusedAccumulation acc;
  if (source_indices.size() != target_indices.size() ||
      !indicesInRange(source, source_indices) ||
      !indicesInRange(target, target_indices)) {
    return acc;
  }

  acc.count = source_indices.size();
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto& sp = source[static_cast<std::size_t>(source_indices[i])];
    const auto& tp = target[static_cast<std::size_t>(target_indices[i])];
    const float sx = sp.x;
    const float sy = sp.y;
    const float sz = sp.z;
    const float tx = tp.x;
    const float ty = tp.y;
    const float tz = tp.z;
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.target_source_sum[0] += tx * sx;
    acc.target_source_sum[1] += tx * sy;
    acc.target_source_sum[2] += tx * sz;
    acc.target_source_sum[3] += ty * sx;
    acc.target_source_sum[4] += ty * sy;
    acc.target_source_sum[5] += ty * sz;
    acc.target_source_sum[6] += tz * sx;
    acc.target_source_sum[7] += tz * sy;
    acc.target_source_sum[8] += tz * sz;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedCorrespondencesStd(const pcl::PointCloud<PointSource>& source,
                                  const pcl::PointCloud<PointTarget>& target,
                                  const pcl::Correspondences& correspondences)
{
  FusedAccumulation acc;
  if (!correspondencesInRange(source, target, correspondences))
    return acc;

  acc.count = correspondences.size();
  for (const auto& corr : correspondences) {
    const auto& sp = source[static_cast<std::size_t>(corr.index_query)];
    const auto& tp = target[static_cast<std::size_t>(corr.index_match)];
    const float sx = sp.x;
    const float sy = sp.y;
    const float sz = sp.z;
    const float tx = tp.x;
    const float ty = tp.y;
    const float tz = tp.z;
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.target_source_sum[0] += tx * sx;
    acc.target_source_sum[1] += tx * sy;
    acc.target_source_sum[2] += tx * sz;
    acc.target_source_sum[3] += ty * sx;
    acc.target_source_sum[4] += ty * sy;
    acc.target_source_sum[5] += ty * sz;
    acc.target_source_sum[6] += tz * sx;
    acc.target_source_sum[7] += tz * sy;
    acc.target_source_sum[8] += tz * sz;
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
inline FusedAccumulation
accumulateFusedRVV(const pcl::PointCloud<PointSource>& source,
                   const pcl::PointCloud<PointTarget>& target)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  FusedAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceSum(ssx, vlmax);
  acc.source_sum[1] = reduceSum(ssy, vlmax);
  acc.source_sum[2] = reduceSum(ssz, vlmax);
  acc.target_sum[0] = reduceSum(stx, vlmax);
  acc.target_sum[1] = reduceSum(sty, vlmax);
  acc.target_sum[2] = reduceSum(stz, vlmax);
  acc.target_source_sum[0] = reduceSum(t0s0, vlmax);
  acc.target_source_sum[1] = reduceSum(t0s1, vlmax);
  acc.target_source_sum[2] = reduceSum(t0s2, vlmax);
  acc.target_source_sum[3] = reduceSum(t1s0, vlmax);
  acc.target_source_sum[4] = reduceSum(t1s1, vlmax);
  acc.target_source_sum[5] = reduceSum(t1s2, vlmax);
  acc.target_source_sum[6] = reduceSum(t2s0, vlmax);
  acc.target_source_sum[7] = reduceSum(t2s1, vlmax);
  acc.target_source_sum[8] = reduceSum(t2s2, vlmax);
  return acc;
}

template <typename PointT>
inline bool
sourceIndicesFitRVVGather(const pcl::PointCloud<PointT>& source,
                          const pcl::Indices& indices)
{
  return source.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>() &&
         sourceIndicesInRange(source, indices);
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedSourceIndexedRVV(const pcl::PointCloud<PointSource>& source,
                                const pcl::Indices& indices,
                                const pcl::PointCloud<PointTarget>& target)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SrcPod = typename SrcLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "source-indexed RVV test helper expects 32-bit PCL indices.");

  FusedAccumulation acc;
  acc.count = indices.size();
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* idx_i32 = reinterpret_cast<const std::int32_t*>(indices.data());

  std::size_t i = 0;
  while (i < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const vint32m2_t v_idx_i32 = __riscv_vle32_v_i32m2(idx_i32 + i, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SrcPod,
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base, v_off, vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceSum(ssx, vlmax);
  acc.source_sum[1] = reduceSum(ssy, vlmax);
  acc.source_sum[2] = reduceSum(ssz, vlmax);
  acc.target_sum[0] = reduceSum(stx, vlmax);
  acc.target_sum[1] = reduceSum(sty, vlmax);
  acc.target_sum[2] = reduceSum(stz, vlmax);
  acc.target_source_sum[0] = reduceSum(t0s0, vlmax);
  acc.target_source_sum[1] = reduceSum(t0s1, vlmax);
  acc.target_source_sum[2] = reduceSum(t0s2, vlmax);
  acc.target_source_sum[3] = reduceSum(t1s0, vlmax);
  acc.target_source_sum[4] = reduceSum(t1s1, vlmax);
  acc.target_source_sum[5] = reduceSum(t1s2, vlmax);
  acc.target_source_sum[6] = reduceSum(t2s0, vlmax);
  acc.target_source_sum[7] = reduceSum(t2s1, vlmax);
  acc.target_source_sum[8] = reduceSum(t2s2, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedDualIndicesRVV(const pcl::PointCloud<PointSource>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<PointTarget>& target,
                              const pcl::Indices& target_indices)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SrcPod = typename SrcLayout::Pod;
  using TgtPod = typename TgtLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "dual-indices RVV test helper expects 32-bit PCL indices.");

  FusedAccumulation acc;
  acc.count = source_indices.size();
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* source_idx_i32 = reinterpret_cast<const std::int32_t*>(source_indices.data());
  const auto* target_idx_i32 = reinterpret_cast<const std::int32_t*>(target_indices.data());

  std::size_t i = 0;
  while (i < source_indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(source_indices.size() - i);
    const vint32m2_t v_source_idx_i32 = __riscv_vle32_v_i32m2(source_idx_i32 + i, vl);
    const vint32m2_t v_target_idx_i32 = __riscv_vle32_v_i32m2(target_idx_i32 + i, vl);
    const vuint32m2_t v_source_off = pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_source_idx_i32), vl);
    const vuint32m2_t v_target_off = pcl::rvv_load::byte_offsets_u32m2<TgtPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_target_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SrcPod,
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base, v_source_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TgtPod,
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base, v_target_off, vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceSum(ssx, vlmax);
  acc.source_sum[1] = reduceSum(ssy, vlmax);
  acc.source_sum[2] = reduceSum(ssz, vlmax);
  acc.target_sum[0] = reduceSum(stx, vlmax);
  acc.target_sum[1] = reduceSum(sty, vlmax);
  acc.target_sum[2] = reduceSum(stz, vlmax);
  acc.target_source_sum[0] = reduceSum(t0s0, vlmax);
  acc.target_source_sum[1] = reduceSum(t0s1, vlmax);
  acc.target_source_sum[2] = reduceSum(t0s2, vlmax);
  acc.target_source_sum[3] = reduceSum(t1s0, vlmax);
  acc.target_source_sum[4] = reduceSum(t1s1, vlmax);
  acc.target_source_sum[5] = reduceSum(t1s2, vlmax);
  acc.target_source_sum[6] = reduceSum(t2s0, vlmax);
  acc.target_source_sum[7] = reduceSum(t2s1, vlmax);
  acc.target_source_sum[8] = reduceSum(t2s2, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline FusedAccumulation
accumulateFusedCorrespondencesRVV(const pcl::PointCloud<PointSource>& source,
                                  const pcl::PointCloud<PointTarget>& target,
                                  const pcl::Correspondences& correspondences)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SrcPod = typename SrcLayout::Pod;
  using TgtPod = typename TgtLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "correspondence RVV test helper expects 32-bit PCL indices.");

  FusedAccumulation acc;
  acc.count = correspondences.size();
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < correspondences.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(correspondences.size() - i);
    const auto* query_i32 = reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 = reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TgtPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SrcPod,
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TgtPod,
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceSum(ssx, vlmax);
  acc.source_sum[1] = reduceSum(ssy, vlmax);
  acc.source_sum[2] = reduceSum(ssz, vlmax);
  acc.target_sum[0] = reduceSum(stx, vlmax);
  acc.target_sum[1] = reduceSum(sty, vlmax);
  acc.target_sum[2] = reduceSum(stz, vlmax);
  acc.target_source_sum[0] = reduceSum(t0s0, vlmax);
  acc.target_source_sum[1] = reduceSum(t0s1, vlmax);
  acc.target_source_sum[2] = reduceSum(t0s2, vlmax);
  acc.target_source_sum[3] = reduceSum(t1s0, vlmax);
  acc.target_source_sum[4] = reduceSum(t1s1, vlmax);
  acc.target_source_sum[5] = reduceSum(t1s2, vlmax);
  acc.target_source_sum[6] = reduceSum(t2s0, vlmax);
  acc.target_source_sum[7] = reduceSum(t2s1, vlmax);
  acc.target_source_sum[8] = reduceSum(t2s2, vlmax);
  return acc;
}
#endif // __RVV10__

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedDualIndicesStd(const pcl::PointCloud<PointSource>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<PointTarget>& target,
                            const pcl::Indices& target_indices,
                            CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = std::min(source_indices.size(), target_indices.size());
    stats->accepted_points = source_indices.size() == target_indices.size() &&
                                     indicesInRange(source, source_indices) &&
                                     indicesInRange(target, target_indices)
                                 ? source_indices.size()
                                 : 0;
    stats->used_fallback = true;
  }
  return solveUmeyamaNoScaleFromAccumulation(
      accumulateFusedDualIndicesStd(source, source_indices, target, target_indices));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedDualIndicesCandidate(const pcl::PointCloud<PointSource>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<PointTarget>& target,
                                  const pcl::Indices& target_indices,
                                  CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = std::min(source_indices.size(), target_indices.size());
    stats->accepted_points = source_indices.size() == target_indices.size() &&
                                     indicesInRange(source, source_indices) &&
                                     indicesInRange(target, target_indices)
                                 ? source_indices.size()
                                 : 0;
  }

  if (source_indices.size() != target_indices.size() || source_indices.empty() ||
      !indicesInRange(source, source_indices) || !indicesInRange(target, target_indices)) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source.is_dense && target.is_dense && source_indices.size() >= 16) {
      if (stats) {
        stats->layout_supported = true;
        stats->used_rvv = true;
      }
      return solveUmeyamaNoScaleFromAccumulation(
          accumulateFusedDualIndicesRVV(source, source_indices, target, target_indices));
    }
    if (stats)
      stats->layout_supported = true;
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateFusedDualIndicesStd(source, source_indices, target, target_indices, nullptr);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedCorrespondencesStd(const pcl::PointCloud<PointSource>& source,
                                const pcl::PointCloud<PointTarget>& target,
                                const pcl::Correspondences& correspondences,
                                CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = correspondences.size();
    stats->accepted_points = correspondencesInRange(source, target, correspondences)
                                 ? correspondences.size()
                                 : 0;
    stats->used_fallback = true;
  }
  return solveUmeyamaNoScaleFromAccumulation(
      accumulateFusedCorrespondencesStd(source, target, correspondences));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedCorrespondencesCandidate(const pcl::PointCloud<PointSource>& source,
                                      const pcl::PointCloud<PointTarget>& target,
                                      const pcl::Correspondences& correspondences,
                                      CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = correspondences.size();
    stats->accepted_points = correspondencesInRange(source, target, correspondences)
                                 ? correspondences.size()
                                 : 0;
  }

  if (correspondences.empty() || !correspondencesInRange(source, target, correspondences)) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source.is_dense && target.is_dense && correspondences.size() >= 16) {
      if (stats) {
        stats->layout_supported = true;
        stats->used_rvv = true;
      }
      return solveUmeyamaNoScaleFromAccumulation(
          accumulateFusedCorrespondencesRVV(source, target, correspondences));
    }
    if (stats)
      stats->layout_supported = true;
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateFusedCorrespondencesStd(source, target, correspondences, nullptr);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicDualIndicesUmeyama(const pcl::PointCloud<PointSource>& source,
                                 const pcl::Indices& source_indices,
                                 const pcl::PointCloud<PointTarget>& target,
                                 const pcl::Indices& target_indices)
{
  pcl::registration::TransformationEstimationSVD<PointSource, PointTarget, float> estimator(true);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, source_indices, target, target_indices, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicCorrespondencesUmeyama(const pcl::PointCloud<PointSource>& source,
                                     const pcl::PointCloud<PointTarget>& target,
                                     const pcl::Correspondences& correspondences)
{
  pcl::registration::TransformationEstimationSVD<PointSource, PointTarget, float> estimator(true);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, transform);
  return transform;
}

inline Eigen::Matrix4f
solveUmeyamaNoScaleFromAccumulation(const FusedAccumulation& acc)
{
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  if (acc.count == 0)
    return transform;

  const float inv_n = 1.0f / static_cast<float>(acc.count);
  const Eigen::Vector3f source_mean(acc.source_sum[0] * inv_n,
                                    acc.source_sum[1] * inv_n,
                                    acc.source_sum[2] * inv_n);
  const Eigen::Vector3f target_mean(acc.target_sum[0] * inv_n,
                                    acc.target_sum[1] * inv_n,
                                    acc.target_sum[2] * inv_n);
  Eigen::Matrix3f sigma;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      sigma(r, c) = acc.target_source_sum[r * 3 + c] * inv_n -
                    target_mean[r] * source_mean[c];
    }
  }

  Eigen::JacobiSVD<Eigen::Matrix3f> svd(sigma, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Vector3f signs = Eigen::Vector3f::Ones();
  if (svd.matrixU().determinant() * svd.matrixV().determinant() < 0.0f)
    signs[2] = -1.0f;
  const Eigen::Matrix3f rotation = svd.matrixU() * signs.asDiagonal() *
                                   svd.matrixV().transpose();
  transform.template topLeftCorner<3, 3>() = rotation;
  transform.template block<3, 1>(0, 3) = target_mean - rotation * source_mean;
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedStd(const pcl::PointCloud<PointSource>& source,
                 const pcl::PointCloud<PointTarget>& target,
                 CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
    stats->used_fallback = true;
  }
  return solveUmeyamaNoScaleFromAccumulation(accumulateFusedStd(source, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedCandidate(const pcl::PointCloud<PointSource>& source,
                       const pcl::PointCloud<PointTarget>& target,
                       CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
  }

  if (source.size() != target.size() || source.empty()) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source.is_dense && target.is_dense && source.size() >= 16) {
      if (stats) {
        stats->layout_supported = true;
        stats->used_rvv = true;
      }
      return solveUmeyamaNoScaleFromAccumulation(accumulateFusedRVV(source, target));
    }
    if (stats)
      stats->layout_supported = true;
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateFusedStd(source, target, nullptr);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedSourceIndexedStd(const pcl::PointCloud<PointSource>& source,
                              const pcl::Indices& indices,
                              const pcl::PointCloud<PointTarget>& target,
                              CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = indices.size();
    stats->accepted_points = indices.size() == target.size() &&
                                     sourceIndicesInRange(source, indices)
                                 ? indices.size()
                                 : 0;
    stats->used_fallback = true;
  }
  return solveUmeyamaNoScaleFromAccumulation(
      accumulateFusedSourceIndexedStd(source, indices, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFusedSourceIndexedCandidate(const pcl::PointCloud<PointSource>& source,
                                    const pcl::Indices& indices,
                                    const pcl::PointCloud<PointTarget>& target,
                                    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = indices.size();
    stats->accepted_points = indices.size() == target.size() &&
                                     sourceIndicesInRange(source, indices)
                                 ? indices.size()
                                 : 0;
  }

  if (indices.size() != target.size() || indices.empty() ||
      !sourceIndicesInRange(source, indices)) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source.is_dense && target.is_dense && indices.size() >= 16 &&
        sourceIndicesFitRVVGather(source, indices)) {
      if (stats) {
        stats->layout_supported = true;
        stats->used_rvv = true;
      }
      return solveUmeyamaNoScaleFromAccumulation(
          accumulateFusedSourceIndexedRVV(source, indices, target));
    }
    if (stats)
      stats->layout_supported = true;
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateFusedSourceIndexedStd(source, indices, target, nullptr);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicUmeyama(const pcl::PointCloud<PointSource>& source,
                      const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimationSVD<PointSource, PointTarget, float> estimator(true);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicSourceIndexedUmeyama(const pcl::PointCloud<PointSource>& source,
                                   const pcl::Indices& indices,
                                   const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimationSVD<PointSource, PointTarget, float> estimator(true);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, indices, target, transform);
  return transform;
}

inline float
matrixMaxAbsDiff(const Eigen::Matrix4f& a, const Eigen::Matrix4f& b)
{
  return (a - b).cwiseAbs().maxCoeff();
}

inline std::uint64_t
matrixChecksum(const Eigen::Matrix4f& matrix)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      const auto scaled = static_cast<std::int64_t>(matrix(r, c) * 1000000.0f);
      checksum = (checksum ^ static_cast<std::uint64_t>(scaled)) * 1099511628211ull;
    }
  }
  return checksum;
}

} // namespace pcl::registration::rvv_tesvd_support
