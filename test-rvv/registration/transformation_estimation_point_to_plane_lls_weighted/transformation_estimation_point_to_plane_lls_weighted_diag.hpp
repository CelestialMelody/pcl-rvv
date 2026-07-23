/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_point_to_plane_lls_weighted 主题的
 * production-shaped diagnostic（生产形态诊断，尽量复用真实入口数据形状的测试专用诊断）
 * helper。它不修改 PCL production（生产源码）；目标是把 weighted point-to-plane
 * LLS（带权重点到平面线性最小二乘）的逐点 finite check（有限值检查）、weight load
 *（权重加载）、weight * normal（权重乘法线）和 a/b/c/d 公式放进 RVV（RISC-V Vector，
 * RISC-V 向量扩展）路径，再把结果交回 scalar tail（标量尾段）按原顺序累加
 * normal-equation（法方程）。
 *
 * 阅读提示：
 * - reference path（参考链路）复刻当前 production helper 的公式和求解边界。
 * - production 源码用 ConstCloudIterator（常量点云迭代器）把全云、indices（索引）
 *   和 correspondences（对应关系）入口统一成逐点流；本诊断为了写清 RVV 取数成本，
 *   显式拆成全云顺序扫描和对应关系索引扫描两条路径。
 * - RVV path（RVV 链路）手写部分只接管字段读取、mask（掩码）、公式 staging（分阶段
 *   暂存）和 vcompress（保序压缩）；压缩 buffer tail（临时缓冲尾段）在 -O3 下可能
 *   被编译器自动变成 vector reduction（向量规约），因此必须用反汇编归属复核。
 * - Eigen solve（Eigen 求解器）和 constructTransformationMatrix（构造变换矩阵）每次
 *   estimate 调用只执行一次，当前保留标量。
 */

#pragma once

#include <pcl/rvv_point_load.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

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

inline bool
finite_point_and_normal(const pcl::PointNormal& source, const pcl::PointNormal& target)
{
  return std::isfinite(source.x) && std::isfinite(source.y) &&
         std::isfinite(source.z) && std::isfinite(target.x) &&
         std::isfinite(target.y) && std::isfinite(target.z) &&
         std::isfinite(target.normal_x) && std::isfinite(target.normal_y) &&
         std::isfinite(target.normal_z);
}

// 这个函数是 weighted helper 的权威标量行构造：先把 weight 乘到 target normal，
// 再用加权后的 nx/ny/nz 生成 a/b/c/d 和 normal-equation 贡献。
inline void
accumulate_weighted_row(const float sx,
                        const float sy,
                        const float sz,
                        const float dx,
                        const float dy,
                        const float dz,
                        const float normal_x,
                        const float normal_y,
                        const float normal_z,
                        const float weight,
                        NormalEquation& eq)
{
  const float nx = normal_x * weight;
  const float ny = normal_y * weight;
  const float nz = normal_z * weight;

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

inline Matrix4f
construct_transformation_matrix(const Vector6d& x)
{
  const double alpha = x(0);
  const double beta = x(1);
  const double gamma = x(2);
  Matrix4f transformation = Matrix4f::Zero();
  transformation(0, 0) = static_cast<float>(std::cos(gamma) * std::cos(beta));
  transformation(0, 1) = static_cast<float>(
      -std::sin(gamma) * std::cos(alpha) +
      std::cos(gamma) * std::sin(beta) * std::sin(alpha));
  transformation(0, 2) = static_cast<float>(
      std::sin(gamma) * std::sin(alpha) +
      std::cos(gamma) * std::sin(beta) * std::cos(alpha));
  transformation(1, 0) = static_cast<float>(std::sin(gamma) * std::cos(beta));
  transformation(1, 1) = static_cast<float>(
      std::cos(gamma) * std::cos(alpha) +
      std::sin(gamma) * std::sin(beta) * std::sin(alpha));
  transformation(1, 2) = static_cast<float>(
      -std::cos(gamma) * std::sin(alpha) +
      std::sin(gamma) * std::sin(beta) * std::cos(alpha));
  transformation(2, 0) = static_cast<float>(-std::sin(beta));
  transformation(2, 1) = static_cast<float>(std::cos(beta) * std::sin(alpha));
  transformation(2, 2) = static_cast<float>(std::cos(beta) * std::cos(alpha));
  transformation(0, 3) = static_cast<float>(x(3));
  transformation(1, 3) = static_cast<float>(x(4));
  transformation(2, 3) = static_cast<float>(x(5));
  transformation(3, 3) = 1.0f;
  return transformation;
}

inline Matrix4f
solve_normal_equation(NormalEquation eq)
{
  complete_symmetric_upper(eq);
  const Vector6d x = static_cast<Vector6d>(eq.ata.inverse() * eq.atb);
  return construct_transformation_matrix(x);
}

inline NormalEquation
accumulate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                    const pcl::PointCloud<pcl::PointNormal>& target,
                    const std::vector<float>& weights,
                    AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  const std::size_t n = std::min(std::min(source.size(), target.size()), weights.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite_point_and_normal(source[i], target[i]))
      continue;
    accumulate_weighted_row(source[i].x,
                            source[i].y,
                            source[i].z,
                            target[i].x,
                            target[i].y,
                            target[i].z,
                            target[i].normal_x,
                            target[i].normal_y,
                            target[i].normal_z,
                            weights[i],
                            eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

inline NormalEquation
accumulate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                               const pcl::PointCloud<pcl::PointNormal>& target,
                               const pcl::Correspondences& correspondences,
                               AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  for (const auto& correspondence : correspondences) {
    if (correspondence.index_query < 0 || correspondence.index_match < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(correspondence.index_query);
    const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    if (!finite_point_and_normal(source[src_index], target[tgt_index]))
      continue;
    accumulate_weighted_row(source[src_index].x,
                            source[src_index].y,
                            source[src_index].z,
                            target[tgt_index].x,
                            target[tgt_index].y,
                            target[tgt_index].z,
                            target[tgt_index].normal_x,
                            target[tgt_index].normal_y,
                            target[tgt_index].normal_z,
                            correspondence.weight,
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
inline vbool16_t
finite_mask_f32m2(vfloat32m2_t value, const std::size_t vl)
{
  const vfloat32m2_t abs_value = __riscv_vfabs_v_f32m2(value, vl);
  return __riscv_vmfle_vf_f32m2_b16(abs_value, std::numeric_limits<float>::max(), vl);
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
#endif // __RVV10__

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

inline Matrix4f
estimate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                  const pcl::PointCloud<pcl::PointNormal>& target,
                  const std::vector<float>& weights,
                  AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_std_full(source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                        const pcl::PointCloud<pcl::PointNormal>& target,
                        const std::vector<float>& weights,
                        AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full(source, target, weights, stats));
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

inline double
matrix_checksum(const Matrix4f& matrix)
{
  double checksum = 0.0;
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      checksum += static_cast<double>(matrix(row, col)) * (1.0 + row * 4 + col);
  return checksum;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
