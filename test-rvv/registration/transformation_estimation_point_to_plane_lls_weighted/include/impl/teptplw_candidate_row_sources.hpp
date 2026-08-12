/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的 source-indexed、dual-indices 和
 * correspondences candidate 入口。它们是 row source 诊断证据，不代表 production
 * 已批准这些入口进入 RVV dispatch。
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

inline bool
prepare_valid_source_indices_u32(const pcl::PointCloud<pcl::PointNormal>& source,
                                 const pcl::Indices& source_indices,
                                 const std::size_t n,
                                 std::vector<std::uint32_t>& prepared)
{
  prepared.clear();
  prepared.reserve(n);
  for (std::size_t k = 0; k < n; ++k) {
    if (source_indices[k] < 0)
      return false;
    const auto src_index = static_cast<std::size_t>(source_indices[k]);
    if (src_index >= source.size())
      return false;
    prepared.push_back(static_cast<std::uint32_t>(src_index));
  }
  return true;
}

inline NormalEquation
accumulate_candidate_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                    const pcl::Indices& source_indices,
                                    const pcl::PointCloud<pcl::PointNormal>& target,
                                    const std::vector<float>& weights,
                                    AccumulationStats* stats = nullptr)
{
  const std::size_t n =
      std::min(std::min(source_indices.size(), target.size()), weights.size());
#ifdef __RVV10__
  // source-indexed 路径：source 侧由 index stream 决定，所以用 gather 读取；
  // target 和 weights 仍按 row k 连续前进，分别用 stride load 和 contiguous load。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    if (!prepare_valid_source_indices_u32(source, source_indices, n, src_indices))
      return accumulate_std_source_indices(source, source_indices, target, weights, stats);

    NormalEquation eq;
    const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vuint32m2_t v_src_idx =
          __riscv_vle32_v_u32m2(src_indices.data() + i, vl);
      const vuint32m2_t src_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_src_idx, vl);

      vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          src_base, src_offsets, vl, sx, sy, sz);
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
  return accumulate_std_source_indices(source, source_indices, target, weights, stats);
}

