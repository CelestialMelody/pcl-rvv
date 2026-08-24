/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_svd_scale 的同构标量 reference（参考链路）和
 * test-only RVV candidate（测试专用 RVV 候选）。candidate 直接从 ordered-cloud-pair
 * 点对累加 source sum、target sum、source-target cross sum 和 source square sum，然后
 * 保留 Eigen 3x3 SVD 作为标量后段。
 *
 * 证据边界：
 * 这些 helper 只证明 scale-aware fused accumulation（带尺度估计的融合累加）的诊断价值。
 * 它们没有接入 production public overload（生产公开重载），也不证明 fallback、泛型点型或
 * indexed / correspondence 路径。
 */

#pragma once

#include "tesvd_scale_support.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/transformation_estimation_svd_scale.h>

#include <Eigen/SVD>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_tesvd_scale_support {

struct MatrixLocalDemeanData {
  Eigen::Matrix<float, 4, Eigen::Dynamic> source_demean;
  Eigen::Matrix<float, 4, Eigen::Dynamic> target_demean;
  Eigen::Vector4f source_centroid{Eigen::Vector4f::Zero()};
  Eigen::Vector4f target_centroid{Eigen::Vector4f::Zero()};
};

inline MatrixLocalDemeanData
makeMatrixLocalDemeanData(const pcl::PointCloud<pcl::PointXYZ>& source,
                          const pcl::PointCloud<pcl::PointXYZ>& target)
{
  MatrixLocalDemeanData data;
  const std::size_t n = std::min(source.size(), target.size());
  data.source_demean.resize(4, static_cast<Eigen::Index>(n));
  data.target_demean.resize(4, static_cast<Eigen::Index>(n));
  if (n == 0) {
    return data;
  }

  for (std::size_t i = 0; i < n; ++i) {
    data.source_centroid.x() += source[i].x;
    data.source_centroid.y() += source[i].y;
    data.source_centroid.z() += source[i].z;
    data.target_centroid.x() += target[i].x;
    data.target_centroid.y() += target[i].y;
    data.target_centroid.z() += target[i].z;
  }
  data.source_centroid /= static_cast<float>(n);
  data.target_centroid /= static_cast<float>(n);
  data.source_centroid.w() = 0.0f;
  data.target_centroid.w() = 0.0f;

  for (std::size_t i = 0; i < n; ++i) {
    data.source_demean(0, static_cast<Eigen::Index>(i)) =
        source[i].x - data.source_centroid.x();
    data.source_demean(1, static_cast<Eigen::Index>(i)) =
        source[i].y - data.source_centroid.y();
    data.source_demean(2, static_cast<Eigen::Index>(i)) =
        source[i].z - data.source_centroid.z();
    data.source_demean(3, static_cast<Eigen::Index>(i)) = 0.0f;
    data.target_demean(0, static_cast<Eigen::Index>(i)) =
        target[i].x - data.target_centroid.x();
    data.target_demean(1, static_cast<Eigen::Index>(i)) =
        target[i].y - data.target_centroid.y();
    data.target_demean(2, static_cast<Eigen::Index>(i)) =
        target[i].z - data.target_centroid.z();
    data.target_demean(3, static_cast<Eigen::Index>(i)) = 0.0f;
  }
  return data;
}

inline Eigen::Matrix3f
rotationFromCorrelation(const Eigen::Matrix3f& H)
{
  Eigen::JacobiSVD<Eigen::Matrix3f> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix3f u = svd.matrixU();
  Eigen::Matrix3f v = svd.matrixV();
  if (u.determinant() * v.determinant() < 0.0f) {
    for (int x = 0; x < 3; ++x)
      v(x, 2) *= -1.0f;
  }
  return v * u.transpose();
}

inline Eigen::Matrix4f
assembleScaleTransform(const Eigen::Matrix3f& R,
                       const float scale,
                       const Eigen::Vector4f& source_centroid,
                       const Eigen::Vector4f& target_centroid)
{
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform.template topLeftCorner<3, 3>() = scale * R;
  transform.template block<3, 1>(0, 3) =
      target_centroid.template head<3>() - scale * R * source_centroid.template head<3>();
  return transform;
}

