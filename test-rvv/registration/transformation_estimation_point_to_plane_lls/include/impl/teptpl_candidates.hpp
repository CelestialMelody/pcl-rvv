/*
 * 本文件做什么：
 * 这是 TEPTPL test-rvv support 的 candidate wrapper（候选入口包装）层。测试和 bench
 * 通过这里调用标量 reference、full-cloud candidate、trusted-dense、indexed/dual/correspondences
 * diagnostic/probing 以及各类 reduction candidate，再统一进入 matrix solve。
 *
 * 证据边界：
 * wrapper 只把 test-support normal-equation 接到 4x4 输出，方便测试和 bench 复用。
 * 这些 direct helper 是 A/B、historical baseline 和 probing 入口；production dispatch
 * 证据来自 production-facing tests、asm attribution 和 production-dispatch board 5-run。
 */

#pragma once

#include "teptpl_reductions.hpp"
#include "teptpl_row_sources.hpp"

#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_support {

// fused-reduction diagnostic（融合规约诊断）不经过 vcompress/buffer/scalar lane
// tail。它改变跨 lane 累加树，只用于评估后续优化方向，不代表 production 语义。
inline NormalEquation
accumulate_candidate_full_fused_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(source.size(), target.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
    vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
    vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero;
    vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero;
    vfloat32m1_t nxnx = zero, nxny = zero, nxnz = zero;
    vfloat32m1_t nyny = zero, nynz = zero, nznz = zero;
    vfloat32m1_t ad = zero, bd = zero, cd = zero, nxd = zero, nyd = zero, nzd = zero;
    std::size_t accepted_points = 0;
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m1(n - i);
      accumulate_full_fused_reduction_chunk(source_base,
                                            target_base,
                                            i,
                                            vl,
                                            aa,
                                            ab,
                                            ac,
                                            anx,
                                            any,
                                            anz,
                                            bb,
                                            bc,
                                            bnx,
                                            bny,
                                            bnz,
                                            cc,
                                            cnx,
                                            cny,
                                            cnz,
                                            nxnx,
                                            nxny,
                                            nxnz,
                                            nyny,
                                            nynz,
                                            nznz,
                                            ad,
                                            bd,
                                            cd,
                                            nxd,
                                            nyd,
                                            nzd,
                                            accepted_points);
      i += vl;
    }

    NormalEquation eq;
    eq.ata.coeffRef(0) = reduce_sum_f32m1(aa, vlmax);
    eq.ata.coeffRef(1) = reduce_sum_f32m1(ab, vlmax);
    eq.ata.coeffRef(2) = reduce_sum_f32m1(ac, vlmax);
    eq.ata.coeffRef(3) = reduce_sum_f32m1(anx, vlmax);
    eq.ata.coeffRef(4) = reduce_sum_f32m1(any, vlmax);
    eq.ata.coeffRef(5) = reduce_sum_f32m1(anz, vlmax);
    eq.ata.coeffRef(7) = reduce_sum_f32m1(bb, vlmax);
    eq.ata.coeffRef(8) = reduce_sum_f32m1(bc, vlmax);
    eq.ata.coeffRef(9) = reduce_sum_f32m1(bnx, vlmax);
    eq.ata.coeffRef(10) = reduce_sum_f32m1(bny, vlmax);
    eq.ata.coeffRef(11) = reduce_sum_f32m1(bnz, vlmax);
    eq.ata.coeffRef(14) = reduce_sum_f32m1(cc, vlmax);
    eq.ata.coeffRef(15) = reduce_sum_f32m1(cnx, vlmax);
    eq.ata.coeffRef(16) = reduce_sum_f32m1(cny, vlmax);
    eq.ata.coeffRef(17) = reduce_sum_f32m1(cnz, vlmax);
    eq.ata.coeffRef(21) = reduce_sum_f32m1(nxnx, vlmax);
    eq.ata.coeffRef(22) = reduce_sum_f32m1(nxny, vlmax);
    eq.ata.coeffRef(23) = reduce_sum_f32m1(nxnz, vlmax);
    eq.ata.coeffRef(28) = reduce_sum_f32m1(nyny, vlmax);
    eq.ata.coeffRef(29) = reduce_sum_f32m1(nynz, vlmax);
    eq.ata.coeffRef(35) = reduce_sum_f32m1(nznz, vlmax);
    eq.atb.coeffRef(0) = reduce_sum_f32m1(ad, vlmax);
    eq.atb.coeffRef(1) = reduce_sum_f32m1(bd, vlmax);
    eq.atb.coeffRef(2) = reduce_sum_f32m1(cd, vlmax);
    eq.atb.coeffRef(3) = reduce_sum_f32m1(nxd, vlmax);
    eq.atb.coeffRef(4) = reduce_sum_f32m1(nyd, vlmax);
    eq.atb.coeffRef(5) = reduce_sum_f32m1(nzd, vlmax);
    eq.accepted_points = accepted_points;
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, stats);
}

