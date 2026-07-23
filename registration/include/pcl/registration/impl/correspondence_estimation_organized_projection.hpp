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

#ifndef PCL_REGISTRATION_CORRESPONDENCE_ESTIMATION_ORGANIZED_PROJECTION_IMPL_HPP_
#define PCL_REGISTRATION_CORRESPONDENCE_ESTIMATION_ORGANIZED_PROJECTION_IMPL_HPP_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/field_traits.h>
#endif

namespace pcl {

namespace registration {

namespace detail {

struct OrganizedProjectionCandidate {
  std::uint32_t source_index;
  float x;
  float y;
  float z;
};

struct ProjectedOrganizedProjectionCandidate {
  std::uint32_t source_index;
  std::uint32_t target_index;
  float x;
  float y;
  float z;
};

struct AcceptedOrganizedProjectionCandidate {
  std::uint32_t source_index;
  std::uint32_t target_index;
  float distance;
};

#if defined(__RVV10__)

inline vbool16_t
organizedProjectionFiniteMask(vfloat32m2_t values, const std::size_t vl)
{
  const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16(values, values, vl);
  const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2(values, vl);
  const vfloat32m2_t inf_v =
      __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16(abs_v, inf_v, vl);
  return __riscv_vmand_mm_b16(eq_self, not_inf, vl);
}

inline bool
organizedProjectionTransformIsIdentity(const Eigen::Matrix4f& transform)
{
  return transform(0, 0) == 1.0f && transform(0, 1) == 0.0f &&
         transform(0, 2) == 0.0f && transform(0, 3) == 0.0f &&
         transform(1, 0) == 0.0f && transform(1, 1) == 1.0f &&
         transform(1, 2) == 0.0f && transform(1, 3) == 0.0f &&
         transform(2, 0) == 0.0f && transform(2, 1) == 0.0f &&
         transform(2, 2) == 1.0f && transform(2, 3) == 0.0f &&
         transform(3, 0) == 0.0f && transform(3, 1) == 0.0f &&
         transform(3, 2) == 0.0f && transform(3, 3) == 1.0f;
}

inline vbool16_t
organizedProjectionDistanceThresholdMask(vfloat32m2_t dist,
                                         const double max_distance,
                                         const std::size_t vl)
{
  const float distance_threshold = static_cast<float>(max_distance);
  return static_cast<double>(distance_threshold) < max_distance
             ? __riscv_vmfle_vf_f32m2_b16(dist, distance_threshold, vl)
             : __riscv_vmflt_vf_f32m2_b16(dist, distance_threshold, vl);
}

template <typename PointSource, typename PointTarget>
inline bool
projectOrganizedProjectionCandidatesRVV(
    const pcl::PointCloud<PointSource>& input,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& indices,
    const Eigen::Matrix4f& src_to_tgt_transformation,
    std::vector<OrganizedProjectionCandidate>& candidates)
{
  if constexpr (!pcl::rvv::RVVXYZFloatLayout<PointSource>::value ||
                !pcl::rvv::RVVXYZFloatLayout<PointTarget>::value) {
    return false;
  } else {
    constexpr std::size_t kXOff =
        pcl::traits::offset<PointSource, pcl::fields::x>::value;
    constexpr std::size_t kYOff =
        pcl::traits::offset<PointSource, pcl::fields::y>::value;
    constexpr std::size_t kZOff =
        pcl::traits::offset<PointSource, pcl::fields::z>::value;

    const std::size_t n = indices.size();
    if (n < 64 ||
        n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        input.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() ||
        target.width >
            static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        target.height >
            static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
      return false;

    candidates.resize(n);
    auto* out = candidates.data();
    std::size_t kept = 0;
    std::size_t i = 0;
    const auto* base = reinterpret_cast<const std::uint8_t*>(input.points.data());

    const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
    if (vlmax > 64)
      return false;

    alignas(16) std::uint32_t source_buf[64];
    alignas(16) float x_buf[64];
    alignas(16) float y_buf[64];
    alignas(16) float z_buf[64];

    while (i < n) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
      const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
      const vuint32m2_t offsets = __riscv_vmul_vx_u32m2(
          v_indices_u, static_cast<std::uint32_t>(sizeof(PointSource)), vl);

      vfloat32m2_t x;
      vfloat32m2_t y;
      vfloat32m2_t z;
      pcl::rvv_load::indexed_load3_fields_f32m2<
          typename pcl::traits::POD<PointSource>::type, kXOff, kYOff, kZOff>(
          base, offsets, vl, x, y, z);

      vfloat32m2_t tx;
      vfloat32m2_t ty;
      vfloat32m2_t tz;
      if (organizedProjectionTransformIsIdentity(src_to_tgt_transformation)) {
        tx = x;
        ty = y;
        tz = z;
      } else {
        // Match Eigen's scalar row-dot lowering for Matrix4f * getVector4fMap():
        // (m0*x + m1*y) + (m2*z + m3*w), with w fixed to 1.
        vfloat32m2_t tx_lo =
            __riscv_vfmul_vf_f32m2(y, src_to_tgt_transformation(0, 1), vl);
        tx_lo =
            __riscv_vfmacc_vf_f32m2(tx_lo, src_to_tgt_transformation(0, 0), x, vl);
        vfloat32m2_t tx_hi =
            __riscv_vfmv_v_f_f32m2(src_to_tgt_transformation(0, 3), vl);
        tx_hi =
            __riscv_vfmacc_vf_f32m2(tx_hi, src_to_tgt_transformation(0, 2), z, vl);
        tx = __riscv_vfadd_vv_f32m2(tx_hi, tx_lo, vl);

        vfloat32m2_t ty_lo =
            __riscv_vfmul_vf_f32m2(y, src_to_tgt_transformation(1, 1), vl);
        ty_lo =
            __riscv_vfmacc_vf_f32m2(ty_lo, src_to_tgt_transformation(1, 0), x, vl);
        vfloat32m2_t ty_hi =
            __riscv_vfmv_v_f_f32m2(src_to_tgt_transformation(1, 3), vl);
        ty_hi =
            __riscv_vfmacc_vf_f32m2(ty_hi, src_to_tgt_transformation(1, 2), z, vl);
        ty = __riscv_vfadd_vv_f32m2(ty_hi, ty_lo, vl);

        vfloat32m2_t tz_lo =
            __riscv_vfmul_vf_f32m2(y, src_to_tgt_transformation(2, 1), vl);
        tz_lo =
            __riscv_vfmacc_vf_f32m2(tz_lo, src_to_tgt_transformation(2, 0), x, vl);
        vfloat32m2_t tz_hi =
            __riscv_vfmv_v_f_f32m2(src_to_tgt_transformation(2, 3), vl);
        tz_hi =
            __riscv_vfmacc_vf_f32m2(tz_hi, src_to_tgt_transformation(2, 2), z, vl);
        tz = __riscv_vfadd_vv_f32m2(tz_hi, tz_lo, vl);
      }

      // This helper stages source validity, transform and positive transformed
      // depth. Later RVV helpers may stage projected pixels and target predicates;
      // correspondence append stays scalar to preserve output order.
      vbool16_t keep = __riscv_vmand_mm_b16(
          __riscv_vmand_mm_b16(organizedProjectionFiniteMask(x, vl),
                               organizedProjectionFiniteMask(y, vl),
                               vl),
          organizedProjectionFiniteMask(z, vl),
          vl);
      keep = __riscv_vmand_mm_b16(keep,
                                  __riscv_vmfgt_vf_f32m2_b16(tz, 0.0f, vl),
                                  vl);

      const vuint32m2_t source_kept =
          __riscv_vcompress_vm_u32m2(v_indices_u, keep, vl);
      const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(tx, keep, vl);
      const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(ty, keep, vl);
      const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(tz, keep, vl);
      const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

      __riscv_vse32_v_u32m2(source_buf, source_kept, keep_count);
      __riscv_vse32_v_f32m2(x_buf, x_kept, keep_count);
      __riscv_vse32_v_f32m2(y_buf, y_kept, keep_count);
      __riscv_vse32_v_f32m2(z_buf, z_kept, keep_count);

      for (std::size_t lane = 0; lane < keep_count; ++lane)
        out[kept + lane] =
            OrganizedProjectionCandidate{source_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};

      kept += keep_count;
      i += vl;
    }

    candidates.resize(kept);
    return true;
  }
}

template <typename PointTarget>
inline bool
projectOrganizedProjectionPixelsRVV(
    const pcl::PointCloud<PointTarget>& target,
    const Eigen::Matrix3f& projection_matrix,
    const std::vector<OrganizedProjectionCandidate>& candidates,
    std::vector<ProjectedOrganizedProjectionCandidate>& projected)
{
  const std::size_t n = candidates.size();
  if (n < 64 ||
      n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
      target.size() >
          static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
      target.width >
          static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
      target.height >
          static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    return false;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  if (vlmax > 64)
    return false;

  projected.resize(n);
  auto* out = projected.data();
  std::size_t kept = 0;
  std::size_t i = 0;

  alignas(16) std::uint32_t source_buf[64];
  alignas(16) std::uint32_t target_buf[64];
  alignas(16) float x_buf[64];
  alignas(16) float y_buf[64];
  alignas(16) float z_buf[64];

  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* base = candidates.data() + i;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
    const vuint32m2_t offsets = __riscv_vmul_vx_u32m2(
        __riscv_vid_v_u32m2(vl),
        static_cast<std::uint32_t>(sizeof(OrganizedProjectionCandidate)),
        vl);

    const vuint32m2_t source =
        __riscv_vluxei32_v_u32m2(reinterpret_cast<const std::uint32_t*>(bytes),
                                 offsets,
                                 vl);
    const vfloat32m2_t x = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(
            bytes + offsetof(OrganizedProjectionCandidate, x)),
        offsets,
        vl);
    const vfloat32m2_t y = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(
            bytes + offsetof(OrganizedProjectionCandidate, y)),
        offsets,
        vl);
    const vfloat32m2_t z = __riscv_vluxei32_v_f32m2(
        reinterpret_cast<const float*>(
            bytes + offsetof(OrganizedProjectionCandidate, z)),
        offsets,
        vl);

