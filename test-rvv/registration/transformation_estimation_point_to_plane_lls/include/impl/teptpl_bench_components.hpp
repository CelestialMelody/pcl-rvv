/*
 * 本文件做什么：
 * TEPTPL bench 的 component-only（组件消融）helper。它们拆分 load/store、公式、
 * mask/compress 和 scalar tail 成本，只提供归因线索，不替代端到端 board 证据。
 */

#pragma once

#include "teptpl_bench_fixtures.hpp"

#include <pcl/rvv_point_load.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace pcl::registration::rvv_te_pt2plane_lls_bench {

inline double
componentFullLoadStoreOnly(const pcl::PointCloud<pcl::PointNormal>& source,
                           const pcl::PointCloud<pcl::PointNormal>& target)
{
  const std::size_t n = std::min(source.size(), target.size());
  double checksum = 0.0;
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64) {
    constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
    constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
    constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
    constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
    constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
    constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());
    alignas(16) float sx[64], sy[64], sz[64], dx[64], dy[64], dz[64], nx[64], ny[64], nz[64];
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      vfloat32m2_t vsx, vsy, vsz, vdx, vdy, vdz, vnx, vny, vnz;
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          source_base + i * sizeof(pcl::PointNormal), vl, vsx, vsy, vsz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          target_base + i * sizeof(pcl::PointNormal), vl, vdx, vdy, vdz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
          target_base + i * sizeof(pcl::PointNormal), vl, vnx, vny, vnz);
      __riscv_vse32_v_f32m2(sx, vsx, vl);
      __riscv_vse32_v_f32m2(sy, vsy, vl);
      __riscv_vse32_v_f32m2(sz, vsz, vl);
      __riscv_vse32_v_f32m2(dx, vdx, vl);
      __riscv_vse32_v_f32m2(dy, vdy, vl);
      __riscv_vse32_v_f32m2(dz, vdz, vl);
      __riscv_vse32_v_f32m2(nx, vnx, vl);
      __riscv_vse32_v_f32m2(ny, vny, vl);
      __riscv_vse32_v_f32m2(nz, vnz, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        checksum += sx[lane] + sy[lane] * 2.0 + sz[lane] * 3.0 + dx[lane] * 4.0 +
                    dy[lane] * 5.0 + dz[lane] * 6.0 + nx[lane] * 7.0 +
                    ny[lane] * 8.0 + nz[lane] * 9.0;
      i += vl;
    }
    return checksum;
  }
#endif
  for (std::size_t i = 0; i < n; ++i)
    checksum += source[i].x + source[i].y * 2.0 + source[i].z * 3.0 +
                target[i].x * 4.0 + target[i].y * 5.0 + target[i].z * 6.0 +
                target[i].normal_x * 7.0 + target[i].normal_y * 8.0 +
                target[i].normal_z * 9.0;
  return checksum;
}

inline double
componentDualGatherLoadStoreOnly(const pcl::PointCloud<pcl::PointNormal>& source,
                                 const pcl::Indices& source_indices,
                                 const pcl::PointCloud<pcl::PointNormal>& target,
                                 const pcl::Indices& target_indices)
{
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  double checksum = 0.0;
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64) {
    constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
    constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
    constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
    constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
    constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
    constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());
    alignas(16) float sx[64], sy[64], sz[64], dx[64], dy[64], dz[64], nx[64], ny[64], nz[64];
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vint32m2_t src_idx =
          __riscv_vle32_v_i32m2(source_indices.data() + i, vl);
      const vint32m2_t tgt_idx =
          __riscv_vle32_v_i32m2(target_indices.data() + i, vl);
      const vuint32m2_t src_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(
          __riscv_vreinterpret_v_i32m2_u32m2(src_idx), vl);
      const vuint32m2_t tgt_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(
          __riscv_vreinterpret_v_i32m2_u32m2(tgt_idx), vl);
      vfloat32m2_t vsx, vsy, vsz, vdx, vdy, vdz, vnx, vny, vnz;
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          source_base, src_offsets, vl, vsx, vsy, vsz);
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          target_base, tgt_offsets, vl, vdx, vdy, vdz);
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
          target_base, tgt_offsets, vl, vnx, vny, vnz);
      __riscv_vse32_v_f32m2(sx, vsx, vl);
      __riscv_vse32_v_f32m2(sy, vsy, vl);
      __riscv_vse32_v_f32m2(sz, vsz, vl);
      __riscv_vse32_v_f32m2(dx, vdx, vl);
      __riscv_vse32_v_f32m2(dy, vdy, vl);
      __riscv_vse32_v_f32m2(dz, vdz, vl);
      __riscv_vse32_v_f32m2(nx, vnx, vl);
      __riscv_vse32_v_f32m2(ny, vny, vl);
      __riscv_vse32_v_f32m2(nz, vnz, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        checksum += sx[lane] + sy[lane] * 2.0 + sz[lane] * 3.0 + dx[lane] * 4.0 +
                    dy[lane] * 5.0 + dz[lane] * 6.0 + nx[lane] * 7.0 +
                    ny[lane] * 8.0 + nz[lane] * 9.0;
      i += vl;
    }
    return checksum;
  }
