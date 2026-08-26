/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
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

#ifndef PCL_FEATURES_IMPL_VFH_H_
#define PCL_FEATURES_IMPL_VFH_H_

#include <pcl/features/vfh.h>
#include <pcl/features/pfh_tools.h>
#include <pcl/common/common.h>
#include <pcl/common/centroid.h>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>

#include <riscv_vector.h>

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <vector>
#endif

#if defined(__RVV10__)
namespace pcl::detail
{
template <typename PointT, bool HasNormal = pcl::traits::has_normal<PointT>::value>
struct VFHNormalAoSFloatLayout : std::false_type {};

template <typename PointT>
struct VFHNormalAoSFloatLayout<PointT, true> {
  using Pod = typename pcl::traits::POD<PointT>::type;

  static constexpr std::size_t kNX = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalX;
  static constexpr std::size_t kNY = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalY;
  static constexpr std::size_t kNZ = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalZ;

  static constexpr bool value =
      pcl::rvv::RVVNormalFloatLayout<PointT>::value &&
      std::is_standard_layout_v<Pod> && sizeof(PointT) == sizeof(Pod) &&
      sizeof(PointT) % alignof(float) == 0 && kNX % alignof(float) == 0 &&
      kNY % alignof(float) == 0 && kNZ % alignof(float) == 0;
};

inline int
vfhRVVBinAngularFeature (const float value, const int bins)
{
  const float scaled = static_cast<float> (bins) *
                       ((value + static_cast<float> (M_PI)) *
                        (1.0f / (2.0f * static_cast<float> (M_PI))));
  int bin = static_cast<int> (std::floor (scaled));
  if (bin < 0) return 0;
  if (bin >= bins) return bins - 1;
  return bin;
}

inline int
vfhRVVBinViewpointFeature (const float value, const int bins)
{
  int bin = static_cast<int> (std::floor (static_cast<float> (bins) * value));
  if (bin < 0) return 0;
  if (bin >= bins) return bins - 1;
  return bin;
}

inline vint32m2_t
vfhRVVAngularBins (const vfloat32m2_t values, const int bins, const std::size_t vl)
{
  const float scale = static_cast<float> (bins) *
                      (1.0f / (2.0f * static_cast<float> (M_PI)));
  vfloat32m2_t scaled = __riscv_vfmul_vf_f32m2 (
      __riscv_vfadd_vf_f32m2 (values, static_cast<float> (M_PI), vl), scale, vl);
  scaled = __riscv_vfmin_vf_f32m2 (
      __riscv_vfmax_vf_f32m2 (scaled, 0.0f, vl), static_cast<float> (bins - 1), vl);
  return __riscv_vfcvt_rtz_x_f_v_i32m2 (scaled, vl);
}

inline vint32m2_t
vfhRVVViewpointBins (const vfloat32m2_t values, const int bins, const std::size_t vl)
{
  vfloat32m2_t scaled =
      __riscv_vfmul_vf_f32m2 (values, static_cast<float> (bins), vl);
  scaled = __riscv_vfmin_vf_f32m2 (
      __riscv_vfmax_vf_f32m2 (scaled, 0.0f, vl), static_cast<float> (bins - 1), vl);
  return __riscv_vfcvt_rtz_x_f_v_i32m2 (scaled, vl);
}

template <typename PointNT>
Eigen::Vector4f
computeVFHNormalCentroidRVV (const pcl::PointCloud<PointNT>& normals,
                             const pcl::Indices& indices)
{
  using NormalLayout = VFHNormalAoSFloatLayout<PointNT>;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2 ();
  vfloat32m2_t acc_nx = __riscv_vfmv_v_f_f32m2 (0.0f, vlmax);
  vfloat32m2_t acc_ny = __riscv_vfmv_v_f_f32m2 (0.0f, vlmax);
  vfloat32m2_t acc_nz = __riscv_vfmv_v_f_f32m2 (0.0f, vlmax);
  const auto* base = reinterpret_cast<const std::uint8_t*> (normals.points.data ());

  for (std::size_t offset = 0; offset < indices.size ();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (indices.size () - offset);
    const auto* batch = base + offset * sizeof (PointNT);

    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    pcl::rvv_load::strided_load3_f32m2<sizeof (PointNT), NormalLayout::kNX, NormalLayout::kNY, NormalLayout::kNZ> (
        batch, vl, nx, ny, nz);

    acc_nx = __riscv_vfadd_vv_f32m2_tu (acc_nx, acc_nx, nx, vl);
    acc_ny = __riscv_vfadd_vv_f32m2_tu (acc_ny, acc_ny, ny, vl);
    acc_nz = __riscv_vfadd_vv_f32m2_tu (acc_nz, acc_nz, nz, vl);
    offset += vl;
  }

  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1 (0.0f, 1);
  const float inv_count = 1.0f / static_cast<float> (indices.size ());
  Eigen::Vector4f normal = Eigen::Vector4f::Zero ();
  normal[0] = __riscv_vfmv_f_s_f32m1_f32 (
                  __riscv_vfredosum_vs_f32m2_f32m1 (acc_nx, zero, vlmax)) *
              inv_count;
  normal[1] = __riscv_vfmv_f_s_f32m1_f32 (
                  __riscv_vfredosum_vs_f32m2_f32m1 (acc_ny, zero, vlmax)) *
              inv_count;
  normal[2] = __riscv_vfmv_f_s_f32m1_f32 (
                  __riscv_vfredosum_vs_f32m2_f32m1 (acc_nz, zero, vlmax)) *
              inv_count;
  return normal;
}

template <typename PointInT, typename PointNT>
void
accumulateVFHSPFHRVV (const Eigen::Vector4f& centroid_p,
                      const Eigen::Vector4f& centroid_n,
                      const pcl::PointCloud<PointInT>& cloud,
                      const pcl::PointCloud<PointNT>& normals,
                      const pcl::Indices& indices,
                      const bool normalize_bins,
                      float* histogram)
{
  using PointLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointInT>;
  using NormalLayout = VFHNormalAoSFloatLayout<PointNT>;

  const float hist_incr =
      normalize_bins ? 100.0f / static_cast<float> (indices.size () - 1) : 1.0f;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2 ();
  std::vector<std::int32_t> f1_bins (vlmax);
  std::vector<std::int32_t> f2_bins (vlmax);
  std::vector<std::int32_t> f3_bins (vlmax);
  std::vector<std::int32_t> valid (vlmax);
  const auto* point_base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());
  const auto* normal_base = reinterpret_cast<const std::uint8_t*> (normals.points.data ());

