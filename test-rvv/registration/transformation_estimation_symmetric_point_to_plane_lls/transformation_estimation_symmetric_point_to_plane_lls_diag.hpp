/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_symmetric_point_to_plane_lls 主题的
 * production-shaped diagnostic（生产形态诊断，尽量复用真实入口数据形状的测试专用诊断）
 * helper。它不修改 PCL production（生产源码）；目标是把 symmetric point-to-plane
 * LLS（对称点到平面线性最小二乘）的逐点 finite check（有限值检查）、source/target
 * normal（源/目标法线）同向选择、(p + q).cross(n) 公式和 right-hand side（右端项）
 * 放进 RVV（RISC-V Vector，RISC-V 向量扩展）路径，再把压缩后的行交回 scalar tail
 *（标量尾段）按原顺序累加 normal-equation（法方程）。
 *
 * 阅读提示：
 * - reference path（参考链路）复刻当前 production helper 的公式和 Eigen LDLT
 *   solve（Eigen LDLT 求解器）边界。
 * - production 源码用 ConstCloudIterator（常量点云迭代器）把全云、indices（索引）
 *   和 correspondences（对应关系）入口统一成逐点流；本诊断为了写清 RVV 取数成本，
 *   显式拆成全云顺序扫描和对应关系索引扫描两条路径。
 * - RVV path（RVV 链路）只接管字段读取、mask（掩码）、法线选择、公式 staging
 *   （分阶段暂存）和 vcompress（保序压缩）；Eigen solve 和最终 4x4 矩阵构造保留标量。
 */

#pragma once

#include <pcl/common/rvv_point_load.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pcl::registration::rvv_te_symmetric_pt2plane_lls_diag {

using Matrix4f = Eigen::Matrix4f;
using Matrix6f = Eigen::Matrix<float, 6, 6>;
using Vector6f = Eigen::Matrix<float, 6, 1>;

struct AccumulationStats {
  std::size_t input_points = 0;
  std::size_t accepted_points = 0;
  bool used_rvv = false;
};

struct NormalEquation {
  Matrix6f ata = Matrix6f::Zero();
  Vector6f atb = Vector6f::Zero();
  std::size_t accepted_points = 0;
};

