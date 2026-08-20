/*
 * 本文件做什么：
 * 本文件负责调用真实 TransformationEstimation2D public overload 的 test-only wrapper。
 *
 * wrapper 用于对拍和 family A/B anchor，本身不改变 production 源码。
 */

#pragma once

#include "te2d_core_types.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublic2D(const pcl::PointCloud<PointSource>& source,
                 const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float> estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicSourceIndexed2D(const pcl::PointCloud<PointSource>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float> estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, source_indices, target, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicDualIndexed2D(const pcl::PointCloud<PointSource>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<PointTarget>& target,
                            const pcl::Indices& target_indices)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float> estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicCorrespondence2D(const pcl::PointCloud<PointSource>& source,
                               const pcl::PointCloud<PointTarget>& target,
                               const pcl::Correspondences& correspondences)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float> estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, transform);
  return transform;
}

} // namespace pcl::registration::rvv_te2d_support
