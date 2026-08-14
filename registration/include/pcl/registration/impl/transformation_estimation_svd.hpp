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
 * $Id$
 *
 */

#ifndef PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_SVD_HPP_
#define PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_SVD_HPP_

#include <pcl/common/eigen.h>
#include <pcl/correspondence.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl {

namespace registration {

#ifdef __RVV10__
namespace detail {

struct TransformationEstimationSVDF32Accumulation {
  float source_sum[3]{0.0f, 0.0f, 0.0f};
  float target_sum[3]{0.0f, 0.0f, 0.0f};
  float target_source_sum[9]{0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f,
                             0.0f};
  std::size_t count{0};
};

inline float
reduceTransformationEstimationSVDF32M2(const vfloat32m2_t value,
                                       const std::size_t vlmax)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return __riscv_vfmv_f_s_f32m1_f32(
      __riscv_vfredosum_vs_f32m2_f32m1(value, zero, vlmax));
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDF32Accumulation
accumulateTransformationEstimationSVDOrderedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt)
{
  TransformationEstimationSVDF32Accumulation acc;
  const std::size_t nr_points = cloud_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
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
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceTransformationEstimationSVDF32M2(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDF32M2(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDF32M2(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDF32M2(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDF32M2(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDF32M2(stz, vlmax);
  acc.target_source_sum[0] = reduceTransformationEstimationSVDF32M2(t0s0, vlmax);
  acc.target_source_sum[1] = reduceTransformationEstimationSVDF32M2(t0s1, vlmax);
  acc.target_source_sum[2] = reduceTransformationEstimationSVDF32M2(t0s2, vlmax);
  acc.target_source_sum[3] = reduceTransformationEstimationSVDF32M2(t1s0, vlmax);
  acc.target_source_sum[4] = reduceTransformationEstimationSVDF32M2(t1s1, vlmax);
  acc.target_source_sum[5] = reduceTransformationEstimationSVDF32M2(t1s2, vlmax);
  acc.target_source_sum[6] = reduceTransformationEstimationSVDF32M2(t2s0, vlmax);
  acc.target_source_sum[7] = reduceTransformationEstimationSVDF32M2(t2s1, vlmax);
  acc.target_source_sum[8] = reduceTransformationEstimationSVDF32M2(t2s2, vlmax);
  return acc;
}

template <typename PointT>
inline bool
transformationEstimationSVDSourceIndicesInRange(const pcl::PointCloud<PointT>& cloud,
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
transformationEstimationSVDSourceIndicesFitRVVGather(
    const pcl::PointCloud<PointT>& cloud,
    const pcl::Indices& indices)
{
  return cloud.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>() &&
         transformationEstimationSVDSourceIndicesInRange(cloud, indices);
}

template <typename PointSource, typename PointTarget>
inline bool
transformationEstimationSVDDualIndicesFitRVVGather(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt)
{
  return transformationEstimationSVDSourceIndicesFitRVVGather(cloud_src, indices_src) &&
         transformationEstimationSVDSourceIndicesFitRVVGather(cloud_tgt, indices_tgt);
}

template <typename PointSource, typename PointTarget>
inline bool
transformationEstimationSVDCorrespondencesInRange(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences)
{
  for (const auto& corr : correspondences) {
    if (corr.index_query < 0 || corr.index_match < 0)
      return false;
    if (static_cast<std::size_t>(corr.index_query) >= cloud_src.size() ||
        static_cast<std::size_t>(corr.index_match) >= cloud_tgt.size()) {
      return false;
    }
  }
  return true;
}

template <typename PointSource, typename PointTarget>
inline bool
transformationEstimationSVDCorrespondencesFitRVVGather(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences)
{
  return cloud_src.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() &&
         cloud_tgt.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>() &&
         transformationEstimationSVDCorrespondencesInRange(
             cloud_src, cloud_tgt, correspondences);
}

template <typename PointSource,
          typename PointTarget,
          typename SrcLayout,
          typename TgtLayout>
inline TransformationEstimationSVDF32Accumulation
accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt)
{
  using SrcPod = typename SrcLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "source-indexed SVD RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDF32Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
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
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceTransformationEstimationSVDF32M2(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDF32M2(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDF32M2(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDF32M2(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDF32M2(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDF32M2(stz, vlmax);
  acc.target_source_sum[0] = reduceTransformationEstimationSVDF32M2(t0s0, vlmax);
  acc.target_source_sum[1] = reduceTransformationEstimationSVDF32M2(t0s1, vlmax);
  acc.target_source_sum[2] = reduceTransformationEstimationSVDF32M2(t0s2, vlmax);
  acc.target_source_sum[3] = reduceTransformationEstimationSVDF32M2(t1s0, vlmax);
  acc.target_source_sum[4] = reduceTransformationEstimationSVDF32M2(t1s1, vlmax);
  acc.target_source_sum[5] = reduceTransformationEstimationSVDF32M2(t1s2, vlmax);
  acc.target_source_sum[6] = reduceTransformationEstimationSVDF32M2(t2s0, vlmax);
  acc.target_source_sum[7] = reduceTransformationEstimationSVDF32M2(t2s1, vlmax);
  acc.target_source_sum[8] = reduceTransformationEstimationSVDF32M2(t2s2, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline TransformationEstimationSVDF32Accumulation
accumulateTransformationEstimationSVDDualIndicesCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SrcPod = typename SrcLayout::Pod;
  using TgtPod = typename TgtLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "dual-indices SVD RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDF32Accumulation acc;
  const std::size_t nr_points = indices_src.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
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
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceTransformationEstimationSVDF32M2(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDF32M2(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDF32M2(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDF32M2(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDF32M2(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDF32M2(stz, vlmax);
  acc.target_source_sum[0] = reduceTransformationEstimationSVDF32M2(t0s0, vlmax);
  acc.target_source_sum[1] = reduceTransformationEstimationSVDF32M2(t0s1, vlmax);
  acc.target_source_sum[2] = reduceTransformationEstimationSVDF32M2(t0s2, vlmax);
  acc.target_source_sum[3] = reduceTransformationEstimationSVDF32M2(t1s0, vlmax);
  acc.target_source_sum[4] = reduceTransformationEstimationSVDF32M2(t1s1, vlmax);
  acc.target_source_sum[5] = reduceTransformationEstimationSVDF32M2(t1s2, vlmax);
  acc.target_source_sum[6] = reduceTransformationEstimationSVDF32M2(t2s0, vlmax);
  acc.target_source_sum[7] = reduceTransformationEstimationSVDF32M2(t2s1, vlmax);
  acc.target_source_sum[8] = reduceTransformationEstimationSVDF32M2(t2s2, vlmax);
  return acc;
}

template <typename PointSource, typename PointTarget>
inline TransformationEstimationSVDF32Accumulation
accumulateTransformationEstimationSVDCorrespondencePairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;
  using SrcPod = typename SrcLayout::Pod;
  using TgtPod = typename TgtLayout::Pod;
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "correspondence SVD RVV path expects 32-bit PCL indices.");

  TransformationEstimationSVDF32Accumulation acc;
  const std::size_t nr_points = correspondences.size();
  acc.count = nr_points;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t ssx = zero, ssy = zero, ssz = zero;
  vfloat32m2_t stx = zero, sty = zero, stz = zero;
  vfloat32m2_t t0s0 = zero, t0s1 = zero, t0s2 = zero;
  vfloat32m2_t t1s0 = zero, t1s1 = zero, t1s2 = zero;
  vfloat32m2_t t2s0 = zero, t2s1 = zero, t2s2 = zero;
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
    const auto* query_i32 = reinterpret_cast<const std::int32_t*>(query_base + i * corr_stride);
    const auto* match_i32 = reinterpret_cast<const std::int32_t*>(match_base + i * corr_stride);
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
    ssx = __riscv_vfadd_vv_f32m2_tu(ssx, ssx, sx, vl);
    ssy = __riscv_vfadd_vv_f32m2_tu(ssy, ssy, sy, vl);
    ssz = __riscv_vfadd_vv_f32m2_tu(ssz, ssz, sz, vl);
    stx = __riscv_vfadd_vv_f32m2_tu(stx, stx, tx, vl);
    sty = __riscv_vfadd_vv_f32m2_tu(sty, sty, ty, vl);
    stz = __riscv_vfadd_vv_f32m2_tu(stz, stz, tz, vl);
    t0s0 = __riscv_vfmacc_vv_f32m2_tu(t0s0, tx, sx, vl);
    t0s1 = __riscv_vfmacc_vv_f32m2_tu(t0s1, tx, sy, vl);
    t0s2 = __riscv_vfmacc_vv_f32m2_tu(t0s2, tx, sz, vl);
    t1s0 = __riscv_vfmacc_vv_f32m2_tu(t1s0, ty, sx, vl);
    t1s1 = __riscv_vfmacc_vv_f32m2_tu(t1s1, ty, sy, vl);
    t1s2 = __riscv_vfmacc_vv_f32m2_tu(t1s2, ty, sz, vl);
    t2s0 = __riscv_vfmacc_vv_f32m2_tu(t2s0, tz, sx, vl);
    t2s1 = __riscv_vfmacc_vv_f32m2_tu(t2s1, tz, sy, vl);
    t2s2 = __riscv_vfmacc_vv_f32m2_tu(t2s2, tz, sz, vl);
    i += vl;
  }

  acc.source_sum[0] = reduceTransformationEstimationSVDF32M2(ssx, vlmax);
  acc.source_sum[1] = reduceTransformationEstimationSVDF32M2(ssy, vlmax);
  acc.source_sum[2] = reduceTransformationEstimationSVDF32M2(ssz, vlmax);
  acc.target_sum[0] = reduceTransformationEstimationSVDF32M2(stx, vlmax);
  acc.target_sum[1] = reduceTransformationEstimationSVDF32M2(sty, vlmax);
  acc.target_sum[2] = reduceTransformationEstimationSVDF32M2(stz, vlmax);
  acc.target_source_sum[0] = reduceTransformationEstimationSVDF32M2(t0s0, vlmax);
  acc.target_source_sum[1] = reduceTransformationEstimationSVDF32M2(t0s1, vlmax);
  acc.target_source_sum[2] = reduceTransformationEstimationSVDF32M2(t0s2, vlmax);
  acc.target_source_sum[3] = reduceTransformationEstimationSVDF32M2(t1s0, vlmax);
  acc.target_source_sum[4] = reduceTransformationEstimationSVDF32M2(t1s1, vlmax);
  acc.target_source_sum[5] = reduceTransformationEstimationSVDF32M2(t1s2, vlmax);
  acc.target_source_sum[6] = reduceTransformationEstimationSVDF32M2(t2s0, vlmax);
  acc.target_source_sum[7] = reduceTransformationEstimationSVDF32M2(t2s1, vlmax);
  acc.target_source_sum[8] = reduceTransformationEstimationSVDF32M2(t2s2, vlmax);
  return acc;
}

inline Eigen::Matrix4f
solveTransformationEstimationSVDF32(
    const TransformationEstimationSVDF32Accumulation& acc)
{
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  if (acc.count == 0)
    return transform;

  const float inv_n = 1.0f / static_cast<float>(acc.count);
  const Eigen::Vector3f source_mean(acc.source_sum[0] * inv_n,
                                    acc.source_sum[1] * inv_n,
                                    acc.source_sum[2] * inv_n);
  const Eigen::Vector3f target_mean(acc.target_sum[0] * inv_n,
                                    acc.target_sum[1] * inv_n,
                                    acc.target_sum[2] * inv_n);
  Eigen::Matrix3f sigma;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      sigma(r, c) = acc.target_source_sum[r * 3 + c] * inv_n -
                    target_mean[r] * source_mean[c];
    }
  }

  Eigen::JacobiSVD<Eigen::Matrix3f> svd(sigma, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Vector3f signs = Eigen::Vector3f::Ones();
  if (svd.matrixU().determinant() * svd.matrixV().determinant() < 0.0f)
    signs[2] = -1.0f;
  const Eigen::Matrix3f rotation =
      svd.matrixU() * signs.asDiagonal() * svd.matrixV().transpose();
  transform.template topLeftCorner<3, 3>() = rotation;
  transform.template block<3, 1>(0, 3) = target_mean - rotation * source_mean;
  return transform;
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDOrderedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const bool use_umeyama,
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
    if (!use_umeyama || cloud_tgt.size() != nr_points || !cloud_src.is_dense ||
        !cloud_tgt.is_dense || nr_points < 16) {
      return false;
    }

    transformation_matrix = solveTransformationEstimationSVDF32(
        accumulateTransformationEstimationSVDOrderedCloudPairRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, cloud_tgt));
    return true;
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDSourceIndexedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const bool use_umeyama,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = indices_src.size();
    if (!use_umeyama || cloud_tgt.size() != nr_points || !cloud_src.is_dense ||
        !cloud_tgt.is_dense || nr_points < 16 ||
        !transformationEstimationSVDSourceIndicesFitRVVGather(cloud_src,
                                                              indices_src)) {
      return false;
    }

    transformation_matrix = solveTransformationEstimationSVDF32(
        accumulateTransformationEstimationSVDSourceIndexedCloudPairRVV<
            PointSource,
            PointTarget,
            SrcLayout,
            TgtLayout>(cloud_src, indices_src, cloud_tgt));
    return true;
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDDualIndicesCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt,
    const bool use_umeyama,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = indices_src.size();
    if (!use_umeyama || indices_tgt.size() != nr_points || !cloud_src.is_dense ||
        !cloud_tgt.is_dense || nr_points < 16 ||
        !transformationEstimationSVDDualIndicesFitRVVGather(
            cloud_src, indices_src, cloud_tgt, indices_tgt)) {
      return false;
    }

    transformation_matrix = solveTransformationEstimationSVDF32(
        accumulateTransformationEstimationSVDDualIndicesCloudPairRVV<
            PointSource,
            PointTarget>(cloud_src, indices_src, cloud_tgt, indices_tgt));
    return true;
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
estimateRigidTransformationSVDCorrespondencePairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences,
    const bool use_umeyama,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>;

  if constexpr (!std::is_same_v<Scalar, float> || !SrcLayout::value ||
                !TgtLayout::value) {
    return false;
  }
  else {
    const std::size_t nr_points = correspondences.size();
    if (!use_umeyama || correspondences.empty() || !cloud_src.is_dense ||
        !cloud_tgt.is_dense || nr_points < 16 ||
        !transformationEstimationSVDCorrespondencesFitRVVGather(
            cloud_src, cloud_tgt, correspondences)) {
      return false;
    }

    transformation_matrix = solveTransformationEstimationSVDF32(
        accumulateTransformationEstimationSVDCorrespondencePairRVV<PointSource, PointTarget>(
            cloud_src, cloud_tgt, correspondences));
    return true;
  }
}

} // namespace detail
#endif // __RVV10__

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = cloud_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationSVD::estimateRigidTransformation] Number "
              "or points in source (%zu) differs than target (%zu)!\n",
              static_cast<std::size_t>(nr_points),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDOrderedCloudPairRVV(
          cloud_src, cloud_tgt, use_umeyama_, transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  if (indices_src.size() != cloud_tgt.size()) {
    PCL_ERROR("[pcl::TransformationSVD::estimateRigidTransformation] Number or points "
              "in source (%zu) differs than target (%zu)!\n",
              indices_src.size(),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDSourceIndexedCloudPairRVV(
          cloud_src, indices_src, cloud_tgt, use_umeyama_, transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Indices& indices_tgt,
                                Matrix4& transformation_matrix) const
{
  if (indices_src.size() != indices_tgt.size()) {
    PCL_ERROR("[pcl::TransformationEstimationSVD::estimateRigidTransformation] Number "
              "or points in source (%zu) differs than target (%zu)!\n",
              indices_src.size(),
              indices_tgt.size());
    return;
  }

#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDDualIndicesCloudPairRVV(
          cloud_src, indices_src, cloud_tgt, indices_tgt, use_umeyama_,
          transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Correspondences& correspondences,
                                Matrix4& transformation_matrix) const
{
#ifdef __RVV10__
  if (detail::estimateRigidTransformationSVDCorrespondencePairRVV(
          cloud_src, cloud_tgt, correspondences, use_umeyama_, transformation_matrix)) {
    return;
  }
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(ConstCloudIterator<PointSource>& source_it,
                                ConstCloudIterator<PointTarget>& target_it,
                                Matrix4& transformation_matrix) const
{
  // Convert to Eigen format
  const int npts = static_cast<int>(source_it.size());

  if (use_umeyama_) {
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> cloud_src(3, npts);
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> cloud_tgt(3, npts);

    for (int i = 0; i < npts; ++i) {
      cloud_src(0, i) = source_it->x;
      cloud_src(1, i) = source_it->y;
      cloud_src(2, i) = source_it->z;
      ++source_it;

      cloud_tgt(0, i) = target_it->x;
      cloud_tgt(1, i) = target_it->y;
      cloud_tgt(2, i) = target_it->z;
      ++target_it;
    }

    // Call Umeyama directly from Eigen (PCL patched version until Eigen is released)
    transformation_matrix = pcl::umeyama(cloud_src, cloud_tgt, false);
  }
  else {
    source_it.reset();
    target_it.reset();
    // <cloud_src,cloud_src> is the source dataset
    transformation_matrix.setIdentity();

    Eigen::Matrix<Scalar, 4, 1> centroid_src, centroid_tgt;
    // Estimate the centroids of source, target
    compute3DCentroid(source_it, centroid_src);
    compute3DCentroid(target_it, centroid_tgt);
    source_it.reset();
    target_it.reset();

    // Subtract the centroids from source, target
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> cloud_src_demean,
        cloud_tgt_demean;
    demeanPointCloud(source_it, centroid_src, cloud_src_demean);
    demeanPointCloud(target_it, centroid_tgt, cloud_tgt_demean);

    getTransformationFromCorrelation(cloud_src_demean,
                                     centroid_src,
                                     cloud_tgt_demean,
                                     centroid_tgt,
                                     transformation_matrix);
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationSVD<PointSource, PointTarget, Scalar>::
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

  // Return the correct transformation
  transformation_matrix.template topLeftCorner<3, 3>() = R;
  const Eigen::Matrix<Scalar, 3, 1> Rc(R * centroid_src.template head<3>());
  transformation_matrix.template block<3, 1>(0, 3) =
      centroid_tgt.template head<3>() - Rc;

  if (pcl::console::isVerbosityLevelEnabled(pcl::console::L_DEBUG)) {
    size_t N = cloud_src_demean.cols();
    PCL_DEBUG("[pcl::registration::TransformationEstimationSVD::"
              "getTransformationFromCorrelation] Loss: %.10e\n",
              (cloud_tgt_demean - R * cloud_src_demean).squaredNorm() / N);
  }
}

} // namespace registration
} // namespace pcl

//#define PCL_INSTANTIATE_TransformationEstimationSVD(T,U) template class PCL_EXPORTS
// pcl::registration::TransformationEstimationSVD<T,U>;

#endif /* PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_SVD_HPP_ */