  for (std::size_t offset = 0; offset < indices.size ();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (indices.size () - offset);
    const auto* point_batch = point_base + offset * sizeof (PointInT);
    const auto* normal_batch = normal_base + offset * sizeof (PointNT);

    vfloat32m2_t px;
    vfloat32m2_t py;
    vfloat32m2_t pz;
    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    pcl::rvv_load::strided_load3_f32m2<sizeof (PointInT), PointLayout::kX, PointLayout::kY, PointLayout::kZ> (
        point_batch, vl, px, py, pz);
    pcl::rvv_load::strided_load3_f32m2<sizeof (PointNT), NormalLayout::kNX, NormalLayout::kNY, NormalLayout::kNZ> (
        normal_batch, vl, nx, ny, nz);

    const vfloat32m2_t centroid_x = __riscv_vfmv_v_f_f32m2 (centroid_p[0], vl);
    const vfloat32m2_t centroid_y = __riscv_vfmv_v_f_f32m2 (centroid_p[1], vl);
    const vfloat32m2_t centroid_z = __riscv_vfmv_v_f_f32m2 (centroid_p[2], vl);
    const vfloat32m2_t centroid_nx = __riscv_vfmv_v_f_f32m2 (centroid_n[0], vl);
    const vfloat32m2_t centroid_ny = __riscv_vfmv_v_f_f32m2 (centroid_n[1], vl);
    const vfloat32m2_t centroid_nz = __riscv_vfmv_v_f_f32m2 (centroid_n[2], vl);

    vfloat32m2_t dx = __riscv_vfsub_vv_f32m2 (px, centroid_x, vl);
    vfloat32m2_t dy = __riscv_vfsub_vv_f32m2 (py, centroid_y, vl);
    vfloat32m2_t dz = __riscv_vfsub_vv_f32m2 (pz, centroid_z, vl);

    vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2 (dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2 (dist2, vl);
    const vbool16_t dist_valid = __riscv_vmfne_vf_f32m2_b16 (dist, 0.0f, vl);
    const vfloat32m2_t dist_safe = __riscv_vmerge_vvm_f32m2 (
        __riscv_vfmv_v_f_f32m2 (1.0f, vl), dist, dist_valid, vl);

    const vfloat32m2_t angle1 = __riscv_vfdiv_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (
            __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (centroid_nx, dx, vl), centroid_ny, dy, vl),
            centroid_nz,
            dz,
            vl),
        dist_safe,
        vl);
    const vfloat32m2_t angle2 = __riscv_vfdiv_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (
            __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (nx, dx, vl), ny, dy, vl), nz, dz, vl),
        dist_safe,
        vl);

    const vfloat32m2_t abs_angle1 = __riscv_vfsgnjx_vv_f32m2 (angle1, angle1, vl);
    const vfloat32m2_t abs_angle2 = __riscv_vfsgnjx_vv_f32m2 (angle2, angle2, vl);
    vbool16_t swap = __riscv_vmflt_vv_f32m2_b16 (abs_angle1, abs_angle2, vl);
    swap = __riscv_vmand_mm_b16 (swap, __riscv_vmfle_vf_f32m2_b16 (abs_angle1, 1.0f, vl), vl);
    swap = __riscv_vmand_mm_b16 (swap, __riscv_vmfle_vf_f32m2_b16 (abs_angle2, 1.0f, vl), vl);

    const vfloat32m2_t swapped_dx = __riscv_vfneg_v_f32m2 (dx, vl);
    const vfloat32m2_t swapped_dy = __riscv_vfneg_v_f32m2 (dy, vl);
    const vfloat32m2_t swapped_dz = __riscv_vfneg_v_f32m2 (dz, vl);
    dx = __riscv_vmerge_vvm_f32m2 (dx, swapped_dx, swap, vl);
    dy = __riscv_vmerge_vvm_f32m2 (dy, swapped_dy, swap, vl);
    dz = __riscv_vmerge_vvm_f32m2 (dz, swapped_dz, swap, vl);

    const vfloat32m2_t ux = __riscv_vmerge_vvm_f32m2 (centroid_nx, nx, swap, vl);
    const vfloat32m2_t uy = __riscv_vmerge_vvm_f32m2 (centroid_ny, ny, swap, vl);
    const vfloat32m2_t uz = __riscv_vmerge_vvm_f32m2 (centroid_nz, nz, swap, vl);
    const vfloat32m2_t target_nx = __riscv_vmerge_vvm_f32m2 (nx, centroid_nx, swap, vl);
    const vfloat32m2_t target_ny = __riscv_vmerge_vvm_f32m2 (ny, centroid_ny, swap, vl);
    const vfloat32m2_t target_nz = __riscv_vmerge_vvm_f32m2 (nz, centroid_nz, swap, vl);

    vfloat32m2_t vx = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dy, uz, vl),
                                             __riscv_vfmul_vv_f32m2 (dz, uy, vl),
                                             vl);
    vfloat32m2_t vy = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dz, ux, vl),
                                             __riscv_vfmul_vv_f32m2 (dx, uz, vl),
                                             vl);
    vfloat32m2_t vz = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (dx, uy, vl),
                                             __riscv_vfmul_vv_f32m2 (dy, ux, vl),
                                             vl);
    vfloat32m2_t vnorm2 = __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vx, vx, vl), vy, vy, vl);
    vnorm2 = __riscv_vfmacc_vv_f32m2 (vnorm2, vz, vz, vl);
    const vfloat32m2_t vnorm = __riscv_vfsqrt_v_f32m2 (vnorm2, vl);
    const vbool16_t vnorm_valid = __riscv_vmfne_vf_f32m2_b16 (vnorm, 0.0f, vl);
    const vbool16_t lane_valid = __riscv_vmand_mm_b16 (dist_valid, vnorm_valid, vl);
    const vfloat32m2_t vnorm_safe = __riscv_vmerge_vvm_f32m2 (
        __riscv_vfmv_v_f_f32m2 (1.0f, vl), vnorm, vnorm_valid, vl);
    vx = __riscv_vfdiv_vv_f32m2 (vx, vnorm_safe, vl);
    vy = __riscv_vfdiv_vv_f32m2 (vy, vnorm_safe, vl);
    vz = __riscv_vfdiv_vv_f32m2 (vz, vnorm_safe, vl);

    const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (uy, vz, vl),
                                                   __riscv_vfmul_vv_f32m2 (uz, vy, vl),
                                                   vl);
    const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (uz, vx, vl),
                                                   __riscv_vfmul_vv_f32m2 (ux, vz, vl),
                                                   vl);
    const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (ux, vy, vl),
                                                   __riscv_vfmul_vv_f32m2 (uy, vx, vl),
                                                   vl);
    const vfloat32m2_t f1_v =
        pcl::atan2_RVV_f32m2 (__riscv_vfmacc_vv_f32m2 (
                                  __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (wx, target_nx, vl),
                                                          wy,
                                                          target_ny,
                                                          vl),
                                  wz,
                                  target_nz,
                                  vl),
                              __riscv_vfmacc_vv_f32m2 (
                                  __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (ux, target_nx, vl),
                                                          uy,
                                                          target_ny,
                                                          vl),
                                  uz,
                                  target_nz,
                                  vl),
                              vl);
    const vfloat32m2_t f2_v = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vx, target_nx, vl), vy, target_ny, vl),
        vz,
        target_nz,
        vl);
    const vfloat32m2_t f3_v = __riscv_vmerge_vvm_f32m2 (angle1, __riscv_vfneg_v_f32m2 (angle2, vl), swap, vl);

    __riscv_vse32_v_i32m2 (f1_bins.data (), vfhRVVAngularBins (f1_v, 45, vl), vl);
    __riscv_vse32_v_i32m2 (f2_bins.data (), vfhRVVAngularBins (f2_v, 45, vl), vl);
    __riscv_vse32_v_i32m2 (f3_bins.data (), vfhRVVAngularBins (f3_v, 45, vl), vl);

    const vint32m2_t zero = __riscv_vmv_v_x_i32m2 (0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2 (zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2 (valid.data (), vvalid, vl);

    for (std::size_t lane = 0; lane < vl; ++lane)
    {
      if (valid[lane] == 0)
        continue;
      histogram[f1_bins[lane]] += hist_incr;
      histogram[45 + f2_bins[lane]] += hist_incr;
      histogram[90 + f3_bins[lane]] += hist_incr;
    }

    offset += vl;
  }
}

