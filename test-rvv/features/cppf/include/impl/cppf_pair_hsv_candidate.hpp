#pragma once

/*
 * 本文件做什么：
 * 这里保存 CPPF Phase 000 的 test-only RVV candidate（测试专用 RVV 候选）。
 * 候选先按 production（生产源码）的输出顺序把非 identity pair（非同一点对）
 * 收集到 SoA staging（分字段暂存），再用 RVV 批量计算 `f1..f10`。`alpha_m`
 * 暂时复用标量 reference，用来把 colored pair feature（彩色点对特征）和
 * CPPF 特有旋转后段分开取证。
 *
 * 证据边界：
 * 这是 component ablation（组件消融）候选，不是 production dispatch（生产分流）。
 * Staging 成本应计入 bench；QEMU correctness（QEMU 正确性验证）只证明功能和
 * 路径，不证明真实性能。
 */

#include "impl/cppf_reference.hpp"

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

#include <cstdint>
#include <vector>

namespace pcl::features::rvv_test::cppf
{

struct PairHSVStaging
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
  std::vector<std::int32_t> c1r;
  std::vector<std::int32_t> c1g;
  std::vector<std::int32_t> c1b;
  std::vector<std::int32_t> c2r;
  std::vector<std::int32_t> c2g;
  std::vector<std::int32_t> c2b;
  std::vector<std::uint32_t> output_rows;
  std::vector<std::uint32_t> p1_indices;
  std::vector<std::uint32_t> p2_indices;
};

inline void
reservePairHSVStaging(PairHSVStaging& staging, const std::size_t pair_count)
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
  staging.c1r.reserve(pair_count);
  staging.c1g.reserve(pair_count);
  staging.c1b.reserve(pair_count);
  staging.c2r.reserve(pair_count);
  staging.c2g.reserve(pair_count);
  staging.c2b.reserve(pair_count);
  staging.output_rows.reserve(pair_count);
  staging.p1_indices.reserve(pair_count);
  staging.p2_indices.reserve(pair_count);
}

inline PairHSVStaging
makePairHSVStaging(const CloudT& cloud, const pcl::Indices& indices)
{
  PairHSVStaging staging;
  reservePairHSVStaging(staging, indices.size() * cloud.size());
  for (std::size_t index_i = 0; index_i < indices.size(); ++index_i)
  {
    const auto i = static_cast<std::size_t>(indices[index_i]);
    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      if (i == j)
        continue;

      const Eigen::Vector4i c1 = cloud[i].getRGBVector4i();
      const Eigen::Vector4i c2 = cloud[j].getRGBVector4i();
      staging.p1x.push_back(cloud[i].x);
      staging.p1y.push_back(cloud[i].y);
      staging.p1z.push_back(cloud[i].z);
      staging.p2x.push_back(cloud[j].x);
      staging.p2y.push_back(cloud[j].y);
      staging.p2z.push_back(cloud[j].z);
      staging.n1x.push_back(cloud[i].normal_x);
      staging.n1y.push_back(cloud[i].normal_y);
      staging.n1z.push_back(cloud[i].normal_z);
      staging.n2x.push_back(cloud[j].normal_x);
      staging.n2y.push_back(cloud[j].normal_y);
      staging.n2z.push_back(cloud[j].normal_z);
      staging.c1r.push_back(c1[0]);
      staging.c1g.push_back(c1[1]);
      staging.c1b.push_back(c1[2]);
      staging.c2r.push_back(c2[0]);
      staging.c2g.push_back(c2[1]);
      staging.c2b.push_back(c2[2]);
      staging.output_rows.push_back(static_cast<std::uint32_t>(index_i * cloud.size() + j));
      staging.p1_indices.push_back(static_cast<std::uint32_t>(i));
      staging.p2_indices.push_back(static_cast<std::uint32_t>(j));
    }
  }
  return staging;
}