#endif
  for (std::size_t i = 0; i < n; ++i) {
    const auto si = static_cast<std::size_t>(source_indices[i]);
    const auto ti = static_cast<std::size_t>(target_indices[i]);
    checksum += source[si].x + source[si].y * 2.0 + source[si].z * 3.0 +
                target[ti].x * 4.0 + target[ti].y * 5.0 + target[ti].z * 6.0 +
                target[ti].normal_x * 7.0 + target[ti].normal_y * 8.0 +
                target[ti].normal_z * 9.0;
  }
  return checksum;
}

inline double
componentFormulaStoreOnly(const ComponentSoA& soa)
{
  const std::size_t n = soa.sx.size();
  double checksum = 0.0;
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64) {
    alignas(16) float a[64], b[64], c[64], d[64];
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vfloat32m2_t sx = __riscv_vle32_v_f32m2(soa.sx.data() + i, vl);
      const vfloat32m2_t sy = __riscv_vle32_v_f32m2(soa.sy.data() + i, vl);
      const vfloat32m2_t sz = __riscv_vle32_v_f32m2(soa.sz.data() + i, vl);
      const vfloat32m2_t dx = __riscv_vle32_v_f32m2(soa.dx.data() + i, vl);
      const vfloat32m2_t dy = __riscv_vle32_v_f32m2(soa.dy.data() + i, vl);
      const vfloat32m2_t dz = __riscv_vle32_v_f32m2(soa.dz.data() + i, vl);
      const vfloat32m2_t nx = __riscv_vle32_v_f32m2(soa.nx.data() + i, vl);
      const vfloat32m2_t ny = __riscv_vle32_v_f32m2(soa.ny.data() + i, vl);
      const vfloat32m2_t nz = __riscv_vle32_v_f32m2(soa.nz.data() + i, vl);
      vfloat32m2_t va, vb, vc, vd;
      support::staged_formula(sx, sy, sz, dx, dy, dz, nx, ny, nz, vl, va, vb, vc, vd);
      __riscv_vse32_v_f32m2(a, va, vl);
      __riscv_vse32_v_f32m2(b, vb, vl);
      __riscv_vse32_v_f32m2(c, vc, vl);
      __riscv_vse32_v_f32m2(d, vd, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        checksum += a[lane] + b[lane] * 2.0 + c[lane] * 3.0 + d[lane] * 4.0;
      i += vl;
    }
    return checksum;
  }
#endif
  for (std::size_t i = 0; i < n; ++i) {
    const float a = soa.nz[i] * soa.sy[i] - soa.ny[i] * soa.sz[i];
    const float b = soa.nx[i] * soa.sz[i] - soa.nz[i] * soa.sx[i];
    const float c = soa.ny[i] * soa.sx[i] - soa.nx[i] * soa.sy[i];
    const float d = soa.nx[i] * soa.dx[i] + soa.ny[i] * soa.dy[i] +
                    soa.nz[i] * soa.dz[i] - soa.nx[i] * soa.sx[i] -
                    soa.ny[i] * soa.sy[i] - soa.nz[i] * soa.sz[i];
    checksum += a + b * 2.0 + c * 3.0 + d * 4.0;
  }
  return checksum;
}

