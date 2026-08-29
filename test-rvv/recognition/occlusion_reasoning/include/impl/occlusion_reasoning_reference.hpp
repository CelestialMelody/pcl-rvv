/*
 * 本文件负责标量参考链路（reference path）。它复刻
 * pcl::occlusion_reasoning::filter() 的保留索引语义，但直接输出 indices，
 * 用来避开 copyPointCloud 的额外成本和输出对象构造噪声。
 */

#pragma once

#include <pcl/point_cloud.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pcl::recognition::occlusion_reasoning_test {

struct FilterParams {
  float focal;
  float threshold;
  bool check_invalid_depth{true};
};

struct CandidateStats {
  std::size_t input_points{0};
  std::size_t vector_chunks{0};
  std::size_t lanes_tested{0};
  std::size_t projected_candidates{0};
  std::size_t kept_points{0};
};

template <typename SceneT>
inline bool
sceneDepthIsValid(const SceneT& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

template <typename SceneT, typename ModelT>
inline void
filterIndicesStd(const pcl::PointCloud<SceneT>& organized_cloud,
                 const pcl::PointCloud<ModelT>& to_be_filtered,
                 const FilterParams& params,
                 std::vector<int>& indices_to_keep)
{
  const float cx = static_cast<float>(organized_cloud.width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(organized_cloud.height) / 2.0f - 0.5f;

  indices_to_keep.resize(to_be_filtered.size());
  std::size_t keep = 0;
  for (std::size_t i = 0; i < to_be_filtered.size(); ++i) {
    const float x = to_be_filtered[i].x;
    const float y = to_be_filtered[i].y;
    const float z = to_be_filtered[i].z;
    const int u = static_cast<int>(params.focal * x / z + cx);
    const int v = static_cast<int>(params.focal * y / z + cy);

    if (u >= static_cast<int>(organized_cloud.width) ||
        v >= static_cast<int>(organized_cloud.height) || u < 0 || v < 0)
      continue;

    const auto& scene_point = organized_cloud.at(u, v);
    if (params.check_invalid_depth && !sceneDepthIsValid(scene_point))
      continue;

    if ((z - scene_point.z) > params.threshold)
      continue;

    indices_to_keep[keep++] = static_cast<int>(i);
  }
  indices_to_keep.resize(keep);
}

template <typename SceneT, typename ModelT>
inline std::uint64_t
checksumIndicesForFilter(const pcl::PointCloud<SceneT>& organized_cloud,
                         const pcl::PointCloud<ModelT>& to_be_filtered,
                         const FilterParams& params)
{
  std::vector<int> indices;
  filterIndicesStd(organized_cloud, to_be_filtered, params, indices);
  std::uint64_t checksum = 1469598103934665603ull;
  for (const int index : indices)
    checksum = (checksum ^ static_cast<std::uint32_t>(index)) * 1099511628211ull;
  checksum = (checksum ^ static_cast<std::uint32_t>(indices.size())) * 1099511628211ull;
  return checksum;
}

} // namespace pcl::recognition::occlusion_reasoning_test
