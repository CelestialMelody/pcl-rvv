/*
 * 本文件做什么：
 * 这是 TEPTPL test-rvv support（测试支撑）公共层，保存 Stats、NormalEquation、
 * 标量 reference（参考链路）和 matrix/solve helper。它复刻的是
 * point-to-plane LLS normal-equation（法方程）与 4x4 matrix 构造的审查边界。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv 的对拍、diagnostic/probing、ablation 和 bench，不是
 * production dispatch（生产分流）。它们不能证明真实公开入口命中 RVV，也不能把当前
 * full-cloud f32 AoS layout-gated production implementation 扩展到 indexed、
 * correspondences、weighted 或 Scalar=double。
 */

#pragma once

#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_support {

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
template <typename PointSource, typename PointTarget>
inline bool
finite_point_and_normal(const PointSource& source, const PointTarget& target)
{
  return std::isfinite(source.x) && std::isfinite(source.y) &&
         std::isfinite(source.z) && std::isfinite(target.x) &&
         std::isfinite(target.y) && std::isfinite(target.z) &&
         std::isfinite(target.normal_x) && std::isfinite(target.normal_y) &&
         std::isfinite(target.normal_z);
}

// 这个 helper 接收已经完成逐点公式 staging 的一行，统一维护 6x6 / 6x1
// normal-equation 累加。RVV compress tail 和 trusted-dense tail 都回到这里。
inline void
accumulate_formula_values(const double a,
                          const double b,
                          const double c,
                          const double d,
                          const double nx,
                          const double ny,
                          const double nz,
                          NormalEquation& eq)
{
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

  eq.atb.coeffRef(0) += a * d;
  eq.atb.coeffRef(1) += b * d;
  eq.atb.coeffRef(2) += c * d;
  eq.atb.coeffRef(3) += nx * d;
  eq.atb.coeffRef(4) += ny * d;
  eq.atb.coeffRef(5) += nz * d;
  ++eq.accepted_points;
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
  const double d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz;
  accumulate_formula_values(a, b, c, d, nx, ny, nz, eq);
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
template <typename PointSource, typename PointTarget>
inline NormalEquation
accumulate_std_full(const pcl::PointCloud<PointSource>& source,
                    const pcl::PointCloud<PointTarget>& target,
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

// correspondences 标量参考链路覆盖乱序和重复输入；helper 会跳过非法 index，
// 但当前 diagnostic 合同没有单独声明或测试非法 index 行为。
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

// source indices 标量参考链路：映射公开 `cloud_src + indices_src + cloud_tgt`
// 入口。source 侧按 row 序号读取 indices，target 侧按 row 序号顺序读取。这里的
// 防御性非法 index 跳过只服务 test-rvv 参考链路；生产公开入口没有定义负数或越界
// index 的额外过滤语义，当前 indexed diagnostic 只声明 valid-index-only（只覆盖有效索引）。
inline NormalEquation
accumulate_std_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<pcl::PointNormal>& target,
                              AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  const std::size_t n = std::min(source_indices.size(), target.size());
  for (std::size_t row = 0; row < n; ++row) {
    const int raw_src_index = source_indices[row];
    if (raw_src_index < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(raw_src_index);
    if (src_index >= source.size())
      continue;
    if (!finite_point_and_normal(source[src_index], target[row]))
      continue;
    accumulate_row(source[src_index].x,
                   source[src_index].y,
                   source[src_index].z,
                   target[row].x,
                   target[row].y,
                   target[row].z,
                   target[row].normal_x,
                   target[row].normal_y,
                   target[row].normal_z,
                   eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

// 双侧 indices 标量参考链路：映射公开 `cloud_src + indices_src + cloud_tgt +
// indices_tgt` 入口。source 和 target 各自使用独立 index stream；它不解析
// pcl::Correspondence，也不展开 weight。这个 reference（参考链路）用于把双侧
// gather（离散加载）与 correspondences（对应关系）额外展开成本拆开审查。
inline NormalEquation
accumulate_std_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<pcl::PointNormal>& target,
                            const pcl::Indices& target_indices,
                            AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  for (std::size_t row = 0; row < n; ++row) {
    const int raw_src_index = source_indices[row];
    const int raw_tgt_index = target_indices[row];
    if (raw_src_index < 0 || raw_tgt_index < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(raw_src_index);
    const auto tgt_index = static_cast<std::size_t>(raw_tgt_index);
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
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_support