inline double
componentMaskCompressOnly(const ComponentSoA& soa, const FormulaSoA& formula)
{
  const std::size_t n = soa.sx.size();
  double checksum = 0.0;
#ifdef __RVV10__
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64) {
    alignas(16) float a[64], b[64], c[64], d[64], nx[64], ny[64], nz[64];
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      vbool16_t keep = support::finite_mask_f32m2(
          __riscv_vle32_v_f32m2(soa.sx.data() + i, vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.sy.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.sz.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.dx.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.dy.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.dz.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.nx.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.ny.data() + i, vl), vl), vl);
      keep = __riscv_vmand_mm_b16(
          keep, support::finite_mask_f32m2(__riscv_vle32_v_f32m2(soa.nz.data() + i, vl), vl), vl);

      const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
      const vfloat32m2_t va = __riscv_vle32_v_f32m2(formula.a.data() + i, vl);
      const vfloat32m2_t vb = __riscv_vle32_v_f32m2(formula.b.data() + i, vl);
      const vfloat32m2_t vc = __riscv_vle32_v_f32m2(formula.c.data() + i, vl);
      const vfloat32m2_t vd = __riscv_vle32_v_f32m2(formula.d.data() + i, vl);
      const vfloat32m2_t vnx = __riscv_vle32_v_f32m2(formula.nx.data() + i, vl);
      const vfloat32m2_t vny = __riscv_vle32_v_f32m2(formula.ny.data() + i, vl);
      const vfloat32m2_t vnz = __riscv_vle32_v_f32m2(formula.nz.data() + i, vl);
      __riscv_vse32_v_f32m2(a, __riscv_vcompress_vm_f32m2(va, keep, vl), kept);
      __riscv_vse32_v_f32m2(b, __riscv_vcompress_vm_f32m2(vb, keep, vl), kept);
      __riscv_vse32_v_f32m2(c, __riscv_vcompress_vm_f32m2(vc, keep, vl), kept);
      __riscv_vse32_v_f32m2(d, __riscv_vcompress_vm_f32m2(vd, keep, vl), kept);
      __riscv_vse32_v_f32m2(nx, __riscv_vcompress_vm_f32m2(vnx, keep, vl), kept);
      __riscv_vse32_v_f32m2(ny, __riscv_vcompress_vm_f32m2(vny, keep, vl), kept);
      __riscv_vse32_v_f32m2(nz, __riscv_vcompress_vm_f32m2(vnz, keep, vl), kept);
      for (std::size_t lane = 0; lane < kept; ++lane)
        checksum += a[lane] + b[lane] * 2.0 + c[lane] * 3.0 + d[lane] * 4.0 +
                    nx[lane] * 5.0 + ny[lane] * 6.0 + nz[lane] * 7.0;
      i += vl;
    }
    return checksum;
  }
#endif
  for (std::size_t i = 0; i < n; ++i) {
    if (!std::isfinite(soa.sx[i]) || !std::isfinite(soa.sy[i]) ||
        !std::isfinite(soa.sz[i]) || !std::isfinite(soa.dx[i]) ||
        !std::isfinite(soa.dy[i]) || !std::isfinite(soa.dz[i]) ||
        !std::isfinite(soa.nx[i]) || !std::isfinite(soa.ny[i]) ||
        !std::isfinite(soa.nz[i]))
      continue;
    checksum += formula.a[i] + formula.b[i] * 2.0 + formula.c[i] * 3.0 +
                formula.d[i] * 4.0 + formula.nx[i] * 5.0 +
                formula.ny[i] * 6.0 + formula.nz[i] * 7.0;
  }
  return checksum;
}

inline double
componentTailOnly(const FormulaSoA& formula)
{
  support::NormalEquation eq;
  for (std::size_t i = 0; i < formula.a.size(); ++i) {
    support::accumulate_formula_values(formula.a[i],
                                    formula.b[i],
                                    formula.c[i],
                                    formula.d[i],
                                    formula.nx[i],
                                    formula.ny[i],
                                    formula.nz[i],
                                    eq);
  }
  return normalEquationChecksum(eq);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_bench
