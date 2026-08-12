/*
 * 本文件做什么：
 * dual-indices（双索引路径）和 correspondences（对应关系索引路径）的
 * block-reduction / fused-abcd-ilp 实现族诊断。它把 query/match 或双 index
 * 展开成 source/target index pair，再复用 full-cloud adopted family 的 A/B/C/N
 * block groups。这里仍是 test-rvv 诊断代码，不能证明 production dispatch。
 */

#pragma once

#include "teptplw_reductions.hpp"
#include "teptplw_row_sources.hpp"

#include <pcl/correspondence.h>
#include <pcl/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

inline bool
prepare_valid_index_pairs_u32(const pcl::PointCloud<pcl::PointNormal>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<pcl::PointNormal>& target,
                              const pcl::Indices& target_indices,
                              const std::size_t n,
                              std::vector<std::uint32_t>& prepared_source,
                              std::vector<std::uint32_t>& prepared_target)
{
  prepared_source.clear();
  prepared_target.clear();
  prepared_source.reserve(n);
  prepared_target.reserve(n);
  for (std::size_t k = 0; k < n; ++k) {
    if (source_indices[k] < 0 || target_indices[k] < 0)
      return false;
    const auto src_index = static_cast<std::size_t>(source_indices[k]);
    const auto tgt_index = static_cast<std::size_t>(target_indices[k]);
    if (src_index >= source.size() || tgt_index >= target.size())
      return false;
    prepared_source.push_back(static_cast<std::uint32_t>(src_index));
    prepared_target.push_back(static_cast<std::uint32_t>(tgt_index));
  }
  return true;
}

