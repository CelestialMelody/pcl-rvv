#pragma once

/*
 * 本文件做什么：
 * 这里保存 VFH topic 的 test-only scalar reference（测试专用标量参考链路）
 * 和第一阶段 RVV candidate（候选实现）入口。reference 复刻
 * `features/include/pcl/features/impl/vfh.hpp` 中 centroid（质心）、
 * normal centroid（法线质心）、centroid-to-point SPFH（从质心到点的简化点特征直方图）
 * 和 viewpoint histogram（视点直方图）语义，用来给候选 helper 做 same-chain
 * （同构链路）对拍。
 *
 * 证据边界：
 * 这些 helper 只证明当前 `PointNormal -> VFHSignature308`、dense finite
 * synthetic cloud（有限合成点云）和默认 VFH 参数下的诊断语义。它们不修改
 * production dispatch（生产分流），也不能把板卡诊断收益直接外推为 production evidence
 * （生产证据）。
 */

#include <pcl/features/pfh_tools.h>
#include <pcl/features/vfh.h>
#include <pcl/common/centroid.h>
#include <pcl/common/common.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>

#include <riscv_vector.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace pcl::features::rvv_test::vfh
{
using PointT = pcl::PointNormal;
using CloudT = pcl::PointCloud<PointT>;
using Signature = std::array<float, 308>;

struct VFHOptions
{
  bool normalize_bins = true;
  bool normalize_distances = false;
  bool size_component = false;
  Eigen::Vector4f viewpoint = Eigen::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
};

inline CloudT::Ptr
makeFeatureCloud(const int side)
{
  auto cloud = CloudT::Ptr(new CloudT);
  cloud->reserve(static_cast<std::size_t>(side * side));

  for (int y = 0; y < side; ++y)
  {
    for (int x = 0; x < side; ++x)
    {
      const float fx = static_cast<float>(x) * 0.047f;
      const float fy = static_cast<float>(y) * 0.039f;
      const float wave = 0.018f * std::sin(0.41f * fx) + 0.013f * std::cos(0.29f * fy);

      PointT p;
      p.x = fx + 0.021f * std::sin(0.17f * fy);
      p.y = fy + 0.019f * std::cos(0.11f * fx);
      p.z = 0.18f * fx - 0.13f * fy + wave;

      Eigen::Vector3f normal(-0.18f + 0.0074f * std::cos(0.41f * fx),
                             0.13f + 0.0038f * std::sin(0.29f * fy),
                             1.0f);
      normal.normalize();
      p.normal_x = normal.x();
      p.normal_y = normal.y();
      p.normal_z = normal.z();
      p.curvature = 0.0f;
      cloud->push_back(p);
    }
  }

  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline pcl::Indices
makeSequentialIndices(const std::size_t size)
{
  pcl::Indices indices(size);
  std::iota(indices.begin(), indices.end(), 0);
  return indices;
}

inline Eigen::Vector4f
computeCentroidReference(const CloudT& cloud, const pcl::Indices& indices)
{
  Eigen::Vector4f centroid = Eigen::Vector4f::Zero();
  pcl::compute3DCentroid(cloud, indices, centroid);
  return centroid;
}

inline Eigen::Vector4f
computeNormalCentroidReference(const CloudT& cloud, const pcl::Indices& indices)
{
  Eigen::Vector4f normal = Eigen::Vector4f::Zero();
  for (const auto index : indices)
    normal += cloud[static_cast<std::size_t>(index)].getNormalVector4fMap();
  normal /= static_cast<float>(indices.size());
  normal[3] = 0.0f;
  return normal;
}

inline int
binAngularFeature(const float value, const int bins)
{
  constexpr float kPi = 3.14159265358979323846f;
  const float scale = 1.0f / (2.0f * kPi);
  return std::clamp(static_cast<int>(std::floor(static_cast<float>(bins) * ((value + kPi) * scale))),
                    0,
                    bins - 1);
}

inline int
binDistanceFeature(const float value, const int bins)
{
  return std::clamp(static_cast<int>(std::floor(static_cast<float>(bins) * value)), 0, bins - 1);
}

inline float
maxDistanceToCentroid(const CloudT& cloud, const pcl::Indices& indices, const Eigen::Vector4f& centroid)
{
  Eigen::Vector4f max_pt;
  pcl::getMaxDistance(cloud, indices, centroid, max_pt);
  max_pt[3] = 0.0f;
  return (centroid - max_pt).norm();
}

inline void
accumulateSPFHReference(const Eigen::Vector4f& centroid_p,
                        const Eigen::Vector4f& centroid_n,
                        const CloudT& cloud,
                        const pcl::Indices& indices,
                        const VFHOptions& options,
                        Signature& signature)
{
  const float hist_incr =
      options.normalize_bins ? 100.0f / static_cast<float>(indices.size() - 1) : 1.0f;
  const float size_incr = options.size_component ? hist_incr : 0.0f;
  const float distance_norm =
      options.normalize_distances ? maxDistanceToCentroid(cloud, indices, centroid_p) : 1.0f;

  for (const auto index : indices)
  {
    float f1 = 0.0f;
    float f2 = 0.0f;
    float f3 = 0.0f;
    float f4 = 0.0f;
    if (!pcl::computePairFeatures(centroid_p,
                                  centroid_n,
                                  cloud[static_cast<std::size_t>(index)].getVector4fMap(),
                                  cloud[static_cast<std::size_t>(index)].getNormalVector4fMap(),
                                  f1,
                                  f2,
                                  f3,
                                  f4))
      continue;

    signature[binAngularFeature(f1, 45)] += hist_incr;
    signature[45 + binAngularFeature(f2, 45)] += hist_incr;
    signature[90 + binAngularFeature(f3, 45)] += hist_incr;
    if (size_incr != 0.0f)
    {
      const float distance_value =
          options.normalize_distances && distance_norm != 0.0f ? f4 / distance_norm : f4 * 100.0f;
      signature[135 + binDistanceFeature(distance_value, 45)] += size_incr;
    }
  }
}

inline void
accumulateViewpointReference(const Eigen::Vector4f& centroid_p,
                             const CloudT& cloud,
                             const pcl::Indices& indices,
                             const VFHOptions& options,
                             Signature& signature)
{
  Eigen::Vector4f d_vp_p = options.viewpoint - centroid_p;
  d_vp_p.normalize();
  const float hist_incr =
      options.normalize_bins ? 100.0f / static_cast<float>(indices.size()) : 1.0f;

  for (const auto index : indices)
  {
    Eigen::Vector4f normal = cloud[static_cast<std::size_t>(index)].getNormalVector4fMap();
    normal[3] = 0.0f;
    const double alpha = (normal.dot(d_vp_p) + 1.0) * 0.5;
    const auto bin = std::clamp(static_cast<std::size_t>(std::floor(alpha * 128.0)), 0ul, 127ul);
    signature[180 + bin] += hist_incr;
  }
}

#if defined(__RVV10__)
inline bool
indicesAreSequentialFullCloud(const CloudT& cloud, const pcl::Indices& indices)
{
  if (indices.size() != cloud.size())
    return false;

  for (std::size_t i = 0; i < indices.size(); ++i)
  {
    if (indices[i] != static_cast<pcl::index_t>(i))
      return false;
  }

  return true;
}

inline Eigen::Vector4f
computeNormalCentroidRVV(const CloudT& cloud, const pcl::Indices& indices)
{
  constexpr std::size_t kPointStrideBytes = sizeof(PointT);
  constexpr std::size_t kNXOff = pcl::traits::offset<PointT, pcl::fields::normal_x>::value;
  constexpr std::size_t kNYOff = pcl::traits::offset<PointT, pcl::fields::normal_y>::value;
  constexpr std::size_t kNZOff = pcl::traits::offset<PointT, pcl::fields::normal_z>::value;

  static_assert(pcl::rvv::RVVXYZNormalFloatLayout<PointT>::value,
                "VFH Phase 030 normal centroid candidate expects a dense float PointNormal AoS layout.");

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  vfloat32m2_t acc_nx = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t acc_ny = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t acc_nz = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  const std::uint8_t* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());

  for (std::size_t offset = 0; offset < indices.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const std::uint8_t* batch_u8 = base_u8 + offset * kPointStrideBytes;

    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    rvv_load::strided_load3_f32m2<kPointStrideBytes, kNXOff, kNYOff, kNZOff>(
        batch_u8, vl, nx, ny, nz);

    acc_nx = __riscv_vfadd_vv_f32m2_tu(acc_nx, acc_nx, nx, vl);
    acc_ny = __riscv_vfadd_vv_f32m2_tu(acc_ny, acc_ny, ny, vl);
    acc_nz = __riscv_vfadd_vv_f32m2_tu(acc_nz, acc_nz, nz, vl);
    offset += vl;
  }

  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  const float inv_count = 1.0f / static_cast<float>(indices.size());
  Eigen::Vector4f normal = Eigen::Vector4f::Zero();
  normal[0] =
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredosum_vs_f32m2_f32m1(acc_nx, zero, vlmax)) *
      inv_count;
  normal[1] =
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredosum_vs_f32m2_f32m1(acc_ny, zero, vlmax)) *
      inv_count;
  normal[2] =
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredosum_vs_f32m2_f32m1(acc_nz, zero, vlmax)) *
      inv_count;
  return normal;
}

