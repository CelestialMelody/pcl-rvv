/*
 * 本文件做什么：
 * 这里复刻 pcl::MarchingCubes<PointNT>::createSurface 的 active cell 输出逻辑，
 * 并提供一个 RVV candidate（RVV 候选）。Std 和 RVV 两侧共享同一份 synthetic
 * grid（合成网格）与 checksum（校验和）策略，便于 correctness、bench 和
 * Evidence Doctor 复核同一 A/B 边界。
 *
 * 阅读提示：
 * - emitSurfaceStd 是标量参考链路，使用和 production 相同的 cube vertex / edge table /
 *   tri table 语义，但避免每个 cell 分配临时 vector。
 * - emitSurfaceCandidate 在 __RVV10__ 构建中批量计算 12 条 edge 的插值坐标；三角表
 *   遍历和 PointNormal 写回仍是标量。
 * - reconstructGrid 只扫描已经生成好的 grid，不包含 Hoppe kd-tree 或 RBF solver 成本。
 */

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/surface/marching_cubes.h>

#include <Eigen/Core>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::surface::rvv_marching_cubes_support {

struct GridSpec {
  int res_x{32};
  int res_y{32};
  int res_z{32};
  float iso_level{0.0f};
  Eigen::Array3f lower{-1.0f, -1.0f, -1.0f};
  Eigen::Array3f size_voxel{2.0f / 32.0f, 2.0f / 32.0f, 2.0f / 32.0f};
};

struct SurfaceStats {
  std::size_t points{0};
  std::size_t triangles{0};
  std::size_t visited_cells{0};
  std::size_t active_cells{0};
  std::uint64_t checksum{1469598103934665603ull};
};

enum class GridKind { sphere, wave, sparse_sphere };

inline const char*
gridKindName(const GridKind kind)
{
  switch (kind) {
    case GridKind::sphere:
      return "sphere";
    case GridKind::wave:
      return "wave";
    case GridKind::sparse_sphere:
      return "sparse_sphere";
  }
  return "unknown";
}

inline std::uint64_t
mixChecksum(std::uint64_t value, const std::uint64_t next)
{
  value ^= next + 0x9e3779b97f4a7c15ull + (value << 6) + (value >> 2);
  return value;
}

inline std::uint64_t
floatBits(const float value)
{
  const auto quantized = static_cast<std::int64_t>(std::llround(value * 1000000.0f));
  return static_cast<std::uint64_t>(quantized);
}

inline int
gridIndex(const GridSpec& spec, const int x, const int y, const int z)
{
  return x * spec.res_y * spec.res_z + y * spec.res_z + z;
}

inline GridSpec
makeGridSpec(const int resolution)
{
  GridSpec spec;
  spec.res_x = resolution;
  spec.res_y = resolution;
  spec.res_z = resolution;
  spec.iso_level = 0.0f;
  spec.lower = Eigen::Array3f(-1.05f, -1.05f, -1.05f);
  spec.size_voxel =
      Eigen::Array3f(2.10f / static_cast<float>(resolution),
                     2.10f / static_cast<float>(resolution),
                     2.10f / static_cast<float>(resolution));
  return spec;
}

inline float
signedDistance(const Eigen::Array3f& point, const GridKind kind)
{
  const float x = point[0];
  const float y = point[1];
  const float z = point[2];
  if (kind == GridKind::wave) {
    const float wave = 0.18f * std::sin(8.0f * x) * std::cos(6.0f * y);
    return z + wave;
  }
  const float radius =
      kind == GridKind::sparse_sphere ? 0.58f : 0.72f;
  return std::sqrt(x * x + y * y + z * z) - radius;
}

