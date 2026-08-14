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
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 4091) - 2045) * 0.004f;
  }
  return cloud;
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
                                       offsetof(PointSource, x),
                                       offsetof(PointSource, y),
                                       offsetof(PointSource, z)>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       offsetof(PointTarget, x),
                                       offsetof(PointTarget, y),
                                       offsetof(PointTarget, z)>(
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
                                       offsetof(PointSource, x),
                                       offsetof(PointSource, y),
                                       offsetof(PointSource, z)>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       offsetof(PointTarget, x),
                                       offsetof(PointTarget, y),
                                       offsetof(PointTarget, z)>(
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
estimateFused2DStd(const pcl::PointCloud<PointSource>& source,
                   const pcl::PointCloud<PointTarget>& target,
                   CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
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
  if constexpr (pcl::rvv_load::kRVVXYZPointCompatible<PointSource> &&
                pcl::rvv_load::kRVVXYZPointCompatible<PointTarget>) {
    if (stats)
      stats->layout_supported = true;
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
