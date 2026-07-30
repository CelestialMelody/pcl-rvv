/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的 RVV shared math pipeline、vcompress tail
 * 和 full-cloud block-reduction A/B helper。
 */

#pragma once

#include "teptplw_common.hpp"

#include <pcl/rvv_point_load.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

#ifdef __RVV10__
inline vbool16_t
finite_mask_f32m2(vfloat32m2_t value, const std::size_t vl)
{
  const vfloat32m2_t abs_value = __riscv_vfabs_v_f32m2(value, vl);
  return __riscv_vmfle_vf_f32m2_b16(abs_value, std::numeric_limits<float>::max(), vl);
}

inline vbool32_t
finite_mask_f32m1(vfloat32m1_t value, const std::size_t vl)
{
  const vfloat32m1_t abs_value = __riscv_vfabs_v_f32m1(value, vl);
  return __riscv_vmfle_vf_f32m1_b32(abs_value, std::numeric_limits<float>::max(), vl);
}

inline double
reduce_sum_f32m1(vfloat32m1_t value, const std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return static_cast<double>(
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredosum_vs_f32m1_f32m1(value, zero, vl)));
}

inline void
accumulate_staged_rows(vfloat32m2_t a,
                       vfloat32m2_t b,
                       vfloat32m2_t c,
                       vfloat32m2_t d,
                       vfloat32m2_t nx,
                       vfloat32m2_t ny,
                       vfloat32m2_t nz,
                       vbool16_t keep,
                       const std::size_t vl,
                       NormalEquation& eq)
{
  // fixed buffer gate（固定缓冲验收条件）由调用方用 vlmax <= 64 保证。
  // 源码层面这里按 vcompress（保序压缩）后的顺序消费 buffer；反汇编显示 -O3 会把
  // 部分 buffer 累加自动变成 vector reduction（向量规约），这是本轮保留给 reviewer
  // 审查的语义和性能边界，不是 production（生产源码）承诺。
  alignas(16) float a_buf[64];
  alignas(16) float b_buf[64];
  alignas(16) float c_buf[64];
  alignas(16) float d_buf[64];
  alignas(16) float nx_buf[64];
  alignas(16) float ny_buf[64];
  alignas(16) float nz_buf[64];
  const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
  if (kept == 0)
    return;

  __riscv_vse32_v_f32m2(a_buf, __riscv_vcompress_vm_f32m2(a, keep, vl), kept);
  __riscv_vse32_v_f32m2(b_buf, __riscv_vcompress_vm_f32m2(b, keep, vl), kept);
  __riscv_vse32_v_f32m2(c_buf, __riscv_vcompress_vm_f32m2(c, keep, vl), kept);
  __riscv_vse32_v_f32m2(d_buf, __riscv_vcompress_vm_f32m2(d, keep, vl), kept);
  __riscv_vse32_v_f32m2(nx_buf, __riscv_vcompress_vm_f32m2(nx, keep, vl), kept);
  __riscv_vse32_v_f32m2(ny_buf, __riscv_vcompress_vm_f32m2(ny, keep, vl), kept);
  __riscv_vse32_v_f32m2(nz_buf, __riscv_vcompress_vm_f32m2(nz, keep, vl), kept);

  for (std::size_t lane = 0; lane < kept; ++lane) {
    const double a_d = a_buf[lane];
    const double b_d = b_buf[lane];
    const double c_d = c_buf[lane];
    const double nx_d = nx_buf[lane];
    const double ny_d = ny_buf[lane];
    const double nz_d = nz_buf[lane];
    const double d_d = d_buf[lane];

    eq.ata.coeffRef(0) += a_d * a_d;
    eq.ata.coeffRef(1) += a_d * b_d;
    eq.ata.coeffRef(2) += a_d * c_d;
    eq.ata.coeffRef(3) += a_d * nx_d;
    eq.ata.coeffRef(4) += a_d * ny_d;
    eq.ata.coeffRef(5) += a_d * nz_d;
    eq.ata.coeffRef(7) += b_d * b_d;
    eq.ata.coeffRef(8) += b_d * c_d;
    eq.ata.coeffRef(9) += b_d * nx_d;
    eq.ata.coeffRef(10) += b_d * ny_d;
    eq.ata.coeffRef(11) += b_d * nz_d;
    eq.ata.coeffRef(14) += c_d * c_d;
    eq.ata.coeffRef(15) += c_d * nx_d;
    eq.ata.coeffRef(16) += c_d * ny_d;
    eq.ata.coeffRef(17) += c_d * nz_d;
    eq.ata.coeffRef(21) += nx_d * nx_d;
    eq.ata.coeffRef(22) += nx_d * ny_d;
    eq.ata.coeffRef(23) += nx_d * nz_d;
    eq.ata.coeffRef(28) += ny_d * ny_d;
    eq.ata.coeffRef(29) += ny_d * nz_d;
    eq.ata.coeffRef(35) += nz_d * nz_d;

    eq.atb.coeffRef(0) += a_d * d_d;
    eq.atb.coeffRef(1) += b_d * d_d;
    eq.atb.coeffRef(2) += c_d * d_d;
    eq.atb.coeffRef(3) += nx_d * d_d;
    eq.atb.coeffRef(4) += ny_d * d_d;
    eq.atb.coeffRef(5) += nz_d * d_d;
    ++eq.accepted_points;
  }
}

