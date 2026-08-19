/*
 * 本文件保存 organized_fast_mesh 的 test-only RVV candidate（测试专用 RVV 候选）。
 *
 * 当前候选族：
 * - 先缓存每个点的 finite（有限值）状态，避免每个三角形 / 四边形重复做 isFinite。
 * - 在 __RVV10__ 构建中用 RVV stride load（跨步加载）批量读取 PointXYZ 的 x/y/z 字段，
 *   再保序交给标量 polygon append tail（标量追加尾段）。
 * - adaptive cut 的 z 对角线差值也用 RVV 批量计算，但 polygon 输出仍保持标量顺序。
 *
 * 证据边界：
 * 这不是 production dispatch。它只覆盖 PointXYZ / float / triangle_pixel_size=1 且
 * storeShadowedFaces(true) 的 production-shaped diagnostic（生产形态诊断）。
 */

#pragma once

#include "ofm_reference.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <pcl/surface/organized_fast_mesh.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::surface::rvv_ofm_support
{

namespace detail
{

inline std::vector<std::uint8_t>
computeFiniteFlagsScalar(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::vector<std::uint8_t> finite(cloud.size(), 0);
  for (std::size_t i = 0; i < cloud.size(); ++i)
    finite[i] = validPoint(cloud[i]) ? 1u : 0u;
  return finite;
}

#if defined(__RVV10__)
inline std::vector<std::uint8_t>
computeFiniteFlagsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::vector<std::uint8_t> finite(cloud.size(), 0);
  if (cloud.empty())
    return finite;

  const auto stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ));
  const float max_finite = std::numeric_limits<float>::max();
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  std::vector<float> xs(vlmax), ys(vlmax), zs(vlmax);

  std::size_t offset = 0;
  while (offset < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m1(cloud.size() - offset);
    const auto* base = &cloud[offset];
    const vfloat32m1_t vx = __riscv_vlse32_v_f32m1(&base[0].x, stride, vl);
    const vfloat32m1_t vy = __riscv_vlse32_v_f32m1(&base[0].y, stride, vl);
    const vfloat32m1_t vz = __riscv_vlse32_v_f32m1(&base[0].z, stride, vl);
    __riscv_vse32_v_f32m1(xs.data(), vx, vl);
    __riscv_vse32_v_f32m1(ys.data(), vy, vl);
    __riscv_vse32_v_f32m1(zs.data(), vz, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      finite[offset + lane] =
          (std::isfinite(xs[lane]) && std::isfinite(ys[lane]) &&
           std::isfinite(zs[lane]) && std::abs(xs[lane]) <= max_finite &&
           std::abs(ys[lane]) <= max_finite && std::abs(zs[lane]) <= max_finite)
              ? 1u
              : 0u;
    }
    offset += vl;
  }
  return finite;
}
#endif

inline bool
cachedTriangleValid(const std::vector<std::uint8_t>& finite,
                    const int a,
                    const int b,
                    const int c)
{
  return finite[static_cast<std::size_t>(a)] &&
         finite[static_cast<std::size_t>(b)] &&
         finite[static_cast<std::size_t>(c)];
}

inline bool
cachedQuadValid(const std::vector<std::uint8_t>& finite,
                const int a,
                const int b,
                const int c,
                const int d)
{
  return finite[static_cast<std::size_t>(a)] &&
         finite[static_cast<std::size_t>(b)] &&
         finite[static_cast<std::size_t>(c)] &&
         finite[static_cast<std::size_t>(d)];
}

inline void
addTriangleAt(const int a,
              const int b,
              const int c,
              const int idx,
              std::vector<pcl::Vertices>& polygons)
{
  assert(idx < static_cast<int>(polygons.size()));
  polygons[static_cast<std::size_t>(idx)].vertices.resize(3);
  polygons[static_cast<std::size_t>(idx)].vertices[0] = static_cast<std::uint32_t>(a);
  polygons[static_cast<std::size_t>(idx)].vertices[1] = static_cast<std::uint32_t>(b);
  polygons[static_cast<std::size_t>(idx)].vertices[2] = static_cast<std::uint32_t>(c);
}

