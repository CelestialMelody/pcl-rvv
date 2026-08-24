/*
 * 本文件做什么：
 * 这里放 transformation_estimation_svd_scale 的 fixture（夹具）和共享统计结构。
 * 这些 helper 只构造确定性点云、scale transform（尺度变换）和 checksum（校验和），
 * 供 reference、RVV candidate、gtest 和 bench 复用。
 *
 * 证据边界：
 * 本文件不包含 RVV intrinsic（RVV 内建函数），也不代表 production dispatch。它只定义
 * 输入样本和输出指纹，使后续诊断能复现。
 */

#pragma once

#include <pcl/common/eigen.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/register_point_struct.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>

struct LocalSVDScalePaddedXYZSource {
  float pre;
  float x;
  std::uint32_t tag;
  float y;
  float pad0;
  float pad1;
  float z;
  float tail;
};

struct LocalSVDScaleWideXYZTarget {
  std::uint32_t id;
  float pad0;
  float x;
  float pad1;
  float pad2;
  float y;
  float pad3;
  float z;
  float tail0;
  float tail1;
};

struct LocalSVDScaleCompactXYZSource {
  float x;
  float y;
  float z;
  std::uint32_t tag;
  float tail;
};

struct LocalSVDScaleCompactXYZTarget {
  float pad0;
  float x;
  float y;
  float z;
  std::uint32_t id;
  float tail;
};

struct LocalSVDScaleHugePaddingXYZSource {
  std::uint64_t pre0;
  float x;
  float pad0[3];
  float y;
  float pad1[5];
  float z;
  float tail[7];
};

struct LocalSVDScaleHugePaddingXYZTarget {
  float pad0[4];
  float x;
  float pad1[7];
  float y;
  float pad2[2];
  float z;
  std::uint64_t id;
  float tail[8];
};

struct alignas(64) LocalSVDScaleAligned64XYZSource {
  std::uint8_t pre[16];
  float x;
  std::uint8_t pad0[12];
  float y;
  std::uint8_t pad1[24];
  float z;
  std::uint8_t tail[7];
};

struct alignas(32) LocalSVDScaleAligned32XYZTarget {
  std::uint8_t pre[28];
  float x;
  std::uint8_t pad0[28];
  float y;
  std::uint8_t pad1[28];
  float z;
  std::uint8_t tail[5];
};

POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScalePaddedXYZSource,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleWideXYZTarget,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleCompactXYZSource,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleCompactXYZTarget,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleHugePaddingXYZSource,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleHugePaddingXYZTarget,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleAligned64XYZSource,
                                  (float, x, x)(float, y, y)(float, z, z))
POINT_CLOUD_REGISTER_POINT_STRUCT(LocalSVDScaleAligned32XYZTarget,
                                  (float, x, x)(float, y, y)(float, z, z))

namespace pcl::registration::rvv_tesvd_scale_support {

struct CandidateStats {
  bool used_rvv{false};
  bool used_fallback{false};
  bool layout_supported{false};
  bool degenerate_source{false};
  std::size_t input_points{0};
  std::size_t accepted_points{0};
};

struct ScaleAccumulation {
  float source_sum[3]{0.0f, 0.0f, 0.0f};
  float target_sum[3]{0.0f, 0.0f, 0.0f};
  float source_target_sum[9]{0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f};
  float source_square_sum{0.0f};
  std::size_t count{0};
};

struct ScaleAccumulationD64 {
  double source_sum[3]{0.0, 0.0, 0.0};
  double target_sum[3]{0.0, 0.0, 0.0};
  double source_target_sum[9]{0.0,
                              0.0,
                              0.0,
                              0.0,
                              0.0,
                              0.0,
                              0.0,
                              0.0,
                              0.0};
  double source_square_sum{0.0};
  std::size_t count{0};
};

struct MatrixLocalScaleStats {
  float max_matrix_error{0.0f};
  std::size_t points{0};
};

inline Eigen::Matrix4f
makeSimilarityTransform()
{
  const float angle = 0.37f;
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  constexpr float scale = 1.37f;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform(0, 0) = scale * c;
  transform(0, 1) = -scale * s;
  transform(1, 0) = scale * s;
  transform(1, 1) = scale * c;
  transform(2, 2) = scale;
  transform(0, 3) = 0.83f;
  transform(1, 3) = -1.17f;
  transform(2, 3) = 0.41f;
  return transform;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
makePointCloudXYZ(const std::size_t n)
{
  pcl::PointCloud<PointT> cloud;
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

inline pcl::PointCloud<pcl::PointXYZ>
makePointXYZCloud(const std::size_t n)
{
  return makePointCloudXYZ<pcl::PointXYZ>(n);
}

template <typename PointT>
inline pcl::PointCloud<PointT>
makeDegeneratePointCloudXYZ(const std::size_t n)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (auto& point : cloud) {
    point.x = 1.0f;
    point.y = -2.0f;
    point.z = 0.5f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZ>
makeDegeneratePointXYZCloud(const std::size_t n)
{
  return makeDegeneratePointCloudXYZ<pcl::PointXYZ>(n);
}

template <typename PointT>
inline pcl::PointCloud<PointT>
transformCloudXYZ(const pcl::PointCloud<PointT>& source,
                  const Eigen::Matrix4f& transform)
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

template <typename PointSource, typename PointTarget>
inline pcl::PointCloud<PointTarget>
transformCloudXYZTo(const pcl::PointCloud<PointSource>& source,
                    const Eigen::Matrix4f& transform)
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
    target[i].z = q.z();
  }
  return target;
}

inline pcl::Indices
makeStrideIndices(const std::size_t count,
                  const std::size_t stride,
                  const std::size_t offset = 0)
{
  pcl::Indices indices;
  indices.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    indices.push_back(static_cast<pcl::index_t>(offset + i * stride));
  return indices;
}

inline pcl::Indices
makeReversedIndices(pcl::Indices indices)
{
  std::reverse(indices.begin(), indices.end());
  return indices;
}

inline pcl::Indices
makeDeterministicShuffledIndices(const pcl::Indices& base)
{
  pcl::Indices indices;
  indices.reserve(base.size());
  if (base.empty())
    return indices;
  constexpr std::size_t step = 73;
  for (std::size_t i = 0; i < base.size(); ++i)
    indices.push_back(base[(i * step) % base.size()]);
  return indices;
}

inline pcl::Indices
makeSortedIndices(pcl::Indices indices)
{
  std::sort(indices.begin(), indices.end());
  return indices;
}

inline std::pair<pcl::Indices, pcl::Indices>
makeSortedIndexPairsBySource(pcl::Indices source_indices, pcl::Indices target_indices)
{
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(n);
  target_indices.resize(n);
  pcl::Indices order(n);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](const std::size_t lhs, const std::size_t rhs) {
    if (source_indices[lhs] != source_indices[rhs])
      return source_indices[lhs] < source_indices[rhs];
    return target_indices[lhs] < target_indices[rhs];
  });
  pcl::Indices sorted_source;
  pcl::Indices sorted_target;
  sorted_source.reserve(n);
  sorted_target.reserve(n);
  for (const std::size_t idx : order) {
    sorted_source.push_back(source_indices[idx]);
    sorted_target.push_back(target_indices[idx]);
  }
  return {std::move(sorted_source), std::move(sorted_target)};
}

