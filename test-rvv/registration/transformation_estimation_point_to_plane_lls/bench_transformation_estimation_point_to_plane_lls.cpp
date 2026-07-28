/*
 * 本文件做什么：
 * 这个 benchmark（性能测试）同时保留三类 case：
 * - diagnostic direct：显式调用 test-rvv helper，用于归因和历史方案复核。
 * - public-entry-shaped：输入/输出仿照公开 full-cloud overload，但 RVV 侧仍调用
 *   bench-only shim；它不等于 production dispatch。
 * - production-dispatch：std/RVV 两侧都调用真实公开 full-cloud overload，是当前
 *   f32 AoS layout-gated full-cloud production evidence 的 bench 入口。
 *
 * 输出合同：
 * 每个 case 输出 avg ms/iter、Total Time 和 checksum（校验和），供 QEMU correctness
 *（QEMU 正确性验证，不代表真实性能）与 board evidence（板卡证据）共用解析脚本。
 * source-indexed 和 dual-indices case 的 index vector 在计时前生成；correspondences
 * case 的 query/match 展开属于被测 candidate 的入口成本。
 * QEMU timing 永远只作路径和日志格式信号；性能结论只使用板卡/目标硬件数据。
 */

#include "test_support_transformation_estimation_point_to_plane_lls.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace support = pcl::registration::rvv_te_pt2plane_lls_support;

namespace {

pcl::PointCloud<pcl::PointNormal>
makeCloudWithAtLeast(const std::size_t target_size)
{
  // bench 输入固定为解析曲面，避免随机数让 std/RVV checksum（校验和）不可复现。
  const int radius = static_cast<int>(std::ceil(std::sqrt(target_size) / 2.0));
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
  for (int ix = -radius; ix <= radius; ++ix) {
    for (int iy = -radius; iy <= radius; ++iy) {
      const float x = static_cast<float>(ix) * 0.025f;
      const float y = static_cast<float>(iy) * 0.025f;
      pcl::PointNormal point;
      point.x = x;
      point.y = y;
      point.z = 0.08f * x * x + 0.16f * x * y - 0.22f * y + 0.7f;
      point.normal_x = -0.16f * x - 0.16f * y;
      point.normal_y = -0.16f * x + 0.22f;
      point.normal_z = 1.0f;
      const float norm = std::sqrt(point.normal_x * point.normal_x +
                                   point.normal_y * point.normal_y +
                                   point.normal_z * point.normal_z);
      point.normal_x /= norm;
      point.normal_y /= norm;
      point.normal_z /= norm;
      cloud.push_back(point);
      if (cloud.size() == target_size) {
        cloud.width = cloud.size();
        return cloud;
      }
    }
  }
  cloud.width = cloud.size();
  return cloud;
}

Eigen::Matrix4f
makeTransform()
{
  // 与专项测试保持同一温和刚体变换，减少 bench 与 correctness case 的解释差异。
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform.row(0) << 0.9938f, 0.0988f, 0.0517f, 0.1000f;
  transform.row(1) << -0.0997f, 0.9949f, 0.0149f, -0.2000f;
  transform.row(2) << -0.0500f, -0.0200f, 0.9986f, 0.3000f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

pcl::PointCloud<pcl::PointXYZ>
copySourceAsXYZ(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // production-dispatch generic case 用它隔离 source 只读 xyz 的字段合同。
  pcl::PointCloud<pcl::PointXYZ> xyz;
  xyz.height = 1;
  xyz.is_dense = source.is_dense;
  xyz.reserve(source.size());
  for (const auto& point : source) {
    pcl::PointXYZ copy;
    copy.x = point.x;
    copy.y = point.y;
    copy.z = point.z;
    xyz.push_back(copy);
  }
  xyz.width = xyz.size();
  return xyz;
}

pcl::PointCloud<pcl::PointXYZINormal>
copyTargetAsXYZINormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  // production-dispatch generic target case 用它隔离 target 的 xyz+normal f32 AoS
  // 字段合同；intensity 不参与 TEPTPL full-cloud row。
  pcl::PointCloud<pcl::PointXYZINormal> copy_cloud;
  copy_cloud.height = 1;
  copy_cloud.is_dense = target.is_dense;
  copy_cloud.reserve(target.size());
  for (const auto& point : target) {
    pcl::PointXYZINormal copy;
    copy.x = point.x;
    copy.y = point.y;
    copy.z = point.z;
    copy.normal_x = point.normal_x;
    copy.normal_y = point.normal_y;
    copy.normal_z = point.normal_z;
    copy.intensity = 1.0f;
    copy_cloud.push_back(copy);
  }
  copy_cloud.width = copy_cloud.size();
  return copy_cloud;
}

pcl::Correspondences
makeCorrespondences(const std::size_t n)
{
  // same-index subset 专门给 gather bench 使用；query/match 相同，因此访问相关性较好。
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  for (std::size_t i = 3; i < n; i += 5)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  return correspondences;
}

pcl::Correspondences
makeLocalOffsetCorrespondences(const std::size_t n)
{
  // local-offset 分布让 target match 偏离 query，但仍保持局部性。它用于判断
  // correspondences 正向是否依赖 `index_query == index_match` 这种过强相关性。
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  if (n == 0)
    return correspondences;
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>((i + 17) % n), 0.0f);
  for (std::size_t i = 3; i < n; i += 5)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>((i + 31) % n), 0.0f);
  return correspondences;
}