inline void
prepare_valid_correspondence_rows_u32(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    std::vector<std::uint32_t>& prepared_source,
    std::vector<std::uint32_t>& prepared_target,
    std::vector<float>& prepared_weights)
{
  prepared_source.clear();
  prepared_target.clear();
  prepared_weights.clear();
  prepared_source.reserve(correspondences.size());
  prepared_target.reserve(correspondences.size());
  prepared_weights.reserve(correspondences.size());
  for (const auto& correspondence : correspondences) {
    if (correspondence.index_query < 0 || correspondence.index_match < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(correspondence.index_query);
    const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    prepared_source.push_back(static_cast<std::uint32_t>(src_index));
    prepared_target.push_back(static_cast<std::uint32_t>(tgt_index));
    prepared_weights.push_back(correspondence.weight);
  }
}

#ifdef __RVV10__
template <WeightedFusedFormulaMode Mode>
inline void
load_index_pair_block_fused_vectors(const std::uint8_t* source_base,
                                    const std::uint32_t* source_indices,
                                    const std::uint8_t* target_base,
                                    const std::uint32_t* target_indices,
                                    const float* weights,
                                    const std::size_t i,
                                    const std::size_t vl,
                                    vfloat32m1_t& a,
                                    vfloat32m1_t& b,
                                    vfloat32m1_t& c,
                                    vfloat32m1_t& d,
                                    vfloat32m1_t& nx,
                                    vfloat32m1_t& ny,
                                    vfloat32m1_t& nz,
                                    vbool32_t& keep)
{
  // index-pair family：source 和 target 都由索引流 gather；formula、
  // finite mask、A/B/C/N block-reduction 和 solver 边界保持同族。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);

  const vuint32m1_t source_index_vector =
      __riscv_vle32_v_u32m1(source_indices + i, vl);
  const vuint32m1_t target_index_vector =
      __riscv_vle32_v_u32m1(target_indices + i, vl);
  const vuint32m1_t source_offsets =
      __riscv_vmul_vx_u32m1(source_index_vector, sizeof(pcl::PointNormal), vl);
  const vuint32m1_t target_offsets =
      __riscv_vmul_vx_u32m1(target_index_vector, sizeof(pcl::PointNormal), vl);

  const auto gather_source = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vluxei32_v_f32m1(
        reinterpret_cast<const float*>(source_base + offset), source_offsets, vl);
  };
  const auto gather_target = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vluxei32_v_f32m1(
        reinterpret_cast<const float*>(target_base + offset), target_offsets, vl);
  };

  const vfloat32m1_t sx = gather_source(kX);
  const vfloat32m1_t sy = gather_source(kY);
  const vfloat32m1_t sz = gather_source(kZ);
  const vfloat32m1_t dx = gather_target(kX);
  const vfloat32m1_t dy = gather_target(kY);
  const vfloat32m1_t dz = gather_target(kZ);
  const vfloat32m1_t normal_x = gather_target(kNX);
  const vfloat32m1_t normal_y = gather_target(kNY);
  const vfloat32m1_t normal_z = gather_target(kNZ);
  const vfloat32m1_t weight = __riscv_vle32_v_f32m1(weights + i, vl);

  keep = finite_mask_f32m1(sx, vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(normal_x, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(normal_y, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(normal_z, vl), vl);

  weighted_fused_formula_m1<Mode>(sx,
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

  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  a = __riscv_vmerge_vvm_f32m1(zero, a, keep, vl);
  b = __riscv_vmerge_vvm_f32m1(zero, b, keep, vl);
  c = __riscv_vmerge_vvm_f32m1(zero, c, keep, vl);
  d = __riscv_vmerge_vvm_f32m1(zero, d, keep, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, keep, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, keep, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, keep, vl);
}

template <WeightedFusedFormulaMode Mode, typename AccumulateFn>
inline void
for_each_index_pair_block_vector(const std::uint8_t* source_base,
                                 const std::uint32_t* source_indices,
                                 const std::uint8_t* target_base,
                                 const std::uint32_t* target_indices,
                                 const float* weights,
                                 const std::size_t begin,
                                 const std::size_t end,
                                 AccumulateFn&& accumulate)
{
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_index_pair_block_fused_vectors<Mode>(
        source_base,
        source_indices,
        target_base,
        target_indices,
        weights,
        i,
        vl,
        a,
        b,
        c,
        d,
        nx,
        ny,
        nz,
        keep);
    accumulate(a, b, c, d, nx, ny, nz, keep, vl);
    i += vl;
  }
}

template <WeightedFusedFormulaMode Mode>
inline void
accumulate_index_pair_block_fused_formula_group_a(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const std::uint32_t* target_indices,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
  vfloat32m1_t ad = zero;
  for_each_index_pair_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
      target_indices,
      weights,
      begin,
      end,
      [&](vfloat32m1_t a,
          vfloat32m1_t b,
          vfloat32m1_t c,
          vfloat32m1_t d,
          vfloat32m1_t nx,
          vfloat32m1_t ny,
          vfloat32m1_t nz,
          vbool32_t keep,
          std::size_t vl) {
        eq.accepted_points += __riscv_vcpop_m_b32(keep, vl);
        aa = __riscv_vfmacc_vv_f32m1_tu(aa, a, a, vl);
        ab = __riscv_vfmacc_vv_f32m1_tu(ab, a, b, vl);
        ac = __riscv_vfmacc_vv_f32m1_tu(ac, a, c, vl);
        anx = __riscv_vfmacc_vv_f32m1_tu(anx, a, nx, vl);
        any = __riscv_vfmacc_vv_f32m1_tu(any, a, ny, vl);
        anz = __riscv_vfmacc_vv_f32m1_tu(anz, a, nz, vl);
        ad = __riscv_vfmacc_vv_f32m1_tu(ad, a, d, vl);
      });
  eq.ata.coeffRef(0) += reduce_sum_f32m1(aa, vlmax);
  eq.ata.coeffRef(1) += reduce_sum_f32m1(ab, vlmax);
  eq.ata.coeffRef(2) += reduce_sum_f32m1(ac, vlmax);
  eq.ata.coeffRef(3) += reduce_sum_f32m1(anx, vlmax);
  eq.ata.coeffRef(4) += reduce_sum_f32m1(any, vlmax);
  eq.ata.coeffRef(5) += reduce_sum_f32m1(anz, vlmax);
  eq.atb.coeffRef(0) += reduce_sum_f32m1(ad, vlmax);
}

