/*
 * 本文件做什么：
 * production direct、layout gate、fallback 和 fused production path 的 correctness 证据。
 * 共享 fixtures/assertions 来自 test_teptpl.h，TEST body 保持原 case 名和证据语义。
 */

#include "test_teptpl.h"

using namespace pcl::registration::rvv_te_pt2plane_lls_test;
namespace support = pcl::registration::rvv_te_pt2plane_lls_support;
namespace prod_detail = pcl::registration::detail;

// Production dispatch check: std/RVV builds both call the same public full-cloud
// overload. In the RVV build this should hit the exact PointNormal,float gate;
// the tolerance acknowledges the changed RVV reduction tree.
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudPublicOverloadMatrixMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats std_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const Eigen::Matrix4f std_matrix = solveTestEquation(std_eq);

  EXPECT_EQ(std_stats.input_points, source.size());
  EXPECT_EQ(std_stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, std_matrix, 5e-4f);
}

// Production direct normal-equation check: RVV build 用 production RVV helper 构造
// actual，expected 来自 test-only scalar reference。它比只看 matrix 更早捕获
// reduction-tree 或 mask 回归，但不把 production detail 标量 helper 当 reference。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudNormalEquationMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(solveTestEquation(candidate_eq), solveTestEquation(std_eq), 5e-4f);
}

// Production direct invalid-lane check: public overload、production RVV helper 和
// test-only scalar reference 必须对 NaN/Inf 行作同样剔除。它不覆盖
// indexed/correspondences 的 iterator 入口。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudInvalidLanesMatchStdWithinBudget)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 5e-4f);
}

// Production direct scale-stress check: 复核 full-cloud dispatch 在较大数值范围下仍落在
// production 误差预算内。失败时优先审查 block-reduction 的加法树与求解矩阵。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudScaleStressMatchesStdWithinBudget)
{
  const auto source = makeScaleStressCloud();
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 5e-4f);
}

// Generic production direct check: source 侧换成 PointXYZ，证明 RVV gate 只要求 source
// xyz 的 f32 AoS layout，不再要求 exact PointNormal。invalid lane 仍必须按同一 finite
// mask 剔除，并保持 accepted_points、ATA/ATb 和 matrix 预算。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudGenericPointXYZInvalidLanesMatchStdWithinBudget)
{
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source_normal);
  auto source = copySourceAsXYZ(source_normal);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 5e-4f);
}

// Generic public-overload matrix check: PointXYZ source + PointNormal target 是原模板标量
// 合同可编译的常见组合。本测试证明 full-cloud public overload 在泛型 RVV gate 下仍与
// 标量 normal-equation reference 对齐；QEMU correctness 不代表板卡性能结论。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudGenericPointXYZScaleStressMatchesStdWithinBudget)
{
  const auto source_normal = makeScaleStressCloud();
  const auto target = makeTargetCloud(source_normal);
  const auto source = copySourceAsXYZ(source_normal);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 5e-4f);
}

// Generic target smoke: target 使用 PointXYZINormal，证明 target gate 需要 xyz+normal
// f32 AoS layout，而不是 exact PointNormal。这个 case 仍只覆盖 full-cloud/Scalar=float。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudGenericTargetXYZINormalMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto target_normal = makeTargetCloud(source_normal);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(target_normal);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                             pcl::PointXYZINormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const auto candidate_eq =
      buildProductionDefaultEquation(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationNear(candidate_eq, std_eq);
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 5e-4f);
}

// fused production path 的 PointNormal 代表样本：用 near-cancellation + scale-stress
// 形态覆盖 a/b/c/d 的逐点公式树、accepted_points、ATA/ATb 和矩阵预算。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudPointNormalNearCancellationMatchesStdWithinBudget)
{
  const auto [source, target] = makeNearCancellationCloudPair();
  support::AccumulationStats std_stats;
  support::AccumulationStats fused_stats;
  const auto std_eq = support::accumulate_std_full(source, target, &std_stats);
  const auto fused_eq =
      buildProductionDefaultEquation(source, target, &fused_stats);

  EXPECT_EQ(std_stats.input_points, source.size());
  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(solveTestEquation(fused_eq), solveTestEquation(std_eq), 8e-4f);
}

