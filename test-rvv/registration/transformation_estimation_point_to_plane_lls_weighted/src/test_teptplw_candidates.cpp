/*
 * 本文件做什么：
 * candidate、block-reduction、fused formula 与 production-default 对拍。
 * 共享 fixtures/assertions 来自 test_teptplw.h，TEST body 保持证据语义清晰。
 */

#include "test_teptplw.h"

#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;
namespace prod_detail = pcl::registration::detail;
using namespace pcl::registration::rvv_te_pt2plane_lls_weighted_test;

// 这个测试验证连续 PointNormal 全云输入下，RVV 候选链路会命中带权公式暂存阶段
//（weighted staging）并与标量一致。如果 RVV 构建没有命中或矩阵超差，说明跨步加载、
// 权重加载或公式 gate 断了。
TEST(TransformationEstimationPointToPlaneLLSWeighted, FullCloudCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_full(source, target, weights, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个 A/B 测试把 current vcompress + fixed buffer + tail 与 full-cloud
// block-reduction 分开。block 仅改变跨 lane reduction tree，不改变 weighted lane formula。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionMatchesStdWithinBudget)
{
  auto source = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());
  source[7].x = std::numeric_limits<float>::quiet_NaN();
  target[19].normal_z = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats current_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation current_eq =
      diag::accumulate_candidate_full(source, target, weights, &current_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_NEAR((current_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((current_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  EXPECT_NEAR((block_eq.ata - std_eq.ata).norm(), 0.0, 2e-1);
  EXPECT_NEAR((block_eq.atb - std_eq.atb).norm(), 0.0, 2e-1);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   2e-3f);
}

// 这个测试构造 near-cancellation（近抵消）样本：target 沿 normal 做很小的正负扰动，
// 让 ATb 中的贡献互相抵消。它专门保护 block-reduction 改变规约树后的矩阵预算。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionNearCancellationStressMatchesStd)
{
  auto source = makeSurfaceCloud(31, 0.055f);
  auto target = source;
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    const float eps = sign * (2.0e-4f + 1.0e-5f * static_cast<float>(i % 7));
    target[i].x += eps * target[i].normal_x;
    target[i].y += eps * target[i].normal_y;
    target[i].z += eps * target[i].normal_z;
    weights.push_back(0.25f + 0.05f * static_cast<float>(i % 11));
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(block_eq, std_eq, 5e-2, 2e-5, 5e-4, 2e-4);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   4e-3f);
}

// 这个测试把坐标和权重拉开到更宽尺度，确认 block-reduction 在大 ATA/ATb 量级下
// 仍保持可解释的相对误差预算。它不把该预算写成 production 语义，只服务 PI1 风险评估。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionScaleStressMatchesStd)
{
  auto source = makeSurfaceCloud(33, 0.21f);
  for (auto& point : source) {
    point.x *= 8.0f;
    point.y *= 8.0f;
    point.z *= 4.0f;
  }
  const auto target = makeTargetCloud(source);
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float scale = (i % 3 == 0) ? 0.015f : ((i % 3 == 1) ? 3.5f : 48.0f);
    weights.push_back(scale + 0.125f * static_cast<float>(i % 5));
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(block_eq, std_eq, 4e4, 3e-5, 1e4, 3e-5);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   8e-3f);
}

// 这个测试同时放入非有限 point/normal 和非有限 weight。point/normal 非有限的 lane
// 必须被剔除；point/normal 有限但 weight 非有限的 lane 必须保留并污染法方程。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionPreservesNonFiniteWeightSemantics)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_y = std::numeric_limits<float>::infinity();
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(block_eq.ata.norm()));
}

// fused formula A/B 常规样本：candidate B 只改变逐点 a/b/c/d 公式树，仍复用
// 当前 block baseline A 的 full-cloud、连续 weights、finite mask 和 A/B/C/N 规约。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockFusedFormulaCandidatesMatchBlockAndStdWithinBudget)
{
  auto source = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());
  source[7].x = std::numeric_limits<float>::quiet_NaN();
  target[19].normal_z = std::numeric_limits<float>::infinity();

  for (const auto& candidate : weightedFusedFormulaCases())
    expectWeightedFusedCandidateMatchesBlockAndStd(
        candidate, source, target, weights, 3e-1, 4e-5, 3e-1, 4e-5, 3e-3f);
}

