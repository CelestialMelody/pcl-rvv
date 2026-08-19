/*
 * 本文件复刻 organized_fast_mesh 在 storeShadowedFaces(true) 下的标量 polygon
 * 生成语义：按 organized 行列顺序检查有限值，再按原顺序追加三角形或四边形。
 * 这里不覆盖 shadow edge（阴影边）判断；那条路径需要单独的 production-shaped
 * diagnostic（生产形态诊断）。
 */

#pragma once

#include "ofm_types.hpp"

#include <pcl/common/point_tests.h>

#include <cmath>

namespace pcl::surface::rvv_ofm_support
{

inline void
addTriangle(const int a, const int b, const int c, std::vector<pcl::Vertices>& polygons)
{
  pcl::Vertices polygon;
  polygon.vertices.resize(3);
  polygon.vertices[0] = static_cast<std::uint32_t>(a);
  polygon.vertices[1] = static_cast<std::uint32_t>(b);
  polygon.vertices[2] = static_cast<std::uint32_t>(c);
  polygons.push_back(polygon);
}

inline void
addQuad(const int a,
        const int b,
        const int c,
        const int d,
        std::vector<pcl::Vertices>& polygons)
{
  pcl::Vertices polygon;
  polygon.vertices.resize(4);
  polygon.vertices[0] = static_cast<std::uint32_t>(a);
  polygon.vertices[1] = static_cast<std::uint32_t>(b);
  polygon.vertices[2] = static_cast<std::uint32_t>(c);
  polygon.vertices[3] = static_cast<std::uint32_t>(d);
  polygons.push_back(polygon);
}

template <typename PointT>
inline bool
validPoint(const PointT& point)
{
  return pcl::isFinite(point);
}

template <typename PointT>
inline bool
validTriangle(const pcl::PointCloud<PointT>& cloud,
              const int a,
              const int b,
              const int c)
{
  return validPoint(cloud[static_cast<std::size_t>(a)]) &&
         validPoint(cloud[static_cast<std::size_t>(b)]) &&
         validPoint(cloud[static_cast<std::size_t>(c)]);
}

template <typename PointT>
inline bool
validQuad(const pcl::PointCloud<PointT>& cloud,
          const int a,
          const int b,
          const int c,
          const int d)
{
  return validPoint(cloud[static_cast<std::size_t>(a)]) &&
         validPoint(cloud[static_cast<std::size_t>(b)]) &&
         validPoint(cloud[static_cast<std::size_t>(c)]) &&
         validPoint(cloud[static_cast<std::size_t>(d)]);
}

template <typename PointT>
inline MeshRunResult
generateMeshReference(const pcl::PointCloud<PointT>& cloud,
                      const MeshOptions& options)
{
  MeshRunResult result;
  const int last_column = options.width - options.column_step;
  const int last_row = options.height - options.row_step;
  const int y_big_incr = options.row_step * options.width;
  const int x_big_incr = y_big_incr + options.column_step;
  result.polygons.reserve(static_cast<std::size_t>(options.width) *
                          static_cast<std::size_t>(options.height) * 2u);

  for (int y = 0; y < last_row; y += options.row_step) {
    int i = y * options.width;
    int index_right = i + options.column_step;
    int index_down = i + y_big_incr;
    int index_down_right = i + x_big_incr;

    for (int x = 0; x < last_column;
         x += options.column_step,
             i += options.column_step,
             index_right += options.column_step,
             index_down += options.column_step,
             index_down_right += options.column_step) {
      if (options.kind == MeshKind::quad) {
        if (validQuad(cloud, i, index_right, index_down_right, index_down))
          addQuad(i, index_right, index_down_right, index_down, result.polygons);
      }
      else if (options.kind == MeshKind::right_cut) {
        if (validTriangle(cloud, i, index_down_right, index_right))
          addTriangle(i, index_down_right, index_right, result.polygons);
        if (validTriangle(cloud, i, index_down, index_down_right))
          addTriangle(i, index_down, index_down_right, result.polygons);
      }
      else if (options.kind == MeshKind::left_cut) {
        if (validTriangle(cloud, i, index_down, index_right))
          addTriangle(i, index_down, index_right, result.polygons);
        if (validTriangle(cloud, index_right, index_down, index_down_right))
          addTriangle(index_right, index_down, index_down_right, result.polygons);
      }
      else {
        const bool right_cut_upper = validTriangle(cloud, i, index_down_right, index_right);
        const bool right_cut_lower = validTriangle(cloud, i, index_down, index_down_right);
        const bool left_cut_upper = validTriangle(cloud, i, index_down, index_right);
        const bool left_cut_lower = validTriangle(cloud, index_right, index_down, index_down_right);

        if (right_cut_upper && right_cut_lower && left_cut_upper && left_cut_lower) {
          const float dist_right_cut =
              std::abs(cloud[static_cast<std::size_t>(index_down)].z -
                       cloud[static_cast<std::size_t>(index_right)].z);
          const float dist_left_cut =
              std::abs(cloud[static_cast<std::size_t>(i)].z -
                       cloud[static_cast<std::size_t>(index_down_right)].z);
          if (dist_right_cut >= dist_left_cut) {
            addTriangle(i, index_down_right, index_right, result.polygons);
            addTriangle(i, index_down, index_down_right, result.polygons);
          }
          else {
            addTriangle(i, index_down, index_right, result.polygons);
            addTriangle(index_right, index_down, index_down_right, result.polygons);
          }
        }
        else {
          if (right_cut_upper)
            addTriangle(i, index_down_right, index_right, result.polygons);
          if (right_cut_lower)
            addTriangle(i, index_down, index_down_right, result.polygons);
          if (left_cut_upper)
            addTriangle(i, index_down, index_right, result.polygons);
          if (left_cut_lower)
            addTriangle(index_right, index_down, index_down_right, result.polygons);
        }
      }
    }
  }
  result.checksum = polygonChecksum(result.polygons);
  return result;
}

}  // namespace pcl::surface::rvv_ofm_support
