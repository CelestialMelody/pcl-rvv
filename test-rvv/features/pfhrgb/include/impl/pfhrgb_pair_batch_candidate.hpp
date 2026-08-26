#pragma once

/*
 * 本文件做什么：
 * 这里保存 PFHRGB Phase 000 的 test-only RVV candidate（测试专用 RVV 候选）。
 * 它把邻域中的 ordered pairs（有向点对）整理成连续 SoA staging（结构数组到分字段暂存），
 * 用 RVV lane-level helper（单段向量 helper）计算几何 pair tuple 和 RGB ratio（颜色比例），
 * 最后保持 production 的标量 histogram scatter（直方图离散累加）顺序。
 *
 * 证据边界：
 * 这是 production-shaped diagnostic（生产形态诊断），不是 production dispatch（生产分流）。
 * Staging 成本计入 bench；点类型 gate、direct AoS load、fallback 矩阵和真实 production
 * 入口证据都要等后续 phase 再审计。
 */

#include "impl/pfhrgb_reference.hpp"

#include <pcl/search/kdtree.h>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>

#include <riscv_vector.h>
#endif

#include <algorithm>
#include <cstdint>
#include <vector>

namespace pcl::features::rvv_test::pfhrgb
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
  std::vector<float> r1;
  std::vector<float> g1;
  std::vector<float> b1;
  std::vector<float> r2;
  std::vector<float> g2;
  std::vector<float> b2;
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
  staging.r1.reserve(size);
  staging.g1.reserve(size);
  staging.b1.reserve(size);
  staging.r2.reserve(size);
  staging.g2.reserve(size);
  staging.b2.reserve(size);
}

inline void
clearPairBatch(PairBatchStaging& staging)
{
  staging.p1x.clear();
  staging.p1y.clear();
  staging.p1z.clear();
  staging.p2x.clear();
  staging.p2y.clear();
  staging.p2z.clear();
  staging.n1x.clear();
  staging.n1y.clear();
  staging.n1z.clear();
  staging.n2x.clear();
  staging.n2y.clear();
  staging.n2z.clear();
  staging.r1.clear();
  staging.g1.clear();
  staging.b1.clear();
  staging.r2.clear();
  staging.g2.clear();
  staging.b2.clear();
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
  staging.r1.push_back(static_cast<float>(p1.r));
  staging.g1.push_back(static_cast<float>(p1.g));
  staging.b1.push_back(static_cast<float>(p1.b));
  staging.r2.push_back(static_cast<float>(p2.r));
  staging.g2.push_back(static_cast<float>(p2.g));
  staging.b2.push_back(static_cast<float>(p2.b));
}

inline void
fillPairBatchStaging(PairBatchStaging& staging, const CloudT& cloud, const pcl::Indices& indices)
{
  clearPairBatch(staging);
  reservePairBatch(staging, indices.size() * (indices.size() - 1));
  for (const auto index_i : indices)
  {
    for (const auto index_j : indices)
    {
      if (index_i == index_j)
        continue;
      appendPair(staging,
                 cloud[static_cast<std::size_t>(index_i)],
                 cloud[static_cast<std::size_t>(index_j)]);
    }
  }
}

inline PairBatchStaging
makePairBatchStaging(const CloudT& cloud, const pcl::Indices& indices)
{
  PairBatchStaging staging;
  fillPairBatchStaging(staging, cloud, indices);
  return staging;
}

struct PairBatchWorkspace
{
  PairBatchStaging staging;
  std::vector<float> f1;
  std::vector<float> f2;
  std::vector<float> f3;
  std::vector<float> f5;
  std::vector<float> f6;
  std::vector<float> f7;
  std::vector<std::int32_t> valid;
};

#if defined(__RVV10__)
inline vfloat32m2_t
computeColorRatioRVV(const vfloat32m2_t numerator, const vfloat32m2_t denominator, const std::size_t vl)
{
  const vbool16_t non_zero = __riscv_vmfne_vf_f32m2_b16(denominator, 0.0f, vl);
  const vfloat32m2_t raw_ratio = __riscv_vfdiv_vv_f32m2(numerator, denominator, vl);
  const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2(1.0f, vl);
  vfloat32m2_t ratio = __riscv_vmerge_vvm_f32m2(one, raw_ratio, non_zero, vl);
  const vbool16_t greater_than_one = __riscv_vmfgt_vf_f32m2_b16(ratio, 1.0f, vl);
  const vfloat32m2_t inverted = __riscv_vfneg_v_f32m2(__riscv_vfrdiv_vf_f32m2(ratio, 1.0f, vl), vl);
  ratio = __riscv_vmerge_vvm_f32m2(ratio, inverted, greater_than_one, vl);
  return ratio;
}