    // Pixel staging uses the same projection expression as the scalar tail.
    // The fused multiply-add shape keeps truncation at pixel boundaries aligned
    // with the scalar RVV build's contracted expression.
    vfloat32m2_t uv0 = __riscv_vfmul_vf_f32m2(z, projection_matrix(0, 2), vl);
    uv0 = __riscv_vfmacc_vf_f32m2(uv0, projection_matrix(0, 0), x, vl);
    vfloat32m2_t uv1 = __riscv_vfmul_vf_f32m2(z, projection_matrix(1, 2), vl);
    uv1 = __riscv_vfmacc_vf_f32m2(uv1, projection_matrix(1, 1), y, vl);

    const vint32m2_t u =
        __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv0, z, vl), vl);
    const vint32m2_t v =
        __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv1, z, vl), vl);

    vbool16_t keep = __riscv_vmsge_vx_i32m2_b16(u, 0, vl);
    keep = __riscv_vmand_mm_b16(
        keep,
        __riscv_vmslt_vx_i32m2_b16(u, static_cast<int>(target.width), vl),
        vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmsge_vx_i32m2_b16(v, 0, vl), vl);
    keep = __riscv_vmand_mm_b16(
        keep,
        __riscv_vmslt_vx_i32m2_b16(v, static_cast<int>(target.height), vl),
        vl);

    const vint32m2_t target_index_i = __riscv_vadd_vv_i32m2(
        __riscv_vmul_vx_i32m2(v, static_cast<int>(target.width), vl), u, vl);
    const vuint32m2_t target_index =
        __riscv_vreinterpret_v_i32m2_u32m2(target_index_i);

    const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(source, keep, vl);
    const vuint32m2_t target_kept =
        __riscv_vcompress_vm_u32m2(target_index, keep, vl);
    const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(x, keep, vl);
    const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(y, keep, vl);
    const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(z, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

    __riscv_vse32_v_u32m2(source_buf, source_kept, keep_count);
    __riscv_vse32_v_u32m2(target_buf, target_kept, keep_count);
    __riscv_vse32_v_f32m2(x_buf, x_kept, keep_count);
    __riscv_vse32_v_f32m2(y_buf, y_kept, keep_count);
    __riscv_vse32_v_f32m2(z_buf, z_kept, keep_count);

    for (std::size_t lane = 0; lane < keep_count; ++lane)
      out[kept + lane] = ProjectedOrganizedProjectionCandidate{
          source_buf[lane], target_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};

    kept += keep_count;
    i += vl;
  }

  projected.resize(kept);
  return true;
}

