/*
 * 本文件做什么：
 * 这里保存 transformation_estimation_dual_quaternion 的 test-only reference
 * （测试专用参考链路）和 RVV candidate（RVV 候选链路）。candidate 只替换
 * ordered-cloud-pair（顺序点云对，source / target 按同一下标一一对应）路径里的
 * dual-quaternion C1 / C2 前置累加；4x4 SelfAdjointEigenSolver（自伴随特征求解器）
 * 和 quaternion（四元数）到矩阵的后段仍使用 Eigen 标量代码。
 *
 * 证据边界：
 * 当前 helper 不修改 production 源码，不证明真实 public entry（公开入口）已经命中 RVV。
 * indices（索引）、dual indices（双索引）和 correspondences（对应关系）仍是后续 row source
 * policy（行来源策略）评估对象。RVV 路径只覆盖标准布局、float x/y/z 字段、稠密 ordered
 * 点云对；其它情况回退到同构标量 reference。
 */

#pragma once

#include <pcl/common/eigen.h>
#include <pcl/correspondence.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/transformation_estimation_dual_quaternion.h>
#include <pcl/types.h>

#include "tedq_adapters.hpp"

#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_tedq_support {

constexpr std::size_t kRvvMinPoints = 32;

struct CandidateStats {
  bool used_rvv{false};
  bool used_fallback{false};
  bool used_gather{false};
  bool layout_supported{false};
  bool used_staging{false};
  bool used_correspondence_index_stream{false};
  bool used_correspondence_segment_stream{false};
  std::size_t input_points{0};
  std::size_t accepted_points{0};
};

struct DualQuaternionAccumulation {
  double c1[16]{0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0};
  double c2[16]{0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0};
  std::size_t count{0};
};

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicDualQuaternion(const pcl::PointCloud<PointSource>& source,
                              const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimationDualQuaternion<PointSource, PointTarget, float>
      estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicSourceIndexedDualQuaternion(const pcl::PointCloud<PointSource>& source,
                                          const pcl::Indices& indices,
                                          const pcl::PointCloud<PointTarget>& target)
{
  pcl::registration::TransformationEstimationDualQuaternion<PointSource, PointTarget, float>
      estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, indices, target, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicDualIndexedDualQuaternion(const pcl::PointCloud<PointSource>& source,
                                        const pcl::Indices& source_indices,
                                        const pcl::PointCloud<PointTarget>& target,
                                        const pcl::Indices& target_indices)
{
  pcl::registration::TransformationEstimationDualQuaternion<PointSource, PointTarget, float>
      estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, transform);
  return transform;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimatePublicCorrespondenceDualQuaternion(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  pcl::registration::TransformationEstimationDualQuaternion<PointSource, PointTarget, float>
      estimator;
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, transform);
  return transform;
}

inline Eigen::Matrix4f
finishDualQuaternionEstimate(const DualQuaternionAccumulation& acc)
{
  Eigen::Matrix4f transformation_matrix = Eigen::Matrix4f::Identity();
  if (acc.count == 0)
    return transformation_matrix;

  Eigen::Matrix<double, 4, 4> C1 = Eigen::Matrix<double, 4, 4>::Zero();
  Eigen::Matrix<double, 4, 4> C2 = Eigen::Matrix<double, 4, 4>::Zero();
  double* c1 = C1.data();
  double* c2 = C2.data();
  std::copy(std::begin(acc.c1), std::end(acc.c1), c1);
  std::copy(std::begin(acc.c2), std::end(acc.c2), c2);

  c1[4] = c1[1];
  c1[8] = c1[2];
  c1[9] = c1[6];
  c1[12] = c1[3];
  c1[13] = c1[7];
  c1[14] = c1[11];
  c2[4] = -c2[1];
  c2[8] = -c2[2];
  c2[12] = -c2[3];
  c2[9] = -c2[6];
  c2[13] = -c2[7];
  c2[14] = -c2[11];

  C1 *= -2.0;
  C2 *= 2.0;

  const Eigen::Matrix<double, 4, 4> A =
      (0.25 / static_cast<double>(acc.count)) * C2.transpose() * C2 - C1;
  const Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 4, 4>> es(A);

  ptrdiff_t i = 0;
  es.eigenvalues().maxCoeff(&i);
  const Eigen::Matrix<double, 4, 1> qmat = es.eigenvectors().col(i);
  const Eigen::Matrix<double, 4, 1> smat =
      -(0.5 / static_cast<double>(acc.count)) * C2 * qmat;

  const Eigen::Quaternion<double> q(qmat(3), qmat(0), qmat(1), qmat(2));
  const Eigen::Quaternion<double> s(smat(3), smat(0), smat(1), smat(2));
  const Eigen::Quaternion<double> t = s * q.conjugate();
  const Eigen::Matrix<double, 3, 3> R(q.toRotationMatrix());

  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col)
      transformation_matrix(row, col) = static_cast<float>(R(row, col));
  }
  transformation_matrix(0, 3) = static_cast<float>(-t.x());
  transformation_matrix(1, 3) = static_cast<float>(-t.y());
  transformation_matrix(2, 3) = static_cast<float>(-t.z());
  return transformation_matrix;
}

