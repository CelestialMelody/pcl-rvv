/*
 * 本文件做什么：
 * public semantics 与公开入口标量语义。
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

// 这个测试验证 test-rvv 标量诊断是否复刻公开全云带权估计入口的 fallback 标量语义。
// 大规模 full-cloud public overload 在 RVV build 可能命中 production 分流，所以这里用
// n < 64 的小样本专门保护 scalar reference（标量参考链路）。
TEST(TransformationEstimationPointToPlaneLLSWeighted, StdDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(3, 0.22f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_full(source, target, weights, &stats);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试验证公开对应关系入口（correspondences）直接使用 correspondence.weight 的语义。
// 它证明权重来源和全云 setCorrespondenceWeights 路径不同，不能混为一个入口。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     StdCorrespondencesMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeWeightedCorrespondences(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_correspondences(source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试闭合 source-indexed（源索引路径）的公开入口：权重来自
// setCorrespondenceWeights，row k 使用 source[indices[k]] 和 target[k]。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     StdSourceIndexedMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, source_indices, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_source_indices(source, source_indices, target, weights, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试闭合 dual-indices（双索引路径）的公开入口：两条 index stream 决定取点，
// 但连续 weights[k] 仍按 row 序号推进。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     StdDualIndicesMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(target.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix = diag::estimate_std_dual_indices(
      source, source_indices, target, target_indices, weights, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