inline std::vector<float>
makeGrid(const GridSpec& spec, const GridKind kind)
{
  std::vector<float> grid(static_cast<std::size_t>(spec.res_x) * spec.res_y *
                          spec.res_z);
  for (int x = 0; x < spec.res_x; ++x) {
    for (int y = 0; y < spec.res_y; ++y) {
      for (int z = 0; z < spec.res_z; ++z) {
        const Eigen::Array3f point =
            spec.lower +
            spec.size_voxel * Eigen::Array3f(static_cast<float>(x),
                                             static_cast<float>(y),
                                             static_cast<float>(z));
        float value = signedDistance(point, kind);
        if (kind == GridKind::sparse_sphere && ((x * 17 + y * 7 + z * 3) % 97 == 0))
          value = std::numeric_limits<float>::quiet_NaN();
        grid[static_cast<std::size_t>(gridIndex(spec, x, y, z))] = value;
      }
    }
  }
  return grid;
}

inline bool
loadLeaf(const GridSpec& spec,
         const std::vector<float>& grid,
         const int x,
         const int y,
         const int z,
         std::array<float, 8>& leaf)
{
  leaf[0] = grid[static_cast<std::size_t>(gridIndex(spec, x, y, z))];
  leaf[1] = grid[static_cast<std::size_t>(gridIndex(spec, x + 1, y, z))];
  leaf[2] = grid[static_cast<std::size_t>(gridIndex(spec, x + 1, y, z + 1))];
  leaf[3] = grid[static_cast<std::size_t>(gridIndex(spec, x, y, z + 1))];
  leaf[4] = grid[static_cast<std::size_t>(gridIndex(spec, x, y + 1, z))];
  leaf[5] = grid[static_cast<std::size_t>(gridIndex(spec, x + 1, y + 1, z))];
  leaf[6] = grid[static_cast<std::size_t>(gridIndex(spec, x + 1, y + 1, z + 1))];
  leaf[7] = grid[static_cast<std::size_t>(gridIndex(spec, x, y + 1, z + 1))];

  for (const float value : leaf) {
    if (std::isnan(value))
      return false;
  }
  return true;
}

inline int
cubeIndex(const std::array<float, 8>& leaf, const float iso_level)
{
  int cubeindex = 0;
  if (leaf[0] < iso_level) cubeindex |= 1;
  if (leaf[1] < iso_level) cubeindex |= 2;
  if (leaf[2] < iso_level) cubeindex |= 4;
  if (leaf[3] < iso_level) cubeindex |= 8;
  if (leaf[4] < iso_level) cubeindex |= 16;
  if (leaf[5] < iso_level) cubeindex |= 32;
  if (leaf[6] < iso_level) cubeindex |= 64;
  if (leaf[7] < iso_level) cubeindex |= 128;
  return cubeindex;
}

inline std::array<Eigen::Vector3f, 8>
cellCorners(const GridSpec& spec, const int x, const int y, const int z)
{
  const Eigen::Array3f center =
      spec.lower +
      spec.size_voxel * Eigen::Array3f(static_cast<float>(x),
                                       static_cast<float>(y),
                                       static_cast<float>(z));
  std::array<Eigen::Vector3f, 8> points;
  for (int i = 0; i < 8; ++i) {
    Eigen::Vector3f point = center.matrix();
    if (i & 0x4)
      point[1] = center[1] + spec.size_voxel[1];
    if (i & 0x2)
      point[2] = center[2] + spec.size_voxel[2];
    if ((i & 0x1) ^ ((i >> 1) & 0x1))
      point[0] = center[0] + spec.size_voxel[0];
    points[static_cast<std::size_t>(i)] = point;
  }
  return points;
}

inline Eigen::Vector3f
interpolateEdgeScalar(const Eigen::Vector3f& p1,
                      const Eigen::Vector3f& p2,
                      const float val_p1,
                      const float val_p2,
                      const float iso_level)
{
  const float mu = (iso_level - val_p1) / (val_p2 - val_p1);
  return p1 + mu * (p2 - p1);
}

inline void
appendPoint(const Eigen::Vector3f& point, pcl::PointCloud<pcl::PointNormal>& cloud)
{
  pcl::PointNormal out;
  out.x = point[0];
  out.y = point[1];
  out.z = point[2];
  out.normal_x = 0.0f;
  out.normal_y = 0.0f;
  out.normal_z = 1.0f;
  cloud.push_back(out);
}

