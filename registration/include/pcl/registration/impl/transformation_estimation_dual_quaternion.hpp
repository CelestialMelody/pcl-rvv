/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010, Willow Garage, Inc.
 *  Copyright (c) 2012-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_DQ_HPP_
#define PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_DQ_HPP_

#include <pcl/common/eigen.h>

#include <Eigen/Eigenvalues> // for EigenSolver

#if defined(__RVV10__)
#include <pcl/rvv_point_traits.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <riscv_vector.h>
#endif

namespace pcl {

namespace registration {

#if defined(__RVV10__)
namespace detail {

constexpr std::size_t kTransformationEstimationDualQuaternionMinRVVPoints = 32;

struct TransformationEstimationDualQuaternionAccumulation {
  double c1[16]{};
  double c2[16]{};
  std::size_t count{0};
};

template <typename Scalar>
inline void
finishTransformationEstimationDualQuaternion(
    const TransformationEstimationDualQuaternionAccumulation& acc,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  transformation_matrix.setIdentity();

  Eigen::Matrix<double, 4, 4> C1 = Eigen::Matrix<double, 4, 4>::Zero();
  Eigen::Matrix<double, 4, 4> C2 = Eigen::Matrix<double, 4, 4>::Zero();
  double* c1 = C1.data();
  double* c2 = C2.data();
  for (int i = 0; i < 16; ++i) {
    c1[i] = acc.c1[i];
    c2[i] = acc.c2[i];
  }

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
      transformation_matrix(row, col) = static_cast<Scalar>(R(row, col));
  }
  transformation_matrix(0, 3) = static_cast<Scalar>(-t.x());
  transformation_matrix(1, 3) = static_cast<Scalar>(-t.y());
  transformation_matrix(2, 3) = static_cast<Scalar>(-t.z());
}

inline vfloat32mf2_t
loadTransformationEstimationDualQuaternionStridedFieldF32MF2(
    const std::uint8_t* base,
    const std::size_t field_offset,
    const std::ptrdiff_t stride,
    const std::size_t vl)
{
  return __riscv_vlse32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), stride, vl);
}

template <typename PointT>
inline vfloat32mf2_t
loadTransformationEstimationDualQuaternionIndexedFieldF32MF2(
    const std::uint8_t* base,
    const std::size_t field_offset,
    const vuint32mf2_t byte_offsets,
    const std::size_t vl)
{
  return __riscv_vluxei32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), byte_offsets, vl);
}

inline vfloat64m1_t
widenTransformationEstimationDualQuaternionF32ToF64(vfloat32mf2_t value,
                                                    const std::size_t vl)
{
  return __riscv_vfwcvt_f_f_v_f64m1(value, vl);
}

inline vfloat64m1_t
widenMulTransformationEstimationDualQuaternion(vfloat32mf2_t lhs,
                                               vfloat32mf2_t rhs,
                                               const std::size_t vl)
{
  return widenTransformationEstimationDualQuaternionF32ToF64(
      __riscv_vfmul_vv_f32mf2(lhs, rhs, vl), vl);
}

inline vfloat64m1_t
widenAddTransformationEstimationDualQuaternion(vfloat32mf2_t lhs,
                                               vfloat32mf2_t rhs,
                                               const std::size_t vl)
{
  return widenTransformationEstimationDualQuaternionF32ToF64(
      __riscv_vfadd_vv_f32mf2(lhs, rhs, vl), vl);
}

inline vfloat64m1_t
widenSubTransformationEstimationDualQuaternion(vfloat32mf2_t lhs,
                                               vfloat32mf2_t rhs,
                                               const std::size_t vl)
{
  return widenTransformationEstimationDualQuaternionF32ToF64(
      __riscv_vfsub_vv_f32mf2(lhs, rhs, vl), vl);
}

inline vfloat64m1_t
addTransformationEstimationDualQuaternionTerm(vfloat64m1_t acc,
                                              vfloat64m1_t term,
                                              const std::size_t vl)
{
  return __riscv_vfadd_vv_f64m1_tu(acc, acc, term, vl);
}