inline void
accumulateSPFHCentroidPairMathRVV(const Eigen::Vector4f& centroid_p,
                                  const Eigen::Vector4f& centroid_n,
                                  const CloudT& cloud,
                                  const pcl::Indices& indices,
                                  const VFHOptions& options,
                                  Signature& signature)
{
  constexpr std::size_t kPointStrideBytes = sizeof(PointT);
  constexpr std::size_t kXOff = pcl::traits::offset<PointT, pcl::fields::x>::value;
  constexpr std::size_t kYOff = pcl::traits::offset<PointT, pcl::fields::y>::value;
  constexpr std::size_t kZOff = pcl::traits::offset<PointT, pcl::fields::z>::value;
  constexpr std::size_t kNXOff = pcl::traits::offset<PointT, pcl::fields::normal_x>::value;
  constexpr std::size_t kNYOff = pcl::traits::offset<PointT, pcl::fields::normal_y>::value;
  constexpr std::size_t kNZOff = pcl::traits::offset<PointT, pcl::fields::normal_z>::value;

  static_assert(pcl::rvv::RVVXYZNormalFloatLayout<PointT>::value,
                "VFH Phase 000 candidate expects a dense float PointNormal AoS layout.");

  const float hist_incr =
      options.normalize_bins ? 100.0f / static_cast<float>(indices.size() - 1) : 1.0f;
  const float size_incr = options.size_component ? hist_incr : 0.0f;
  const float distance_norm =
      options.normalize_distances ? maxDistanceToCentroid(cloud, indices, centroid_p) : 1.0f;

  std::vector<float> f1(indices.size());
  std::vector<float> f2(indices.size());
  std::vector<float> f3(indices.size());
  std::vector<float> f4(indices.size());
  std::vector<int32_t> valid(indices.size());

  const std::uint8_t* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());

  for (std::size_t offset = 0; offset < indices.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const std::uint8_t* batch_u8 = base_u8 + offset * kPointStrideBytes;

    vfloat32m2_t px;
    vfloat32m2_t py;
    vfloat32m2_t pz;
    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    rvv_load::strided_load3_f32m2<kPointStrideBytes, kXOff, kYOff, kZOff>(
        batch_u8, vl, px, py, pz);
    rvv_load::strided_load3_f32m2<kPointStrideBytes, kNXOff, kNYOff, kNZOff>(
        batch_u8, vl, nx, ny, nz);

    const vfloat32m2_t centroid_x = __riscv_vfmv_v_f_f32m2(centroid_p[0], vl);
    const vfloat32m2_t centroid_y = __riscv_vfmv_v_f_f32m2(centroid_p[1], vl);
    const vfloat32m2_t centroid_z = __riscv_vfmv_v_f_f32m2(centroid_p[2], vl);
    const vfloat32m2_t centroid_nx = __riscv_vfmv_v_f_f32m2(centroid_n[0], vl);
    const vfloat32m2_t centroid_ny = __riscv_vfmv_v_f_f32m2(centroid_n[1], vl);
    const vfloat32m2_t centroid_nz = __riscv_vfmv_v_f_f32m2(centroid_n[2], vl);

    vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(px, centroid_x, vl);
    vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(py, centroid_y, vl);
    vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(pz, centroid_z, vl);

    vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
    const vbool16_t dist_valid = __riscv_vmfne_vf_f32m2_b16(dist, 0.0f, vl);
    const vfloat32m2_t dist_safe = __riscv_vmerge_vvm_f32m2(
        __riscv_vfmv_v_f_f32m2(1.0f, vl), dist, dist_valid, vl);

    const vfloat32m2_t angle1 = __riscv_vfdiv_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(
            __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(centroid_nx, dx, vl), centroid_ny, dy, vl),
            centroid_nz,
            dz,
            vl),
        dist_safe,
        vl);
    const vfloat32m2_t angle2 = __riscv_vfdiv_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(
            __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(nx, dx, vl), ny, dy, vl), nz, dz, vl),
        dist_safe,
        vl);

    const vfloat32m2_t abs_angle1 =
        __riscv_vfmin_vf_f32m2(__riscv_vfsgnjx_vv_f32m2(angle1, angle1, vl), 1.0f, vl);
    const vfloat32m2_t abs_angle2 =
        __riscv_vfmin_vf_f32m2(__riscv_vfsgnjx_vv_f32m2(angle2, angle2, vl), 1.0f, vl);
    const vbool16_t swap = __riscv_vmflt_vv_f32m2_b16(abs_angle1, abs_angle2, vl);

    const vfloat32m2_t swapped_dx = __riscv_vfneg_v_f32m2(dx, vl);
    const vfloat32m2_t swapped_dy = __riscv_vfneg_v_f32m2(dy, vl);
    const vfloat32m2_t swapped_dz = __riscv_vfneg_v_f32m2(dz, vl);
    dx = __riscv_vmerge_vvm_f32m2(dx, swapped_dx, swap, vl);
    dy = __riscv_vmerge_vvm_f32m2(dy, swapped_dy, swap, vl);
    dz = __riscv_vmerge_vvm_f32m2(dz, swapped_dz, swap, vl);

    const vfloat32m2_t ux = __riscv_vmerge_vvm_f32m2(centroid_nx, nx, swap, vl);
    const vfloat32m2_t uy = __riscv_vmerge_vvm_f32m2(centroid_ny, ny, swap, vl);
    const vfloat32m2_t uz = __riscv_vmerge_vvm_f32m2(centroid_nz, nz, swap, vl);
    const vfloat32m2_t target_nx = __riscv_vmerge_vvm_f32m2(nx, centroid_nx, swap, vl);
    const vfloat32m2_t target_ny = __riscv_vmerge_vvm_f32m2(ny, centroid_ny, swap, vl);
    const vfloat32m2_t target_nz = __riscv_vmerge_vvm_f32m2(nz, centroid_nz, swap, vl);

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
    const vbool16_t vnorm_valid = __riscv_vmfne_vf_f32m2_b16(vnorm, 0.0f, vl);
    const vbool16_t lane_valid = __riscv_vmand_mm_b16(dist_valid, vnorm_valid, vl);
    const vfloat32m2_t vnorm_safe = __riscv_vmerge_vvm_f32m2(
        __riscv_vfmv_v_f_f32m2(1.0f, vl), vnorm, vnorm_valid, vl);
    vx = __riscv_vfdiv_vv_f32m2(vx, vnorm_safe, vl);
    vy = __riscv_vfdiv_vv_f32m2(vy, vnorm_safe, vl);
    vz = __riscv_vfdiv_vv_f32m2(vz, vnorm_safe, vl);

    const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(uy, vz, vl),
                                                   __riscv_vfmul_vv_f32m2(uz, vy, vl),
                                                   vl);
    const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(uz, vx, vl),
                                                   __riscv_vfmul_vv_f32m2(ux, vz, vl),
                                                   vl);
    const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(ux, vy, vl),
                                                   __riscv_vfmul_vv_f32m2(uy, vx, vl),
                                                   vl);
    const vfloat32m2_t f1_v =
        pcl::atan2_RVV_f32m2(__riscv_vfmacc_vv_f32m2(
                                 __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(wx, target_nx, vl),
                                                         wy,
                                                         target_ny,
                                                         vl),
                                 wz,
                                 target_nz,
                                 vl),
                             __riscv_vfmacc_vv_f32m2(
                                 __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(ux, target_nx, vl),
                                                         uy,
                                                         target_ny,
                                                         vl),
                                 uz,
                                 target_nz,
                                 vl),
                             vl);
    const vfloat32m2_t f2_v = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, target_nx, vl), vy, target_ny, vl),
        vz,
        target_nz,
        vl);
    const vfloat32m2_t f3_v = __riscv_vmerge_vvm_f32m2(angle1, __riscv_vfneg_v_f32m2(angle2, vl), swap, vl);

    __riscv_vse32_v_f32m2(f1.data() + offset, f1_v, vl);
    __riscv_vse32_v_f32m2(f2.data() + offset, f2_v, vl);
    __riscv_vse32_v_f32m2(f3.data() + offset, f3_v, vl);
    __riscv_vse32_v_f32m2(f4.data() + offset, dist, vl);

    const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2(zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2(valid.data() + offset, vvalid, vl);
    offset += vl;
  }

  for (std::size_t i = 0; i < indices.size(); ++i)
  {
    if (valid[i] == 0)
      continue;

    signature[binAngularFeature(f1[i], 45)] += hist_incr;
    signature[45 + binAngularFeature(f2[i], 45)] += hist_incr;
    signature[90 + binAngularFeature(f3[i], 45)] += hist_incr;
    if (size_incr != 0.0f)
    {
      const float distance_value =
          options.normalize_distances && distance_norm != 0.0f ? f4[i] / distance_norm : f4[i] * 100.0f;
      signature[135 + binDistanceFeature(distance_value, 45)] += size_incr;
    }
  }
}

