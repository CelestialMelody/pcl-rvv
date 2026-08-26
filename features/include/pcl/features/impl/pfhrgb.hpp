/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011, Alexandru-Eugen Ichim
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
 *  $Id$
 */

#ifndef PCL_FEATURES_IMPL_PFHRGB_H_
#define PCL_FEATURES_IMPL_PFHRGB_H_

#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/features/pfhrgb.h>
#include <pcl/point_types.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined(__GNUC__)
#define PCL_PFHRGB_RVV_NOINLINE __attribute__((noinline))
#else
#define PCL_PFHRGB_RVV_NOINLINE
#endif

namespace pcl::pfhrgb_rvv_detail
{
template <typename PointInT, typename PointNT> void
computePointPFHRGBSignatureStd (const pcl::PointCloud<PointInT> &cloud,
                                const pcl::PointCloud<PointNT> &normals,
                                const pcl::Indices &indices,
                                const int nr_split,
                                Eigen::VectorXf &pfhrgb_histogram)
{
  int h_index, h_p;
  int f_index[7];
  float pfhrgb_tuple[7];

  pfhrgb_histogram.setZero ();

  const float d_pi = 1.0f / (2.0f * static_cast<float> (M_PI));
  const float hist_incr = 100.0f / static_cast<float> (indices.size () * (indices.size () - 1) / 2);

  for (const auto& index_i: indices)
  {
    for (const auto& index_j: indices)
    {
      if (index_i == index_j)
        continue;

      Eigen::Vector4i colors1 (cloud[index_i].r, cloud[index_i].g, cloud[index_i].b, 0),
          colors2 (cloud[index_j].r, cloud[index_j].g, cloud[index_j].b, 0);

      if (!pcl::computeRGBPairFeatures (cloud[index_i].getVector4fMap (), normals[index_i].getNormalVector4fMap (),
                                        colors1,
                                        cloud[index_j].getVector4fMap (), normals[index_j].getNormalVector4fMap (),
                                        colors2,
                                        pfhrgb_tuple[0], pfhrgb_tuple[1], pfhrgb_tuple[2], pfhrgb_tuple[3],
                                        pfhrgb_tuple[4], pfhrgb_tuple[5], pfhrgb_tuple[6]))
        continue;

      f_index[0] = static_cast<int> (std::floor (nr_split * ((pfhrgb_tuple[0] + M_PI) * d_pi)));
      for (int i = 1; i < 3; ++i)
      {
        const float feature_value = nr_split * ((pfhrgb_tuple[i] + 1.0) * 0.5);
        f_index[i] = static_cast<int> (std::floor (feature_value));
      }
      for (int i = 4; i < 7; ++i)
      {
        const float feature_value = nr_split * ((pfhrgb_tuple[i] + 1.0) * 0.5);
        f_index[i] = static_cast<int> (std::floor (feature_value));
      }
      for (auto& feature: f_index)
      {
        feature = std::min(nr_split - 1, std::max(0, feature));
      }

      h_index = 0;
      h_p     = 1;
      for (int d = 0; d < 3; ++d)
      {
        h_index += h_p * f_index[d];
        h_p     *= nr_split;
      }
      pfhrgb_histogram[h_index] += hist_incr;

      h_index = 125;
      h_p     = 1;
      for (int d = 4; d < 7; ++d)
      {
        h_index += h_p * f_index[d];
        h_p     *= nr_split;
      }
      pfhrgb_histogram[h_index] += hist_incr;
    }
  }
}

#if defined(__RVV10__)
struct PFHRGBPairBatchStaging
{
  std::vector<float> p1x, p1y, p1z;
  std::vector<float> p2x, p2y, p2z;
  std::vector<float> n1x, n1y, n1z;
  std::vector<float> n2x, n2y, n2z;
  std::vector<float> r1, g1, b1;
  std::vector<float> r2, g2, b2;
};

struct PFHRGBPairBatchWorkspace
{
  PFHRGBPairBatchStaging staging;
  std::vector<float> f1, f2, f3, f5, f6, f7;
  std::vector<std::int32_t> valid;
};

inline void
reservePairBatch (PFHRGBPairBatchStaging& staging, const std::size_t size)
{
  staging.p1x.reserve (size);
  staging.p1y.reserve (size);
  staging.p1z.reserve (size);
  staging.p2x.reserve (size);
  staging.p2y.reserve (size);
  staging.p2z.reserve (size);
  staging.n1x.reserve (size);
  staging.n1y.reserve (size);
  staging.n1z.reserve (size);
  staging.n2x.reserve (size);
  staging.n2y.reserve (size);
  staging.n2z.reserve (size);
  staging.r1.reserve (size);
  staging.g1.reserve (size);
  staging.b1.reserve (size);
  staging.r2.reserve (size);
  staging.g2.reserve (size);
  staging.b2.reserve (size);
}

inline void
clearPairBatch (PFHRGBPairBatchStaging& staging)
{
  staging.p1x.clear ();
  staging.p1y.clear ();
  staging.p1z.clear ();
  staging.p2x.clear ();
  staging.p2y.clear ();
  staging.p2z.clear ();
  staging.n1x.clear ();
  staging.n1y.clear ();
  staging.n1z.clear ();
  staging.n2x.clear ();
  staging.n2y.clear ();
  staging.n2z.clear ();
  staging.r1.clear ();
  staging.g1.clear ();
  staging.b1.clear ();
  staging.r2.clear ();
  staging.g2.clear ();
  staging.b2.clear ();
}

inline void
appendPair (PFHRGBPairBatchStaging& staging,
            const pcl::PointXYZRGBNormal& p1,
            const pcl::PointXYZRGBNormal& p2,
            const pcl::PointXYZRGBNormal& n1,
            const pcl::PointXYZRGBNormal& n2)
{
  staging.p1x.push_back (p1.x);
  staging.p1y.push_back (p1.y);
  staging.p1z.push_back (p1.z);
  staging.p2x.push_back (p2.x);
  staging.p2y.push_back (p2.y);
  staging.p2z.push_back (p2.z);
  staging.n1x.push_back (n1.normal_x);
  staging.n1y.push_back (n1.normal_y);
  staging.n1z.push_back (n1.normal_z);
  staging.n2x.push_back (n2.normal_x);
  staging.n2y.push_back (n2.normal_y);
  staging.n2z.push_back (n2.normal_z);
  staging.r1.push_back (static_cast<float> (p1.r));
  staging.g1.push_back (static_cast<float> (p1.g));
  staging.b1.push_back (static_cast<float> (p1.b));
  staging.r2.push_back (static_cast<float> (p2.r));
  staging.g2.push_back (static_cast<float> (p2.g));
  staging.b2.push_back (static_cast<float> (p2.b));
}

inline void
fillPairBatchStaging (PFHRGBPairBatchStaging& staging,
                      const pcl::PointCloud<pcl::PointXYZRGBNormal> &cloud,
                      const pcl::PointCloud<pcl::PointXYZRGBNormal> &normals,
                      const pcl::Indices &indices)
{
  clearPairBatch (staging);
  reservePairBatch (staging, indices.size () * (indices.size () - 1));
  for (const auto index_i : indices)
  {
    for (const auto index_j : indices)
    {
      if (index_i == index_j)
        continue;
      appendPair (staging,
                  cloud[static_cast<std::size_t> (index_i)],
                  cloud[static_cast<std::size_t> (index_j)],
                  normals[static_cast<std::size_t> (index_i)],
                  normals[static_cast<std::size_t> (index_j)]);
    }
  }
}

inline vfloat32m2_t
computeColorRatioRVV (const vfloat32m2_t numerator, const vfloat32m2_t denominator, const std::size_t vl)
{
  const vbool16_t non_zero = __riscv_vmfne_vf_f32m2_b16 (denominator, 0.0f, vl);
  const vfloat32m2_t raw_ratio = __riscv_vfdiv_vv_f32m2 (numerator, denominator, vl);
  const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
  vfloat32m2_t ratio = __riscv_vmerge_vvm_f32m2 (one, raw_ratio, non_zero, vl);
  const vbool16_t greater_than_one = __riscv_vmfgt_vf_f32m2_b16 (ratio, 1.0f, vl);
  const vfloat32m2_t inverted = __riscv_vfneg_v_f32m2 (__riscv_vfrdiv_vf_f32m2 (ratio, 1.0f, vl), vl);
  ratio = __riscv_vmerge_vvm_f32m2 (ratio, inverted, greater_than_one, vl);
  return (ratio);
}

inline void
computePairTuplesRVV (const PFHRGBPairBatchStaging& staging,
                      std::vector<float>& f1,
                      std::vector<float>& f2,
                      std::vector<float>& f3,
                      std::vector<float>& f5,
                      std::vector<float>& f6,
                      std::vector<float>& f7,
                      std::vector<std::int32_t>& valid)
{
  const std::size_t pair_count = staging.p1x.size ();
  f1.resize (pair_count);
  f2.resize (pair_count);
  f3.resize (pair_count);
  f5.resize (pair_count);
  f6.resize (pair_count);
  f7.resize (pair_count);
  valid.resize (pair_count);

  for (std::size_t offset = 0; offset < pair_count;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (pair_count - offset);
    const vfloat32m2_t p1x = __riscv_vle32_v_f32m2 (staging.p1x.data () + offset, vl);
    const vfloat32m2_t p1y = __riscv_vle32_v_f32m2 (staging.p1y.data () + offset, vl);
    const vfloat32m2_t p1z = __riscv_vle32_v_f32m2 (staging.p1z.data () + offset, vl);
    const vfloat32m2_t p2x = __riscv_vle32_v_f32m2 (staging.p2x.data () + offset, vl);
    const vfloat32m2_t p2y = __riscv_vle32_v_f32m2 (staging.p2y.data () + offset, vl);
    const vfloat32m2_t p2z = __riscv_vle32_v_f32m2 (staging.p2z.data () + offset, vl);
    const vfloat32m2_t n1x = __riscv_vle32_v_f32m2 (staging.n1x.data () + offset, vl);
    const vfloat32m2_t n1y = __riscv_vle32_v_f32m2 (staging.n1y.data () + offset, vl);
    const vfloat32m2_t n1z = __riscv_vle32_v_f32m2 (staging.n1z.data () + offset, vl);
    const vfloat32m2_t n2x = __riscv_vle32_v_f32m2 (staging.n2x.data () + offset, vl);
    const vfloat32m2_t n2y = __riscv_vle32_v_f32m2 (staging.n2y.data () + offset, vl);
    const vfloat32m2_t n2z = __riscv_vle32_v_f32m2 (staging.n2z.data () + offset, vl);

    const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (p2x, p1x, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (p2y, p1y, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2 (p2z, p1z, vl);
    vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2 (dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2 (dist2, vl);
    vbool16_t lane_valid = __riscv_vmfne_vf_f32m2_b16 (dist, 0.0f, vl);

    const vfloat32m2_t n1_dot_delta = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n1x, dx, vl), n1y, dy, vl), n1z, dz, vl);
    const vfloat32m2_t vf3 = __riscv_vfdiv_vv_f32m2 (n1_dot_delta, dist, vl);

    vfloat32m2_t vx = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dy, n1z, vl),
                                             __riscv_vfmul_vv_f32m2 (dz, n1y, vl),
                                             vl);
    vfloat32m2_t vy = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dz, n1x, vl),
                                             __riscv_vfmul_vv_f32m2 (dx, n1z, vl),
                                             vl);
    vfloat32m2_t vz = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, n1y, vl),
                                             __riscv_vfmul_vv_f32m2 (dy, n1x, vl),
                                             vl);
    vfloat32m2_t vnorm2 = __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vx, vx, vl), vy, vy, vl);
    vnorm2 = __riscv_vfmacc_vv_f32m2 (vnorm2, vz, vz, vl);
    const vfloat32m2_t vnorm = __riscv_vfsqrt_v_f32m2 (vnorm2, vl);
    lane_valid = __riscv_vmand_mm_b16 (lane_valid, __riscv_vmfne_vf_f32m2_b16 (vnorm, 0.0f, vl), vl);
    vx = __riscv_vfdiv_vv_f32m2 (vx, vnorm, vl);
    vy = __riscv_vfdiv_vv_f32m2 (vy, vnorm, vl);
    vz = __riscv_vfdiv_vv_f32m2 (vz, vnorm, vl);

    const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n1y, vz, vl),
                                                   __riscv_vfmul_vv_f32m2 (n1z, vy, vl),
                                                   vl);
    const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n1z, vx, vl),
                                                   __riscv_vfmul_vv_f32m2 (n1x, vz, vl),
                                                   vl);
    const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n1x, vy, vl),
                                                   __riscv_vfmul_vv_f32m2 (n1y, vx, vl),
                                                   vl);
    const vfloat32m2_t vf2 = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vx, n2x, vl), vy, n2y, vl),
        vz,
        n2z,
        vl);
    const vfloat32m2_t atan_y = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (wx, n2x, vl), wy, n2y, vl),
        wz,
        n2z,
        vl);
    const vfloat32m2_t atan_x = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (n1x, n2x, vl), n1y, n2y, vl),
        n1z,
        n2z,
        vl);
    const vfloat32m2_t vf1 = pcl::atan2_RVV_f32m2 (atan_y, atan_x, vl);

    const vfloat32m2_t vf5 = computeColorRatioRVV (
        __riscv_vle32_v_f32m2 (staging.r1.data () + offset, vl),
        __riscv_vle32_v_f32m2 (staging.r2.data () + offset, vl),
        vl);
    const vfloat32m2_t vf6 = computeColorRatioRVV (
        __riscv_vle32_v_f32m2 (staging.g1.data () + offset, vl),
        __riscv_vle32_v_f32m2 (staging.g2.data () + offset, vl),
        vl);
    const vfloat32m2_t vf7 = computeColorRatioRVV (
        __riscv_vle32_v_f32m2 (staging.b1.data () + offset, vl),
        __riscv_vle32_v_f32m2 (staging.b2.data () + offset, vl),
        vl);

    __riscv_vse32_v_f32m2 (f1.data () + offset, vf1, vl);
    __riscv_vse32_v_f32m2 (f2.data () + offset, vf2, vl);
    __riscv_vse32_v_f32m2 (f3.data () + offset, vf3, vl);
    __riscv_vse32_v_f32m2 (f5.data () + offset, vf5, vl);
    __riscv_vse32_v_f32m2 (f6.data () + offset, vf6, vl);
    __riscv_vse32_v_f32m2 (f7.data () + offset, vf7, vl);
    const vint32m2_t zero = __riscv_vmv_v_x_i32m2 (0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2 (zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2 (valid.data () + offset, vvalid, vl);
    offset += vl;
  }
}

