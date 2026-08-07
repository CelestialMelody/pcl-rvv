/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的 full-cloud candidate、block baseline 和
 * fused formula layout-gated 入口。它只服务 test-rvv 证据，不证明其它 row source 的
 * production dispatch。
 */

#pragma once

#include "teptplw_reductions.hpp"
#include "teptplw_row_sources.hpp"

#include <pcl/correspondence.h>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>


namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

inline NormalEquation
accumulate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                          const pcl::PointCloud<pcl::PointNormal>& target,
                          const std::vector<float>& weights,
                          AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(std::min(source.size(), target.size()), weights.size());
#ifdef __RVV10__
  // 全云顺序扫描路径（full-cloud data flow）：第 i 个 source、target 和 weights[i]
  // 直接组成同一行法方程贡献。点字段仍是 PointNormal 的 AoS（结构数组）布局，
  // 所以这里用跨步加载读取 x/y/z 和 normal 字段，用连续加载读取权重。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    NormalEquation eq;
    const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          src_base + i * sizeof(pcl::PointNormal), vl, sx, sy, sz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, dx, dy, dz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, normal_x, normal_y, normal_z);
      const vfloat32m2_t weight = __riscv_vle32_v_f32m2(weights.data() + i, vl);

      vbool16_t keep = finite_mask_f32m2(sx, vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_x, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_y, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_z, vl), vl);

      vfloat32m2_t a, b, c, d, nx, ny, nz;
      staged_weighted_formula(sx,
                              sy,
                              sz,
                              dx,
                              dy,
                              dz,
                              normal_x,
                              normal_y,
                              normal_z,
                              weight,
                              vl,
                              a,
                              b,
                              c,
                              d,
                              nx,
                              ny,
                              nz);
      accumulate_staged_rows(a, b, c, d, nx, ny, nz, keep, vl, eq);
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
  return accumulate_std_full(source, target, weights, stats);
}

// block-reduction diagnostic 只用于 weighted full-cloud A/B：它保留 RowSourcePolicy、
// WeightPolicy、finite mask 和 non-fused lane formula，改用 A/B/C/N 四组长期 vector
// partial sums + vfredosum，来隔离 vcompress + fixed buffer + tail 的影响。
inline NormalEquation
accumulate_candidate_full_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(std::min(source.size(), target.size()), weights.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    NormalEquation eq;
    const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t begin = 0; begin < n; begin += block_rows) {
      const std::size_t end = std::min(n, begin + block_rows);
      accumulate_full_block_reduction_group_a(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulate_full_block_reduction_group_b(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulate_full_block_reduction_group_c(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulate_full_block_reduction_group_n(
          source_base, target_base, weights.data(), begin, end, eq);
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, weights, stats);
}

template <WeightedFusedFormulaMode Mode>
inline NormalEquation
accumulate_candidate_full_block_fused_formula(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(std::min(source.size(), target.size()), weights.size());
#ifdef __RVV10__
  // fused formula candidates 只替换 full-cloud block-reduction 内的 a/b/c/d
  // 逐点公式树；RowSourcePolicy、连续 weights、finite mask、A/B/C/N 分组、
  // vfredosum 和 solver 边界都保持与当前 block baseline 一致。
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    NormalEquation eq;
    const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t begin = 0; begin < n; begin += block_rows) {
      const std::size_t end = std::min(n, begin + block_rows);
      accumulate_full_block_fused_formula_group_a<Mode>(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulate_full_block_fused_formula_group_b<Mode>(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulate_full_block_fused_formula_group_c<Mode>(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulate_full_block_fused_formula_group_n<Mode>(
          source_base, target_base, weights.data(), begin, end, eq);
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, weights, stats);
}

template <WeightedFusedFormulaMode Mode, typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_formula_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>;

  const std::size_t n = source.size();
#ifdef __RVV10__
  // layout-gated fused formula candidate（布局门控融合公式候选）和 production
  // full-cloud gate 对齐：source 只要求 xyz f32 AoS，target 要求 xyz+normal f32 AoS，
  // weights 必须连续且等长；不满足时回到同一标量 reference。
  if constexpr (SrcLayout::value && TgtLayout::value) {
    if (target.size() == n && weights.size() == n && n >= 64 &&
        __riscv_vsetvlmax_e32m1() <= 64 &&
        n <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() &&
        n <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>()) {
      constexpr std::size_t kBlockChunks = 8;
      const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
      const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
      NormalEquation eq;
      const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
      const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
      for (std::size_t begin = 0; begin < n; begin += block_rows) {
        const std::size_t end = std::min(n, begin + block_rows);
        accumulate_full_block_fused_formula_group_a<
            Mode,
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(source_base, target_base, weights.data(), begin, end, eq);
        accumulate_full_block_fused_formula_group_b<
            Mode,
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(source_base, target_base, weights.data(), begin, end, eq);
        accumulate_full_block_fused_formula_group_c<
            Mode,
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(source_base, target_base, weights.data(), begin, end, eq);
        accumulate_full_block_fused_formula_group_n<
            Mode,
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(source_base, target_base, weights.data(), begin, end, eq);
      }
      if (stats) {
        stats->input_points = n;
        stats->accepted_points = eq.accepted_points;
        stats->used_rvv = true;
      }
      return eq;
    }
  }
#endif // __RVV10__
  return accumulate_std_full(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_reduction_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::BlockBaseline>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_abc(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::AbcFused>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_abc_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::AbcFused>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_abc_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::AbcFusedIlp>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_abc_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_abc_ilp_layout_gated(
      source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_d_six_term(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula<
      WeightedFusedFormulaMode::DSixTermFma>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_d_six_term_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::DSixTermFma>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_d_six_term_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::DSixTermFmaIlp>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_d_six_term_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula<
      WeightedFusedFormulaMode::DSixTermFmaIlp>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_d_displacement(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula<
      WeightedFusedFormulaMode::DDisplacementFused>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_d_displacement_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::DDisplacementFused>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_d_displacement_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::DDisplacementFusedIlp>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_d_displacement_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula<
      WeightedFusedFormulaMode::DDisplacementFusedIlp>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_abcd(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula<
      WeightedFusedFormulaMode::AbcdFused>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_abcd_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::AbcdFused>(source, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_candidate_full_block_fused_abcd_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula_layout_gated<
      WeightedFusedFormulaMode::AbcdFusedIlp>(source, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_full_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_full_block_fused_formula<
      WeightedFusedFormulaMode::AbcdFusedIlp>(source, target, weights, stats);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