inline void
addQuadAt(const int a,
          const int b,
          const int c,
          const int d,
          const int idx,
          std::vector<pcl::Vertices>& polygons)
{
  assert(idx < static_cast<int>(polygons.size()));
  polygons[static_cast<std::size_t>(idx)].vertices.resize(4);
  polygons[static_cast<std::size_t>(idx)].vertices[0] = static_cast<std::uint32_t>(a);
  polygons[static_cast<std::size_t>(idx)].vertices[1] = static_cast<std::uint32_t>(b);
  polygons[static_cast<std::size_t>(idx)].vertices[2] = static_cast<std::uint32_t>(c);
  polygons[static_cast<std::size_t>(idx)].vertices[3] = static_cast<std::uint32_t>(d);
}

#if defined(__RVV10__)
inline void
computeAdaptiveDiagonalPreferenceRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                     const MeshOptions& options,
                                     const int y,
                                     std::vector<std::uint8_t>& prefer_right)
{
  const int cell_count =
      (options.width - options.column_step + options.column_step - 1) /
      options.column_step;
  prefer_right.assign(static_cast<std::size_t>(cell_count), 0);

  const int y_big_incr = options.row_step * options.width;
  const int x_big_incr = y_big_incr + options.column_step;
  const int row_base = y * options.width;
  const auto stride = static_cast<std::ptrdiff_t>(options.column_step * sizeof(pcl::PointXYZ));
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  std::vector<float> right(vlmax), left(vlmax);

  std::size_t cell = 0;
  while (cell < static_cast<std::size_t>(cell_count)) {
    const std::size_t vl = __riscv_vsetvl_e32m1(static_cast<std::size_t>(cell_count) - cell);
    const int base = row_base + static_cast<int>(cell) * options.column_step;
    const auto* p0 = &cloud[static_cast<std::size_t>(base)];
    const auto* pr = &cloud[static_cast<std::size_t>(base + options.column_step)];
    const auto* pd = &cloud[static_cast<std::size_t>(base + y_big_incr)];
    const auto* pdr = &cloud[static_cast<std::size_t>(base + x_big_incr)];

    const vfloat32m1_t zi = __riscv_vlse32_v_f32m1(&p0[0].z, stride, vl);
    const vfloat32m1_t zr = __riscv_vlse32_v_f32m1(&pr[0].z, stride, vl);
    const vfloat32m1_t zd = __riscv_vlse32_v_f32m1(&pd[0].z, stride, vl);
    const vfloat32m1_t zdr = __riscv_vlse32_v_f32m1(&pdr[0].z, stride, vl);
    const vfloat32m1_t dist_right = __riscv_vfabs_v_f32m1(__riscv_vfsub_vv_f32m1(zd, zr, vl), vl);
    const vfloat32m1_t dist_left = __riscv_vfabs_v_f32m1(__riscv_vfsub_vv_f32m1(zi, zdr, vl), vl);
    __riscv_vse32_v_f32m1(right.data(), dist_right, vl);
    __riscv_vse32_v_f32m1(left.data(), dist_left, vl);

    for (std::size_t lane = 0; lane < vl; ++lane)
      prefer_right[cell + lane] = (right[lane] >= left[lane]) ? 1u : 0u;
    cell += vl;
  }
}
#endif

}  // namespace detail