template <typename PointTarget>
inline bool
acceptProjectedOrganizedProjectionCandidatesRVV(
    const pcl::PointCloud<PointTarget>& target,
    const float depth_threshold,
    const double max_distance,
    const std::vector<ProjectedOrganizedProjectionCandidate>& projected,
    std::vector<AcceptedOrganizedProjectionCandidate>& accepted)
{
  if constexpr (!pcl::rvv::RVVXYZFloatLayout<PointTarget>::value) {
    return false;
  } else {
    constexpr std::size_t kXOff =
        pcl::traits::offset<PointTarget, pcl::fields::x>::value;
    constexpr std::size_t kYOff =
        pcl::traits::offset<PointTarget, pcl::fields::y>::value;
    constexpr std::size_t kZOff =
        pcl::traits::offset<PointTarget, pcl::fields::z>::value;

    const std::size_t n = projected.size();
    if (n < 64 ||
        n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        target.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>())
      return false;

    const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
    if (vlmax > 64)
      return false;

    accepted.resize(n);
    std::size_t kept = 0;
    std::size_t i = 0;
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(target.points.data());

    alignas(16) std::uint32_t source_buf[64];
    alignas(16) std::uint32_t target_buf[64];
    alignas(16) float sx_buf[64];
    alignas(16) float sy_buf[64];
    alignas(16) float sz_buf[64];
    alignas(16) float tx_buf[64];
    alignas(16) float ty_buf[64];
    alignas(16) float tz_buf[64];

    while (i < n) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const auto* base = projected.data() + i;
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
      const vuint32m2_t projected_offsets = __riscv_vmul_vx_u32m2(
          __riscv_vid_v_u32m2(vl),
          static_cast<std::uint32_t>(sizeof(ProjectedOrganizedProjectionCandidate)),
          vl);

      const vuint32m2_t source =
          __riscv_vluxei32_v_u32m2(reinterpret_cast<const std::uint32_t*>(bytes),
                                   projected_offsets,
                                   vl);
      const vuint32m2_t target_index = __riscv_vluxei32_v_u32m2(
          reinterpret_cast<const std::uint32_t*>(
              bytes + offsetof(ProjectedOrganizedProjectionCandidate, target_index)),
          projected_offsets,
          vl);
      const vfloat32m2_t sx = __riscv_vluxei32_v_f32m2(
          reinterpret_cast<const float*>(
              bytes + offsetof(ProjectedOrganizedProjectionCandidate, x)),
          projected_offsets,
          vl);
      const vfloat32m2_t sy = __riscv_vluxei32_v_f32m2(
          reinterpret_cast<const float*>(
              bytes + offsetof(ProjectedOrganizedProjectionCandidate, y)),
          projected_offsets,
          vl);
      const vfloat32m2_t sz = __riscv_vluxei32_v_f32m2(
          reinterpret_cast<const float*>(
              bytes + offsetof(ProjectedOrganizedProjectionCandidate, z)),
          projected_offsets,
          vl);

      const vuint32m2_t target_offsets = __riscv_vmul_vx_u32m2(
          target_index, static_cast<std::uint32_t>(sizeof(PointTarget)), vl);

      vfloat32m2_t tx;
      vfloat32m2_t ty;
      vfloat32m2_t tz;
      pcl::rvv_load::indexed_load3_fields_f32m2<
          typename pcl::traits::POD<PointTarget>::type, kXOff, kYOff, kZOff>(
          target_base, target_offsets, vl, tx, ty, tz);

      // The RVV stage covers target gather, target finite, depth and the final
      // distance predicate, then compresses survivors in scan order. Distance is
      // still recomputed with Eigen before writing pcl::Correspondence, keeping
      // the stored value tied to the production scalar expression.
      vbool16_t keep = __riscv_vmand_mm_b16(
          __riscv_vmand_mm_b16(organizedProjectionFiniteMask(tx, vl),
                               organizedProjectionFiniteMask(ty, vl),
                               vl),
          organizedProjectionFiniteMask(tz, vl),
          vl);
      const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(sz, tz, vl);
      keep = __riscv_vmand_mm_b16(
          keep,
          __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(dz, vl),
                                      depth_threshold,
                                      vl),
          vl);
      const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(sx, tx, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(sy, ty, vl);
      vfloat32m2_t dist2 = __riscv_vfmul_vv_f32m2(dx, dx, vl);
      dist2 = __riscv_vfmacc_vv_f32m2(dist2, dy, dy, vl);
      dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
      const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
      const vbool16_t distance_keep =
          organizedProjectionDistanceThresholdMask(dist, max_distance, vl);
      keep = __riscv_vmand_mm_b16(keep, distance_keep, vl);

      const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(source, keep, vl);
      const vuint32m2_t target_kept =
          __riscv_vcompress_vm_u32m2(target_index, keep, vl);
      const vfloat32m2_t sx_kept = __riscv_vcompress_vm_f32m2(sx, keep, vl);
      const vfloat32m2_t sy_kept = __riscv_vcompress_vm_f32m2(sy, keep, vl);
      const vfloat32m2_t sz_kept = __riscv_vcompress_vm_f32m2(sz, keep, vl);
      const vfloat32m2_t tx_kept = __riscv_vcompress_vm_f32m2(tx, keep, vl);
      const vfloat32m2_t ty_kept = __riscv_vcompress_vm_f32m2(ty, keep, vl);
      const vfloat32m2_t tz_kept = __riscv_vcompress_vm_f32m2(tz, keep, vl);
      const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

      __riscv_vse32_v_u32m2(source_buf, source_kept, keep_count);
      __riscv_vse32_v_u32m2(target_buf, target_kept, keep_count);
      __riscv_vse32_v_f32m2(sx_buf, sx_kept, keep_count);
      __riscv_vse32_v_f32m2(sy_buf, sy_kept, keep_count);
      __riscv_vse32_v_f32m2(sz_buf, sz_kept, keep_count);
      __riscv_vse32_v_f32m2(tx_buf, tx_kept, keep_count);
      __riscv_vse32_v_f32m2(ty_buf, ty_kept, keep_count);
      __riscv_vse32_v_f32m2(tz_buf, tz_kept, keep_count);

      for (std::size_t lane = 0; lane < keep_count; ++lane) {
        const Eigen::Vector3f p_src3(sx_buf[lane], sy_buf[lane], sz_buf[lane]);
        const Eigen::Vector3f p_tgt(tx_buf[lane], ty_buf[lane], tz_buf[lane]);
        const double scalar_dist = (p_src3 - p_tgt).norm();
        if (scalar_dist < max_distance)
          accepted[kept++] = AcceptedOrganizedProjectionCandidate{
              source_buf[lane],
              target_buf[lane],
              static_cast<float>(scalar_dist)};
      }

      i += vl;
    }

    accepted.resize(kept);
    return true;
  }
}

