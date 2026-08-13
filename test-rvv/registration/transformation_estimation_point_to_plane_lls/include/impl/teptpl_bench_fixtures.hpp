/*
 * 本文件做什么：
 * TEPTPL bench 使用的确定性输入、CLI option、SoA staging 和 checksum helper。
 * 它只准备 benchmark（性能测试）输入与输出合同，不单独证明 production dispatch。
 */

#pragma once

#include "teptpl_candidates.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_bench {

namespace support = pcl::registration::rvv_te_pt2plane_lls_support;

inline pcl::PointCloud<pcl::PointNormal>
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

inline Eigen::Matrix4f
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

inline pcl::PointCloud<pcl::PointXYZ>
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

inline pcl::PointCloud<pcl::PointXYZINormal>
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

inline pcl::Correspondences
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

inline pcl::Correspondences
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

inline pcl::Indices
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

inline pcl::Indices
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

inline pcl::Correspondences
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
inline pcl::PointCloud<PointT>
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

inline std::vector<std::size_t>
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

inline BenchOptions
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

inline ComponentSoA
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

inline FormulaSoA
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

inline double
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

inline void
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

} // namespace pcl::registration::rvv_te_pt2plane_lls_bench