inline void
computePairTuplesRVV(const PairBatchStaging& staging,
                     std::vector<float>& f1,
                     std::vector<float>& f2,
                     std::vector<float>& f3,
                     std::vector<float>& f5,
                     std::vector<float>& f6,
                     std::vector<float>& f7,
                     std::vector<std::int32_t>& valid)
{
  const std::size_t pair_count = staging.p1x.size();
  f1.resize(pair_count);
  f2.resize(pair_count);
  f3.resize(pair_count);
  f5.resize(pair_count);
  f6.resize(pair_count);
  f7.resize(pair_count);
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

    const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(p2x, p1x, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(p2y, p1y, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(p2z, p1z, vl);
    vfloat32m2_t dist2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, dx, vl), dy, dy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
    vbool16_t lane_valid = __riscv_vmfne_vf_f32m2_b16(dist, 0.0f, vl);

    const vfloat32m2_t n1_dot_delta = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, dx, vl), n1y, dy, vl), n1z, dz, vl);
    const vfloat32m2_t vf3 = __riscv_vfdiv_vv_f32m2(n1_dot_delta, dist, vl);

    vfloat32m2_t vx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dy, n1z, vl),
                                             __riscv_vfmul_vv_f32m2(dz, n1y, vl),
                                             vl);
    vfloat32m2_t vy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dz, n1x, vl),
                                             __riscv_vfmul_vv_f32m2(dx, n1z, vl),
                                             vl);
    vfloat32m2_t vz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(dx, n1y, vl),
                                             __riscv_vfmul_vv_f32m2(dy, n1x, vl),
                                             vl);
    vfloat32m2_t vnorm2 = __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, vx, vl), vy, vy, vl);
    vnorm2 = __riscv_vfmacc_vv_f32m2(vnorm2, vz, vz, vl);
    const vfloat32m2_t vnorm = __riscv_vfsqrt_v_f32m2(vnorm2, vl);
    lane_valid = __riscv_vmand_mm_b16(lane_valid, __riscv_vmfne_vf_f32m2_b16(vnorm, 0.0f, vl), vl);
    vx = __riscv_vfdiv_vv_f32m2(vx, vnorm, vl);
    vy = __riscv_vfdiv_vv_f32m2(vy, vnorm, vl);
    vz = __riscv_vfdiv_vv_f32m2(vz, vnorm, vl);

    const vfloat32m2_t wx = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(n1y, vz, vl),
                                                   __riscv_vfmul_vv_f32m2(n1z, vy, vl),
                                                   vl);
    const vfloat32m2_t wy = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(n1z, vx, vl),
                                                   __riscv_vfmul_vv_f32m2(n1x, vz, vl),
                                                   vl);
    const vfloat32m2_t wz = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, vy, vl),
                                                   __riscv_vfmul_vv_f32m2(n1y, vx, vl),
                                                   vl);
    const vfloat32m2_t vf2 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, n2x, vl), vy, n2y, vl),
        vz,
        n2z,
        vl);
    const vfloat32m2_t atan_y = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(wx, n2x, vl), wy, n2y, vl),
        wz,
        n2z,
        vl);
    const vfloat32m2_t atan_x = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(n1x, n2x, vl), n1y, n2y, vl),
        n1z,
        n2z,
        vl);
    const vfloat32m2_t vf1 = pcl::atan2_RVV_f32m2(atan_y, atan_x, vl);

    const vfloat32m2_t vf5 = computeColorRatioRVV(
        __riscv_vle32_v_f32m2(staging.r1.data() + offset, vl),
        __riscv_vle32_v_f32m2(staging.r2.data() + offset, vl),
        vl);
    const vfloat32m2_t vf6 = computeColorRatioRVV(
        __riscv_vle32_v_f32m2(staging.g1.data() + offset, vl),
        __riscv_vle32_v_f32m2(staging.g2.data() + offset, vl),
        vl);
    const vfloat32m2_t vf7 = computeColorRatioRVV(
        __riscv_vle32_v_f32m2(staging.b1.data() + offset, vl),
        __riscv_vle32_v_f32m2(staging.b2.data() + offset, vl),
        vl);

    __riscv_vse32_v_f32m2(f1.data() + offset, vf1, vl);
    __riscv_vse32_v_f32m2(f2.data() + offset, vf2, vl);
    __riscv_vse32_v_f32m2(f3.data() + offset, vf3, vl);
    __riscv_vse32_v_f32m2(f5.data() + offset, vf5, vl);
    __riscv_vse32_v_f32m2(f6.data() + offset, vf6, vl);
    __riscv_vse32_v_f32m2(f7.data() + offset, vf7, vl);
    const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
    const vint32m2_t vvalid = __riscv_vmerge_vxm_i32m2(zero, 1, lane_valid, vl);
    __riscv_vse32_v_i32m2(valid.data() + offset, vvalid, vl);
    offset += vl;
  }
}
#endif

