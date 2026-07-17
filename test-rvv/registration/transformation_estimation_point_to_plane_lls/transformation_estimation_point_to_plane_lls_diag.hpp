/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_point_to_plane_lls 主题的
 * production-shaped diagnostic（生产形态诊断，尽量复用真实入口数据形状的测试专用诊断）
 * helper。它不修改 PCL 生产源码；目标是把 point-to-plane LLS 的 normal-equation
 *（法方程）构造拆成“RVV 公式 staging（暂存阶段）+ 标量累加 tail（尾段）”，再和
 * 当前标量实现对拍。这样 reviewer 可以分开判断两件事：逐点公式是否适合 RVV
 *（RISC-V Vector，可变向量扩展），以及改变规约结构前是否已有足够证据。
 */

#pragma once

#include <pcl/common/rvv_point_load.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_diag {

using Matrix4f = Eigen::Matrix4f;
using Matrix6d = Eigen::Matrix<double, 6, 6>;
using Vector6d = Eigen::Matrix<double, 6, 1>;

struct AccumulationStats {
  std::size_t input_points = 0;
  std::size_t accepted_points = 0;
  bool used_rvv = false;
};

struct NormalEquation {
  Matrix6d ata = Matrix6d::Zero();
  Vector6d atb = Vector6d::Zero();
  std::size_t accepted_points = 0;
};

// 这个函数复刻 production 的有效点判断，供标量参考链路和测试断言共用。
inline bool
finite_point_and_normal(const pcl::PointNormal& source, const pcl::PointNormal& target)
{
  return std::isfinite(source.x) && std::isfinite(source.y) &&
         std::isfinite(source.z) && std::isfinite(target.x) &&
         std::isfinite(target.y) && std::isfinite(target.z) &&
         std::isfinite(target.normal_x) && std::isfinite(target.normal_y) &&
         std::isfinite(target.normal_z);
}

// 这个函数是一行 normal-equation 的权威标量 tail；RVV 路径压缩 lane 后也回到这里等价的累加结构。
inline void
accumulate_row(const float sx,
               const float sy,
               const float sz,
               const float dx,
               const float dy,
               const float dz,
               const float nx,
               const float ny,
               const float nz,
               NormalEquation& eq)
{
  // 这里刻意保留和 production（生产源码）相同的 float 逐点公式，再转 double 累加。
  // RVV staging 也输出同一组 a/b/c/d，后续 scalar tail 按 lane 顺序调用本函数。
  const double a = nz * sy - ny * sz;
  const double b = nx * sz - nz * sx;
  const double c = ny * sx - nx * sy;

  eq.ata.coeffRef(0) += a * a;
  eq.ata.coeffRef(1) += a * b;
  eq.ata.coeffRef(2) += a * c;
  eq.ata.coeffRef(3) += a * nx;
  eq.ata.coeffRef(4) += a * ny;
  eq.ata.coeffRef(5) += a * nz;
  eq.ata.coeffRef(7) += b * b;
  eq.ata.coeffRef(8) += b * c;
  eq.ata.coeffRef(9) += b * nx;
  eq.ata.coeffRef(10) += b * ny;
  eq.ata.coeffRef(11) += b * nz;
  eq.ata.coeffRef(14) += c * c;
  eq.ata.coeffRef(15) += c * nx;
  eq.ata.coeffRef(16) += c * ny;
  eq.ata.coeffRef(17) += c * nz;
  eq.ata.coeffRef(21) += nx * nx;
  eq.ata.coeffRef(22) += nx * ny;
  eq.ata.coeffRef(23) += nx * nz;
  eq.ata.coeffRef(28) += ny * ny;
  eq.ata.coeffRef(29) += ny * nz;
  eq.ata.coeffRef(35) += nz * nz;

  const double d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz;
  eq.atb.coeffRef(0) += a * d;
  eq.atb.coeffRef(1) += b * d;
  eq.atb.coeffRef(2) += c * d;
  eq.atb.coeffRef(3) += nx * d;
  eq.atb.coeffRef(4) += ny * d;
  eq.atb.coeffRef(5) += nz * d;
  ++eq.accepted_points;
}