template <typename PointSource, typename PointTarget>
inline DualQuaternionAccumulation
accumulateDualQuaternionStd(const pcl::PointCloud<PointSource>& source,
                             const pcl::PointCloud<PointTarget>& target)
{
  DualQuaternionAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  for (std::size_t i = 0; i < n; ++i) {
    const PointSource& a = source[i];
    const PointTarget& b = target[i];
    const double axbx = a.x * b.x;
    const double ayby = a.y * b.y;
    const double azbz = a.z * b.z;
    const double axby = a.x * b.y;
    const double aybx = a.y * b.x;
    const double axbz = a.x * b.z;
    const double azbx = a.z * b.x;
    const double aybz = a.y * b.z;
    const double azby = a.z * b.y;
    acc.c1[0] += axbx - azbz - ayby;
    acc.c1[5] += ayby - azbz - axbx;
    acc.c1[10] += azbz - axbx - ayby;
    acc.c1[15] += axbx + ayby + azbz;
    acc.c1[1] += axby + aybx;
    acc.c1[2] += axbz + azbx;
    acc.c1[3] += aybz - azby;
    acc.c1[6] += azby + aybz;
    acc.c1[7] += azbx - axbz;
    acc.c1[11] += axby - aybx;
    acc.c2[1] += a.z + b.z;
    acc.c2[2] -= a.y + b.y;
    acc.c2[3] += a.x - b.x;
    acc.c2[6] += a.x + b.x;
    acc.c2[7] += a.y - b.y;
    acc.c2[11] += a.z - b.z;
  }
  return acc;
}

#ifdef __RVV10__
inline vfloat32mf2_t
stridedLoadFieldF32mf2(const std::uint8_t* base,
                       const std::size_t field_offset,
                       const std::ptrdiff_t stride,
                       const std::size_t vl)
{
  return __riscv_vlse32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), stride, vl);
}

template <typename PointT>
inline vfloat32mf2_t
indexedLoadFieldF32mf2(const std::uint8_t* base,
                       const std::size_t field_offset,
                       vuint32mf2_t byte_offsets,
                       const std::size_t vl)
{
  static_assert(std::is_standard_layout<PointT>::value,
                "indexed TEDQ gather requires standard-layout point storage");
  return __riscv_vluxei32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), byte_offsets, vl);
}

struct DualIndexedVectorLoader {
  const std::uint32_t* source_indices;
  const std::uint32_t* target_indices;

  inline void
  load(const std::size_t i,
       const std::size_t vl,
       vuint32mf2_t& source,
       vuint32mf2_t& target) const
  {
    source = __riscv_vle32_v_u32mf2(source_indices + i, vl);
    target = __riscv_vle32_v_u32mf2(target_indices + i, vl);
  }
};

struct CorrespondenceIndexStreamLoader {
  const std::uint8_t* correspondences;

  inline void
  load(const std::size_t i,
       const std::size_t vl,
       vuint32mf2_t& source,
       vuint32mf2_t& target) const
  {
    static_assert(sizeof(pcl::index_t) == sizeof(std::uint32_t),
                  "RVV correspondence index stream requires 32-bit index_t");
    const auto* base =
        correspondences + i * sizeof(pcl::Correspondence);
    source = __riscv_vlse32_v_u32mf2(
        reinterpret_cast<const std::uint32_t*>(
            base + offsetof(pcl::Correspondence, index_query)),
        static_cast<std::ptrdiff_t>(sizeof(pcl::Correspondence)),
        vl);
    target = __riscv_vlse32_v_u32mf2(
        reinterpret_cast<const std::uint32_t*>(
            base + offsetof(pcl::Correspondence, index_match)),
        static_cast<std::ptrdiff_t>(sizeof(pcl::Correspondence)),
        vl);
  }
};

struct CorrespondenceSegmentIndexStreamLoader {
  const std::uint8_t* correspondences;

  inline void
  load(const std::size_t i,
       const std::size_t vl,
       vuint32mf2_t& source,
       vuint32mf2_t& target) const
  {
    static_assert(sizeof(pcl::index_t) == sizeof(std::uint32_t),
                  "RVV correspondence segment stream requires 32-bit index_t");
    static_assert(sizeof(pcl::Correspondence) == 3 * sizeof(std::uint32_t),
                  "RVV correspondence segment stream requires three packed 32-bit fields");
    const auto* base =
        correspondences + i * sizeof(pcl::Correspondence);
    const vuint32mf2x3_t fields = __riscv_vlseg3e32_v_u32mf2x3(
        reinterpret_cast<const std::uint32_t*>(
            base + offsetof(pcl::Correspondence, index_query)),
        vl);
    source = __riscv_vget_v_u32mf2x3_u32mf2(fields, 0);
    target = __riscv_vget_v_u32mf2x3_u32mf2(fields, 1);
  }
};

inline vfloat64m1_t
widen(vfloat32mf2_t value, const std::size_t vl)
{
  return __riscv_vfwcvt_f_f_v_f64m1(value, vl);
}

inline vfloat64m1_t
widenMul(vfloat32mf2_t lhs, vfloat32mf2_t rhs, const std::size_t vl)
{
  return widen(__riscv_vfmul_vv_f32mf2(lhs, rhs, vl), vl);
}

inline vfloat64m1_t
widenAdd(vfloat32mf2_t lhs, vfloat32mf2_t rhs, const std::size_t vl)
{
  return widen(__riscv_vfadd_vv_f32mf2(lhs, rhs, vl), vl);
}

inline vfloat64m1_t
widenSub(vfloat32mf2_t lhs, vfloat32mf2_t rhs, const std::size_t vl)
{
  return widen(__riscv_vfsub_vv_f32mf2(lhs, rhs, vl), vl);
}

inline vfloat64m1_t
addTerm(vfloat64m1_t acc, vfloat64m1_t term, const std::size_t vl)
{
  return __riscv_vfadd_vv_f64m1_tu(acc, acc, term, vl);
}

inline double
reduceF64(vfloat64m1_t value, const std::size_t vlmax)
{
  const vfloat64m1_t zero = __riscv_vfmv_s_f_f64m1(0.0, 1);
  return __riscv_vfmv_f_s_f64m1_f64(
      __riscv_vfredosum_vs_f64m1_f64m1(value, zero, vlmax));
}

