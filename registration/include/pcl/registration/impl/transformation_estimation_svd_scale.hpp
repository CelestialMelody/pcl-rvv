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

#ifndef PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_SVD_SCALE_HPP_
#define PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_SVD_SCALE_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <pcl/point_types.h>
#include <riscv_vector.h>
#endif

namespace pcl {

namespace registration {

#ifdef __RVV10__
namespace detail {

struct TransformationEstimationSVDScaleF32Accumulation {
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

struct TransformationEstimationSVDScaleD64Accumulation {
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

struct TransformationEstimationSVDScaleContiguousRange {
  bool valid{false};
  std::size_t offset{0};
  std::size_t count{0};
};

struct TransformationEstimationSVDScaleContiguousCorrespondenceRange {
  bool valid{false};
  std::size_t query_offset{0};
  std::size_t match_offset{0};
  std::size_t count{0};
};

inline float
reduceTransformationEstimationSVDScaleF32M2(const vfloat32m2_t value,
                                            const std::size_t vlmax)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return __riscv_vfmv_f_s_f32m1_f32(
      __riscv_vfredosum_vs_f32m2_f32m1(value, zero, vlmax));
}

inline vfloat32mf2_t
stridedLoadTransformationEstimationSVDScaleF32MF2(const std::uint8_t* base,
                                                  const std::size_t field_offset,
                                                  const std::ptrdiff_t stride,
                                                  const std::size_t vl)
{
  return __riscv_vlse32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), stride, vl);
}

inline vfloat32mf2_t
indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
    const std::uint8_t* base,
    const std::size_t field_offset,
    const vuint32mf2_t byte_offsets,
    const std::size_t vl)
{
  return __riscv_vluxei32_v_f32mf2(
      reinterpret_cast<const float*>(base + field_offset), byte_offsets, vl);
}

inline vfloat64m1_t
widenTransformationEstimationSVDScaleF64(const vfloat32mf2_t value,
                                         const std::size_t vl)
{
  return __riscv_vfwcvt_f_f_v_f64m1(value, vl);
}

inline vfloat64m1_t
addTransformationEstimationSVDScaleF64(const vfloat64m1_t acc,
                                       const vfloat64m1_t term,
                                       const std::size_t vl)
{
  return __riscv_vfadd_vv_f64m1_tu(acc, acc, term, vl);
}

inline vfloat64m1_t
mulTransformationEstimationSVDScaleF64(const vfloat64m1_t lhs,
                                       const vfloat64m1_t rhs,
                                       const std::size_t vl)
{
  return __riscv_vfmul_vv_f64m1(lhs, rhs, vl);
}

inline double
reduceTransformationEstimationSVDScaleF64M1(const vfloat64m1_t value,
                                            const std::size_t vlmax)
{
  const vfloat64m1_t zero = __riscv_vfmv_s_f_f64m1(0.0, 1);
  return __riscv_vfmv_f_s_f64m1_f64(
      __riscv_vfredosum_vs_f64m1_f64m1(value, zero, vlmax));
}

inline void
accumulateTransformationEstimationSVDScaleD64Lanes(const vfloat64m1_t sx,
                                                   const vfloat64m1_t sy,
                                                   const vfloat64m1_t sz,
                                                   const vfloat64m1_t tx,
                                                   const vfloat64m1_t ty,
                                                   const vfloat64m1_t tz,
                                                   const std::size_t vl,
                                                   vfloat64m1_t& ssx,
                                                   vfloat64m1_t& ssy,
                                                   vfloat64m1_t& ssz,
                                                   vfloat64m1_t& stx,
                                                   vfloat64m1_t& sty,
                                                   vfloat64m1_t& stz,
                                                   vfloat64m1_t& c00,
                                                   vfloat64m1_t& c01,
                                                   vfloat64m1_t& c02,
                                                   vfloat64m1_t& c10,
                                                   vfloat64m1_t& c11,
                                                   vfloat64m1_t& c12,
                                                   vfloat64m1_t& c20,
                                                   vfloat64m1_t& c21,
                                                   vfloat64m1_t& c22,
                                                   vfloat64m1_t& source_square)
{
  ssx = addTransformationEstimationSVDScaleF64(ssx, sx, vl);
  ssy = addTransformationEstimationSVDScaleF64(ssy, sy, vl);
  ssz = addTransformationEstimationSVDScaleF64(ssz, sz, vl);
  stx = addTransformationEstimationSVDScaleF64(stx, tx, vl);
  sty = addTransformationEstimationSVDScaleF64(sty, ty, vl);
  stz = addTransformationEstimationSVDScaleF64(stz, tz, vl);
  c00 = addTransformationEstimationSVDScaleF64(
      c00, mulTransformationEstimationSVDScaleF64(sx, tx, vl), vl);
  c01 = addTransformationEstimationSVDScaleF64(
      c01, mulTransformationEstimationSVDScaleF64(sx, ty, vl), vl);
  c02 = addTransformationEstimationSVDScaleF64(
      c02, mulTransformationEstimationSVDScaleF64(sx, tz, vl), vl);
  c10 = addTransformationEstimationSVDScaleF64(
      c10, mulTransformationEstimationSVDScaleF64(sy, tx, vl), vl);
  c11 = addTransformationEstimationSVDScaleF64(
      c11, mulTransformationEstimationSVDScaleF64(sy, ty, vl), vl);
  c12 = addTransformationEstimationSVDScaleF64(
      c12, mulTransformationEstimationSVDScaleF64(sy, tz, vl), vl);
  c20 = addTransformationEstimationSVDScaleF64(
      c20, mulTransformationEstimationSVDScaleF64(sz, tx, vl), vl);
  c21 = addTransformationEstimationSVDScaleF64(
      c21, mulTransformationEstimationSVDScaleF64(sz, ty, vl), vl);
  c22 = addTransformationEstimationSVDScaleF64(
      c22, mulTransformationEstimationSVDScaleF64(sz, tz, vl), vl);
  source_square = addTransformationEstimationSVDScaleF64(
      source_square, mulTransformationEstimationSVDScaleF64(sx, sx, vl), vl);
  source_square = addTransformationEstimationSVDScaleF64(
      source_square, mulTransformationEstimationSVDScaleF64(sy, sy, vl), vl);
  source_square = addTransformationEstimationSVDScaleF64(
      source_square, mulTransformationEstimationSVDScaleF64(sz, sz, vl), vl);
}

inline void
accumulateTransformationEstimationSVDScaleF32Lanes(const vfloat32m2_t sx,
                                                   const vfloat32m2_t sy,
                                                   const vfloat32m2_t sz,
                                                   const vfloat32m2_t tx,
                                                   const vfloat32m2_t ty,
                                                   const vfloat32m2_t tz,
                                                   const std::size_t vl,
                                                   vfloat32m2_t& ssx,
                                                   vfloat32m2_t& ssy,
                                                   vfloat32m2_t& ssz,
                                                   vfloat32m2_t& stx,
                                                   vfloat32m2_t& sty,
                                                   vfloat32m2_t& stz,
                                                   vfloat32m2_t& c00,
                                                   vfloat32m2_t& c01,
                                                   vfloat32m2_t& c02,
                                                   vfloat32m2_t& c10,
                                                   vfloat32m2_t& c11,
                                                   vfloat32m2_t& c12,
                                                   vfloat32m2_t& c20,
                                                   vfloat32m2_t& c21,
                                                   vfloat32m2_t& c22,
                                                   vfloat32m2_t& source_square)
{
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
}

inline void
finishTransformationEstimationSVDScaleF32Accumulation(
    TransformationEstimationSVDScaleF32Accumulation& acc,
    const std::size_t vlmax,
    const vfloat32m2_t ssx,
    const vfloat32m2_t ssy,
    const vfloat32m2_t ssz,
    const vfloat32m2_t stx,
    const vfloat32m2_t sty,
    const vfloat32m2_t stz,
    const vfloat32m2_t c00,
    const vfloat32m2_t c01,
    const vfloat32m2_t c02,
    const vfloat32m2_t c10,
    const vfloat32m2_t c11,
    const vfloat32m2_t c12,
    const vfloat32m2_t c20,
    const vfloat32m2_t c21,
    const vfloat32m2_t c22,
    const vfloat32m2_t source_square)
{
  acc.source_sum[0] = reduceTransformationEstimationSVDScaleF32M2(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDScaleF32M2(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDScaleF32M2(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDScaleF32M2(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDScaleF32M2(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDScaleF32M2(stz, vlmax);
  acc.source_target_sum[0] =
      reduceTransformationEstimationSVDScaleF32M2(c00, vlmax);
  acc.source_target_sum[1] =
      reduceTransformationEstimationSVDScaleF32M2(c01, vlmax);
  acc.source_target_sum[2] =
      reduceTransformationEstimationSVDScaleF32M2(c02, vlmax);
  acc.source_target_sum[3] =
      reduceTransformationEstimationSVDScaleF32M2(c10, vlmax);
  acc.source_target_sum[4] =
      reduceTransformationEstimationSVDScaleF32M2(c11, vlmax);
  acc.source_target_sum[5] =
      reduceTransformationEstimationSVDScaleF32M2(c12, vlmax);
  acc.source_target_sum[6] =
      reduceTransformationEstimationSVDScaleF32M2(c20, vlmax);
  acc.source_target_sum[7] =
      reduceTransformationEstimationSVDScaleF32M2(c21, vlmax);
  acc.source_target_sum[8] =
      reduceTransformationEstimationSVDScaleF32M2(c22, vlmax);
  acc.source_square_sum =
      reduceTransformationEstimationSVDScaleF32M2(source_square, vlmax);
}

inline void
finishTransformationEstimationSVDScaleD64Accumulation(
    TransformationEstimationSVDScaleD64Accumulation& acc,
    const std::size_t vlmax,
    const vfloat64m1_t ssx,
    const vfloat64m1_t ssy,
    const vfloat64m1_t ssz,
    const vfloat64m1_t stx,
    const vfloat64m1_t sty,
    const vfloat64m1_t stz,
    const vfloat64m1_t c00,
    const vfloat64m1_t c01,
    const vfloat64m1_t c02,
    const vfloat64m1_t c10,
    const vfloat64m1_t c11,
    const vfloat64m1_t c12,
    const vfloat64m1_t c20,
    const vfloat64m1_t c21,
    const vfloat64m1_t c22,
    const vfloat64m1_t source_square)
{
  acc.source_sum[0] = reduceTransformationEstimationSVDScaleF64M1(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDScaleF64M1(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDScaleF64M1(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDScaleF64M1(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDScaleF64M1(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDScaleF64M1(stz, vlmax);
  acc.source_target_sum[0] =
      reduceTransformationEstimationSVDScaleF64M1(c00, vlmax);
  acc.source_target_sum[1] =
      reduceTransformationEstimationSVDScaleF64M1(c01, vlmax);
  acc.source_target_sum[2] =
      reduceTransformationEstimationSVDScaleF64M1(c02, vlmax);
  acc.source_target_sum[3] =
      reduceTransformationEstimationSVDScaleF64M1(c10, vlmax);
  acc.source_target_sum[4] =
      reduceTransformationEstimationSVDScaleF64M1(c11, vlmax);
  acc.source_target_sum[5] =
      reduceTransformationEstimationSVDScaleF64M1(c12, vlmax);
  acc.source_target_sum[6] =
      reduceTransformationEstimationSVDScaleF64M1(c20, vlmax);
  acc.source_target_sum[7] =
      reduceTransformationEstimationSVDScaleF64M1(c21, vlmax);
  acc.source_target_sum[8] =
      reduceTransformationEstimationSVDScaleF64M1(c22, vlmax);
  acc.source_square_sum =
      reduceTransformationEstimationSVDScaleF64M1(source_square, vlmax);
}

inline TransformationEstimationSVDScaleContiguousRange
transformationEstimationSVDScaleContiguousIndexRange(const pcl::Indices& indices,
                                                     const std::size_t cloud_size)
{
  TransformationEstimationSVDScaleContiguousRange range;
  if (indices.empty())
    return range;

  const pcl::index_t first = indices.front();
  if (first < 0)
    return range;

  for (std::size_t i = 1; i < indices.size(); ++i) {
    if (indices[i] != first + static_cast<pcl::index_t>(i))
      return range;
  }

  const std::size_t offset = static_cast<std::size_t>(first);
  if (offset > cloud_size || indices.size() > cloud_size - offset)
    return range;

  range.valid = true;
  range.offset = offset;
  range.count = indices.size();
  return range;
}

inline TransformationEstimationSVDScaleContiguousCorrespondenceRange
transformationEstimationSVDScaleContiguousCorrespondenceRange(
    const pcl::Correspondences& correspondences,
    const std::size_t source_size,
    const std::size_t target_size)
{
  TransformationEstimationSVDScaleContiguousCorrespondenceRange range;
  if (correspondences.empty())
    return range;

  const pcl::index_t first_query = correspondences.front().index_query;
  const pcl::index_t first_match = correspondences.front().index_match;
  if (first_query < 0 || first_match < 0)
    return range;

  for (std::size_t i = 1; i < correspondences.size(); ++i) {
    if (correspondences[i].index_query != first_query + static_cast<pcl::index_t>(i) ||
        correspondences[i].index_match != first_match + static_cast<pcl::index_t>(i)) {
      return range;
    }
  }

  const std::size_t query_offset = static_cast<std::size_t>(first_query);
  const std::size_t match_offset = static_cast<std::size_t>(first_match);
  if (query_offset > source_size || match_offset > target_size ||
      correspondences.size() > source_size - query_offset ||
      correspondences.size() > target_size - match_offset) {
    return range;
  }

  range.valid = true;
  range.query_offset = query_offset;
  range.match_offset = match_offset;
  range.count = correspondences.size();
  return range;
}

inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleOrderedPointXYZD64RVV(
    const pcl::PointCloud<pcl::PointXYZ>& cloud_src,
    const pcl::PointCloud<pcl::PointXYZ>& cloud_tgt)
{
  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = cloud_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const auto* src = source_base + i * sizeof(pcl::PointXYZ);
    const auto* tgt = target_base + i * sizeof(pcl::PointXYZ);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, offsetof(pcl::PointXYZ, x), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, offsetof(pcl::PointXYZ, y), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, offsetof(pcl::PointXYZ, z), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, x), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, y), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, z), sizeof(pcl::PointXYZ), vl),
        vl);

    ssx = addTransformationEstimationSVDScaleF64(ssx, sx, vl);
    ssy = addTransformationEstimationSVDScaleF64(ssy, sy, vl);
    ssz = addTransformationEstimationSVDScaleF64(ssz, sz, vl);
    stx = addTransformationEstimationSVDScaleF64(stx, tx, vl);
    sty = addTransformationEstimationSVDScaleF64(sty, ty, vl);
    stz = addTransformationEstimationSVDScaleF64(stz, tz, vl);
    c00 = addTransformationEstimationSVDScaleF64(
        c00, mulTransformationEstimationSVDScaleF64(sx, tx, vl), vl);
    c01 = addTransformationEstimationSVDScaleF64(
        c01, mulTransformationEstimationSVDScaleF64(sx, ty, vl), vl);
    c02 = addTransformationEstimationSVDScaleF64(
        c02, mulTransformationEstimationSVDScaleF64(sx, tz, vl), vl);
    c10 = addTransformationEstimationSVDScaleF64(
        c10, mulTransformationEstimationSVDScaleF64(sy, tx, vl), vl);
    c11 = addTransformationEstimationSVDScaleF64(
        c11, mulTransformationEstimationSVDScaleF64(sy, ty, vl), vl);
    c12 = addTransformationEstimationSVDScaleF64(
        c12, mulTransformationEstimationSVDScaleF64(sy, tz, vl), vl);
    c20 = addTransformationEstimationSVDScaleF64(
        c20, mulTransformationEstimationSVDScaleF64(sz, tx, vl), vl);
    c21 = addTransformationEstimationSVDScaleF64(
        c21, mulTransformationEstimationSVDScaleF64(sz, ty, vl), vl);
    c22 = addTransformationEstimationSVDScaleF64(
        c22, mulTransformationEstimationSVDScaleF64(sz, tz, vl), vl);
    source_square = addTransformationEstimationSVDScaleF64(
        source_square, mulTransformationEstimationSVDScaleF64(sx, sx, vl), vl);
    source_square = addTransformationEstimationSVDScaleF64(
        source_square, mulTransformationEstimationSVDScaleF64(sy, sy, vl), vl);
    source_square = addTransformationEstimationSVDScaleF64(
        source_square, mulTransformationEstimationSVDScaleF64(sz, sz, vl), vl);
    i += vl;
  }

  acc.source_sum[0] = reduceTransformationEstimationSVDScaleF64M1(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDScaleF64M1(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDScaleF64M1(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDScaleF64M1(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDScaleF64M1(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDScaleF64M1(stz, vlmax);
  acc.source_target_sum[0] =
      reduceTransformationEstimationSVDScaleF64M1(c00, vlmax);
  acc.source_target_sum[1] =
      reduceTransformationEstimationSVDScaleF64M1(c01, vlmax);
  acc.source_target_sum[2] =
      reduceTransformationEstimationSVDScaleF64M1(c02, vlmax);
  acc.source_target_sum[3] =
      reduceTransformationEstimationSVDScaleF64M1(c10, vlmax);
  acc.source_target_sum[4] =
      reduceTransformationEstimationSVDScaleF64M1(c11, vlmax);
  acc.source_target_sum[5] =
      reduceTransformationEstimationSVDScaleF64M1(c12, vlmax);
  acc.source_target_sum[6] =
      reduceTransformationEstimationSVDScaleF64M1(c20, vlmax);
  acc.source_target_sum[7] =
      reduceTransformationEstimationSVDScaleF64M1(c21, vlmax);
  acc.source_target_sum[8] =
      reduceTransformationEstimationSVDScaleF64M1(c22, vlmax);
  acc.source_square_sum =
      reduceTransformationEstimationSVDScaleF64M1(source_square, vlmax);
  return acc;
}

inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleContiguousOffsetPointXYZD64RVV(
    const pcl::PointCloud<pcl::PointXYZ>& cloud_src,
    const std::size_t source_offset,
    const pcl::PointCloud<pcl::PointXYZ>& cloud_tgt,
    const std::size_t target_offset,
    const std::size_t nr_points)
{
  TransformationEstimationSVDScaleD64Accumulation acc;
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(
      cloud_src.points.data() + source_offset);
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(
      cloud_tgt.points.data() + target_offset);

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const auto* src = source_base + i * sizeof(pcl::PointXYZ);
    const auto* tgt = target_base + i * sizeof(pcl::PointXYZ);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, offsetof(pcl::PointXYZ, x), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, offsetof(pcl::PointXYZ, y), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, offsetof(pcl::PointXYZ, z), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, x), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, y), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, z), sizeof(pcl::PointXYZ), vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleOrderedCloudPairD64RVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt)
{
  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = cloud_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const auto* src = source_base + i * sizeof(PointSource);
    const auto* tgt = target_base + i * sizeof(PointTarget);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, SrcLayout::kX, sizeof(PointSource), vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, SrcLayout::kY, sizeof(PointSource), vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, SrcLayout::kZ, sizeof(PointSource), vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kX, sizeof(PointTarget), vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kY, sizeof(PointTarget), vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kZ, sizeof(PointTarget), vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleContiguousOffsetPairD64RVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const std::size_t source_offset,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::size_t target_offset,
    const std::size_t nr_points)
{
  TransformationEstimationSVDScaleD64Accumulation acc;
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(
      cloud_src.points.data() + source_offset);
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(
      cloud_tgt.points.data() + target_offset);

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const auto* src = source_base + i * sizeof(PointSource);
    const auto* tgt = target_base + i * sizeof(PointTarget);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, SrcLayout::kX, sizeof(PointSource), vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, SrcLayout::kY, sizeof(PointSource), vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            src, SrcLayout::kZ, sizeof(PointSource), vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kX, sizeof(PointTarget), vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kY, sizeof(PointTarget), vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kZ, sizeof(PointTarget), vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleSourceIndexedCloudPairD64RVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "source-indexed SVD scale double RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* indices_i32 = reinterpret_cast<const std::int32_t*>(indices_src.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const vint32mf2_t v_idx_i32 = __riscv_vle32_v_i32mf2(indices_i32 + i, vl);
    const vuint32mf2_t v_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_idx_i32),
        static_cast<std::uint32_t>(sizeof(PointSource)),
        vl);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kX, v_off, vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kY, v_off, vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kZ, v_off, vl),
        vl);
    const auto* tgt = target_base + i * sizeof(PointTarget);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kX, sizeof(PointTarget), vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kY, sizeof(PointTarget), vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, TgtLayout::kZ, sizeof(PointTarget), vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleDualIndicesCloudPairD64RVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "dual-indices SVD scale double RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* source_idx_i32 = reinterpret_cast<const std::int32_t*>(indices_src.data());
  const auto* target_idx_i32 = reinterpret_cast<const std::int32_t*>(indices_tgt.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const vint32mf2_t v_source_idx_i32 = __riscv_vle32_v_i32mf2(source_idx_i32 + i, vl);
    const vint32mf2_t v_target_idx_i32 = __riscv_vle32_v_i32mf2(target_idx_i32 + i, vl);
    const vuint32mf2_t v_source_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_source_idx_i32),
        static_cast<std::uint32_t>(sizeof(PointSource)),
        vl);
    const vuint32mf2_t v_target_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_target_idx_i32),
        static_cast<std::uint32_t>(sizeof(PointTarget)),
        vl);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kX, v_source_off, vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kY, v_source_off, vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kZ, v_source_off, vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, TgtLayout::kX, v_target_off, vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, TgtLayout::kY, v_target_off, vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, TgtLayout::kZ, v_target_off, vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "correspondence SVD scale double RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = correspondences.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32mf2_t v_query_i32 = __riscv_vlse32_v_i32mf2(query_i32, corr_stride, vl);
    const vint32mf2_t v_match_i32 = __riscv_vlse32_v_i32mf2(match_i32, corr_stride, vl);
    const vuint32mf2_t v_query_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_query_i32),
        static_cast<std::uint32_t>(sizeof(PointSource)),
        vl);
    const vuint32mf2_t v_match_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_match_i32),
        static_cast<std::uint32_t>(sizeof(PointTarget)),
        vl);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kX, v_query_off, vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kY, v_query_off, vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, SrcLayout::kZ, v_query_off, vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, TgtLayout::kX, v_match_off, vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, TgtLayout::kY, v_match_off, vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, TgtLayout::kZ, v_match_off, vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleSourceIndexedPointXYZD64RVV(
    const pcl::PointCloud<pcl::PointXYZ>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<pcl::PointXYZ>& cloud_tgt)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "source-indexed SVD scale double RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* indices_i32 = reinterpret_cast<const std::int32_t*>(indices_src.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const vint32mf2_t v_idx_i32 = __riscv_vle32_v_i32mf2(indices_i32 + i, vl);
    const vuint32mf2_t v_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_idx_i32),
        static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)),
        vl);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, x), v_off, vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, y), v_off, vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, z), v_off, vl),
        vl);
    const auto* tgt = target_base + i * sizeof(pcl::PointXYZ);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, x), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, y), sizeof(pcl::PointXYZ), vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        stridedLoadTransformationEstimationSVDScaleF32MF2(
            tgt, offsetof(pcl::PointXYZ, z), sizeof(pcl::PointXYZ), vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleDualIndicesPointXYZD64RVV(
    const pcl::PointCloud<pcl::PointXYZ>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<pcl::PointXYZ>& cloud_tgt,
    const pcl::Indices& indices_tgt)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "dual-indices SVD scale double RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* source_idx_i32 = reinterpret_cast<const std::int32_t*>(indices_src.data());
  const auto* target_idx_i32 = reinterpret_cast<const std::int32_t*>(indices_tgt.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const vint32mf2_t v_source_idx_i32 = __riscv_vle32_v_i32mf2(source_idx_i32 + i, vl);
    const vint32mf2_t v_target_idx_i32 = __riscv_vle32_v_i32mf2(target_idx_i32 + i, vl);
    const vuint32mf2_t v_source_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_source_idx_i32),
        static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)),
        vl);
    const vuint32mf2_t v_target_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_target_idx_i32),
        static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)),
        vl);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, x), v_source_off, vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, y), v_source_off, vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, z), v_source_off, vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, offsetof(pcl::PointXYZ, x), v_target_off, vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, offsetof(pcl::PointXYZ, y), v_target_off, vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, offsetof(pcl::PointXYZ, z), v_target_off, vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

