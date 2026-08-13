/*
 * 本文件做什么：
 * 公开 estimator 与 test-rvv 标量诊断链路的 public semantics（公开入口语义）对拍。
 * 共享 fixtures/assertions 来自 test_teptpl.h，TEST body 保持原 case 名和证据语义。
 */

#include "test_teptpl.h"

using namespace pcl::registration::rvv_te_pt2plane_lls_test;
namespace support = pcl::registration::rvv_te_pt2plane_lls_support;
namespace prod_detail = pcl::registration::detail;

// 这个测试验证 test-rvv 的标量诊断入口是否复刻当前公开 estimator 的输出。
// RVV 构建中的 full-cloud public overload 可能命中 production block-reduction
// dispatch，因此这里按 reduction-tree 变化后的 production 预算对拍。
TEST(TransformationEstimationPointToPlaneLLS, StdDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(14, 0.22f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_full(source, target, &stats);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
#ifdef __RVV10__
  expectMatrixNear(diagnostic_matrix, public_matrix, 5e-4f);
#else
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
#endif
}

// 这个测试重建 source indices + target full-cloud 公开 overload 的数据流：source 通过
// indices 取点，target 是按 row 顺序压好的紧凑全云。如果失败，说明公开入口和诊断
// reference 的 row 枚举语义已经不一致。
TEST(TransformationEstimationPointToPlaneLLS,
     StdSourceIndicesDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target_full = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const auto target = copyIndexedCloud(target_full, source_indices);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, source_indices, target, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_source_indices(source, source_indices, target, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}

// 这个测试重建 source+target indices 公开 overload 的数据流。target 使用独立 index
// stream，证明 dual-indices diagnostic 不是把同一组 indices 复制到两侧。
TEST(TransformationEstimationPointToPlaneLLS,
     StdDualIndicesDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target = makeTargetCloud(source);
  pcl::Indices source_indices = makeIndexedRows(source.size());
  pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const std::size_t paired_rows = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(paired_rows);
  target_indices.resize(paired_rows);
  EXPECT_NE(source_indices, target_indices);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_dual_indices(source, source_indices, target, target_indices, &stats);

  EXPECT_EQ(stats.input_points, paired_rows);
  EXPECT_EQ(stats.accepted_points, paired_rows);
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}

// 这个测试重建 correspondences 公开 overload 的数据流：row 来自 index_query/index_match。
// 它只证明有效 query/match 输入下的公开入口一致性，不定义非法 index 行为。
TEST(TransformationEstimationPointToPlaneLLS,
     StdCorrespondencesDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeShuffledCorrespondences(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  support::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}
