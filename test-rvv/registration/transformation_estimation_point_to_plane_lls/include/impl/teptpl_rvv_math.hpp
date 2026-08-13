/*
 * 本文件做什么：
 * 这是 TEPTPL test-rvv support 的 RVV math（RVV 数学流水线）层，保存 finite mask、
 * staged formula（分阶段逐点公式）、shared math pipeline（共享数学流水线）和 full-cloud
 * reduction vector loader。它把已加载的一段 row 转成 a/b/c/d、accepted_points 和 ATA/ATb
 * 所需向量。
 *
 * 证据边界：
 * 这里解释有限值掩码、invalid lane、vcompress、vfredosum 和 reduction tree（规约树）
 * 的数值边界，但它仍是 test-rvv support 资产，不能单独证明 production dispatch。
 */

#pragma once

#include "teptpl_common.hpp"

#include <pcl/rvv_point_load.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace pcl::registration::rvv_te_pt2plane_lls_support {

#ifdef __RVV10__
// RVV 没有直接的 isfinite intrinsic；这个 mask 用有限最大值比较同时排除 NaN 和 Inf。
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

// 这个函数是 RVV staging 与标量 tail 的接口；它也是当前方案性能瓶颈的主要观察点。
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
  // vcompress（保序压缩）后逐 lane 进入标量 tail，保持 normal-equation 累加顺序。
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
    accumulate_formula_values(a_buf[lane],
                              b_buf[lane],
                              c_buf[lane],
                              d_buf[lane],
                              nx_buf[lane],
                              ny_buf[lane],
                              nz_buf[lane],
                              eq);
  }
}

// trusted-dense diagnostic（可信 dense 诊断）跳过 finite mask 和 vcompress，只在调用方
// 明确选择该入口时使用。它不复刻当前 production 行为；它用于评估未来若允许依赖
// is_dense 合同，mask/compress 成本是否是值得消除的瓶颈。
inline void
accumulate_staged_rows_trusted_dense(vfloat32m2_t a,
                                     vfloat32m2_t b,
                                     vfloat32m2_t c,
                                     vfloat32m2_t d,
                                     vfloat32m2_t nx,
                                     vfloat32m2_t ny,
                                     vfloat32m2_t nz,
                                     const std::size_t vl,
                                     NormalEquation& eq)
{
  alignas(16) float a_buf[64];
  alignas(16) float b_buf[64];
  alignas(16) float c_buf[64];
  alignas(16) float d_buf[64];
  alignas(16) float nx_buf[64];
  alignas(16) float ny_buf[64];
  alignas(16) float nz_buf[64];

  __riscv_vse32_v_f32m2(a_buf, a, vl);
  __riscv_vse32_v_f32m2(b_buf, b, vl);
  __riscv_vse32_v_f32m2(c_buf, c, vl);
  __riscv_vse32_v_f32m2(d_buf, d, vl);
  __riscv_vse32_v_f32m2(nx_buf, nx, vl);
  __riscv_vse32_v_f32m2(ny_buf, ny, vl);
  __riscv_vse32_v_f32m2(nz_buf, nz, vl);

  for (std::size_t lane = 0; lane < vl; ++lane) {
    accumulate_formula_values(a_buf[lane],
                              b_buf[lane],
                              c_buf[lane],
                              d_buf[lane],
                              nx_buf[lane],
                              ny_buf[lane],
                              nz_buf[lane],
                              eq);
  }
}

// 逐点公式 staging 只改变每个 lane 内的执行形态，不做跨 lane reduction。
inline void
staged_formula(vfloat32m2_t sx,
               vfloat32m2_t sy,
               vfloat32m2_t sz,
               vfloat32m2_t dx,
               vfloat32m2_t dy,
               vfloat32m2_t dz,
               vfloat32m2_t nx,
               vfloat32m2_t ny,
               vfloat32m2_t nz,
               const std::size_t vl,
               vfloat32m2_t& a,
               vfloat32m2_t& b,
               vfloat32m2_t& c,
               vfloat32m2_t& d)
{
  // 逐点公式当前不显式使用 fused intrinsic（融合乘加内建函数），
  // 目的是让语义审计时的变量边界更容易定位；这不是 FMA 的永久禁用理由。
  // normal-equation 累加阶段另有 vfmacc 路径，未来若 fused 这里的公式需单独复核。
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

// shared math pipeline（共享数学流水线）：调用者已经决定字段来自 stride load（跨步加载）
// 还是 gather（离散加载），这里统一复刻 production 的 finite check、point-to-plane
// LLS 逐点公式和保序压缩 tail。它不关心 row 来自哪个公开 overload。
inline void
accumulate_loaded_rows(vfloat32m2_t sx,
                       vfloat32m2_t sy,
                       vfloat32m2_t sz,
                       vfloat32m2_t dx,
                       vfloat32m2_t dy,
                       vfloat32m2_t dz,
                       vfloat32m2_t nx,
                       vfloat32m2_t ny,
                       vfloat32m2_t nz,
                       const std::size_t vl,
                       NormalEquation& eq)
{
  vbool16_t keep = finite_mask_f32m2(sx, vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dx, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dy, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dz, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(nx, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(ny, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(nz, vl), vl);

  vfloat32m2_t a, b, c, d;
  staged_formula(sx, sy, sz, dx, dy, dz, nx, ny, nz, vl, a, b, c, d);
  accumulate_staged_rows(a, b, c, d, nx, ny, nz, keep, vl, eq);
}

inline void
accumulate_loaded_rows_trusted_dense(vfloat32m2_t sx,
                                     vfloat32m2_t sy,
                                     vfloat32m2_t sz,
                                     vfloat32m2_t dx,
                                     vfloat32m2_t dy,
                                     vfloat32m2_t dz,
                                     vfloat32m2_t nx,
                                     vfloat32m2_t ny,
                                     vfloat32m2_t nz,
                                     const std::size_t vl,
                                     NormalEquation& eq)
{
  vfloat32m2_t a, b, c, d;
  staged_formula(sx, sy, sz, dx, dy, dz, nx, ny, nz, vl, a, b, c, d);
  accumulate_staged_rows_trusted_dense(a, b, c, d, nx, ny, nz, vl, eq);
}

inline double
reduce_sum_f32m1(vfloat32m1_t value, const std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return static_cast<double>(
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredosum_vs_f32m1_f32m1(value, zero, vl)));
}

inline double
reduce_product_sum_f32m1(vfloat32m1_t lhs, vfloat32m1_t rhs, const std::size_t vl)
{
  return reduce_sum_f32m1(__riscv_vfmul_vv_f32m1(lhs, rhs, vl), vl);
}

// block-reduction 共享的 full-cloud lane loader（向量通道加载器）：按 full-cloud
// 语义读取 source[k] 和 target[k] 的 PointNormal 字段，生成 finite mask（有限值掩码）
// 后构造 a/b/c/d。invalid lane 在这里被置零，后续 A/B/C/N 组只能累加有效 row；
// accepted_points 由调用方用同一 keep mask 统计。这里不追求 bitwise 等价，因为
// vfredosum 会引入不同于标量 row-order double 累加的 reduction tree。
inline void
load_full_reduction_vectors(const std::uint8_t* source_base,
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
#endif // __RVV10__

} // namespace pcl::registration::rvv_te_pt2plane_lls_support