#endif // __RVV10__

template <typename PointTarget>
inline void
finishOrganizedProjectionCorrespondences(
    const pcl::PointCloud<PointTarget>& target,
    const Eigen::Matrix3f& projection_matrix,
    const float depth_threshold,
    const double max_distance,
    const std::vector<OrganizedProjectionCandidate>& candidates,
    pcl::Correspondences& correspondences)
{
  correspondences.resize(candidates.size());
  std::size_t c_index = 0;

  for (const auto& candidate : candidates) {
    const Eigen::Vector3f p_src3(candidate.x, candidate.y, candidate.z);
    const Eigen::Vector3f uv(projection_matrix * p_src3);

    /// Check if the point was behind the camera
    if (uv[2] <= 0)
      continue;

    const int u = static_cast<int>(uv[0] / uv[2]);
    const int v = static_cast<int>(uv[1] / uv[2]);

    if (u >= 0 && u < static_cast<int>(target.width) && v >= 0 &&
        v < static_cast<int>(target.height)) {
      const PointTarget& pt_tgt = target.at(u, v);
      if (!isFinite(pt_tgt))
        continue;
      /// Check if the depth difference is larger than the threshold
      if (std::abs(uv[2] - pt_tgt.z) > depth_threshold)
        continue;

      const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
      if (dist < max_distance)
        correspondences[c_index++] = pcl::Correspondence(
            static_cast<int>(candidate.source_index),
            v * target.width + u,
            static_cast<float>(dist));
    }
  }

  correspondences.resize(c_index);
}

