/*
 * 本文件做什么：
 * 本文件负责 TE2D row source（行来源）统计、合法性检查、索引展开和物化。
 *
 * materialize / index 展开成本属于测试和 bench 证据边界，不能替代 production 证据。
 */

#pragma once

#include "te2d_layout_helpers.hpp"

namespace pcl::registration::rvv_te2d_support {

template <typename PointT>
inline std::size_t
countFiniteIndexedRows(const pcl::PointCloud<PointT>& cloud,
                       const pcl::Indices& indices)
{
  std::size_t finite_count = 0;
  for (const auto index : indices) {
    if (isValidCloudIndex(cloud, index) &&
        pcl::isFinite(cloud[static_cast<std::size_t>(index)]))
      ++finite_count;
  }
  return finite_count;
}

template <typename PointSource>
inline std::size_t
countFiniteCorrespondenceQueryRows(const pcl::PointCloud<PointSource>& source,
                                   const pcl::Correspondences& correspondences)
{
  std::size_t finite_count = 0;
  for (const auto& correspondence : correspondences) {
    const auto index = static_cast<pcl::index_t>(correspondence.index_query);
    if (isValidCloudIndex(source, index) &&
        pcl::isFinite(source[static_cast<std::size_t>(index)]))
      ++finite_count;
  }
  return finite_count;
}

template <typename PointTarget>
inline std::size_t
countFiniteCorrespondenceMatchRows(const pcl::PointCloud<PointTarget>& target,
                                   const pcl::Correspondences& correspondences)
{
  std::size_t finite_count = 0;
  for (const auto& correspondence : correspondences) {
    const auto index = static_cast<pcl::index_t>(correspondence.index_match);
    if (isValidCloudIndex(target, index) &&
        pcl::isFinite(target[static_cast<std::size_t>(index)]))
      ++finite_count;
  }
  return finite_count;
}

// 这组 helper 把不同 row source policy（行来源策略）物化成顺序点云对。
// materialize 成本属于调用方 bench 的计时边界；它不能被隐藏在 candidate 之外，
// 因为 source-indexed、dual-indexed 和 correspondence-pair 的 gather / index 展开
// 正是本阶段要观察的性能风险。helper 只接受合法索引，非法索引不属于本阶段
// 的 public semantics（公开接口语义）合同。
template <typename PointSource, typename PointTarget>
inline bool
materializeSourceIndexedPair(const pcl::PointCloud<PointSource>& source,
                             const pcl::Indices& source_indices,
                             const pcl::PointCloud<PointTarget>& target,
                             pcl::PointCloud<PointSource>& materialized_source,
                             pcl::PointCloud<PointTarget>& materialized_target)
{
  if (source_indices.size() != target.size())
    return false;

  materialized_source.width = static_cast<std::uint32_t>(source_indices.size());
  materialized_source.height = 1;
  materialized_source.is_dense = source.is_dense && target.is_dense;
  materialized_source.resize(source_indices.size());
  materialized_target = target;
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto index = source_indices[i];
    if (index < 0 || static_cast<std::size_t>(index) >= source.size())
      return false;
    materialized_source[i] = source[static_cast<std::size_t>(index)];
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
materializeDualIndexedPair(const pcl::PointCloud<PointSource>& source,
                           const pcl::Indices& source_indices,
                           const pcl::PointCloud<PointTarget>& target,
                           const pcl::Indices& target_indices,
                           pcl::PointCloud<PointSource>& materialized_source,
                           pcl::PointCloud<PointTarget>& materialized_target)
{
  if (source_indices.size() != target_indices.size())
    return false;

  materialized_source.width = static_cast<std::uint32_t>(source_indices.size());
  materialized_source.height = 1;
  materialized_source.is_dense = source.is_dense && target.is_dense;
  materialized_source.resize(source_indices.size());
  materialized_target.width = static_cast<std::uint32_t>(target_indices.size());
  materialized_target.height = 1;
  materialized_target.is_dense = source.is_dense && target.is_dense;
  materialized_target.resize(target_indices.size());
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto source_index = source_indices[i];
    const auto target_index = target_indices[i];
    if (source_index < 0 || target_index < 0 ||
        static_cast<std::size_t>(source_index) >= source.size() ||
        static_cast<std::size_t>(target_index) >= target.size())
      return false;
    materialized_source[i] = source[static_cast<std::size_t>(source_index)];
    materialized_target[i] = target[static_cast<std::size_t>(target_index)];
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
materializeCorrespondencePair(const pcl::PointCloud<PointSource>& source,
                              const pcl::PointCloud<PointTarget>& target,
                              const pcl::Correspondences& correspondences,
                              pcl::PointCloud<PointSource>& materialized_source,
                              pcl::PointCloud<PointTarget>& materialized_target)
{
  materialized_source.width = static_cast<std::uint32_t>(correspondences.size());
  materialized_source.height = 1;
  materialized_source.is_dense = source.is_dense && target.is_dense;
  materialized_source.resize(correspondences.size());
  materialized_target.width = static_cast<std::uint32_t>(correspondences.size());
  materialized_target.height = 1;
  materialized_target.is_dense = source.is_dense && target.is_dense;
  materialized_target.resize(correspondences.size());
  for (std::size_t i = 0; i < correspondences.size(); ++i) {
    const auto& correspondence = correspondences[i];
    if (correspondence.index_query < 0 || correspondence.index_match < 0 ||
        static_cast<std::size_t>(correspondence.index_query) >= source.size() ||
        static_cast<std::size_t>(correspondence.index_match) >= target.size())
      return false;
    materialized_source[i] =
        source[static_cast<std::size_t>(correspondence.index_query)];
    materialized_target[i] =
        target[static_cast<std::size_t>(correspondence.index_match)];
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
extractCorrespondenceIndices(const pcl::PointCloud<PointSource>& source,
                             const pcl::PointCloud<PointTarget>& target,
                             const pcl::Correspondences& correspondences,
                             pcl::Indices& source_indices,
                             pcl::Indices& target_indices)
{
  source_indices.resize(correspondences.size());
  target_indices.resize(correspondences.size());
  for (std::size_t i = 0; i < correspondences.size(); ++i) {
    const auto& correspondence = correspondences[i];
    const auto query_index = static_cast<pcl::index_t>(correspondence.index_query);
    const auto match_index = static_cast<pcl::index_t>(correspondence.index_match);
    if (!isValidCloudIndex(source, query_index) ||
        !isValidCloudIndex(target, match_index))
      return false;
    source_indices[i] = query_index;
    target_indices[i] = match_index;
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
isValidDenseSourceIndexedPair(const pcl::PointCloud<PointSource>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<PointTarget>& target)
{
  if (!source.is_dense || !target.is_dense || source.empty() ||
      source_indices.empty() || source_indices.size() != target.size())
    return false;
  if (source.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>())
    return false;
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto index = source_indices[i];
    if (!isValidCloudIndex(source, index))
      return false;
    if (!pcl::isFinite(source[static_cast<std::size_t>(index)]) ||
        !pcl::isFinite(target[i]))
      return false;
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
isValidDenseDualIndexedPair(const pcl::PointCloud<PointSource>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<PointTarget>& target,
                            const pcl::Indices& target_indices)
{
  if (!source.is_dense || !target.is_dense || source.empty() || target.empty() ||
      source_indices.empty() || source_indices.size() != target_indices.size())
    return false;
  if (source.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() ||
      target.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>())
    return false;
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto source_index = source_indices[i];
    const auto target_index = target_indices[i];
    if (!isValidCloudIndex(source, source_index) ||
        !isValidCloudIndex(target, target_index))
      return false;
    if (!pcl::isFinite(source[static_cast<std::size_t>(source_index)]) ||
        !pcl::isFinite(target[static_cast<std::size_t>(target_index)]))
      return false;
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
isValidDenseCorrespondencePair(const pcl::PointCloud<PointSource>& source,
                               const pcl::PointCloud<PointTarget>& target,
                               const pcl::Correspondences& correspondences)
{
  if (!source.is_dense || !target.is_dense || source.empty() || target.empty() ||
      correspondences.empty())
    return false;
  if (source.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() ||
      target.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>())
    return false;
  for (const auto& correspondence : correspondences) {
    const auto query_index = static_cast<pcl::index_t>(correspondence.index_query);
    const auto match_index = static_cast<pcl::index_t>(correspondence.index_match);
    if (!isValidCloudIndex(source, query_index) ||
        !isValidCloudIndex(target, match_index))
      return false;
    if (!pcl::isFinite(source[static_cast<std::size_t>(query_index)]) ||
        !pcl::isFinite(target[static_cast<std::size_t>(match_index)]))
      return false;
  }
  return true;
}


} // namespace pcl::registration::rvv_te2d_support