inline int
binUnitFeature (const float value, const int bins)
{
  const float scaled = static_cast<float> (bins) * ((value + 1.0f) * 0.5f);
  int bin = static_cast<int> (std::floor (scaled));
  return (std::min (bins - 1, std::max (0, bin)));
}

inline int
binAngularFeature (const float value, const int bins)
{
  const float scaled = static_cast<float> (bins) *
                       ((value + static_cast<float> (M_PI)) *
                        (1.0f / (2.0f * static_cast<float> (M_PI))));
  int bin = static_cast<int> (std::floor (scaled));
  return (std::min (bins - 1, std::max (0, bin)));
}

inline void
accumulatePFHRGBHistogramBins (const float f1,
                               const float f2,
                               const float f3,
                               const float f5,
                               const float f6,
                               const float f7,
                               const int nr_split,
                               const float hist_incr,
                               Eigen::VectorXf& histogram)
{
  const int f1_bin = binAngularFeature (f1, nr_split);
  const int f2_bin = binUnitFeature (f2, nr_split);
  const int f3_bin = binUnitFeature (f3, nr_split);
  const int f5_bin = binUnitFeature (f5, nr_split);
  const int f6_bin = binUnitFeature (f6, nr_split);
  const int f7_bin = binUnitFeature (f7, nr_split);

  histogram[f1_bin + nr_split * f2_bin + nr_split * nr_split * f3_bin] += hist_incr;
  histogram[125 + f5_bin + nr_split * f6_bin + nr_split * nr_split * f7_bin] += hist_incr;
}