// near-cancellation（近抵消）样本专门保护 d 公式树：目标点只沿 normal 做极小正负
// 位移，ATb 会互相抵消，能暴露 d-six-term-fma 和 d-displacement-fused 的舍入差异。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockFusedFormulaNearCancellationStressMatchesBlockAndStd)
{
  auto source = makeSurfaceCloud(31, 0.055f);
  auto target = source;
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    const float eps = sign * (2.0e-4f + 1.0e-5f * static_cast<float>(i % 7));
    target[i].x += eps * target[i].normal_x;
    target[i].y += eps * target[i].normal_y;
    target[i].z += eps * target[i].normal_z;
    weights.push_back(0.25f + 0.05f * static_cast<float>(i % 11));
  }

  for (const auto& candidate : weightedFusedFormulaCases())
    expectWeightedFusedCandidateMatchesBlockAndStd(
        candidate, source, target, weights, 6e-2, 3e-5, 8e-4, 4e-4, 5e-3f);
}

// scale-stress（尺度压力）样本覆盖大坐标和宽权重动态范围。它给 fused formula
// 只承诺预算内对齐，不要求和标量 row-order double 累加 bitwise 相同。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockFusedFormulaScaleStressMatchesBlockAndStd)
{
  auto source = makeSurfaceCloud(33, 0.21f);
  for (auto& point : source) {
    point.x *= 8.0f;
    point.y *= 8.0f;
    point.z *= 4.0f;
  }
  const auto target = makeTargetCloud(source);
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float scale = (i % 3 == 0) ? 0.015f : ((i % 3 == 1) ? 3.5f : 48.0f);
    weights.push_back(scale + 0.125f * static_cast<float>(i % 5));
  }

  for (const auto& candidate : weightedFusedFormulaCases())
    expectWeightedFusedCandidateMatchesBlockAndStd(
        candidate, source, target, weights, 5e4, 5e-5, 2e4, 5e-5, 1e-2f);
}

// 非有限语义样本：point/normal 非有限 lane 被 finite mask 剔除；weight 不参与 mask。
// 因此有限 point/normal + 非有限 weight 的 lane 仍应保留，并把 NaN/Inf 传播到法方程。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockFusedFormulaPreservesNonFiniteWeightSemantics)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_y = std::numeric_limits<float>::infinity();
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);

  for (const auto& candidate : weightedFusedFormulaCases()) {
    SCOPED_TRACE(candidate.name);
    diag::AccumulationStats candidate_stats;
    const diag::NormalEquation candidate_eq =
        candidate.accumulate(source, target, weights, &candidate_stats);
    EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
    EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
    EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
    EXPECT_TRUE(candidate_stats.used_rvv);
#else
    EXPECT_FALSE(candidate_stats.used_rvv);
#endif
    EXPECT_FALSE(std::isfinite(candidate_eq.ata.norm()));
  }
}

