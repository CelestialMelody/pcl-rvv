/*
 * 本文件做什么：
 * 本文件负责 TE2D production-detail family A/B 对照 wrapper。
 *
 * 这些 wrapper 通过物化或 staged indices 连接真实 public overload，只支撑同边界测试对照。
 */

#pragma once

#include "te2d_public_wrappers.hpp"
#include "te2d_row_sources.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointSource>
inline bool
materializeSourceIndexedSourceRows(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    pcl::PointCloud<PointSource>& materialized_source)
{
  if (source_indices.empty())
    return false;

  materialized_source.width = static_cast<std::uint32_t>(source_indices.size());
  materialized_source.height = 1;
  materialized_source.is_dense = source.is_dense;
  materialized_source.resize(source_indices.size());
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto index = source_indices[i];
    if (!isValidCloudIndex(source, index))
      return false;
    materialized_source[i] = source[static_cast<std::size_t>(index)];
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicSourceIndexedMaterializedOrdered2D(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target)
{
  if (source_indices.size() != target.size())
    return Eigen::Matrix4f::Identity();

  pcl::PointCloud<PointSource> materialized_source;
  if (!materializeSourceIndexedSourceRows(source, source_indices, materialized_source))
    return Eigen::Matrix4f::Identity();

  return estimatePublic2D(materialized_source, target);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicDualIndexedMaterializedOrdered2D(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& target_indices)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeDualIndexedPair(source,
                                  source_indices,
                                  target,
                                  target_indices,
                                  materialized_source,
                                  materialized_target))
    return Eigen::Matrix4f::Identity();

  return estimatePublic2D(materialized_source, materialized_target);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicCorrespondenceMaterializedOrdered2D(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeCorrespondencePair(source,
                                     target,
                                     correspondences,
                                     materialized_source,
                                     materialized_target))
    return Eigen::Matrix4f::Identity();

  return estimatePublic2D(materialized_source, materialized_target);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicCorrespondenceStagedDualIndexed2D(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  pcl::Indices source_indices;
  pcl::Indices target_indices;
  if (!extractCorrespondenceIndices(
          source, target, correspondences, source_indices, target_indices))
    return Eigen::Matrix4f::Identity();

  return estimatePublicDualIndexed2D(source, source_indices, target, target_indices);
}

} // namespace pcl::registration::rvv_te2d_support