inline void
accumulateViewpointRVV(const Eigen::Vector4f& centroid_p,
                       const CloudT& cloud,
                       const pcl::Indices& indices,
                       const VFHOptions& options,
                       Signature& signature)
{
  constexpr std::size_t kPointStrideBytes = sizeof(PointT);
  constexpr std::size_t kNXOff = pcl::traits::offset<PointT, pcl::fields::normal_x>::value;
  constexpr std::size_t kNYOff = pcl::traits::offset<PointT, pcl::fields::normal_y>::value;
  constexpr std::size_t kNZOff = pcl::traits::offset<PointT, pcl::fields::normal_z>::value;

  static_assert(pcl::rvv::RVVXYZNormalFloatLayout<PointT>::value,
                "VFH Phase 020 candidate expects a dense float PointNormal AoS layout.");

  Eigen::Vector4f d_vp_p = options.viewpoint - centroid_p;
  d_vp_p.normalize();
  const float hist_incr =
      options.normalize_bins ? 100.0f / static_cast<float>(indices.size()) : 1.0f;

  std::vector<float> alpha_values(indices.size());
  const std::uint8_t* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());

  for (std::size_t offset = 0; offset < indices.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const std::uint8_t* batch_u8 = base_u8 + offset * kPointStrideBytes;

    vfloat32m2_t nx;
    vfloat32m2_t ny;
    vfloat32m2_t nz;
    rvv_load::strided_load3_f32m2<kPointStrideBytes, kNXOff, kNYOff, kNZOff>(
        batch_u8, vl, nx, ny, nz);

    const vfloat32m2_t dot = __riscv_vfmacc_vf_f32m2(
        __riscv_vfmacc_vf_f32m2(__riscv_vfmul_vf_f32m2(nx, d_vp_p[0], vl), d_vp_p[1], ny, vl),
        d_vp_p[2],
        nz,
        vl);
    const vfloat32m2_t alpha = __riscv_vfmul_vf_f32m2(
        __riscv_vfadd_vf_f32m2(dot, 1.0f, vl), 0.5f, vl);
    __riscv_vse32_v_f32m2(alpha_values.data() + offset, alpha, vl);

    offset += vl;
  }

  for (const float alpha : alpha_values)
  {
    const auto bin = std::clamp(static_cast<std::size_t>(std::floor(alpha * 128.0f)), 0ul, 127ul);
    signature[180 + bin] += hist_incr;
  }
}
#endif