#if defined(__RVV10__)
inline void
computeHSVRVV(const vint32m2_t r,
              const vint32m2_t g,
              const vint32m2_t b,
              vfloat32m2_t& hue_unit,
              vfloat32m2_t& saturation,
              vfloat32m2_t& value,
              const std::size_t vl)
{
  const vint32m2_t max_rg = __riscv_vmax_vv_i32m2(r, g, vl);
  const vint32m2_t max_rgb = __riscv_vmax_vv_i32m2(max_rg, b, vl);
  const vint32m2_t min_rg = __riscv_vmin_vv_i32m2(r, g, vl);
  const vint32m2_t min_rgb = __riscv_vmin_vv_i32m2(min_rg, b, vl);
  const vint32m2_t diff_i = __riscv_vsub_vv_i32m2(max_rgb, min_rgb, vl);

  const vfloat32m2_t max_f = __riscv_vfcvt_f_x_v_f32m2(max_rgb, vl);
  const vfloat32m2_t diff_f = __riscv_vfcvt_f_x_v_f32m2(diff_i, vl);
  const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2(1.0f, vl);
  const vbool16_t max_is_zero = __riscv_vmseq_vx_i32m2_b16(max_rgb, 0, vl);
  const vbool16_t diff_is_zero = __riscv_vmseq_vx_i32m2_b16(diff_i, 0, vl);
  const vfloat32m2_t max_safe =
      __riscv_vmerge_vvm_f32m2(max_f, one, max_is_zero, vl);
  const vfloat32m2_t diff_safe =
      __riscv_vmerge_vvm_f32m2(diff_f, one, diff_is_zero, vl);

  value = __riscv_vfdiv_vf_f32m2(max_f, 255.0f, vl);
  saturation = __riscv_vfdiv_vv_f32m2(diff_f, max_safe, vl);
  saturation = __riscv_vmerge_vvm_f32m2(saturation, __riscv_vfmv_v_f_f32m2(0.0f, vl), max_is_zero, vl);

  const vfloat32m2_t r_f = __riscv_vfcvt_f_x_v_f32m2(r, vl);
  const vfloat32m2_t g_f = __riscv_vfcvt_f_x_v_f32m2(g, vl);
  const vfloat32m2_t b_f = __riscv_vfcvt_f_x_v_f32m2(b, vl);
  const vfloat32m2_t rg_delta = __riscv_vfsub_vv_f32m2(g_f, b_f, vl);
  const vfloat32m2_t gb_delta = __riscv_vfsub_vv_f32m2(b_f, r_f, vl);
  const vfloat32m2_t br_delta = __riscv_vfsub_vv_f32m2(r_f, g_f, vl);
  vfloat32m2_t hue_r =
      __riscv_vfmul_vf_f32m2(__riscv_vfdiv_vv_f32m2(rg_delta, diff_safe, vl), 60.0f, vl);
  const vbool16_t hue_r_negative = __riscv_vmflt_vf_f32m2_b16(hue_r, 0.0f, vl);
  hue_r = __riscv_vfadd_vv_f32m2(
      hue_r,
      __riscv_vmerge_vvm_f32m2(__riscv_vfmv_v_f_f32m2(0.0f, vl),
                               __riscv_vfmv_v_f_f32m2(360.0f, vl),
                               hue_r_negative,
                               vl),
      vl);
  const vfloat32m2_t hue_g = __riscv_vfmul_vf_f32m2(
      __riscv_vfadd_vf_f32m2(__riscv_vfdiv_vv_f32m2(gb_delta, diff_safe, vl), 2.0f, vl),
      60.0f,
      vl);
  const vfloat32m2_t hue_b = __riscv_vfmul_vf_f32m2(
      __riscv_vfadd_vf_f32m2(__riscv_vfdiv_vv_f32m2(br_delta, diff_safe, vl), 4.0f, vl),
      60.0f,
      vl);

  const vbool16_t max_is_r = __riscv_vmseq_vv_i32m2_b16(max_rgb, r, vl);
  const vbool16_t max_is_g = __riscv_vmand_mm_b16(
      __riscv_vmnot_m_b16(max_is_r, vl), __riscv_vmseq_vv_i32m2_b16(max_rgb, g, vl), vl);
  vfloat32m2_t hue = __riscv_vmerge_vvm_f32m2(hue_b, hue_g, max_is_g, vl);
  hue = __riscv_vmerge_vvm_f32m2(hue, hue_r, max_is_r, vl);
  const vbool16_t hue_zero = __riscv_vmor_mm_b16(max_is_zero, diff_is_zero, vl);
  hue = __riscv_vmerge_vvm_f32m2(hue, __riscv_vfmv_v_f_f32m2(0.0f, vl), hue_zero, vl);
  hue_unit = __riscv_vfdiv_vf_f32m2(hue, 360.0f, vl);
}