inline TransformationEstimationSVDScaleD64Accumulation
accumulateTransformationEstimationSVDScaleCorrespondencePointXYZD64RVV(
    const pcl::PointCloud<pcl::PointXYZ>& cloud_src,
    const pcl::PointCloud<pcl::PointXYZ>& cloud_tgt,
    const pcl::Correspondences& correspondences)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "correspondence SVD scale double RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleD64Accumulation acc;
  const std::size_t nr_points = correspondences.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32mf2();
  const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vlmax);
  vfloat64m1_t ssx = zero, ssy = zero, ssz = zero;
  vfloat64m1_t stx = zero, sty = zero, stz = zero;
  vfloat64m1_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m1_t c10 = zero, c11 = zero, c12 = zero;
  vfloat64m1_t c20 = zero, c21 = zero, c22 = zero;
  vfloat64m1_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32mf2(nr_points - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32mf2_t v_query_i32 = __riscv_vlse32_v_i32mf2(query_i32, corr_stride, vl);
    const vint32mf2_t v_match_i32 = __riscv_vlse32_v_i32mf2(match_i32, corr_stride, vl);
    const vuint32mf2_t v_query_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_query_i32),
        static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)),
        vl);
    const vuint32mf2_t v_match_off = __riscv_vmul_vx_u32mf2(
        __riscv_vreinterpret_v_i32mf2_u32mf2(v_match_i32),
        static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)),
        vl);
    const vfloat64m1_t sx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, x), v_query_off, vl),
        vl);
    const vfloat64m1_t sy = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, y), v_query_off, vl),
        vl);
    const vfloat64m1_t sz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            source_base, offsetof(pcl::PointXYZ, z), v_query_off, vl),
        vl);
    const vfloat64m1_t tx = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, offsetof(pcl::PointXYZ, x), v_match_off, vl),
        vl);
    const vfloat64m1_t ty = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, offsetof(pcl::PointXYZ, y), v_match_off, vl),
        vl);
    const vfloat64m1_t tz = widenTransformationEstimationSVDScaleF64(
        indexedLoadTransformationEstimationSVDScalePointXYZF32MF2(
            target_base, offsetof(pcl::PointXYZ, z), v_match_off, vl),
        vl);
    accumulateTransformationEstimationSVDScaleD64Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleD64Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleF32Accumulation