inline double
reduceTransformationEstimationDualQuaternionF64(vfloat64m1_t value,
                                                const std::size_t vlmax)
{
  const vfloat64m1_t zero = __riscv_vfmv_s_f_f64m1(0.0, 1);
  return __riscv_vfmv_f_s_f64m1_f64(
      __riscv_vfredosum_vs_f64m1_f64m1(value, zero, vlmax));
}

template <typename PointT, typename Layout>
struct TransformationEstimationDualQuaternionOrderedXYZLoader {
  const std::uint8_t* base;

  inline void
  load(const std::size_t i,
       const std::size_t vl,
       vfloat32mf2_t& x,
       vfloat32mf2_t& y,
       vfloat32mf2_t& z) const
  {
    const auto* row = base + i * sizeof(PointT);
    x = loadTransformationEstimationDualQuaternionStridedFieldF32MF2(
        row, Layout::kX, sizeof(PointT), vl);
    y = loadTransformationEstimationDualQuaternionStridedFieldF32MF2(
        row, Layout::kY, sizeof(PointT), vl);
    z = loadTransformationEstimationDualQuaternionStridedFieldF32MF2(
        row, Layout::kZ, sizeof(PointT), vl);
  }
};

template <typename PointT, typename Layout>
struct TransformationEstimationDualQuaternionIndexedXYZLoader {
  const std::uint8_t* base;
  const pcl::index_t* indices;

  inline void
  load(const std::size_t i,
       const std::size_t vl,
       vfloat32mf2_t& x,
       vfloat32mf2_t& y,
       vfloat32mf2_t& z) const
  {
    static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                  "TEDQ RVV indexed path expects 32-bit PCL indices.");
    const auto* indices_i32 = reinterpret_cast<const std::int32_t*>(indices + i);
    const vint32mf2_t v_index = __riscv_vle32_v_i32mf2(indices_i32, vl);
    const vuint32mf2_t byte_offsets = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_index),
        static_cast<std::uint32_t>(sizeof(PointT)),
        vl);
    x = loadTransformationEstimationDualQuaternionIndexedFieldF32MF2<PointT>(
        base, Layout::kX, byte_offsets, vl);
    y = loadTransformationEstimationDualQuaternionIndexedFieldF32MF2<PointT>(
        base, Layout::kY, byte_offsets, vl);
    z = loadTransformationEstimationDualQuaternionIndexedFieldF32MF2<PointT>(
        base, Layout::kZ, byte_offsets, vl);
  }
};

