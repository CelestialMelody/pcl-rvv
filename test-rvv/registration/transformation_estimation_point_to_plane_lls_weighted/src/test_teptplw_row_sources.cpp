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