// 代表点型 correctness：`abc-fused` 和只重排独立 a/b/c 计算顺序的
// `abc-fused-ilp` 已经从 PointNormal 专用 helper 提升到 layout-gated generic
// test_support candidate。这里覆盖 production 当前使用的三类代表 source/target 组合。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudGenericAbcFusedRepresentativePointTypesMatchStd)
{
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target_normal = makeTargetCloud(source_normal);
  std::vector<float> weights = makeWeights(source_normal.size());
  source_normal[7].x = std::numeric_limits<float>::quiet_NaN();
  target_normal[19].normal_z = std::numeric_limits<float>::infinity();

  expectGenericAbcFusedCandidatesMatchStd("PointNormal -> PointNormal",
                                          source_normal,
                                          target_normal,
                                          weights,
                                          3e-1,
                                          4e-5,
                                          3e-1,
                                          4e-5,
                                          3e-3f);

  const auto source_xyz = copySourceAsXYZ(source_normal);
  expectGenericAbcFusedCandidatesMatchStd("PointXYZ -> PointNormal",
                                          source_xyz,
                                          target_normal,
                                          weights,
                                          3e-1,
                                          4e-5,
                                          3e-1,
                                          4e-5,
                                          3e-3f);

  const auto target_xyzinormal = copyTargetAsXYZINormal(target_normal);
  expectGenericAbcFusedCandidatesMatchStd("PointXYZ -> PointXYZINormal",
                                          source_xyz,
                                          target_xyzinormal,
                                          weights,
                                          3e-1,
                                          4e-5,
                                          3e-1,
                                          4e-5,
                                          3e-3f);
}

// 代表点型 correctness：D 类公式和 abcd 组合也必须先通过 generic
// layout-gated 对拍，再进入 production 接入讨论。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudGenericDAndAbcdFusedRepresentativePointTypesMatchStd)
{
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target_normal = makeTargetCloud(source_normal);
  std::vector<float> weights = makeWeights(source_normal.size());
  source_normal[7].x = std::numeric_limits<float>::quiet_NaN();
  target_normal[19].normal_z = std::numeric_limits<float>::infinity();

  expectGenericDAndAbcdFusedCandidatesMatchStd("PointNormal -> PointNormal",
                                               source_normal,
                                               target_normal,
                                               weights,
                                               3e-1,
                                               4e-5,
                                               3e-1,
                                               4e-5,
                                               3e-3f);

  const auto source_xyz = copySourceAsXYZ(source_normal);
  expectGenericDAndAbcdFusedCandidatesMatchStd("PointXYZ -> PointNormal",
                                               source_xyz,
                                               target_normal,
                                               weights,
                                               3e-1,
                                               4e-5,
                                               3e-1,
                                               4e-5,
                                               3e-3f);

  const auto target_xyzinormal = copyTargetAsXYZINormal(target_normal);
  expectGenericDAndAbcdFusedCandidatesMatchStd("PointXYZ -> PointXYZINormal",
                                               source_xyz,
                                               target_xyzinormal,
                                               weights,
                                               3e-1,
                                               4e-5,
                                               3e-1,
                                               4e-5,
                                               3e-3f);
}

TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionDefaultFusedAbcdIlpRepresentativePointTypesMatchStd)
{
#if defined(__RVV10__)
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target_normal = makeTargetCloud(source_normal);
  std::vector<float> weights = makeWeights(source_normal.size());
  source_normal[7].x = std::numeric_limits<float>::quiet_NaN();
  target_normal[19].normal_z = std::numeric_limits<float>::infinity();

  expectProductionDefaultFusedAbcdIlpMatchesStd("PointNormal -> PointNormal",
                                                source_normal,
                                                target_normal,
                                                weights,
                                                3e-1,
                                                4e-5,
                                                3e-1,
                                                4e-5,
                                                3e-3f);

  const auto source_xyz = copySourceAsXYZ(source_normal);
  expectProductionDefaultFusedAbcdIlpMatchesStd("PointXYZ -> PointNormal",
                                                source_xyz,
                                                target_normal,
                                                weights,
                                                3e-1,
                                                4e-5,
                                                3e-1,
                                                4e-5,
                                                3e-3f);

  const auto target_xyzinormal = copyTargetAsXYZINormal(target_normal);
  expectProductionDefaultFusedAbcdIlpMatchesStd("PointXYZ -> PointXYZINormal",
                                                source_xyz,
                                                target_xyzinormal,
                                                weights,
                                                3e-1,
                                                4e-5,
                                                3e-1,
                                                4e-5,
                                                3e-3f);
#else
  GTEST_SKIP() << "production default fused AbcdFusedIlp path is RVV-only";
#endif
}