template <typename PointSource, typename PointTarget>
inline DualQuaternionAccumulation
accumulateDualQuaternionRVVImpl(const pcl::PointCloud<PointSource>& source,
                                const pcl::PointCloud<PointTarget>& target)
{
  DualQuaternionAccumulation acc;
  const std::size_t n = std::min(source.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t c100 = zero, c105 = zero, c110 = zero, c115 = zero;
  vfloat64m1_t c101 = zero, c102 = zero, c103 = zero, c106 = zero;
  vfloat64m1_t c107 = zero, c111 = zero;
  vfloat64m1_t c201 = zero, c202 = zero, c203 = zero;
  vfloat64m1_t c206 = zero, c207 = zero, c211 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(n - i);
    const auto* src = source_base + i * sizeof(PointSource);
    const auto* tgt = target_base + i * sizeof(PointTarget);
    const vfloat32mf2_t ax =
        stridedLoadFieldF32mf2(src, offsetof(PointSource, x), sizeof(PointSource), vl);
    const vfloat32mf2_t ay =
        stridedLoadFieldF32mf2(src, offsetof(PointSource, y), sizeof(PointSource), vl);
    const vfloat32mf2_t az =
        stridedLoadFieldF32mf2(src, offsetof(PointSource, z), sizeof(PointSource), vl);
    const vfloat32mf2_t bx =
        stridedLoadFieldF32mf2(tgt, offsetof(PointTarget, x), sizeof(PointTarget), vl);
    const vfloat32mf2_t by =
        stridedLoadFieldF32mf2(tgt, offsetof(PointTarget, y), sizeof(PointTarget), vl);
    const vfloat32mf2_t bz =
        stridedLoadFieldF32mf2(tgt, offsetof(PointTarget, z), sizeof(PointTarget), vl);

    const vfloat64m1_t axbx = widenMul(ax, bx, vl);
    const vfloat64m1_t ayby = widenMul(ay, by, vl);
    const vfloat64m1_t azbz = widenMul(az, bz, vl);
    const vfloat64m1_t axby = widenMul(ax, by, vl);
    const vfloat64m1_t aybx = widenMul(ay, bx, vl);
    const vfloat64m1_t axbz = widenMul(ax, bz, vl);
    const vfloat64m1_t azbx = widenMul(az, bx, vl);
    const vfloat64m1_t aybz = widenMul(ay, bz, vl);
    const vfloat64m1_t azby = widenMul(az, by, vl);

    c100 = addTerm(
        c100,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(axbx, azbz, vl), ayby, vl),
        vl);
    c105 = addTerm(
        c105,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(ayby, azbz, vl), axbx, vl),
        vl);
    c110 = addTerm(
        c110,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(azbz, axbx, vl), ayby, vl),
        vl);
    c115 = addTerm(
        c115,
        __riscv_vfadd_vv_f64m1(__riscv_vfadd_vv_f64m1(axbx, ayby, vl), azbz, vl),
        vl);
    c101 = addTerm(c101, __riscv_vfadd_vv_f64m1(axby, aybx, vl), vl);
    c102 = addTerm(c102, __riscv_vfadd_vv_f64m1(axbz, azbx, vl), vl);
    c103 = addTerm(c103, __riscv_vfsub_vv_f64m1(aybz, azby, vl), vl);
    c106 = addTerm(c106, __riscv_vfadd_vv_f64m1(azby, aybz, vl), vl);
    c107 = addTerm(c107, __riscv_vfsub_vv_f64m1(azbx, axbz, vl), vl);
    c111 = addTerm(c111, __riscv_vfsub_vv_f64m1(axby, aybx, vl), vl);
    c201 = addTerm(c201, widenAdd(az, bz, vl), vl);
    c202 = addTerm(c202, __riscv_vfneg_v_f64m1(widenAdd(ay, by, vl), vl), vl);
    c203 = addTerm(c203, widenSub(ax, bx, vl), vl);
    c206 = addTerm(c206, widenAdd(ax, bx, vl), vl);
    c207 = addTerm(c207, widenSub(ay, by, vl), vl);
    c211 = addTerm(c211, widenSub(az, bz, vl), vl);
    i += vl;
  }

  acc.c1[0] = reduceF64(c100, vlmax);
  acc.c1[5] = reduceF64(c105, vlmax);
  acc.c1[10] = reduceF64(c110, vlmax);
  acc.c1[15] = reduceF64(c115, vlmax);
  acc.c1[1] = reduceF64(c101, vlmax);
  acc.c1[2] = reduceF64(c102, vlmax);
  acc.c1[3] = reduceF64(c103, vlmax);
  acc.c1[6] = reduceF64(c106, vlmax);
  acc.c1[7] = reduceF64(c107, vlmax);
  acc.c1[11] = reduceF64(c111, vlmax);
  acc.c2[1] = reduceF64(c201, vlmax);
  acc.c2[2] = reduceF64(c202, vlmax);
  acc.c2[3] = reduceF64(c203, vlmax);
  acc.c2[6] = reduceF64(c206, vlmax);
  acc.c2[7] = reduceF64(c207, vlmax);
  acc.c2[11] = reduceF64(c211, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline DualQuaternionAccumulation
accumulateDualQuaternionRVVSourceIndexedImpl(const pcl::PointCloud<PointSource>& source,
                                             const pcl::Indices& indices,
                                             const pcl::PointCloud<PointTarget>& target)
{
  DualQuaternionAccumulation acc;
  const std::size_t n = std::min(indices.size(), target.size());
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t c100 = zero, c105 = zero, c110 = zero, c115 = zero;
  vfloat64m1_t c101 = zero, c102 = zero, c103 = zero, c106 = zero;
  vfloat64m1_t c107 = zero, c111 = zero;
  vfloat64m1_t c201 = zero, c202 = zero, c203 = zero;
  vfloat64m1_t c206 = zero, c207 = zero, c211 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());
  const auto* index_base = reinterpret_cast<const std::uint32_t*>(indices.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(n - i);
    const auto* tgt = target_base + i * sizeof(PointTarget);
    const vuint32mf2_t v_index = __riscv_vle32_v_u32mf2(index_base + i, vl);
    const vuint32mf2_t v_byte_offsets =
        __riscv_vmul_vx_u32mf2(v_index, static_cast<std::uint32_t>(sizeof(PointSource)), vl);
    const vfloat32mf2_t ax =
        indexedLoadFieldF32mf2<PointSource>(
            source_base, offsetof(PointSource, x), v_byte_offsets, vl);
    const vfloat32mf2_t ay =
        indexedLoadFieldF32mf2<PointSource>(
            source_base, offsetof(PointSource, y), v_byte_offsets, vl);
    const vfloat32mf2_t az =
        indexedLoadFieldF32mf2<PointSource>(
            source_base, offsetof(PointSource, z), v_byte_offsets, vl);
    const vfloat32mf2_t bx =
        stridedLoadFieldF32mf2(
            tgt, offsetof(PointTarget, x), sizeof(PointTarget), vl);
    const vfloat32mf2_t by =
        stridedLoadFieldF32mf2(
            tgt, offsetof(PointTarget, y), sizeof(PointTarget), vl);
    const vfloat32mf2_t bz =
        stridedLoadFieldF32mf2(
            tgt, offsetof(PointTarget, z), sizeof(PointTarget), vl);

    const vfloat64m1_t axbx = widenMul(ax, bx, vl);
    const vfloat64m1_t ayby = widenMul(ay, by, vl);
    const vfloat64m1_t azbz = widenMul(az, bz, vl);
    const vfloat64m1_t axby = widenMul(ax, by, vl);
    const vfloat64m1_t aybx = widenMul(ay, bx, vl);
    const vfloat64m1_t axbz = widenMul(ax, bz, vl);
    const vfloat64m1_t azbx = widenMul(az, bx, vl);
    const vfloat64m1_t aybz = widenMul(ay, bz, vl);
    const vfloat64m1_t azby = widenMul(az, by, vl);

    c100 = addTerm(
        c100,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(axbx, azbz, vl), ayby, vl),
        vl);
    c105 = addTerm(
        c105,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(ayby, azbz, vl), axbx, vl),
        vl);
    c110 = addTerm(
        c110,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(azbz, axbx, vl), ayby, vl),
        vl);
    c115 = addTerm(
        c115,
        __riscv_vfadd_vv_f64m1(__riscv_vfadd_vv_f64m1(axbx, ayby, vl), azbz, vl),
        vl);
    c101 = addTerm(c101, __riscv_vfadd_vv_f64m1(axby, aybx, vl), vl);
    c102 = addTerm(c102, __riscv_vfadd_vv_f64m1(axbz, azbx, vl), vl);
    c103 = addTerm(c103, __riscv_vfsub_vv_f64m1(aybz, azby, vl), vl);
    c106 = addTerm(c106, __riscv_vfadd_vv_f64m1(azby, aybz, vl), vl);
    c107 = addTerm(c107, __riscv_vfsub_vv_f64m1(azbx, axbz, vl), vl);
    c111 = addTerm(c111, __riscv_vfsub_vv_f64m1(axby, aybx, vl), vl);
    c201 = addTerm(c201, widenAdd(az, bz, vl), vl);
    c202 = addTerm(c202, __riscv_vfneg_v_f64m1(widenAdd(ay, by, vl), vl), vl);
    c203 = addTerm(c203, widenSub(ax, bx, vl), vl);
    c206 = addTerm(c206, widenAdd(ax, bx, vl), vl);
    c207 = addTerm(c207, widenSub(ay, by, vl), vl);
    c211 = addTerm(c211, widenSub(az, bz, vl), vl);
    i += vl;
  }

  acc.c1[0] = reduceF64(c100, vlmax);
  acc.c1[5] = reduceF64(c105, vlmax);
  acc.c1[10] = reduceF64(c110, vlmax);
  acc.c1[15] = reduceF64(c115, vlmax);
  acc.c1[1] = reduceF64(c101, vlmax);
  acc.c1[2] = reduceF64(c102, vlmax);
  acc.c1[3] = reduceF64(c103, vlmax);
  acc.c1[6] = reduceF64(c106, vlmax);
  acc.c1[7] = reduceF64(c107, vlmax);
  acc.c1[11] = reduceF64(c111, vlmax);
  acc.c2[1] = reduceF64(c201, vlmax);
  acc.c2[2] = reduceF64(c202, vlmax);
  acc.c2[3] = reduceF64(c203, vlmax);
  acc.c2[6] = reduceF64(c206, vlmax);
  acc.c2[7] = reduceF64(c207, vlmax);
  acc.c2[11] = reduceF64(c211, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget, typename IndexLoader>
inline DualQuaternionAccumulation
accumulateDualQuaternionRVVIndexedImpl(const pcl::PointCloud<PointSource>& source,
                                       const pcl::PointCloud<PointTarget>& target,
                                       const std::size_t n,
                                       const IndexLoader& index_loader)
{
  DualQuaternionAccumulation acc;
  acc.count = n;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t c100 = zero, c105 = zero, c110 = zero, c115 = zero;
  vfloat64m1_t c101 = zero, c102 = zero, c103 = zero, c106 = zero;
  vfloat64m1_t c107 = zero, c111 = zero;
  vfloat64m1_t c201 = zero, c202 = zero, c203 = zero;
  vfloat64m1_t c206 = zero, c207 = zero, c211 = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(source.data());
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.data());

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(n - i);
    vuint32mf2_t source_index;
    vuint32mf2_t target_index;
    index_loader.load(i, vl, source_index, target_index);
    const vuint32mf2_t source_byte_offsets =
        __riscv_vmul_vx_u32mf2(
            source_index, static_cast<std::uint32_t>(sizeof(PointSource)), vl);
    const vuint32mf2_t target_byte_offsets =
        __riscv_vmul_vx_u32mf2(
            target_index, static_cast<std::uint32_t>(sizeof(PointTarget)), vl);
    const vfloat32mf2_t ax =
        indexedLoadFieldF32mf2<PointSource>(
            source_base, offsetof(PointSource, x), source_byte_offsets, vl);
    const vfloat32mf2_t ay =
        indexedLoadFieldF32mf2<PointSource>(
            source_base, offsetof(PointSource, y), source_byte_offsets, vl);
    const vfloat32mf2_t az =
        indexedLoadFieldF32mf2<PointSource>(
            source_base, offsetof(PointSource, z), source_byte_offsets, vl);
    const vfloat32mf2_t bx =
        indexedLoadFieldF32mf2<PointTarget>(
            target_base, offsetof(PointTarget, x), target_byte_offsets, vl);
    const vfloat32mf2_t by =
        indexedLoadFieldF32mf2<PointTarget>(
            target_base, offsetof(PointTarget, y), target_byte_offsets, vl);
    const vfloat32mf2_t bz =
        indexedLoadFieldF32mf2<PointTarget>(
            target_base, offsetof(PointTarget, z), target_byte_offsets, vl);

    const vfloat64m1_t axbx = widenMul(ax, bx, vl);
    const vfloat64m1_t ayby = widenMul(ay, by, vl);
    const vfloat64m1_t azbz = widenMul(az, bz, vl);
    const vfloat64m1_t axby = widenMul(ax, by, vl);
    const vfloat64m1_t aybx = widenMul(ay, bx, vl);
    const vfloat64m1_t axbz = widenMul(ax, bz, vl);
    const vfloat64m1_t azbx = widenMul(az, bx, vl);
    const vfloat64m1_t aybz = widenMul(ay, bz, vl);
    const vfloat64m1_t azby = widenMul(az, by, vl);

    c100 = addTerm(
        c100,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(axbx, azbz, vl), ayby, vl),
        vl);
    c105 = addTerm(
        c105,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(ayby, azbz, vl), axbx, vl),
        vl);
    c110 = addTerm(
        c110,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(azbz, axbx, vl), ayby, vl),
        vl);
    c115 = addTerm(
        c115,
        __riscv_vfadd_vv_f64m1(__riscv_vfadd_vv_f64m1(axbx, ayby, vl), azbz, vl),
        vl);
    c101 = addTerm(c101, __riscv_vfadd_vv_f64m1(axby, aybx, vl), vl);
    c102 = addTerm(c102, __riscv_vfadd_vv_f64m1(axbz, azbx, vl), vl);
    c103 = addTerm(c103, __riscv_vfsub_vv_f64m1(aybz, azby, vl), vl);
    c106 = addTerm(c106, __riscv_vfadd_vv_f64m1(azby, aybz, vl), vl);
    c107 = addTerm(c107, __riscv_vfsub_vv_f64m1(azbx, axbz, vl), vl);
    c111 = addTerm(c111, __riscv_vfsub_vv_f64m1(axby, aybx, vl), vl);
    c201 = addTerm(c201, widenAdd(az, bz, vl), vl);
    c202 = addTerm(c202, __riscv_vfneg_v_f64m1(widenAdd(ay, by, vl), vl), vl);
    c203 = addTerm(c203, widenSub(ax, bx, vl), vl);
    c206 = addTerm(c206, widenAdd(ax, bx, vl), vl);
    c207 = addTerm(c207, widenSub(ay, by, vl), vl);
    c211 = addTerm(c211, widenSub(az, bz, vl), vl);
    i += vl;
  }

  acc.c1[0] = reduceF64(c100, vlmax);
  acc.c1[5] = reduceF64(c105, vlmax);
  acc.c1[10] = reduceF64(c110, vlmax);
  acc.c1[15] = reduceF64(c115, vlmax);
  acc.c1[1] = reduceF64(c101, vlmax);
  acc.c1[2] = reduceF64(c102, vlmax);
  acc.c1[3] = reduceF64(c103, vlmax);
  acc.c1[6] = reduceF64(c106, vlmax);
  acc.c1[7] = reduceF64(c107, vlmax);
  acc.c1[11] = reduceF64(c111, vlmax);
  acc.c2[1] = reduceF64(c201, vlmax);
  acc.c2[2] = reduceF64(c202, vlmax);
  acc.c2[3] = reduceF64(c203, vlmax);
  acc.c2[6] = reduceF64(c206, vlmax);
  acc.c2[7] = reduceF64(c207, vlmax);
  acc.c2[11] = reduceF64(c211, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline DualQuaternionAccumulation
accumulateDualQuaternionRVVDualIndexedImpl(const pcl::PointCloud<PointSource>& source,
                                           const pcl::Indices& source_indices,
                                           const pcl::PointCloud<PointTarget>& target,
                                           const pcl::Indices& target_indices)
{
  const auto* source_index_base =
      reinterpret_cast<const std::uint32_t*>(source_indices.data());
  const auto* target_index_base =
      reinterpret_cast<const std::uint32_t*>(target_indices.data());
  return accumulateDualQuaternionRVVIndexedImpl(
      source,
      target,
      std::min(source_indices.size(), target_indices.size()),
      DualIndexedVectorLoader{source_index_base, target_index_base});
}
#endif // __RVV10__

template <typename PointSource, typename PointTarget>
inline DualQuaternionAccumulation
accumulateDualQuaternionCandidate(const pcl::PointCloud<PointSource>& source,
                                   const pcl::PointCloud<PointTarget>& target,
                                   CandidateStats* stats = nullptr)
{
  CandidateStats local_stats;
  local_stats.input_points = std::min(source.size(), target.size());
  local_stats.accepted_points = local_stats.input_points;
  local_stats.layout_supported =
      std::is_standard_layout<PointSource>::value && std::is_standard_layout<PointTarget>::value;

#ifdef __RVV10__
  if constexpr (std::is_standard_layout<PointSource>::value &&
                std::is_standard_layout<PointTarget>::value) {
    if (local_stats.input_points >= kRvvMinPoints) {
      local_stats.used_rvv = true;
      if (stats)
        *stats = local_stats;
      return accumulateDualQuaternionRVVImpl(source, target);
    }
  }
#endif

  local_stats.used_fallback = true;
  if (stats)
    *stats = local_stats;
  return accumulateDualQuaternionStd(source, target);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionStd(const pcl::PointCloud<PointSource>& source,
                           const pcl::PointCloud<PointTarget>& target,
                           CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = std::min(source.size(), target.size());
    stats->accepted_points = stats->input_points;
    stats->layout_supported =
        std::is_standard_layout<PointSource>::value &&
        std::is_standard_layout<PointTarget>::value;
  }
  return finishDualQuaternionEstimate(accumulateDualQuaternionStd(source, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionCandidate(const pcl::PointCloud<PointSource>& source,
                                 const pcl::PointCloud<PointTarget>& target,
                                 CandidateStats* stats = nullptr)
{
  return finishDualQuaternionEstimate(
      accumulateDualQuaternionCandidate(source, target, stats));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionSourceIndexedStd(const pcl::PointCloud<PointSource>& source,
                                       const pcl::Indices& indices,
                                       const pcl::PointCloud<PointTarget>& target,
                                       CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = indices.size();
    stats->used_staging = true;
    stats->used_fallback = true;
  }
  if (indices.size() != target.size() || !indicesInRange(source, indices))
    return Eigen::Matrix4f::Identity();
  const auto staged_source = materializeByIndices(source, indices);
  if (stats) {
    stats->accepted_points = staged_source.size();
    stats->layout_supported =
        std::is_standard_layout<PointSource>::value &&
        std::is_standard_layout<PointTarget>::value;
  }
  return finishDualQuaternionEstimate(accumulateDualQuaternionStd(staged_source, target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionSourceIndexedCandidate(const pcl::PointCloud<PointSource>& source,
                                             const pcl::Indices& indices,
                                             const pcl::PointCloud<PointTarget>& target,
                                             CandidateStats* stats = nullptr)
{
  if (indices.size() != target.size() || !indicesInRange(source, indices)) {
    if (stats) {
      *stats = {};
      stats->input_points = indices.size();
      stats->used_staging = true;
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }
  const auto staged_source = materializeByIndices(source, indices);
  CandidateStats local_stats;
  const Eigen::Matrix4f matrix =
      estimateDualQuaternionCandidate(staged_source, target, &local_stats);
  local_stats.used_staging = true;
  if (stats)
    *stats = local_stats;
  return matrix;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionSourceIndexedDirectCandidate(const pcl::PointCloud<PointSource>& source,
                                                   const pcl::Indices& indices,
                                                   const pcl::PointCloud<PointTarget>& target,
                                                   CandidateStats* stats = nullptr)
{
  if (indices.size() != target.size() || !indicesInRange(source, indices)) {
    if (stats) {
      *stats = {};
      stats->input_points = indices.size();
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }

  if (stats) {
    *stats = {};
    stats->input_points = indices.size();
    stats->accepted_points = indices.size();
    stats->layout_supported =
        std::is_standard_layout<PointSource>::value &&
        std::is_standard_layout<PointTarget>::value;
  }

#ifdef __RVV10__
  constexpr std::uint64_t kMaxU32ByteOffset =
      std::numeric_limits<std::uint32_t>::max();
  const bool source_offsets_fit =
      source.size() <= kMaxU32ByteOffset / sizeof(PointSource);
  if constexpr (std::is_standard_layout<PointSource>::value &&
                std::is_standard_layout<PointTarget>::value) {
    if (indices.size() >= kRvvMinPoints && source_offsets_fit) {
      if (stats) {
        stats->used_rvv = true;
        stats->used_gather = true;
      }
      return finishDualQuaternionEstimate(
          accumulateDualQuaternionRVVSourceIndexedImpl(source, indices, target));
    }
  }
#endif

  if (stats) {
    stats->used_fallback = true;
  }
  return estimateDualQuaternionSourceIndexedStd(source, indices, target, stats);
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionDualIndexedStd(const pcl::PointCloud<PointSource>& source,
                                     const pcl::Indices& source_indices,
                                     const pcl::PointCloud<PointTarget>& target,
                                     const pcl::Indices& target_indices,
                                     CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = std::min(source_indices.size(), target_indices.size());
    stats->used_staging = true;
    stats->used_fallback = true;
  }
  if (source_indices.size() != target_indices.size() ||
      !indicesInRange(source, source_indices) || !indicesInRange(target, target_indices))
    return Eigen::Matrix4f::Identity();
  const auto staged_source = materializeByIndices(source, source_indices);
  const auto staged_target = materializeByIndices(target, target_indices);
  if (stats) {
    stats->accepted_points = staged_source.size();
    stats->layout_supported =
        std::is_standard_layout<PointSource>::value &&
        std::is_standard_layout<PointTarget>::value;
  }
  return finishDualQuaternionEstimate(
      accumulateDualQuaternionStd(staged_source, staged_target));
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionDualIndexedCandidate(const pcl::PointCloud<PointSource>& source,
                                           const pcl::Indices& source_indices,
                                           const pcl::PointCloud<PointTarget>& target,
                                           const pcl::Indices& target_indices,
                                           CandidateStats* stats = nullptr)
{
  if (source_indices.size() != target_indices.size() ||
      !indicesInRange(source, source_indices) || !indicesInRange(target, target_indices)) {
    if (stats) {
      *stats = {};
      stats->input_points = std::min(source_indices.size(), target_indices.size());
      stats->used_staging = true;
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }
  const auto staged_source = materializeByIndices(source, source_indices);
  const auto staged_target = materializeByIndices(target, target_indices);
  CandidateStats local_stats;
  const Eigen::Matrix4f matrix =
      estimateDualQuaternionCandidate(staged_source, staged_target, &local_stats);
  local_stats.used_staging = true;
  if (stats)
    *stats = local_stats;
  return matrix;
}

template <typename PointSource, typename PointTarget>
inline Eigen::Matrix4f
estimateDualQuaternionDualIndexedDirectCandidate(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& target_indices,
    CandidateStats* stats = nullptr)
{
  if (source_indices.size() != target_indices.size() ||
      !indicesInRange(source, source_indices) || !indicesInRange(target, target_indices)) {
    if (stats) {
      *stats = {};
      stats->input_points = std::min(source_indices.size(), target_indices.size());
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }

  if (stats) {
    *stats = {};
    stats->input_points = source_indices.size();
    stats->accepted_points = source_indices.size();
    stats->layout_supported =
        std::is_standard_layout<PointSource>::value &&
        std::is_standard_layout<PointTarget>::value;
  }

#ifdef __RVV10__
  constexpr std::uint64_t kMaxU32ByteOffset =
      std::numeric_limits<std::uint32_t>::max();
  const bool source_offsets_fit =
      source.size() <= kMaxU32ByteOffset / sizeof(PointSource);
  const bool target_offsets_fit =
      target.size() <= kMaxU32ByteOffset / sizeof(PointTarget);
  if constexpr (std::is_standard_layout<PointSource>::value &&
                std::is_standard_layout<PointTarget>::value) {
    if (source_indices.size() >= kRvvMinPoints && source_offsets_fit &&
        target_offsets_fit) {
      if (stats) {
        stats->used_rvv = true;
        stats->used_gather = true;
      }
      return finishDualQuaternionEstimate(
          accumulateDualQuaternionRVVDualIndexedImpl(
              source, source_indices, target, target_indices));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateDualQuaternionDualIndexedStd(
      source, source_indices, target, target_indices, stats);
}

template <typename PointT>
inline Eigen::Matrix4f
estimateDualQuaternionCorrespondenceStd(const pcl::PointCloud<PointT>& source,
                                        const pcl::PointCloud<PointT>& target,
                                        const pcl::Correspondences& correspondences,
                                        CandidateStats* stats = nullptr)
{
  if (stats) {
    *stats = {};
    stats->input_points = correspondences.size();
    stats->used_staging = true;
    stats->used_fallback = true;
  }
  if (!correspondencesInRange(source, target, correspondences))
    return Eigen::Matrix4f::Identity();

  pcl::Indices source_indices;
  pcl::Indices target_indices;
  source_indices.reserve(correspondences.size());
  target_indices.reserve(correspondences.size());
  for (const auto& corr : correspondences) {
    source_indices.push_back(static_cast<pcl::index_t>(corr.index_query));
    target_indices.push_back(static_cast<pcl::index_t>(corr.index_match));
  }
  return estimateDualQuaternionDualIndexedStd(
      source, source_indices, target, target_indices, stats);
}

template <typename PointT>
inline Eigen::Matrix4f
estimateDualQuaternionCorrespondenceCandidate(
    const pcl::PointCloud<PointT>& source,
    const pcl::PointCloud<PointT>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (!correspondencesInRange(source, target, correspondences)) {
    if (stats) {
      *stats = {};
      stats->input_points = correspondences.size();
      stats->used_staging = true;
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }

  pcl::Indices source_indices;
  pcl::Indices target_indices;
  source_indices.reserve(correspondences.size());
  target_indices.reserve(correspondences.size());
  for (const auto& corr : correspondences) {
    source_indices.push_back(static_cast<pcl::index_t>(corr.index_query));
    target_indices.push_back(static_cast<pcl::index_t>(corr.index_match));
  }
  return estimateDualQuaternionDualIndexedCandidate(
      source, source_indices, target, target_indices, stats);
}

template <typename PointT>
inline Eigen::Matrix4f
estimateDualQuaternionCorrespondenceDirectCandidate(
    const pcl::PointCloud<PointT>& source,
    const pcl::PointCloud<PointT>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (!correspondencesInRange(source, target, correspondences)) {
    if (stats) {
      *stats = {};
      stats->input_points = correspondences.size();
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }

  pcl::Indices source_indices;
  pcl::Indices target_indices;
  source_indices.reserve(correspondences.size());
  target_indices.reserve(correspondences.size());
  for (const auto& corr : correspondences) {
    source_indices.push_back(static_cast<pcl::index_t>(corr.index_query));
    target_indices.push_back(static_cast<pcl::index_t>(corr.index_match));
  }
  return estimateDualQuaternionDualIndexedDirectCandidate(
      source, source_indices, target, target_indices, stats);
}

template <typename PointT>
inline Eigen::Matrix4f
estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate(
    const pcl::PointCloud<PointT>& source,
    const pcl::PointCloud<PointT>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (!correspondencesInRange(source, target, correspondences)) {
    if (stats) {
      *stats = {};
      stats->input_points = correspondences.size();
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }

  if (stats) {
    *stats = {};
    stats->input_points = correspondences.size();
    stats->accepted_points = correspondences.size();
    stats->layout_supported =
        std::is_standard_layout<PointT>::value &&
        std::is_standard_layout<pcl::Correspondence>::value;
  }

#ifdef __RVV10__
  constexpr std::uint64_t kMaxU32ByteOffset =
      std::numeric_limits<std::uint32_t>::max();
  const bool source_offsets_fit =
      source.size() <= kMaxU32ByteOffset / sizeof(PointT);
  const bool target_offsets_fit =
      target.size() <= kMaxU32ByteOffset / sizeof(PointT);
  if constexpr (std::is_standard_layout<PointT>::value &&
                std::is_standard_layout<pcl::Correspondence>::value &&
                sizeof(pcl::index_t) == sizeof(std::uint32_t)) {
    if (correspondences.size() >= kRvvMinPoints && source_offsets_fit &&
        target_offsets_fit) {
      if (stats) {
        stats->used_rvv = true;
        stats->used_gather = true;
        stats->used_correspondence_index_stream = true;
      }
      const auto* correspondence_base =
          reinterpret_cast<const std::uint8_t*>(correspondences.data());
      return finishDualQuaternionEstimate(accumulateDualQuaternionRVVIndexedImpl(
          source,
          target,
          correspondences.size(),
          CorrespondenceIndexStreamLoader{correspondence_base}));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateDualQuaternionCorrespondenceDirectCandidate(
      source, target, correspondences, stats);
}

template <typename PointT>
inline Eigen::Matrix4f
estimateDualQuaternionCorrespondenceSegmentIndexStreamCandidate(
    const pcl::PointCloud<PointT>& source,
    const pcl::PointCloud<PointT>& target,
    const pcl::Correspondences& correspondences,
    CandidateStats* stats = nullptr)
{
  if (!correspondencesInRange(source, target, correspondences)) {
    if (stats) {
      *stats = {};
      stats->input_points = correspondences.size();
      stats->used_fallback = true;
    }
    return Eigen::Matrix4f::Identity();
  }

  if (stats) {
    *stats = {};
    stats->input_points = correspondences.size();
    stats->accepted_points = correspondences.size();
    stats->layout_supported =
        std::is_standard_layout<PointT>::value &&
        std::is_standard_layout<pcl::Correspondence>::value;
  }

#ifdef __RVV10__
  constexpr std::uint64_t kMaxU32ByteOffset =
      std::numeric_limits<std::uint32_t>::max();
  const bool source_offsets_fit =
      source.size() <= kMaxU32ByteOffset / sizeof(PointT);
  const bool target_offsets_fit =
      target.size() <= kMaxU32ByteOffset / sizeof(PointT);
  if constexpr (std::is_standard_layout<PointT>::value &&
                std::is_standard_layout<pcl::Correspondence>::value &&
                sizeof(pcl::index_t) == sizeof(std::uint32_t) &&
                sizeof(pcl::Correspondence) == 3 * sizeof(std::uint32_t)) {
    if (correspondences.size() >= kRvvMinPoints && source_offsets_fit &&
        target_offsets_fit) {
      if (stats) {
        stats->used_rvv = true;
        stats->used_gather = true;
        stats->used_correspondence_segment_stream = true;
      }
      const auto* correspondence_base =
          reinterpret_cast<const std::uint8_t*>(correspondences.data());
      return finishDualQuaternionEstimate(accumulateDualQuaternionRVVIndexedImpl(
          source,
          target,
          correspondences.size(),
          CorrespondenceSegmentIndexStreamLoader{correspondence_base}));
    }
  }
#endif

  if (stats)
    stats->used_fallback = true;
  return estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate(
      source, target, correspondences, stats);
}

inline std::uint64_t
mixHash64(const std::uint64_t seed, const std::uint64_t value)
{
  return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
}

inline std::uint64_t
accumulationChecksum(const DualQuaternionAccumulation& acc)
{
  std::uint64_t hash = 1469598103934665603ull;
  hash = mixHash64(hash, static_cast<std::uint64_t>(acc.count));
  for (const double value : acc.c1) {
    // 这里只做 bench 日志 fingerprint（路径指纹），不是逐位正确性断言。
    // RVV reduction tree 与标量顺序不同，raw double bitwise checksum 会把正常舍入差异
    // 写成 mismatch；矩阵正确性仍由 gtest 的误差预算保护。
    const auto bucket = static_cast<std::int64_t>(std::llround(value / 1024.0));
    hash = mixHash64(hash, static_cast<std::uint64_t>(bucket));
  }
  for (const double value : acc.c2) {
    const auto bucket = static_cast<std::int64_t>(std::llround(value / 1024.0));
    hash = mixHash64(hash, static_cast<std::uint64_t>(bucket));
  }
  return hash;
}

inline std::uint64_t
matrixChecksum(const Eigen::Matrix4f& matrix)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (int row = 0; row < matrix.rows(); ++row) {
    for (int col = 0; col < matrix.cols(); ++col) {
      std::uint32_t bits = 0;
      const float value = matrix(row, col);
      std::memcpy(&bits, &value, sizeof(bits));
      hash = mixHash64(hash, bits);
    }
  }
  return hash;
}

inline float
maxAbsDiff(const Eigen::Matrix4f& lhs, const Eigen::Matrix4f& rhs)
{
  float max_diff = 0.0f;
  for (int row = 0; row < lhs.rows(); ++row) {
    for (int col = 0; col < lhs.cols(); ++col)
      max_diff = std::max(max_diff, std::abs(lhs(row, col) - rhs(row, col)));
  }
  return max_diff;
}

} // namespace pcl::registration::rvv_tedq_support
