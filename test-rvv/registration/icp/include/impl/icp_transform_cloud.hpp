/*
 * 本文件做什么：
 * 这里复刻 `IterativeClosestPoint::transformCloud` 的 test-only 标量 reference（参考链路）
 * 和 RVV candidate（RVV 候选链路）。它使用运行期字段 offset（字段偏移）和字节访问，
 * 对齐 production 的 `memcpy` 语义，并显式支持 input/output 是同一个点云的 in-place（原地写回）形态。
 *
 * 证据边界：
 * 这是 production-shaped diagnostic（生产形态诊断），不是 production direct（真实生产路径证据）。
 * 当前候选只覆盖全云顺序扫描；ICP 的 correspondence search（对应搜索）和 transform estimation
 * （变换估计）仍由原 production 链路处理。
 */

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace pcl::registration::rvv_icp_support {

struct FieldLayout {
  std::size_t x{0};
  std::size_t y{0};
  std::size_t z{0};
  std::size_t normal_x{0};
  std::size_t normal_y{0};
  std::size_t normal_z{0};
};

struct TransformStats {
  std::size_t input_points{0};
  std::size_t xyz_written{0};
  std::size_t normals_written{0};
  bool used_rvv{false};
  bool used_fallback{false};
};

inline FieldLayout
pointXYZLayout()
{
  return {offsetof(pcl::PointXYZ, x),
          offsetof(pcl::PointXYZ, y),
          offsetof(pcl::PointXYZ, z),
          0,
          0,
          0};
}

inline FieldLayout
pointNormalLayout()
{
  return {offsetof(pcl::PointNormal, x),
          offsetof(pcl::PointNormal, y),
          offsetof(pcl::PointNormal, z),
          offsetof(pcl::PointNormal, normal_x),
          offsetof(pcl::PointNormal, normal_y),
          offsetof(pcl::PointNormal, normal_z)};
}

inline Eigen::Matrix4f
makeRigidTransform()
{
  Eigen::Matrix4f t = Eigen::Matrix4f::Identity();
  t(0, 0) = 0.9362934f;
  t(0, 1) = -0.3129918f;
  t(0, 2) = 0.1593451f;
  t(1, 0) = 0.2896295f;
  t(1, 1) = 0.9447025f;
  t(1, 2) = 0.1537920f;
  t(2, 0) = -0.1986693f;
  t(2, 1) = -0.0978434f;
  t(2, 2) = 0.9751704f;
  t(0, 3) = 0.12f;
  t(1, 3) = -0.07f;
  t(2, 3) = 0.045f;
  return t;
}

inline pcl::PointCloud<pcl::PointXYZ>
makePointXYZCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 180.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 210.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 240.0f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointNormal>
makePointNormalCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float a = static_cast<float>((i % 997) + 1) * 0.0031f;
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 200.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 4093) - 2046) / 230.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 4091) - 2045) / 260.0f;
    cloud[i].normal_x = std::cos(a) * 0.77f;
    cloud[i].normal_y = std::sin(a) * 0.63f;
    cloud[i].normal_z = 0.24f + std::cos(a * 0.5f) * 0.31f;
  }
  return cloud;
}

inline bool
finiteFloat(float v)
{
  return std::isfinite(v);
}

inline std::uint64_t
mixChecksum(std::uint64_t hash, float value)
{
  // Bench checksum 是日志形状和路径指纹；正式数值一致性由 gtest 的误差预算负责。
  // RVV 候选使用 FMA 后会有细小舍入差异，过细指纹会把合法误差误报成 checksum 不一致。
  const auto bucket =
      static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 100.0));
  hash ^= static_cast<std::uint64_t>(bucket);
  hash *= 1099511628211ull;
  return hash;
}

template <typename PointT>
std::uint64_t
checksumXYZ(const pcl::PointCloud<PointT>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t i = 0; i < cloud.size(); i += 17) {
    hash = mixChecksum(hash, cloud[i].x);
    hash = mixChecksum(hash, cloud[i].y);
    hash = mixChecksum(hash, cloud[i].z);
  }
  return hash ^ cloud.size();
}

