/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
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
 * $Id$
 *
 */

#ifndef PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_POINT_TO_PLANE_LLS_WEIGHTED_HPP_
#define PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_POINT_TO_PLANE_LLS_WEIGHTED_HPP_

#include <pcl/cloud_iterator.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace pcl {

namespace registration {

namespace detail {

struct PointToPlaneLLSWeightedFullCloudStats {
  std::size_t input_points = 0;
  std::size_t accepted_points = 0;
  bool used_rvv = false;
};

inline void
setPointToPlaneLLSWeightedStats(PointToPlaneLLSWeightedFullCloudStats* stats,
                                const std::size_t input_points,
                                const std::size_t accepted_points,
                                const bool used_rvv)
{
  if (!stats)
    return;
  stats->input_points = input_points;
  stats->accepted_points = accepted_points;
  stats->used_rvv = used_rvv;
}

struct PointToPlaneLLSWeightedNormalEquation {
  Eigen::Matrix<double, 6, 6> ata = Eigen::Matrix<double, 6, 6>::Zero();
  Eigen::Matrix<double, 6, 1> atb = Eigen::Matrix<double, 6, 1>::Zero();
  std::size_t accepted_points = 0;
};

template <typename PointSource, typename PointTarget>
inline bool
isFinitePointToPlaneLLSWeightedRow(const PointSource& source, const PointTarget& target)
{
  return std::isfinite(source.x) && std::isfinite(source.y) &&
         std::isfinite(source.z) && std::isfinite(target.x) &&
         std::isfinite(target.y) && std::isfinite(target.z) &&
         std::isfinite(target.normal_x) && std::isfinite(target.normal_y) &&
         std::isfinite(target.normal_z);
}

template <typename PointSource, typename PointTarget>
inline void
accumulatePointToPlaneLLSWeightedRow(const PointSource& source,
                                     const PointTarget& target,
                                     const float weight,
                                     PointToPlaneLLSWeightedNormalEquation& eq)
{
  const float& sx = source.x;
  const float& sy = source.y;
  const float& sz = source.z;
  const float& dx = target.x;
  const float& dy = target.y;
  const float& dz = target.z;
  const float nx = target.normal[0] * weight;
  const float ny = target.normal[1] * weight;
  const float nz = target.normal[2] * weight;

  const double a = nz * sy - ny * sz;
  const double b = nx * sz - nz * sx;
  const double c = ny * sx - nx * sy;

  eq.ata.coeffRef(0) += a * a;
  eq.ata.coeffRef(1) += a * b;
  eq.ata.coeffRef(2) += a * c;
  eq.ata.coeffRef(3) += a * nx;
  eq.ata.coeffRef(4) += a * ny;
  eq.ata.coeffRef(5) += a * nz;
  eq.ata.coeffRef(7) += b * b;
  eq.ata.coeffRef(8) += b * c;
  eq.ata.coeffRef(9) += b * nx;
  eq.ata.coeffRef(10) += b * ny;
  eq.ata.coeffRef(11) += b * nz;
  eq.ata.coeffRef(14) += c * c;
  eq.ata.coeffRef(15) += c * nx;
  eq.ata.coeffRef(16) += c * ny;
  eq.ata.coeffRef(17) += c * nz;
  eq.ata.coeffRef(21) += nx * nx;
  eq.ata.coeffRef(22) += nx * ny;
  eq.ata.coeffRef(23) += nx * nz;
  eq.ata.coeffRef(28) += ny * ny;
  eq.ata.coeffRef(29) += ny * nz;
  eq.ata.coeffRef(35) += nz * nz;

  const double d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz;
  eq.atb.coeffRef(0) += a * d;
  eq.atb.coeffRef(1) += b * d;
  eq.atb.coeffRef(2) += c * d;
  eq.atb.coeffRef(3) += nx * d;
  eq.atb.coeffRef(4) += ny * d;
  eq.atb.coeffRef(5) += nz * d;
  ++eq.accepted_points;
}

inline void
completePointToPlaneLLSWeightedNormalEquation(
    PointToPlaneLLSWeightedNormalEquation& eq)
{
  eq.ata.coeffRef(6) = eq.ata.coeff(1);
  eq.ata.coeffRef(12) = eq.ata.coeff(2);
  eq.ata.coeffRef(13) = eq.ata.coeff(8);
  eq.ata.coeffRef(18) = eq.ata.coeff(3);
  eq.ata.coeffRef(19) = eq.ata.coeff(9);
  eq.ata.coeffRef(20) = eq.ata.coeff(15);
  eq.ata.coeffRef(24) = eq.ata.coeff(4);
  eq.ata.coeffRef(25) = eq.ata.coeff(10);
  eq.ata.coeffRef(26) = eq.ata.coeff(16);
  eq.ata.coeffRef(27) = eq.ata.coeff(22);
  eq.ata.coeffRef(30) = eq.ata.coeff(5);
  eq.ata.coeffRef(31) = eq.ata.coeff(11);
  eq.ata.coeffRef(32) = eq.ata.coeff(17);
  eq.ata.coeffRef(33) = eq.ata.coeff(23);
  eq.ata.coeffRef(34) = eq.ata.coeff(29);
}

template <typename PointSource, typename PointTarget>
inline PointToPlaneLLSWeightedNormalEquation
buildPointToPlaneLLSWeightedFullCloudStd(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
  PointToPlaneLLSWeightedNormalEquation eq;
  const std::size_t nr_points =
      std::min(std::min(cloud_src.size(), cloud_tgt.size()), weights.size());
  for (std::size_t i = 0; i < nr_points; ++i) {
    if (!isFinitePointToPlaneLLSWeightedRow(cloud_src[i], cloud_tgt[i]))
      continue;
    accumulatePointToPlaneLLSWeightedRow(cloud_src[i], cloud_tgt[i], weights[i], eq);
  }
  if (stats) {
    stats->input_points = nr_points;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

template <typename PointSource, typename PointTarget>
inline PointToPlaneLLSWeightedNormalEquation
buildPointToPlaneLLSWeightedSourceIndicesStd(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
  PointToPlaneLLSWeightedNormalEquation eq;
  const std::size_t nr_points =
      std::min(std::min(indices_src.size(), cloud_tgt.size()), weights.size());
  for (std::size_t row = 0; row < nr_points; ++row) {
    if (indices_src[row] < 0)
      continue;
    const auto source_index = static_cast<std::size_t>(indices_src[row]);
    if (source_index >= cloud_src.size())
      continue;
    if (!isFinitePointToPlaneLLSWeightedRow(cloud_src[source_index], cloud_tgt[row]))
      continue;
    accumulatePointToPlaneLLSWeightedRow(
        cloud_src[source_index], cloud_tgt[row], weights[row], eq);
  }
  setPointToPlaneLLSWeightedStats(stats, nr_points, eq.accepted_points, false);
  return eq;
}

template <typename Scalar>
inline void
constructPointToPlaneLLSWeightedTransformationMatrix(
    const Eigen::Matrix<double, 6, 1>& x,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  const double alpha = x(0);
  const double beta = x(1);
  const double gamma = x(2);
  transformation_matrix = Eigen::Matrix<Scalar, 4, 4>::Zero();
  transformation_matrix(0, 0) =
      static_cast<Scalar>(std::cos(gamma) * std::cos(beta));
  transformation_matrix(0, 1) = static_cast<Scalar>(
      -std::sin(gamma) * std::cos(alpha) +
      std::cos(gamma) * std::sin(beta) * std::sin(alpha));
  transformation_matrix(0, 2) = static_cast<Scalar>(
      std::sin(gamma) * std::sin(alpha) +
      std::cos(gamma) * std::sin(beta) * std::cos(alpha));
  transformation_matrix(1, 0) =
      static_cast<Scalar>(std::sin(gamma) * std::cos(beta));
  transformation_matrix(1, 1) = static_cast<Scalar>(
      std::cos(gamma) * std::cos(alpha) +
      std::sin(gamma) * std::sin(beta) * std::sin(alpha));
  transformation_matrix(1, 2) = static_cast<Scalar>(
      -std::cos(gamma) * std::sin(alpha) +
      std::sin(gamma) * std::sin(beta) * std::cos(alpha));
  transformation_matrix(2, 0) = static_cast<Scalar>(-std::sin(beta));
  transformation_matrix(2, 1) =
      static_cast<Scalar>(std::cos(beta) * std::sin(alpha));
  transformation_matrix(2, 2) =
      static_cast<Scalar>(std::cos(beta) * std::cos(alpha));
  transformation_matrix(0, 3) = static_cast<Scalar>(x(3));
  transformation_matrix(1, 3) = static_cast<Scalar>(x(4));
  transformation_matrix(2, 3) = static_cast<Scalar>(x(5));
  transformation_matrix(3, 3) = static_cast<Scalar>(1);
}

template <typename Scalar>
inline void
solvePointToPlaneLLSWeightedNormalEquation(
    PointToPlaneLLSWeightedNormalEquation eq,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  completePointToPlaneLLSWeightedNormalEquation(eq);
  const Eigen::Matrix<double, 6, 1> x =
      static_cast<Eigen::Matrix<double, 6, 1>>(eq.ata.inverse() * eq.atb);
  constructPointToPlaneLLSWeightedTransformationMatrix(x, transformation_matrix);
}

template <typename PointSource, typename PointTarget>
inline bool
canUsePointToPlaneLLSWeightedFullCloudRVV(const std::size_t nr_points,
                                          const std::size_t target_points,
                                          const std::size_t weights_size,
                                          const std::size_t vlmax)
{
  return target_points == nr_points && weights_size == nr_points && nr_points >= 64 &&
         vlmax <= 64 &&
         nr_points <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() &&
         nr_points <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>();
}

template <typename PointSource, typename PointTarget>
inline bool
canUsePointToPlaneLLSWeightedSourceIndicesRVV(const std::size_t source_points,
                                             const std::size_t index_points,
                                             const std::size_t target_points,
                                             const std::size_t weights_size,
                                             const std::size_t vlmax)
{
  return target_points == index_points && weights_size == index_points &&
         index_points >= 64 && vlmax <= 64 &&
         source_points <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() &&
         target_points <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>();
}

#if defined(__RVV10__)
inline vbool32_t
finitePointToPlaneLLSWeightedF32M1(vfloat32m1_t value, const std::size_t vl)
{
  const vfloat32m1_t abs_value = __riscv_vfabs_v_f32m1(value, vl);
  return __riscv_vmfle_vf_f32m1_b32(abs_value, std::numeric_limits<float>::max(), vl);
}

inline float
reducePointToPlaneLLSWeightedSumF32M1(vfloat32m1_t value, const std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  const vfloat32m1_t reduced = __riscv_vfredosum_vs_f32m1_f32m1(value, zero, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline vbool16_t
finitePointToPlaneLLSWeightedF32M2(vfloat32m2_t value, const std::size_t vl)
{
  const vfloat32m2_t abs_value = __riscv_vfabs_v_f32m2(value, vl);
  return __riscv_vmfle_vf_f32m2_b16(abs_value, std::numeric_limits<float>::max(), vl);
}

inline void
accumulatePointToPlaneLLSWeightedCompressedRowsF32M2(
    vfloat32m2_t a,
    vfloat32m2_t b,
    vfloat32m2_t c,
    vfloat32m2_t d,
    vfloat32m2_t nx,
    vfloat32m2_t ny,
    vfloat32m2_t nz,
    vbool16_t keep,
    const std::size_t vl,
    PointToPlaneLLSWeightedNormalEquation& eq)
{
  alignas(16) float a_buf[64];
  alignas(16) float b_buf[64];
  alignas(16) float c_buf[64];
  alignas(16) float d_buf[64];
  alignas(16) float nx_buf[64];
  alignas(16) float ny_buf[64];
  alignas(16) float nz_buf[64];
  const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
  if (kept == 0)
    return;

  __riscv_vse32_v_f32m2(a_buf, __riscv_vcompress_vm_f32m2(a, keep, vl), kept);
  __riscv_vse32_v_f32m2(b_buf, __riscv_vcompress_vm_f32m2(b, keep, vl), kept);
  __riscv_vse32_v_f32m2(c_buf, __riscv_vcompress_vm_f32m2(c, keep, vl), kept);
  __riscv_vse32_v_f32m2(d_buf, __riscv_vcompress_vm_f32m2(d, keep, vl), kept);
  __riscv_vse32_v_f32m2(nx_buf, __riscv_vcompress_vm_f32m2(nx, keep, vl), kept);
  __riscv_vse32_v_f32m2(ny_buf, __riscv_vcompress_vm_f32m2(ny, keep, vl), kept);
  __riscv_vse32_v_f32m2(nz_buf, __riscv_vcompress_vm_f32m2(nz, keep, vl), kept);

  for (std::size_t lane = 0; lane < kept; ++lane) {
    const double a_d = a_buf[lane];
    const double b_d = b_buf[lane];
    const double c_d = c_buf[lane];
    const double nx_d = nx_buf[lane];
    const double ny_d = ny_buf[lane];
    const double nz_d = nz_buf[lane];
    const double d_d = d_buf[lane];

    eq.ata.coeffRef(0) += a_d * a_d;
    eq.ata.coeffRef(1) += a_d * b_d;
    eq.ata.coeffRef(2) += a_d * c_d;
    eq.ata.coeffRef(3) += a_d * nx_d;
    eq.ata.coeffRef(4) += a_d * ny_d;
    eq.ata.coeffRef(5) += a_d * nz_d;
    eq.ata.coeffRef(7) += b_d * b_d;
    eq.ata.coeffRef(8) += b_d * c_d;
    eq.ata.coeffRef(9) += b_d * nx_d;
    eq.ata.coeffRef(10) += b_d * ny_d;
    eq.ata.coeffRef(11) += b_d * nz_d;
    eq.ata.coeffRef(14) += c_d * c_d;
    eq.ata.coeffRef(15) += c_d * nx_d;
    eq.ata.coeffRef(16) += c_d * ny_d;
    eq.ata.coeffRef(17) += c_d * nz_d;
    eq.ata.coeffRef(21) += nx_d * nx_d;
    eq.ata.coeffRef(22) += nx_d * ny_d;
    eq.ata.coeffRef(23) += nx_d * nz_d;
    eq.ata.coeffRef(28) += ny_d * ny_d;
    eq.ata.coeffRef(29) += ny_d * nz_d;
    eq.ata.coeffRef(35) += nz_d * nz_d;

    eq.atb.coeffRef(0) += a_d * d_d;
    eq.atb.coeffRef(1) += b_d * d_d;
    eq.atb.coeffRef(2) += c_d * d_d;
    eq.atb.coeffRef(3) += nx_d * d_d;
    eq.atb.coeffRef(4) += ny_d * d_d;
    eq.atb.coeffRef(5) += nz_d * d_d;
    ++eq.accepted_points;
  }
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
loadPointToPlaneLLSWeightedSourceIndexedVectors(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t i,
    const std::size_t vl,
    vfloat32m2_t& a,
    vfloat32m2_t& b,
    vfloat32m2_t& c,
    vfloat32m2_t& d,
    vfloat32m2_t& nx,
    vfloat32m2_t& ny,
    vfloat32m2_t& nz,
    vbool16_t& keep)
{
  constexpr std::ptrdiff_t kTargetStride = sizeof(PointTarget);
  const vuint32m2_t source_index_vector =
      __riscv_vle32_v_u32m2(source_indices + i, vl);
  const vuint32m2_t source_offsets =
      __riscv_vmul_vx_u32m2(source_index_vector, sizeof(PointSource), vl);

  const auto gather_source = [&](const std::size_t offset) -> vfloat32m2_t {
    return __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(source_base + offset), source_offsets, vl);
  };
  const auto load_target = [&](const std::size_t offset) -> vfloat32m2_t {
    return __riscv_vlse32_v_f32m2(
        reinterpret_cast<const float*>(target_base + i * sizeof(PointTarget) + offset),
        kTargetStride,
        vl);
  };

  const vfloat32m2_t sx = gather_source(SrcLayout::kX);
  const vfloat32m2_t sy = gather_source(SrcLayout::kY);
  const vfloat32m2_t sz = gather_source(SrcLayout::kZ);
  const vfloat32m2_t dx = load_target(TgtLayout::kX);
  const vfloat32m2_t dy = load_target(TgtLayout::kY);
  const vfloat32m2_t dz = load_target(TgtLayout::kZ);
  const vfloat32m2_t normal_x = load_target(TgtLayout::kNX);
  const vfloat32m2_t normal_y = load_target(TgtLayout::kNY);
  const vfloat32m2_t normal_z = load_target(TgtLayout::kNZ);
  const vfloat32m2_t weight = __riscv_vle32_v_f32m2(weights + i, vl);

  keep = finitePointToPlaneLLSWeightedF32M2(sx, vl);
  keep = __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(sy, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(sz, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(dx, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(dy, vl), vl);
  keep = __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(dz, vl), vl);
  keep =
      __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(normal_x, vl), vl);
  keep =
      __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(normal_y, vl), vl);
  keep =
      __riscv_vmand_mm_b16(keep, finitePointToPlaneLLSWeightedF32M2(normal_z, vl), vl);

  nx = __riscv_vfmul_vv_f32m2(normal_x, weight, vl);
  ny = __riscv_vfmul_vv_f32m2(normal_y, weight, vl);
  nz = __riscv_vfmul_vv_f32m2(normal_z, weight, vl);

  a = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nz, sy, vl),
                             __riscv_vfmul_vv_f32m2(ny, sz, vl),
                             vl);
  b = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nx, sz, vl),
                             __riscv_vfmul_vv_f32m2(nz, sx, vl),
                             vl);
  c = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(ny, sx, vl),
                             __riscv_vfmul_vv_f32m2(nx, sy, vl),
                             vl);