inline std::pair<pcl::Indices, pcl::Indices>
makeSortedIndexPairsByTarget(pcl::Indices source_indices, pcl::Indices target_indices)
{
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(n);
  target_indices.resize(n);
  pcl::Indices order(n);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](const std::size_t lhs, const std::size_t rhs) {
    if (target_indices[lhs] != target_indices[rhs])
      return target_indices[lhs] < target_indices[rhs];
    return source_indices[lhs] < source_indices[rhs];
  });
  pcl::Indices sorted_source;
  pcl::Indices sorted_target;
  sorted_source.reserve(n);
  sorted_target.reserve(n);
  for (const std::size_t idx : order) {
    sorted_source.push_back(source_indices[idx]);
    sorted_target.push_back(target_indices[idx]);
  }
  return {std::move(sorted_source), std::move(sorted_target)};
}

inline pcl::Correspondences
makeSortedCorrespondencesByQueryIndex(pcl::Correspondences correspondences)
{
  std::sort(correspondences.begin(), correspondences.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.index_query != rhs.index_query)
      return lhs.index_query < rhs.index_query;
    if (lhs.index_match != rhs.index_match)
      return lhs.index_match < rhs.index_match;
    return lhs.distance < rhs.distance;
  });
  return correspondences;
}

template <typename PointT>
inline pcl::PointCloud<PointT>
selectPointCloudByIndices(const pcl::PointCloud<PointT>& cloud,
                          const pcl::Indices& indices)
{
  pcl::PointCloud<PointT> selected;
  selected.width = static_cast<std::uint32_t>(indices.size());
  selected.height = 1;
  selected.is_dense = cloud.is_dense;
  selected.resize(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    selected[i] = cloud[static_cast<std::size_t>(indices[i])];
  return selected;
}

inline pcl::Correspondences
makeCorrespondences(const pcl::Indices& source_indices, const pcl::Indices& target_indices)
{
  pcl::Correspondences correspondences;
  const std::size_t n = std::min(source_indices.size(), target_indices.size());
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    pcl::Correspondence corr;
    corr.index_query = source_indices[i];
    corr.index_match = target_indices[i];
    corr.distance = 0.0f;
    correspondences.push_back(corr);
  }
  return correspondences;
}

inline std::uint64_t
matrixChecksum(const Eigen::Matrix4f& matrix)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < matrix.rows(); ++r) {
    for (int c = 0; c < matrix.cols(); ++c) {
      const auto quantized =
          static_cast<std::int64_t>(std::llround(static_cast<double>(matrix(r, c)) * 1000000.0));
      checksum ^= static_cast<std::uint64_t>(quantized) + 0x9e3779b97f4a7c15ull +
                  (checksum << 6) + (checksum >> 2);
      checksum *= 1099511628211ull;
    }
  }
  return checksum;
}

inline std::uint64_t
matrixChecksum(const Eigen::Matrix4d& matrix)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < matrix.rows(); ++r) {
    for (int c = 0; c < matrix.cols(); ++c) {
      const auto quantized = static_cast<std::int64_t>(std::llround(matrix(r, c) * 1000000.0));
      checksum ^= static_cast<std::uint64_t>(quantized) + 0x9e3779b97f4a7c15ull +
                  (checksum << 6) + (checksum >> 2);
      checksum *= 1099511628211ull;
    }
  }
  return checksum;
}

} // namespace pcl::registration::rvv_tesvd_scale_support
