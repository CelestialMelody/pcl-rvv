#pragma once

/*
 * 本文件做什么：
 * 这里保存 PFH（Point Feature Histogram，点特征直方图）Phase 010 的 test-only
 * RVV candidate（测试专用 RVV 候选）。它先把邻域中的 all-pairs（所有点对）
 * 整理成连续 SoA staging（结构数组到分字段暂存），再用 RVV lane-level helper
 * （单段向量 helper）计算 pair tuple，最后保持 production 的标量 histogram scatter
 * （直方图离散累加）顺序。
 *
 * 证据边界：
 * 这是 production-shaped diagnostic（生产形态诊断），不是 production dispatch
 * （生产分流）。Staging 成本计入 bench；normal 字段 traits gate、direct AoS load
 * 和真实 production fallback 都要等后续 PI1 再审计。
 */

#include "impl/pfh_reference.hpp"

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>

#include <riscv_vector.h>
#endif

#include <cstdint>
#include <vector>

namespace pcl::features::rvv_test::pfh
{

struct PairBatchStaging
{
  std::vector<float> p1x;
  std::vector<float> p1y;
  std::vector<float> p1z;
  std::vector<float> p2x;
  std::vector<float> p2y;
  std::vector<float> p2z;
  std::vector<float> n1x;
  std::vector<float> n1y;
  std::vector<float> n1z;
  std::vector<float> n2x;
  std::vector<float> n2y;
  std::vector<float> n2z;
};

struct PairOffsetStaging
{
  std::vector<std::uint32_t> p1_offsets;
  std::vector<std::uint32_t> p2_offsets;
};

inline void
reservePairBatch(PairBatchStaging& staging, const std::size_t size)
{
  staging.p1x.reserve(size);
  staging.p1y.reserve(size);
  staging.p1z.reserve(size);
  staging.p2x.reserve(size);
  staging.p2y.reserve(size);
  staging.p2z.reserve(size);
  staging.n1x.reserve(size);
  staging.n1y.reserve(size);
  staging.n1z.reserve(size);
  staging.n2x.reserve(size);
  staging.n2y.reserve(size);
  staging.n2z.reserve(size);
}

inline void
appendPair(PairBatchStaging& staging, const PointT& p1, const PointT& p2)
{
  staging.p1x.push_back(p1.x);
  staging.p1y.push_back(p1.y);
  staging.p1z.push_back(p1.z);
  staging.p2x.push_back(p2.x);
  staging.p2y.push_back(p2.y);
  staging.p2z.push_back(p2.z);
  staging.n1x.push_back(p1.normal_x);
  staging.n1y.push_back(p1.normal_y);
  staging.n1z.push_back(p1.normal_z);
  staging.n2x.push_back(p2.normal_x);
  staging.n2y.push_back(p2.normal_y);
  staging.n2z.push_back(p2.normal_z);
}

inline PairBatchStaging
makePairBatchStaging(const CloudT& cloud, const pcl::Indices& indices)
{
  PairBatchStaging staging;
  reservePairBatch(staging, indices.size() * (indices.size() - 1) / 2);
  for (std::size_t i_idx = 0; i_idx < indices.size(); ++i_idx)
  {
    for (std::size_t j_idx = 0; j_idx < i_idx; ++j_idx)
    {
      const auto i = static_cast<std::size_t>(indices[i_idx]);
      const auto j = static_cast<std::size_t>(indices[j_idx]);
      if (!isFinitePoint(cloud[i]) || !isFinitePoint(cloud[j]))
        continue;
      appendPair(staging, cloud[i], cloud[j]);
    }
  }
  return staging;
}

inline PairOffsetStaging
makePairOffsetStaging(const CloudT& cloud, const pcl::Indices& indices)
{
  PairOffsetStaging staging;
  staging.p1_offsets.reserve(indices.size() * (indices.size() - 1) / 2);
  staging.p2_offsets.reserve(indices.size() * (indices.size() - 1) / 2);
  for (std::size_t i_idx = 0; i_idx < indices.size(); ++i_idx)
  {
    for (std::size_t j_idx = 0; j_idx < i_idx; ++j_idx)
    {
      if (indices[i_idx] < 0 || indices[j_idx] < 0)
        continue;
      const auto i = static_cast<std::size_t>(indices[i_idx]);
      const auto j = static_cast<std::size_t>(indices[j_idx]);
      if (!isFinitePoint(cloud[i]) || !isFinitePoint(cloud[j]))
        continue;
      staging.p1_offsets.push_back(static_cast<std::uint32_t>(i * sizeof(PointT)));
      staging.p2_offsets.push_back(static_cast<std::uint32_t>(j * sizeof(PointT)));
    }
  }
  return staging;
}

#if defined(__RVV10__)
inline void
computePairTuplesRVV(const PairBatchStaging& staging,
                     std::vector<float>& f1,
                     std::vector<float>& f2,
                     std::vector<float>& f3,
                     std::vector<std::int32_t>& valid)
{
  const std::size_t pair_count = staging.p1x.size();
  f1.resize(pair_count);
  f2.resize(pair_count);
  f3.resize(pair_count);
  valid.resize(pair_count);

  for (std::size_t offset = 0; offset < pair_count;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(pair_count - offset);
    const vfloat32m2_t p1x = __riscv_vle32_v_f32m2(staging.p1x.data() + offset, vl);
    const vfloat32m2_t p1y = __riscv_vle32_v_f32m2(staging.p1y.data() + offset, vl);
    const vfloat32m2_t p1z = __riscv_vle32_v_f32m2(staging.p1z.data() + offset, vl);
    const vfloat32m2_t p2x = __riscv_vle32_v_f32m2(staging.p2x.data() + offset, vl);
    const vfloat32m2_t p2y = __riscv_vle32_v_f32m2(staging.p2y.data() + offset, vl);
    const vfloat32m2_t p2z = __riscv_vle32_v_f32m2(staging.p2z.data() + offset, vl);
    const vfloat32m2_t n1x = __riscv_vle32_v_f32m2(staging.n1x.data() + offset, vl);
    const vfloat32m2_t n1y = __riscv_vle32_v_f32m2(staging.n1y.data() + offset, vl);
    const vfloat32m2_t n1z = __riscv_vle32_v_f32m2(staging.n1z.data() + offset, vl);
    const vfloat32m2_t n2x = __riscv_vle32_v_f32m2(staging.n2x.data() + offset, vl);
    const vfloat32m2_t n2y = __riscv_vle32_v_f32m2(staging.n2y.data() + offset, vl);
    const vfloat32m2_t n2z = __riscv_vle32_v_f32m2(staging.n2z.data() + offset, vl);

    vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(p2x, p1x, vl);
    vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(p2y, p1y, vl);
    vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(p2z, p1z, vl);
    vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
    vbool16_t lane_valid = __riscv_vmfne_vf_f32m2_b16(dist, 0.0f, vl);

    const vfloat32m2_t n1_dot_delta = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, dx, vl), n1y, dy, vl), n1z, dz, vl);
    const vfloat32m2_t n2_dot_delta = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n2x, dx, vl), n2y, dy, vl), n2z, dz, vl);
    const vfloat32m2_t angle1 = __riscv_vfdiv_vv_f32m2(n1_dot_delta, dist, vl);
    const vfloat32m2_t angle2 = __riscv_vfdiv_vv_f32m2(n2_dot_delta, dist, vl);
    const vfloat32m2_t abs_angle1 = __riscv_vfsgnjx_vv_f32m2(angle1, angle1, vl);
    const vfloat32m2_t abs_angle2 = __riscv_vfsgnjx_vv_f32m2(angle2, angle2, vl);
    const vbool16_t swap = __riscv_vmflt_vv_f32m2_b16(abs_angle1, abs_angle2, vl);

    const vfloat32m2_t neg_dx = __riscv_vfneg_v_f32m2(dx, vl);
    const vfloat32m2_t neg_dy = __riscv_vfneg_v_f32m2(dy, vl);
    const vfloat32m2_t neg_dz = __riscv_vfneg_v_f32m2(dz, vl);
    dx = __riscv_vmerge_vvm_f32m2(dx, neg_dx, swap, vl);
    dy = __riscv_vmerge_vvm_f32m2(dy, neg_dy, swap, vl);
    dz = __riscv_vmerge_vvm_f32m2(dz, neg_dz, swap, vl);

    const vfloat32m2_t ux = __riscv_vmerge_vvm_f32m2(n1x, n2x, swap, vl);
    const vfloat32m2_t uy = __riscv_vmerge_vvm_f32m2(n1y, n2y, swap, vl);
    const vfloat32m2_t uz = __riscv_vmerge_vvm_f32m2(n1z, n2z, swap, vl);
    const vfloat32m2_t target_nx = __riscv_vmerge_vvm_f32m2(n2x, n1x, swap, vl);
    const vfloat32m2_t target_ny = __riscv_vmerge_vvm_f32m2(n2y, n1y, swap, vl);
    const vfloat32m2_t target_nz = __riscv_vmerge_vvm_f32m2(n2z, n1z, swap, vl);
    const vfloat32m2_t neg_angle2 = __riscv_vfneg_v_f32m2(angle2, vl);
    const vfloat32m2_t vf3 = __riscv_vmerge_vvm_f32m2(angle1, neg_angle2, swap, vl);

    vfloat32m2_t vx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dy, uz, vl),
                                             __riscv_vfmul_vv_f32m2(dz, uy, vl),
                                             vl);
    vfloat32m2_t vy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dz, ux, vl),
                                             __riscv_vfmul_vv_f32m2(dx, uz, vl),
                                             vl);
    vfloat32m2_t vz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, uy, vl),
                                             __riscv_vfmul_vv_f32m2(dy, ux, vl),
                                             vl);
    vfloat32m2_t vnorm2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, vx, vl), vy, vy, vl);
    vnorm2 = __riscv_vfmacc_vv_f32m2(vnorm2, vz, vz, vl);
    const vfloat32m2_t vnorm = __riscv_vfsqrt_v_f32m2(vnorm2, vl);
    lane_valid = __riscv_vmand_mm_b16(lane_valid, __riscv_vmfne_vf_f32m2_b16(vnorm, 0.0f, vl), vl);
    vx = __riscv_vfdiv_vv_f32m2(vx, vnorm, vl);
    vy = __riscv_vfdiv_vv_f32m2(vy, vnorm, vl);
    vz = __riscv_vfdiv_vv_f32m2(vz, vnorm, vl);

    const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(uy, vz, vl),
                                                   __riscv_vfmul_vv_f32m2(uz, vy, vl),
                                                   vl);
    const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(uz, vx, vl),
                                                   __riscv_vfmul_vv_f32m2(ux, vz, vl),
                                                   vl);
    const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(ux, vy, vl),
                                                   __riscv_vfmul_vv_f32m2(uy, vx, vl),
                                                   vl);
    const vfloat32m2_t vf2 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, target_nx, vl), vy, target_ny, vl),
        vz,
        target_nz,
        vl);
    const vfloat32m2_t atan_y = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(wx, target_nx, vl), wy, target_ny, vl),
        wz,
        target_nz,
        vl);
    const vfloat32m2_t atan_x = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(ux, target_nx, vl), uy, target_ny, vl),
        uz,
        target_nz,
        vl);
    const vfloat32m2_t vf1 = pcl::atan2_RVV_f32m2(atan_y, atan_x, vl);

    __riscv_vse32_v_f32m2(f1.data() + offset, vf1, vl);
    __riscv_vse32_v_f32m2(f2.data() + offset, vf2, vl);
    __riscv_vse32_v_f32m2(f3.data() + offset, vf3, vl);
    const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2(zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2(valid.data() + offset, vvalid, vl);
    offset += vl;
  }
}