  d = __riscv_vfmul_vv_f32m2(nx, dx, vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(ny, dy, vl), vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(nz, dz, vl), vl);
  d = __riscv_vfsub_vv_f32m2(d, __riscv_vfmul_vv_f32m2(nx, sx, vl), vl);
  d = __riscv_vfsub_vv_f32m2(d, __riscv_vfmul_vv_f32m2(ny, sy, vl), vl);
  d = __riscv_vfsub_vv_f32m2(d, __riscv_vfmul_vv_f32m2(nz, sz, vl), vl);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
loadPointToPlaneLLSWeightedSourceIndexedBlockVectors(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t i,
    const std::size_t vl,
    vfloat32m1_t& a,
    vfloat32m1_t& b,
    vfloat32m1_t& c,
    vfloat32m1_t& d,
    vfloat32m1_t& nx,
    vfloat32m1_t& ny,
    vfloat32m1_t& nz,
    vbool32_t& keep)
{
  constexpr std::ptrdiff_t kTargetStride = sizeof(PointTarget);
  const vuint32m1_t source_index_vector =
      __riscv_vle32_v_u32m1(source_indices + i, vl);
  const vuint32m1_t source_offsets =
      __riscv_vmul_vx_u32m1(source_index_vector, sizeof(PointSource), vl);

  const auto gather_source = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vluxei32_v_f32m1(
        reinterpret_cast<const float*>(source_base + offset), source_offsets, vl);
  };
  const auto load_target = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(target_base + i * sizeof(PointTarget) + offset),
        kTargetStride,
        vl);
  };

  const vfloat32m1_t sx = gather_source(SrcLayout::kX);
  const vfloat32m1_t sy = gather_source(SrcLayout::kY);
  const vfloat32m1_t sz = gather_source(SrcLayout::kZ);
  const vfloat32m1_t dx = load_target(TgtLayout::kX);
  const vfloat32m1_t dy = load_target(TgtLayout::kY);
  const vfloat32m1_t dz = load_target(TgtLayout::kZ);
  const vfloat32m1_t normal_x = load_target(TgtLayout::kNX);
  const vfloat32m1_t normal_y = load_target(TgtLayout::kNY);
  const vfloat32m1_t normal_z = load_target(TgtLayout::kNZ);
  const vfloat32m1_t weight = __riscv_vle32_v_f32m1(weights + i, vl);

  keep = finitePointToPlaneLLSWeightedF32M1(sx, vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(sy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(sz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(dx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(dy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(dz, vl), vl);
  keep =
      __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(normal_x, vl), vl);
  keep =
      __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(normal_y, vl), vl);
  keep =
      __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(normal_z, vl), vl);

  nx = __riscv_vfmul_vv_f32m1(normal_x, weight, vl);
  ny = __riscv_vfmul_vv_f32m1(normal_y, weight, vl);
  nz = __riscv_vfmul_vv_f32m1(normal_z, weight, vl);

  a = __riscv_vfmul_vv_f32m1(ny, sz, vl);
  b = __riscv_vfmul_vv_f32m1(nz, sx, vl);
  c = __riscv_vfmul_vv_f32m1(nx, sy, vl);
  const vfloat32m1_t dsx = __riscv_vfsub_vv_f32m1(dx, sx, vl);
  const vfloat32m1_t dsy = __riscv_vfsub_vv_f32m1(dy, sy, vl);
  const vfloat32m1_t dsz = __riscv_vfsub_vv_f32m1(dz, sz, vl);

  a = __riscv_vfmsac_vv_f32m1(a, nz, sy, vl);
  b = __riscv_vfmsac_vv_f32m1(b, nx, sz, vl);
  c = __riscv_vfmsac_vv_f32m1(c, ny, sx, vl);
  d = __riscv_vfmul_vv_f32m1(nx, dsx, vl);
  d = __riscv_vfmacc_vv_f32m1(d, ny, dsy, vl);
  d = __riscv_vfmacc_vv_f32m1(d, nz, dsz, vl);

  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  a = __riscv_vmerge_vvm_f32m1(zero, a, keep, vl);
  b = __riscv_vmerge_vvm_f32m1(zero, b, keep, vl);
  c = __riscv_vmerge_vvm_f32m1(zero, c, keep, vl);
  d = __riscv_vmerge_vvm_f32m1(zero, d, keep, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, keep, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, keep, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, keep, vl);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
loadPointToPlaneLLSWeightedFullReductionVectors(const std::uint8_t* source_base,
                                                const std::uint8_t* target_base,
                                                const float* weights,
                                                const std::size_t i,
                                                const std::size_t vl,
                                                vfloat32m1_t& a,
                                                vfloat32m1_t& b,
                                                vfloat32m1_t& c,
                                                vfloat32m1_t& d,
                                                vfloat32m1_t& nx,
                                                vfloat32m1_t& ny,
                                                vfloat32m1_t& nz,
                                                vbool32_t& keep)
{
  constexpr std::ptrdiff_t kSourceStride = sizeof(PointSource);
  constexpr std::ptrdiff_t kTargetStride = sizeof(PointTarget);

  // Layout gates prove single-float fields and AoS byte offsets. Source and
  // target still carry separate offsets and strides.
  const auto load_source = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(source_base + i * sizeof(PointSource) + offset),
        kSourceStride,
        vl);
  };
  const auto load_target = [&](const std::size_t offset) -> vfloat32m1_t {
    return __riscv_vlse32_v_f32m1(
        reinterpret_cast<const float*>(target_base + i * sizeof(PointTarget) + offset),
        kTargetStride,
        vl);
  };

  const vfloat32m1_t sx = load_source(SrcLayout::kX);
  const vfloat32m1_t sy = load_source(SrcLayout::kY);
  const vfloat32m1_t sz = load_source(SrcLayout::kZ);
  const vfloat32m1_t dx = load_target(TgtLayout::kX);
  const vfloat32m1_t dy = load_target(TgtLayout::kY);
  const vfloat32m1_t dz = load_target(TgtLayout::kZ);
  const vfloat32m1_t normal_x = load_target(TgtLayout::kNX);
  const vfloat32m1_t normal_y = load_target(TgtLayout::kNY);
  const vfloat32m1_t normal_z = load_target(TgtLayout::kNZ);
  const vfloat32m1_t weight = __riscv_vle32_v_f32m1(weights + i, vl);

  // Match the scalar iterator contract: finite checks cover point fields and
  // target normals, while non-finite weights remain observable.
  keep = finitePointToPlaneLLSWeightedF32M1(sx, vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(sy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(sz, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(dx, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(dy, vl), vl);
  keep = __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(dz, vl), vl);
  keep =
      __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(normal_x, vl), vl);
  keep =
      __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(normal_y, vl), vl);
  keep =
      __riscv_vmand_mm_b32(keep, finitePointToPlaneLLSWeightedF32M1(normal_z, vl), vl);

  nx = __riscv_vfmul_vv_f32m1(normal_x, weight, vl);
  ny = __riscv_vfmul_vv_f32m1(normal_y, weight, vl);
  nz = __riscv_vfmul_vv_f32m1(normal_z, weight, vl);

  // Keep this vfmsac form tied to scalar correctness tests; changing operand
  // order by visual sign inspection flips the solved rotation terms.
  a = __riscv_vfmul_vv_f32m1(ny, sz, vl);
  b = __riscv_vfmul_vv_f32m1(nz, sx, vl);
  c = __riscv_vfmul_vv_f32m1(nx, sy, vl);
  const vfloat32m1_t dsx = __riscv_vfsub_vv_f32m1(dx, sx, vl);
  const vfloat32m1_t dsy = __riscv_vfsub_vv_f32m1(dy, sy, vl);
  const vfloat32m1_t dsz = __riscv_vfsub_vv_f32m1(dz, sz, vl);

  a = __riscv_vfmsac_vv_f32m1(a, nz, sy, vl);
  b = __riscv_vfmsac_vv_f32m1(b, nx, sz, vl);
  c = __riscv_vfmsac_vv_f32m1(c, ny, sx, vl);
  d = __riscv_vfmul_vv_f32m1(nx, dsx, vl);
  d = __riscv_vfmacc_vv_f32m1(d, ny, dsy, vl);
  d = __riscv_vfmacc_vv_f32m1(d, nz, dsz, vl);

  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  a = __riscv_vmerge_vvm_f32m1(zero, a, keep, vl);
  b = __riscv_vmerge_vvm_f32m1(zero, b, keep, vl);
  c = __riscv_vmerge_vvm_f32m1(zero, c, keep, vl);
  d = __riscv_vmerge_vvm_f32m1(zero, d, keep, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, keep, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, keep, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, keep, vl);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedBlockGroupA(const std::uint8_t* source_base,
                                             const std::uint8_t* target_base,
                                             const float* weights,
                                             const std::size_t begin,
                                             const std::size_t end,
                                             PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
  vfloat32m1_t ad = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedFullReductionVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    eq.accepted_points += __riscv_vcpop_m_b32(keep, vl);
    aa = __riscv_vfmacc_vv_f32m1_tu(aa, a, a, vl);
    ab = __riscv_vfmacc_vv_f32m1_tu(ab, a, b, vl);
    ac = __riscv_vfmacc_vv_f32m1_tu(ac, a, c, vl);
    anx = __riscv_vfmacc_vv_f32m1_tu(anx, a, nx, vl);
    any = __riscv_vfmacc_vv_f32m1_tu(any, a, ny, vl);
    anz = __riscv_vfmacc_vv_f32m1_tu(anz, a, nz, vl);
    ad = __riscv_vfmacc_vv_f32m1_tu(ad, a, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(0) += reducePointToPlaneLLSWeightedSumF32M1(aa, vlmax);
  eq.ata.coeffRef(1) += reducePointToPlaneLLSWeightedSumF32M1(ab, vlmax);
  eq.ata.coeffRef(2) += reducePointToPlaneLLSWeightedSumF32M1(ac, vlmax);
  eq.ata.coeffRef(3) += reducePointToPlaneLLSWeightedSumF32M1(anx, vlmax);
  eq.ata.coeffRef(4) += reducePointToPlaneLLSWeightedSumF32M1(any, vlmax);
  eq.ata.coeffRef(5) += reducePointToPlaneLLSWeightedSumF32M1(anz, vlmax);
  eq.atb.coeffRef(0) += reducePointToPlaneLLSWeightedSumF32M1(ad, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedBlockGroupB(const std::uint8_t* source_base,
                                             const std::uint8_t* target_base,
                                             const float* weights,
                                             const std::size_t begin,
                                             const std::size_t end,
                                             PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero, bd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedFullReductionVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    bb = __riscv_vfmacc_vv_f32m1_tu(bb, b, b, vl);
    bc = __riscv_vfmacc_vv_f32m1_tu(bc, b, c, vl);
    bnx = __riscv_vfmacc_vv_f32m1_tu(bnx, b, nx, vl);
    bny = __riscv_vfmacc_vv_f32m1_tu(bny, b, ny, vl);
    bnz = __riscv_vfmacc_vv_f32m1_tu(bnz, b, nz, vl);
    bd = __riscv_vfmacc_vv_f32m1_tu(bd, b, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(7) += reducePointToPlaneLLSWeightedSumF32M1(bb, vlmax);
  eq.ata.coeffRef(8) += reducePointToPlaneLLSWeightedSumF32M1(bc, vlmax);
  eq.ata.coeffRef(9) += reducePointToPlaneLLSWeightedSumF32M1(bnx, vlmax);
  eq.ata.coeffRef(10) += reducePointToPlaneLLSWeightedSumF32M1(bny, vlmax);
  eq.ata.coeffRef(11) += reducePointToPlaneLLSWeightedSumF32M1(bnz, vlmax);
  eq.atb.coeffRef(1) += reducePointToPlaneLLSWeightedSumF32M1(bd, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedBlockGroupC(const std::uint8_t* source_base,
                                             const std::uint8_t* target_base,
                                             const float* weights,
                                             const std::size_t begin,
                                             const std::size_t end,
                                             PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero, cd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedFullReductionVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    cc = __riscv_vfmacc_vv_f32m1_tu(cc, c, c, vl);
    cnx = __riscv_vfmacc_vv_f32m1_tu(cnx, c, nx, vl);
    cny = __riscv_vfmacc_vv_f32m1_tu(cny, c, ny, vl);
    cnz = __riscv_vfmacc_vv_f32m1_tu(cnz, c, nz, vl);
    cd = __riscv_vfmacc_vv_f32m1_tu(cd, c, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(14) += reducePointToPlaneLLSWeightedSumF32M1(cc, vlmax);
  eq.ata.coeffRef(15) += reducePointToPlaneLLSWeightedSumF32M1(cnx, vlmax);
  eq.ata.coeffRef(16) += reducePointToPlaneLLSWeightedSumF32M1(cny, vlmax);
  eq.ata.coeffRef(17) += reducePointToPlaneLLSWeightedSumF32M1(cnz, vlmax);
  eq.atb.coeffRef(2) += reducePointToPlaneLLSWeightedSumF32M1(cd, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedBlockGroupN(const std::uint8_t* source_base,
                                             const std::uint8_t* target_base,
                                             const float* weights,
                                             const std::size_t begin,
                                             const std::size_t end,
                                             PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t nxnx = zero, nxny = zero, nxnz = zero;
  vfloat32m1_t nyny = zero, nynz = zero, nznz = zero;
  vfloat32m1_t nxd = zero, nyd = zero, nzd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedFullReductionVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    nxnx = __riscv_vfmacc_vv_f32m1_tu(nxnx, nx, nx, vl);
    nxny = __riscv_vfmacc_vv_f32m1_tu(nxny, nx, ny, vl);
    nxnz = __riscv_vfmacc_vv_f32m1_tu(nxnz, nx, nz, vl);
    nyny = __riscv_vfmacc_vv_f32m1_tu(nyny, ny, ny, vl);
    nynz = __riscv_vfmacc_vv_f32m1_tu(nynz, ny, nz, vl);
    nznz = __riscv_vfmacc_vv_f32m1_tu(nznz, nz, nz, vl);
    nxd = __riscv_vfmacc_vv_f32m1_tu(nxd, nx, d, vl);
    nyd = __riscv_vfmacc_vv_f32m1_tu(nyd, ny, d, vl);
    nzd = __riscv_vfmacc_vv_f32m1_tu(nzd, nz, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(21) += reducePointToPlaneLLSWeightedSumF32M1(nxnx, vlmax);
  eq.ata.coeffRef(22) += reducePointToPlaneLLSWeightedSumF32M1(nxny, vlmax);
  eq.ata.coeffRef(23) += reducePointToPlaneLLSWeightedSumF32M1(nxnz, vlmax);
  eq.ata.coeffRef(28) += reducePointToPlaneLLSWeightedSumF32M1(nyny, vlmax);
  eq.ata.coeffRef(29) += reducePointToPlaneLLSWeightedSumF32M1(nynz, vlmax);
  eq.ata.coeffRef(35) += reducePointToPlaneLLSWeightedSumF32M1(nznz, vlmax);
  eq.atb.coeffRef(3) += reducePointToPlaneLLSWeightedSumF32M1(nxd, vlmax);
  eq.atb.coeffRef(4) += reducePointToPlaneLLSWeightedSumF32M1(nyd, vlmax);
  eq.atb.coeffRef(5) += reducePointToPlaneLLSWeightedSumF32M1(nzd, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupA(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t aa = zero, ab = zero, ac = zero, anx = zero, any = zero, anz = zero;
  vfloat32m1_t ad = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedSourceIndexedBlockVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, source_indices, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    eq.accepted_points += __riscv_vcpop_m_b32(keep, vl);
    aa = __riscv_vfmacc_vv_f32m1_tu(aa, a, a, vl);
    ab = __riscv_vfmacc_vv_f32m1_tu(ab, a, b, vl);
    ac = __riscv_vfmacc_vv_f32m1_tu(ac, a, c, vl);
    anx = __riscv_vfmacc_vv_f32m1_tu(anx, a, nx, vl);
    any = __riscv_vfmacc_vv_f32m1_tu(any, a, ny, vl);
    anz = __riscv_vfmacc_vv_f32m1_tu(anz, a, nz, vl);
    ad = __riscv_vfmacc_vv_f32m1_tu(ad, a, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(0) += reducePointToPlaneLLSWeightedSumF32M1(aa, vlmax);
  eq.ata.coeffRef(1) += reducePointToPlaneLLSWeightedSumF32M1(ab, vlmax);
  eq.ata.coeffRef(2) += reducePointToPlaneLLSWeightedSumF32M1(ac, vlmax);
  eq.ata.coeffRef(3) += reducePointToPlaneLLSWeightedSumF32M1(anx, vlmax);
  eq.ata.coeffRef(4) += reducePointToPlaneLLSWeightedSumF32M1(any, vlmax);
  eq.ata.coeffRef(5) += reducePointToPlaneLLSWeightedSumF32M1(anz, vlmax);
  eq.atb.coeffRef(0) += reducePointToPlaneLLSWeightedSumF32M1(ad, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupB(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t bb = zero, bc = zero, bnx = zero, bny = zero, bnz = zero, bd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedSourceIndexedBlockVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, source_indices, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    bb = __riscv_vfmacc_vv_f32m1_tu(bb, b, b, vl);
    bc = __riscv_vfmacc_vv_f32m1_tu(bc, b, c, vl);
    bnx = __riscv_vfmacc_vv_f32m1_tu(bnx, b, nx, vl);
    bny = __riscv_vfmacc_vv_f32m1_tu(bny, b, ny, vl);
    bnz = __riscv_vfmacc_vv_f32m1_tu(bnz, b, nz, vl);
    bd = __riscv_vfmacc_vv_f32m1_tu(bd, b, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(7) += reducePointToPlaneLLSWeightedSumF32M1(bb, vlmax);
  eq.ata.coeffRef(8) += reducePointToPlaneLLSWeightedSumF32M1(bc, vlmax);
  eq.ata.coeffRef(9) += reducePointToPlaneLLSWeightedSumF32M1(bnx, vlmax);
  eq.ata.coeffRef(10) += reducePointToPlaneLLSWeightedSumF32M1(bny, vlmax);
  eq.ata.coeffRef(11) += reducePointToPlaneLLSWeightedSumF32M1(bnz, vlmax);
  eq.atb.coeffRef(1) += reducePointToPlaneLLSWeightedSumF32M1(bd, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupC(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t cc = zero, cnx = zero, cny = zero, cnz = zero, cd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedSourceIndexedBlockVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, source_indices, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    cc = __riscv_vfmacc_vv_f32m1_tu(cc, c, c, vl);
    cnx = __riscv_vfmacc_vv_f32m1_tu(cnx, c, nx, vl);
    cny = __riscv_vfmacc_vv_f32m1_tu(cny, c, ny, vl);
    cnz = __riscv_vfmacc_vv_f32m1_tu(cnz, c, nz, vl);
    cd = __riscv_vfmacc_vv_f32m1_tu(cd, c, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(14) += reducePointToPlaneLLSWeightedSumF32M1(cc, vlmax);
  eq.ata.coeffRef(15) += reducePointToPlaneLLSWeightedSumF32M1(cnx, vlmax);
  eq.ata.coeffRef(16) += reducePointToPlaneLLSWeightedSumF32M1(cny, vlmax);
  eq.ata.coeffRef(17) += reducePointToPlaneLLSWeightedSumF32M1(cnz, vlmax);
  eq.atb.coeffRef(2) += reducePointToPlaneLLSWeightedSumF32M1(cd, vlmax);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline void
accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupN(
    const std::uint8_t* source_base,
    const std::uint32_t* source_indices,
    const std::uint8_t* target_base,
    const float* weights,
    const std::size_t begin,
    const std::size_t end,
    PointToPlaneLLSWeightedNormalEquation& eq)
{
  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vlmax);
  vfloat32m1_t nxnx = zero, nxny = zero, nxnz = zero;
  vfloat32m1_t nyny = zero, nynz = zero, nznz = zero;
  vfloat32m1_t nxd = zero, nyd = zero, nzd = zero;
  for (std::size_t i = begin; i < end;) {
    const std::size_t vl = __riscv_vsetvl_e32m1(end - i);
    vfloat32m1_t a, b, c, d, nx, ny, nz;
    vbool32_t keep;
    loadPointToPlaneLLSWeightedSourceIndexedBlockVectors<
        PointSource,
        PointTarget,
        SrcLayout,
        TgtLayout>(
        source_base, source_indices, target_base, weights, i, vl, a, b, c, d, nx, ny, nz, keep);
    nxnx = __riscv_vfmacc_vv_f32m1_tu(nxnx, nx, nx, vl);
    nxny = __riscv_vfmacc_vv_f32m1_tu(nxny, nx, ny, vl);
    nxnz = __riscv_vfmacc_vv_f32m1_tu(nxnz, nx, nz, vl);
    nyny = __riscv_vfmacc_vv_f32m1_tu(nyny, ny, ny, vl);
    nynz = __riscv_vfmacc_vv_f32m1_tu(nynz, ny, nz, vl);
    nznz = __riscv_vfmacc_vv_f32m1_tu(nznz, nz, nz, vl);
    nxd = __riscv_vfmacc_vv_f32m1_tu(nxd, nx, d, vl);
    nyd = __riscv_vfmacc_vv_f32m1_tu(nyd, ny, d, vl);
    nzd = __riscv_vfmacc_vv_f32m1_tu(nzd, nz, d, vl);
    i += vl;
  }
  eq.ata.coeffRef(21) += reducePointToPlaneLLSWeightedSumF32M1(nxnx, vlmax);
  eq.ata.coeffRef(22) += reducePointToPlaneLLSWeightedSumF32M1(nxny, vlmax);
  eq.ata.coeffRef(23) += reducePointToPlaneLLSWeightedSumF32M1(nxnz, vlmax);
  eq.ata.coeffRef(28) += reducePointToPlaneLLSWeightedSumF32M1(nyny, vlmax);
  eq.ata.coeffRef(29) += reducePointToPlaneLLSWeightedSumF32M1(nynz, vlmax);
  eq.ata.coeffRef(35) += reducePointToPlaneLLSWeightedSumF32M1(nznz, vlmax);
  eq.atb.coeffRef(3) += reducePointToPlaneLLSWeightedSumF32M1(nxd, vlmax);
  eq.atb.coeffRef(4) += reducePointToPlaneLLSWeightedSumF32M1(nyd, vlmax);
  eq.atb.coeffRef(5) += reducePointToPlaneLLSWeightedSumF32M1(nzd, vlmax);
}

template <typename PointSource, typename PointTarget>
inline bool
buildPointToPlaneLLSWeightedFullCloudBlockRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedNormalEquation& eq,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>;

  const std::size_t nr_points = cloud_src.size();
  if constexpr (!SrcLayout::value || !TgtLayout::value) {
    setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
    return false;
  }
  else {
    if (!canUsePointToPlaneLLSWeightedFullCloudRVV<PointSource, PointTarget>(
            nr_points, cloud_tgt.size(), weights.size(), __riscv_vsetvlmax_e32m1())) {
      setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
      return false;
    }

    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows =
        std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    eq = PointToPlaneLLSWeightedNormalEquation{};
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
    for (std::size_t begin = 0; begin < nr_points; begin += block_rows) {
      const std::size_t end = std::min(nr_points, begin + block_rows);
      accumulatePointToPlaneLLSWeightedBlockGroupA<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulatePointToPlaneLLSWeightedBlockGroupB<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulatePointToPlaneLLSWeightedBlockGroupC<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, target_base, weights.data(), begin, end, eq);
      accumulatePointToPlaneLLSWeightedBlockGroupN<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, target_base, weights.data(), begin, end, eq);
    }
    if (stats) {
      stats->input_points = nr_points;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return true;
  }
}

template <typename PointSource, typename PointTarget>
inline bool
buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedNormalEquation& eq,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>;

  const std::size_t nr_points = indices_src.size();
  if constexpr (!SrcLayout::value || !TgtLayout::value) {
    setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
    return false;
  }
  else {
    if (!canUsePointToPlaneLLSWeightedSourceIndicesRVV<PointSource, PointTarget>(
            cloud_src.size(),
            nr_points,
            cloud_tgt.size(),
            weights.size(),
            __riscv_vsetvlmax_e32m1())) {
      setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
      return false;
    }

    std::vector<std::uint32_t> source_indices;
    source_indices.reserve(nr_points);
    for (std::size_t row = 0; row < nr_points; ++row) {
      if (indices_src[row] < 0) {
        setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
        return false;
      }
      const auto source_index = static_cast<std::size_t>(indices_src[row]);
      if (source_index >= cloud_src.size()) {
        setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
        return false;
      }
      source_indices.push_back(static_cast<std::uint32_t>(source_index));
    }

    constexpr std::size_t kBlockChunks = 8;
    const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
    const std::size_t block_rows =
        std::max<std::size_t>(vlmax, vlmax * kBlockChunks);
    eq = PointToPlaneLLSWeightedNormalEquation{};
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
    for (std::size_t begin = 0; begin < nr_points; begin += block_rows) {
      const std::size_t end = std::min(nr_points, begin + block_rows);
      accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupA<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, source_indices.data(), target_base, weights.data(), begin, end, eq);
      accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupB<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, source_indices.data(), target_base, weights.data(), begin, end, eq);
      accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupC<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, source_indices.data(), target_base, weights.data(), begin, end, eq);
      accumulatePointToPlaneLLSWeightedSourceIndexedBlockGroupN<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(
          source_base, source_indices.data(), target_base, weights.data(), begin, end, eq);
    }
    setPointToPlaneLLSWeightedStats(stats, nr_points, eq.accepted_points, true);
    return true;
  }
}

template <typename PointSource, typename PointTarget>
inline bool
buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedNormalEquation& eq,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>;

  const std::size_t nr_points = indices_src.size();
  if constexpr (!SrcLayout::value || !TgtLayout::value) {
    setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
    return false;
  }
  else {
    if (!canUsePointToPlaneLLSWeightedSourceIndicesRVV<PointSource, PointTarget>(
            cloud_src.size(),
            nr_points,
            cloud_tgt.size(),
            weights.size(),
            __riscv_vsetvlmax_e32m2())) {
      setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
      return false;
    }

    std::vector<std::uint32_t> source_indices;
    source_indices.reserve(nr_points);
    for (std::size_t row = 0; row < nr_points; ++row) {
      if (indices_src[row] < 0) {
        setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
        return false;
      }
      const auto source_index = static_cast<std::size_t>(indices_src[row]);
      if (source_index >= cloud_src.size()) {
        setPointToPlaneLLSWeightedStats(stats, nr_points, 0, false);
        return false;
      }
      source_indices.push_back(static_cast<std::uint32_t>(source_index));
    }

    eq = PointToPlaneLLSWeightedNormalEquation{};
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
    for (std::size_t i = 0; i < nr_points;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
      vfloat32m2_t a, b, c, d, nx, ny, nz;
      vbool16_t keep;
      loadPointToPlaneLLSWeightedSourceIndexedVectors<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(source_base,
                     source_indices.data(),
                     target_base,
                     weights.data(),
                     i,
                     vl,
                     a,
                     b,
                     c,
                     d,
                     nx,
                     ny,
                     nz,
                     keep);
      accumulatePointToPlaneLLSWeightedCompressedRowsF32M2(
          a, b, c, d, nx, ny, nz, keep, vl, eq);
      i += vl;
    }
    setPointToPlaneLLSWeightedStats(stats, nr_points, eq.accepted_points, true);
    return true;
  }
}
#endif // __RVV10__

template <typename PointSource, typename PointTarget>
inline PointToPlaneLLSWeightedNormalEquation
buildPointToPlaneLLSWeightedFullCloudDefault(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
#if defined(__RVV10__)
  PointToPlaneLLSWeightedNormalEquation eq;
  if (buildPointToPlaneLLSWeightedFullCloudBlockRVV(cloud_src, cloud_tgt, weights, eq, stats))
    return eq;
#endif
  return buildPointToPlaneLLSWeightedFullCloudStd(cloud_src, cloud_tgt, weights, stats);
}

template <typename PointSource, typename PointTarget>
inline PointToPlaneLLSWeightedNormalEquation
buildPointToPlaneLLSWeightedSourceIndicesDefault(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    PointToPlaneLLSWeightedFullCloudStats* stats = nullptr)
{
#if defined(__RVV10__)
  PointToPlaneLLSWeightedNormalEquation eq;
  if (buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(
          cloud_src, indices_src, cloud_tgt, weights, eq, stats))
    return eq;
  if (buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
          cloud_src, indices_src, cloud_tgt, weights, eq, stats))
    return eq;
#endif
  return buildPointToPlaneLLSWeightedSourceIndicesStd(
      cloud_src, indices_src, cloud_tgt, weights, stats);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimatePointToPlaneLLSWeightedFullCloudRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
#if defined(__RVV10__)
  if constexpr (std::is_same_v<Scalar, float>) {
    PointToPlaneLLSWeightedNormalEquation eq;
    if (!buildPointToPlaneLLSWeightedFullCloudBlockRVV(cloud_src, cloud_tgt, weights, eq))
      return false;
    solvePointToPlaneLLSWeightedNormalEquation(eq, transformation_matrix);
    return true;
  }
  return false;
#else
  (void)cloud_src;
  (void)cloud_tgt;
  (void)weights;
  (void)transformation_matrix;
  return false;
#endif
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimatePointToPlaneLLSWeightedSourceIndicesRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::vector<float>& weights,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
#if defined(__RVV10__)
  if constexpr (std::is_same_v<Scalar, float>) {
    PointToPlaneLLSWeightedNormalEquation eq;
    if (!buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(
            cloud_src, indices_src, cloud_tgt, weights, eq) &&
        !buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
            cloud_src, indices_src, cloud_tgt, weights, eq))
      return false;
    solvePointToPlaneLLSWeightedNormalEquation(eq, transformation_matrix);
    return true;
  }
  return false;
#else
  (void)cloud_src;
  (void)indices_src;
  (void)cloud_tgt;
  (void)weights;
  (void)transformation_matrix;
  return false;
#endif
}

} // namespace detail

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationPointToPlaneLLSWeighted<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = cloud_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationPointToPlaneLLSWeighted::"
              "estimateRigidTransformation] Number or points in source (%zu) differs "
              "than target (%zu)!\n",
              static_cast<std::size_t>(nr_points),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

  if (weights_.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationPointToPlaneLLSWeighted::"
              "estimateRigidTransformation] Number or weights from the number of "
              "correspondences! Use setWeights () to set them.\n");
    return;
  }

#if defined(__RVV10__)
  // The weighted fast path is limited to full-cloud f32 AoS rows with
  // contiguous weights; layout, size, VLEN, and Scalar misses fall back here.
  if constexpr (std::is_same_v<Scalar, float>) {
    if (detail::estimatePointToPlaneLLSWeightedFullCloudRVV<
            PointSource,
            PointTarget,
            Scalar>(cloud_src, cloud_tgt, weights_, transformation_matrix))
      return;
  }
#endif // defined(__RVV10__)

  ConstCloudIterator<PointSource> source_it(cloud_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  typename std::vector<Scalar>::const_iterator weights_it = weights_.begin();
  estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationPointToPlaneLLSWeighted<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const std::size_t nr_points = indices_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationPointToPlaneLLSWeighted::"
              "estimateRigidTransformation] Number or points in source (%zu) differs "
              "than target (%zu)!\n",
              indices_src.size(),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

  if (weights_.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationPointToPlaneLLSWeighted::"
              "estimateRigidTransformation] Number or weights from the number of "
              "correspondences! Use setWeights () to set them.\n");
    return;
  }

#if defined(__RVV10__)
  // The indexed fast path is limited to valid source-index rows with contiguous
  // weights. It keeps target rows sequential and falls back to the original
  // iterator path on layout, size, VLEN, Scalar, or index-gate misses.
  if constexpr (std::is_same_v<Scalar, float>) {
    if (detail::estimatePointToPlaneLLSWeightedSourceIndicesRVV<
            PointSource,
            PointTarget,
            Scalar>(cloud_src, indices_src, cloud_tgt, weights_, transformation_matrix))
      return;
  }
#endif // defined(__RVV10__)

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  typename std::vector<Scalar>::const_iterator weights_it = weights_.begin();
  estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationPointToPlaneLLSWeighted<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Indices& indices_tgt,
                                Matrix4& transformation_matrix) const
{
  const std::size_t nr_points = indices_src.size();
  if (indices_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationPointToPlaneLLSWeighted::"
              "estimateRigidTransformation] Number or points in source (%lu) differs "
              "than target (%lu)!\n",
              indices_src.size(),
              indices_tgt.size());
    return;
  }

  if (weights_.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationPointToPlaneLLSWeighted::"
              "estimateRigidTransformation] Number or weights from the number of "
              "correspondences! Use setWeights () to set them.\n");
    return;
  }

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
  typename std::vector<Scalar>::const_iterator weights_it = weights_.begin();
  estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationPointToPlaneLLSWeighted<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Correspondences& correspondences,
                                Matrix4& transformation_matrix) const
{
  ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
  std::vector<Scalar> weights(correspondences.size());
  for (std::size_t i = 0; i < correspondences.size(); ++i)
    weights[i] = correspondences[i].weight;
  typename std::vector<Scalar>::const_iterator weights_it = weights.begin();
  estimateRigidTransformation(source_it, target_it, weights_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationPointToPlaneLLSWeighted<PointSource, PointTarget, Scalar>::
    constructTransformationMatrix(const double& alpha,
                                  const double& beta,
                                  const double& gamma,
                                  const double& tx,
                                  const double& ty,
                                  const double& tz,
                                  Matrix4& transformation_matrix) const
{
  // Construct the transformation matrix from rotation and translation
  transformation_matrix = Eigen::Matrix<Scalar, 4, 4>::Zero();
  transformation_matrix(0, 0) = static_cast<Scalar>(std::cos(gamma) * std::cos(beta));
  transformation_matrix(0, 1) = static_cast<Scalar>(
      -sin(gamma) * std::cos(alpha) + std::cos(gamma) * sin(beta) * sin(alpha));
  transformation_matrix(0, 2) = static_cast<Scalar>(
      sin(gamma) * sin(alpha) + std::cos(gamma) * sin(beta) * std::cos(alpha));
  transformation_matrix(1, 0) = static_cast<Scalar>(sin(gamma) * std::cos(beta));
  transformation_matrix(1, 1) = static_cast<Scalar>(
      std::cos(gamma) * std::cos(alpha) + sin(gamma) * sin(beta) * sin(alpha));
  transformation_matrix(1, 2) = static_cast<Scalar>(
      -std::cos(gamma) * sin(alpha) + sin(gamma) * sin(beta) * std::cos(alpha));
  transformation_matrix(2, 0) = static_cast<Scalar>(-sin(beta));
  transformation_matrix(2, 1) = static_cast<Scalar>(std::cos(beta) * sin(alpha));
  transformation_matrix(2, 2) = static_cast<Scalar>(std::cos(beta) * std::cos(alpha));

  transformation_matrix(0, 3) = static_cast<Scalar>(tx);
  transformation_matrix(1, 3) = static_cast<Scalar>(ty);
  transformation_matrix(2, 3) = static_cast<Scalar>(tz);
  transformation_matrix(3, 3) = static_cast<Scalar>(1);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationPointToPlaneLLSWeighted<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(
        ConstCloudIterator<PointSource>& source_it,
        ConstCloudIterator<PointTarget>& target_it,
        typename std::vector<Scalar>::const_iterator& weights_it,
        Matrix4& transformation_matrix) const
{
  using Vector6d = Eigen::Matrix<double, 6, 1>;
  using Matrix6d = Eigen::Matrix<double, 6, 6>;

  Matrix6d ATA;
  Vector6d ATb;
  ATA.setZero();
  ATb.setZero();

  while (source_it.isValid() && target_it.isValid()) {
    if (!std::isfinite(source_it->x) || !std::isfinite(source_it->y) ||
        !std::isfinite(source_it->z) || !std::isfinite(target_it->x) ||
        !std::isfinite(target_it->y) || !std::isfinite(target_it->z) ||
        !std::isfinite(target_it->normal_x) || !std::isfinite(target_it->normal_y) ||
        !std::isfinite(target_it->normal_z)) {
      ++source_it;
      ++target_it;
      ++weights_it;
      continue;
    }

    const float& sx = source_it->x;
    const float& sy = source_it->y;
    const float& sz = source_it->z;
    const float& dx = target_it->x;
    const float& dy = target_it->y;
    const float& dz = target_it->z;
    const float& nx = target_it->normal[0] * (*weights_it);
    const float& ny = target_it->normal[1] * (*weights_it);
    const float& nz = target_it->normal[2] * (*weights_it);

    double a = nz * sy - ny * sz;
    double b = nx * sz - nz * sx;
    double c = ny * sx - nx * sy;

    //    0  1  2  3  4  5
    //    6  7  8  9 10 11
    //   12 13 14 15 16 17
    //   18 19 20 21 22 23
    //   24 25 26 27 28 29
    //   30 31 32 33 34 35

    ATA.coeffRef(0) += a * a;
    ATA.coeffRef(1) += a * b;
    ATA.coeffRef(2) += a * c;
    ATA.coeffRef(3) += a * nx;
    ATA.coeffRef(4) += a * ny;
    ATA.coeffRef(5) += a * nz;
    ATA.coeffRef(7) += b * b;
    ATA.coeffRef(8) += b * c;
    ATA.coeffRef(9) += b * nx;
    ATA.coeffRef(10) += b * ny;
    ATA.coeffRef(11) += b * nz;
    ATA.coeffRef(14) += c * c;
    ATA.coeffRef(15) += c * nx;
    ATA.coeffRef(16) += c * ny;
    ATA.coeffRef(17) += c * nz;
    ATA.coeffRef(21) += nx * nx;
    ATA.coeffRef(22) += nx * ny;
    ATA.coeffRef(23) += nx * nz;
    ATA.coeffRef(28) += ny * ny;
    ATA.coeffRef(29) += ny * nz;
    ATA.coeffRef(35) += nz * nz;

    double d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz;
    ATb.coeffRef(0) += a * d;
    ATb.coeffRef(1) += b * d;
    ATb.coeffRef(2) += c * d;
    ATb.coeffRef(3) += nx * d;
    ATb.coeffRef(4) += ny * d;
    ATb.coeffRef(5) += nz * d;

    ++source_it;
    ++target_it;
    ++weights_it;
  }

  ATA.coeffRef(6) = ATA.coeff(1);
  ATA.coeffRef(12) = ATA.coeff(2);
  ATA.coeffRef(13) = ATA.coeff(8);
  ATA.coeffRef(18) = ATA.coeff(3);
  ATA.coeffRef(19) = ATA.coeff(9);
  ATA.coeffRef(20) = ATA.coeff(15);
  ATA.coeffRef(24) = ATA.coeff(4);
  ATA.coeffRef(25) = ATA.coeff(10);
  ATA.coeffRef(26) = ATA.coeff(16);
  ATA.coeffRef(27) = ATA.coeff(22);
  ATA.coeffRef(30) = ATA.coeff(5);
  ATA.coeffRef(31) = ATA.coeff(11);
  ATA.coeffRef(32) = ATA.coeff(17);
  ATA.coeffRef(33) = ATA.coeff(23);
  ATA.coeffRef(34) = ATA.coeff(29);

  // Solve A*x = b
  Vector6d x = static_cast<Vector6d>(ATA.inverse() * ATb);

  // Construct the transformation matrix from x
  constructTransformationMatrix(
      x(0), x(1), x(2), x(3), x(4), x(5), transformation_matrix);
}

} // namespace registration
} // namespace pcl

#endif /* PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_POINT_TO_PLANE_LLS_WEIGHTED_HPP_  \
        */