#ifdef __RVV10__
template <WeightedFusedFormulaMode Mode>
inline void
load_source_indexed_block_reduction_fused_vectors(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
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
  // source-indexed block/fused family：只把 full-cloud 的 source stride load
  // 替换成 source index gather；target normal、连续 weight、finite mask 和
  // block-reduction 规约边界保持同族，便于和 staged-gather/compressed-tail A/B。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  constexpr std::ptrdiff_t kTargetStride = sizeof(pcl::PointNormal);

  const vuint32m1_t source_index_vector =
      __riscv_vle32_v_u32m1(source_indices + i, vl);
  const vuint32m1_t source_offsets =
      __riscv_vmul_vx_u32m1(source_index_vector, sizeof(pcl::PointNormal), vl);

  const auto gather_source = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vluxei32_v_f32m1(
        reinterpret_cast<const float*>(source_base + offset), source_offsets, vl);
  };
  const auto load_target = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(target_base + i * sizeof(pcl::PointNormal) + offset),
        kTargetStride,
        vl);
  };

  const vfloat32m1_t sx = gather_source(kX);
  const vfloat32m1_t sy = gather_source(kY);
  const vfloat32m1_t sz = gather_source(kZ);
  const vfloat32m1_t dx = load_target(kX);
  const vfloat32m1_t dy = load_target(kY);
  const vfloat32m1_t dz = load_target(kZ);
  const vfloat32m1_t normal_x = load_target(kNX);
  const vfloat32m1_t normal_y = load_target(kNY);
  const vfloat32m1_t normal_z = load_target(kNZ);
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
for_each_source_indexed_block_vector(const std::uint8_t* source_base,
                                     const std::uint32_t* source_indices,
                                     const std::uint8_t* target_base,
                                     const float* weights,
                                     const std::size_t begin,
                                     const std::size_t end,
                                     AccumulateFn&& accumulate)
{
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_source_indexed_block_reduction_fused_vectors<Mode>(
        source_base,
        source_indices,
        target_base,
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
accumulate_source_indexed_block_fused_formula_group_a(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
  vfloat32m1_t ad = zero;
  for_each_source_indexed_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
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
accumulate_source_indexed_block_fused_formula_group_b(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero, bd = zero;
  for_each_source_indexed_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
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
accumulate_source_indexed_block_fused_formula_group_c(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero, cd = zero;
  for_each_source_indexed_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
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
accumulate_source_indexed_block_fused_formula_group_n(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
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
  for_each_source_indexed_block_vector<Mode>(
      source_base,
      source_indices,
      target_base,
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
#endif // __RVV10__

template <WeightedFusedFormulaMode Mode>
inline NormalEquation
accumulate_candidate_source_indices_block_fused_formula(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  const std::size_t n =
      std::min(std::min(source_indices.size(), target.size()), weights.size());
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m1() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    if (!prepare_valid_source_indices_u32(source, source_indices, n, src_indices))
      return accumulate_std_source_indices(source, source_indices, target, weights, stats);

    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows = std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    NormalEquation eq;
    const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t begin = 0; begin < n; begin += block_rows) {
      const std::size_t end = std::min(n, begin + block_rows);
      accumulate_source_indexed_block_fused_formula_group_a<Mode>(
          src_base, src_indices.data(), tgt_base, weights.data(), begin, end, eq);
      accumulate_source_indexed_block_fused_formula_group_b<Mode>(
          src_base, src_indices.data(), tgt_base, weights.data(), begin, end, eq);
      accumulate_source_indexed_block_fused_formula_group_c<Mode>(
          src_base, src_indices.data(), tgt_base, weights.data(), begin, end, eq);
      accumulate_source_indexed_block_fused_formula_group_n<Mode>(
          src_base, src_indices.data(), tgt_base, weights.data(), begin, end, eq);
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_source_indices(source, source_indices, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_source_indices_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_source_indices_block_fused_formula<
      WeightedFusedFormulaMode::BlockBaseline>(
      source, source_indices, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_source_indices_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return accumulate_candidate_source_indices_block_fused_formula<
      WeightedFusedFormulaMode::AbcdFusedIlp>(
      source, source_indices, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<pcl::PointNormal>& target,
                                  const pcl::Indices& target_indices,
                                  const std::vector<float>& weights,
                                  AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(
      std::min(source_indices.size(), target_indices.size()), weights.size());
#ifdef __RVV10__
  // dual-indices 路径：source/target 两侧都用独立 index stream gather，weight 仍按
  // row 序号连续读取。它专门把“双侧 gather”与 correspondences 的 query/match/weight
  // 展开成本分开审查。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      target.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    std::vector<std::uint32_t> tgt_indices;
    src_indices.reserve(n);
    tgt_indices.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
      if (source_indices[k] < 0 || target_indices[k] < 0)
        return accumulate_std_dual_indices(
            source, source_indices, target, target_indices, weights, stats);
      const auto src_index = static_cast<std::size_t>(source_indices[k]);
      const auto tgt_index = static_cast<std::size_t>(target_indices[k]);
      if (src_index >= source.size() || tgt_index >= target.size())
        return accumulate_std_dual_indices(
            source, source_indices, target, target_indices, weights, stats);
      src_indices.push_back(static_cast<std::uint32_t>(src_index));
      tgt_indices.push_back(static_cast<std::uint32_t>(tgt_index));
    }

    NormalEquation eq;
    const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vuint32m2_t v_src_idx =
          __riscv_vle32_v_u32m2(src_indices.data() + i, vl);
      const vuint32m2_t v_tgt_idx =
          __riscv_vle32_v_u32m2(tgt_indices.data() + i, vl);
      const vuint32m2_t src_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_src_idx, vl);
      const vuint32m2_t tgt_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_tgt_idx, vl);

      vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          src_base, src_offsets, vl, sx, sy, sz);
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          tgt_base, tgt_offsets, vl, dx, dy, dz);
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
          tgt_base, tgt_offsets, vl, normal_x, normal_y, normal_z);
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
  return accumulate_std_dual_indices(
      source, source_indices, target, target_indices, weights, stats);
}

inline NormalEquation
accumulate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                     const pcl::PointCloud<pcl::PointNormal>& target,
                                     const pcl::Correspondences& correspondences,
                                     AccumulationStats* stats = nullptr)
{
#ifdef __RVV10__
  // 对应关系索引路径（correspondences data flow）：production 入口通过
  // ConstCloudIterator 隐藏 index_query/index_match 的间接访问；RVV intrinsic 需要
  // 明确的索引数组才能 gather（离散加载）点字段，所以诊断先标量展开 index/weight。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (correspondences.size() >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      target.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    std::vector<std::uint32_t> tgt_indices;
    std::vector<float> weights;
    src_indices.reserve(correspondences.size());
    tgt_indices.reserve(correspondences.size());
    weights.reserve(correspondences.size());
    for (const auto& correspondence : correspondences) {
      if (correspondence.index_query < 0 || correspondence.index_match < 0)
        continue;
      const auto src_index = static_cast<std::size_t>(correspondence.index_query);
      const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
      if (src_index >= source.size() || tgt_index >= target.size())
        continue;
      src_indices.push_back(static_cast<std::uint32_t>(src_index));
      tgt_indices.push_back(static_cast<std::uint32_t>(tgt_index));
      weights.push_back(correspondence.weight);
    }
    if (src_indices.size() >= 64) {
      NormalEquation eq;
      const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
      const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
      for (std::size_t i = 0; i < src_indices.size();) {
        const std::size_t vl = __riscv_vsetvl_e32m2(src_indices.size() - i);
        const vuint32m2_t v_src_idx =
            __riscv_vle32_v_u32m2(src_indices.data() + i, vl);
        const vuint32m2_t v_tgt_idx =
            __riscv_vle32_v_u32m2(tgt_indices.data() + i, vl);
        const vuint32m2_t src_offsets =
            pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_src_idx, vl);
        const vuint32m2_t tgt_offsets =
            pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_tgt_idx, vl);

        vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            src_base, src_offsets, vl, sx, sy, sz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            tgt_base, tgt_offsets, vl, dx, dy, dz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
            tgt_base, tgt_offsets, vl, normal_x, normal_y, normal_z);
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
        stats->input_points = correspondences.size();
        stats->accepted_points = eq.accepted_points;
        stats->used_rvv = true;
      }
      return eq;
    }
  }
#endif // __RVV10__
  return accumulate_std_correspondences(source, target, correspondences, stats);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
