/*
 * 本文件做什么：
 * 本文件负责 TE2D fixtures（输入样本）和二维刚体变换构造。
 *
 * 这些样本只用于 test-rvv correctness / bench 输入，不能扩大 production 覆盖范围。
 */

#pragma once

#include "te2d_core_types.hpp"

namespace pcl::registration::rvv_te2d_support {

inline Eigen::Matrix4f
makeRigid2DTransform(const float angle = 0.37f,
                     const float tx = 0.83f,
                     const float ty = -1.17f)
{
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform(0, 0) = c;
  transform(0, 1) = -s;
  transform(1, 0) = s;
  transform(1, 1) = c;
  transform(0, 3) = tx;
  transform(1, 3) = ty;
  return transform;
}

template <typename PointT>
inline void
setXYZLikeExtraFields(PointT& point, const float field_bias)
{
  if constexpr (pcl::traits::has_field<PointT, pcl::fields::intensity>::value)
    point.intensity = field_bias + 0.25f;

  if constexpr (pcl::traits::has_field<PointT, pcl::fields::normal_x>::value) {
    point.normal_x = field_bias + 0.50f;
    point.normal_y = field_bias - 0.75f;
    point.normal_z = field_bias + 1.25f;
  }

  if constexpr (pcl::traits::has_field<PointT, pcl::fields::curvature>::value)
    point.curvature = field_bias + 1.75f;
}

// 这个 fixture 让不同 PointT 共享同一份 x/y/z 几何，同时把 intensity、normal
// 和 curvature 等额外字段填成非零有限值。它用于证明本候选只读取 x/y/z；
// 额外字段的变化必须不会改变估计矩阵。
template <typename PointT>
inline pcl::PointCloud<PointT>
makeXYZLikeCloud(const std::size_t n, const float field_bias = 0.0f)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.013f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.009f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 4091) - 2045) * 0.004f;
    setXYZLikeExtraFields(
        cloud[i], field_bias + static_cast<float>(i % 17) * 0.031f);
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZ>
makePointXYZCloud(const std::size_t n)
{
  return makeXYZLikeCloud<pcl::PointXYZ>(n);
}

inline pcl::PointCloud<pcl::PointXYZ>
makeNearCancellationCloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float base = 1024.0f + static_cast<float>(i % 17) * 0.125f;
    cloud[i].x = base + static_cast<float>(static_cast<int>(i % 7) - 3) * 0.0017f;
    cloud[i].y = -base + static_cast<float>(static_cast<int>((i * 5) % 11) - 5) * 0.0013f;
    cloud[i].z = 0.0f;
  }
  return cloud;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
transformCloud2D(const pcl::PointCloud<PointT>& source, const Eigen::Matrix4f& transform)
{
  pcl::PointCloud<PointT> target = source;
  for (std::size_t i = 0; i < source.size(); ++i) {
    const Eigen::Vector4f p(source[i].x, source[i].y, source[i].z, 1.0f);
    const Eigen::Vector4f q = transform * p;
    target[i].x = q.x();
    target[i].y = q.y();
    target[i].z = source[i].z;
  }
  target.is_dense = source.is_dense;
  return target;
}

template <typename PointSource, typename PointTarget>
inline pcl::PointCloud<PointTarget>
transformCloud2DTo(const pcl::PointCloud<PointSource>& source,
                   const Eigen::Matrix4f& transform,
                   const float target_field_bias = 0.0f)
{
  pcl::PointCloud<PointTarget> target;
  target.width = source.width;
  target.height = source.height;
  target.is_dense = source.is_dense;
  target.resize(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const Eigen::Vector4f p(source[i].x, source[i].y, source[i].z, 1.0f);
    const Eigen::Vector4f q = transform * p;
    target[i].x = q.x();
    target[i].y = q.y();
    target[i].z = source[i].z;
    setXYZLikeExtraFields(
        target[i], target_field_bias + static_cast<float>(i % 19) * 0.023f);
  }
  return target;
}

} // namespace pcl::registration::rvv_te2d_support