inline void
computePointPFHRGBSignaturePairBatchRVVWithWorkspace(const CloudT& cloud,
                                                     const pcl::Indices& indices,
                                                     const int nr_split,
                                                     Eigen::VectorXf& histogram,
                                                     PairBatchWorkspace& workspace)
{
#if defined(__RVV10__)
  histogram.setZero(2 * nr_split * nr_split * nr_split);
  const float hist_incr =
      100.0f / static_cast<float>(indices.size() * (indices.size() - 1) / 2);
  fillPairBatchStaging(workspace.staging, cloud, indices);
  computePairTuplesRVV(workspace.staging,
                       workspace.f1,
                       workspace.f2,
                       workspace.f3,
                       workspace.f5,
                       workspace.f6,
                       workspace.f7,
                       workspace.valid);

  for (std::size_t i = 0; i < workspace.valid.size(); ++i)
  {
    if (workspace.valid[i] == 0)
      continue;
    accumulatePFHRGBHistogramBins(workspace.f1[i],
                                  workspace.f2[i],
                                  workspace.f3[i],
                                  workspace.f5[i],
                                  workspace.f6[i],
                                  workspace.f7[i],
                                  nr_split,
                                  hist_incr,
                                  histogram);
  }
#else
  (void)workspace;
  computePointPFHRGBReference(cloud, indices, nr_split, histogram);
#endif
}

inline void
computePointPFHRGBSignaturePairBatchRVV(const CloudT& cloud,
                                        const pcl::Indices& indices,
                                        const int nr_split,
                                        Eigen::VectorXf& histogram)
{
  PairBatchWorkspace workspace;
  computePointPFHRGBSignaturePairBatchRVVWithWorkspace(cloud, indices, nr_split, histogram, workspace);
}

inline void
copyHistogramToSignature(const Eigen::VectorXf& histogram, pcl::PFHRGBSignature250& descriptor)
{
  std::copy(histogram.data(), histogram.data() + histogram.size(), descriptor.histogram);
}

inline void
computePublicPFHRGBWithPairBatchCandidate(const CloudT& cloud,
                                          const int k,
                                          pcl::PointCloud<pcl::PFHRGBSignature250>& output)
{
  auto tree = pcl::make_shared<pcl::search::KdTree<PointT>>();
  tree->setInputCloud(cloud.makeShared());

  output.clear();
  output.resize(cloud.size());
  output.width = static_cast<std::uint32_t>(cloud.size());
  output.height = 1;
  output.is_dense = true;

  pcl::Indices nn_indices(static_cast<std::size_t>(k));
  std::vector<float> nn_dists(static_cast<std::size_t>(k));
  Eigen::VectorXf histogram(250);

  for (std::size_t idx = 0; idx < cloud.size(); ++idx)
  {
    tree->nearestKSearch(static_cast<int>(idx), k, nn_indices, nn_dists);
    computePointPFHRGBSignaturePairBatchRVV(cloud, nn_indices, 5, histogram);
    copyHistogramToSignature(histogram, output[idx]);
  }
}

inline void
computePublicPFHRGBWithReusablePairBatchCandidate(const CloudT& cloud,
                                                  const int k,
                                                  pcl::PointCloud<pcl::PFHRGBSignature250>& output)
{
  auto tree = pcl::make_shared<pcl::search::KdTree<PointT>>();
  tree->setInputCloud(cloud.makeShared());

  output.clear();
  output.resize(cloud.size());
  output.width = static_cast<std::uint32_t>(cloud.size());
  output.height = 1;
  output.is_dense = true;

  pcl::Indices nn_indices(static_cast<std::size_t>(k));
  std::vector<float> nn_dists(static_cast<std::size_t>(k));
  Eigen::VectorXf histogram(250);
  PairBatchWorkspace workspace;

  for (std::size_t idx = 0; idx < cloud.size(); ++idx)
  {
    tree->nearestKSearch(static_cast<int>(idx), k, nn_indices, nn_dists);
    computePointPFHRGBSignaturePairBatchRVVWithWorkspace(cloud, nn_indices, 5, histogram, workspace);
    copyHistogramToSignature(histogram, output[idx]);
  }
}

} // namespace pcl::features::rvv_test::pfhrgb