// 坐标有限性 helper：供标量参考链路和 RVV fallback（回退路径）共用。
// 它对应 production helper 中对 p/q 坐标的 finite check（有限值检查），证据角色是
// 保证 test-rvv 的 reference path（参考链路）不会比 production 多检查或少检查坐标字段。
inline bool
finite_xyz(const pcl::PointNormal& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

// 合成 normal 有限性 helper：symmetric production 先根据同向规则得到 n，再检查 n 是否有限。
// 调用者是全云和 correspondences 的标量诊断路径；RVV 路径用 finite_mask_f32m2 复刻同一语义。
inline bool
finite_normal(const float nx, const float ny, const float nz)
{
  return std::isfinite(nx) && std::isfinite(ny) && std::isfinite(nz);
}

// 标量法线选择 helper：复刻 production 的 enforce_same_direction_normals_ 分支。
// 它把 source normal 和 target normal 映射成 symmetric LLS 真正使用的 n；专项测试用它
// 证明默认同向 gate（验收条件）和关闭 gate 两种公开语义都与 estimator 一致。
inline void
select_symmetric_normal(const float n1x,
                        const float n1y,
                        const float n1z,
                        const float n2x,
                        const float n2y,
                        const float n2z,
                        const bool enforce_same_direction_normals,
                        float& nx,
                        float& ny,
                        float& nz)
{
  if (!enforce_same_direction_normals) {
    nx = n1x + n2x;
    ny = n1y + n2y;
    nz = n1z + n2z;
    return;
  }

  const float dot = n1x * n2x + n1y * n2y + n1z * n2z;
  if (dot >= 0.0f) {
    nx = n1x + n2x;
    ny = n1y + n2y;
    nz = n1z + n2z;
  }
  else {
    nx = n1x - n2x;
    ny = n1y - n2y;
    nz = n1z - n2z;
  }
}

// 这个函数是一行 symmetric normal-equation 的权威标量尾段。它对应 production 中的
// `v << (p + q).cross(n), n`、`M.rankUpdate(v)` 和 `ATb += v * d` 三个动作。
// 标量参考链路直接调用它；RVV 路径把 a/b/c/d/n 暂存并压缩后回到同样的累加结构，
// 证据角色是把“RVV staging 是否正确”和“后续规约语义是否可接受”分开审查。
inline void
accumulate_symmetric_row(const float sx,
                         const float sy,
                         const float sz,
                         const float tx,
                         const float ty,
                         const float tz,
                         const float nx,
                         const float ny,
                         const float nz,
                         NormalEquation& eq)
{
  const float px = sx + tx;
  const float py = sy + ty;
  const float pz = sz + tz;
  const float dx = tx - sx;
  const float dy = ty - sy;
  const float dz = tz - sz;

  const float a = py * nz - pz * ny;
  const float b = pz * nx - px * nz;
  const float c = px * ny - py * nx;
  const float d = dx * nx + dy * ny + dz * nz;

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

// 4x4 矩阵构造 helper：复刻 production 的 Euler angle（欧拉角）和 translation
//（平移）组合顺序。调用者是 solve_normal_equation；它每次 estimate 只执行一次，
// 因此当前不是 RVV 热点，只作为完整 matrix checksum（矩阵校验和）证据的一部分。
inline Matrix4f
construct_transformation_matrix(const Vector6f& parameters)
{
  const Eigen::AngleAxisf rotation_z(parameters(2), Eigen::Vector3f::UnitZ());
  const Eigen::AngleAxisf rotation_y(parameters(1), Eigen::Vector3f::UnitY());
  const Eigen::AngleAxisf rotation_x(parameters(0), Eigen::Vector3f::UnitX());
  const Eigen::Translation<float, 3> translation(
      parameters(3), parameters(4), parameters(5));
  const Eigen::Transform<float, 3, Eigen::Affine> transform =
      rotation_z * rotation_y * rotation_x * translation * rotation_z * rotation_y *
      rotation_x;
  return transform.matrix();
}

// Eigen 存储映射 helper：线性 coeffRef(index) 在 Eigen 默认 column-major（列优先存储）
// 矩阵里先写到下三角；production 使用 selfadjointView<Eigen::Upper>()，诊断求解前要把
// 对应项补到上三角。它只修正 test-rvv 诊断的矩阵视图，不改变 production 或 RVV 公式。
inline void
complete_symmetric_matrix(NormalEquation& eq)
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

// 完整诊断求解边界：normal-equation 已经构造完成后，继续使用 Eigen LDLT solve
//（Eigen LDLT 求解器）和 production 同形矩阵构造。调用者是 estimate_* wrapper；
// 证据角色是让 correctness（正确性）和 bench（性能测试）覆盖完整估计输出，而不是只覆盖局部行公式。
inline Matrix4f
solve_normal_equation(NormalEquation eq)
{
  complete_symmetric_matrix(eq);
  const Vector6f x = eq.ata.template selfadjointView<Eigen::Upper>().ldlt().solve(eq.atb);
  return construct_transformation_matrix(x);
}

// 全云标量参考链路：映射公开 full-cloud（全云顺序扫描）入口，第 i 个 source 对第 i 个 target。
// 测试用它与 production estimator 对拍，也用它作为 RVV candidate 的 same-chain（同构链路）参考。
inline NormalEquation
accumulate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                    const pcl::PointCloud<pcl::PointNormal>& target,
                    const bool enforce_same_direction_normals,
                    AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  const std::size_t n = std::min(source.size(), target.size());
  for (std::size_t i = 0; i < n; ++i) {
    float nx, ny, nz;
    select_symmetric_normal(source[i].normal_x,
                            source[i].normal_y,
                            source[i].normal_z,
                            target[i].normal_x,
                            target[i].normal_y,
                            target[i].normal_z,
                            enforce_same_direction_normals,
                            nx,
                            ny,
                            nz);
    if (!finite_xyz(source[i]) || !finite_xyz(target[i]) ||
        !finite_normal(nx, ny, nz))
      continue;
    accumulate_symmetric_row(
        source[i].x, source[i].y, source[i].z, target[i].x, target[i].y, target[i].z,
        nx, ny, nz, eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

// 对应关系标量参考链路：映射公开 correspondences（对应关系）入口的 index_query/index_match
// 逐项访问。它保留列表顺序、重复 index 和非法 index 跳过规则，供 gather（离散加载）
// RVV 候选对拍；它不是 indexed 路径性能归因，只是 correctness 参考。
inline NormalEquation
accumulate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                               const pcl::PointCloud<pcl::PointNormal>& target,
                               const pcl::Correspondences& correspondences,
                               const bool enforce_same_direction_normals,
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
    float nx, ny, nz;
    select_symmetric_normal(source[src_index].normal_x,
                            source[src_index].normal_y,
                            source[src_index].normal_z,
                            target[tgt_index].normal_x,
                            target[tgt_index].normal_y,
                            target[tgt_index].normal_z,
                            enforce_same_direction_normals,
                            nx,
                            ny,
                            nz);
    if (!finite_xyz(source[src_index]) || !finite_xyz(target[tgt_index]) ||
        !finite_normal(nx, ny, nz))
      continue;
    accumulate_symmetric_row(source[src_index].x,
                             source[src_index].y,
                             source[src_index].z,
                             target[tgt_index].x,
                             target[tgt_index].y,
                             target[tgt_index].z,
                             nx,
                             ny,
                             nz,
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
// RVV 有限值 mask helper：用 abs(value) <= max_float 复刻 std::isfinite 对 NaN/Inf 的剔除效果。
// 调用者是全云和 correspondences RVV candidate；证据角色是证明 invalid lane（无效向量通道）
// 会在合成 normal 后被剔除，和 production 的 continue 语义一致。
inline vbool16_t
finite_mask_f32m2(vfloat32m2_t value, const std::size_t vl)
{
  const vfloat32m2_t abs_value = __riscv_vfabs_v_f32m2(value, vl);
  return __riscv_vmfle_vf_f32m2_b16(abs_value, std::numeric_limits<float>::max(), vl);
}

// RVV 法线选择 helper：把 production 的标量 dot 分支改写成 lane mask（向量通道掩码）
// 和 vmerge（按掩码合并）。调用者是两个 RVV candidate；它只生成合成后的 n，
// 后续 finite mask 仍按 production 语义检查 n，而不是分别检查 n1/n2。
inline void
select_symmetric_normal_v(vfloat32m2_t n1x,
                          vfloat32m2_t n1y,
                          vfloat32m2_t n1z,
                          vfloat32m2_t n2x,
                          vfloat32m2_t n2y,
                          vfloat32m2_t n2z,
                          const bool enforce_same_direction_normals,
                          const std::size_t vl,
                          vfloat32m2_t& nx,
                          vfloat32m2_t& ny,
                          vfloat32m2_t& nz)
{
  const vfloat32m2_t add_x = __riscv_vfadd_vv_f32m2(n1x, n2x, vl);
  const vfloat32m2_t add_y = __riscv_vfadd_vv_f32m2(n1y, n2y, vl);
  const vfloat32m2_t add_z = __riscv_vfadd_vv_f32m2(n1z, n2z, vl);
  if (!enforce_same_direction_normals) {
    nx = add_x;
    ny = add_y;
    nz = add_z;
    return;
  }

  vfloat32m2_t dot = __riscv_vfmul_vv_f32m2(n1x, n2x, vl);
  dot = __riscv_vfadd_vv_f32m2(dot, __riscv_vfmul_vv_f32m2(n1y, n2y, vl), vl);
  dot = __riscv_vfadd_vv_f32m2(dot, __riscv_vfmul_vv_f32m2(n1z, n2z, vl), vl);
  const vbool16_t same_direction = __riscv_vmfge_vf_f32m2_b16(dot, 0.0f, vl);

  const vfloat32m2_t sub_x = __riscv_vfsub_vv_f32m2(n1x, n2x, vl);
  const vfloat32m2_t sub_y = __riscv_vfsub_vv_f32m2(n1y, n2y, vl);
  const vfloat32m2_t sub_z = __riscv_vfsub_vv_f32m2(n1z, n2z, vl);
  nx = __riscv_vmerge_vvm_f32m2(sub_x, add_x, same_direction, vl);
  ny = __riscv_vmerge_vvm_f32m2(sub_y, add_y, same_direction, vl);
  nz = __riscv_vmerge_vvm_f32m2(sub_z, add_z, same_direction, vl);
}

// RVV 公式 staging helper：接收已经合成且通过后续 mask 审查的 n，生成
// `(p + q).cross(n)` 的 a/b/c 和 `(q - p).dot(n)` 的 d。调用者是两个 RVV candidate；
// 它对应 production 热点循环里的纯逐点公式，不负责 ATA/ATb 累加树。
inline void
staged_symmetric_formula(vfloat32m2_t sx,
                         vfloat32m2_t sy,
                         vfloat32m2_t sz,
                         vfloat32m2_t tx,
                         vfloat32m2_t ty,
                         vfloat32m2_t tz,
                         vfloat32m2_t nx,
                         vfloat32m2_t ny,
                         vfloat32m2_t nz,
                         const std::size_t vl,
                         vfloat32m2_t& a,
                         vfloat32m2_t& b,
                         vfloat32m2_t& c,
                         vfloat32m2_t& d)
{
  // 当前候选显式保持 vfmul + vfadd/vfsub 结构，暂缓 fused multiply-add
  //（融合乘加）intrinsic，便于审计与 production float 表达式的差异。
  const vfloat32m2_t px = __riscv_vfadd_vv_f32m2(sx, tx, vl);
  const vfloat32m2_t py = __riscv_vfadd_vv_f32m2(sy, ty, vl);
  const vfloat32m2_t pz = __riscv_vfadd_vv_f32m2(sz, tz, vl);
  const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(tx, sx, vl);
  const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(ty, sy, vl);
  const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(tz, sz, vl);

  a = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(py, nz, vl),
                             __riscv_vfmul_vv_f32m2(pz, ny, vl),
                             vl);
  b = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(pz, nx, vl),
                             __riscv_vfmul_vv_f32m2(px, nz, vl),
                             vl);
  c = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(px, ny, vl),
                             __riscv_vfmul_vv_f32m2(py, nx, vl),
                             vl);
  d = __riscv_vfmul_vv_f32m2(dx, nx, vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(dy, ny, vl), vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(dz, nz, vl), vl);
}

// 压缩和标量尾段 helper：把 RVV 生成的 a/b/c/d/n 按 keep mask 做 vcompress（保序压缩），
// 再按压缩后顺序累加 ATA/ATb。它对应 production 中“有效点按扫描顺序贡献法方程”的
// 可见语义；反汇编显示编译器可能把尾段自动形成 vfredosum.vs，所以这里是诊断边界，
// 不是 production-ready（可接入生产）规约方案。
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
  // vcompress（保序压缩）保证 scalar tail 看到的 lane 顺序与原逐点扫描一致。
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
    const float a_f = a_buf[lane];
    const float b_f = b_buf[lane];
    const float c_f = c_buf[lane];
    const float nx_f = nx_buf[lane];
    const float ny_f = ny_buf[lane];
    const float nz_f = nz_buf[lane];
    const float d_f = d_buf[lane];

    eq.ata.coeffRef(0) += a_f * a_f;
    eq.ata.coeffRef(1) += a_f * b_f;
    eq.ata.coeffRef(2) += a_f * c_f;
    eq.ata.coeffRef(3) += a_f * nx_f;
    eq.ata.coeffRef(4) += a_f * ny_f;
    eq.ata.coeffRef(5) += a_f * nz_f;
    eq.ata.coeffRef(7) += b_f * b_f;
    eq.ata.coeffRef(8) += b_f * c_f;
    eq.ata.coeffRef(9) += b_f * nx_f;
    eq.ata.coeffRef(10) += b_f * ny_f;
    eq.ata.coeffRef(11) += b_f * nz_f;
    eq.ata.coeffRef(14) += c_f * c_f;
    eq.ata.coeffRef(15) += c_f * nx_f;
    eq.ata.coeffRef(16) += c_f * ny_f;
    eq.ata.coeffRef(17) += c_f * nz_f;
    eq.ata.coeffRef(21) += nx_f * nx_f;
    eq.ata.coeffRef(22) += nx_f * ny_f;
    eq.ata.coeffRef(23) += nx_f * nz_f;
    eq.ata.coeffRef(28) += ny_f * ny_f;
    eq.ata.coeffRef(29) += ny_f * nz_f;
    eq.ata.coeffRef(35) += nz_f * nz_f;

    eq.atb.coeffRef(0) += a_f * d_f;
    eq.atb.coeffRef(1) += b_f * d_f;
    eq.atb.coeffRef(2) += c_f * d_f;
    eq.atb.coeffRef(3) += nx_f * d_f;
    eq.atb.coeffRef(4) += ny_f * d_f;
    eq.atb.coeffRef(5) += nz_f * d_f;
    ++eq.accepted_points;
  }
}
#endif // __RVV10__