accumulateTransformationEstimationSVDScaleOrderedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt)
{
  TransformationEstimationSVDScaleF32Accumulation acc;
  const std::size_t nr_points = cloud_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
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

  acc.source_sum[0] = reduceTransformationEstimationSVDScaleF32M2(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDScaleF32M2(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDScaleF32M2(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDScaleF32M2(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDScaleF32M2(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDScaleF32M2(stz, vlmax);
  acc.source_target_sum[0] =
      reduceTransformationEstimationSVDScaleF32M2(c00, vlmax);
  acc.source_target_sum[1] =
      reduceTransformationEstimationSVDScaleF32M2(c01, vlmax);
  acc.source_target_sum[2] =
      reduceTransformationEstimationSVDScaleF32M2(c02, vlmax);
  acc.source_target_sum[3] =
      reduceTransformationEstimationSVDScaleF32M2(c10, vlmax);
  acc.source_target_sum[4] =
      reduceTransformationEstimationSVDScaleF32M2(c11, vlmax);
  acc.source_target_sum[5] =
      reduceTransformationEstimationSVDScaleF32M2(c12, vlmax);
  acc.source_target_sum[6] =
      reduceTransformationEstimationSVDScaleF32M2(c20, vlmax);
  acc.source_target_sum[7] =
      reduceTransformationEstimationSVDScaleF32M2(c21, vlmax);
  acc.source_target_sum[8] =
      reduceTransformationEstimationSVDScaleF32M2(c22, vlmax);
  acc.source_square_sum =
      reduceTransformationEstimationSVDScaleF32M2(source_square, vlmax);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleF32Accumulation
accumulateTransformationEstimationSVDScaleContiguousOffsetPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const std::size_t source_offset,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const std::size_t target_offset,
    const std::size_t nr_points)
{
  TransformationEstimationSVDScaleF32Accumulation acc;
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base = reinterpret_cast<const std::uint8_t*>(
      cloud_src.points.data() + source_offset);
  const auto* target_base = reinterpret_cast<const std::uint8_t*>(
      cloud_tgt.points.data() + target_offset);

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    accumulateTransformationEstimationSVDScaleF32Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleF32Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleF32Accumulation
accumulateTransformationEstimationSVDScaleSourceIndexedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt)
{
  using SrcPod = typename SrcLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "source-indexed SVD scale RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleF32Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* indices_i32 = reinterpret_cast<const std::int32_t*>(indices_src.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    const vint32m2_t v_idx_i32 = __riscv_vle32_v_i32m2(indices_i32 + i, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SrcPod,
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base, v_off, vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    accumulateTransformationEstimationSVDScaleF32Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleF32Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleF32Accumulation
accumulateTransformationEstimationSVDScaleDualIndicesCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt)
{
  using SrcPod = typename SrcLayout::Pod;
  using TgtPod = typename TgtLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "dual-indices SVD scale RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleF32Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* source_idx_i32 = reinterpret_cast<const std::int32_t*>(indices_src.data());
  const auto* target_idx_i32 = reinterpret_cast<const std::int32_t*>(indices_tgt.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    const vint32m2_t v_source_idx_i32 = __riscv_vle32_v_i32m2(source_idx_i32 + i, vl);
    const vint32m2_t v_target_idx_i32 = __riscv_vle32_v_i32m2(target_idx_i32 + i, vl);
    const vuint32m2_t v_source_off = pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_source_idx_i32), vl);
    const vuint32m2_t v_target_off = pcl::rvv_load::byte_offsets_u32m2<TgtPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_target_idx_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SrcPod,
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base, v_source_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TgtPod,
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base, v_target_off, vl, tx, ty, tz);
    accumulateTransformationEstimationSVDScaleF32Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleF32Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDScaleF32Accumulation
accumulateTransformationEstimationSVDScaleCorrespondencePairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences)
{
  using SrcPod = typename SrcLayout::Pod;
  using TgtPod = typename TgtLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "correspondence SVD scale RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDScaleF32Accumulation acc;
  const std::size_t nr_points = correspondences.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat32m2_t c10 = zero, c11 = zero, c12 = zero;
  vfloat32m2_t c20 = zero, c21 = zero, c22 = zero;
  vfloat32m2_t source_square = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());
  const auto* corr_base = reinterpret_cast<const std::uint8_t*>(correspondences.data());
  const auto* query_base = corr_base + offsetof(pcl::Correspondence, index_query);
  const auto* match_base = corr_base + offsetof(pcl::Correspondence, index_match);
  const ptrdiff_t corr_stride = static_cast<ptrdiff_t>(sizeof(pcl::Correspondence));

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    const auto* query_i32 =
        reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 =
        reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
    const vint32m2_t v_query_i32 = __riscv_vlse32_v_i32m2(query_i32, corr_stride, vl);
    const vint32m2_t v_match_i32 = __riscv_vlse32_v_i32m2(match_i32, corr_stride, vl);
    const vuint32m2_t v_query_off = pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_query_i32), vl);
    const vuint32m2_t v_match_off = pcl::rvv_load::byte_offsets_u32m2<TgtPod>(
        __riscv_vreinterpret_v_i32m2_u32m2(v_match_i32), vl);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::indexed_load3_f32m2<SrcPod,
                                       SrcLayout::kX,
                                       SrcLayout::kY,
                                       SrcLayout::kZ>(
        source_base, v_query_off, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_f32m2<TgtPod,
                                       TgtLayout::kX,
                                       TgtLayout::kY,
                                       TgtLayout::kZ>(
        target_base, v_match_off, vl, tx, ty, tz);
    accumulateTransformationEstimationSVDScaleF32Lanes(sx,
                                                       sy,
                                                       sz,
                                                       tx,
                                                       ty,
                                                       tz,
                                                       vl,
                                                       ssx,
                                                       ssy,
                                                       ssz,
                                                       stx,
                                                       sty,
                                                       stz,
                                                       c00,
                                                       c01,
                                                       c02,
                                                       c10,
                                                       c11,
                                                       c12,
                                                       c20,
                                                       c21,
                                                       c22,
                                                       source_square);
    i += vl;
  }

  finishTransformationEstimationSVDScaleF32Accumulation(acc,
                                                        vlmax,
                                                        ssx,
                                                        ssy,
                                                        ssz,
                                                        stx,
                                                        sty,
                                                        stz,
                                                        c00,
                                                        c01,
                                                        c02,
                                                        c10,
                                                        c11,
                                                        c12,
                                                        c20,
                                                        c21,
                                                        c22,
                                                        source_square);
  return acc;
}

