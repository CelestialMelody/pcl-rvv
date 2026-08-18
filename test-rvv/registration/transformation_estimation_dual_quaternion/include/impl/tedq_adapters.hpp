/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_dual_quaternion 的测试夹具和 row-source
 * adapter（行来源适配器）。它负责构造确定性点云、非 identity 索引 / 对应关系、检查
 * 输入范围以及 materialize（暂存）成 ordered cloud（顺序点云对）。
 *
 * 证据边界：
 * 这些 helper 只为 test-rvv 的 correctness、bench 和诊断候选提供输入语义，不修改
 * production 源码，也不能证明真实 public entry（公开入口）已经命中 RVV。staging
 * 的展开成本是否计入 bench，由调用它的候选 wrapper 决定。
 */

#pragma once

#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace pcl::registration::rvv_tedq_support {

inline Eigen::Matrix4f
makeRigidTransform()
{
  const float angle = 0.41f;
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform(0, 0) = c;
  transform(0, 1) = -s;
  transform(1, 0) = s;
  transform(1, 1) = c;
  transform(0, 3) = 0.73f;
  transform(1, 3) = -0.37f;
  transform(2, 3) = 0.29f;
  return transform;
}

inline pcl::PointCloud<pcl::PointXYZ>
makePointXYZCloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.0125f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.0085f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.0105f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZI>
makePointXYZICloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.011f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 4093) - 2046) * 0.0095f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 17) % 4091) - 2045) * 0.0075f;
    cloud[i].intensity = static_cast<float>(i % 257) * 0.25f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZRGB>
makePointXYZRGBCloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZRGB> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.010f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 11) % 4093) - 2046) * 0.008f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 19) % 4091) - 2045) * 0.0065f;
    cloud[i].r = static_cast<std::uint8_t>((i * 3) % 251);
    cloud[i].g = static_cast<std::uint8_t>((i * 5) % 253);
    cloud[i].b = static_cast<std::uint8_t>((i * 7) % 255);
  }
  return cloud;
}

inline pcl::Indices
makeSourceIndices(const std::size_t source_size, const std::size_t n)
{
  pcl::Indices indices;
  indices.resize(n);
  if (source_size == 0)
    return indices;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t index = (i * 13 + 97) % source_size;
    indices[i] = static_cast<pcl::index_t>(index);
  }
  return indices;
}

inline pcl::Indices
makeTargetIndices(const std::size_t target_size, const std::size_t n)
{
  pcl::Indices indices;
  indices.resize(n);
  if (target_size == 0)
    return indices;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t index = (i * 17 + 53) % target_size;
    indices[i] = static_cast<pcl::index_t>(index);
  }
  return indices;
}

// 这三种 pattern（索引分布）只用于 correspondence-pair 的输入局部性消融：
// contiguous 表示连续访问，local_window 表示在小窗口内访问但窗口之间留有间隔，
// strided 表示跨步访问。
enum class CorrespondenceIndexPattern {
  contiguous,
  local_window,
  strided,
};

inline std::size_t
correspondencePatternIndex(const std::size_t point_count,
                           const std::size_t i,
                           const CorrespondenceIndexPattern pattern)
{
  if (point_count == 0)
    return 0;
  switch (pattern) {
    case CorrespondenceIndexPattern::contiguous:
      return i % point_count;
    case CorrespondenceIndexPattern::local_window:
      return ((i / 64) * 256 + (i % 64)) % point_count;
    case CorrespondenceIndexPattern::strided:
      return (i * 13 + 97) % point_count;
  }
  return 0;
}

inline pcl::Correspondences
makeCorrespondencesForPattern(const std::size_t source_size,
                              const std::size_t target_size,
                              const std::size_t n,
                              const CorrespondenceIndexPattern pattern)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t query =
        correspondencePatternIndex(source_size, i, pattern);
    const std::size_t match =
        correspondencePatternIndex(target_size, i, pattern);
    correspondences.emplace_back(static_cast<int>(query),
                                 static_cast<int>(match),
                                 0.0f);
  }
  return correspondences;
}