inline void
updatePointChecksum(std::uint64_t& checksum, const pcl::PointNormal& point)
{
  checksum = mixChecksum(checksum, floatBits(point.x));
  checksum = mixChecksum(checksum, floatBits(point.y));
  checksum = mixChecksum(checksum, floatBits(point.z));
}

inline void
appendTriangleVertices(const int cubeindex,
                       const std::array<Eigen::Vector3f, 12>& vertex_list,
                       pcl::PointCloud<pcl::PointNormal>& cloud,
                       SurfaceStats& stats)
{
  for (int i = 0; pcl::triTable[cubeindex][i] != -1; i += 3) {
    appendPoint(vertex_list[static_cast<std::size_t>(pcl::triTable[cubeindex][i])],
                cloud);
    appendPoint(vertex_list[static_cast<std::size_t>(pcl::triTable[cubeindex][i + 1])],
                cloud);
    appendPoint(vertex_list[static_cast<std::size_t>(pcl::triTable[cubeindex][i + 2])],
                cloud);
    ++stats.triangles;
  }
}

inline bool
emitSurfaceStd(const GridSpec& spec,
               const std::array<float, 8>& leaf,
               const int x,
               const int y,
               const int z,
               pcl::PointCloud<pcl::PointNormal>& cloud,
               SurfaceStats& stats)
{
  const int index = cubeIndex(leaf, spec.iso_level);
  if (pcl::edgeTable[index] == 0)
    return false;

  const auto p = cellCorners(spec, x, y, z);
  std::array<Eigen::Vector3f, 12> vertex_list;
  if (pcl::edgeTable[index] & 1)
    vertex_list[0] = interpolateEdgeScalar(p[0], p[1], leaf[0], leaf[1], spec.iso_level);
  if (pcl::edgeTable[index] & 2)
    vertex_list[1] = interpolateEdgeScalar(p[1], p[2], leaf[1], leaf[2], spec.iso_level);
  if (pcl::edgeTable[index] & 4)
    vertex_list[2] = interpolateEdgeScalar(p[2], p[3], leaf[2], leaf[3], spec.iso_level);
  if (pcl::edgeTable[index] & 8)
    vertex_list[3] = interpolateEdgeScalar(p[3], p[0], leaf[3], leaf[0], spec.iso_level);
  if (pcl::edgeTable[index] & 16)
    vertex_list[4] = interpolateEdgeScalar(p[4], p[5], leaf[4], leaf[5], spec.iso_level);
  if (pcl::edgeTable[index] & 32)
    vertex_list[5] = interpolateEdgeScalar(p[5], p[6], leaf[5], leaf[6], spec.iso_level);
  if (pcl::edgeTable[index] & 64)
    vertex_list[6] = interpolateEdgeScalar(p[6], p[7], leaf[6], leaf[7], spec.iso_level);
  if (pcl::edgeTable[index] & 128)
    vertex_list[7] = interpolateEdgeScalar(p[7], p[4], leaf[7], leaf[4], spec.iso_level);
  if (pcl::edgeTable[index] & 256)
    vertex_list[8] = interpolateEdgeScalar(p[0], p[4], leaf[0], leaf[4], spec.iso_level);
  if (pcl::edgeTable[index] & 512)
    vertex_list[9] = interpolateEdgeScalar(p[1], p[5], leaf[1], leaf[5], spec.iso_level);
  if (pcl::edgeTable[index] & 1024)
    vertex_list[10] = interpolateEdgeScalar(p[2], p[6], leaf[2], leaf[6], spec.iso_level);
  if (pcl::edgeTable[index] & 2048)
    vertex_list[11] = interpolateEdgeScalar(p[3], p[7], leaf[3], leaf[7], spec.iso_level);

  appendTriangleVertices(index, vertex_list, cloud, stats);
  ++stats.active_cells;
  return true;
}