template <typename PointTarget>
inline void
finishOrganizedProjectionCorrespondencesFromProjected(
    const pcl::PointCloud<PointTarget>& target,
    const float depth_threshold,
    const double max_distance,
    const std::vector<ProjectedOrganizedProjectionCandidate>& projected,
    pcl::Correspondences& correspondences)
{
  correspondences.resize(projected.size());
  std::size_t c_index = 0;

  for (const auto& candidate : projected) {
    const Eigen::Vector3f p_src3(candidate.x, candidate.y, candidate.z);
    const PointTarget& pt_tgt = target.points[candidate.target_index];
    if (!isFinite(pt_tgt))
      continue;
    /// Check if the depth difference is larger than the threshold
    if (std::abs(candidate.z - pt_tgt.z) > depth_threshold)
      continue;

    const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
    if (dist < max_distance)
      correspondences[c_index++] = pcl::Correspondence(
          static_cast<int>(candidate.source_index),
          static_cast<int>(candidate.target_index),
          static_cast<float>(dist));
  }

  correspondences.resize(c_index);
}

inline void
finishOrganizedProjectionCorrespondencesFromAccepted(
    const std::vector<AcceptedOrganizedProjectionCandidate>& accepted,
    pcl::Correspondences& correspondences)
{
  correspondences.resize(accepted.size());
  for (std::size_t i = 0; i < accepted.size(); ++i) {
    correspondences[i] =
        pcl::Correspondence(static_cast<int>(accepted[i].source_index),
                            static_cast<int>(accepted[i].target_index),
                            accepted[i].distance);
  }
}