inline void
computePairHSVFeaturesRVV(const PairHSVStaging& staging,
                          std::vector<float>& f1,
                          std::vector<float>& f2,
                          std::vector<float>& f3,
                          std::vector<float>& f4,
                          std::vector<float>& f5,
                          std::vector<float>& f6,
                          std::vector<float>& f7,
                          std::vector<float>& f8,
                          std::vector<float>& f9,
                          std::vector<float>& f10)
{
  const std::size_t pair_count = staging.p1x.size();
  f1.resize(pair_count);
  f2.resize(pair_count);
  f3.resize(pair_count);
  f4.resize(pair_count);
  f5.resize(pair_count);
  f6.resize(pair_count);
  f7.resize(pair_count);
  f8.resize(pair_count);
  f9.resize(pair_count);
  f10.resize(pair_count);

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

    const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(p2x, p1x, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(p2y, p1y, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(p2z, p1z, vl);
    vfloat32m2_t dist2 =
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
    const vfloat32m2_t unit_dx = __riscv_vfdiv_vv_f32m2(dx, dist, vl);
    const vfloat32m2_t unit_dy = __riscv_vfdiv_vv_f32m2(dy, dist, vl);
    const vfloat32m2_t unit_dz = __riscv_vfdiv_vv_f32m2(dz, dist, vl);

    const vfloat32m2_t vf1 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, unit_dx, vl), n1y, unit_dy, vl),
        n1z,
        unit_dz,
        vl);
    const vfloat32m2_t vf2 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n2x, unit_dx, vl), n2y, unit_dy, vl),
        n2z,
        unit_dz,
        vl);
    const vfloat32m2_t vf3 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, n2x, vl), n1y, n2y, vl),
        n1z,
        n2z,
        vl);

    vfloat32m2_t h1;
    vfloat32m2_t s1;
    vfloat32m2_t v1;
    vfloat32m2_t h2;
    vfloat32m2_t s2;
    vfloat32m2_t v2;
    computeHSVRVV(__riscv_vle32_v_i32m2(staging.c1r.data() + offset, vl),
                  __riscv_vle32_v_i32m2(staging.c1g.data() + offset, vl),
                  __riscv_vle32_v_i32m2(staging.c1b.data() + offset, vl),
                  h1,
                  s1,
                  v1,
                  vl);
    computeHSVRVV(__riscv_vle32_v_i32m2(staging.c2r.data() + offset, vl),
                  __riscv_vle32_v_i32m2(staging.c2g.data() + offset, vl),
                  __riscv_vle32_v_i32m2(staging.c2b.data() + offset, vl),
                  h2,
                  s2,
                  v2,
                  vl);

    __riscv_vse32_v_f32m2(f1.data() + offset, vf1, vl);
    __riscv_vse32_v_f32m2(f2.data() + offset, vf2, vl);
    __riscv_vse32_v_f32m2(f3.data() + offset, vf3, vl);
    __riscv_vse32_v_f32m2(f4.data() + offset, dist, vl);
    __riscv_vse32_v_f32m2(f5.data() + offset, h1, vl);
    __riscv_vse32_v_f32m2(f6.data() + offset, s1, vl);
    __riscv_vse32_v_f32m2(f7.data() + offset, v1, vl);
    __riscv_vse32_v_f32m2(f8.data() + offset, h2, vl);
    __riscv_vse32_v_f32m2(f9.data() + offset, s2, vl);
    __riscv_vse32_v_f32m2(f10.data() + offset, v2, vl);
    offset += vl;
  }
}
#endif

inline void
computeCPPFPairHSVBatchRVV(const CloudT& cloud,
                           const pcl::Indices& indices,
                           pcl::PointCloud<pcl::CPPFSignature>& output)
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
  const PairHSVStaging staging = makePairHSVStaging(cloud, indices);
  std::vector<float> f1;
  std::vector<float> f2;
  std::vector<float> f3;
  std::vector<float> f4;
  std::vector<float> f5;
  std::vector<float> f6;
  std::vector<float> f7;
  std::vector<float> f8;
  std::vector<float> f9;
  std::vector<float> f10;
  computePairHSVFeaturesRVV(staging, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);

  for (std::size_t k = 0; k < f1.size(); ++k)
  {
    auto& signature = output[staging.output_rows[k]];
    signature.f1 = f1[k];
    signature.f2 = f2[k];
    signature.f3 = f3[k];
    signature.f4 = f4[k];
    signature.f5 = f5[k];
    signature.f6 = f6[k];
    signature.f7 = f7[k];
    signature.f8 = f8[k];
    signature.f9 = f9[k];
    signature.f10 = f10[k];
    signature.alpha_m = computeAlphaMReference(cloud[staging.p1_indices[k]],
                                               cloud[staging.p1_indices[k]],
                                               cloud[staging.p2_indices[k]]);
  }
#else
  computeCPPFReference(cloud, indices, output);
#endif
}

} // namespace pcl::features::rvv_test::cppf