inline std::uint64_t
checksumXYZNormal(const pcl::PointCloud<pcl::PointNormal>& cloud)
{
  std::uint64_t hash = checksumXYZ(cloud);
  for (std::size_t i = 0; i < cloud.size(); i += 19) {
    hash = mixChecksum(hash, cloud[i].normal_x);
    hash = mixChecksum(hash, cloud[i].normal_y);
    hash = mixChecksum(hash, cloud[i].normal_z);
  }
  return hash ^ (cloud.size() << 1);
}

template <typename PointT>
TransformStats
transformCloudStd(const pcl::PointCloud<PointT>& input,
                  pcl::PointCloud<PointT>& output,
                  const Eigen::Matrix4f& transform,
                  const FieldLayout& layout,
                  bool has_normals)
{
  TransformStats stats;
  stats.input_points = input.size();
  stats.used_fallback = true;
  output = input;

  const Eigen::Matrix3f rot = transform.template block<3, 3>(0, 0);
  Eigen::Vector4f pt(0.0f, 0.0f, 0.0f, 1.0f), pt_t;
  Eigen::Vector3f nt, nt_t;

  for (std::size_t i = 0; i < input.size(); ++i) {
    const auto* data_in = reinterpret_cast<const std::uint8_t*>(&input[i]);
    auto* data_out = reinterpret_cast<std::uint8_t*>(&output[i]);
    std::memcpy(&pt[0], data_in + layout.x, sizeof(float));
    std::memcpy(&pt[1], data_in + layout.y, sizeof(float));
    std::memcpy(&pt[2], data_in + layout.z, sizeof(float));

    if (!finiteFloat(pt[0]) || !finiteFloat(pt[1]) || !finiteFloat(pt[2]))
      continue;

    pt_t.noalias() = transform * pt;
    std::memcpy(data_out + layout.x, &pt_t[0], sizeof(float));
    std::memcpy(data_out + layout.y, &pt_t[1], sizeof(float));
    std::memcpy(data_out + layout.z, &pt_t[2], sizeof(float));
    ++stats.xyz_written;

    if (!has_normals)
      continue;

    std::memcpy(&nt[0], data_in + layout.normal_x, sizeof(float));
    std::memcpy(&nt[1], data_in + layout.normal_y, sizeof(float));
    std::memcpy(&nt[2], data_in + layout.normal_z, sizeof(float));

    if (!finiteFloat(nt[0]) || !finiteFloat(nt[1]) || !finiteFloat(nt[2]))
      continue;

    nt_t.noalias() = rot * nt;
    std::memcpy(data_out + layout.normal_x, &nt_t[0], sizeof(float));
    std::memcpy(data_out + layout.normal_y, &nt_t[1], sizeof(float));
    std::memcpy(data_out + layout.normal_z, &nt_t[2], sizeof(float));
    ++stats.normals_written;
  }

  return stats;
}

#ifdef __RVV10__
inline vbool16_t
finiteMaskF32M2(vfloat32m2_t value, const std::size_t vl)
{
  vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(value, value, vl);
  finite = __riscv_vmand_mm_b16(
      finite,
      __riscv_vmflt_vf_f32m2_b16(
          __riscv_vfabs_v_f32m2(value, vl), std::numeric_limits<float>::infinity(), vl),
      vl);
  return finite;
}