inline Signature
computeVFHSignatureReference(const CloudT& cloud,
                             const pcl::Indices& indices,
                             const VFHOptions& options = {})
{
  Signature signature{};
  if (indices.size() < 2)
    return signature;

  const Eigen::Vector4f centroid_p = computeCentroidReference(cloud, indices);
  const Eigen::Vector4f centroid_n = computeNormalCentroidReference(cloud, indices);
  accumulateSPFHReference(centroid_p, centroid_n, cloud, indices, options, signature);
  accumulateViewpointReference(centroid_p, cloud, indices, options, signature);
  return signature;
}

inline bool
computeVFHSignatureCentroidSPFHRVV(const CloudT& cloud,
                                   const pcl::Indices& indices,
                                   Signature& signature,
                                   const VFHOptions& options = {})
{
#if defined(__RVV10__)
  signature.fill(0.0f);
  if (indices.size() < 2 || !indicesAreSequentialFullCloud(cloud, indices))
    return false;

  const Eigen::Vector4f centroid_p = computeCentroidReference(cloud, indices);
  const Eigen::Vector4f centroid_n = computeNormalCentroidReference(cloud, indices);
  accumulateSPFHCentroidPairMathRVV(centroid_p, centroid_n, cloud, indices, options, signature);
  accumulateViewpointReference(centroid_p, cloud, indices, options, signature);
  return true;
#else
  return false;
#endif
}