// fused production path 的 generic source 样本：PointXYZ source + PointNormal target。
// invalid lane 必须继续用同一 finite mask 剔除，不能改变 accepted_points 或 matrix。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudGenericPointXYZInvalidLanesMatchStdWithinBudget)
{
  auto source_normal = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source_normal);
  auto source = copySourceAsXYZ(source_normal);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats fused_stats;
  const auto std_eq = support::accumulate_std_full(source, target, &std_stats);
  const auto fused_eq =
      buildProductionDefaultEquation(source, target, &fused_stats);

  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(solveTestEquation(fused_eq), solveTestEquation(std_eq), 5e-4f);
}

// fused production path 的 generic target 样本：PointXYZINormal target 证明 target gate 仍
// 只要求 xyz+normal 的 f32 AoS 语义，不要求 exact PointNormal。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudPointXYZToPointXYZINormalScaleStressMatchesStdWithinBudget)
{
  const auto source_normal = makeScaleStressCloud();
  const auto target_normal = makeTargetCloud(source_normal);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(target_normal);

  support::AccumulationStats std_stats;
  support::AccumulationStats fused_stats;
  const auto std_eq = support::accumulate_std_full(source, target, &std_stats);
  const auto fused_eq =
      buildProductionDefaultEquation(source, target, &fused_stats);

  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(solveTestEquation(fused_eq), solveTestEquation(std_eq), 5e-4f);
}

// fused production path 的 fallback smoke：小规模输入必须继续走标量。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.2f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats stats;
#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation rvv_eq;
  prod_detail::PointToPlaneLLSFullCloudStats rvv_stats;
  EXPECT_FALSE(prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
      source, target, rvv_eq, &rvv_stats));
  EXPECT_FALSE(rvv_stats.used_rvv);
#endif
  const auto std_eq = support::accumulate_std_full(source, target, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 1e-3f);
}

// fused production path 的 fallback smoke：layout gate 失败时也必须回到标量。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFusedFullCloudLayoutGateFailureFallsBackToScalar)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source_normal);
  const auto source = copySourceAsDoubleXYZ(source_normal);
  static_assert(!pcl::rvv::RVVXYZAoSFloatLayout<TEPTPLDoubleXYZPoint>::value);

  pcl::registration::TransformationEstimationPointToPlaneLLS<TEPTPLDoubleXYZPoint,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats fused_stats;
  support::AccumulationStats std_stats;
  const auto fused_eq = buildProductionDefaultEquation(source, target, &fused_stats);
  const auto std_eq =
      support::accumulate_std_full(source, target, &std_stats);

#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation rvv_eq;
  prod_detail::PointToPlaneLLSFullCloudStats rvv_stats;
  EXPECT_FALSE(prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
      source, target, rvv_eq, &rvv_stats));
  EXPECT_FALSE(rvv_stats.used_rvv);
#endif
  EXPECT_FALSE(fused_stats.used_rvv);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  expectProductionEquationNear(fused_eq, std_eq);
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 5e-4f);
}

// Production fallback check: n < 64 时即使是 PointNormal/float/full-cloud 也不得进入
// RVV block path。它保护小规模输入的标量语义和 dispatch 成本边界。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.2f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats stats;
#ifdef __RVV10__
  prod_detail::PointToPlaneLLSNormalEquation rvv_eq;
  prod_detail::PointToPlaneLLSFullCloudStats rvv_stats;
  EXPECT_FALSE(
      prod_detail::buildPointToPlaneLLSFullCloudBlockRVVFusedFormula(
          source, target, rvv_eq, &rvv_stats));
  EXPECT_FALSE(rvv_stats.used_rvv);
#endif
  const auto std_eq =
      support::accumulate_std_full(source, target, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, solveTestEquation(std_eq), 1e-3f);
}

// Production fallback smoke: Scalar=double 仍不在本轮 RVV 范围内，必须继续走原标量
// public overload。点字段 float32 和输出 Scalar=float 是两条不同边界。
TEST(TransformationEstimationPointToPlaneLLS,
     ProductionFullCloudScalarDoubleFallbackSmoke)
{
  const auto source_normal = makeSurfaceCloud(4, 0.2f);
  const auto target = makeTargetCloud(source_normal);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal,
                                                             double>
      double_estimator;
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();
  double_estimator.estimateRigidTransformation(source_normal, target, double_matrix);
  EXPECT_TRUE(double_matrix.allFinite());
#ifdef __RVV10__
  Eigen::Matrix4d rvv_matrix = Eigen::Matrix4d::Identity();
  EXPECT_FALSE(prod_detail::estimatePointToPlaneLLSFullCloudRVV(
      source_normal, target, rvv_matrix));
#endif
}