template <typename PointNT>
void
accumulateVFHViewpointRVV (const Eigen::Vector4f& centroid_p,
                           const pcl::PointCloud<PointNT>& normals,
                           const pcl::Indices& indices,
                           const Eigen::Vector4f& viewpoint,
                           const bool normalize_bins,
                           float* histogram)
{
  using NormalLayout = VFHNormalAoSFloatLayout<PointNT>;

  Eigen::Vector4f d_vp_p = viewpoint - centroid_p;
  d_vp_p.normalize ();
  const float hist_incr =
      normalize_bins ? 100.0f / static_cast<float> (indices.size ()) : 1.0f;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2 ();
  std::vector<std::int32_t> alpha_bins (vlmax);
  const auto* base = reinterpret_cast<const std::uint8_t*> (normals.points.data ());

  for (std::size_t offset = 0; offset < indices.size ();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (indices.size () - offset);
    const auto* batch = base + offset * sizeof (PointNT);

    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    pcl::rvv_load::strided_load3_f32m2<sizeof (PointNT), NormalLayout::kNX, NormalLayout::kNY, NormalLayout::kNZ> (
        batch, vl, nx, ny, nz);

    const vfloat32m2_t dot = __riscv_vfmacc_vf_f32m2 (
        __riscv_vfmacc_vf_f32m2 (__riscv_vfmul_vf_f32m2 (nx, d_vp_p[0], vl), d_vp_p[1], ny, vl),
        d_vp_p[2],
        nz,
        vl);
    const vfloat32m2_t alpha = __riscv_vfmul_vf_f32m2 (
        __riscv_vfadd_vf_f32m2 (dot, 1.0f, vl), 0.5f, vl);
    __riscv_vse32_v_i32m2 (alpha_bins.data (), vfhRVVViewpointBins (alpha, 128, vl), vl);
    for (std::size_t lane = 0; lane < vl; ++lane)
      histogram[180 + alpha_bins[lane]] += hist_incr;
    offset += vl;
  }
}

