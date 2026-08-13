/*
 * 本文件做什么：
 * 这是 TEPTPL test-rvv support 的 reduction candidates（规约候选）层，保存 fused、
 * grouped 和 block reduction helper。它们用于对比 normal-equation ATA/ATb 的不同规约
 * 组织，解释当前生产候选为什么选择 block-reduction。
 *
 * 证据边界：
 * 这些 helper 属于 test-rvv support。它们可以保护 finite mask、accepted_points、
 * A/B/C/N 分组和 reduction-tree 误差预算，但不能单独证明 public overload dispatch、
 * fallback、indexed/correspondences 或 Scalar=double。
 */

#pragma once

#include "teptpl_rvv_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace pcl::registration::rvv_te_pt2plane_lls_support {

#ifdef __RVV10__
inline void
accumulate_full_fused_reduction_chunk(const std::uint8_t* source_base,
                                      const std::uint8_t* target_base,
                                      const std::size_t i,
                                      const std::size_t vl,
                                      vfloat32m1_t& aa,
                                      vfloat32m1_t& ab,
                                      vfloat32m1_t& ac,
                                      vfloat32m1_t& anx,
                                      vfloat32m1_t& any,
                                      vfloat32m1_t& anz,
                                      vfloat32m1_t& bb,
                                      vfloat32m1_t& bc,
                                      vfloat32m1_t& bnx,
                                      vfloat32m1_t& bny,
                                      vfloat32m1_t& bnz,
                                      vfloat32m1_t& cc,
                                      vfloat32m1_t& cnx,
                                      vfloat32m1_t& cny,
                                      vfloat32m1_t& cnz,
                                      vfloat32m1_t& nxnx,
                                      vfloat32m1_t& nxny,
                                      vfloat32m1_t& nxnz,
                                      vfloat32m1_t& nyny,
                                      vfloat32m1_t& nynz,
                                      vfloat32m1_t& nznz,
                                      vfloat32m1_t& ad,
                                      vfloat32m1_t& bd,
                                      vfloat32m1_t& cd,
                                      vfloat32m1_t& nxd,
                                      vfloat32m1_t& nyd,
                                      vfloat32m1_t& nzd,
                                      std::size_t& accepted_points)
{
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  constexpr std::ptrdiff_t kStride = sizeof(pcl::PointNormal);

  const auto load = [&](const std::uint8_t* base,
                        const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(base + i * sizeof(pcl::PointNormal) + offset),
        kStride,
        vl);
  };

  const vfloat32m1_t sx = load(source_base, kX);
  const vfloat32m1_t sy = load(source_base, kY);
  const vfloat32m1_t sz = load(source_base, kZ);
  const vfloat32m1_t dx = load(target_base, kX);
  const vfloat32m1_t dy = load(target_base, kY);
  const vfloat32m1_t dz = load(target_base, kZ);
  vfloat32m1_t nx = load(target_base, kNX);
  vfloat32m1_t ny = load(target_base, kNY);
  vfloat32m1_t nz = load(target_base, kNZ);

  vbool32_t keep = finite_mask_f32m1(sx, vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(nx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(ny, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(nz, vl), vl);

  vfloat32m1_t a = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(nz, sy, vl),
                                           __riscv_vfmul_vv_f32m1(ny, sz, vl),
                                           vl);
  vfloat32m1_t b = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(nx, sz, vl),
                                           __riscv_vfmul_vv_f32m1(nz, sx, vl),
                                           vl);
  vfloat32m1_t c = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(ny, sx, vl),
                                           __riscv_vfmul_vv_f32m1(nx, sy, vl),
                                           vl);
  vfloat32m1_t d = __riscv_vfmul_vv_f32m1(nx, dx, vl);
  d = __riscv_vfadd_vv_f32m1(d, __riscv_vfmul_vv_f32m1(ny, dy, vl), vl);
  d = __riscv_vfadd_vv_f32m1(d, __riscv_vfmul_vv_f32m1(nz, dz, vl), vl);
  d = __riscv_vfsub_vv_f32m1(d, __riscv_vfmul_vv_f32m1(nx, sx, vl), vl);
  d = __riscv_vfsub_vv_f32m1(d, __riscv_vfmul_vv_f32m1(ny, sy, vl), vl);
  d = __riscv_vfsub_vv_f32m1(d, __riscv_vfmul_vv_f32m1(nz, sz, vl), vl);

  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  a = __riscv_vmerge_vvm_f32m1(zero, a, keep, vl);
  b = __riscv_vmerge_vvm_f32m1(zero, b, keep, vl);
  c = __riscv_vmerge_vvm_f32m1(zero, c, keep, vl);
  d = __riscv_vmerge_vvm_f32m1(zero, d, keep, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, keep, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, keep, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, keep, vl);
  accepted_points += __riscv_vcpop_m_b32(keep, vl);

  aa = __riscv_vfmacc_vv_f32m1_tu(aa, a, a, vl);
  ab = __riscv_vfmacc_vv_f32m1_tu(ab, a, b, vl);
  ac = __riscv_vfmacc_vv_f32m1_tu(ac, a, c, vl);
  anx = __riscv_vfmacc_vv_f32m1_tu(anx, a, nx, vl);
  any = __riscv_vfmacc_vv_f32m1_tu(any, a, ny, vl);
  anz = __riscv_vfmacc_vv_f32m1_tu(anz, a, nz, vl);
  bb = __riscv_vfmacc_vv_f32m1_tu(bb, b, b, vl);
  bc = __riscv_vfmacc_vv_f32m1_tu(bc, b, c, vl);
  bnx = __riscv_vfmacc_vv_f32m1_tu(bnx, b, nx, vl);
  bny = __riscv_vfmacc_vv_f32m1_tu(bny, b, ny, vl);
  bnz = __riscv_vfmacc_vv_f32m1_tu(bnz, b, nz, vl);
  cc = __riscv_vfmacc_vv_f32m1_tu(cc, c, c, vl);
  cnx = __riscv_vfmacc_vv_f32m1_tu(cnx, c, nx, vl);
  cny = __riscv_vfmacc_vv_f32m1_tu(cny, c, ny, vl);
  cnz = __riscv_vfmacc_vv_f32m1_tu(cnz, c, nz, vl);
  nxnx = __riscv_vfmacc_vv_f32m1_tu(nxnx, nx, nx, vl);
  nxny = __riscv_vfmacc_vv_f32m1_tu(nxny, nx, ny, vl);
  nxnz = __riscv_vfmacc_vv_f32m1_tu(nxnz, nx, nz, vl);
  nyny = __riscv_vfmacc_vv_f32m1_tu(nyny, ny, ny, vl);
  nynz = __riscv_vfmacc_vv_f32m1_tu(nynz, ny, nz, vl);
  nznz = __riscv_vfmacc_vv_f32m1_tu(nznz, nz, nz, vl);
  ad = __riscv_vfmacc_vv_f32m1_tu(ad, a, d, vl);
  bd = __riscv_vfmacc_vv_f32m1_tu(bd, b, d, vl);
  cd = __riscv_vfmacc_vv_f32m1_tu(cd, c, d, vl);
  nxd = __riscv_vfmacc_vv_f32m1_tu(nxd, nx, d, vl);
  nyd = __riscv_vfmacc_vv_f32m1_tu(nyd, ny, d, vl);
  nzd = __riscv_vfmacc_vv_f32m1_tu(nzd, nz, d, vl);
}