// production 源码只累加上三角；求解前需要补齐下三角，保持 Eigen inverse 的输入一致。
inline void
complete_symmetric_upper(NormalEquation& eq)
{
  eq.ata.coeffRef(6) = eq.ata.coeff(1);
  eq.ata.coeffRef(12) = eq.ata.coeff(2);
  eq.ata.coeffRef(13) = eq.ata.coeff(8);
  eq.ata.coeffRef(18) = eq.ata.coeff(3);
  eq.ata.coeffRef(19) = eq.ata.coeff(9);
  eq.ata.coeffRef(20) = eq.ata.coeff(15);
  eq.ata.coeffRef(24) = eq.ata.coeff(4);
  eq.ata.coeffRef(25) = eq.ata.coeff(10);
  eq.ata.coeffRef(26) = eq.ata.coeff(16);
  eq.ata.coeffRef(27) = eq.ata.coeff(22);
  eq.ata.coeffRef(30) = eq.ata.coeff(5);
  eq.ata.coeffRef(31) = eq.ata.coeff(11);
  eq.ata.coeffRef(32) = eq.ata.coeff(17);
  eq.ata.coeffRef(33) = eq.ata.coeff(23);
  eq.ata.coeffRef(34) = eq.ata.coeff(29);
}

// 这个 helper 复刻 production 的欧拉角到 4x4 矩阵构造，避免诊断路径依赖 protected 成员。
inline Matrix4f
construct_transformation_matrix(const Vector6d& x)
{
  const double alpha = x(0);
  const double beta = x(1);
  const double gamma = x(2);
  Matrix4f transformation = Matrix4f::Zero();
  transformation(0, 0) =
      static_cast<float>(std::cos(gamma) * std::cos(beta));
  transformation(0, 1) = static_cast<float>(
      -std::sin(gamma) * std::cos(alpha) +
      std::cos(gamma) * std::sin(beta) * std::sin(alpha));
  transformation(0, 2) = static_cast<float>(
      std::sin(gamma) * std::sin(alpha) +
      std::cos(gamma) * std::sin(beta) * std::cos(alpha));
  transformation(1, 0) =
      static_cast<float>(std::sin(gamma) * std::cos(beta));
  transformation(1, 1) = static_cast<float>(
      std::cos(gamma) * std::cos(alpha) +
      std::sin(gamma) * std::sin(beta) * std::sin(alpha));
  transformation(1, 2) = static_cast<float>(
      -std::cos(gamma) * std::sin(alpha) +
      std::sin(gamma) * std::sin(beta) * std::cos(alpha));
  transformation(2, 0) = static_cast<float>(-std::sin(beta));
  transformation(2, 1) =
      static_cast<float>(std::cos(beta) * std::sin(alpha));
  transformation(2, 2) =
      static_cast<float>(std::cos(beta) * std::cos(alpha));
  transformation(0, 3) = static_cast<float>(x(3));
  transformation(1, 3) = static_cast<float>(x(4));
  transformation(2, 3) = static_cast<float>(x(5));
  transformation(3, 3) = 1.0f;
  return transformation;
}

// 这个函数代表完整诊断求解边界：normal-equation 已构造完成后仍交给 Eigen 处理。
inline Matrix4f
solve_normal_equation(NormalEquation eq)
{
  complete_symmetric_upper(eq);
  const Vector6d x = static_cast<Vector6d>(eq.ata.inverse() * eq.atb);
  return construct_transformation_matrix(x);
}