#if defined(__RVV10__)
inline void
interpolateEdgesRVV(const GridSpec& spec,
                    const std::array<float, 8>& leaf,
                    const std::array<Eigen::Vector3f, 8>& p,
                    std::array<Eigen::Vector3f, 12>& vertex_list)
{
  constexpr std::array<int, 12> kA{{0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3}};
  constexpr std::array<int, 12> kB{{1, 2, 3, 0, 5, 6, 7, 4, 4, 5, 6, 7}};
  alignas(64) float ax[12], ay[12], az[12], dx[12], dy[12], dz[12], va[12], vb[12];
  alignas(64) float ox[12], oy[12], oz[12];

  for (std::size_t edge = 0; edge < kA.size(); ++edge) {
    const int a = kA[edge];
    const int b = kB[edge];
    ax[edge] = p[static_cast<std::size_t>(a)][0];
    ay[edge] = p[static_cast<std::size_t>(a)][1];
    az[edge] = p[static_cast<std::size_t>(a)][2];
    dx[edge] = p[static_cast<std::size_t>(b)][0] - ax[edge];
    dy[edge] = p[static_cast<std::size_t>(b)][1] - ay[edge];
    dz[edge] = p[static_cast<std::size_t>(b)][2] - az[edge];
    va[edge] = leaf[static_cast<std::size_t>(a)];
    vb[edge] = leaf[static_cast<std::size_t>(b)];
  }

  std::size_t offset = 0;
  while (offset < kA.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m1(kA.size() - offset);
    const vfloat32m1_t v_iso = __riscv_vfmv_v_f_f32m1(spec.iso_level, vl);
    const vfloat32m1_t v_a = __riscv_vle32_v_f32m1(va + offset, vl);
    const vfloat32m1_t v_b = __riscv_vle32_v_f32m1(vb + offset, vl);
    const vfloat32m1_t mu =
        __riscv_vfdiv_vv_f32m1(__riscv_vfsub_vv_f32m1(v_iso, v_a, vl),
                               __riscv_vfsub_vv_f32m1(v_b, v_a, vl),
                               vl);

    const vfloat32m1_t x =
        __riscv_vfmacc_vv_f32m1(__riscv_vle32_v_f32m1(ax + offset, vl),
                                mu,
                                __riscv_vle32_v_f32m1(dx + offset, vl),
                                vl);
    const vfloat32m1_t y =
        __riscv_vfmacc_vv_f32m1(__riscv_vle32_v_f32m1(ay + offset, vl),
                                mu,
                                __riscv_vle32_v_f32m1(dy + offset, vl),
                                vl);
    const vfloat32m1_t z =
        __riscv_vfmacc_vv_f32m1(__riscv_vle32_v_f32m1(az + offset, vl),
                                mu,
                                __riscv_vle32_v_f32m1(dz + offset, vl),
                                vl);
    __riscv_vse32_v_f32m1(ox + offset, x, vl);
    __riscv_vse32_v_f32m1(oy + offset, y, vl);
    __riscv_vse32_v_f32m1(oz + offset, z, vl);
    offset += vl;
  }

  for (std::size_t edge = 0; edge < kA.size(); ++edge)
    vertex_list[edge] = Eigen::Vector3f(ox[edge], oy[edge], oz[edge]);
}
#endif

inline bool
emitSurfaceCandidate(const GridSpec& spec,
                     const std::array<float, 8>& leaf,
                     const int x,
                     const int y,
                     const int z,
                     pcl::PointCloud<pcl::PointNormal>& cloud,
                     SurfaceStats& stats)
{
#if !defined(__RVV10__)
  return emitSurfaceStd(spec, leaf, x, y, z, cloud, stats);
#else
  const int index = cubeIndex(leaf, spec.iso_level);
  if (pcl::edgeTable[index] == 0)
    return false;

  const auto p = cellCorners(spec, x, y, z);
  std::array<Eigen::Vector3f, 12> vertex_list;
  interpolateEdgesRVV(spec, leaf, p, vertex_list);
  appendTriangleVertices(index, vertex_list, cloud, stats);
  ++stats.active_cells;
  return true;
#endif
}

