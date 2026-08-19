/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_2D 的 fixtures（输入样本）、标量 reference
 * （参考链路）和 fused 2D correlation accumulator（融合 2D 相关项累加器）candidate。
 *
 * 证据边界：
 * 当前 helper 只服务 test-rvv 诊断。它不会修改 production 源码，也不能证明真实公开入口
 * 已经命中 RVV。非有限输入、索引路径和 correspondence-pair（对应关系点对）只在测试和
 * 文档中刻画，不从顺序点云对 candidate 外推。
 */

#pragma once

#include <pcl/common/point_tests.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/registration/transformation_estimation_2D.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_te2d_support {

struct CandidateStats {
  bool used_rvv{false};
  bool used_fallback{false};
  bool layout_supported{false};
  bool source_layout_supported{false};
  bool target_layout_supported{false};
  bool dense_finite_input{false};
  std::size_t input_points{0};
  std::size_t accepted_points{0};
  std::size_t source_finite_points{0};
  std::size_t target_finite_points{0};
};

struct Fused2DAccumulation {
  float source_centroid[2]{0.0f, 0.0f};
  float target_centroid[2]{0.0f, 0.0f};
  float correlation[4]{0.0f, 0.0f, 0.0f, 0.0f};
  std::size_t count{0};
};

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

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DStd(const pcl::PointCloud<PointSource>& source,
                     const pcl::PointCloud<PointTarget>& target)
{
  Fused2DAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (std::size_t i = 0; i < n; ++i) {
    source_sum[0] += source[i].x;
    source_sum[1] += source[i].y;
    target_sum[0] += target[i].x;
    target_sum[1] += target[i].y;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = source_sum[0] * inv_n;
  acc.source_centroid[1] = source_sum[1] * inv_n;
  acc.target_centroid[0] = target_sum[0] * inv_n;
  acc.target_centroid[1] = target_sum[1] * inv_n;

  for (std::size_t i = 0; i < n; ++i) {
    const float sx = source[i].x - acc.source_centroid[0];
    const float sy = source[i].y - acc.source_centroid[1];
    const float tx = target[i].x - acc.target_centroid[0];
    const float ty = target[i].y - acc.target_centroid[1];
    acc.correlation[0] += sx * tx;
    acc.correlation[1] += sx * ty;
    acc.correlation[2] += sy * tx;
    acc.correlation[3] += sy * ty;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DSourceIndexedDirectStd(const pcl::PointCloud<PointSource>& source,
                                        const pcl::Indices& source_indices,
                                        const pcl::PointCloud<PointTarget>& target)
{
  Fused2DAccumulation acc;
  const std::size_t n = source_indices.size();
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (std::size_t i = 0; i < n; ++i) {
    const auto& source_point = source[static_cast<std::size_t>(source_indices[i])];
    const auto& target_point = target[i];
    source_sum[0] += source_point.x;
    source_sum[1] += source_point.y;
    target_sum[0] += target_point.x;
    target_sum[1] += target_point.y;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = source_sum[0] * inv_n;
  acc.source_centroid[1] = source_sum[1] * inv_n;
  acc.target_centroid[0] = target_sum[0] * inv_n;
  acc.target_centroid[1] = target_sum[1] * inv_n;

  for (std::size_t i = 0; i < n; ++i) {
    const auto& source_point = source[static_cast<std::size_t>(source_indices[i])];
    const auto& target_point = target[i];
    const float sx = source_point.x - acc.source_centroid[0];
    const float sy = source_point.y - acc.source_centroid[1];
    const float tx = target_point.x - acc.target_centroid[0];
    const float ty = target_point.y - acc.target_centroid[1];
    acc.correlation[0] += sx * tx;
    acc.correlation[1] += sx * ty;
    acc.correlation[2] += sy * tx;
    acc.correlation[3] += sy * ty;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DDualIndexedDirectStd(const pcl::PointCloud<PointSource>& source,
                                      const pcl::Indices& source_indices,
                                      const pcl::PointCloud<PointTarget>& target,
                                      const pcl::Indices& target_indices)
{
  Fused2DAccumulation acc;
  const std::size_t n = source_indices.size();
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (std::size_t i = 0; i < n; ++i) {
    const auto& source_point = source[static_cast<std::size_t>(source_indices[i])];
    const auto& target_point = target[static_cast<std::size_t>(target_indices[i])];
    source_sum[0] += source_point.x;
    source_sum[1] += source_point.y;
    target_sum[0] += target_point.x;
    target_sum[1] += target_point.y;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = source_sum[0] * inv_n;
  acc.source_centroid[1] = source_sum[1] * inv_n;
  acc.target_centroid[0] = target_sum[0] * inv_n;
  acc.target_centroid[1] = target_sum[1] * inv_n;

  for (std::size_t i = 0; i < n; ++i) {
    const auto& source_point = source[static_cast<std::size_t>(source_indices[i])];
    const auto& target_point = target[static_cast<std::size_t>(target_indices[i])];
    const float sx = source_point.x - acc.source_centroid[0];
    const float sy = source_point.y - acc.source_centroid[1];
    const float tx = target_point.x - acc.target_centroid[0];
    const float ty = target_point.y - acc.target_centroid[1];
    acc.correlation[0] += sx * tx;
    acc.correlation[1] += sx * ty;
    acc.correlation[2] += sy * tx;
    acc.correlation[3] += sy * ty;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DCorrespondenceDirectStd(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  Fused2DAccumulation acc;
  const std::size_t n = correspondences.size();
  acc.count = n;
  if (n == 0)
    return acc;

  float source_sum[2]{0.0f, 0.0f};
  float target_sum[2]{0.0f, 0.0f};
  for (const auto& correspondence : correspondences) {
    const auto& source_point =
        source[static_cast<std::size_t>(correspondence.index_query)];
    const auto& target_point =
        target[static_cast<std::size_t>(correspondence.index_match)];
    source_sum[0] += source_point.x;
    source_sum[1] += source_point.y;
    target_sum[0] += target_point.x;
    target_sum[1] += target_point.y;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = source_sum[0] * inv_n;
  acc.source_centroid[1] = source_sum[1] * inv_n;
  acc.target_centroid[0] = target_sum[0] * inv_n;
  acc.target_centroid[1] = target_sum[1] * inv_n;

  for (const auto& correspondence : correspondences) {
    const auto& source_point =
        source[static_cast<std::size_t>(correspondence.index_query)];
    const auto& target_point =
        target[static_cast<std::size_t>(correspondence.index_match)];
    const float sx = source_point.x - acc.source_centroid[0];
    const float sy = source_point.y - acc.source_centroid[1];
    const float tx = target_point.x - acc.target_centroid[0];
    const float ty = target_point.y - acc.target_centroid[1];
    acc.correlation[0] += sx * tx;
    acc.correlation[1] += sx * ty;
    acc.correlation[2] += sy * tx;
    acc.correlation[3] += sy * ty;
  }
  return acc;
}

#ifdef __RVV10__
inline float
reduceSum(vfloat32m2_t value, const std::size_t vlmax)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return __riscv_vfmv_f_s_f32m1_f32(
      __riscv_vfredosum_vs_f32m2_f32m1(value, zero, vlmax));
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DRVV(const pcl::PointCloud<PointSource>& source,
                     const pcl::PointCloud<PointTarget>& target)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  static_assert(SourceLayout::value && TargetLayout::value,
                "RVV fused candidate requires traits-gated xyz AoS layouts");

  Fused2DAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero;
  vfloat32m2_t stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = reduceSum(ssx, vlmax) * inv_n;
  acc.source_centroid[1] = reduceSum(ssy, vlmax) * inv_n;
  acc.target_centroid[0] = reduceSum(stx, vlmax) * inv_n;
  acc.target_centroid[1] = reduceSum(sty, vlmax) * inv_n;

  vfloat32m2_t sx_tx = zero, sx_ty = zero, sy_tx = zero, sy_ty = zero;
  i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    const vfloat32m2_t source_x = __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t source_y = __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t target_x = __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t target_y = __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
    sx_tx = __riscv_vfmacc_vv_f32m2_tu(sx_tx, source_x, target_x, vl);
    sx_ty = __riscv_vfmacc_vv_f32m2_tu(sx_ty, source_x, target_y, vl);
    sy_tx = __riscv_vfmacc_vv_f32m2_tu(sy_tx, source_y, target_x, vl);
    sy_ty = __riscv_vfmacc_vv_f32m2_tu(sy_ty, source_y, target_y, vl);
    i += vl;
  }

  acc.correlation[0] = reduceSum(sx_tx, vlmax);
  acc.correlation[1] = reduceSum(sx_ty, vlmax);
  acc.correlation[2] = reduceSum(sy_tx, vlmax);
  acc.correlation[3] = reduceSum(sy_ty, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DSourceIndexedDirectRVV(const pcl::PointCloud<PointSource>& source,
                                        const pcl::Indices& source_indices,
                                        const pcl::PointCloud<PointTarget>& target)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "direct source-indexed RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "direct source-indexed RVV candidate expects 32-bit PCL indices");

  Fused2DAccumulation acc;
  const std::size_t n = source_indices.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* indices_i32 = reinterpret_cast<const std::int32_t*>(source_indices.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_idx_i32 = __riscv_vle32_v_i32m2(indices_i32 + i, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_off, vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = reduceSum(ssx, vlmax) * inv_n;
  acc.source_centroid[1] = reduceSum(ssy, vlmax) * inv_n;
  acc.target_centroid[0] = reduceSum(stx, vlmax) * inv_n;
  acc.target_centroid[1] = reduceSum(sty, vlmax) * inv_n;

  vfloat32m2_t sx_tx = zero, sx_ty = zero, sy_tx = zero, sy_ty = zero;
  i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_idx_i32 = __riscv_vle32_v_i32m2(indices_i32 + i, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_off, vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    const vfloat32m2_t source_x =
        __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t source_y =
        __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t target_x =
        __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t target_y =
        __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
    sx_tx = __riscv_vfmacc_vv_f32m2_tu(sx_tx, source_x, target_x, vl);
    sx_ty = __riscv_vfmacc_vv_f32m2_tu(sx_ty, source_x, target_y, vl);
    sy_tx = __riscv_vfmacc_vv_f32m2_tu(sy_tx, source_y, target_x, vl);
    sy_ty = __riscv_vfmacc_vv_f32m2_tu(sy_ty, source_y, target_y, vl);
    i += vl;
  }

  acc.correlation[0] = reduceSum(sx_tx, vlmax);
  acc.correlation[1] = reduceSum(sx_ty, vlmax);
  acc.correlation[2] = reduceSum(sy_tx, vlmax);
  acc.correlation[3] = reduceSum(sy_ty, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DDualIndexedDirectRVV(const pcl::PointCloud<PointSource>& source,
                                      const pcl::Indices& source_indices,
                                      const pcl::PointCloud<PointTarget>& target,
                                      const pcl::Indices& target_indices)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  using TargetPod = typename TargetLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "direct dual-indexed RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "direct dual-indexed RVV candidate expects 32-bit PCL indices");

  Fused2DAccumulation acc;
  const std::size_t n = source_indices.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* source_idx_i32 =
      reinterpret_cast<const std::int32_t*>(source_indices.data());
  const auto* target_idx_i32 =
      reinterpret_cast<const std::int32_t*>(target_indices.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_source_idx_i32 = __riscv_vle32_v_i32m2(source_idx_i32 + i, vl);
    const vint32m2_t v_target_idx_i32 = __riscv_vle32_v_i32m2(target_idx_i32 + i, vl);
    const vuint32m2_t v_source_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_source_idx_i32), vl);
    const vuint32m2_t v_target_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_target_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_source_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_target_off, vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = reduceSum(ssx, vlmax) * inv_n;
  acc.source_centroid[1] = reduceSum(ssy, vlmax) * inv_n;
  acc.target_centroid[0] = reduceSum(stx, vlmax) * inv_n;
  acc.target_centroid[1] = reduceSum(sty, vlmax) * inv_n;

  vfloat32m2_t sx_tx = zero, sx_ty = zero, sy_tx = zero, sy_ty = zero;
  i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_source_idx_i32 = __riscv_vle32_v_i32m2(source_idx_i32 + i, vl);
    const vint32m2_t v_target_idx_i32 = __riscv_vle32_v_i32m2(target_idx_i32 + i, vl);
    const vuint32m2_t v_source_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_source_idx_i32), vl);
    const vuint32m2_t v_target_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_target_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_source_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_target_off, vl, tx, ty, tz);
    const vfloat32m2_t source_x =
        __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t source_y =
        __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t target_x =
        __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t target_y =
        __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
    sx_tx = __riscv_vfmacc_vv_f32m2_tu(sx_tx, source_x, target_x, vl);
    sx_ty = __riscv_vfmacc_vv_f32m2_tu(sx_ty, source_x, target_y, vl);
    sy_tx = __riscv_vfmacc_vv_f32m2_tu(sy_tx, source_y, target_x, vl);
    sy_ty = __riscv_vfmacc_vv_f32m2_tu(sy_ty, source_y, target_y, vl);
    i += vl;
  }

  acc.correlation[0] = reduceSum(sx_tx, vlmax);
  acc.correlation[1] = reduceSum(sx_ty, vlmax);
  acc.correlation[2] = reduceSum(sy_tx, vlmax);
  acc.correlation[3] = reduceSum(sy_ty, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DCorrespondenceDirectRVV(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  using TargetPod = typename TargetLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "direct correspondence RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::Correspondence::index_query) == sizeof(std::int32_t),
                "direct correspondence RVV candidate expects 32-bit correspondence indices");

  Fused2DAccumulation acc;
  const std::size_t n = correspondences.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = reduceSum(ssx, vlmax) * inv_n;
  acc.source_centroid[1] = reduceSum(ssy, vlmax) * inv_n;
  acc.target_centroid[0] = reduceSum(stx, vlmax) * inv_n;
  acc.target_centroid[1] = reduceSum(sty, vlmax) * inv_n;

  vfloat32m2_t sx_tx = zero, sx_ty = zero, sy_tx = zero, sy_ty = zero;
  i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
    const vfloat32m2_t source_x =
        __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t source_y =
        __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t target_x =
        __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t target_y =
        __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
    sx_tx = __riscv_vfmacc_vv_f32m2_tu(sx_tx, source_x, target_x, vl);
    sx_ty = __riscv_vfmacc_vv_f32m2_tu(sx_ty, source_x, target_y, vl);
    sy_tx = __riscv_vfmacc_vv_f32m2_tu(sy_tx, source_y, target_x, vl);
    sy_ty = __riscv_vfmacc_vv_f32m2_tu(sy_ty, source_y, target_y, vl);
    i += vl;
  }

  acc.correlation[0] = reduceSum(sx_tx, vlmax);
  acc.correlation[1] = reduceSum(sx_ty, vlmax);
  acc.correlation[2] = reduceSum(sy_tx, vlmax);
  acc.correlation[3] = reduceSum(sy_ty, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Fused2DAccumulation
accumulateFused2DCorrespondenceChunkedXYZStagingRVV(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TargetLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SourcePod = typename SourceLayout::Pod;
  using TargetPod = typename TargetLayout::Pod;
  static_assert(SourceLayout::value && TargetLayout::value,
                "chunked correspondence RVV candidate requires xyz AoS layouts");
  static_assert(sizeof(pcl::Correspondence::index_query) == sizeof(std::int32_t),
                "chunked correspondence RVV candidate expects 32-bit indices");

  Fused2DAccumulation acc;
  const std::size_t n = correspondences.size();
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, stx = zero, sty = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                       SourceLayout::kX,
                                       SourceLayout::kY,
                                       SourceLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                       TargetLayout::kX,
                                       TargetLayout::kY,
                                       TargetLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(n);
  acc.source_centroid[0] = reduceSum(ssx, vlmax) * inv_n;
  acc.source_centroid[1] = reduceSum(ssy, vlmax) * inv_n;
  acc.target_centroid[0] = reduceSum(stx, vlmax) * inv_n;
  acc.target_centroid[1] = reduceSum(sty, vlmax) * inv_n;

  vfloat32m2_t sx_tx = zero, sx_ty = zero, sy_tx = zero, sy_ty = zero;
  constexpr std::size_t kChunkRows = 256;
  alignas(64) float staged_sx[kChunkRows];
  alignas(64) float staged_sy[kChunkRows];
  alignas(64) float staged_sz[kChunkRows];
  alignas(64) float staged_tx[kChunkRows];
  alignas(64) float staged_ty[kChunkRows];
  alignas(64) float staged_tz[kChunkRows];

  for (std::size_t chunk_begin = 0; chunk_begin < n; chunk_begin += kChunkRows) {
    const std::size_t chunk_count = std::min(kChunkRows, n - chunk_begin);
    std::size_t staged = 0;
    while (staged < chunk_count) {
      const std::size_t global = chunk_begin + staged;
      const std::size_t vl = __riscv_vsetvl_e32m2(chunk_count - staged);
      const auto* query_i32 =
          reinterpret_cast<const std::int32_t*>(query_base + global * corr_stride);
      const auto* match_i32 =
          reinterpret_cast<const std::int32_t*>(match_base + global * corr_stride);
      const vint32m2_t v_query_i32 =
          __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
      const vint32m2_t v_match_i32 =
          __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
      const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SourcePod>(
          __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
      const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TargetPod>(
          __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
      vfloat32m2_t sx, sy, sz, tx, ty, tz;
      pcl::rvv_load::indexed_load3_f32m2<SourcePod,
                                         SourceLayout::kX,
                                         SourceLayout::kY,
                                         SourceLayout::kZ>(
          source_base, v_query_off, vl, sx, sy, sz);
      pcl::rvv_load::indexed_load3_f32m2<TargetPod,
                                         TargetLayout::kX,
                                         TargetLayout::kY,
                                         TargetLayout::kZ>(
          target_base, v_match_off, vl, tx, ty, tz);
      __riscv_vse32_v_f32m2(staged_sx + staged, sx, vl);
      __riscv_vse32_v_f32m2(staged_sy + staged, sy, vl);
      __riscv_vse32_v_f32m2(staged_sz + staged, sz, vl);
      __riscv_vse32_v_f32m2(staged_tx + staged, tx, vl);
      __riscv_vse32_v_f32m2(staged_ty + staged, ty, vl);
      __riscv_vse32_v_f32m2(staged_tz + staged, tz, vl);
      staged += vl;
    }

    staged = 0;
    while (staged < chunk_count) {
      const std::size_t vl = __riscv_vsetvl_e32m2(chunk_count - staged);
      const vfloat32m2_t sx = __riscv_vle32_v_f32m2(staged_sx + staged, vl);
      const vfloat32m2_t sy = __riscv_vle32_v_f32m2(staged_sy + staged, vl);
      const vfloat32m2_t tx = __riscv_vle32_v_f32m2(staged_tx + staged, vl);
      const vfloat32m2_t ty = __riscv_vle32_v_f32m2(staged_ty + staged, vl);
      const vfloat32m2_t source_x =
          __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
      const vfloat32m2_t source_y =
          __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
      const vfloat32m2_t target_x =
          __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
      const vfloat32m2_t target_y =
          __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
      sx_tx = __riscv_vfmacc_vv_f32m2_tu(sx_tx, source_x, target_x, vl);
      sx_ty = __riscv_vfmacc_vv_f32m2_tu(sx_ty, source_x, target_y, vl);
      sy_tx = __riscv_vfmacc_vv_f32m2_tu(sy_tx, source_y, target_x, vl);
      sy_ty = __riscv_vfmacc_vv_f32m2_tu(sy_ty, source_y, target_y, vl);
      staged += vl;
    }
  }

  acc.correlation[0] = reduceSum(sx_tx, vlmax);
  acc.correlation[1] = reduceSum(sx_ty, vlmax);
  acc.correlation[2] = reduceSum(sy_tx, vlmax);
  acc.correlation[3] = reduceSum(sy_ty, vlmax);
  return acc;
}
#endif

inline Eigen::Matrix4f
solveTransform2DFromAccumulation(const Fused2DAccumulation& acc)
{
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  if (acc.count == 0)
    return transform;

  const float src_cx = acc.source_centroid[0];
  const float src_cy = acc.source_centroid[1];
  const float tgt_cx = acc.target_centroid[0];
  const float tgt_cy = acc.target_centroid[1];

  const float h00 = acc.correlation[0];
  const float h01 = acc.correlation[1];
  const float h10 = acc.correlation[2];
  const float h11 = acc.correlation[3];
  const float angle = std::atan2(h01 - h10, h00 + h11);
  const float c = std::cos(angle);
  const float s = std::sin(angle);

  transform(0, 0) = c;
  transform(0, 1) = -s;
  transform(1, 0) = s;
  transform(1, 1) = c;
  transform(0, 3) = tgt_cx - (c * src_cx - s * src_cy);
  transform(1, 3) = tgt_cy - (s * src_cx + c * src_cy);
  transform(2, 3) = 0.0f;
  return transform;
}

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

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DStd(const pcl::PointCloud<PointSource>& source,
                   const pcl::PointCloud<PointTarget>& target,
                   CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
    stats->source_finite_points = countFinitePoints(source);
    stats->target_finite_points = countFinitePoints(target);
    stats->dense_finite_input = isDenseFiniteOrderedPair(source, target);
    stats->used_fallback = true;
  }
  return solveTransform2DFromAccumulation(accumulateFused2DStd(source, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DCandidate(const pcl::PointCloud<PointSource>& source,
                         const pcl::PointCloud<PointTarget>& target,
                         CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = std::min(source.size(), target.size());
    stats->source_finite_points = countFinitePoints(source);
    stats->target_finite_points = countFinitePoints(target);
  }

  if (source.size() != target.size() || source.empty()) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

  const bool dense_finite = isDenseFiniteOrderedPair(source, target);
  if (stats) {
    stats->dense_finite_input = dense_finite;
    stats->accepted_points = dense_finite ? source.size() : 0;
  }

  if (!dense_finite) {
    if (stats)
      stats->used_fallback = true;
    return estimatePublic2D(source, target);
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(accumulateFused2DRVV(source, target));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateFused2DStd(source, target);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DSourceIndexedCandidate(const pcl::PointCloud<PointSource>& source,
                                      const pcl::Indices& source_indices,
                                      const pcl::PointCloud<PointTarget>& target,
                                      CandidateStats* stats = nullptr)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeSourceIndexedPair(
          source, source_indices, target, materialized_source, materialized_target)) {
    if (stats) {
      *stats = {};
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }
  return estimateFused2DCandidate(materialized_source, materialized_target, stats);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DDualIndexedCandidate(const pcl::PointCloud<PointSource>& source,
                                    const pcl::Indices& source_indices,
                                    const pcl::PointCloud<PointTarget>& target,
                                    const pcl::Indices& target_indices,
                                    CandidateStats* stats = nullptr)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeDualIndexedPair(source,
                                  source_indices,
                                  target,
                                  target_indices,
                                  materialized_source,
                                  materialized_target)) {
    if (stats) {
      *stats = {};
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }
  return estimateFused2DCandidate(materialized_source, materialized_target, stats);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DCorrespondenceCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!materializeCorrespondencePair(source,
                                     target,
                                     correspondences,
                                     materialized_source,
                                     materialized_target)) {
    if (stats) {
      *stats = {};
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }
  return estimateFused2DCandidate(materialized_source, materialized_target, stats);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DSourceIndexedDirectGatherCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = source_indices.size();
    stats->source_finite_points = countFiniteIndexedRows(source, source_indices);
    stats->target_finite_points = countFinitePrefixRows(target, source_indices.size());
  }

  const bool valid_dense = isValidDenseSourceIndexedPair(source, source_indices, target);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? source_indices.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source_indices.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DSourceIndexedDirectRVV(source, source_indices, target));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DSourceIndexedDirectStd(source, source_indices, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DDualIndexedDirectGatherCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& target_indices,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = source_indices.size();
    stats->source_finite_points = countFiniteIndexedRows(source, source_indices);
    stats->target_finite_points = countFiniteIndexedRows(target, target_indices);
  }

  const bool valid_dense =
      isValidDenseDualIndexedPair(source, source_indices, target, target_indices);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? source_indices.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (source_indices.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DDualIndexedDirectRVV(
              source, source_indices, target, target_indices));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DDualIndexedDirectStd(
          source, source_indices, target, target_indices));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DCorrespondenceDirectGatherCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = correspondences.size();
    stats->source_finite_points =
        countFiniteCorrespondenceQueryRows(source, correspondences);
    stats->target_finite_points =
        countFiniteCorrespondenceMatchRows(target, correspondences);
  }

  const bool valid_dense = isValidDenseCorrespondencePair(source, target, correspondences);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? correspondences.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (correspondences.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DCorrespondenceDirectRVV(source, target, correspondences));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DCorrespondenceDirectStd(source, target, correspondences));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    populateLayoutStats<PointSource, PointTarget>(stats);
    stats->input_points = correspondences.size();
    stats->source_finite_points =
        countFiniteCorrespondenceQueryRows(source, correspondences);
    stats->target_finite_points =
        countFiniteCorrespondenceMatchRows(target, correspondences);
  }

  const bool valid_dense = isValidDenseCorrespondencePair(source, target, correspondences);
  if (stats) {
    stats->dense_finite_input = valid_dense;
    stats->accepted_points = valid_dense ? correspondences.size() : 0;
  }
  if (!valid_dense) {
    if (stats)
      stats->used_fallback = true;
    return Eigen::Matrix4f::Identity();
  }

#ifdef __RVV10__
  if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value &&
                pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value) {
    if (correspondences.size() >= 16) {
      if (stats)
        stats->used_rvv = true;
      return solveTransform2DFromAccumulation(
          accumulateFused2DCorrespondenceChunkedXYZStagingRVV(
              source, target, correspondences));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return solveTransform2DFromAccumulation(
      accumulateFused2DCorrespondenceDirectStd(source, target, correspondences));
}

inline float
matrixMaxAbsDiff(const Eigen::Matrix4f& a, const Eigen::Matrix4f& b)
{
  return (a - b).cwiseAbs().maxCoeff();
}

inline std::uint64_t
matrixChecksum(const Eigen::Matrix4f& matrix)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      const auto scaled = static_cast<std::int64_t>(matrix(r, c) * 1000000.0f);
      checksum = (checksum ^ static_cast<std::uint64_t>(scaled)) * 1099511628211ull;
    }
  }
  return checksum;
}

} // namespace pcl::registration::rvv_te2d_support