// 旧后段公式复刻 production 当前 shape：先构造 R4 * source_demean，再逐列与
// target_demean 点积得到 scale 分子。它是 Phase 020 的 baseline，不是新推荐实现。
inline Eigen::Matrix4f
estimateScaleMatrixLocalLegacy(const pcl::PointCloud<pcl::PointXYZ>& source,
                               const pcl::PointCloud<pcl::PointXYZ>& target)
{
  const MatrixLocalDemeanData data = makeMatrixLocalDemeanData(source, target);
  if (data.source_demean.cols() == 0)
    return Eigen::Matrix4f::Identity();

  const Eigen::Matrix3f H =
      (data.source_demean * data.target_demean.transpose()).topLeftCorner<3, 3>();
  const Eigen::Matrix3f R = rotationFromCorrelation(H);

  Eigen::Matrix4f R4 = Eigen::Matrix4f::Identity();
  R4.template topLeftCorner<3, 3>() = R;
  const Eigen::Matrix<float, 4, Eigen::Dynamic> rotated_source = R4 * data.source_demean;

  double sum_ss = 0.0;
  double sum_tt = 0.0;
  for (Eigen::Index i = 0; i < data.source_demean.cols(); ++i) {
    sum_ss += static_cast<double>(data.source_demean(0, i)) * data.source_demean(0, i);
    sum_ss += static_cast<double>(data.source_demean(1, i)) * data.source_demean(1, i);
    sum_ss += static_cast<double>(data.source_demean(2, i)) * data.source_demean(2, i);
    sum_tt += static_cast<double>(data.target_demean(0, i)) * rotated_source(0, i);
    sum_tt += static_cast<double>(data.target_demean(1, i)) * rotated_source(1, i);
    sum_tt += static_cast<double>(data.target_demean(2, i)) * rotated_source(2, i);
  }
  if (sum_ss <= 0.0 || !std::isfinite(sum_ss))
    return Eigen::Matrix4f::Identity();
  return assembleScaleTransform(
      R, static_cast<float>(sum_tt / sum_ss), data.source_centroid, data.target_centroid);
}

// trace 简化公式验证 `sum_tt = trace(R * H)`。它只移除后段临时矩阵和逐列点积，
// 不改变 centroid / demean / SVD 这几段成本。
inline Eigen::Matrix4f
estimateScaleMatrixLocalTrace(const pcl::PointCloud<pcl::PointXYZ>& source,
                              const pcl::PointCloud<pcl::PointXYZ>& target)
{
  const MatrixLocalDemeanData data = makeMatrixLocalDemeanData(source, target);
  if (data.source_demean.cols() == 0)
    return Eigen::Matrix4f::Identity();

  const Eigen::Matrix3f H =
      (data.source_demean * data.target_demean.transpose()).topLeftCorner<3, 3>();
  const Eigen::Matrix3f R = rotationFromCorrelation(H);
  double sum_ss = 0.0;
  for (Eigen::Index i = 0; i < data.source_demean.cols(); ++i) {
    sum_ss += static_cast<double>(data.source_demean(0, i)) * data.source_demean(0, i);
    sum_ss += static_cast<double>(data.source_demean(1, i)) * data.source_demean(1, i);
    sum_ss += static_cast<double>(data.source_demean(2, i)) * data.source_demean(2, i);
  }
  if (sum_ss <= 0.0 || !std::isfinite(sum_ss))
    return Eigen::Matrix4f::Identity();
  return assembleScaleTransform(
      R, static_cast<float>((R * H).trace() / sum_ss), data.source_centroid, data.target_centroid);
}

