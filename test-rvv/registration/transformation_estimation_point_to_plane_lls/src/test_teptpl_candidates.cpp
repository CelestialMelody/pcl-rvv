/*
 * 本文件做什么：
 * full-cloud RVV candidate、reduction 变体和 public-entry-shaped diagnostic 的 correctness 对拍。
 * 共享 fixtures/assertions 来自 test_teptpl.h，TEST body 保持原 case 名和证据语义。
 */

#include "test_teptpl.h"

using namespace pcl::registration::rvv_te_pt2plane_lls_test;
namespace support = pcl::registration::rvv_te_pt2plane_lls_support;
namespace prod_detail = pcl::registration::detail;

// 这个测试验证连续 PointNormal 全云输入下，RVV candidate（RVV 候选链路）与标量参考链路保持一致。
// 如果 RVV 构建没有命中 staging 或矩阵超差，说明 full-cloud 主路径证据或数值 gate 断了。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimate_std_full(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_full(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

// fused-reduction diagnostic 不走 vcompress + buffer + scalar lane tail；它改变跨 lane
// 累加树，因此使用独立误差预算，只用于判断后续优化方向是否值得继续。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudFusedReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_fused_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// grouped-reduction diagnostic 用 chunk-local vector reduction 写回标量 normal-equation，
// 目的是降低 27 个 accumulator 长期活跃带来的寄存器压力；它仍改变加法树。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudGroupedReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_grouped_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// grouped invalid-lane 诊断保护 finite mask 进入 reduction-tree 前清零的语义。
// 它解释历史 grouped 方案为何数值上可控，但不能证明 production dispatch 或性能。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudGroupedReductionInvalidLanesWithinBudget)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_grouped_reduction(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}

// scale-stress 诊断给 grouped reduction 一个高动态范围样本，避免只在温和曲面上对拍。
// grouped 已因板卡证据暂缓；这个测试保留为历史方案回归，不支撑当前 production 范围。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudGroupedReductionScaleStressWithinBudget)
{
  const auto source = makeScaleStressCloud();
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_grouped_reduction(source, target, &candidate_stats);

  const double ata_budget = std::max(1.0, std_eq.ata.norm() * 1e-4);
  const double atb_budget = std::max(1.0, std_eq.atb.norm() * 1e-4);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_LE((candidate_eq.ata - std_eq.ata).norm(), ata_budget);
  EXPECT_LE((candidate_eq.atb - std_eq.atb).norm(), atb_budget);
}

// block-reduction diagnostic 按 a/b/c/normal 四组在一个 row block 内累计 partial
// sums。它降低同时活跃 accumulator 数量，但重复 load/formula，并改变 reduction tree。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudBlockReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// block invalid-lane 诊断是当前 production block-reduction 的局部保护：NaN/Inf lane
// 必须被 mask 成零并不计入 accepted_points。它不覆盖 public dispatch gate。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudBlockReductionInvalidLanesWithinBudget)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// block scale-stress 诊断保护 reduction-tree 误差预算：高动态范围样本下 ATA/ATb
// 允许相对误差，但 accepted_points 和矩阵合同仍需对齐标量。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudBlockReductionScaleStressWithinBudget)
{
  const auto source = makeScaleStressCloud();
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &candidate_stats);

  const double ata_budget = std::max(1.0, std_eq.ata.norm() * 1e-4);
  const double atb_budget = std::max(1.0, std_eq.atb.norm() * 1e-4);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_LE((candidate_eq.ata - std_eq.ata).norm(), ata_budget);
  EXPECT_LE((candidate_eq.atb - std_eq.atb).norm(), atb_budget);
}

// fused-formula block 变体只替换 a/b/c/d 的 lane 内公式，保留 current block 的
// A/B/C/N 分组。这个探索测试证明它在现有正常样本下仍落在 normal-equation 预算内；
// 近似抵消和 scale-stress 的额外风险由后面的专门样本覆盖。
TEST(TransformationEstimationPointToPlaneLLS,
     FullCloudBlockFusedFormulaReductionMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(20, 0.14f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full_block_fused_formula_reduction(
          source, target, &candidate_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f candidate_matrix = support::solve_normal_equation(candidate_eq);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 1.0);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 1.0);
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// fused-formula near-cancellation 样本专门把 d 公式的 dx-sx、dy-sy、dz-sz 做成“两个大项
// 相减只剩小余量”的形态，同时叠加 scale-stress 和一个无效 lane。它用来约束：
// 1) accepted_points 只统计 finite row；
// 2) ATA/ATb 和矩阵仍在预算内；
// 3) 逐点 fused 写法不会在抵消区间里比 current block 更脆弱。
TEST(TransformationEstimationPointToPlaneLLS,
     FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget)
{
  const auto [source, target] = makeNearCancellationCloudPair();

  support::AccumulationStats std_stats;
  support::AccumulationStats current_stats;
  support::AccumulationStats fused_stats;
  const support::NormalEquation std_eq = support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation current_eq =
      support::accumulate_candidate_full_block_reduction(source, target, &current_stats);
  const support::NormalEquation fused_eq =
      support::accumulate_candidate_full_block_fused_formula_reduction(
          source, target, &fused_stats);
  const Eigen::Matrix4f std_matrix = support::solve_normal_equation(std_eq);
  const Eigen::Matrix4f current_matrix = support::solve_normal_equation(current_eq);
  const Eigen::Matrix4f fused_matrix = support::solve_normal_equation(fused_eq);

  EXPECT_EQ(std_stats.input_points, source.size());
  EXPECT_EQ(current_stats.input_points, std_stats.input_points);
  EXPECT_EQ(fused_stats.input_points, std_stats.input_points);
  EXPECT_EQ(current_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(fused_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(current_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(fused_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(current_stats.used_rvv);
  EXPECT_TRUE(fused_stats.used_rvv);
#else
  EXPECT_FALSE(current_stats.used_rvv);
  EXPECT_FALSE(fused_stats.used_rvv);
#endif
  const double current_ata_delta = (current_eq.ata - std_eq.ata).norm();
  const double current_atb_delta = (current_eq.atb - std_eq.atb).norm();
  const double fused_ata_delta = (fused_eq.ata - std_eq.ata).norm();
  const double fused_atb_delta = (fused_eq.atb - std_eq.atb).norm();
  const double ata_budget = std::max(256.0, std_eq.ata.norm() * 1e-4);
  const double atb_budget = std::max(16.0, std_eq.atb.norm() * 1e-4);
  EXPECT_LE(fused_ata_delta, ata_budget);
  EXPECT_LE(fused_atb_delta, atb_budget);
  EXPECT_LE(fused_ata_delta, std::max(1.0, current_ata_delta * 1.25));
  EXPECT_LE(fused_atb_delta, std::max(1.0, current_atb_delta * 1.25));
  expectMatrixNear(current_matrix, std_matrix, 8e-4f);
  expectMatrixNear(fused_matrix, std_matrix, 8e-4f);
}

// Public-entry-shaped check: inputs and output matrix follow the public
// full-cloud overload contract, while the RVV side still calls only the
// test-rvv block shim. This proves shape compatibility, not production dispatch.
TEST(TransformationEstimationPointToPlaneLLS,
     FullCloudBlockReductionPublicEntryShapeMatchesPublicWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f block_matrix =
      support::estimate_candidate_full_block_reduction(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, source.size());
  EXPECT_EQ(candidate_stats.accepted_points, source.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(block_matrix, public_matrix, 5e-4f);
}
