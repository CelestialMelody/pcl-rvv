/*
 * 本文件做什么：
 * 本文件负责 TE2D layout gate（布局验收）和顺序点云对 dense / finite 检查。
 *
 * 这些 helper 只说明测试候选是否能安全读取 x/y/z，不等于 production dispatch。
 */

#pragma once

#include "te2d_core_types.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointT>
inline std::size_t
countFinitePoints(const pcl::PointCloud<PointT>& cloud)
{
  std::size_t count = 0;
  for (const auto& point : cloud) {
    if (pcl::isFinite(point))
      ++count;
  }
  return count;
}

template <typename PointT>
inline bool
isValidCloudIndex(const pcl::PointCloud<PointT>& cloud, const pcl::index_t index)
{
  return index >= 0 && static_cast<std::size_t>(index) < cloud.size();
}

template <typename PointT>
inline std::size_t
countFinitePrefixRows(const pcl::PointCloud<PointT>& cloud, const std::size_t count)
{
  std::size_t finite_count = 0;
  const std::size_t n = std::min(count, cloud.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (pcl::isFinite(cloud[i]))
      ++finite_count;
  }
  return finite_count;
}

template <typename PointSource, typename PointTarget>
inline void
populateLayoutStats(CandidateStats* stats)
{
  if (!stats)
    return;

  stats->source_layout_supported =
      pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value;
  stats->target_layout_supported =
      pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value;
  stats->layout_supported =
      stats->source_layout_supported && stats->target_layout_supported;
}

template <typename PointSource, typename PointTarget>
inline bool
isDenseFiniteOrderedPair(const pcl::PointCloud<PointSource>& source,
                         const pcl::PointCloud<PointTarget>& target)
{
  if (!source.is_dense || !target.is_dense || source.size() != target.size() ||
      source.empty())
    return false;
  return countFinitePoints(source) == source.size() &&
         countFinitePoints(target) == target.size();
}

} // namespace pcl::registration::rvv_te2d_support