inline Eigen::Matrix4f
solveScaleFromAccumulation(const ScaleAccumulation& acc, CandidateStats* stats = nullptr)
{
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  if (stats) {
    stats->input_points = acc.count;
    stats->accepted_points = acc.count;
  }
  if (acc.count == 0)
    return transform;

  const float inv_n = 1.0f / static_cast<float>(acc.count);
  const Eigen::Vector3f source_mean(acc.source_sum[0] * inv_n,
                                    acc.source_sum[1] * inv_n,
                                    acc.source_sum[2] * inv_n);
  const Eigen::Vector3f target_mean(acc.target_sum[0] * inv_n,
                                    acc.target_sum[1] * inv_n,
                                    acc.target_sum[2] * inv_n);

  Eigen::Matrix3f H;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      H(r, c) = acc.source_target_sum[r * 3 + c] -
                static_cast<float>(acc.count) * source_mean[r] * target_mean[c];
    }
  }

  Eigen::JacobiSVD<Eigen::Matrix3f> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix3f u = svd.matrixU();
  Eigen::Matrix3f v = svd.matrixV();
  if (u.determinant() * v.determinant() < 0.0f) {
    for (int x = 0; x < 3; ++x)
      v(x, 2) *= -1.0f;
  }
  const Eigen::Matrix3f R = v * u.transpose();

  const float sum_ss =
      acc.source_square_sum - static_cast<float>(acc.count) * source_mean.squaredNorm();
  if (sum_ss <= 0.0f || !std::isfinite(sum_ss)) {
    if (stats)
      stats->degenerate_source = true;
    return transform;
  }

  const float sum_tt = (R * H).trace();
  const float scale = sum_tt / sum_ss;
  transform.template topLeftCorner<3, 3>() = scale * R;
  transform.template block<3, 1>(0, 3) = target_mean - scale * R * source_mean;
  return transform;
}

inline Eigen::Matrix4d
solveScaleFromAccumulationDouble(const ScaleAccumulationD64& acc,
                                 CandidateStats* stats = nullptr)
{
  Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
  if (stats) {
    stats->input_points = acc.count;
    stats->accepted_points = acc.count;
  }
  if (acc.count == 0)
    return transform;

  const double inv_n = 1.0 / static_cast<double>(acc.count);
  const Eigen::Vector3d source_mean(acc.source_sum[0] * inv_n,
                                    acc.source_sum[1] * inv_n,
                                    acc.source_sum[2] * inv_n);
  const Eigen::Vector3d target_mean(acc.target_sum[0] * inv_n,
                                    acc.target_sum[1] * inv_n,
                                    acc.target_sum[2] * inv_n);

  Eigen::Matrix3d H;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      H(r, c) = acc.source_target_sum[r * 3 + c] -
                static_cast<double>(acc.count) * source_mean[r] * target_mean[c];
    }
  }

  Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix3d u = svd.matrixU();
  Eigen::Matrix3d v = svd.matrixV();
  if (u.determinant() * v.determinant() < 0.0) {
    for (int x = 0; x < 3; ++x)
      v(x, 2) *= -1.0;
  }
  const Eigen::Matrix3d R = v * u.transpose();

  const double sum_ss =
      acc.source_square_sum - static_cast<double>(acc.count) * source_mean.squaredNorm();
  if (sum_ss <= 0.0 || !std::isfinite(sum_ss)) {
    if (stats)
      stats->degenerate_source = true;
    return transform;
  }

  const double sum_tt = (R * H).trace();
  const double scale = sum_tt / sum_ss;
  transform.template topLeftCorner<3, 3>() = scale * R;
  transform.template block<3, 1>(0, 3) = target_mean - scale * R * source_mean;
  return transform;
}

