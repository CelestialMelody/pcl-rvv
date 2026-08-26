#pragma once

/*
 * 本文件做什么：
 * 这里保存 PPF Phase 010 的 test-only RVV candidate（测试专用 RVV 候选）。
 * 候选先按 production 的输出顺序收集非 identity pair（非同一点对）到 SoA staging
 * （分字段暂存），再用 RVV 批量计算当前 `impl/ppf.hpp` 实际调用的 `computePairFeatures`
 * 风格 `f1..f4`。`alpha_m` 暂时保持标量 reference，用来把 pair-feature math
 * （点对特征数学链路）和 PPF 特有旋转后段分开取证。
 *
 * 证据边界：
 * 这是 component ablation（组件消融）候选，不是 production dispatch（生产分流）。
 * Staging 成本应计入 bench；QEMU correctness（QEMU 正确性验证）只证明功能和路径，不证明真实性能。
 */

#include "impl/ppf_reference.hpp"

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>

#include <riscv_vector.h>
#endif

#include <cstdint>
#include <vector>

namespace pcl::features::rvv_test::ppf
{

struct PairFeatureStaging
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
  std::vector<std::uint32_t> output_rows;
  std::vector<std::uint32_t> p1_indices;
  std::vector<std::uint32_t> p2_indices;
};

inline void
reservePairFeatureStaging(PairFeatureStaging& staging, const std::size_t pair_count)
{
  staging.p1x.reserve(pair_count);
  staging.p1y.reserve(pair_count);
  staging.p1z.reserve(pair_count);
  staging.p2x.reserve(pair_count);
  staging.p2y.reserve(pair_count);
  staging.p2z.reserve(pair_count);
  staging.n1x.reserve(pair_count);
  staging.n1y.reserve(pair_count);
  staging.n1z.reserve(pair_count);
  staging.n2x.reserve(pair_count);
  staging.n2y.reserve(pair_count);
  staging.n2z.reserve(pair_count);
  staging.output_rows.reserve(pair_count);
  staging.p1_indices.reserve(pair_count);
  staging.p2_indices.reserve(pair_count);
}

inline PairFeatureStaging
makePairFeatureStaging(const XYZCloudT& cloud,
                       const NormalCloudT& normals,
                       const pcl::Indices& indices)
{
  PairFeatureStaging staging;
  reservePairFeatureStaging(staging, indices.size() * cloud.size());
  for (std::size_t index_i = 0; index_i < indices.size(); ++index_i)
  {
    const auto i = static_cast<std::size_t>(indices[index_i]);
    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      if (i == j)
        continue;
      staging.p1x.push_back(cloud[i].x);
      staging.p1y.push_back(cloud[i].y);
      staging.p1z.push_back(cloud[i].z);
      staging.p2x.push_back(cloud[j].x);
      staging.p2y.push_back(cloud[j].y);
      staging.p2z.push_back(cloud[j].z);
      staging.n1x.push_back(normals[i].normal_x);
      staging.n1y.push_back(normals[i].normal_y);
      staging.n1z.push_back(normals[i].normal_z);
      staging.n2x.push_back(normals[j].normal_x);
      staging.n2y.push_back(normals[j].normal_y);
      staging.n2z.push_back(normals[j].normal_z);
      staging.output_rows.push_back(static_cast<std::uint32_t>(index_i * cloud.size() + j));
      staging.p1_indices.push_back(static_cast<std::uint32_t>(i));
      staging.p2_indices.push_back(static_cast<std::uint32_t>(j));
    }
  }
  return staging;
}

#if defined(__RVV10__)
inline void
computePairFeaturesRVV(const PairFeatureStaging& staging,
                       std::vector<float>& f1,
                       std::vector<float>& f2,
                       std::vector<float>& f3,
                       std::vector<float>& f4,
                       std::vector<std::int32_t>& valid)
{
  const std::size_t pair_count = staging.p1x.size();
  f1.resize(pair_count);
  f2.resize(pair_count);
  f3.resize(pair_count);
  f4.resize(pair_count);
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
    __riscv_vse32_v_f32m2(f4.data() + offset, dist, vl);
    const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2(zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2(valid.data() + offset, vvalid, vl);
    offset += vl;
  }
}
#endif

inline void
computePPFPairFeatureBatchRVV(const XYZCloudT& cloud,
                              const NormalCloudT& normals,
                              const pcl::Indices& indices,
                              pcl::PointCloud<pcl::PPFSignature>& output)
{
  output.resize(indices.size() * cloud.size());
  output.height = 1;
  output.width = static_cast<std::uint32_t>(output.size());
  output.is_dense = true;

  for (auto& signature : output)
    setNaN(signature);
  if (!indices.empty())
    output.is_dense = false;

#if defined(__RVV10__)
  const PairFeatureStaging staging = makePairFeatureStaging(cloud, normals, indices);
  std::vector<float> f1;
  std::vector<float> f2;
  std::vector<float> f3;
  std::vector<float> f4;
  std::vector<std::int32_t> valid;
  computePairFeaturesRVV(staging, f1, f2, f3, f4, valid);

  for (std::size_t i = 0; i < valid.size(); ++i)
  {
    const std::size_t row = staging.output_rows[i];
    if (valid[i] == 0)
    {
      setNaN(output[row]);
      output.is_dense = false;
      continue;
    }
    auto& signature = output[row];
    signature.f1 = f1[i];
    signature.f2 = f2[i];
    signature.f3 = f3[i];
    signature.f4 = f4[i];
    signature.alpha_m = computeAlphaMReference(cloud[staging.p1_indices[i]],
                                               normals[staging.p1_indices[i]],
                                               cloud[staging.p2_indices[i]]);
  }
#else
  computePPFReference(cloud, normals, indices, output);
#endif
}

} // namespace pcl::features::rvv_test::ppf
