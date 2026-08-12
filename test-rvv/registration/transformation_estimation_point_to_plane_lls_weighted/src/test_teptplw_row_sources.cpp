/*
 * 本文件做什么：
 * row source、fallback gate 与有限性语义。
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

// 这个测试验证 source-indexed（源索引路径）：第 k 行来自 source[indices[k]]、
// target[k] 和 weights[k]。它把单侧 gather 与连续权重读取分开审查。
TEST(TransformationEstimationPointToPlaneLLSWeighted, SourceIndexedCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_source_indices(source, source_indices, target, weights, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_source_indices(
      source, source_indices, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// 这个测试把 source-indexed 当前 staged-gather / compressed-tail 和新补的
// block-reduction baseline 放到同一个 valid-index-only 语义边界里对拍。它只证明
// test-rvv candidate 的正确性，不代表生产已经替换实现族。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     SourceIndexedBlockReductionMatchesStdWithinBudget)
{
  auto source = makeSurfaceCloud(32, 0.10f);
  auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());
  source[7].x = std::numeric_limits<float>::quiet_NaN();
  target[19].normal_z = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats staged_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_source_indices(
          source, source_indices, target, weights, &std_stats);
  const diag::NormalEquation staged_eq =
      diag::accumulate_candidate_source_indices(
          source, source_indices, target, weights, &staged_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_source_indices_block_reduction(
          source, source_indices, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(staged_stats.used_rvv);
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(staged_stats.used_rvv);
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_NEAR((staged_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((staged_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  expectNormalEquationWithinBudget(block_eq, std_eq, 3e-1, 5e-5, 3e-1, 5e-5);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   4e-3f);
}

// 这个测试审计 source-indexed 是否能迁移 full-cloud 当前 adopted 的
// fused-abcd-ilp code shape。它仍是 pre-production diagnostic，失败时说明公式 / ILP
// 形态不能直接从 full-cloud 外推。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     SourceIndexedBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget)
{
  auto source = makeSurfaceCloud(31, 0.055f);
  auto target = source;
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  std::vector<float> weights;
  weights.reserve(source_indices.size());
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    const float eps = sign * (2.0e-4f + 1.0e-5f * static_cast<float>(i % 7));
    target[i].x += eps * target[i].normal_x;
    target[i].y += eps * target[i].normal_y;
    target[i].z += eps * target[i].normal_z;
    weights.push_back(0.25f + 0.05f * static_cast<float>(i % 11));
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  diag::AccumulationStats fused_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_source_indices(
          source, source_indices, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_source_indices_block_reduction(
          source, source_indices, target, weights, &block_stats);
  const diag::NormalEquation fused_eq =
      diag::accumulate_candidate_source_indices_block_fused_abcd_ilp(
          source, source_indices, target, weights, &fused_stats);

  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(fused_stats.accepted_points, block_stats.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, block_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(fused_eq, std_eq, 6e-2, 5e-5, 8e-4, 6e-4);
  expectNormalEquationWithinBudget(fused_eq, block_eq, 6e-2, 5e-5, 8e-4, 6e-4);
  expectMatrixNear(diag::solve_normal_equation(fused_eq),
                   diag::solve_normal_equation(std_eq),
                   5e-3f);
  expectMatrixNear(diag::solve_normal_equation(fused_eq),
                   diag::solve_normal_equation(block_eq),
                   5e-3f);
}

// source-indexed block/fused family 也必须保持 production 的 weight finite 语义：
// 只检查点和 normal 是否有限；有限点上的 NaN/Inf weight 不会被 mask 掉。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     SourceIndexedBlockFamilyPreservesNonFiniteWeightSemantics)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  std::vector<float> weights = makeWeights(source_indices.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_y = std::numeric_limits<float>::infinity();
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  diag::AccumulationStats fused_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_source_indices(
          source, source_indices, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_source_indices_block_reduction(
          source, source_indices, target, weights, &block_stats);
  const diag::NormalEquation fused_eq =
      diag::accumulate_candidate_source_indices_block_fused_abcd_ilp(
          source, source_indices, target, weights, &fused_stats);

  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(block_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(fused_eq.ata.norm()));
}

// 这个测试验证 dual-indices（双索引路径）：source 和 target 都来自独立 index stream，
// weight 仍按 row 序号读取。它不能替代 correspondences 的 query/match/weight 展开证据。
TEST(TransformationEstimationPointToPlaneLLSWeighted, DualIndicesCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(34, 0.095f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = diag::estimate_std_dual_indices(
      source, source_indices, target, target_indices, weights, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_dual_indices(
      source, source_indices, target, target_indices, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 6e-4f);
}

// dual-indices 的 block-baseline carry-over（实现族迁移候选）使用 source/target
// 双侧 gather，但 shared math pipeline 保持 full-cloud adopted 的 A/B/C/N block
// groups。这个测试只证明 test-rvv candidate 的同边界正确性。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     DualIndicesBlockReductionMatchesStdWithinBudget)
{
  auto source = makeSurfaceCloud(32, 0.10f);
  auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());
  source[7].x = std::numeric_limits<float>::quiet_NaN();
  target[19].normal_z = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats staged_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq = diag::accumulate_std_dual_indices(
      source, source_indices, target, target_indices, weights, &std_stats);
  const diag::NormalEquation staged_eq = diag::accumulate_candidate_dual_indices(
      source, source_indices, target, target_indices, weights, &staged_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_dual_indices_block_reduction(
          source, source_indices, target, target_indices, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(staged_stats.used_rvv);
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(staged_stats.used_rvv);
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_NEAR((staged_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((staged_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  expectNormalEquationWithinBudget(block_eq, std_eq, 5e-1, 5e-5, 5e-1, 5e-5);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   6e-3f);
}

// 这个测试把 full-cloud adopted 的 fused-abcd-ilp 公式形态迁移到 dual-indices
// 的双 gather 边界内。若失败，说明公式 / ILP 形态不能只凭 full-cloud 结论外推。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     DualIndicesBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget)
{
  auto source = makeSurfaceCloud(31, 0.055f);
  auto target = source;
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(source.size());
  std::vector<float> weights;
  weights.reserve(source_indices.size());
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto target_index = static_cast<std::size_t>(target_indices[i]);
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    const float eps = sign * (2.5e-4f + 1.0e-5f * static_cast<float>(i % 5));
    target[target_index].x += eps * target[target_index].normal_x;
    target[target_index].y += eps * target[target_index].normal_y;
    target[target_index].z += eps * target[target_index].normal_z;
    weights.push_back(0.30f + 0.04f * static_cast<float>(i % 13));
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  diag::AccumulationStats fused_stats;
  const diag::NormalEquation std_eq = diag::accumulate_std_dual_indices(
      source, source_indices, target, target_indices, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_dual_indices_block_reduction(
          source, source_indices, target, target_indices, weights, &block_stats);
  const diag::NormalEquation fused_eq =
      diag::accumulate_candidate_dual_indices_block_fused_abcd_ilp(
          source, source_indices, target, target_indices, weights, &fused_stats);

  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(fused_stats.accepted_points, block_stats.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, block_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(fused_eq, std_eq, 2e-1, 5e-5, 2e-3, 1e-3);
  expectNormalEquationWithinBudget(fused_eq, block_eq, 2e-1, 5e-5, 2e-3, 1e-3);
  expectMatrixNear(diag::solve_normal_equation(fused_eq),
                   diag::solve_normal_equation(std_eq),
                   7e-3f);
  expectMatrixNear(diag::solve_normal_equation(fused_eq),
                   diag::solve_normal_equation(block_eq),
                   7e-3f);
}

// 这个测试验证对应关系路径（correspondences）的乱序、重复 index 和 weight 字段。
// 如果失败，说明 gather（离散加载）、权重展开或 vcompress 后保序尾段的证据断了。
TEST(TransformationEstimationPointToPlaneLLSWeighted, CorrespondenceCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeWeightedCorrespondences(source.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 4e-4f);
}

// correspondences 的 block-baseline candidate 先标量展开 query/match/weight，再走
// index-pair block family。展开成本属于本诊断计时边界，但这里先只锁 correctness。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     CorrespondenceBlockReductionMatchesStdWithinBudget)
{
  auto source = makeSurfaceCloud(32, 0.10f);
  auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeWeightedCorrespondences(source.size());
  source[4].x = std::numeric_limits<float>::quiet_NaN();
  target[6].normal_z = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats staged_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_correspondences(source, target, correspondences, &std_stats);
  const diag::NormalEquation staged_eq =
      diag::accumulate_candidate_correspondences(
          source, target, correspondences, &staged_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_correspondences_block_reduction(
          source, target, correspondences, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(staged_stats.used_rvv);
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(staged_stats.used_rvv);
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_NEAR((staged_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((staged_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  expectNormalEquationWithinBudget(block_eq, std_eq, 5e-1, 5e-5, 5e-1, 5e-5);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   6e-3f);
}

// correspondences fused-abcd-ilp 同时覆盖 query/match 展开、correspondence.weight
// 和双侧 gather。它仍是 pre-production diagnostic，不能替代真实 public overload。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     CorrespondenceBlockFusedAbcdIlpMatchesBlockAndStdWithinBudget)
{
  auto source = makeSurfaceCloud(31, 0.055f);
  auto target = source;
  const pcl::Correspondences correspondences = makeWeightedCorrespondences(source.size());
  for (std::size_t i = 0; i < target.size(); ++i) {
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    const float eps = sign * (2.0e-4f + 1.5e-5f * static_cast<float>(i % 7));
    target[i].x += eps * target[i].normal_x;
    target[i].y += eps * target[i].normal_y;
    target[i].z += eps * target[i].normal_z;
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  diag::AccumulationStats fused_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_correspondences(source, target, correspondences, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_correspondences_block_reduction(
          source, target, correspondences, &block_stats);
  const diag::NormalEquation fused_eq =
      diag::accumulate_candidate_correspondences_block_fused_abcd_ilp(
          source, target, correspondences, &fused_stats);

  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(fused_stats.accepted_points, block_stats.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, block_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(fused_eq, std_eq, 2e-1, 5e-5, 2e-3, 1e-3);
  expectNormalEquationWithinBudget(fused_eq, block_eq, 2e-1, 5e-5, 2e-3, 1e-3);
  expectMatrixNear(diag::solve_normal_equation(fused_eq),
                   diag::solve_normal_equation(std_eq),
                   7e-3f);
  expectMatrixNear(diag::solve_normal_equation(fused_eq),
                   diag::solve_normal_equation(block_eq),
                   7e-3f);
}

// 这个测试单独隔离小规模 fallback（回退路径）gate：输入太小时必须回到标量链路。
// 如果失败，说明规模阈值或 fallback normal-equation 等价性断了。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     SmallInputFallsBackForIsolatedSizeGate)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  diag::AccumulationStats stats;
  const diag::NormalEquation candidate =
      diag::accumulate_candidate_full(source, target, weights, &stats);
  const diag::NormalEquation reference = diag::accumulate_std_full(source, target, weights);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(candidate.accepted_points, reference.accepted_points);
  EXPECT_NEAR((candidate.ata - reference.ata).norm(), 0.0, 1e-12);
  EXPECT_NEAR((candidate.atb - reference.atb).norm(), 0.0, 1e-12);
}

// 这个测试验证 NaN/Inf invalid lane（无效 lane）会被 finite mask（有限值掩码）剔除。
// weight 本身不参与当前 production finite check，所以本 case 只放有限权重。
TEST(TransformationEstimationPointToPlaneLLSWeighted, InvalidLaneMaskMatchesStd)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation candidate_eq =
      diag::accumulate_candidate_full(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}

// 这个测试锁住 weight finite semantics（权重有限性语义）：production 只检查点和
// normal 是否有限，不检查 weight。非有限权重应参与计算并污染法方程，而不是被 mask 跳过。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     NonFiniteWeightsAreNotMaskedWhenPointsAreFinite)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation candidate_eq =
      diag::accumulate_candidate_full(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(candidate_eq.ata.norm()));
}