inline MeshRunResult
generateMeshCandidate(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      const MeshOptions& options)
{
#if !defined(__RVV10__)
  return generateMeshReference(cloud, options);
#else
  MeshRunResult result;
  const auto finite = detail::computeFiniteFlagsRVV(cloud);
  const int last_column = options.width - options.column_step;
  const int last_row = options.height - options.row_step;
  const int y_big_incr = options.row_step * options.width;
  const int x_big_incr = y_big_incr + options.column_step;
  int idx = 0;
  result.polygons.resize(static_cast<std::size_t>(options.width) *
                         static_cast<std::size_t>(options.height) * 2u);

  std::vector<std::uint8_t> prefer_right;
  for (int y = 0; y < last_row; y += options.row_step) {
    if (options.kind == MeshKind::adaptive_cut)
      detail::computeAdaptiveDiagonalPreferenceRVV(cloud, options, y, prefer_right);

    int i = y * options.width;
    int index_right = i + options.column_step;
    int index_down = i + y_big_incr;
    int index_down_right = i + x_big_incr;
    int cell = 0;

    for (int x = 0; x < last_column;
         x += options.column_step,
             i += options.column_step,
             index_right += options.column_step,
             index_down += options.column_step,
             index_down_right += options.column_step,
             ++cell) {
      if (options.kind == MeshKind::quad) {
        if (detail::cachedQuadValid(finite, i, index_right, index_down_right, index_down))
          detail::addQuadAt(i, index_right, index_down_right, index_down, idx++, result.polygons);
      }
      else if (options.kind == MeshKind::right_cut) {
        if (detail::cachedTriangleValid(finite, i, index_down_right, index_right))
          detail::addTriangleAt(i, index_down_right, index_right, idx++, result.polygons);
        if (detail::cachedTriangleValid(finite, i, index_down, index_down_right))
          detail::addTriangleAt(i, index_down, index_down_right, idx++, result.polygons);
      }
      else if (options.kind == MeshKind::left_cut) {
        if (detail::cachedTriangleValid(finite, i, index_down, index_right))
          detail::addTriangleAt(i, index_down, index_right, idx++, result.polygons);
        if (detail::cachedTriangleValid(finite, index_right, index_down, index_down_right))
          detail::addTriangleAt(index_right, index_down, index_down_right, idx++, result.polygons);
      }
      else {
        const bool right_cut_upper =
            detail::cachedTriangleValid(finite, i, index_down_right, index_right);
        const bool right_cut_lower =
            detail::cachedTriangleValid(finite, i, index_down, index_down_right);
        const bool left_cut_upper =
            detail::cachedTriangleValid(finite, i, index_down, index_right);
        const bool left_cut_lower =
            detail::cachedTriangleValid(finite, index_right, index_down, index_down_right);

        if (right_cut_upper && right_cut_lower && left_cut_upper && left_cut_lower) {
          if (prefer_right[static_cast<std::size_t>(cell)]) {
            detail::addTriangleAt(i, index_down_right, index_right, idx++, result.polygons);
            detail::addTriangleAt(i, index_down, index_down_right, idx++, result.polygons);
          }
          else {
            detail::addTriangleAt(i, index_down, index_right, idx++, result.polygons);
            detail::addTriangleAt(index_right, index_down, index_down_right, idx++, result.polygons);
          }
        }
        else {
          if (right_cut_upper)
            detail::addTriangleAt(i, index_down_right, index_right, idx++, result.polygons);
          if (right_cut_lower)
            detail::addTriangleAt(i, index_down, index_down_right, idx++, result.polygons);
          if (left_cut_upper)
            detail::addTriangleAt(i, index_down, index_right, idx++, result.polygons);
          if (left_cut_lower)
            detail::addTriangleAt(index_right, index_down, index_down_right, idx++, result.polygons);
        }
      }
    }
  }
  result.polygons.resize(static_cast<std::size_t>(idx));
  result.checksum = polygonChecksum(result.polygons);
  return result;
#endif
}

inline MeshRunResult
generateMeshCurrentBuild(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      const MeshOptions& options)
{
#if defined(__RVV10__)
  return generateMeshCandidate(cloud, options);
#else
  return generateMeshReference(cloud, options);
#endif
}

template <typename PointT>
inline MeshRunResult
generateMeshPublicPath(const pcl::PointCloud<PointT>& cloud,
                       const MeshOptions& options,
                       const bool store_shadowed_faces)
{
  using Ofm = pcl::OrganizedFastMesh<PointT>;
  MeshRunResult result;
  auto cloud_ptr = pcl::make_shared<pcl::PointCloud<PointT>>(cloud);
  Ofm ofm;
  pcl::PolygonMesh mesh;
  ofm.setInputCloud(cloud_ptr);
  ofm.setTrianglePixelSizeRows(options.row_step + 1);
  ofm.setTrianglePixelSizeColumns(options.column_step + 1);
  switch (options.kind) {
    case MeshKind::quad: ofm.setTriangulationType(Ofm::QUAD_MESH); break;
    case MeshKind::right_cut: ofm.setTriangulationType(Ofm::TRIANGLE_RIGHT_CUT); break;
    case MeshKind::left_cut: ofm.setTriangulationType(Ofm::TRIANGLE_LEFT_CUT); break;
    case MeshKind::adaptive_cut: ofm.setTriangulationType(Ofm::TRIANGLE_ADAPTIVE_CUT); break;
  }
  ofm.storeShadowedFaces(store_shadowed_faces);
  ofm.reconstruct(mesh);
  result.polygons = std::move(mesh.polygons);
  result.checksum = polygonChecksum(result.polygons);
  return result;
}

}  // namespace pcl::surface::rvv_ofm_support