inline bool
solveTransformationEstimationSVDScaleF32(
    const TransformationEstimationSVDScaleF32Accumulation& acc,
    Eigen::Matrix<float, 4, 4>& transformation_matrix)
{
  transformation_matrix.setIdentity();
  if (acc.count == 0)
    return false;

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
  if (sum_ss <= 0.0f || !std::isfinite(sum_ss))
    return false;

  const float scale = (R * H).trace() / sum_ss;
  if (!std::isfinite(scale))
    return false;

  transformation_matrix.template topLeftCorner<3, 3>() = scale * R;
  transformation_matrix.template block<3, 1>(0, 3) =
      target_mean - scale * R * source_mean;
  return true;
}

inline bool
solveTransformationEstimationSVDScaleD64(
    const TransformationEstimationSVDScaleD64Accumulation& acc,
    Eigen::Matrix<double, 4, 4>& transformation_matrix)
{
  transformation_matrix.setIdentity();
  if (acc.count == 0)
    return false;

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
  if (sum_ss <= 0.0 || !std::isfinite(sum_ss))
    return false;

  const double scale = (R * H).trace() / sum_ss;
  if (!std::isfinite(scale))
    return false;

  transformation_matrix.template topLeftCorner<3, 3>() = scale * R;
  transformation_matrix.template block<3, 1>(0, 3) =
      target_mean - scale * R * source_mean;
  return true;
}