template <WeightedFusedFormulaMode Mode>
inline void
accumulate_index_pair_block_fused_formula_group_b(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const std::uint32_t* target_indices,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero, bd = zero;
  for_each_index_pair_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
      target_indices,
      weights,
      begin,
      end,
      [&](vfloat32m1_t,
          vfloat32m1_t b,
          vfloat32m1_t c,
          vfloat32m1_t d,
          vfloat32m1_t nx,
          vfloat32m1_t ny,
          vfloat32m1_t nz,
          vbool32_t,
          std::size_t vl) {
        bb = __riscv_vfmacc_vv_f32m1_tu(bb, b, b, vl);
        bc = __riscv_vfmacc_vv_f32m1_tu(bc, b, c, vl);
        bnx = __riscv_vfmacc_vv_f32m1_tu(bnx, b, nx, vl);
        bny = __riscv_vfmacc_vv_f32m1_tu(bny, b, ny, vl);
        bnz = __riscv_vfmacc_vv_f32m1_tu(bnz, b, nz, vl);
        bd = __riscv_vfmacc_vv_f32m1_tu(bd, b, d, vl);
      });
  eq.ata.coeffRef(7) += reduce_sum_f32m1(bb, vlmax);
  eq.ata.coeffRef(8) += reduce_sum_f32m1(bc, vlmax);
  eq.ata.coeffRef(9) += reduce_sum_f32m1(bnx, vlmax);
  eq.ata.coeffRef(10) += reduce_sum_f32m1(bny, vlmax);
  eq.ata.coeffRef(11) += reduce_sum_f32m1(bnz, vlmax);
  eq.atb.coeffRef(1) += reduce_sum_f32m1(bd, vlmax);
}

template <WeightedFusedFormulaMode Mode>
inline void
accumulate_index_pair_block_fused_formula_group_c(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const std::uint32_t* target_indices,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero, cd = zero;
  for_each_index_pair_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
      target_indices,
      weights,
      begin,
      end,
      [&](vfloat32m1_t,
          vfloat32m1_t,
          vfloat32m1_t c,
          vfloat32m1_t d,
          vfloat32m1_t nx,
          vfloat32m1_t ny,
          vfloat32m1_t nz,
          vbool32_t,
          std::size_t vl) {
        cc = __riscv_vfmacc_vv_f32m1_tu(cc, c, c, vl);
        cnx = __riscv_vfmacc_vv_f32m1_tu(cnx, c, nx, vl);
        cny = __riscv_vfmacc_vv_f32m1_tu(cny, c, ny, vl);
        cnz = __riscv_vfmacc_vv_f32m1_tu(cnz, c, nz, vl);
        cd = __riscv_vfmacc_vv_f32m1_tu(cd, c, d, vl);
      });
  eq.ata.coeffRef(14) += reduce_sum_f32m1(cc, vlmax);
  eq.ata.coeffRef(15) += reduce_sum_f32m1(cnx, vlmax);
  eq.ata.coeffRef(16) += reduce_sum_f32m1(cny, vlmax);
  eq.ata.coeffRef(17) += reduce_sum_f32m1(cnz, vlmax);
  eq.atb.coeffRef(2) += reduce_sum_f32m1(cd, vlmax);
}

template <WeightedFusedFormulaMode Mode>
inline void
accumulate_index_pair_block_fused_formula_group_n(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const std::uint32_t* target_indices,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t nxnx = zero, nxny = zero, nxnz = zero;
  vfloat32m1_t nyny = zero, nynz = zero, nznz = zero;
  vfloat32m1_t nxd = zero, nyd = zero, nzd = zero;
  for_each_index_pair_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
      target_indices,
      weights,
      begin,
      end,
      [&](vfloat32m1_t,
          vfloat32m1_t,
          vfloat32m1_t,
          vfloat32m1_t d,
          vfloat32m1_t nx,
          vfloat32m1_t ny,
          vfloat32m1_t nz,
          vbool32_t,
          std::size_t vl) {
        nxnx = __riscv_vfmacc_vv_f32m1_tu(nxnx, nx, nx, vl);
        nxny = __riscv_vfmacc_vv_f32m1_tu(nxny, nx, ny, vl);
        nxnz = __riscv_vfmacc_vv_f32m1_tu(nxnz, nx, nz, vl);
        nyny = __riscv_vfmacc_vv_f32m1_tu(nyny, ny, ny, vl);
        nynz = __riscv_vfmacc_vv_f32m1_tu(nynz, ny, nz, vl);
        nznz = __riscv_vfmacc_vv_f32m1_tu(nznz, nz, nz, vl);
        nxd = __riscv_vfmacc_vv_f32m1_tu(nxd, nx, d, vl);
        nyd = __riscv_vfmacc_vv_f32m1_tu(nyd, ny, d, vl);
        nzd = __riscv_vfmacc_vv_f32m1_tu(nzd, nz, d, vl);
      });
  eq.ata.coeffRef(21) += reduce_sum_f32m1(nxnx, vlmax);
  eq.ata.coeffRef(22) += reduce_sum_f32m1(nxny, vlmax);
  eq.ata.coeffRef(23) += reduce_sum_f32m1(nxnz, vlmax);
  eq.ata.coeffRef(28) += reduce_sum_f32m1(nyny, vlmax);
  eq.ata.coeffRef(29) += reduce_sum_f32m1(nynz, vlmax);
  eq.ata.coeffRef(35) += reduce_sum_f32m1(nznz, vlmax);
  eq.atb.coeffRef(3) += reduce_sum_f32m1(nxd, vlmax);
  eq.atb.coeffRef(4) += reduce_sum_f32m1(nyd, vlmax);
  eq.atb.coeffRef(5) += reduce_sum_f32m1(nzd, vlmax);
}