PCL_PFHRGB_RVV_NOINLINE inline bool
computePointPFHRGBSignatureRVV (const pcl::PointCloud<pcl::PointXYZRGBNormal> &cloud,
                                const pcl::PointCloud<pcl::PointXYZRGBNormal> &normals,
                                const pcl::Indices &indices,
                                const int nr_split,
                                Eigen::VectorXf &pfhrgb_histogram,
                                PFHRGBPairBatchWorkspace& workspace)
{
  if (nr_split != 5 || indices.size () < 4)
    return (false);

  pfhrgb_histogram.setZero (250);
  const float hist_incr = 100.0f / static_cast<float> (indices.size () * (indices.size () - 1) / 2);
  fillPairBatchStaging (workspace.staging, cloud, normals, indices);
  computePairTuplesRVV (workspace.staging,
                        workspace.f1,
                        workspace.f2,
                        workspace.f3,
                        workspace.f5,
                        workspace.f6,
                        workspace.f7,
                        workspace.valid);

  for (std::size_t i = 0; i < workspace.valid.size (); ++i)
  {
    if (workspace.valid[i] == 0)
      continue;
    accumulatePFHRGBHistogramBins (workspace.f1[i],
                                   workspace.f2[i],
                                   workspace.f3[i],
                                   workspace.f5[i],
                                   workspace.f6[i],
                                   workspace.f7[i],
                                   nr_split,
                                   hist_incr,
                                   pfhrgb_histogram);
  }
  return (true);
}
#endif
} // namespace pcl::pfhrgb_rvv_detail

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> bool
pcl::PFHRGBEstimation<PointInT, PointNT, PointOutT>::computeRGBPairFeatures (
    const pcl::PointCloud<PointInT> &cloud, const pcl::PointCloud<PointNT> &normals,
    int p_idx, int q_idx,
    float &f1, float &f2, float &f3, float &f4, float &f5, float &f6, float &f7)
{
  Eigen::Vector4i colors1 (cloud[p_idx].r, cloud[p_idx].g, cloud[p_idx].b, 0),
      colors2 (cloud[q_idx].r, cloud[q_idx].g, cloud[q_idx].b, 0);
  pcl::computeRGBPairFeatures (cloud[p_idx].getVector4fMap (), normals[p_idx].getNormalVector4fMap (),
                               colors1,
                               cloud[q_idx].getVector4fMap (), normals[q_idx].getNormalVector4fMap (),
                               colors2,
                               f1, f2, f3, f4, f5, f6, f7);
  return (true);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> void
pcl::PFHRGBEstimation<PointInT, PointNT, PointOutT>::computePointPFHRGBSignature (
    const pcl::PointCloud<PointInT> &cloud, const pcl::PointCloud<PointNT> &normals,
    const pcl::Indices &indices, int nr_split, Eigen::VectorXf &pfhrgb_histogram)
{
#if defined(__RVV10__)
  if constexpr (std::is_same_v<PointInT, pcl::PointXYZRGBNormal> &&
                std::is_same_v<PointNT, pcl::PointXYZRGBNormal> &&
                std::is_same_v<PointOutT, pcl::PFHRGBSignature250>)
  {
    pcl::pfhrgb_rvv_detail::PFHRGBPairBatchWorkspace workspace;
    if (pcl::pfhrgb_rvv_detail::computePointPFHRGBSignatureRVV (cloud,
                                                                normals,
                                                                indices,
                                                                nr_split,
                                                                pfhrgb_histogram,
                                                                workspace))
      return;
  }
#endif
  pcl::pfhrgb_rvv_detail::computePointPFHRGBSignatureStd (cloud, normals, indices, nr_split, pfhrgb_histogram);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> void
pcl::PFHRGBEstimation<PointInT, PointNT, PointOutT>::computeFeature (PointCloudOut &output)
{
  /// nr_subdiv^3 for RGB and nr_subdiv^3 for the angular features
  pfhrgb_histogram_.setZero (static_cast<Eigen::Index>(2) * nr_subdiv_ * nr_subdiv_ * nr_subdiv_);
  pfhrgb_tuple_.setZero (7);

  // Allocate enough space to hold the results
  // \note This resize is irrelevant for a radiusSearch ().
  pcl::Indices nn_indices (k_);
  std::vector<float> nn_dists (k_);

  // Iterating over the entire index vector
#if defined(__RVV10__)
  pcl::pfhrgb_rvv_detail::PFHRGBPairBatchWorkspace rvv_workspace;
#endif
  for (std::size_t idx = 0; idx < indices_->size (); ++idx)
  {
    this->searchForNeighbors ((*indices_)[idx], search_parameter_, nn_indices, nn_dists);

    // Estimate the PFH signature at each patch
#if defined(__RVV10__)
    if constexpr (std::is_same_v<PointInT, pcl::PointXYZRGBNormal> &&
                  std::is_same_v<PointNT, pcl::PointXYZRGBNormal> &&
                  std::is_same_v<PointOutT, pcl::PFHRGBSignature250>)
    {
      if (!pcl::pfhrgb_rvv_detail::computePointPFHRGBSignatureRVV (*surface_,
                                                                   *normals_,
                                                                   nn_indices,
                                                                   nr_subdiv_,
                                                                   pfhrgb_histogram_,
                                                                   rvv_workspace))
        pcl::pfhrgb_rvv_detail::computePointPFHRGBSignatureStd (*surface_, *normals_, nn_indices, nr_subdiv_, pfhrgb_histogram_);
    }
    else
#endif
    {
      computePointPFHRGBSignature (*surface_, *normals_, nn_indices, nr_subdiv_, pfhrgb_histogram_);
    }

    std::copy (pfhrgb_histogram_.data (), pfhrgb_histogram_.data () + pfhrgb_histogram_.size (),
                 output[idx].histogram);
  }
}

#define PCL_INSTANTIATE_PFHRGBEstimation(T,NT,OutT) template class PCL_EXPORTS pcl::PFHRGBEstimation<T,NT,OutT>;

#undef PCL_PFHRGB_RVV_NOINLINE

#endif /* PCL_FEATURES_IMPL_PFHRGB_H_ */