template <typename PointSource, typename PointTarget>
inline void
determineCorrespondencesOrganizedProjectionStd(
    const pcl::PointCloud<PointSource>& input,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& indices,
    const Eigen::Matrix4f& src_to_tgt_transformation,
    const Eigen::Matrix3f& projection_matrix,
    const float depth_threshold,
    const double max_distance,
    pcl::Correspondences& correspondences)
{
  correspondences.resize(indices.size());
  std::size_t c_index = 0;

  for (const auto& src_idx : indices) {
    if (isFinite(input[src_idx])) {
      const Eigen::Vector4f p_src(src_to_tgt_transformation *
                                  input[src_idx].getVector4fMap());
      const Eigen::Vector3f p_src3(p_src[0], p_src[1], p_src[2]);
      const Eigen::Vector3f uv(projection_matrix * p_src3);

      /// Check if the point was behind the camera
      if (uv[2] <= 0)
        continue;

      const int u = static_cast<int>(uv[0] / uv[2]);
      const int v = static_cast<int>(uv[1] / uv[2]);

      if (u >= 0 && u < static_cast<int>(target.width) && v >= 0 &&
          v < static_cast<int>(target.height)) {
        const PointTarget& pt_tgt = target.at(u, v);
        if (!isFinite(pt_tgt))
          continue;
        /// Check if the depth difference is larger than the threshold
        if (std::abs(uv[2] - pt_tgt.z) > depth_threshold)
          continue;

        const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
        if (dist < max_distance)
          correspondences[c_index++] = pcl::Correspondence(
              src_idx, v * target.width + u, static_cast<float>(dist));
      }
    }
  }

  correspondences.resize(c_index);
}

} // namespace detail