// grouped-reduction diagnostic（分组/分块规约诊断）把 27 个 normal-equation
// 项按 chunk 局部规约后写回标量矩阵，避免 27 个 vector accumulator 长期活跃。
inline NormalEquation
accumulate_candidate_full_grouped_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(source.size(), target.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    NormalEquation eq;
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m1(n - i);
      accumulate_full_grouped_reduction_chunk(source_base, target_base, i, vl, eq);
      i += vl;
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, stats);
}

// block-reduction diagnostic（分块规约诊断）位于 fused 27 accumulator 和
// chunk-local grouped 27 reductions 之间。它把 27 个 normal-equation 项拆成
// a/b/c/normal 四组，每组只持有 5-9 个 vector partial sums，并在一个 row block
// 内跨多个 VL chunk 累加后规约。它降低寄存器压力和 spill 风险，但会重复读取同一
// block 并重复计算公式；它仍改变 reduction tree。
//
// 当前 production patch 复用了同一数学组织，但只在 exact PointNormal/float/full-cloud
// gate 下进入真实 public overload。这个 diagnostic helper 本身不能证明 dispatch、
// fallback、indexed/correspondences 或 Scalar=double。
inline NormalEquation
accumulate_candidate_full_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(source.size(), target.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    NormalEquation eq;
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t begin = 0; begin < n; begin += block_rows) {
      const std::size_t end = std::min(n, begin + block_rows);
      accumulate_full_block_reduction_group_a(source_base, target_base, begin, end, eq);
      accumulate_full_block_reduction_group_b(source_base, target_base, begin, end, eq);
      accumulate_full_block_reduction_group_c(source_base, target_base, begin, end, eq);
      accumulate_full_block_reduction_group_n(source_base, target_base, begin, end, eq);
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, stats);
}

// fused-formula block variant（逐点公式融合的分块规约变体）保留 block A/B/C/N
// reduction 组织，只替换 load_full_reduction_vectors 内的 a/b/c/d 公式。它用于
// current block vs fused-formula block direct A/B 和 historical baseline。当前 production
// 默认也使用同一 fused 公式树；test_support direct helper 本身不证明 production dispatch。
inline NormalEquation
accumulate_candidate_full_block_fused_formula_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(source.size(), target.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    NormalEquation eq;
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t begin = 0; begin < n; begin += block_rows) {
      const std::size_t end = std::min(n, begin + block_rows);
      accumulate_full_block_fused_formula_group_a(
          source_base, target_base, begin, end, eq);
      accumulate_full_block_fused_formula_group_b(
          source_base, target_base, begin, end, eq);
      accumulate_full_block_fused_formula_group_c(
          source_base, target_base, begin, end, eq);
      accumulate_full_block_fused_formula_group_n(
          source_base, target_base, begin, end, eq);
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, stats);
}

// 全云 candidate 用 stride load 覆盖连续 PointNormal；未命中 gate 时自然回到标量参考链路。
inline NormalEquation
accumulate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                          const pcl::PointCloud<pcl::PointNormal>& target,
                          AccumulationStats* stats = nullptr)
{
  return accumulate_row_source<FullCloudRowSource>(
      FullCloudRowSource::Context{source, target}, stats);
}

inline NormalEquation
accumulate_candidate_full_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return accumulate_row_source_trusted_dense<FullCloudRowSource>(
      FullCloudRowSource::Context{source, target}, stats);
}

// source-indexed candidate 用同一 shared math pipeline，只替换前半段 row source。
// source index stream 必须是有效索引；本诊断不改变 production 的非法 index 行为。
inline NormalEquation
accumulate_candidate_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                    const pcl::Indices& source_indices,
                                    const pcl::PointCloud<pcl::PointNormal>& target,
                                    AccumulationStats* stats = nullptr)
{
  return accumulate_row_source<SourceIndexedRowSource>(
      SourceIndexedRowSource::Context{source, source_indices, target}, stats);
}

inline NormalEquation
accumulate_candidate_source_indices_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return accumulate_row_source_trusted_dense<SourceIndexedRowSource>(
      SourceIndexedRowSource::Context{source, source_indices, target}, stats);
}

// dual-indices candidate 使用两条独立 index stream，不经过 correspondence 解析。
// 两条 index stream 都按 valid-index-only 审查；它用于把双侧 gather 成本和
// correspondences 的 query/match 展开成本拆开，而不是扩展 production 行为。
inline NormalEquation
accumulate_candidate_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<pcl::PointNormal>& target,
                                  const pcl::Indices& target_indices,
                                  AccumulationStats* stats = nullptr)
{
  return accumulate_row_source<DualIndexedRowSource>(
      DualIndexedRowSource::Context{source, source_indices, target, target_indices}, stats);
}

inline NormalEquation
accumulate_candidate_dual_indices_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    AccumulationStats* stats = nullptr)
{
  return accumulate_row_source_trusted_dense<DualIndexedRowSource>(
      DualIndexedRowSource::Context{source, source_indices, target, target_indices}, stats);
}