template <WeightedFusedFormulaMode Mode>
inline NormalEquation
accumulate_prepared_index_pair_block_fused_formula(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const std::vector<std::uint32_t>& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<std::uint32_t>& target_indices,
    const std::vector<float>& weights,
    const std::size_t input_points_for_stats,
    AccumulationStats* stats)
{
  constexpr std::size_t kBlockChunks = 8;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
  const std::size_t n =
      std::min(std::min(source_indices.size(), target_indices.size()), weights.size());
  NormalEquation eq;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
  for (std::size_t begin = 0; begin < n; begin += block_rows) {
    const std::size_t end = std::min(n, begin + block_rows);
    accumulate_index_pair_block_fused_formula_group_a<Mode>(
        source_base,
        source_indices.data(),
        target_base,
        target_indices.data(),
        weights.data(),
        begin,
        end,
        eq);
    accumulate_index_pair_block_fused_formula_group_b<Mode>(
        source_base,
        source_indices.data(),
        target_base,
        target_indices.data(),
        weights.data(),
        begin,
        end,
        eq);
    accumulate_index_pair_block_fused_formula_group_c<Mode>(
        source_base,
        source_indices.data(),
        target_base,
        target_indices.data(),
        weights.data(),
        begin,
        end,
        eq);
    accumulate_index_pair_block_fused_formula_group_n<Mode>(
        source_base,
        source_indices.data(),
        target_base,
        target_indices.data(),
        weights.data(),
        begin,
        end,
        eq);
  }
  if (stats) {
    stats->input_points = input_points_for_stats;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = true;
  }
  return eq;
}
#endif // __RVV10__

template <WeightedFusedFormulaMode Mode>
inline NormalEquation
accumulate_candidate_dual_indices_block_fused_formula(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(
      std::min(source_indices.size(), target_indices.size()), weights.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      target.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    std::vector<std::uint32_t> tgt_indices;
    if (!prepare_valid_index_pairs_u32(
            source, source_indices, target, target_indices, n, src_indices, tgt_indices))
      return accumulate_std_dual_indices(
          source, source_indices, target, target_indices, weights, stats);
    return accumulate_prepared_index_pair_block_fused_formula<Mode>(
        source, src_indices, target, tgt_indices, weights, n, stats);
  }
#endif // __RVV10__
  return accumulate_std_dual_indices(
      source, source_indices, target, target_indices, weights, stats);
}

inline NormalEquation
accumulate_candidate_dual_indices_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_dual_indices_block_fused_formula<
      WeightedFusedFormulaMode::BlockBaseline>(
      source, source_indices, target, target_indices, weights, stats);
}

inline NormalEquation
accumulate_candidate_dual_indices_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_dual_indices_block_fused_formula<
      WeightedFusedFormulaMode::AbcdFusedIlp>(
      source, source_indices, target, target_indices, weights, stats);
}

template <WeightedFusedFormulaMode Mode>
inline NormalEquation
accumulate_candidate_correspondences_block_fused_formula(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
#ifdef __RVV10__
  if (correspondences.size() >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      target.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    std::vector<std::uint32_t> tgt_indices;
    std::vector<float> expanded_weights;
    prepare_valid_correspondence_rows_u32(
        source, target, correspondences, src_indices, tgt_indices, expanded_weights);
    if (src_indices.size() >= 64)
      return accumulate_prepared_index_pair_block_fused_formula<Mode>(
          source,
          src_indices,
          target,
          tgt_indices,
          expanded_weights,
          correspondences.size(),
          stats);
  }
#endif // __RVV10__
  return accumulate_std_correspondences(source, target, correspondences, stats);
}

inline NormalEquation
accumulate_candidate_correspondences_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_correspondences_block_fused_formula<
      WeightedFusedFormulaMode::BlockBaseline>(source, target, correspondences, stats);
}

inline NormalEquation
accumulate_candidate_correspondences_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_correspondences_block_fused_formula<
      WeightedFusedFormulaMode::AbcdFusedIlp>(source, target, correspondences, stats);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