template <typename PointSource, typename PointTarget, typename Scalar>
bool
CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>::
    initCompute()
{
  // Set the target_cloud_updated_ variable to true, so that the kd-tree is not built -
  // it is not needed for this class
  target_cloud_updated_ = false;
  if (!CorrespondenceEstimationBase<PointSource, PointTarget, Scalar>::initCompute())
    return (false);

  /// Check if the target cloud is organized
  if (!target_->isOrganized()) {
    PCL_WARN("[pcl::registration::%s::initCompute] Target cloud is not organized.\n",
             getClassName().c_str());
    return (false);
  }

  /// Put the projection matrix together
  projection_matrix_(0, 0) = fx_;
  projection_matrix_(1, 1) = fy_;
  projection_matrix_(0, 2) = cx_;
  projection_matrix_(1, 2) = cy_;

  return (true);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>::
    determineCorrespondences(pcl::Correspondences& correspondences,
                             const double max_distance)
{
  if (!initCompute())
    return;

#if defined(__RVV10__)
  if constexpr (std::is_same_v<Scalar, float>) {
    std::vector<detail::OrganizedProjectionCandidate> candidates;
    if (detail::projectOrganizedProjectionCandidatesRVV(*input_,
                                                        *target_,
                                                        *indices_,
                                                        src_to_tgt_transformation_,
                                                        candidates)) {
      std::vector<detail::ProjectedOrganizedProjectionCandidate> projected;
      if (detail::projectOrganizedProjectionPixelsRVV(
              *target_, projection_matrix_, candidates, projected)) {
        std::vector<detail::AcceptedOrganizedProjectionCandidate> accepted;
        if (detail::acceptProjectedOrganizedProjectionCandidatesRVV(
                *target_,
                depth_threshold_,
                max_distance,
                projected,
                accepted)) {
          detail::finishOrganizedProjectionCorrespondencesFromAccepted(
              accepted, correspondences);
          return;
        }
        detail::finishOrganizedProjectionCorrespondencesFromProjected(
            *target_,
            depth_threshold_,
            max_distance,
            projected,
            correspondences);
        return;
      }
      detail::finishOrganizedProjectionCorrespondences(
          *target_,
          projection_matrix_,
          depth_threshold_,
          max_distance,
          candidates,
          correspondences);
      return;
    }
  }
#endif

  detail::determineCorrespondencesOrganizedProjectionStd(*input_,
                                                         *target_,
                                                         *indices_,
                                                         src_to_tgt_transformation_,
                                                         projection_matrix_,
                                                         depth_threshold_,
                                                         max_distance,
                                                         correspondences);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>::
    determineReciprocalCorrespondences(pcl::Correspondences& correspondences,
                                       const double max_distance)
{
  // Call the normal determineCorrespondences (...), as doing it both ways will not
  // improve the results
  determineCorrespondences(correspondences, max_distance);
}

} // namespace registration
} // namespace pcl

#endif // PCL_REGISTRATION_CORRESPONDENCE_ESTIMATION_ORGANIZED_PROJECTION_IMPL_HPP_
