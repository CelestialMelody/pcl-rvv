/*
 * 本文件做什么：
 * production direct、layout gate 与 fallback 证据。
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

// production direct：真实 public full-cloud overload 使用连续 weights_，RVV build
// 应命中 full-cloud f32 AoS layout-gated block-reduction；std build 则自然保留标量路径。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPublicOverloadMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 2e-3f);
}

// production direct normal-equation：比只比较 matrix 更早捕获 block-reduction 的
// reduction tree、finite mask 或 weight 语义回归。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudNormalEquationMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 2e-1, 2e-5, 2e-1, 2e-5);
  expectMatrixNear(solveProductionEquation(candidate_eq),
                   solveProductionEquation(std_eq),
                   2e-3f);
}

// production numeric stress：尺度压力样本覆盖大 ATA/ATb 量级下的 accepted_points、
// normal-equation 和 public matrix 预算。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudScaleStressMatchesStdWithinBudget)
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

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 4e4, 3e-5, 1e4, 3e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 8e-3f);
}

// production non-finite semantics：point/normal 非有限 lane 被剔除；weight 非有限但
// point/normal 有限时仍参与计算并污染法方程，保持原标量合同。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPreservesNonFiniteWeightSemantics)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_y = std::numeric_limits<float>::infinity();
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(candidate_eq.ata.norm()));
}

// production fallback：小规模输入即使满足 PointNormal/float/full-cloud，也必须回到
// 原 iterator 标量路径，避免 dispatch 成本进入 tiny case。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto default_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, solveProductionEquation(default_eq), 1e-3f);
}

// production gate：size / weights / VL / byte-offset predicate 是显式窄门；不满足
// 任何一项都不能进入 RVV helper。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPredicateGatesAreNarrow)
{
  constexpr std::size_t kMaxRows =
      std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal);
  EXPECT_TRUE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
               pcl::PointXYZ,
               pcl::PointXYZINormal>(64, 64, 64, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(63, 63, 63, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 63, 64, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 63, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 64, 65)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(kMaxRows + 1, kMaxRows + 1, kMaxRows + 1, 64)));
}

// production direct：真实 source-indexed public overload 使用 source_indices[k]、
// target[k] 和 weights_[k]。RVV build 应命中 source gather + target stride helper。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedPublicOverloadMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStd(
          source, source_indices, target, weights, &std_stats);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 3e-3f);
}

// production direct normal-equation：比 public matrix 更早捕获 source index
// staging、gather、finite mask 或 weight 语义回归。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedNormalEquationMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStd(
          source, source_indices, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesDefault(
          source, source_indices, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 4e-1, 3e-5, 4e-1, 3e-5);
  expectMatrixNear(solveProductionEquation(candidate_eq),
                   solveProductionEquation(std_eq),
                   3e-3f);
}

#ifdef __RVV10__
// production default policy：Phase 032 的同边界 A/B 后，source-indexed
// 默认 RVV path 固定回 staged-gather。block-fused helper 仍保留给显式 probe /
// detail A/B，但不能再无意间回到 public 默认优先路径。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedDefaultUsesStagedGatherRVV)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  prod_detail::PointToPlaneLLSWeightedFullCloudStats default_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats staged_stats;
  const auto default_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesDefault(
          source, source_indices, target, weights, &default_stats);
  prod_detail::PointToPlaneLLSWeightedNormalEquation staged_eq;
  ASSERT_TRUE(prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
      source, source_indices, target, weights, staged_eq, &staged_stats));

  EXPECT_TRUE(default_stats.used_rvv);
  EXPECT_TRUE(staged_stats.used_rvv);
  EXPECT_EQ(default_stats.input_points, staged_stats.input_points);
  EXPECT_EQ(default_stats.accepted_points, staged_stats.accepted_points);
  expectProductionEquationWithinBudget(
      default_eq, staged_eq, 0.0, 0.0, 0.0, 0.0);
}
#endif

// production source-indexed fallback：小规模输入不进入 indexed RVV helper，保持原
// iterator 标量路径。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto default_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesDefault(
          source, source_indices, target, weights, &stats);
  EXPECT_FALSE(stats.used_rvv);
  expectMatrixNear(public_matrix, solveProductionEquation(default_eq), 1e-3f);
}

// production source-indexed gate：source cloud size、index count、target count、
// weights、VLEN 和 byte-offset predicate 必须全部满足。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedPredicateGatesAreNarrow)
{
  constexpr std::size_t kMaxRows =
      std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal);
  EXPECT_TRUE((prod_detail::canUsePointToPlaneLLSWeightedSourceIndicesRVV<
               pcl::PointXYZ,
               pcl::PointXYZINormal>(64, 64, 64, 64, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedSourceIndicesRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 63, 63, 63, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedSourceIndicesRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 63, 64, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedSourceIndicesRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 64, 63, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedSourceIndicesRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 64, 64, 65)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedSourceIndicesRVV<
                pcl::PointNormal,
                pcl::PointNormal>(kMaxRows + 1, 64, 64, 64, 64)));
}

#ifdef __RVV10__
// production source-indexed gate：非法 source index 不能进入 gather。这里不声明
// public API 的非法 index 合同，只保护 RVV helper 的 pre-gather gate。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedInvalidIndexRejectsRVVBeforeGather)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  auto negative_index = makeSourceIndices(source.size());
  negative_index[0] = -1;
  prod_detail::PointToPlaneLLSWeightedNormalEquation eq;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats stats;
  EXPECT_FALSE(
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(
          source, negative_index, target, weights, eq, &stats));
  EXPECT_EQ(stats.input_points, negative_index.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_FALSE(stats.used_rvv);

  stats = prod_detail::PointToPlaneLLSWeightedFullCloudStats{};
  EXPECT_FALSE(prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
      source, negative_index, target, weights, eq, &stats));
  EXPECT_EQ(stats.input_points, negative_index.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_FALSE(stats.used_rvv);

  auto out_of_range_index = makeSourceIndices(source.size());
  out_of_range_index[1] = static_cast<int>(source.size());
  stats = prod_detail::PointToPlaneLLSWeightedFullCloudStats{};
  EXPECT_FALSE(
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(
          source, out_of_range_index, target, weights, eq, &stats));
  EXPECT_EQ(stats.input_points, out_of_range_index.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_FALSE(stats.used_rvv);

  stats = prod_detail::PointToPlaneLLSWeightedFullCloudStats{};
  EXPECT_FALSE(prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
      source, out_of_range_index, target, weights, eq, &stats));
  EXPECT_EQ(stats.input_points, out_of_range_index.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_FALSE(stats.used_rvv);
}
#endif

// production source-indexed generic source：PointXYZ source 只提供 xyz 字段，
// target normal 仍来自 PointNormal，indices 仍按 source cloud 下标解释。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedPointXYZSourceMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(32, 0.10f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = makeTargetCloud(source_normal);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStd(
          source, source_indices, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesDefault(
          source, source_indices, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 4e-1, 3e-5, 4e-1, 3e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 3e-3f);
}

// production source-indexed generic target：target 使用 PointXYZINormal 时，
// source index stream 仍只选择 source 行，额外 intensity 字段不参与公式。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionSourceIndexedPointXYZToPointXYZINormalMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(32, 0.10f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(makeTargetCloud(source_normal));
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      pcl::PointXYZINormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesStd(
          source, source_indices, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedSourceIndicesDefault(
          source, source_indices, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 4e-1, 3e-5, 4e-1, 3e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 3e-3f);
}

// production generic source：PointXYZ source 只提供 xyz 字段，target normal 仍来自
// PointNormal。这个 public overload 应命中泛型 source layout gate。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPointXYZSourceMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = makeTargetCloud(source_normal);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 2e-1, 2e-5, 2e-1, 2e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 2e-3f);
}

// production generic target：PointXYZINormal target 证明 target gate 需要 xyz+normal
// f32 AoS layout，不要求 exact PointNormal；额外 intensity 字段不参与 weighted 公式。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPointXYZToPointXYZINormalMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(makeTargetCloud(source_normal));
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      pcl::PointXYZINormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 2e-1, 2e-5, 2e-1, 2e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 2e-3f);
}

// production target layout fallback：这个 target 已注册 xyz/normal fields，但 normal
// 字段是 double，不满足 f32 normal layout，因此 RVV helper 必须拒绝并回到标量。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudDoubleNormalTargetFallsBackToScalar)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsDoubleNormal(makeTargetCloud(source_normal));
  const std::vector<float> weights = makeWeights(source.size());
  static_assert(!pcl::rvv::RVVXYZNormalFloatLayout<TEPTPLWDoubleNormalTarget>::value);

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      TEPTPLWDoubleNormalTarget>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto default_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &stats);
  EXPECT_FALSE(stats.used_rvv);
  expectMatrixNear(public_matrix, solveProductionEquation(default_eq), 1e-6f);
}

// production Scalar gate：输出 Scalar=double 不在本轮 RVV 范围内，即使点类型满足
// f32 AoS layout gate，也必须保留原标量路径。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudScalarDoubleFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> float_weights = makeWeights(source.size());
  std::vector<double> weights;
  weights.reserve(float_weights.size());
  for (const float weight : float_weights)
    weights.push_back(static_cast<double>(weight));

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal,
      double>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);
  EXPECT_TRUE(public_matrix.allFinite());
}