pcl::Indices
makeIndexedRows(const std::size_t n)
{
  // source 侧有效 index stream：非连续、含重复，但没有 query/match 展开阶段。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  if (n > 10) {
    indices.push_back(10);
    indices.push_back(2);
  }
  for (std::size_t i = 3; i < n; i += 5)
    indices.push_back(static_cast<int>(i));
  for (std::size_t i = 11; i < n; i += 41)
    indices.push_back(static_cast<int>(i));
  return indices;
}

pcl::Indices
makeIndependentTargetIndexedRows(const std::size_t n)
{
  // target 侧使用独立有效 index stream，让 dual-indices bench 覆盖两条索引流。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 1; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  if (n > 11) {
    indices.push_back(11);
    indices.push_back(3);
  }
  for (std::size_t i = 4; i < n; i += 5)
    indices.push_back(static_cast<int>(i));
  for (std::size_t i = 17; i < n; i += 37)
    indices.push_back(static_cast<int>(i));
  return indices;
}

pcl::Correspondences
makeCorrespondencesFromIndices(const pcl::Indices& source_indices,
                               const pcl::Indices& target_indices)
{
  // independent-stream correspondences 复用 dual-indices 的两条 index stream。
  // 它让板卡结果可以回答：同样的索引分布下，correspondence 展开和 baseline 差异有多大。
  pcl::Correspondences correspondences;
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  correspondences.reserve(n);
  for (std::size_t row = 0; row < n; ++row)
    correspondences.emplace_back(source_indices[row], target_indices[row], 0.0f);
  return correspondences;
}

template <typename PointT>
pcl::PointCloud<PointT>
copyIndexedCloud(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  // source-indexed 入口要求 target 是紧凑 full-cloud；拷贝发生在计时前。
  pcl::PointCloud<PointT> subset;
  subset.height = 1;
  subset.is_dense = cloud.is_dense;
  subset.reserve(indices.size());
  for (const int index : indices)
    subset.push_back(cloud[static_cast<std::size_t>(index)]);
  subset.width = subset.size();
  return subset;
}

struct BenchResult {
  std::string name;
  double average_ms = 0.0;
  double total_ms = 0.0;
  double checksum = 0.0;
};

struct BenchOptions {
  int iterations = 20;
  std::vector<std::size_t> sizes = {65536u, 262144u};
  std::string case_filter;
  bool component_only = false;
};

struct ComponentSoA {
  std::vector<float> sx;
  std::vector<float> sy;
  std::vector<float> sz;
  std::vector<float> dx;
  std::vector<float> dy;
  std::vector<float> dz;
  std::vector<float> nx;
  std::vector<float> ny;
  std::vector<float> nz;
};

struct FormulaSoA {
  std::vector<float> a;
  std::vector<float> b;
  std::vector<float> c;
  std::vector<float> d;
  std::vector<float> nx;
  std::vector<float> ny;
  std::vector<float> nz;
};