template <std::size_t kStride, std::size_t kX, std::size_t kY, std::size_t kZ>
inline TransformStats
transformPointXYZRVV(const pcl::PointCloud<pcl::PointXYZ>& input,
                     pcl::PointCloud<pcl::PointXYZ>& output,
                     const Eigen::Matrix4f& transform)
{
  TransformStats stats;
  stats.input_points = input.size();
  stats.used_rvv = true;
  output = input;

  const auto* base_in = reinterpret_cast<const std::uint8_t*>(input.data());
  auto* base_out = reinterpret_cast<std::uint8_t*>(output.data());
  for (std::size_t i = 0; i < input.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(input.size() - i);
    const std::uint8_t* in = base_in + i * sizeof(pcl::PointXYZ);
    std::uint8_t* out = base_out + i * sizeof(pcl::PointXYZ);
    vfloat32m2_t x, y, z;
    pcl::rvv_load::strided_load3_f32m2<kStride, kX, kY, kZ>(in, vl, x, y, z);

    vbool16_t keep = finiteMaskF32M2(x, vl);
    keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(y, vl), vl);
    keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(z, vl), vl);

    vfloat32m2_t tx = __riscv_vfmul_vf_f32m2(x, transform(0, 0), vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 1), y, vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 2), z, vl);
    tx = __riscv_vfadd_vf_f32m2(tx, transform(0, 3), vl);

    vfloat32m2_t ty = __riscv_vfmul_vf_f32m2(x, transform(1, 0), vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 1), y, vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 2), z, vl);
    ty = __riscv_vfadd_vf_f32m2(ty, transform(1, 3), vl);

    vfloat32m2_t tz = __riscv_vfmul_vf_f32m2(x, transform(2, 0), vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 1), y, vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 2), z, vl);
    tz = __riscv_vfadd_vf_f32m2(tz, transform(2, 3), vl);

    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep, reinterpret_cast<float*>(out + kX), tx, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep, reinterpret_cast<float*>(out + kY), ty, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep, reinterpret_cast<float*>(out + kZ), tz, vl);
    stats.xyz_written += __riscv_vcpop_m_b16(keep, vl);
    i += vl;
  }
  return stats;
}

template <std::size_t kStride,
          std::size_t kX,
          std::size_t kY,
          std::size_t kZ,
          std::size_t kNX,
          std::size_t kNY,
          std::size_t kNZ>
inline TransformStats
transformPointNormalRVV(const pcl::PointCloud<pcl::PointNormal>& input,
                        pcl::PointCloud<pcl::PointNormal>& output,
                        const Eigen::Matrix4f& transform)
{
  TransformStats stats;
  stats.input_points = input.size();
  stats.used_rvv = true;
  output = input;

  const auto* base_in = reinterpret_cast<const std::uint8_t*>(input.data());
  auto* base_out = reinterpret_cast<std::uint8_t*>(output.data());
  for (std::size_t i = 0; i < input.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(input.size() - i);
    const std::uint8_t* in = base_in + i * sizeof(pcl::PointNormal);
    std::uint8_t* out = base_out + i * sizeof(pcl::PointNormal);
    vfloat32m2_t x, y, z;
    pcl::rvv_load::strided_load3_f32m2<kStride, kX, kY, kZ>(in, vl, x, y, z);

    vbool16_t keep_xyz = finiteMaskF32M2(x, vl);
    keep_xyz = __riscv_vmand_mm_b16(keep_xyz, finiteMaskF32M2(y, vl), vl);
    keep_xyz = __riscv_vmand_mm_b16(keep_xyz, finiteMaskF32M2(z, vl), vl);

    vfloat32m2_t tx = __riscv_vfmul_vf_f32m2(x, transform(0, 0), vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 1), y, vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 2), z, vl);
    tx = __riscv_vfadd_vf_f32m2(tx, transform(0, 3), vl);

    vfloat32m2_t ty = __riscv_vfmul_vf_f32m2(x, transform(1, 0), vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 1), y, vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 2), z, vl);
    ty = __riscv_vfadd_vf_f32m2(ty, transform(1, 3), vl);

    vfloat32m2_t tz = __riscv_vfmul_vf_f32m2(x, transform(2, 0), vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 1), y, vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 2), z, vl);
    tz = __riscv_vfadd_vf_f32m2(tz, transform(2, 3), vl);

    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_xyz, reinterpret_cast<float*>(out + kX), tx, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_xyz, reinterpret_cast<float*>(out + kY), ty, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_xyz, reinterpret_cast<float*>(out + kZ), tz, vl);
    stats.xyz_written += __riscv_vcpop_m_b16(keep_xyz, vl);

    vfloat32m2_t nx, ny, nz;
    pcl::rvv_load::strided_load3_f32m2<kStride, kNX, kNY, kNZ>(in, vl, nx, ny, nz);
    vbool16_t keep_normal = finiteMaskF32M2(nx, vl);
    keep_normal = __riscv_vmand_mm_b16(keep_normal, finiteMaskF32M2(ny, vl), vl);
    keep_normal = __riscv_vmand_mm_b16(keep_normal, finiteMaskF32M2(nz, vl), vl);
    keep_normal = __riscv_vmand_mm_b16(keep_normal, keep_xyz, vl);

    vfloat32m2_t tnx = __riscv_vfmul_vf_f32m2(nx, transform(0, 0), vl);
    tnx = __riscv_vfmacc_vf_f32m2(tnx, transform(0, 1), ny, vl);
    tnx = __riscv_vfmacc_vf_f32m2(tnx, transform(0, 2), nz, vl);

    vfloat32m2_t tny = __riscv_vfmul_vf_f32m2(nx, transform(1, 0), vl);
    tny = __riscv_vfmacc_vf_f32m2(tny, transform(1, 1), ny, vl);
    tny = __riscv_vfmacc_vf_f32m2(tny, transform(1, 2), nz, vl);

    vfloat32m2_t tnz = __riscv_vfmul_vf_f32m2(nx, transform(2, 0), vl);
    tnz = __riscv_vfmacc_vf_f32m2(tnz, transform(2, 1), ny, vl);
    tnz = __riscv_vfmacc_vf_f32m2(tnz, transform(2, 2), nz, vl);

    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_normal, reinterpret_cast<float*>(out + kNX), tnx, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_normal, reinterpret_cast<float*>(out + kNY), tny, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_normal, reinterpret_cast<float*>(out + kNZ), tnz, vl);
    stats.normals_written += __riscv_vcpop_m_b16(keep_normal, vl);
    i += vl;
  }
  return stats;
}
#endif // __RVV10__