// 全云 RVV candidate：映射公开 full-cloud 入口，使用 stride load（跨步加载）读取
// PointNormal 的 AoS（结构数组）字段。它是板卡 1.30x 收益的证据入口；不覆盖 indices、
// 泛型点类型或真实 production dispatch（分流逻辑）。
inline NormalEquation
accumulate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                          const pcl::PointCloud<pcl::PointNormal>& target,
                          const bool enforce_same_direction_normals,
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
      vfloat32m2_t sx, sy, sz, tx, ty, tz, n1x, n1y, n1z, n2x, n2y, n2z;
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          src_base + i * sizeof(pcl::PointNormal), vl, sx, sy, sz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, tx, ty, tz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
          src_base + i * sizeof(pcl::PointNormal), vl, n1x, n1y, n1z);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, n2x, n2y, n2z);

      vfloat32m2_t nx, ny, nz;
      select_symmetric_normal_v(
          n1x, n1y, n1z, n2x, n2y, n2z, enforce_same_direction_normals, vl, nx, ny, nz);
      vbool16_t keep = finite_mask_f32m2(sx, vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(tx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(ty, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(tz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(nx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(ny, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(nz, vl), vl);

      vfloat32m2_t a, b, c, d;
      staged_symmetric_formula(sx, sy, sz, tx, ty, tz, nx, ny, nz, vl, a, b, c, d);
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
  return accumulate_std_full(source, target, enforce_same_direction_normals, stats);
}

// 对应关系 RVV candidate：映射公开 correspondences 入口，但为了 gather 需要先标量展开
// source/target index，再转换为 byte offset（字节偏移）。它证明 indexed row 可以正确对拍；
// 板卡退化只说明当前组合不接 production，不能单独归因到 gather、展开、压缩或 tail。
inline NormalEquation
accumulate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                     const pcl::PointCloud<pcl::PointNormal>& target,
                                     const pcl::Correspondences& correspondences,
                                     const bool enforce_same_direction_normals,
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

        vfloat32m2_t sx, sy, sz, tx, ty, tz, n1x, n1y, n1z, n2x, n2y, n2z;
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            src_base, src_offsets, vl, sx, sy, sz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            tgt_base, tgt_offsets, vl, tx, ty, tz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
            src_base, src_offsets, vl, n1x, n1y, n1z);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
            tgt_base, tgt_offsets, vl, n2x, n2y, n2z);

        vfloat32m2_t nx, ny, nz;
        select_symmetric_normal_v(n1x,
                                  n1y,
                                  n1z,
                                  n2x,
                                  n2y,
                                  n2z,
                                  enforce_same_direction_normals,
                                  vl,
                                  nx,
                                  ny,
                                  nz);
        vbool16_t keep = finite_mask_f32m2(sx, vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(tx, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(ty, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(tz, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(nx, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(ny, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(nz, vl), vl);

        vfloat32m2_t a, b, c, d;
        staged_symmetric_formula(sx, sy, sz, tx, ty, tz, nx, ny, nz, vl, a, b, c, d);
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
  return accumulate_std_correspondences(
      source, target, correspondences, enforce_same_direction_normals, stats);
}

// 完整全云标量估计 wrapper：测试用它输出 4x4 matrix，而不是只比较 ATA/ATb。
// 它让公开 estimator、标量诊断和 RVV candidate 都落到同一 checksum 证据面上。
inline Matrix4f
estimate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                  const pcl::PointCloud<pcl::PointNormal>& target,
                  const bool enforce_same_direction_normals,
                  AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_std_full(source, target, enforce_same_direction_normals, stats));
}

// 完整全云 RVV 估计 wrapper：bench 默认测量这个入口的 normal-equation 构造、
// Eigen solve 和矩阵构造总成本；它仍然是 test-rvv diagnostic，不是 production direct。
inline Matrix4f
estimate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                        const pcl::PointCloud<pcl::PointNormal>& target,
                        const bool enforce_same_direction_normals,
                        AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full(source, target, enforce_same_direction_normals, stats));
}

// 完整 correspondences 标量估计 wrapper：保留公开对应关系列表顺序和重复 index 语义，
// 供 `StdCorrespondencesMatchesPublicEstimator` 和 RVV gather 对拍使用。
inline Matrix4f
estimate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                             const pcl::PointCloud<pcl::PointNormal>& target,
                             const pcl::Correspondences& correspondences,
                             const bool enforce_same_direction_normals,
                             AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_std_correspondences(
      source, target, correspondences, enforce_same_direction_normals, stats));
}

// 完整 correspondences RVV 估计 wrapper：用于 correctness 和 bench 诊断 indexed row 路径。
// 当前板卡证据为负向，因此它的证据角色是“保持诊断和阻止生产接入”，不是候选生产分流。
inline Matrix4f
estimate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                   const pcl::PointCloud<pcl::PointNormal>& target,
                                   const pcl::Correspondences& correspondences,
                                   const bool enforce_same_direction_normals,
                                   AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_correspondences(
      source, target, correspondences, enforce_same_direction_normals, stats));
}

// bench checksum helper：把 4x4 输出矩阵压成稳定标量，供 std/RVV 日志形状和数值漂移对比。
// 它不是数学正确性证明本身，必须和 gtest 逐元素断言、QEMU 日志和板卡日志一起使用。
inline double
matrix_checksum(const Matrix4f& matrix)
{
  double checksum = 0.0;
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      checksum += static_cast<double>(matrix(row, col)) * (1.0 + row * 4 + col);
  return checksum;
}

} // namespace pcl::registration::rvv_te_symmetric_pt2plane_lls_diag