// correspondences candidate 先把 index_query/index_match 展开成 valid-index-only 的
// 双侧 indices，再复用 DualIndexedRowSource。这里的防御性过滤只服务诊断样本归一化；
// 不能推出生产 correspondences overload 会接受或跳过同样的非法 index。
inline NormalEquation
accumulate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                     const pcl::PointCloud<pcl::PointNormal>& target,
                                     const pcl::Correspondences& correspondences,
                                     AccumulationStats* stats = nullptr)
{
  pcl::Indices source_indices;
  pcl::Indices target_indices;
  source_indices.reserve(correspondences.size());
  target_indices.reserve(correspondences.size());
  for (const auto& correspondence : correspondences) {
    if (correspondence.index_query < 0 || correspondence.index_match < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(correspondence.index_query);
    const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    source_indices.push_back(static_cast<int>(src_index));
    target_indices.push_back(static_cast<int>(tgt_index));
  }
  if (stats)
    stats->input_points = correspondences.size();
  if (source_indices.empty()) {
    if (stats) {
      stats->accepted_points = 0;
      stats->used_rvv = false;
    }
    return NormalEquation{};
  }
  NormalEquation eq = accumulate_row_source<DualIndexedRowSource>(
      DualIndexedRowSource::Context{source, source_indices, target, target_indices}, stats);
  if (stats)
    stats->input_points = correspondences.size();
  return eq;
}

inline NormalEquation
accumulate_candidate_correspondences_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
  pcl::Indices source_indices;
  pcl::Indices target_indices;
  source_indices.reserve(correspondences.size());
  target_indices.reserve(correspondences.size());
  for (const auto& correspondence : correspondences) {
    if (correspondence.index_query < 0 || correspondence.index_match < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(correspondence.index_query);
    const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    source_indices.push_back(static_cast<int>(src_index));
    target_indices.push_back(static_cast<int>(tgt_index));
  }
  if (stats)
    stats->input_points = correspondences.size();
  if (source_indices.empty()) {
    if (stats) {
      stats->accepted_points = 0;
      stats->used_rvv = false;
    }
    return NormalEquation{};
  }
  NormalEquation eq = accumulate_row_source_trusted_dense<DualIndexedRowSource>(
      DualIndexedRowSource::Context{source, source_indices, target, target_indices}, stats);
  if (stats)
    stats->input_points = correspondences.size();
  return eq;
}

// 下面这些 estimate_* helper 是测试和 bench 的入口层，负责把 normal-equation
// 诊断连接到 4x4 输出；每个 wrapper 对应一条公开入口数据流。
inline Matrix4f
estimate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                  const pcl::PointCloud<pcl::PointNormal>& target,
                  AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_std_full(source, target, stats));
}

inline Matrix4f
estimate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                        const pcl::PointCloud<pcl::PointNormal>& target,
                        AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full(source, target, stats));
}

inline Matrix4f
estimate_candidate_full_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_trusted_dense(source, target, stats));
}

inline Matrix4f
estimate_candidate_full_fused_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_fused_reduction(source, target, stats));
}

inline Matrix4f
estimate_candidate_full_grouped_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_grouped_reduction(source, target, stats));
}

inline Matrix4f
estimate_candidate_full_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_reduction(source, target, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_formula_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_formula_reduction(source, target, stats));
}

inline Matrix4f
estimate_std_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<pcl::PointNormal>& target,
                            AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_std_source_indices(source, source_indices, target, stats));
}

inline Matrix4f
estimate_candidate_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<pcl::PointNormal>& target,
                                  AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_source_indices(source, source_indices, target, stats));
}

inline Matrix4f
estimate_candidate_source_indices_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_source_indices_trusted_dense(
          source, source_indices, target, stats));
}

inline Matrix4f
estimate_std_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                          const pcl::Indices& source_indices,
                          const pcl::PointCloud<pcl::PointNormal>& target,
                          const pcl::Indices& target_indices,
                          AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_std_dual_indices(source, source_indices, target, target_indices, stats));
}

inline Matrix4f
estimate_candidate_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                const pcl::Indices& source_indices,
                                const pcl::PointCloud<pcl::PointNormal>& target,
                                const pcl::Indices& target_indices,
                                AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_dual_indices(
      source, source_indices, target, target_indices, stats));
}

inline Matrix4f
estimate_candidate_dual_indices_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_dual_indices_trusted_dense(
      source, source_indices, target, target_indices, stats));
}

inline Matrix4f
estimate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                             const pcl::PointCloud<pcl::PointNormal>& target,
                             const pcl::Correspondences& correspondences,
                             AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_std_correspondences(source, target, correspondences, stats));
}

inline Matrix4f
estimate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                   const pcl::PointCloud<pcl::PointNormal>& target,
                                   const pcl::Correspondences& correspondences,
                                   AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_correspondences(source, target, correspondences, stats));
}

inline Matrix4f
estimate_candidate_correspondences_trusted_dense(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_correspondences_trusted_dense(
          source, target, correspondences, stats));
}

// checksum（校验和）只用于 bench 日志对齐，不作为数值正确性的唯一证据。
inline double
matrix_checksum(const Matrix4f& matrix)
{
  double checksum = 0.0;
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      checksum += static_cast<double>(matrix(row, col)) * (1.0 + row * 4 + col);
  return checksum;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_support