std::vector<std::size_t>
parseSizes(const std::string& text)
{
  std::vector<std::size_t> sizes;
  std::stringstream stream(text);
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (item.empty())
      continue;
    sizes.push_back(static_cast<std::size_t>(std::stoul(item)));
  }
  return sizes;
}

BenchOptions
parseOptions(const int argc, char** argv)
{
  BenchOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto readValue = [&](const std::string& name) -> std::string {
      const std::string prefix = name + "=";
      if (arg.rfind(prefix, 0) == 0)
        return arg.substr(prefix.size());
      if (arg == name && i + 1 < argc)
        return argv[++i];
      return {};
    };
    if (const std::string value = readValue("--iterations"); !value.empty()) {
      options.iterations = std::atoi(value.c_str());
    }
    else if (const std::string value = readValue("--size"); !value.empty()) {
      options.sizes = parseSizes(value);
    }
    else if (const std::string value = readValue("--case-filter"); !value.empty()) {
      options.case_filter = value;
    }
    else if (arg == "--component-only") {
      options.component_only = true;
    }
  }
  if (options.iterations <= 0)
    options.iterations = 1;
  if (options.sizes.empty())
    options.sizes = {65536u, 262144u};
  return options;
}

ComponentSoA
makeFullSoA(const pcl::PointCloud<pcl::PointNormal>& source,
            const pcl::PointCloud<pcl::PointNormal>& target)
{
  ComponentSoA soa;
  const std::size_t n = std::min(source.size(), target.size());
  soa.sx.reserve(n);
  soa.sy.reserve(n);
  soa.sz.reserve(n);
  soa.dx.reserve(n);
  soa.dy.reserve(n);
  soa.dz.reserve(n);
  soa.nx.reserve(n);
  soa.ny.reserve(n);
  soa.nz.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    soa.sx.push_back(source[i].x);
    soa.sy.push_back(source[i].y);
    soa.sz.push_back(source[i].z);
    soa.dx.push_back(target[i].x);
    soa.dy.push_back(target[i].y);
    soa.dz.push_back(target[i].z);
    soa.nx.push_back(target[i].normal_x);
    soa.ny.push_back(target[i].normal_y);
    soa.nz.push_back(target[i].normal_z);
  }
  return soa;
}

FormulaSoA
makeFormulaSoA(const ComponentSoA& soa)
{
  FormulaSoA formula;
  const std::size_t n = soa.sx.size();
  formula.a.reserve(n);
  formula.b.reserve(n);
  formula.c.reserve(n);
  formula.d.reserve(n);
  formula.nx.reserve(n);
  formula.ny.reserve(n);
  formula.nz.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float sx = soa.sx[i];
    const float sy = soa.sy[i];
    const float sz = soa.sz[i];
    const float dx = soa.dx[i];
    const float dy = soa.dy[i];
    const float dz = soa.dz[i];
    const float nx = soa.nx[i];
    const float ny = soa.ny[i];
    const float nz = soa.nz[i];
    formula.a.push_back(nz * sy - ny * sz);
    formula.b.push_back(nx * sz - nz * sx);
    formula.c.push_back(ny * sx - nx * sy);
    formula.d.push_back(nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz);
    formula.nx.push_back(nx);
    formula.ny.push_back(ny);
    formula.nz.push_back(nz);
  }
  return formula;
}

double
normalEquationChecksum(const support::NormalEquation& eq)
{
  double checksum = static_cast<double>(eq.accepted_points) * 1e-6;
  for (int row = 0; row < 6; ++row)
    for (int col = 0; col < 6; ++col)
      checksum += eq.ata(row, col) * (1.0 + row * 6 + col) * 1e-9;
  for (int row = 0; row < 6; ++row)
    checksum += eq.atb(row) * (1.0 + row) * 1e-6;
  return checksum;
}

void
estimatePublicEntryShapedFullCloudBlock(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    Eigen::Matrix4f& matrix)
{
#ifdef __RVV10__
  // Bench-only shim: the signature and call site mirror the public full-cloud
  // overload, but production dispatch is not changed or exercised here.
  matrix = support::estimate_candidate_full_block_reduction(source, target);
#else
  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  estimator.estimateRigidTransformation(source, target, matrix);
#endif
}