template <typename SourceLoader, typename TargetLoader>
inline TransformationEstimationDualQuaternionAccumulation
accumulateTransformationEstimationDualQuaternionRVV(
    const std::size_t nr_points,
    const SourceLoader& source_loader,
    const TargetLoader& target_loader)
{
  TransformationEstimationDualQuaternionAccumulation acc;
  acc.count = nr_points;

  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t c100 = zero, c105 = zero, c110 = zero, c115 = zero;
  vfloat64m1_t c101 = zero, c102 = zero, c103 = zero, c106 = zero;
  vfloat64m1_t c107 = zero, c111 = zero;
  vfloat64m1_t c201 = zero, c202 = zero, c203 = zero;
  vfloat64m1_t c206 = zero, c207 = zero, c211 = zero;

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    vfloat32mf2_t ax, ay, az, bx, by, bz;
    source_loader.load(i, vl, ax, ay, az);
    target_loader.load(i, vl, bx, by, bz);

    const vfloat64m1_t axbx =
        widenMulTransformationEstimationDualQuaternion(ax, bx, vl);
    const vfloat64m1_t ayby =
        widenMulTransformationEstimationDualQuaternion(ay, by, vl);
    const vfloat64m1_t azbz =
        widenMulTransformationEstimationDualQuaternion(az, bz, vl);
    const vfloat64m1_t axby =
        widenMulTransformationEstimationDualQuaternion(ax, by, vl);
    const vfloat64m1_t aybx =
        widenMulTransformationEstimationDualQuaternion(ay, bx, vl);
    const vfloat64m1_t axbz =
        widenMulTransformationEstimationDualQuaternion(ax, bz, vl);
    const vfloat64m1_t azbx =
        widenMulTransformationEstimationDualQuaternion(az, bx, vl);
    const vfloat64m1_t aybz =
        widenMulTransformationEstimationDualQuaternion(ay, bz, vl);
    const vfloat64m1_t azby =
        widenMulTransformationEstimationDualQuaternion(az, by, vl);

    c100 = addTransformationEstimationDualQuaternionTerm(
        c100,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(axbx, azbz, vl), ayby, vl),
        vl);
    c105 = addTransformationEstimationDualQuaternionTerm(
        c105,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(ayby, azbz, vl), axbx, vl),
        vl);
    c110 = addTransformationEstimationDualQuaternionTerm(
        c110,
        __riscv_vfsub_vv_f64m1(__riscv_vfsub_vv_f64m1(azbz, axbx, vl), ayby, vl),
        vl);
    c115 = addTransformationEstimationDualQuaternionTerm(
        c115,
        __riscv_vfadd_vv_f64m1(__riscv_vfadd_vv_f64m1(axbx, ayby, vl), azbz, vl),
        vl);
    c101 = addTransformationEstimationDualQuaternionTerm(
        c101, __riscv_vfadd_vv_f64m1(axby, aybx, vl), vl);
    c102 = addTransformationEstimationDualQuaternionTerm(
        c102, __riscv_vfadd_vv_f64m1(axbz, azbx, vl), vl);
    c103 = addTransformationEstimationDualQuaternionTerm(
        c103, __riscv_vfsub_vv_f64m1(aybz, azby, vl), vl);
    c106 = addTransformationEstimationDualQuaternionTerm(
        c106, __riscv_vfadd_vv_f64m1(azby, aybz, vl), vl);
    c107 = addTransformationEstimationDualQuaternionTerm(
        c107, __riscv_vfsub_vv_f64m1(azbx, axbz, vl), vl);
    c111 = addTransformationEstimationDualQuaternionTerm(
        c111, __riscv_vfsub_vv_f64m1(axby, aybx, vl), vl);
    c201 = addTransformationEstimationDualQuaternionTerm(
        c201, widenAddTransformationEstimationDualQuaternion(az, bz, vl), vl);
    c202 = addTransformationEstimationDualQuaternionTerm(
        c202,
        __riscv_vfneg_v_f64m1(
            widenAddTransformationEstimationDualQuaternion(ay, by, vl), vl),
        vl);
    c203 = addTransformationEstimationDualQuaternionTerm(
        c203, widenSubTransformationEstimationDualQuaternion(ax, bx, vl), vl);
    c206 = addTransformationEstimationDualQuaternionTerm(
        c206, widenAddTransformationEstimationDualQuaternion(ax, bx, vl), vl);
    c207 = addTransformationEstimationDualQuaternionTerm(
        c207, widenSubTransformationEstimationDualQuaternion(ay, by, vl), vl);
    c211 = addTransformationEstimationDualQuaternionTerm(
        c211, widenSubTransformationEstimationDualQuaternion(az, bz, vl), vl);
    i += vl;
  }

  acc.c1[0] = reduceTransformationEstimationDualQuaternionF64(c100, vlmax);
  acc.c1[5] = reduceTransformationEstimationDualQuaternionF64(c105, vlmax);
  acc.c1[10] = reduceTransformationEstimationDualQuaternionF64(c110, vlmax);
  acc.c1[15] = reduceTransformationEstimationDualQuaternionF64(c115, vlmax);
  acc.c1[1] = reduceTransformationEstimationDualQuaternionF64(c101, vlmax);
  acc.c1[2] = reduceTransformationEstimationDualQuaternionF64(c102, vlmax);
  acc.c1[3] = reduceTransformationEstimationDualQuaternionF64(c103, vlmax);
  acc.c1[6] = reduceTransformationEstimationDualQuaternionF64(c106, vlmax);
  acc.c1[7] = reduceTransformationEstimationDualQuaternionF64(c107, vlmax);
  acc.c1[11] = reduceTransformationEstimationDualQuaternionF64(c111, vlmax);
  acc.c2[1] = reduceTransformationEstimationDualQuaternionF64(c201, vlmax);
  acc.c2[2] = reduceTransformationEstimationDualQuaternionF64(c202, vlmax);
  acc.c2[3] = reduceTransformationEstimationDualQuaternionF64(c203, vlmax);
  acc.c2[6] = reduceTransformationEstimationDualQuaternionF64(c206, vlmax);
  acc.c2[7] = reduceTransformationEstimationDualQuaternionF64(c207, vlmax);
  acc.c2[11] = reduceTransformationEstimationDualQuaternionF64(c211, vlmax);
  return acc;
}