inline void
computePairTuplesDirectAoSRVV(const CloudT& cloud,
                              const PairOffsetStaging& staging,
                              std::vector<float>& f1,
                              std::vector<float>& f2,
                              std::vector<float>& f3,
                              std::vector<std::int32_t>& valid)
{
  static_assert(pcl::rvv::RVVXYZNormalFloatLayout<PointT>::value,
                "PFH direct-AoS diagnostic needs single-float xyz and normal fields.");

  using Layout = pcl::rvv::RVVXYZNormalFloatLayout<PointT>;
  const std::size_t pair_count = staging.p1_offsets.size();
  f1.resize(pair_count);
  f2.resize(pair_count);
  f3.resize(pair_count);
  valid.resize(pair_count);

  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  for (std::size_t offset = 0; offset < pair_count;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(pair_count - offset);
    const vuint32m2_t p1_offsets =
        __riscv_vle32_v_u32m2(staging.p1_offsets.data() + offset, vl);
    const vuint32m2_t p2_offsets =
        __riscv_vle32_v_u32m2(staging.p2_offsets.data() + offset, vl);

    vfloat32m2_t p1x, p1y, p1z, p2x, p2y, p2z;
    vfloat32m2_t n1x, n1y, n1z, n2x, n2y, n2z;
    pcl::rvv_load::indexed_load3_fields_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ>(
        base, p1_offsets, vl, p1x, p1y, p1z);
    pcl::rvv_load::indexed_load3_fields_f32m2<PointT, Layout::kX, Layout::kY, Layout::kZ>(
        base, p2_offsets, vl, p2x, p2y, p2z);
    pcl::rvv_load::indexed_load3_fields_f32m2<PointT, Layout::kNX, Layout::kNY, Layout::kNZ>(
        base, p1_offsets, vl, n1x, n1y, n1z);
    pcl::rvv_load::indexed_load3_fields_f32m2<PointT, Layout::kNX, Layout::kNY, Layout::kNZ>(
        base, p2_offsets, vl, n2x, n2y, n2z);

    vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(p2x, p1x, vl);
    vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(p2y, p1y, vl);
    vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(p2z, p1z, vl);
    vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
    vbool16_t lane_valid = __riscv_vmfne_vf_f32m2_b16(dist, 0.0f, vl);

    const vfloat32m2_t n1_dot_delta = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, dx, vl), n1y, dy, vl), n1z, dz, vl);
    const vfloat32m2_t n2_dot_delta = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n2x, dx, vl), n2y, dy, vl), n2z, dz, vl);
    const vfloat32m2_t angle1 = __riscv_vfdiv_vv_f32m2(n1_dot_delta, dist, vl);
    const vfloat32m2_t angle2 = __riscv_vfdiv_vv_f32m2(n2_dot_delta, dist, vl);
    const vfloat32m2_t abs_angle1 = __riscv_vfsgnjx_vv_f32m2(angle1, angle1, vl);
    const vfloat32m2_t abs_angle2 = __riscv_vfsgnjx_vv_f32m2(angle2, angle2, vl);
    const vbool16_t swap = __riscv_vmflt_vv_f32m2_b16(abs_angle1, abs_angle2, vl);

    const vfloat32m2_t neg_dx = __riscv_vfneg_v_f32m2(dx, vl);
    const vfloat32m2_t neg_dy = __riscv_vfneg_v_f32m2(dy, vl);
    const vfloat32m2_t neg_dz = __riscv_vfneg_v_f32m2(dz, vl);
    dx = __riscv_vmerge_vvm_f32m2(dx, neg_dx, swap, vl);
    dy = __riscv_vmerge_vvm_f32m2(dy, neg_dy, swap, vl);
    dz = __riscv_vmerge_vvm_f32m2(dz, neg_dz, swap, vl);

    const vfloat32m2_t ux = __riscv_vmerge_vvm_f32m2(n1x, n2x, swap, vl);
    const vfloat32m2_t uy = __riscv_vmerge_vvm_f32m2(n1y, n2y, swap, vl);
    const vfloat32m2_t uz = __riscv_vmerge_vvm_f32m2(n1z, n2z, swap, vl);
    const vfloat32m2_t target_nx = __riscv_vmerge_vvm_f32m2(n2x, n1x, swap, vl);
    const vfloat32m2_t target_ny = __riscv_vmerge_vvm_f32m2(n2y, n1y, swap, vl);
    const vfloat32m2_t target_nz = __riscv_vmerge_vvm_f32m2(n2z, n1z, swap, vl);
    const vfloat32m2_t neg_angle2 = __riscv_vfneg_v_f32m2(angle2, vl);
    const vfloat32m2_t vf3 = __riscv_vmerge_vvm_f32m2(angle1, neg_angle2, swap, vl);

    vfloat32m2_t vx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dy, uz, vl),
                                             __riscv_vfmul_vv_f32m2(dz, uy, vl),
                                             vl);
    vfloat32m2_t vy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dz, ux, vl),
                                             __riscv_vfmul_vv_f32m2(dx, uz, vl),
                                             vl);
    vfloat32m2_t vz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, uy, vl),
                                             __riscv_vfmul_vv_f32m2(dy, ux, vl),
                                             vl);
    vfloat32m2_t vnorm2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, vx, vl), vy, vy, vl);
    vnorm2 = __riscv_vfmacc_vv_f32m2(vnorm2, vz, vz, vl);
    const vfloat32m2_t vnorm = __riscv_vfsqrt_v_f32m2(vnorm2, vl);
    lane_valid = __riscv_vmand_mm_b16(lane_valid, __riscv_vmfne_vf_f32m2_b16(vnorm, 0.0f, vl), vl);
    vx = __riscv_vfdiv_vv_f32m2(vx, vnorm, vl);
    vy = __riscv_vfdiv_vv_f32m2(vy, vnorm, vl);
    vz = __riscv_vfdiv_vv_f32m2(vz, vnorm, vl);

    const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(uy, vz, vl),
                                                   __riscv_vfmul_vv_f32m2(uz, vy, vl),
                                                   vl);
    const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(uz, vx, vl),
                                                   __riscv_vfmul_vv_f32m2(ux, vz, vl),
                                                   vl);
    const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(ux, vy, vl),
                                                   __riscv_vfmul_vv_f32m2(uy, vx, vl),
                                                   vl);
    const vfloat32m2_t vf2 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, target_nx, vl), vy, target_ny, vl),
        vz,
        target_nz,
        vl);
    const vfloat32m2_t atan_y = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(wx, target_nx, vl), wy, target_ny, vl),
        wz,
        target_nz,
        vl);
    const vfloat32m2_t atan_x = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(ux, target_nx, vl), uy, target_ny, vl),
        uz,
        target_nz,
        vl);
    const vfloat32m2_t vf1 = pcl::atan2_RVV_f32m2(atan_y, atan_x, vl);

    __riscv_vse32_v_f32m2(f1.data() + offset, vf1, vl);
    __riscv_vse32_v_f32m2(f2.data() + offset, vf2, vl);
    __riscv_vse32_v_f32m2(f3.data() + offset, vf3, vl);
    const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2(zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2(valid.data() + offset, vvalid, vl);
    offset += vl;
  }
}
#endif