double
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

double
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

double
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

double
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

double
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

template <typename Fn>
BenchResult
runCase(const std::string& name, const int iterations, Fn&& fn)
{
  // runCase 统一输出合同需要的 avg、Total Time 和 checksum，供 QEMU 与板卡日志共用。
  double checksum = 0.0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto stop = std::chrono::steady_clock::now();
  const double total_ms =
      std::chrono::duration<double, std::milli>(stop - start).count();
  return BenchResult{name, total_ms / iterations, total_ms, checksum};
}

template <typename Fn>
void
appendCase(std::vector<BenchResult>& results,
           const BenchOptions& options,
           const std::string& name,
           Fn&& fn)
{
  if (!options.case_filter.empty() &&
      name.find(options.case_filter) == std::string::npos)
    return;
  results.push_back(runCase(name, options.iterations, std::forward<Fn>(fn)));
}

} // namespace

int
main(int argc, char** argv)
{
  const BenchOptions options = parseOptions(argc, argv);
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "Dataset: synthetic point-to-plane LLS normal-equation diagnostic\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  if (!options.case_filter.empty())
    std::cout << "Case filter: " << options.case_filter << "\n";
  if (options.component_only)
    std::cout << "Mode: component-only\n";
#ifdef __RVV10__
  std::cout << "Build: rvv\n";
#else
  std::cout << "Build: std\n";
#endif

  std::vector<BenchResult> results;
  for (const auto n : options.sizes) {
    pcl::PointCloud<pcl::PointNormal> source = makeCloudWithAtLeast(n);
    pcl::PointCloud<pcl::PointNormal> target;
    pcl::transformPointCloudWithNormals(source, target, makeTransform());
    const pcl::PointCloud<pcl::PointXYZ> source_xyz = copySourceAsXYZ(source);
    const pcl::PointCloud<pcl::PointXYZINormal> target_xyzinormal =
        copyTargetAsXYZINormal(target);

    const pcl::Indices indexed_rows = makeIndexedRows(source.size());
    const pcl::PointCloud<pcl::PointNormal> compact_target =
        copyIndexedCloud(target, indexed_rows);
    const pcl::Indices target_indexed_rows =
        makeIndependentTargetIndexedRows(target.size());

    if (options.component_only) {
      const ComponentSoA full_soa = makeFullSoA(source, target);
      const FormulaSoA full_formula = makeFormulaSoA(full_soa);
      appendCase(
          results,
          options,
          "lls component full-cloud load-store-only pointnormal " +
              std::to_string(n),
          [&]() { return componentFullLoadStoreOnly(source, target); });
      appendCase(
          results,
          options,
          "lls component dual-indices independent-stream gather-load-store-only pointnormal " +
              std::to_string(n),
          [&]() {
            return componentDualGatherLoadStoreOnly(
                source, indexed_rows, target, target_indexed_rows);
          });
      appendCase(
          results,
          options,
          "lls component full-cloud formula-store-only pointnormal " +
              std::to_string(n),
          [&]() { return componentFormulaStoreOnly(full_soa); });
      appendCase(
          results,
          options,
          "lls component full-cloud mask-compress-only pointnormal " +
              std::to_string(n),
          [&]() { return componentMaskCompressOnly(full_soa, full_formula); });
      appendCase(
          results,
          options,
          "lls component full-cloud tail-only pointnormal " + std::to_string(n),
          [&]() { return componentTailOnly(full_formula); });
      appendCase(
          results,
          options,
          "lls component full-cloud no-solve pointnormal " + std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud fused-reduction no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_fused_reduction(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud grouped-reduction no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_grouped_reduction(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud block-reduction no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_block_reduction(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      continue;
    }

    appendCase(
        results,
        options,
        "lls normal-equation full-cloud pointnormal " + std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    // Historical diagnostic: trusted-dense/fused/grouped/block rows separate local
    // implementation choices from production dispatch. Only block currently maps to
    // production, and only through the explicit production-dispatch case below.
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_trusted_dense(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud fused-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_fused_reduction(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud grouped-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_grouped_reduction(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud block-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_block_reduction(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    // Production-dispatch evidence: both std and RVV builds enter the same public
    // overload. Under __RVV10__ the production gate may select the f32 AoS
    // layout-gated full-cloud block path; otherwise it is the scalar path.
    appendCase(
        results,
        options,
        "lls production-dispatch full-cloud pointnormal " + std::to_string(n),
        [&]() {
          pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                                     pcl::PointNormal>
              estimator;
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimator.estimateRigidTransformation(source, target, matrix);
          return support::matrix_checksum(matrix);
        });
    appendCase(
        results,
        options,
        "lls production-dispatch full-cloud pointxyz-to-pointnormal " +
            std::to_string(n),
        [&]() {
          pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                                     pcl::PointNormal>
              estimator;
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimator.estimateRigidTransformation(source_xyz, target, matrix);
          return support::matrix_checksum(matrix);
        });
    appendCase(
        results,
        options,
        "lls production-dispatch full-cloud pointxyz-to-pointxyzinormal " +
            std::to_string(n),
        [&]() {
          pcl::registration::TransformationEstimationPointToPlaneLLS<
              pcl::PointXYZ,
              pcl::PointXYZINormal>
              estimator;
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimator.estimateRigidTransformation(source_xyz, target_xyzinormal, matrix);
          return support::matrix_checksum(matrix);
        });
    // Bench-only shape check: useful when comparing public-overload overhead to the
    // block helper, but it does not prove real dispatch or fallback behavior.
    appendCase(
        results,
        options,
        "lls public-entry-shaped full-cloud block-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimatePublicEntryShapedFullCloudBlock(source, target, matrix);
          return support::matrix_checksum(matrix);
        });

    // Historical indexed diagnostics: retained to explain why this closeout does not
    // expand beyond full-cloud. Negative results here must not be collapsed into a
    // single gather-only cause without a separate profile or ablation.
    appendCase(
        results,
        options,
        "lls normal-equation source-indices pointnormal " + std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_source_indices(
              source, indexed_rows, compact_target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation source-indices trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_source_indices_trusted_dense(
                  source, indexed_rows, compact_target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    appendCase(
        results,
        options,
        "lls normal-equation dual-indices same-stream pointnormal " + std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_dual_indices(
              source, indexed_rows, target, indexed_rows, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    appendCase(
        results,
        options,
        "lls normal-equation dual-indices independent-stream pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_dual_indices(
              source, indexed_rows, target, target_indexed_rows, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation dual-indices independent-stream trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_dual_indices_trusted_dense(
                  source, indexed_rows, target, target_indexed_rows, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    // Historical correspondence diagnostics: query/match expansion is included in
    // timing, so these rows are not interchangeable with dual-indices gather rows.
    const pcl::Correspondences correspondences = makeCorrespondences(source.size());
    appendCase(
        results,
        options,
        "lls normal-equation correspondences same-index pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_correspondences(
              source, target, correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    const pcl::Correspondences local_offset_correspondences =
        makeLocalOffsetCorrespondences(source.size());
    appendCase(
        results,
        options,
        "lls normal-equation correspondences local-offset pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_correspondences(
              source, target, local_offset_correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    const pcl::Correspondences independent_stream_correspondences =
        makeCorrespondencesFromIndices(indexed_rows, target_indexed_rows);
    appendCase(
        results,
        options,
        "lls normal-equation correspondences independent-stream pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_correspondences(
              source, target, independent_stream_correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation correspondences independent-stream trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_correspondences_trusted_dense(
                  source, target, independent_stream_correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
  }

  double checksum = 0.0;
  double total = 0.0;
  for (const auto& result : results) {
    checksum += result.checksum;
    total += result.total_ms;
    std::cout << result.name << ": " << result.average_ms << " ms/iter\n";
    std::cout << "  Total Time: " << result.total_ms << " ms\n";
    std::cout << "  Checksum: " << result.checksum << "\n";
  }
  std::cout << "Total Time: " << total << " ms\n";
  std::cout << "Checksum: " << checksum << "\n";
  return 0;
}