template <typename PointInT, typename PointNT, typename PointOutT> bool
computeVFHSignatureRVV (const pcl::PointCloud<PointInT>& cloud,
                        const pcl::PointCloud<PointNT>& normals,
                        const pcl::Indices& indices,
                        const Eigen::Vector4f& viewpoint,
                        const bool normalize_bins,
                        const bool normalize_distances,
                        const bool size_component,
                        float* histogram)
{
  if constexpr (!std::is_same_v<PointOutT, pcl::VFHSignature308> ||
                !pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value ||
                !VFHNormalAoSFloatLayout<PointNT>::value)
    return false;
  else
  {
    if (indices.size () < 2 || indices.size () != cloud.size () ||
        cloud.size () != normals.size () || !cloud.is_dense || !normals.is_dense ||
        !normalize_bins || normalize_distances || size_component ||
        cloud.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointInT> () ||
        normals.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ())
      return false;

    for (std::size_t i = 0; i < indices.size (); ++i)
    {
      if (indices[i] != static_cast<pcl::index_t> (i))
        return false;
    }

    std::fill (histogram, histogram + 308, 0.0f);
    Eigen::Vector4f centroid_p = Eigen::Vector4f::Zero ();
    pcl::compute3DCentroid (cloud, indices, centroid_p);
    const Eigen::Vector4f centroid_n = computeVFHNormalCentroidRVV (normals, indices);
    accumulateVFHSPFHRVV (centroid_p, centroid_n, cloud, normals, indices, normalize_bins, histogram);
    accumulateVFHViewpointRVV (centroid_p, normals, indices, viewpoint, normalize_bins, histogram);
    return true;
  }
}
} // namespace pcl::detail
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointInT, typename PointNT, typename PointOutT> bool
pcl::VFHEstimation<PointInT, PointNT, PointOutT>::initCompute ()
{
  if (input_->size () < 2 || (surface_ && surface_->size () < 2))
  {
    PCL_ERROR ("[pcl::VFHEstimation::initCompute] Input dataset must have at least 2 points!\n");
    return (false);
  }
  if (search_radius_ == 0 && k_ == 0)
    k_ = 1;
  return (Feature<PointInT, PointOutT>::initCompute ());
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointInT, typename PointNT, typename PointOutT> void
pcl::VFHEstimation<PointInT, PointNT, PointOutT>::compute (PointCloudOut &output)
{
  if (!initCompute ())
  {
    output.width = output.height = 0;
    output.clear ();
    return;
  }
  // Copy the header
  output.header = input_->header;

  // Resize the output dataset
  // Important! We should only allocate precisely how many elements we will need, otherwise
  // we risk at pre-allocating too much memory which could lead to bad_alloc
  // (see http://dev.pointclouds.org/issues/657)
  output.width = output.height = 1;
  output.is_dense = input_->is_dense;
  output.resize (1);

  // Perform the actual feature computation
  computeFeature (output);

  Feature<PointInT, PointOutT>::deinitCompute ();
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointInT, typename PointNT, typename PointOutT> void
pcl::VFHEstimation<PointInT, PointNT, PointOutT>::computePointSPFHSignature (const Eigen::Vector4f &centroid_p,
                                                                             const Eigen::Vector4f &centroid_n,
                                                                             const pcl::PointCloud<PointInT> &cloud,
                                                                             const pcl::PointCloud<PointNT> &normals,
                                                                             const pcl::Indices &indices)
{
  Eigen::Vector4f pfh_tuple;
  // Reset the whole thing
  for (int i = 0; i < 4; ++i)
  {
    hist_f_[i].setZero (nr_bins_f_[i]);
  }

  // Get the bounding box of the current cluster
  //Eigen::Vector4f min_pt, max_pt;
  //pcl::getMinMax3D (cloud, indices, min_pt, max_pt);
  //double distance_normalization_factor = (std::max)((centroid_p - min_pt).norm (), (centroid_p - max_pt).norm ());

  //Instead of using the bounding box to normalize the VFH distance component, it is better to use the max_distance
  //from any point to centroid. VFH is invariant to rotation about the roll axis but the bounding box is not,
  //resulting in different normalization factors for point clouds that are just rotated about that axis.

  double distance_normalization_factor = 1.0;
  if (normalize_distances_)
  {
    Eigen::Vector4f max_pt;
    pcl::getMaxDistance (cloud, indices, centroid_p, max_pt);
    max_pt[3] = 0;
    distance_normalization_factor = (centroid_p - max_pt).norm ();
  }

  // Factorization constant
  float hist_incr = 1;
  if (normalize_bins_)
    hist_incr = 100.0f / static_cast<float> (indices.size () - 1);

  float hist_incr_size_component = 0;
  if (size_component_)
    hist_incr_size_component = hist_incr;

  // Iterate over all the points in the neighborhood
  for (const auto &index : indices)
  {
    // Compute the pair P to NNi
    if (!computePairFeatures (centroid_p, centroid_n, cloud[index].getVector4fMap (),
                              normals[index].getNormalVector4fMap (), pfh_tuple[0], pfh_tuple[1],
                              pfh_tuple[2], pfh_tuple[3]))
      continue;

    // Normalize the f1, f2, f3, f4 features and push them in the histogram
    for (int i = 0; i < 3; ++i)
    {
      const int raw_index = static_cast<int> (std::floor (nr_bins_f_[i] * ((pfh_tuple[i] + M_PI) * d_pi_)));
      const int h_index = std::max(std::min(raw_index, nr_bins_f_[i] - 1), 0);
      hist_f_[i] (h_index) += hist_incr;
    }

    if (hist_incr_size_component)
    {
      int h_index;
      if (normalize_distances_)
        h_index = static_cast<int> (std::floor (nr_bins_f_[3] * (pfh_tuple[3] / distance_normalization_factor)));
      else
        h_index = static_cast<int> (pcl_round (pfh_tuple[3] * 100));

      h_index = std::max (std::min (h_index, nr_bins_f_[3] - 1), 0);
      hist_f_[3] (h_index) += hist_incr_size_component;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointInT, typename PointNT, typename PointOutT> void
pcl::VFHEstimation<PointInT, PointNT, PointOutT>::computeFeatureStandard (PointCloudOut &output)
{
  // ---[ Step 1a : compute the centroid in XYZ space
  Eigen::Vector4f xyz_centroid (0, 0, 0, 0);

  if (use_given_centroid_)
    xyz_centroid = centroid_to_use_;
  else
    compute3DCentroid (*surface_, *indices_, xyz_centroid);          // Estimate the XYZ centroid

  // ---[ Step 1b : compute the centroid in normal space
  Eigen::Vector4f normal_centroid = Eigen::Vector4f::Zero ();

  // If the data is dense, we don't need to check for NaN
  if (use_given_normal_)
    normal_centroid = normal_to_use_;
  else
  {
    std::size_t cp = 0;
    if (normals_->is_dense)
    {
      for (const auto& index: *indices_)
      {
        normal_centroid.noalias () += (*normals_)[index].getNormalVector4fMap ();
      }
      cp = indices_->size();
    }
    // NaN or Inf values could exist => check for them
    else
    {
      for (const auto& index: *indices_)
      {
        if (!std::isfinite ((*normals_)[index].normal[0]) ||
            !std::isfinite ((*normals_)[index].normal[1]) ||
            !std::isfinite ((*normals_)[index].normal[2]))
          continue;
        normal_centroid.noalias () += (*normals_)[index].getNormalVector4fMap ();
        cp++;
      }
    }
    normal_centroid /= static_cast<float> (cp);
  }

  // Compute the direction of view from the viewpoint to the centroid
  Eigen::Vector4f viewpoint (vpx_, vpy_, vpz_, 0);
  Eigen::Vector4f d_vp_p = viewpoint - xyz_centroid;
  d_vp_p.normalize ();

  // Estimate the SPFH at nn_indices[0] using the entire cloud
  computePointSPFHSignature (xyz_centroid, normal_centroid, *surface_, *normals_, *indices_);

  // ---[ Step 2 : obtain the viewpoint component
  hist_vp_.setZero (nr_bins_vp_);

  float hist_incr = 1.0;
  if (normalize_bins_)
    hist_incr = 100.0 / static_cast<double> (indices_->size ());

  for (const auto& index: *indices_)
  {
    Eigen::Vector4f normal ((*normals_)[index].normal[0],
                            (*normals_)[index].normal[1],
                            (*normals_)[index].normal[2], 0);
    // Normalize
    double alpha = (normal.dot (d_vp_p) + 1.0) * 0.5;
    auto fi = static_cast<std::size_t> (std::floor (alpha * hist_vp_.size ()));
    fi = std::max<std::size_t> (0u, fi);
    fi = std::min<std::size_t> (hist_vp_.size () - 1, fi);
    // Bin into the histogram
    hist_vp_ [fi] += hist_incr;
  }

  // We only output _1_ signature
  output.resize (1);
  output.width = 1;
  output.height = 1;

  // Estimate the FPFH at nn_indices[0] using the entire cloud and copy the resultant signature
  auto outPtr = std::begin (output[0].histogram);

  for (int i = 0; i < 4; ++i)
  {
    outPtr = std::copy (hist_f_[i].data (), hist_f_[i].data () + hist_f_[i].size (), outPtr);
  }
  outPtr = std::copy (hist_vp_.data (), hist_vp_.data () + hist_vp_.size (), outPtr);
}

#if defined (__RVV10__)
template<typename PointInT, typename PointNT, typename PointOutT>
bool
pcl::VFHEstimation<PointInT, PointNT, PointOutT>::computeFeatureRVV (PointCloudOut &output)
{
  if (!use_given_centroid_ && !use_given_normal_)
  {
    output.resize (1);
    output.width = 1;
    output.height = 1;
    const Eigen::Vector4f viewpoint (vpx_, vpy_, vpz_, 0);
    if (pcl::detail::computeVFHSignatureRVV<PointInT, PointNT, PointOutT> (*surface_,
                                                                            *normals_,
                                                                            *indices_,
                                                                            viewpoint,
                                                                            normalize_bins_,
                                                                            normalize_distances_,
                                                                            size_component_,
                                                                            output[0].histogram))
      return true;
  }

  return false;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> void
pcl::VFHEstimation<PointInT, PointNT, PointOutT>::computeFeature (PointCloudOut &output)
{
#if defined(__RVV10__)
  if (computeFeatureRVV (output))
    return;
#endif
  computeFeatureStandard (output);
}

#define PCL_INSTANTIATE_VFHEstimation(T,NT,OutT) template class PCL_EXPORTS pcl::VFHEstimation<T,NT,OutT>;

#endif    // PCL_FEATURES_IMPL_VFH_H_