inline void
staged_weighted_formula(vfloat32m2_t sx,
                        vfloat32m2_t sy,
                        vfloat32m2_t sz,
                        vfloat32m2_t dx,
                        vfloat32m2_t dy,
                        vfloat32m2_t dz,
                        vfloat32m2_t normal_x,
                        vfloat32m2_t normal_y,
                        vfloat32m2_t normal_z,
                        vfloat32m2_t weight,
                        const std::size_t vl,
                        vfloat32m2_t& a,
                        vfloat32m2_t& b,
                        vfloat32m2_t& c,
                        vfloat32m2_t& d,
                        vfloat32m2_t& nx,
                        vfloat32m2_t& ny,
                        vfloat32m2_t& nz)
{
  // 当前候选显式保持 vfmul + vfadd/vfsub 结构，暂缓 fused multiply-add
  //（融合乘加）intrinsic，便于审计 weight * normal 之后的逐点公式。
  nx = __riscv_vfmul_vv_f32m2(normal_x, weight, vl);
  ny = __riscv_vfmul_vv_f32m2(normal_y, weight, vl);
  nz = __riscv_vfmul_vv_f32m2(normal_z, weight, vl);

  a = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nz, sy, vl),
                             __riscv_vfmul_vv_f32m2(ny, sz, vl),
                             vl);
  b = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nx, sz, vl),
                             __riscv_vfmul_vv_f32m2(nz, sx, vl),
                             vl);
  c = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(ny, sx, vl),
                             __riscv_vfmul_vv_f32m2(nx, sy, vl),
                             vl);

  d = __riscv_vfmul_vv_f32m2(nx, dx, vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(ny, dy, vl), vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(nz, dz, vl), vl);
  d = __riscv_vfsub_vv_f32m2(d, __riscv_vfmul_vv_f32m2(nx, sx, vl), vl);
  d = __riscv_vfsub_vv_f32m2(d, __riscv_vfmul_vv_f32m2(ny, sy, vl), vl);
  d = __riscv_vfsub_vv_f32m2(d, __riscv_vfmul_vv_f32m2(nz, sz, vl), vl);
}

inline void
load_full_block_reduction_vectors(const std::uint8_t* source_base,
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
  // weighted full-cloud block loader：按 full-cloud row 顺序读取点，连续读取
  // weights[k]，只把点和 normal 的 finite mask 用于 lane 置零，不检查 weight。
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
  const vfloat32m1_t normal_x = load(target_base, kNX);
  const vfloat32m1_t normal_y = load(target_base, kNY);
  const vfloat32m1_t normal_z = load(target_base, kNZ);
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

  nx = __riscv_vfmul_vv_f32m1(normal_x, weight, vl);
  ny = __riscv_vfmul_vv_f32m1(normal_y, weight, vl);
  nz = __riscv_vfmul_vv_f32m1(normal_z, weight, vl);

  a = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(nz, sy, vl),
                             __riscv_vfmul_vv_f32m1(ny, sz, vl),
                             vl);
  b = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(nx, sz, vl),
                             __riscv_vfmul_vv_f32m1(nz, sx, vl),
                             vl);
  c = __riscv_vfsub_vv_f32m1(__riscv_vfmul_vv_f32m1(ny, sx, vl),
                             __riscv_vfmul_vv_f32m1(nx, sy, vl),
                             vl);
  d = __riscv_vfmul_vv_f32m1(nx, dx, vl);
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
}

inline void
accumulate_full_block_reduction_group_a(const std::uint8_t* source_base,
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
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_block_reduction_vectors(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
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
                                        const float* weights,
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
    load_full_block_reduction_vectors(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
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
                                        const float* weights,
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
    load_full_block_reduction_vectors(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
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
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    load_full_block_reduction_vectors(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
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

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