inline bool
transformationEstimationSVDScaleNeedsCorrespondenceSortedCopy(
    const pcl::Correspondences& correspondences)
{
  if (correspondences.size() < 65536)
    return false;

  const std::size_t sample_step = std::max<std::size_t>(1, correspondences.size() / 512);
  double normalized_query_delta_sum = 0.0;
  std::size_t samples = 0;
  for (std::size_t i = sample_step; i < correspondences.size(); i += sample_step) {
    const auto lhs = static_cast<double>(correspondences[i - sample_step].index_query);
    const auto rhs = static_cast<double>(correspondences[i].index_query);
    normalized_query_delta_sum += std::abs(rhs - lhs) / static_cast<double>(sample_step);
    ++samples;
  }

  if (samples == 0)
    return false;
  return (normalized_query_delta_sum / static_cast<double>(samples)) > 32.0;
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline bool
estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences,
    Eigen::Matrix<float, 4, 4>& transformation_matrix)
{
  if (!transformationEstimationSVDScaleNeedsCorrespondenceSortedCopy(correspondences))
    return false;

  pcl::Correspondences sorted_correspondences = correspondences;
  std::sort(sorted_correspondences.begin(),
            sorted_correspondences.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.index_query != rhs.index_query)
                return lhs.index_query < rhs.index_query;
              if (lhs.index_match != rhs.index_match)
                return lhs.index_match < rhs.index_match;
              return lhs.distance < rhs.distance;
            });

  return solveTransformationEstimationSVDScaleF32(
      accumulateTransformationEstimationSVDScaleCorrespondencePairRVV<
          PointSource,
          PointTarget,
          SrcLayout,
          TgtLayout>(cloud_src, cloud_tgt, sorted_correspondences),
      transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDScaleOrderedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (std::is_same_v<Scalar, double> && SrcLayout::value &&
                TgtLayout::value) {
    const std::size_t nr_points = cloud_src.size();
    if (cloud_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16) {
      return false;
    }

    return solveTransformationEstimationSVDScaleD64(
        accumulateTransformationEstimationSVDScaleOrderedCloudPairD64RVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, cloud_tgt),
        transformation_matrix);
  }
  else if constexpr (std::is_same_v<Scalar, float> && SrcLayout::value &&
                     TgtLayout::value) {
    const std::size_t nr_points = cloud_src.size();
    if (cloud_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16) {
      return false;
    }

    return solveTransformationEstimationSVDScaleF32(
        accumulateTransformationEstimationSVDScaleOrderedCloudPairRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, cloud_tgt),
        transformation_matrix);
  }
  else {
    return false;
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDScaleSourceIndexedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (std::is_same_v<Scalar, double> && SrcLayout::value &&
                TgtLayout::value) {
    const std::size_t nr_points = indices_src.size();
    if (cloud_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16 ||
        !transformationEstimationSVDSourceIndicesFitRVVGather(cloud_src,
                                                              indices_src)) {
      return false;
    }

    const auto contiguous_source =
        transformationEstimationSVDScaleContiguousIndexRange(indices_src,
                                                             cloud_src.size());
    if (contiguous_source.valid) {
      return solveTransformationEstimationSVDScaleD64(
          accumulateTransformationEstimationSVDScaleContiguousOffsetPairD64RVV<
              PointSource,
              PointTarget,
              SrcLayout,
              TgtLayout>(
              cloud_src, contiguous_source.offset, cloud_tgt, 0, nr_points),
          transformation_matrix);
    }

    return solveTransformationEstimationSVDScaleD64(
        accumulateTransformationEstimationSVDScaleSourceIndexedCloudPairD64RVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(
            cloud_src, indices_src, cloud_tgt),
        transformation_matrix);
  }
  else if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                     !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = indices_src.size();
    if (cloud_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16 ||
        !transformationEstimationSVDSourceIndicesFitRVVGather(cloud_src,
                                                              indices_src)) {
      return false;
    }

    const auto contiguous_source =
        transformationEstimationSVDScaleContiguousIndexRange(indices_src,
                                                             cloud_src.size());
    if (contiguous_source.valid) {
      return solveTransformationEstimationSVDScaleF32(
          accumulateTransformationEstimationSVDScaleContiguousOffsetPairRVV<
              PointSource,
              PointTarget,
              SrcLayout,
              TgtLayout>(cloud_src, contiguous_source.offset, cloud_tgt, 0, nr_points),
          transformation_matrix);
    }

    return solveTransformationEstimationSVDScaleF32(
        accumulateTransformationEstimationSVDScaleSourceIndexedCloudPairRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, indices_src, cloud_tgt),
        transformation_matrix);
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDScaleDualIndicesCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (std::is_same_v<Scalar, double> && SrcLayout::value &&
                TgtLayout::value) {
    const std::size_t nr_points = indices_src.size();
    if (indices_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16 ||
        !transformationEstimationSVDDualIndicesFitRVVGather(
            cloud_src, indices_src, cloud_tgt, indices_tgt)) {
      return false;
    }

    const auto contiguous_source =
        transformationEstimationSVDScaleContiguousIndexRange(indices_src,
                                                             cloud_src.size());
    const auto contiguous_target =
        transformationEstimationSVDScaleContiguousIndexRange(indices_tgt,
                                                             cloud_tgt.size());
    if (contiguous_source.valid && contiguous_target.valid &&
        contiguous_source.count == contiguous_target.count) {
      return solveTransformationEstimationSVDScaleD64(
          accumulateTransformationEstimationSVDScaleContiguousOffsetPairD64RVV<
              PointSource,
              PointTarget,
              SrcLayout,
              TgtLayout>(
              cloud_src,
              contiguous_source.offset,
              cloud_tgt,
              contiguous_target.offset,
              nr_points),
          transformation_matrix);
    }

    return solveTransformationEstimationSVDScaleD64(
        accumulateTransformationEstimationSVDScaleDualIndicesCloudPairD64RVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(
            cloud_src, indices_src, cloud_tgt, indices_tgt),
        transformation_matrix);
  }
  else if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                     !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = indices_src.size();
    if (indices_tgt.size() != nr_points || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16 ||
        !transformationEstimationSVDDualIndicesFitRVVGather(
            cloud_src, indices_src, cloud_tgt, indices_tgt)) {
      return false;
    }

    const auto contiguous_source =
        transformationEstimationSVDScaleContiguousIndexRange(indices_src,
                                                             cloud_src.size());
    const auto contiguous_target =
        transformationEstimationSVDScaleContiguousIndexRange(indices_tgt,
                                                             cloud_tgt.size());
    if (contiguous_source.valid && contiguous_target.valid &&
        contiguous_source.count == contiguous_target.count) {
      return solveTransformationEstimationSVDScaleF32(
          accumulateTransformationEstimationSVDScaleContiguousOffsetPairRVV<
              PointSource,
              PointTarget,
              SrcLayout,
              TgtLayout>(cloud_src,
                         contiguous_source.offset,
                         cloud_tgt,
                         contiguous_target.offset,
                         nr_points),
          transformation_matrix);
    }

    return solveTransformationEstimationSVDScaleF32(
        accumulateTransformationEstimationSVDScaleDualIndicesCloudPairRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, indices_src, cloud_tgt, indices_tgt),
        transformation_matrix);
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDScaleCorrespondencePairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (std::is_same_v<Scalar, double> && SrcLayout::value &&
                TgtLayout::value) {
    const std::size_t nr_points = correspondences.size();
    if (correspondences.empty() || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16 ||
        !transformationEstimationSVDCorrespondencesFitRVVGather(
            cloud_src, cloud_tgt, correspondences)) {
      return false;
    }

    const auto contiguous_correspondence =
        transformationEstimationSVDScaleContiguousCorrespondenceRange(correspondences,
                                                                     cloud_src.size(),
                                                                     cloud_tgt.size());
    if (contiguous_correspondence.valid) {
      return solveTransformationEstimationSVDScaleD64(
          accumulateTransformationEstimationSVDScaleContiguousOffsetPairD64RVV<
              PointSource,
              PointTarget,
              SrcLayout,
              TgtLayout>(
              cloud_src,
              contiguous_correspondence.query_offset,
              cloud_tgt,
              contiguous_correspondence.match_offset,
              nr_points),
          transformation_matrix);
    }

    return solveTransformationEstimationSVDScaleD64(
        accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(
            cloud_src, cloud_tgt, correspondences),
        transformation_matrix);
  }
  else if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                     !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = correspondences.size();
    if (correspondences.empty() || !cloud_src.is_dense || !cloud_tgt.is_dense ||
        nr_points < 16 ||
        !transformationEstimationSVDCorrespondencesFitRVVGather(
            cloud_src, cloud_tgt, correspondences)) {
      return false;
    }

    const auto contiguous_correspondence =
        transformationEstimationSVDScaleContiguousCorrespondenceRange(correspondences,
                                                                     cloud_src.size(),
                                                                     cloud_tgt.size());
    if (contiguous_correspondence.valid) {
      return solveTransformationEstimationSVDScaleF32(
          accumulateTransformationEstimationSVDScaleContiguousOffsetPairRVV<
              PointSource,
              PointTarget,
              SrcLayout,
              TgtLayout>(cloud_src,
                         contiguous_correspondence.query_offset,
                         cloud_tgt,
                         contiguous_correspondence.match_offset,
                         nr_points),
          transformation_matrix);
    }

    if (estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, cloud_tgt, correspondences, transformation_matrix)) {
      return true;
    }

    return solveTransformationEstimationSVDScaleF32(
        accumulateTransformationEstimationSVDScaleCorrespondencePairRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, cloud_tgt, correspondences),
        transformation_matrix);
  }
}

} // namespace detail
#endif // __RVV10__

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDScaleOrderedCloudPairRVV(
          cloud_src, cloud_tgt, transformation_matrix)) {
    return;
  }