using EmitFunction = bool (*)(const GridSpec&,
                              const std::array<float, 8>&,
                              int,
                              int,
                              int,
                              pcl::PointCloud<pcl::PointNormal>&,
                              SurfaceStats&);

inline SurfaceStats
reconstructGrid(const GridSpec& spec,
                const std::vector<float>& grid,
                const EmitFunction emit)
{
  SurfaceStats stats;
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.reserve(static_cast<std::size_t>(spec.res_x * spec.res_y + spec.res_x * spec.res_z +
                                         spec.res_y * spec.res_z) *
                12);

  for (int x = 1; x < spec.res_x - 1; ++x) {
    for (int y = 1; y < spec.res_y - 1; ++y) {
      for (int z = 1; z < spec.res_z - 1; ++z) {
        ++stats.visited_cells;
        std::array<float, 8> leaf;
        if (loadLeaf(spec, grid, x, y, z, leaf))
          emit(spec, leaf, x, y, z, cloud, stats);
      }
    }
  }

  stats.points = cloud.size();
  for (const auto& point : cloud)
    updatePointChecksum(stats.checksum, point);
  stats.checksum = mixChecksum(stats.checksum, stats.points);
  stats.checksum = mixChecksum(stats.checksum, stats.triangles);
  stats.checksum = mixChecksum(stats.checksum, stats.active_cells);
  return stats;
}

inline SurfaceStats
runReference(const GridSpec& spec, const std::vector<float>& grid)
{
  return reconstructGrid(spec, grid, emitSurfaceStd);
}

inline SurfaceStats
runCandidate(const GridSpec& spec, const std::vector<float>& grid)
{
  return reconstructGrid(spec, grid, emitSurfaceCandidate);
}

