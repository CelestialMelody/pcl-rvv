/*
 * 本文件放 organized_fast_mesh 诊断测试共享的小类型和 checksum helper。
 * 它只描述 test-rvv 输入、输出和证据口径，不改变 PCL production 类型。
 */

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/Vertices.h>

#include <cstdint>
#include <string>
#include <vector>

namespace pcl::surface::rvv_ofm_support
{

enum class MeshKind
{
  quad,
  right_cut,
  left_cut,
  adaptive_cut
};

struct MeshOptions
{
  int width{0};
  int height{0};
  int row_step{1};
  int column_step{1};
  MeshKind kind{MeshKind::quad};
};

struct MeshRunResult
{
  std::vector<pcl::Vertices> polygons;
  std::uint64_t checksum{1469598103934665603ull};
};

inline const char*
meshKindName(const MeshKind kind)
{
  switch (kind) {
    case MeshKind::quad: return "quad";
    case MeshKind::right_cut: return "right_cut";
    case MeshKind::left_cut: return "left_cut";
    case MeshKind::adaptive_cut: return "adaptive_cut";
  }
  return "unknown";
}

inline std::uint64_t
mixChecksum(std::uint64_t checksum, const std::uint64_t value)
{
  return (checksum ^ value) * 1099511628211ull;
}

inline std::uint64_t
polygonChecksum(const std::vector<pcl::Vertices>& polygons)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, polygons.size());
  for (const auto& polygon : polygons) {
    checksum = mixChecksum(checksum, polygon.vertices.size());
    for (const auto vertex : polygon.vertices)
      checksum = mixChecksum(checksum, static_cast<std::uint64_t>(vertex + 1u));
  }
  return checksum;
}

inline bool
samePolygons(const std::vector<pcl::Vertices>& lhs,
             const std::vector<pcl::Vertices>& rhs)
{
  if (lhs.size() != rhs.size())
    return false;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].vertices != rhs[i].vertices)
      return false;
  }
  return true;
}

}  // namespace pcl::surface::rvv_ofm_support