inline void
accumulate_full_grouped_reduction_chunk(const std::uint8_t* source_base,
                                        const std::uint8_t* target_base,
                                        const std::size_t i,
                                        const std::size_t vl,
                                        NormalEquation& eq)
{
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  constexpr std::ptrdiff_t kStride = sizeof(pcl::PointNormal);

  const auto load = [&](const std::uint8_t* base,
                        const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(base + i * sizeof(pcl::PointNormal) + offset),
        kStride,
        vl);
  };

  const vfloat32m1_t sx = load(source_base, kX);
  const vfloat32m1_t sy = load(source_base, kY);
  const vfloat32m1_t sz = load(source_base, kZ);
  const vfloat32m1_t dx = load(target_base, kX);
  const vfloat32m1_t dy = load(target_base, kY);
  const vfloat32m1_t dz = load(target_base, kZ);
  vfloat32m1_t nx = load(target_base, kNX);
  vfloat32m1_t ny = load(target_base, kNY);
  vfloat32m1_t nz = load(target_base, kNZ);

  vbool32_t keep = finite_mask_f32m1(sx, vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(nx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(ny, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(nz, vl), vl);

  vfloat32m1_t a = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(nz, sy, vl),
                                           __riscv_vfmul_vv_f32m1(ny, sz, vl),
                                           vl);
  vfloat32m1_t b = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(nx, sz, vl),
                                           __riscv_vfmul_vv_f32m1(nz, sx, vl),
                                           vl);
  vfloat32m1_t c = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(ny, sx, vl),
                                           __riscv_vfmul_vv_f32m1(nx, sy, vl),
                                           vl);
  vfloat32m1_t d = __riscv_vfmul_vv_f32m1(nx, dx, vl);
  d = __riscv_vfadd_vv_f32m1(d, __riscv_vfmul_vv_f32m1(ny, dy, vl), vl);
  d = __riscv_vfadd_vv_f32m1(d, __riscv_vfmul_vv_f32m1(nz, dz, vl), vl);
  d = __riscv_vfsub_vv_f32m1(d, __riscv_vfmul_vv_f32m1(nx, sx, vl), vl);
  d = __riscv_vfsub_vv_f32m1(d, __riscv_vfmul_vv_f32m1(ny, sy, vl), vl);
  d = __riscv_vfsub_vv_f32m1(d, __riscv_vfmul_vv_f32m1(nz, sz, vl), vl);

  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  a = __riscv_vmerge_vvm_f32m1(zero, a, keep, vl);
  b = __riscv_vmerge_vvm_f32m1(zero, b, keep, vl);
  c = __riscv_vmerge_vvm_f32m1(zero, c, keep, vl);
  d = __riscv_vmerge_vvm_f32m1(zero, d, keep, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, keep, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, keep, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, keep, vl);
  eq.accepted_points += __riscv_vcpop_m_b32(keep, vl);

  eq.ata.coeffRef(0) += reduce_product_sum_f32m1(a, a, vl);
  eq.ata.coeffRef(1) += reduce_product_sum_f32m1(a, b, vl);
  eq.ata.coeffRef(2) += reduce_product_sum_f32m1(a, c, vl);
  eq.ata.coeffRef(3) += reduce_product_sum_f32m1(a, nx, vl);
  eq.ata.coeffRef(4) += reduce_product_sum_f32m1(a, ny, vl);
  eq.ata.coeffRef(5) += reduce_product_sum_f32m1(a, nz, vl);
  eq.ata.coeffRef(7) += reduce_product_sum_f32m1(b, b, vl);
  eq.ata.coeffRef(8) += reduce_product_sum_f32m1(b, c, vl);
  eq.ata.coeffRef(9) += reduce_product_sum_f32m1(b, nx, vl);
  eq.ata.coeffRef(10) += reduce_product_sum_f32m1(b, ny, vl);
  eq.ata.coeffRef(11) += reduce_product_sum_f32m1(b, nz, vl);
  eq.ata.coeffRef(14) += reduce_product_sum_f32m1(c, c, vl);
  eq.ata.coeffRef(15) += reduce_product_sum_f32m1(c, nx, vl);
  eq.ata.coeffRef(16) += reduce_product_sum_f32m1(c, ny, vl);
  eq.ata.coeffRef(17) += reduce_product_sum_f32m1(c, nz, vl);
  eq.ata.coeffRef(21) += reduce_product_sum_f32m1(nx, nx, vl);
  eq.ata.coeffRef(22) += reduce_product_sum_f32m1(nx, ny, vl);
  eq.ata.coeffRef(23) += reduce_product_sum_f32m1(nx, nz, vl);
  eq.ata.coeffRef(28) += reduce_product_sum_f32m1(ny, ny, vl);
  eq.ata.coeffRef(29) += reduce_product_sum_f32m1(ny, nz, vl);
  eq.ata.coeffRef(35) += reduce_product_sum_f32m1(nz, nz, vl);
  eq.atb.coeffRef(0) += reduce_product_sum_f32m1(a, d, vl);
  eq.atb.coeffRef(1) += reduce_product_sum_f32m1(b, d, vl);
  eq.atb.coeffRef(2) += reduce_product_sum_f32m1(c, d, vl);
  eq.atb.coeffRef(3) += reduce_product_sum_f32m1(nx, d, vl);
  eq.atb.coeffRef(4) += reduce_product_sum_f32m1(ny, d, vl);
  eq.atb.coeffRef(5) += reduce_product_sum_f32m1(nz, d, vl);
}