template <typename PointT>
inline bool
transformationEstimationDualQuaternionIndicesInRange(
    const pcl::PointCloud<PointT>& cloud,
    const pcl::Indices& indices)
{
  for (const pcl::index_t index : indices) {
    if (index < 0 || static_cast<std::size_t>(index) >= cloud.size())
      return false;
  }
  return true;
}

template <typename PointT>
inline bool
transformationEstimationDualQuaternionIndicesFitRVVGather(
    const pcl::PointCloud<PointT>& cloud,
    const pcl::Indices& indices)
{
  return cloud.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>() &&
         transformationEstimationDualQuaternionIndicesInRange(cloud, indices);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = cloud_src.size();
    if (cloud_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < kTransformationEstimationDualQuaternionMinRVVPoints) {
      return false;
    }

    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
    finishTransformationEstimationDualQuaternion(
        accumulateTransformationEstimationDualQuaternionRVV(
            nr_points,
            TransformationEstimationDualQuaternionOrderedXYZLoader<PointSource,
                                                                   SrcLayout>{
                source_base},
            TransformationEstimationDualQuaternionOrderedXYZLoader<PointTarget,
                                                                   TgtLayout>{
                target_base}),
        transformation_matrix);
    return true;
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationDualQuaternionSourceIndexedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                !TgtLayout::value ||
                sizeof(pcl::index_t) != sizeof(std::int32_t)) {
    return false;
  }
  else {
    const std::size_t nr_points = indices_src.size();
    if (cloud_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < kTransformationEstimationDualQuaternionMinRVVPoints ||
        !transformationEstimationDualQuaternionIndicesFitRVVGather(cloud_src,
                                                                   indices_src)) {
      return false;
    }

    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
    finishTransformationEstimationDualQuaternion(
        accumulateTransformationEstimationDualQuaternionRVV(
            nr_points,
            TransformationEstimationDualQuaternionIndexedXYZLoader<PointSource,
                                                                   SrcLayout>{
                source_base, indices_src.data()},
            TransformationEstimationDualQuaternionOrderedXYZLoader<PointTarget,
                                                                   TgtLayout>{
                target_base}),
        transformation_matrix);
    return true;
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationDualQuaternionDualIndexedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                !TgtLayout::value ||
                sizeof(pcl::index_t) != sizeof(std::int32_t)) {
    return false;
  }
  else {
    const std::size_t nr_points = indices_src.size();
    if (indices_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < kTransformationEstimationDualQuaternionMinRVVPoints ||
        !transformationEstimationDualQuaternionIndicesFitRVVGather(cloud_src,
                                                                   indices_src) ||
        !transformationEstimationDualQuaternionIndicesFitRVVGather(cloud_tgt,
                                                                   indices_tgt)) {
      return false;
    }

    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
    finishTransformationEstimationDualQuaternion(
        accumulateTransformationEstimationDualQuaternionRVV(
            nr_points,
            TransformationEstimationDualQuaternionIndexedXYZLoader<PointSource,
                                                                   SrcLayout>{
                source_base, indices_src.data()},
            TransformationEstimationDualQuaternionIndexedXYZLoader<PointTarget,
                                                                   TgtLayout>{
                target_base, indices_tgt.data()}),
        transformation_matrix);
    return true;
  }
}

} // namespace detail
#endif // __RVV10__

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationDualQuaternion<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = cloud_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR(
        "[pcl::TransformationEstimationDualQuaternion::estimateRigidTransformation] "
        "Number or points in source (%zu) differs than target (%zu)!\n",
        static_cast<std::size_t>(nr_points),
        static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#if defined(__RVV10__)
  if (detail::estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(
          cloud_src, cloud_tgt, transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationDualQuaternion<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  if (indices_src.size() != cloud_tgt.size()) {
    PCL_ERROR("[pcl::TransformationDQ::estimateRigidTransformation] Number or points "
              "in source (%zu) differs than target (%zu)!\n",
              indices_src.size(),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#if defined(__RVV10__)
  if (detail::estimateRigidTransformationDualQuaternionSourceIndexedCloudPairRVV(
          cloud_src, indices_src, cloud_tgt, transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationDualQuaternion<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Indices& indices_tgt,
                                Matrix4& transformation_matrix) const
{
  if (indices_src.size() != indices_tgt.size()) {
    PCL_ERROR(
        "[pcl::TransformationEstimationDualQuaternion::estimateRigidTransformation] "
        "Number or points in source (%lu) differs than target (%lu)!\n",
        indices_src.size(),
        indices_tgt.size());
    return;
  }

#if defined(__RVV10__)
  if (detail::estimateRigidTransformationDualQuaternionDualIndexedCloudPairRVV(
          cloud_src, indices_src, cloud_tgt, indices_tgt, transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationDualQuaternion<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Correspondences& correspondences,
                                Matrix4& transformation_matrix) const
{
  ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationDualQuaternion<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(ConstCloudIterator<PointSource>& source_it,
                                ConstCloudIterator<PointTarget>& target_it,
                                Matrix4& transformation_matrix) const
{
  const int npts = static_cast<int>(source_it.size());

  transformation_matrix.setIdentity();

  // dual quaternion optimization
  Eigen::Matrix<double, 4, 4> C1 = Eigen::Matrix<double, 4, 4>::Zero();
  Eigen::Matrix<double, 4, 4> C2 = Eigen::Matrix<double, 4, 4>::Zero();
  double* c1 = C1.data();
  double* c2 = C2.data();

  for (int i = 0; i < npts; ++i) {
    const PointSource& a = *source_it;
    const PointTarget& b = *target_it;
    const double axbx = a.x * b.x;
    const double ayby = a.y * b.y;
    const double azbz = a.z * b.z;
    const double axby = a.x * b.y;
    const double aybx = a.y * b.x;
    const double axbz = a.x * b.z;
    const double azbx = a.z * b.x;
    const double aybz = a.y * b.z;
    const double azby = a.z * b.y;
    c1[0] += axbx - azbz - ayby;
    c1[5] += ayby - azbz - axbx;
    c1[10] += azbz - axbx - ayby;
    c1[15] += axbx + ayby + azbz;
    c1[1] += axby + aybx;
    c1[2] += axbz + azbx;
    c1[3] += aybz - azby;
    c1[6] += azby + aybz;
    c1[7] += azbx - axbz;
    c1[11] += axby - aybx;

    c2[1] += a.z + b.z;
    c2[2] -= a.y + b.y;
    c2[3] += a.x - b.x;
    c2[6] += a.x + b.x;
    c2[7] += a.y - b.y;
    c2[11] += a.z - b.z;
    ++source_it;
    ++target_it;
  }

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
      (0.25 / static_cast<double>(npts)) * C2.transpose() * C2 - C1;

  const Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 4, 4>> es(A);

  ptrdiff_t i;
  es.eigenvalues().maxCoeff(&i);
  const Eigen::Matrix<double, 4, 1> qmat = es.eigenvectors().col(i);
  const Eigen::Matrix<double, 4, 1> smat =
      -(0.5 / static_cast<double>(npts)) * C2 * qmat;

  const Eigen::Quaternion<double> q(qmat(3), qmat(0), qmat(1), qmat(2));
  const Eigen::Quaternion<double> s(smat(3), smat(0), smat(1), smat(2));

  const Eigen::Quaternion<double> t = s * q.conjugate();

  const Eigen::Matrix<double, 3, 3> R(q.toRotationMatrix());

  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      transformation_matrix(i, j) = R(i, j);

  transformation_matrix(0, 3) = -t.x();
  transformation_matrix(1, 3) = -t.y();
  transformation_matrix(2, 3) = -t.z();
}

} // namespace registration
} // namespace pcl

#endif /* PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_DQ_HPP_ */
