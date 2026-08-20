/*
 * 本文件做什么：
 * 这里是 triangulation topic 的测试支撑聚合入口。它复刻
 * surface/src/on_nurbs/triangulation.cpp 里未裁剪 surface 的参数网格、
 * triangle index 和 Evaluate 扫描语义，并在 __RVV10__ build 下提供一个
 * 测试专用 RVV candidate（候选实现）。
 *
 * 证据边界：
 * 这些 helper 属于接入前诊断（pre-production diagnostic）。它们可以证明
 * 参数网格写入的 correctness（正确性）、反汇编归属和板卡性能信号，但不能
 * 证明 production dispatch（生产分流）已经存在。
 */

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/surface/on_nurbs/triangulation.h>

#ifdef __RVV10__
#include <pcl/rvv_point_store.h>
#include <riscv_vector.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace pcl::surface::rvv_triangulation_support {

struct MeshStats {
  std::uint64_t checksum = 1469598103934665603ull;
  std::size_t points = 0;
  std::size_t polygons = 0;
};

struct GridSpec {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float z0 = 0.0f;
  float width = 1.0f;
  float height = 1.0f;
  unsigned seg_x = 1;
  unsigned seg_y = 1;
};

inline std::uint64_t
mixChecksum(std::uint64_t seed, const std::uint64_t value)
{
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

inline std::uint32_t
floatBits(const float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline void
updatePointChecksum(std::uint64_t& checksum, const pcl::PointXYZ& point)
{
  checksum = mixChecksum(checksum, floatBits(point.x));
  checksum = mixChecksum(checksum, floatBits(point.y));
  checksum = mixChecksum(checksum, floatBits(point.z));
}

inline MeshStats
statsFromCloudAndPolygons(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const std::vector<pcl::Vertices>& polygons)
{
  MeshStats stats;
  stats.points = cloud.size();
  stats.polygons = polygons.size();
  for (const auto& point : cloud)
    updatePointChecksum(stats.checksum, point);
  for (const auto& polygon : polygons) {
    stats.checksum = mixChecksum(stats.checksum, polygon.vertices.size());
    for (const auto vertex : polygon.vertices)
      stats.checksum = mixChecksum(stats.checksum, vertex);
  }
  stats.checksum = mixChecksum(stats.checksum, stats.points);
  stats.checksum = mixChecksum(stats.checksum, stats.polygons);
  return stats;
}

inline GridSpec
makeGridSpec(const unsigned resolution)
{
  GridSpec spec;
  spec.x0 = 0.0f;
  spec.y0 = 0.0f;
  spec.z0 = 0.0f;
  spec.width = 1.0f;
  spec.height = 1.0f;
  spec.seg_x = resolution;
  spec.seg_y = resolution;
  return spec;
}

inline void
createIndicesReference(std::vector<pcl::Vertices>& vertices,
                       const unsigned vidx,
                       const unsigned seg_x,
                       const unsigned seg_y)
{
  vertices.clear();
  vertices.reserve(static_cast<std::size_t>(seg_x) * seg_y * 2);
  for (unsigned j = 0; j < seg_y; ++j) {
    for (unsigned i = 0; i < seg_x; ++i) {
      const unsigned i0 = vidx + (seg_x + 1) * j + i;
      const unsigned i1 = vidx + (seg_x + 1) * j + i + 1;
      const unsigned i2 = vidx + (seg_x + 1) * (j + 1) + i + 1;
      const unsigned i3 = vidx + (seg_x + 1) * (j + 1) + i;

      pcl::Vertices v1;
      v1.vertices.reserve(3);
      v1.vertices.push_back(i0);
      v1.vertices.push_back(i1);
      v1.vertices.push_back(i2);
      vertices.push_back(v1);

      pcl::Vertices v2;
      v2.vertices.reserve(3);
      v2.vertices.push_back(i0);
      v2.vertices.push_back(i2);
      v2.vertices.push_back(i3);
      vertices.push_back(v2);
    }
  }
}

inline void
createParamGridReference(pcl::PointCloud<pcl::PointXYZ>& cloud, const GridSpec& spec)
{
  const std::size_t cols = static_cast<std::size_t>(spec.seg_x) + 1;
  const std::size_t rows = static_cast<std::size_t>(spec.seg_y) + 1;
  const float dx = spec.width / static_cast<float>(spec.seg_x);
  const float dy = spec.height / static_cast<float>(spec.seg_y);

  cloud.clear();
  cloud.resize(cols * rows);
  cloud.width = static_cast<std::uint32_t>(cloud.size());
  cloud.height = 1;
  cloud.is_dense = true;

  for (std::size_t j = 0; j < rows; ++j) {
    for (std::size_t i = 0; i < cols; ++i) {
      auto& point = cloud[j * cols + i];
      point.x = spec.x0 + static_cast<float>(i) * dx;
      point.y = spec.y0 + static_cast<float>(j) * dy;
      point.z = spec.z0;
    }
  }
}

inline void
createParamGridCandidate(pcl::PointCloud<pcl::PointXYZ>& cloud, const GridSpec& spec)
{
#if defined(__RVV10__)
  const std::size_t cols = static_cast<std::size_t>(spec.seg_x) + 1;
  const std::size_t rows = static_cast<std::size_t>(spec.seg_y) + 1;
  const float dx = spec.width / static_cast<float>(spec.seg_x);
  const float dy = spec.height / static_cast<float>(spec.seg_y);

  cloud.clear();
  cloud.resize(cols * rows);
  cloud.width = static_cast<std::uint32_t>(cloud.size());
  cloud.height = 1;
  cloud.is_dense = true;

  for (std::size_t j = 0; j < rows; ++j) {
    std::size_t i = 0;
    auto* row_base =
        reinterpret_cast<std::uint8_t*>(cloud.data() + j * cols);
    const float y = spec.y0 + static_cast<float>(j) * dy;

    while (i < cols) {
      const std::size_t vl = __riscv_vsetvl_e32m2(cols - i);
      const vuint32m2_t lane =
          __riscv_vadd_vx_u32m2(__riscv_vid_v_u32m2(vl), static_cast<std::uint32_t>(i), vl);
      const vfloat32m2_t fi = __riscv_vfcvt_f_xu_v_f32m2(lane, vl);
      const vfloat32m2_t x =
          __riscv_vfmacc_vf_f32m2(__riscv_vfmv_v_f_f32m2(spec.x0, vl), dx, fi, vl);
      const vfloat32m2_t yv = __riscv_vfmv_v_f_f32m2(y, vl);
      const vfloat32m2_t zv = __riscv_vfmv_v_f_f32m2(spec.z0, vl);
      pcl::rvv_store::strided_store3_fields_f32m2<sizeof(pcl::PointXYZ),
                                                  offsetof(pcl::PointXYZ, x),
                                                  offsetof(pcl::PointXYZ, y),
                                                  offsetof(pcl::PointXYZ, z)>(row_base + i * sizeof(pcl::PointXYZ),
                                                                              vl,
                                                                              x,
                                                                              yv,
                                                                              zv);
      i += vl;
    }
  }
#else
  createParamGridReference(cloud, spec);
#endif
}

inline void
evaluateLikeSurfaceInPlace(pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  for (auto& point : cloud) {
    const float u = point.x;
    const float v = point.y;
    point.x = -0.8f + 2.2f * u + 0.08f * u * v;
    point.y = -0.6f + 1.5f * v - 0.05f * u * u;
    point.z = 0.15f + 0.20f * u + 0.40f * v + 0.35f * u * v + 0.10f * v * v;
  }
}

inline MeshStats
runParamGridReference(const unsigned resolution)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  createParamGridReference(cloud, makeGridSpec(resolution));
  return statsFromCloudAndPolygons(cloud, {});
}

inline MeshStats
runParamGridCandidate(const unsigned resolution)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  createParamGridCandidate(cloud, makeGridSpec(resolution));
  return statsFromCloudAndPolygons(cloud, {});
}

inline MeshStats
runSurfaceReference(const unsigned resolution)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  std::vector<pcl::Vertices> polygons;
  const auto spec = makeGridSpec(resolution);
  createParamGridReference(cloud, spec);
  createIndicesReference(polygons, 0, spec.seg_x, spec.seg_y);
  evaluateLikeSurfaceInPlace(cloud);
  return statsFromCloudAndPolygons(cloud, polygons);
}

inline MeshStats
runSurfaceCandidate(const unsigned resolution)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  std::vector<pcl::Vertices> polygons;
  const auto spec = makeGridSpec(resolution);
  createParamGridCandidate(cloud, spec);
  createIndicesReference(polygons, 0, spec.seg_x, spec.seg_y);
  evaluateLikeSurfaceInPlace(cloud);
  return statsFromCloudAndPolygons(cloud, polygons);
}

} // namespace pcl::surface::rvv_triangulation_support