template <typename PointSource, typename PointTarget>
inline ScaleAccumulation
accumulateScaleStd(const pcl::PointCloud<PointSource>& source,
                   const pcl::PointCloud<PointTarget>& target)
{
  ScaleAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  for (std::size_t i = 0; i < n; ++i) {
    const float sx = source[i].x;
    const float sy = source[i].y;
    const float sz = source[i].z;
    const float tx = target[i].x;
    const float ty = target[i].y;
    const float tz = target[i].z;
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.source_target_sum[0] += sx * tx;
    acc.source_target_sum[1] += sx * ty;
    acc.source_target_sum[2] += sx * tz;
    acc.source_target_sum[3] += sy * tx;
    acc.source_target_sum[4] += sy * ty;
    acc.source_target_sum[5] += sy * tz;
    acc.source_target_sum[6] += sz * tx;
    acc.source_target_sum[7] += sz * ty;
    acc.source_target_sum[8] += sz * tz;
    acc.source_square_sum += sx * sx + sy * sy + sz * sz;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline ScaleAccumulationD64
accumulateScaleStdDouble(const pcl::PointCloud<PointSource>& source,
                         const pcl::PointCloud<PointTarget>& target)
{
  ScaleAccumulationD64 acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  for (std::size_t i = 0; i < n; ++i) {
    const double sx = static_cast<double>(source[i].x);
    const double sy = static_cast<double>(source[i].y);
    const double sz = static_cast<double>(source[i].z);
    const double tx = static_cast<double>(target[i].x);
    const double ty = static_cast<double>(target[i].y);
    const double tz = static_cast<double>(target[i].z);
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.source_target_sum[0] += sx * tx;
    acc.source_target_sum[1] += sx * ty;
    acc.source_target_sum[2] += sx * tz;
    acc.source_target_sum[3] += sy * tx;
    acc.source_target_sum[4] += sy * ty;
    acc.source_target_sum[5] += sy * tz;
    acc.source_target_sum[6] += sz * tx;
    acc.source_target_sum[7] += sz * ty;
    acc.source_target_sum[8] += sz * tz;
    acc.source_square_sum += sx * sx + sy * sy + sz * sz;
  }
  return acc;
}

inline ScaleAccumulation
accumulateScaleStdContiguousOffsets(const pcl::PointCloud<pcl::PointXYZ>& source,
                                    const std::size_t source_offset,
                                    const pcl::PointCloud<pcl::PointXYZ>& target,
                                    const std::size_t target_offset,
                                    const std::size_t count)
{
  ScaleAccumulation acc;
  if (source_offset > source.size() || target_offset > target.size())
    return acc;
  const std::size_t source_remaining = source.size() - source_offset;
  const std::size_t target_remaining = target.size() - target_offset;
  const std::size_t n = std::min({count, source_remaining, target_remaining});
  acc.count = n;
  for (std::size_t i = 0; i < n; ++i) {
    const auto& src = source[source_offset + i];
    const auto& tgt = target[target_offset + i];
    const float sx = src.x;
    const float sy = src.y;
    const float sz = src.z;
    const float tx = tgt.x;
    const float ty = tgt.y;
    const float tz = tgt.z;
    acc.source_sum[0] += sx;
    acc.source_sum[1] += sy;
    acc.source_sum[2] += sz;
    acc.target_sum[0] += tx;
    acc.target_sum[1] += ty;
    acc.target_sum[2] += tz;
    acc.source_target_sum[0] += sx * tx;
    acc.source_target_sum[1] += sx * ty;
    acc.source_target_sum[2] += sx * tz;
    acc.source_target_sum[3] += sy * tx;
    acc.source_target_sum[4] += sy * ty;
    acc.source_target_sum[5] += sy * tz;
    acc.source_target_sum[6] += sz * tx;
    acc.source_target_sum[7] += sz * ty;
    acc.source_target_sum[8] += sz * tz;
    acc.source_square_sum += sx * sx + sy * sy + sz * sz;
  }
  return acc;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateScalePublic(const pcl::PointCloud<PointSource>& source,
                    const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, float>
      estimator;
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, matrix);
  return matrix;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateScaleStd(const pcl::PointCloud<PointSource>& source,
                 const pcl::PointCloud<PointTarget>& target,
                 CandidateStats* stats = nullptr)
{
  ScaleAccumulation acc = accumulateScaleStd(source, target);
  return solveScaleFromAccumulation(acc, stats);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4d
estimateScaleStdDouble(const pcl::PointCloud<PointSource>& source,
                       const pcl::PointCloud<PointTarget>& target,
                       CandidateStats* stats = nullptr)
{
  if (stats)
    *stats = CandidateStats{};
  ScaleAccumulationD64 acc = accumulateScaleStdDouble(source, target);
  return solveScaleFromAccumulationDouble(acc, stats);
}

#ifdef __RVV10__
inline vfloat32mf2_t
stridedLoadScaleFieldF32mf2(const std::uint8_t* base,
                            const std::size_t field_offset,
                            const std::ptrdiff_t stride,
                            const std::size_t vl)
{
  return __riscv_vlse32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), stride, vl);
}

inline vfloat64m1_t
widenScaleF64(vfloat32mf2_t value, const std::size_t vl)
{
  return __riscv_vfwcvt_f_f_v_f64m1(value, vl);
}

inline vfloat64m1_t
addScaleF64Term(vfloat64m1_t acc, vfloat64m1_t term, const std::size_t vl)
{
  return __riscv_vfadd_vv_f64m1_tu(acc, acc, term, vl);
}

inline vfloat64m1_t
mulScaleF64(vfloat64m1_t lhs, vfloat64m1_t rhs, const std::size_t vl)
{
  return __riscv_vfmul_vv_f64m1(lhs, rhs, vl);
}

inline double
reduceScaleF64(const vfloat64m1_t value, const std::size_t vlmax)
{
  const vfloat64m1_t zero = __riscv_vfmv_s_f_f64m1(0.0, 1);
  return __riscv_vfmv_f_s_f64m1_f64(
      __riscv_vfredosum_vs_f64m1_f64m1(value, zero, vlmax));
}

inline float
reduceScaleSum(const vfloat32m2_t value, const std::size_t vlmax)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return __riscv_vfmv_f_s_f32m1_f32(
      __riscv_vfredosum_vs_f32m2_f32m1(value, zero, vlmax));
}

inline ScaleAccumulation
accumulateScaleRVV(const pcl::PointCloud<pcl::PointXYZ>& source,
                   const pcl::PointCloud<pcl::PointXYZ>& target)
{
  ScaleAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(
        source_base + i * sizeof(pcl::PointXYZ), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(
        target_base + i * sizeof(pcl::PointXYZ), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    c00 = __riscv_vfmacc_vv_f32m2_tu(c00, sx, tx, vl);
    c01 = __riscv_vfmacc_vv_f32m2_tu(c01, sx, ty, vl);
    c02 = __riscv_vfmacc_vv_f32m2_tu(c02, sx, tz, vl);
    c10 = __riscv_vfmacc_vv_f32m2_tu(c10, sy, tx, vl);
    c11 = __riscv_vfmacc_vv_f32m2_tu(c11, sy, ty, vl);
    c12 = __riscv_vfmacc_vv_f32m2_tu(c12, sy, tz, vl);
    c20 = __riscv_vfmacc_vv_f32m2_tu(c20, sz, tx, vl);
    c21 = __riscv_vfmacc_vv_f32m2_tu(c21, sz, ty, vl);
    c22 = __riscv_vfmacc_vv_f32m2_tu(c22, sz, tz, vl);
    source_square = __riscv_vfmacc_vv_f32m2_tu(source_square, sx, sx, vl);
    source_square = __riscv_vfmacc_vv_f32m2_tu(source_square, sy, sy, vl);
    source_square = __riscv_vfmacc_vv_f32m2_tu(source_square, sz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceScaleSum(ssx, vlmax);
  acc.source_sum[1] = reduceScaleSum(ssy, vlmax);
  acc.source_sum[2] = reduceScaleSum(ssz, vlmax);
  acc.target_sum[0] = reduceScaleSum(stx, vlmax);
  acc.target_sum[1] = reduceScaleSum(sty, vlmax);
  acc.target_sum[2] = reduceScaleSum(stz, vlmax);
  acc.source_target_sum[0] = reduceScaleSum(c00, vlmax);
  acc.source_target_sum[1] = reduceScaleSum(c01, vlmax);
  acc.source_target_sum[2] = reduceScaleSum(c02, vlmax);
  acc.source_target_sum[3] = reduceScaleSum(c10, vlmax);
  acc.source_target_sum[4] = reduceScaleSum(c11, vlmax);
  acc.source_target_sum[5] = reduceScaleSum(c12, vlmax);
  acc.source_target_sum[6] = reduceScaleSum(c20, vlmax);
  acc.source_target_sum[7] = reduceScaleSum(c21, vlmax);
  acc.source_target_sum[8] = reduceScaleSum(c22, vlmax);
  acc.source_square_sum = reduceScaleSum(source_square, vlmax);
  return acc;
}

inline ScaleAccumulationD64
accumulateScaleRVVDouble(const pcl::PointCloud<pcl::PointXYZ>& source,
                         const pcl::PointCloud<pcl::PointXYZ>& target)
{
  ScaleAccumulationD64 acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(n - i);
    const auto* src = source_base + i * sizeof(pcl::PointXYZ);
    const auto* tgt = target_base + i * sizeof(pcl::PointXYZ);
    const vfloat64m1_t sx = widenScaleF64(stridedLoadScaleFieldF32mf2(
                                              src, offsetof(pcl::PointXYZ, x),
                                              sizeof(pcl::PointXYZ), vl),
                                          vl);
    const vfloat64m1_t sy = widenScaleF64(stridedLoadScaleFieldF32mf2(
                                              src, offsetof(pcl::PointXYZ, y),
                                              sizeof(pcl::PointXYZ), vl),
                                          vl);
    const vfloat64m1_t sz = widenScaleF64(stridedLoadScaleFieldF32mf2(
                                              src, offsetof(pcl::PointXYZ, z),
                                              sizeof(pcl::PointXYZ), vl),
                                          vl);
    const vfloat64m1_t tx = widenScaleF64(stridedLoadScaleFieldF32mf2(
                                              tgt, offsetof(pcl::PointXYZ, x),
                                              sizeof(pcl::PointXYZ), vl),
                                          vl);
    const vfloat64m1_t ty = widenScaleF64(stridedLoadScaleFieldF32mf2(
                                              tgt, offsetof(pcl::PointXYZ, y),
                                              sizeof(pcl::PointXYZ), vl),
                                          vl);
    const vfloat64m1_t tz = widenScaleF64(stridedLoadScaleFieldF32mf2(
                                              tgt, offsetof(pcl::PointXYZ, z),
                                              sizeof(pcl::PointXYZ), vl),
                                          vl);

    ssx = addScaleF64Term(ssx, sx, vl);
    ssy = addScaleF64Term(ssy, sy, vl);
    ssz = addScaleF64Term(ssz, sz, vl);
    stx = addScaleF64Term(stx, tx, vl);
    sty = addScaleF64Term(sty, ty, vl);
    stz = addScaleF64Term(stz, tz, vl);
    c00 = addScaleF64Term(c00, mulScaleF64(sx, tx, vl), vl);
    c01 = addScaleF64Term(c01, mulScaleF64(sx, ty, vl), vl);
    c02 = addScaleF64Term(c02, mulScaleF64(sx, tz, vl), vl);
    c10 = addScaleF64Term(c10, mulScaleF64(sy, tx, vl), vl);
    c11 = addScaleF64Term(c11, mulScaleF64(sy, ty, vl), vl);
    c12 = addScaleF64Term(c12, mulScaleF64(sy, tz, vl), vl);
    c20 = addScaleF64Term(c20, mulScaleF64(sz, tx, vl), vl);
    c21 = addScaleF64Term(c21, mulScaleF64(sz, ty, vl), vl);
    c22 = addScaleF64Term(c22, mulScaleF64(sz, tz, vl), vl);
    source_square = addScaleF64Term(source_square, mulScaleF64(sx, sx, vl), vl);
    source_square = addScaleF64Term(source_square, mulScaleF64(sy, sy, vl), vl);
    source_square = addScaleF64Term(source_square, mulScaleF64(sz, sz, vl), vl);
    i += vl;
  }

  acc.source_sum[0] = reduceScaleF64(ssx, vlmax);
  acc.source_sum[1] = reduceScaleF64(ssy, vlmax);
  acc.source_sum[2] = reduceScaleF64(ssz, vlmax);
  acc.target_sum[0] = reduceScaleF64(stx, vlmax);
  acc.target_sum[1] = reduceScaleF64(sty, vlmax);
  acc.target_sum[2] = reduceScaleF64(stz, vlmax);
  acc.source_target_sum[0] = reduceScaleF64(c00, vlmax);
  acc.source_target_sum[1] = reduceScaleF64(c01, vlmax);
  acc.source_target_sum[2] = reduceScaleF64(c02, vlmax);
  acc.source_target_sum[3] = reduceScaleF64(c10, vlmax);
  acc.source_target_sum[4] = reduceScaleF64(c11, vlmax);
  acc.source_target_sum[5] = reduceScaleF64(c12, vlmax);
  acc.source_target_sum[6] = reduceScaleF64(c20, vlmax);
  acc.source_target_sum[7] = reduceScaleF64(c21, vlmax);
  acc.source_target_sum[8] = reduceScaleF64(c22, vlmax);
  acc.source_square_sum = reduceScaleF64(source_square, vlmax);
  return acc;
}

inline ScaleAccumulation
accumulateScaleRVVContiguousOffsets(const pcl::PointCloud<pcl::PointXYZ>& source,
                                    const std::size_t source_offset,
                                    const pcl::PointCloud<pcl::PointXYZ>& target,
                                    const std::size_t target_offset,
                                    const std::size_t count)
{
  ScaleAccumulation acc;
  if (source_offset > source.size() || target_offset > target.size())
    return acc;
  const std::size_t source_remaining = source.size() - source_offset;
  const std::size_t target_remaining = target.size() - target_offset;
  const std::size_t n = std::min({count, source_remaining, target_remaining});
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(
      source.points.data() + source_offset);
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(
      target.points.data() + target_offset);

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(
        source_base + i * sizeof(pcl::PointXYZ), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(
        target_base + i * sizeof(pcl::PointXYZ), vl, tx, ty, tz);
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    c00 = __riscv_vfmacc_vv_f32m2_tu(c00, sx, tx, vl);
    c01 = __riscv_vfmacc_vv_f32m2_tu(c01, sx, ty, vl);
    c02 = __riscv_vfmacc_vv_f32m2_tu(c02, sx, tz, vl);
    c10 = __riscv_vfmacc_vv_f32m2_tu(c10, sy, tx, vl);
    c11 = __riscv_vfmacc_vv_f32m2_tu(c11, sy, ty, vl);
    c12 = __riscv_vfmacc_vv_f32m2_tu(c12, sy, tz, vl);
    c20 = __riscv_vfmacc_vv_f32m2_tu(c20, sz, tx, vl);
    c21 = __riscv_vfmacc_vv_f32m2_tu(c21, sz, ty, vl);
    c22 = __riscv_vfmacc_vv_f32m2_tu(c22, sz, tz, vl);
    source_square = __riscv_vfmacc_vv_f32m2_tu(source_square, sx, sx, vl);
    source_square = __riscv_vfmacc_vv_f32m2_tu(source_square, sy, sy, vl);
    source_square = __riscv_vfmacc_vv_f32m2_tu(source_square, sz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceScaleSum(ssx, vlmax);
  acc.source_sum[1] = reduceScaleSum(ssy, vlmax);
  acc.source_sum[2] = reduceScaleSum(ssz, vlmax);
  acc.target_sum[0] = reduceScaleSum(stx, vlmax);
  acc.target_sum[1] = reduceScaleSum(sty, vlmax);
  acc.target_sum[2] = reduceScaleSum(stz, vlmax);
  acc.source_target_sum[0] = reduceScaleSum(c00, vlmax);
  acc.source_target_sum[1] = reduceScaleSum(c01, vlmax);
  acc.source_target_sum[2] = reduceScaleSum(c02, vlmax);
  acc.source_target_sum[3] = reduceScaleSum(c10, vlmax);
  acc.source_target_sum[4] = reduceScaleSum(c11, vlmax);
  acc.source_target_sum[5] = reduceScaleSum(c12, vlmax);
  acc.source_target_sum[6] = reduceScaleSum(c20, vlmax);
  acc.source_target_sum[7] = reduceScaleSum(c21, vlmax);
  acc.source_target_sum[8] = reduceScaleSum(c22, vlmax);
  acc.source_square_sum = reduceScaleSum(source_square, vlmax);
  return acc;
}
#endif

inline Eigen::Matrix4d
estimateScaleRVVDouble(const pcl::PointCloud<pcl::PointXYZ>& source,
                       const pcl::PointCloud<pcl::PointXYZ>& target,
                       CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = CandidateStats{};
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
  }

  const auto fallback = [&]() {
    CandidateStats solve_stats;
    Eigen::Matrix4d matrix = estimateScaleStdDouble(source, target, &solve_stats);
    if (stats) {
      stats->input_points = solve_stats.input_points;
      stats->accepted_points = solve_stats.accepted_points;
      stats->degenerate_source = solve_stats.degenerate_source;
      stats->used_fallback = true;
    }
    return matrix;
  };

  if (source.size() != target.size() || source.size() < 16 || !source.is_dense ||
      !target.is_dense) {
    return fallback();
  }

#ifdef __RVV10__
  if (stats)
    stats->layout_supported = true;
  CandidateStats solve_stats;
  Eigen::Matrix4d matrix =
      solveScaleFromAccumulationDouble(accumulateScaleRVVDouble(source, target),
                                       &solve_stats);
  if (solve_stats.degenerate_source) {
    if (stats)
      stats->degenerate_source = true;
    return fallback();
  }
  if (stats) {
    stats->used_rvv = true;
    stats->input_points = solve_stats.input_points;
    stats->accepted_points = solve_stats.accepted_points;
  }
  return matrix;
#else
  return fallback();
#endif
}

inline Eigen::Matrix4f
estimateScaleContiguousOffsetCandidate(const pcl::PointCloud<pcl::PointXYZ>& source,
                                       const std::size_t source_offset,
                                       const pcl::PointCloud<pcl::PointXYZ>& target,
                                       const std::size_t target_offset,
                                       const std::size_t count,
                                       CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = CandidateStats{};
    stats->input_points = count;
  }

  const auto fallback = [&]() {
    if (stats)
      stats->used_fallback = true;
    return solveScaleFromAccumulation(
        accumulateScaleStdContiguousOffsets(source, source_offset, target, target_offset, count),
        stats);
  };

  if (!source.is_dense || !target.is_dense || count < 16 ||
      source_offset > source.size() || target_offset > target.size() ||
      count > source.size() - source_offset || count > target.size() - target_offset) {
    return fallback();
  }

#ifdef __RVV10__
  if (stats)
    stats->layout_supported = true;
  CandidateStats solve_stats;
  Eigen::Matrix4f matrix =
      solveScaleFromAccumulation(accumulateScaleRVVContiguousOffsets(
                                     source, source_offset, target, target_offset, count),
                                 &solve_stats);
  if (solve_stats.degenerate_source) {
    if (stats)
      stats->degenerate_source = true;
    return fallback();
  }
  if (stats) {
    stats->used_rvv = true;
    stats->input_points = solve_stats.input_points;
    stats->accepted_points = solve_stats.accepted_points;
  }
  return matrix;
#else
  return fallback();
#endif
}

inline Eigen::Matrix4f
estimateScaleCandidate(const pcl::PointCloud<pcl::PointXYZ>& source,
                       const pcl::PointCloud<pcl::PointXYZ>& target,
                       CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = CandidateStats{};
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
  }

  const auto fallback = [&]() {
    if (stats) {
      stats->used_fallback = true;
    }
    return estimateScaleStd(source, target, stats);
  };

  if (source.size() != target.size() || source.size() < 16 || !source.is_dense ||
      !target.is_dense) {
    return fallback();
  }

#ifdef __RVV10__
  if (stats)
    stats->layout_supported = true;
  ScaleAccumulation acc = accumulateScaleRVV(source, target);
  CandidateStats solve_stats;
  Eigen::Matrix4f matrix = solveScaleFromAccumulation(acc, &solve_stats);
  if (solve_stats.degenerate_source) {
    if (stats)
      stats->degenerate_source = true;
    return fallback();
  }
  if (stats) {
    stats->used_rvv = true;
    stats->input_points = solve_stats.input_points;
    stats->accepted_points = solve_stats.accepted_points;
  }
  return matrix;
#else
  return fallback();
#endif
}

} // namespace pcl::registration::rvv_tesvd_scale_support