inline bool
computeVFHSignatureSPFHAndViewpointRVV(const CloudT& cloud,
                                       const pcl::Indices& indices,
                                       Signature& signature,
                                       const VFHOptions& options = {})
{
#if defined(__RVV10__)
  signature.fill(0.0f);
  if (indices.size() < 2 || !indicesAreSequentialFullCloud(cloud, indices))
    return false;

  const Eigen::Vector4f centroid_p = computeCentroidReference(cloud, indices);
  const Eigen::Vector4f centroid_n = computeNormalCentroidReference(cloud, indices);
  accumulateSPFHCentroidPairMathRVV(centroid_p, centroid_n, cloud, indices, options, signature);
  accumulateViewpointRVV(centroid_p, cloud, indices, options, signature);
  return true;
#else
  return false;
#endif
}

inline bool
computeVFHSignatureCentroidsSPFHAndViewpointRVV(const CloudT& cloud,
                                                const pcl::Indices& indices,
                                                Signature& signature,
                                                const VFHOptions& options = {})
{
#if defined(__RVV10__)
  signature.fill(0.0f);
  if (indices.size() < 2 || !indicesAreSequentialFullCloud(cloud, indices))
    return false;

  const Eigen::Vector4f centroid_p = computeCentroidReference(cloud, indices);
  const Eigen::Vector4f centroid_n = computeNormalCentroidRVV(cloud, indices);
  accumulateSPFHCentroidPairMathRVV(centroid_p, centroid_n, cloud, indices, options, signature);
  accumulateViewpointRVV(centroid_p, cloud, indices, options, signature);
  return true;
#else
  return false;
#endif
}

} // namespace pcl::features::rvv_test::vfh