inline void
computePointPFHSignaturePairBatchRVV(const CloudT& cloud,
                                     const pcl::Indices& indices,
                                     const int nr_split,
                                     Eigen::VectorXf& pfh_histogram)
{
#if defined(__RVV10__)
  pfh_histogram.setZero(nr_split * nr_split * nr_split);
  const float hist_incr =
      100.0f / static_cast<float>(indices.size() * (indices.size() - 1) / 2);
  const PairBatchStaging staging = makePairBatchStaging(cloud, indices);

  std::vector<float> f1;
  std::vector<float> f2;
  std::vector<float> f3;
  std::vector<std::int32_t> valid;
  computePairTuplesRVV(staging, f1, f2, f3, valid);

  for (std::size_t i = 0; i < valid.size(); ++i)
  {
    if (valid[i] == 0)
      continue;
    const int b1 = binAngularFeature(f1[i], nr_split);
    const int b2 = binUnitFeature(f2[i], nr_split);
    const int b3 = binUnitFeature(f3[i], nr_split);
    pfh_histogram[histogramIndex(b1, b2, b3, nr_split)] += hist_incr;
  }
#else
  computePointPFHReference(cloud, indices, nr_split, pfh_histogram);
#endif
}

inline void
computePointPFHSignatureDirectAoSRVV(const CloudT& cloud,
                                     const pcl::Indices& indices,
                                     const int nr_split,
                                     Eigen::VectorXf& pfh_histogram)
{
#if defined(__RVV10__)
  pfh_histogram.setZero(nr_split * nr_split * nr_split);
  const float hist_incr =
      100.0f / static_cast<float>(indices.size() * (indices.size() - 1) / 2);
  const PairOffsetStaging staging = makePairOffsetStaging(cloud, indices);

  std::vector<float> f1;
  std::vector<float> f2;
  std::vector<float> f3;
  std::vector<std::int32_t> valid;
  computePairTuplesDirectAoSRVV(cloud, staging, f1, f2, f3, valid);

  for (std::size_t i = 0; i < valid.size(); ++i)
  {
    if (valid[i] == 0)
      continue;
    const int b1 = binAngularFeature(f1[i], nr_split);
    const int b2 = binUnitFeature(f2[i], nr_split);
    const int b3 = binUnitFeature(f3[i], nr_split);
    pfh_histogram[histogramIndex(b1, b2, b3, nr_split)] += hist_incr;
  }
#else
  computePointPFHReference(cloud, indices, nr_split, pfh_histogram);
#endif
}

} // namespace pcl::features::rvv_test::pfh
