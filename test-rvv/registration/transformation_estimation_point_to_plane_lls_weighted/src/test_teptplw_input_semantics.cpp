/*
 * 本文件做什么：
 * public input semantics。这里覆盖公开入口对数量不匹配、0 权重和负权重的行为。
 * 非法 index / correspondence 不在本文测试。公开 iterator 没有声明 defensive skip 合同。
 */

#include "test_teptplw.h"

#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <gtest/gtest.h>

#include <vector>

namespace prod_detail = pcl::registration::detail;
using namespace pcl::registration::rvv_te_pt2plane_lls_weighted_test;

namespace {

Eigen::Matrix4f
makeSentinelMatrix()
{
  Eigen::Matrix4f matrix;
  matrix << 1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f;
  return matrix;
}

} // namespace

// public full-cloud 输入语义：source/target 数量不一致时，公开入口打印错误并返回。
// 该路径不进入 RVV，也不进入标量 solver。输出矩阵应保持调用前状态。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     PublicFullCloudTargetSizeMismatchKeepsOutputMatrix)
{
  const auto source = makeSurfaceCloud(4, 0.18f);
  auto target = makeTargetCloud(source);
  target.points.pop_back();
  target.width = target.size();
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  const Eigen::Matrix4f expected = makeSentinelMatrix();
  Eigen::Matrix4f actual = expected;
  estimator.estimateRigidTransformation(source, target, actual);

  expectMatrixNear(actual, expected, 0.0f);
}

// public full-cloud 输入语义：weights_ 数量不等于 source 点数时，公开入口打印错误并返回。
// 该检查发生在 full-cloud RVV dispatch 之前。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     PublicFullCloudWeightSizeMismatchKeepsOutputMatrix)
{
  const auto source = makeSurfaceCloud(4, 0.18f);
  const auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  weights.pop_back();

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  const Eigen::Matrix4f expected = makeSentinelMatrix();
  Eigen::Matrix4f actual = expected;
  estimator.estimateRigidTransformation(source, target, actual);

  expectMatrixNear(actual, expected, 0.0f);
}

// public source-indexed 输入语义：target 点数必须等于 source index stream 长度。
// 数量不匹配时，公开入口返回，输出矩阵保持不变。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     PublicSourceIndexedTargetSizeMismatchKeepsOutputMatrix)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  auto target = makeTargetCloud(source);
  target.points.pop_back();
  target.width = target.size();
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  const Eigen::Matrix4f expected = makeSentinelMatrix();
  Eigen::Matrix4f actual = expected;
  estimator.estimateRigidTransformation(source, source_indices, target, actual);

  expectMatrixNear(actual, expected, 0.0f);
}

// public source-indexed 输入语义：weights_ 数量必须等于 source index stream 长度。
// 数量不匹配时，公开入口返回，输出矩阵保持不变。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     PublicSourceIndexedWeightSizeMismatchKeepsOutputMatrix)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  std::vector<float> weights = makeWeights(source_indices.size());
  weights.pop_back();

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  const Eigen::Matrix4f expected = makeSentinelMatrix();
  Eigen::Matrix4f actual = expected;
  estimator.estimateRigidTransformation(source, source_indices, target, actual);

  expectMatrixNear(actual, expected, 0.0f);
}

// public dual-indices 输入语义：source index stream 和 target index stream 长度必须一致。
// 长度不一致时，公开入口返回，输出矩阵保持不变。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     PublicDualIndicesTargetIndexSizeMismatchKeepsOutputMatrix)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  pcl::Indices target_indices = makeTargetIndices(target.size());
  target_indices.pop_back();
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  const Eigen::Matrix4f expected = makeSentinelMatrix();
  Eigen::Matrix4f actual = expected;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, actual);

  expectMatrixNear(actual, expected, 0.0f);
}

// public dual-indices 输入语义：weights_ 数量必须等于 source index stream 长度。
// 两条 index stream 长度一致但 weights_ 长度不一致时，输出矩阵保持不变。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     PublicDualIndicesWeightSizeMismatchKeepsOutputMatrix)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(target.size());
  std::vector<float> weights = makeWeights(source_indices.size());
  weights.pop_back();

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  const Eigen::Matrix4f expected = makeSentinelMatrix();
  Eigen::Matrix4f actual = expected;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, actual);

  expectMatrixNear(actual, expected, 0.0f);
}

// 0 权重是有效输入。含 0 权重的行仍会走同一个 full-cloud public path，
// 其 normal-equation 贡献为 0，并与 production std reference 对齐。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudZeroWeightsMatchStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  for (std::size_t i = 0; i < weights.size(); i += 7)
    weights[i] = 0.0f;

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

// 负权重是有效输入。production 不拒绝负权重，也不把它作为 finite mask 条件。
// 该测试只声明与标量公式一致，不声明业务层是否应该传入负权重。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudNegativeWeightsMatchStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  for (std::size_t i = 3; i < weights.size(); i += 11)
    weights[i] = -weights[i];

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