template <typename PointT>
TransformStats
transformCloudCandidate(const pcl::PointCloud<PointT>& input,
                        pcl::PointCloud<PointT>& output,
                        const Eigen::Matrix4f& transform,
                        const FieldLayout& layout,
                        bool has_normals)
{
  constexpr std::size_t kMinRVVPoints = 32;
  if (input.size() < kMinRVVPoints)
    return transformCloudStd(input, output, transform, layout, has_normals);

#ifdef __RVV10__
  if constexpr (std::is_same_v<PointT, pcl::PointXYZ>) {
    if (!has_normals && layout.x == offsetof(pcl::PointXYZ, x) &&
        layout.y == offsetof(pcl::PointXYZ, y) &&
        layout.z == offsetof(pcl::PointXYZ, z)) {
      return transformPointXYZRVV<sizeof(pcl::PointXYZ),
                                  offsetof(pcl::PointXYZ, x),
                                  offsetof(pcl::PointXYZ, y),
                                  offsetof(pcl::PointXYZ, z)>(input, output, transform);
    }
  }
  if constexpr (std::is_same_v<PointT, pcl::PointNormal>) {
    if (has_normals && layout.x == offsetof(pcl::PointNormal, x) &&
        layout.y == offsetof(pcl::PointNormal, y) &&
        layout.z == offsetof(pcl::PointNormal, z) &&
        layout.normal_x == offsetof(pcl::PointNormal, normal_x) &&
        layout.normal_y == offsetof(pcl::PointNormal, normal_y) &&
        layout.normal_z == offsetof(pcl::PointNormal, normal_z)) {
      return transformPointNormalRVV<sizeof(pcl::PointNormal),
                                     offsetof(pcl::PointNormal, x),
                                     offsetof(pcl::PointNormal, y),
                                     offsetof(pcl::PointNormal, z),
                                     offsetof(pcl::PointNormal, normal_x),
                                     offsetof(pcl::PointNormal, normal_y),
                                     offsetof(pcl::PointNormal, normal_z)>(
          input, output, transform);
    }
  }
#endif // __RVV10__
  return transformCloudStd(input, output, transform, layout, has_normals);
}

} // namespace pcl::registration::rvv_icp_support
