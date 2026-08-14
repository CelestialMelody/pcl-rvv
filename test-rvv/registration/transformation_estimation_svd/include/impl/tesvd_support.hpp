/*
 * 本文件做什么：
 * 这里放 transformation_estimation_svd 的 test-only support（测试支撑）基础类型和
 * fixture（夹具）/样本构造 helper。它们被 correctness、bench 和 candidate helper 复用，
 * 但不代表 production dispatch（生产分流）本身。
 *
 * 证据边界：
 * 这些 helper 只负责构造确定性点云、indices、correspondence 和固定刚体变换，以及保存
 * candidate / reference 共享的统计结构。真正的 RVV candidate、生产 direct helper 和板卡
 * 证据仍在其它文件里闭合。
 */

#pragma once

#include <pcl/common/eigen.h>
#include <pcl/common/transforms.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace pcl::registration::rvv_tesvd_support {

struct CandidateStats {
  bool used_rvv{false};
  bool used_fallback{false};
  bool layout_supported{false};
  std::size_t input_points{0};
  std::size_t accepted_points{0};
};

struct FusedAccumulation {
  float source_sum[3]{0.0f, 0.0f, 0.0f};
  float target_sum[3]{0.0f, 0.0f, 0.0f};
  float target_source_sum[9]{0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f};
  std::size_t count{0};
};

inline Eigen::Matrix4f
makeRigidTransform()
{
  const float angle = 0.37f;
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform(0, 0) = c;
  transform(0, 1) = -s;
  transform(1, 0) = s;
  transform(1, 1) = c;
  transform(0, 3) = 0.83f;
  transform(1, 3) = -1.17f;
  transform(2, 3) = 0.41f;
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
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.013f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.009f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.011f;
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
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.012f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 4093) - 2046) * 0.010f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 17) % 4091) - 2045) * 0.008f;
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
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.011f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 11) % 4093) - 2046) * 0.007f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 19) % 4091) - 2045) * 0.013f;
    cloud[i].r = static_cast<std::uint8_t>((i * 3) % 255);
    cloud[i].g = static_cast<std::uint8_t>((i * 5) % 255);
    cloud[i].b = static_cast<std::uint8_t>((i * 7) % 255);
  }
  return cloud;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
makePointXYZLikeCloud(const std::size_t n)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.014f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 9) % 4093) - 2046) * 0.010f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 15) % 4091) - 2045) * 0.012f;
  }
  return cloud;
}

inline pcl::Indices
makeSourceIndices(const std::size_t source_size, const std::size_t n)
{
  pcl::Indices indices;
  indices.resize(n);
  if (source_size == 0) {
    std::fill(indices.begin(), indices.end(), static_cast<pcl::index_t>(0));
    return indices;
  }
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
  if (target_size == 0) {
    std::fill(indices.begin(), indices.end(), static_cast<pcl::index_t>(0));
    return indices;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t index = (i * 19 + 131) % target_size;
    indices[i] = static_cast<pcl::index_t>(index);
  }
  return indices;
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
    const std::size_t source_index = static_cast<std::size_t>(indices[i]);
    target[i] = source[source_index];
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
transformCloudXYZByIndexedPairs(const pcl::PointCloud<PointT>& source,
                                const pcl::Indices& source_indices,
                                const pcl::Indices& target_indices,
                                const Eigen::Matrix4f& transform)
{
  pcl::PointCloud<PointT> target = makePointXYZLikeCloud<PointT>(
      target_indices.empty()
          ? 0
          : static_cast<std::size_t>(*std::max_element(target_indices.begin(),
                                                       target_indices.end())) + 1);
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  for (std::size_t i = 0; i < n; ++i) {
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

} // namespace pcl::registration::rvv_tesvd_support