inline void
accumulate_full_block_reduction_group_a(const std::uint8_t* source_base,
                                        const std::uint8_t* target_base,
                                        const std::size_t begin,
                                        const std::size_t end,
                                        NormalEquation& eq)
{
  // A 组负责 a 相关 ATA/ATb 项，并且只在这一组统计 accepted_points。其它组复用
  // load_full_reduction_vectors 的 finite mask 置零语义，但不重复计数。
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
  vfloat32m1_t ad = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    eq.accepted_points += __riscv_vcpop_m_b32(keep, vl);
    aa = __riscv_vfmacc_vv_f32m1_tu(aa, a, a, vl);
    ab = __riscv_vfmacc_vv_f32m1_tu(ab, a, b, vl);
    ac = __riscv_vfmacc_vv_f32m1_tu(ac, a, c, vl);
    anx = __riscv_vfmacc_vv_f32m1_tu(anx, a, nx, vl);
    any = __riscv_vfmacc_vv_f32m1_tu(any, a, ny, vl);
    anz = __riscv_vfmacc_vv_f32m1_tu(anz, a, nz, vl);
    ad = __riscv_vfmacc_vv_f32m1_tu(ad, a, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(0) += reduce_sum_f32m1(aa, vlmax);
  eq.ata.coeffRef(1) += reduce_sum_f32m1(ab, vlmax);
  eq.ata.coeffRef(2) += reduce_sum_f32m1(ac, vlmax);
  eq.ata.coeffRef(3) += reduce_sum_f32m1(anx, vlmax);
  eq.ata.coeffRef(4) += reduce_sum_f32m1(any, vlmax);
  eq.ata.coeffRef(5) += reduce_sum_f32m1(anz, vlmax);
  eq.atb.coeffRef(0) += reduce_sum_f32m1(ad, vlmax);
}

inline void
accumulate_full_block_reduction_group_b(const std::uint8_t* source_base,
                                        const std::uint8_t* target_base,
                                        const std::size_t begin,
                                        const std::size_t end,
                                        NormalEquation& eq)
{
  // B 组只累加 b 相关项。它和 A/C/N 组对同一 block 重新 load/formula，换取更低
  // 的同时活跃 accumulator 数量；证据边界仍按 reduction-tree 误差预算审查。
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero, bd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    bb = __riscv_vfmacc_vv_f32m1_tu(bb, b, b, vl);
    bc = __riscv_vfmacc_vv_f32m1_tu(bc, b, c, vl);
    bnx = __riscv_vfmacc_vv_f32m1_tu(bnx, b, nx, vl);
    bny = __riscv_vfmacc_vv_f32m1_tu(bny, b, ny, vl);
    bnz = __riscv_vfmacc_vv_f32m1_tu(bnz, b, nz, vl);
    bd = __riscv_vfmacc_vv_f32m1_tu(bd, b, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(7) += reduce_sum_f32m1(bb, vlmax);
  eq.ata.coeffRef(8) += reduce_sum_f32m1(bc, vlmax);
  eq.ata.coeffRef(9) += reduce_sum_f32m1(bnx, vlmax);
  eq.ata.coeffRef(10) += reduce_sum_f32m1(bny, vlmax);
  eq.ata.coeffRef(11) += reduce_sum_f32m1(bnz, vlmax);
  eq.atb.coeffRef(1) += reduce_sum_f32m1(bd, vlmax);
}

inline void
accumulate_full_block_reduction_group_c(const std::uint8_t* source_base,
                                        const std::uint8_t* target_base,
                                        const std::size_t begin,
                                        const std::size_t end,
                                        NormalEquation& eq)
{
  // C 组只累加 c 相关项。分组后的横向 vfredosum 与标量 row-order double 累加
  // 不同，因此测试保护的是 ATA/ATb 和 matrix 的误差预算，不是 bitwise 等价。
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero, cd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    cc = __riscv_vfmacc_vv_f32m1_tu(cc, c, c, vl);
    cnx = __riscv_vfmacc_vv_f32m1_tu(cnx, c, nx, vl);
    cny = __riscv_vfmacc_vv_f32m1_tu(cny, c, ny, vl);
    cnz = __riscv_vfmacc_vv_f32m1_tu(cnz, c, nz, vl);
    cd = __riscv_vfmacc_vv_f32m1_tu(cd, c, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(14) += reduce_sum_f32m1(cc, vlmax);
  eq.ata.coeffRef(15) += reduce_sum_f32m1(cnx, vlmax);
  eq.ata.coeffRef(16) += reduce_sum_f32m1(cny, vlmax);
  eq.ata.coeffRef(17) += reduce_sum_f32m1(cnz, vlmax);
  eq.atb.coeffRef(2) += reduce_sum_f32m1(cd, vlmax);
}

inline void
accumulate_full_block_reduction_group_n(const std::uint8_t* source_base,
                                        const std::uint8_t* target_base,
                                        const std::size_t begin,
                                        const std::size_t end,
                                        NormalEquation& eq)
{
  // N 组累加 normal-only 和 normal*d 项。它保护 target normal 字段参与
  // normal-equation 的合同；不能单独证明 public overload dispatch 或 fallback。
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t nxnx = zero, nxny = zero, nxnz = zero;
  vfloat32m1_t nyny = zero, nynz = zero, nznz = zero;
  vfloat32m1_t nxd = zero, nyd = zero, nzd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    nxnx = __riscv_vfmacc_vv_f32m1_tu(nxnx, nx, nx, vl);
    nxny = __riscv_vfmacc_vv_f32m1_tu(nxny, nx, ny, vl);
    nxnz = __riscv_vfmacc_vv_f32m1_tu(nxnz, nx, nz, vl);
    nyny = __riscv_vfmacc_vv_f32m1_tu(nyny, ny, ny, vl);
    nynz = __riscv_vfmacc_vv_f32m1_tu(nynz, ny, nz, vl);
    nznz = __riscv_vfmacc_vv_f32m1_tu(nznz, nz, nz, vl);
    nxd = __riscv_vfmacc_vv_f32m1_tu(nxd, nx, d, vl);
    nyd = __riscv_vfmacc_vv_f32m1_tu(nyd, ny, d, vl);
    nzd = __riscv_vfmacc_vv_f32m1_tu(nzd, nz, d, vl);
    i += vl;
  }
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

// fused-formula variant（逐点公式融合形态）只替换 a/b/c/d 的 lane 内公式，保留
// current block-reduction 的 finite mask、invalid lane 置零、accepted_points 统计和
// A/B/C/N 分组。它是 test_support direct A/B 和 historical baseline 入口；当前
// production 默认也使用同一 fused 公式树，但 production 证据来自 production-facing
// tests、asm attribution 和 production-dispatch board 5-run。
inline void
load_full_reduction_vectors_fused_formula(const std::uint8_t* source_base,
                                          const std::uint8_t* target_base,
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
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  constexpr std::ptrdiff_t kStride = sizeof(pcl::PointNormal);

  const auto load = [&](const std::uint8_t* base,
                        const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(base + i * sizeof(pcl::PointNormal) + offset),
        kStride,
        vl);
  };

  const vfloat32m1_t sx = load(source_base, kX);
  const vfloat32m1_t sy = load(source_base, kY);
  const vfloat32m1_t sz = load(source_base, kZ);
  const vfloat32m1_t dx = load(target_base, kX);
  const vfloat32m1_t dy = load(target_base, kY);
  const vfloat32m1_t dz = load(target_base, kZ);
  nx = load(target_base, kNX);
  ny = load(target_base, kNY);
  nz = load(target_base, kNZ);

  keep = finite_mask_f32m1(sx, vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(sz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(dz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(nx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(ny, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finite_mask_f32m1(nz, vl), vl);

  a = __riscv_vfmul_vv_f32m1(ny, sz, vl);
  a = __riscv_vfmsac_vv_f32m1(a, nz, sy, vl);
  b = __riscv_vfmul_vv_f32m1(nz, sx, vl);
  b = __riscv_vfmsac_vv_f32m1(b, nx, sz, vl);
  c = __riscv_vfmul_vv_f32m1(nx, sy, vl);
  c = __riscv_vfmsac_vv_f32m1(c, ny, sx, vl);

  // d 使用 nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz) 的 fused accumulation。
  // 这和 current formula 的 target/source 分段加减树不同，必须单独看数值预算。
  const vfloat32m1_t dsx = __riscv_vfsub_vv_f32m1(dx, sx, vl);
  const vfloat32m1_t dsy = __riscv_vfsub_vv_f32m1(dy, sy, vl);
  const vfloat32m1_t dsz = __riscv_vfsub_vv_f32m1(dz, sz, vl);
  d = __riscv_vfmul_vv_f32m1(nx, dsx, vl);
  d = __riscv_vfmacc_vv_f32m1(d, ny, dsy, vl);
  d = __riscv_vfmacc_vv_f32m1(d, nz, dsz, vl);

  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  a = __riscv_vmerge_vvm_f32m1(zero, a, keep, vl);
  b = __riscv_vmerge_vvm_f32m1(zero, b, keep, vl);
  c = __riscv_vmerge_vvm_f32m1(zero, c, keep, vl);
  d = __riscv_vmerge_vvm_f32m1(zero, d, keep, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, keep, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, keep, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, keep, vl);
}

inline void
accumulate_full_block_fused_formula_group_a(const std::uint8_t* source_base,
                                            const std::uint8_t* target_base,
                                            const std::size_t begin,
                                            const std::size_t end,
                                            NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
  vfloat32m1_t ad = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors_fused_formula(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    eq.accepted_points += __riscv_vcpop_m_b32(keep, vl);
    aa = __riscv_vfmacc_vv_f32m1_tu(aa, a, a, vl);
    ab = __riscv_vfmacc_vv_f32m1_tu(ab, a, b, vl);
    ac = __riscv_vfmacc_vv_f32m1_tu(ac, a, c, vl);
    anx = __riscv_vfmacc_vv_f32m1_tu(anx, a, nx, vl);
    any = __riscv_vfmacc_vv_f32m1_tu(any, a, ny, vl);
    anz = __riscv_vfmacc_vv_f32m1_tu(anz, a, nz, vl);
    ad = __riscv_vfmacc_vv_f32m1_tu(ad, a, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(0) += reduce_sum_f32m1(aa, vlmax);
  eq.ata.coeffRef(1) += reduce_sum_f32m1(ab, vlmax);
  eq.ata.coeffRef(2) += reduce_sum_f32m1(ac, vlmax);
  eq.ata.coeffRef(3) += reduce_sum_f32m1(anx, vlmax);
  eq.ata.coeffRef(4) += reduce_sum_f32m1(any, vlmax);
  eq.ata.coeffRef(5) += reduce_sum_f32m1(anz, vlmax);
  eq.atb.coeffRef(0) += reduce_sum_f32m1(ad, vlmax);
}

inline void
accumulate_full_block_fused_formula_group_b(const std::uint8_t* source_base,
                                            const std::uint8_t* target_base,
                                            const std::size_t begin,
                                            const std::size_t end,
                                            NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero, bd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors_fused_formula(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    bb = __riscv_vfmacc_vv_f32m1_tu(bb, b, b, vl);
    bc = __riscv_vfmacc_vv_f32m1_tu(bc, b, c, vl);
    bnx = __riscv_vfmacc_vv_f32m1_tu(bnx, b, nx, vl);
    bny = __riscv_vfmacc_vv_f32m1_tu(bny, b, ny, vl);
    bnz = __riscv_vfmacc_vv_f32m1_tu(bnz, b, nz, vl);
    bd = __riscv_vfmacc_vv_f32m1_tu(bd, b, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(7) += reduce_sum_f32m1(bb, vlmax);
  eq.ata.coeffRef(8) += reduce_sum_f32m1(bc, vlmax);
  eq.ata.coeffRef(9) += reduce_sum_f32m1(bnx, vlmax);
  eq.ata.coeffRef(10) += reduce_sum_f32m1(bny, vlmax);
  eq.ata.coeffRef(11) += reduce_sum_f32m1(bnz, vlmax);
  eq.atb.coeffRef(1) += reduce_sum_f32m1(bd, vlmax);
}

inline void
accumulate_full_block_fused_formula_group_c(const std::uint8_t* source_base,
                                            const std::uint8_t* target_base,
                                            const std::size_t begin,
                                            const std::size_t end,
                                            NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero, cd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors_fused_formula(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    cc = __riscv_vfmacc_vv_f32m1_tu(cc, c, c, vl);
    cnx = __riscv_vfmacc_vv_f32m1_tu(cnx, c, nx, vl);
    cny = __riscv_vfmacc_vv_f32m1_tu(cny, c, ny, vl);
    cnz = __riscv_vfmacc_vv_f32m1_tu(cnz, c, nz, vl);
    cd = __riscv_vfmacc_vv_f32m1_tu(cd, c, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(14) += reduce_sum_f32m1(cc, vlmax);
  eq.ata.coeffRef(15) += reduce_sum_f32m1(cnx, vlmax);
  eq.ata.coeffRef(16) += reduce_sum_f32m1(cny, vlmax);
  eq.ata.coeffRef(17) += reduce_sum_f32m1(cnz, vlmax);
  eq.atb.coeffRef(2) += reduce_sum_f32m1(cd, vlmax);
}

inline void
accumulate_full_block_fused_formula_group_n(const std::uint8_t* source_base,
                                            const std::uint8_t* target_base,
                                            const std::size_t begin,
                                            const std::size_t end,
                                            NormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t nxnx = zero, nxny = zero, nxnz = zero;
  vfloat32m1_t nyny = zero, nynz = zero, nznz = zero;
  vfloat32m1_t nxd = zero, nyd = zero, nzd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_reduction_vectors_fused_formula(
        source_base, target_base, i, vl, a, b, c, d, nx, ny, nz, keep);
    nxnx = __riscv_vfmacc_vv_f32m1_tu(nxnx, nx, nx, vl);
    nxny = __riscv_vfmacc_vv_f32m1_tu(nxny, nx, ny, vl);
    nxnz = __riscv_vfmacc_vv_f32m1_tu(nxnz, nx, nz, vl);
    nyny = __riscv_vfmacc_vv_f32m1_tu(nyny, ny, ny, vl);
    nynz = __riscv_vfmacc_vv_f32m1_tu(nynz, ny, nz, vl);
    nznz = __riscv_vfmacc_vv_f32m1_tu(nznz, nz, nz, vl);
    nxd = __riscv_vfmacc_vv_f32m1_tu(nxd, nx, d, vl);
    nyd = __riscv_vfmacc_vv_f32m1_tu(nyd, ny, d, vl);
    nzd = __riscv_vfmacc_vv_f32m1_tu(nzd, nz, d, vl);
    i += vl;
  }
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

} // namespace pcl::registration::rvv_te_pt2plane_lls_support