// 全云标量参考链路，用来验证 test-rvv 诊断是否复刻当前 production 公式。
inline NormalEquation
accumulate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                    const pcl::PointCloud<pcl::PointNormal>& target,
                    AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  const std::size_t n = std::min(source.size(), target.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite_point_and_normal(source[i], target[i]))
      continue;
    accumulate_row(source[i].x,
                   source[i].y,
                   source[i].z,
                   target[i].x,
                   target[i].y,
                   target[i].z,
                   target[i].normal_x,
                   target[i].normal_y,
                   target[i].normal_z,
                   eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

// correspondences 标量参考链路覆盖乱序和重复输入；helper 会跳过非法 index，但本轮未单独测试该边界。
inline NormalEquation
accumulate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                               const pcl::PointCloud<pcl::PointNormal>& target,
                               const pcl::Correspondences& correspondences,
                               AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  for (const auto& correspondence : correspondences) {
    const auto src_index = static_cast<std::size_t>(correspondence.index_query);
    const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    if (!finite_point_and_normal(source[src_index], target[tgt_index]))
      continue;
    accumulate_row(source[src_index].x,
                   source[src_index].y,
                   source[src_index].z,
                   target[tgt_index].x,
                   target[tgt_index].y,
                   target[tgt_index].z,
                   target[tgt_index].normal_x,
                   target[tgt_index].normal_y,
                   target[tgt_index].normal_z,
                   eq);
  }
  if (stats) {
    stats->input_points = correspondences.size();
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

#ifdef __RVV10__
// RVV 没有直接的 isfinite intrinsic；这个 mask 用有限最大值比较同时排除 NaN 和 Inf。
inline vbool16_t
finite_mask_f32m2(vfloat32m2_t value, const std::size_t vl)
{
  const vfloat32m2_t abs_value = __riscv_vfabs_v_f32m2(value, vl);
  return __riscv_vmfle_vf_f32m2_b16(abs_value, std::numeric_limits<float>::max(), vl);
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
  // 逐点公式保留乘法、加法、减法的可读边界，不使用 fused intrinsic
  //（融合乘加指令），便于和当前标量表达式对齐。
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
#endif // __RVV10__

// 全云 candidate 用 stride load 覆盖连续 PointNormal；未命中 gate 时自然回到标量参考链路。
inline NormalEquation
accumulate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                          const pcl::PointCloud<pcl::PointNormal>& target,
                          AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(source.size(), target.size());
#ifdef __RVV10__
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
      vfloat32m2_t sx, sy, sz, dx, dy, dz, nx, ny, nz;
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          src_base + i * sizeof(pcl::PointNormal), vl, sx, sy, sz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, dx, dy, dz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, nx, ny, nz);

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

// correspondences candidate 先展开 index，再用 gather load 诊断真实重载里的非连续访问成本。
inline NormalEquation
accumulate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                     const pcl::PointCloud<pcl::PointNormal>& target,
                                     const pcl::Correspondences& correspondences,
                                     AccumulationStats* stats = nullptr)
{
#ifdef __RVV10__
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
    src_indices.reserve(correspondences.size());
    tgt_indices.reserve(correspondences.size());
    for (const auto& correspondence : correspondences) {
      if (correspondence.index_query < 0 || correspondence.index_match < 0)
        continue;
      const auto src_index = static_cast<std::size_t>(correspondence.index_query);
      const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
      if (src_index >= source.size() || tgt_index >= target.size())
        continue;
      src_indices.push_back(static_cast<std::uint32_t>(src_index));
      tgt_indices.push_back(static_cast<std::uint32_t>(tgt_index));
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

        vfloat32m2_t sx, sy, sz, dx, dy, dz, nx, ny, nz;
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            src_base, src_offsets, vl, sx, sy, sz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            tgt_base, tgt_offsets, vl, dx, dy, dz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
            tgt_base, tgt_offsets, vl, nx, ny, nz);

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

// 下面四个 estimate_* helper 是测试和 bench 的入口层，负责把 normal-equation 诊断连接到 4x4 输出。
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

} // namespace pcl::registration::rvv_te_pt2plane_lls_diag