#if defined(__RVV10__)
inline void
scanActiveCellsZRVV(const GridSpec& spec,
                    const std::vector<float>& grid,
                    const int x,
                    const int y,
                    std::vector<int>& active_z)
{
  alignas(64) std::uint32_t cube_indices[256];
  alignas(64) std::uint32_t finite_flags[256];

  const float* g000 = grid.data() + gridIndex(spec, x, y, 1);
  const float* g100 = grid.data() + gridIndex(spec, x + 1, y, 1);
  const float* g010 = grid.data() + gridIndex(spec, x, y + 1, 1);
  const float* g110 = grid.data() + gridIndex(spec, x + 1, y + 1, 1);
  const int z_count = spec.res_z - 2;
  int z_offset = 0;
  while (z_offset < z_count) {
    const std::size_t chunk =
        std::min<std::size_t>(static_cast<std::size_t>(z_count - z_offset),
                              std::size(cube_indices));
    std::size_t consumed = 0;
    while (consumed < chunk) {
      const std::size_t vl = __riscv_vsetvl_e32m1(chunk - consumed);
      const float* p000 = g000 + z_offset + consumed;
      const float* p100 = g100 + z_offset + consumed;
      const float* p010 = g010 + z_offset + consumed;
      const float* p110 = g110 + z_offset + consumed;

      const vfloat32m1_t v0 = __riscv_vle32_v_f32m1(p000, vl);
      const vfloat32m1_t v1 = __riscv_vle32_v_f32m1(p100, vl);
      const vfloat32m1_t v2 = __riscv_vle32_v_f32m1(p100 + 1, vl);
      const vfloat32m1_t v3 = __riscv_vle32_v_f32m1(p000 + 1, vl);
      const vfloat32m1_t v4 = __riscv_vle32_v_f32m1(p010, vl);
      const vfloat32m1_t v5 = __riscv_vle32_v_f32m1(p110, vl);
      const vfloat32m1_t v6 = __riscv_vle32_v_f32m1(p110 + 1, vl);
      const vfloat32m1_t v7 = __riscv_vle32_v_f32m1(p010 + 1, vl);

      vuint32m1_t cube = __riscv_vmv_v_x_u32m1(0, vl);
      auto add_bit = [vl](vuint32m1_t current, const vfloat32m1_t values, const std::uint32_t bit) {
        const vbool32_t below = __riscv_vmflt_vf_f32m1_b32(values, 0.0f, vl);
        const vuint32m1_t with_bit = __riscv_vor_vx_u32m1(current, bit, vl);
        return __riscv_vmerge_vvm_u32m1(current, with_bit, below, vl);
      };
      cube = add_bit(cube, v0, 1);
      cube = add_bit(cube, v1, 2);
      cube = add_bit(cube, v2, 4);
      cube = add_bit(cube, v3, 8);
      cube = add_bit(cube, v4, 16);
      cube = add_bit(cube, v5, 32);
      cube = add_bit(cube, v6, 64);
      cube = add_bit(cube, v7, 128);

      vbool32_t finite = __riscv_vmfeq_vv_f32m1_b32(v0, v0, vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v1, v1, vl), vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v2, v2, vl), vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v3, v3, vl), vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v4, v4, vl), vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v5, v5, vl), vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v6, v6, vl), vl);
      finite = __riscv_vmand_mm_b32(finite, __riscv_vmfeq_vv_f32m1_b32(v7, v7, vl), vl);

      const vuint32m1_t one = __riscv_vmv_v_x_u32m1(1, vl);
      const vuint32m1_t zero = __riscv_vmv_v_x_u32m1(0, vl);
      const vuint32m1_t valid = __riscv_vmerge_vvm_u32m1(zero, one, finite, vl);
      __riscv_vse32_v_u32m1(cube_indices + consumed, cube, vl);
      __riscv_vse32_v_u32m1(finite_flags + consumed, valid, vl);
      consumed += vl;
    }

    for (std::size_t lane = 0; lane < chunk; ++lane) {
      if (finite_flags[lane] != 0 && pcl::edgeTable[cube_indices[lane]] != 0)
        active_z.push_back(1 + z_offset + static_cast<int>(lane));
    }
    z_offset += static_cast<int>(chunk);
  }
}
#endif

inline SurfaceStats
runPrepassCandidate(const GridSpec& spec, const std::vector<float>& grid)
{
#if !defined(__RVV10__)
  return runReference(spec, grid);
#else
  SurfaceStats stats;
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.reserve(static_cast<std::size_t>(spec.res_x * spec.res_y + spec.res_x * spec.res_z +
                                         spec.res_y * spec.res_z) *
                12);
  std::vector<int> active_z;
  active_z.reserve(static_cast<std::size_t>(spec.res_z));

  for (int x = 1; x < spec.res_x - 1; ++x) {
    for (int y = 1; y < spec.res_y - 1; ++y) {
      active_z.clear();
      scanActiveCellsZRVV(spec, grid, x, y, active_z);
      stats.visited_cells += static_cast<std::size_t>(spec.res_z - 2);
      for (const int z : active_z) {
        std::array<float, 8> leaf;
        if (loadLeaf(spec, grid, x, y, z, leaf))
          emitSurfaceStd(spec, leaf, x, y, z, cloud, stats);
      }
    }
  }

  stats.points = cloud.size();
  for (const auto& point : cloud)
    updatePointChecksum(stats.checksum, point);
  stats.checksum = mixChecksum(stats.checksum, stats.points);
  stats.checksum = mixChecksum(stats.checksum, stats.triangles);
  stats.checksum = mixChecksum(stats.checksum, stats.active_cells);
  return stats;
#endif
}

inline bool
sameStats(const SurfaceStats& a, const SurfaceStats& b)
{
  return a.points == b.points && a.triangles == b.triangles &&
         a.visited_cells == b.visited_cells && a.active_cells == b.active_cells &&
         a.checksum == b.checksum;
}

} // namespace pcl::surface::rvv_marching_cubes_support