inline pcl::Correspondences
makeCorrespondences(const std::size_t source_size,
                    const std::size_t target_size,
                    const std::size_t n)
{
  return makeCorrespondencesForPattern(
      source_size, target_size, n, CorrespondenceIndexPattern::strided);
}

template <typename PointT>
inline pcl::PointCloud<PointT>
transformCloudXYZ(const pcl::PointCloud<PointT>& source, const Eigen::Matrix4f& transform)
{
  pcl::PointCloud<PointT> target = source;
  for (std::size_t i = 0; i < source.size(); ++i) {
    const Eigen::Vector4f p(source[i].x, source[i].y, source[i].z, 1.0f);
    const Eigen::Vector4f q = transform * p;
    target[i].x = q.x();
    target[i].y = q.y();
    target[i].z = q.z();
  }
  target.is_dense = source.is_dense;
  return target;
}

template <typename PointT>
inline bool
indicesInRange(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  return std::all_of(indices.begin(), indices.end(), [&](const pcl::index_t index) {
    return index >= 0 && static_cast<std::size_t>(index) < cloud.size();
  });
}

template <typename PointT>
inline bool
correspondencesInRange(const pcl::PointCloud<PointT>& source,
                       const pcl::PointCloud<PointT>& target,
                       const pcl::Correspondences& correspondences)
{
  return std::all_of(correspondences.begin(), correspondences.end(), [&](const auto& corr) {
    return corr.index_query >= 0 &&
           static_cast<std::size_t>(corr.index_query) < source.size() &&
           corr.index_match >= 0 &&
           static_cast<std::size_t>(corr.index_match) < target.size();
  });
}

template <typename PointT>
inline pcl::PointCloud<PointT>
materializeByIndices(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  pcl::PointCloud<PointT> staged;
  staged.width = static_cast<std::uint32_t>(indices.size());
  staged.height = 1;
  staged.is_dense = cloud.is_dense;
  staged.resize(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    staged[i] = cloud[static_cast<std::size_t>(indices[i])];
  return staged;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
transformCloudXYZBySourceIndices(const pcl::PointCloud<PointT>& source,
                                 const pcl::Indices& indices,
                                 const Eigen::Matrix4f& transform)
{
  pcl::PointCloud<PointT> target;
  target.width = static_cast<std::uint32_t>(indices.size());
  target.height = 1;
  target.is_dense = source.is_dense;
  target.resize(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    target[i] = source[static_cast<std::size_t>(indices[i])];
    const Eigen::Vector4f p(target[i].x, target[i].y, target[i].z, 1.0f);
    const Eigen::Vector4f q = transform * p;
    target[i].x = q.x();
    target[i].y = q.y();
    target[i].z = q.z();
  }
  return target;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
transformCloudXYZByPairedIndices(const pcl::PointCloud<PointT>& source,
                                 const pcl::Indices& source_indices,
                                 const pcl::Indices& target_indices,
                                 const std::size_t target_size,
                                 const Eigen::Matrix4f& transform)
{
  pcl::PointCloud<PointT> target;
  target.width = static_cast<std::uint32_t>(target_size);
  target.height = 1;
  target.is_dense = source.is_dense;
  target.resize(target_size);
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const std::size_t source_index = static_cast<std::size_t>(source_indices[i]);
    const std::size_t target_index = static_cast<std::size_t>(target_indices[i]);
    target[target_index] = source[source_index];
    const Eigen::Vector4f p(target[target_index].x,
                            target[target_index].y,
                            target[target_index].z,
                            1.0f);
    const Eigen::Vector4f q = transform * p;
    target[target_index].x = q.x();
    target[target_index].y = q.y();
    target[target_index].z = q.z();
  }
  return target;
}

} // namespace pcl::registration::rvv_tedq_support