#endif

  TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
      estimateRigidTransformation(cloud_src, cloud_tgt, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDScaleSourceIndexedCloudPairRVV(
          cloud_src, indices_src, cloud_tgt, transformation_matrix)) {
    return;
  }
#endif

  TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
      estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Indices& indices_tgt,
                                Matrix4& transformation_matrix) const
{
#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDScaleDualIndicesCloudPairRVV(
          cloud_src, indices_src, cloud_tgt, indices_tgt, transformation_matrix)) {
    return;
  }
#endif

  TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
      estimateRigidTransformation(
          cloud_src, indices_src, cloud_tgt, indices_tgt, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Correspondences& correspondences,
                                Matrix4& transformation_matrix) const
{
#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDScaleCorrespondencePairRVV(
          cloud_src, cloud_tgt, correspondences, transformation_matrix)) {
    return;
  }
#endif

  TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
      estimateRigidTransformation(
          cloud_src, cloud_tgt, correspondences, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>::
    getTransformationFromCorrelation(
        const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& cloud_src_demean,
        const Eigen::Matrix<Scalar, 4, 1>& centroid_src,
        const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& cloud_tgt_demean,
        const Eigen::Matrix<Scalar, 4, 1>& centroid_tgt,
        Matrix4& transformation_matrix) const
{
  transformation_matrix.setIdentity();

  // Assemble the correlation matrix H = source * target'
  Eigen::Matrix<Scalar, 3, 3> H =
      (cloud_src_demean * cloud_tgt_demean.transpose()).template topLeftCorner<3, 3>();

  // Compute the Singular Value Decomposition
  Eigen::JacobiSVD<Eigen::Matrix<Scalar, 3, 3>> svd(
      H, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix<Scalar, 3, 3> u = svd.matrixU();
  Eigen::Matrix<Scalar, 3, 3> v = svd.matrixV();

  // Compute R = V * U'
  if (u.determinant() * v.determinant() < 0) {
    for (int x = 0; x < 3; ++x)
      v(x, 2) *= -1;
  }

  Eigen::Matrix<Scalar, 3, 3> R = v * u.transpose();

  double sum_ss = 0.0f;
  for (unsigned corrIdx = 0; corrIdx < cloud_src_demean.cols(); ++corrIdx) {
    sum_ss += cloud_src_demean(0, corrIdx) * cloud_src_demean(0, corrIdx);
    sum_ss += cloud_src_demean(1, corrIdx) * cloud_src_demean(1, corrIdx);
    sum_ss += cloud_src_demean(2, corrIdx) * cloud_src_demean(2, corrIdx);
  }

  const Scalar sum_tt = (R * H).trace();
  const float scale = static_cast<float>(sum_tt / sum_ss);
  transformation_matrix.template topLeftCorner<3, 3>() = scale * R;
  const Eigen::Matrix<Scalar, 3, 1> Rc(scale * R * centroid_src.template head<3>());
  transformation_matrix.template block<3, 1>(0, 3) =
      centroid_tgt.template head<3>() - Rc;
}

} // namespace registration
} // namespace pcl

//#define PCL_INSTANTIATE_TransformationEstimationSVD(T,U) template class PCL_EXPORTS
// pcl::registration::TransformationEstimationSVD<T,U>;

#endif /* PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_SVD_SCALE_HPP_ */
