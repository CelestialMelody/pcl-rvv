/*
 * 本文件做什么：
 * 历史 row-source diagnostic、trusted-dense 消融、小规模 fallback 和 invalid-lane mask 证据。
 * 共享 fixtures/assertions 来自 test_teptpl.h，TEST body 保持原 case 名和证据语义。
 */

#include "test_teptpl.h"

using namespace pcl::registration::rvv_te_pt2plane_lls_test;
namespace support = pcl::registration::rvv_te_pt2plane_lls_support;
namespace prod_detail = pcl::registration::detail;

// trusted-dense diagnostic 是一个显式消融入口：它依赖 source/target 的 is_dense 合同，
// 跳过 finite mask 和 vcompress。它只用于评估未来 production 若授权 dense gate 是否
// 值得推进；当前 production 仍逐点检查 finite。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudTrustedDenseCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  ASSERT_TRUE(source.is_dense);
  ASSERT_TRUE(target.is_dense);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimate_std_full(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_full_trusted_dense(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

// 这个测试验证 test-rvv-only SourceIndexedRowSource policy：source 侧按 indices gather，
// target 侧按紧凑全云 stride load。失败说明单侧 gather 取数或 shared math pipeline
// 与标量 same-chain（同构链路）不一致。
TEST(TransformationEstimationPointToPlaneLLS, SourceIndicesCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target_full = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const auto target = copyIndexedCloud(target_full, source_indices);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_source_indices(source, source_indices, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_source_indices(source, source_indices, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// source-indexed trusted-dense 是历史消融：跳过 mask/compress 观察单侧 gather 成本。
// 当前 production 没有 dense gate，也没有 indexed dispatch；它只保留为负向证据复核。
TEST(TransformationEstimationPointToPlaneLLS,
     SourceIndicesTrustedDenseCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target_full = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const auto target = copyIndexedCloud(target_full, source_indices);
  ASSERT_TRUE(source.is_dense);
  ASSERT_TRUE(target.is_dense);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_source_indices(source, source_indices, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_source_indices_trusted_dense(
          source, source_indices, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证 dual-indices policy 使用两条独立 index stream。它不解析
// pcl::Correspondence，因此失败只能说明双侧 gather 诊断链路断了，不支持把问题
// 单因归到 correspondences 展开或 gather 某一项。
TEST(TransformationEstimationPointToPlaneLLS, DualIndicesCandidateMatchesStd)
{
  auto source = makeSurfaceCloud(32, 0.10f);
  auto target = makeTargetCloud(source);
  pcl::Indices source_indices = makeIndexedRows(source.size());
  pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const std::size_t paired_rows = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(paired_rows);
  target_indices.resize(paired_rows);

  ASSERT_FALSE(source_indices.empty());
  ASSERT_FALSE(target_indices.empty());
  ASSERT_GE(source_indices.size(), std::size_t{12});
  ASSERT_GE(target_indices.size(), std::size_t{12});
  EXPECT_NE(source_indices, target_indices);
  EXPECT_NE(std::find(source_indices.begin() + 2, source_indices.end(), source_indices[1]),
            source_indices.end());
  EXPECT_NE(std::find(target_indices.begin() + 2, target_indices.end(), target_indices[1]),
            target_indices.end());

  source[static_cast<std::size_t>(source_indices[1])].x =
      std::numeric_limits<float>::quiet_NaN();
  target[static_cast<std::size_t>(target_indices[2])].normal_z =
      std::numeric_limits<float>::infinity();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimate_std_dual_indices(
      source, source_indices, target, target_indices, &std_stats);
  const Eigen::Matrix4f candidate_matrix = support::estimate_candidate_dual_indices(
      source, source_indices, target, target_indices, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 4e-4f);
}

// 这个测试把同一条 index stream 同时用于 source 和 target。它是 correspondences
// same-index case 的公平对照：两者访问相关性接近，但 dual-indices 没有 query/match
// 展开阶段。失败说明 shared dual-index policy 自身不稳定。
TEST(TransformationEstimationPointToPlaneLLS, DualIndicesSameStreamCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices indices = makeIndexedRows(source.size());

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_dual_indices(source, indices, target, indices, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_dual_indices(source, indices, target, indices, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证 correspondences（对应关系）路径中的乱序、重复索引 gather（索引读取）不会改变求解结果。
// 如果失败，说明非连续访问语义、保序 staging 或对应关系路径的证据断了。
TEST(TransformationEstimationPointToPlaneLLS, CorrespondenceCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeShuffledCorrespondences(source.size());

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证 query/match 不相同但仍有局部性的 correspondences。它补上
// same-index 分布没有覆盖的匹配偏移，失败时说明 indexed row 语义或矩阵求解
// 对更真实的 correspondence 分布不稳。
TEST(TransformationEstimationPointToPlaneLLS,
     CorrespondenceLocalOffsetCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences =
      makeLocalOffsetCorrespondences(source.size());

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// 这个测试把 correspondences 构造成与 dual-indices independent-stream 相同的
// query/match 分布。它用于解释性能：若两者板卡结果不同，差异更可能来自
// correspondence 展开、容器布局或 baseline，而不是 index stream 本身。
TEST(TransformationEstimationPointToPlaneLLS,
     CorrespondenceIndependentStreamCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const pcl::Correspondences correspondences =
      makeCorrespondencesFromIndices(source_indices, target_indices);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// correspondence trusted-dense 是历史消融：在 independent-stream 分布上跳过
// mask/compress，帮助分离 correspondence 展开与后段有限值检查成本；它不是 production 证据。
TEST(TransformationEstimationPointToPlaneLLS,
     CorrespondenceIndependentStreamTrustedDenseCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeIndexedRows(source.size());
  const pcl::Indices target_indices = makeIndependentTargetIndexedRows(target.size());
  const pcl::Correspondences correspondences =
      makeCorrespondencesFromIndices(source_indices, target_indices);
  ASSERT_TRUE(source.is_dense);
  ASSERT_TRUE(target.is_dense);

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimate_candidate_correspondences_trusted_dense(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// 这个测试单独隔离小规模 fallback（回退路径）gate：输入太小时必须回到标量链路。
// 如果失败，说明规模阈值 gate 或 fallback 的 normal-equation 等价性断了。
TEST(TransformationEstimationPointToPlaneLLS, SmallInputFallsBackForIsolatedSizeGate)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);

  support::AccumulationStats stats;
  const support::NormalEquation candidate =
      support::accumulate_candidate_full(source, target, &stats);
  const support::NormalEquation reference = support::accumulate_std_full(source, target);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(candidate.accepted_points, reference.accepted_points);
  EXPECT_NEAR((candidate.ata - reference.ata).norm(), 0.0, 1e-12);
  EXPECT_NEAR((candidate.atb - reference.atb).norm(), 0.0, 1e-12);
}

// 这个测试验证 NaN/Inf invalid lane（无效 lane）会被 finite mask（有限值掩码）剔除，并保持标量可见语义。
// 如果失败，说明 mask、vcompress（保序压缩）或压缩后标量 tail 的证据断了。
TEST(TransformationEstimationPointToPlaneLLS, InvalidLaneMaskMatchesStd)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  support::AccumulationStats std_stats;
  support::AccumulationStats candidate_stats;
  const support::NormalEquation std_eq =
      support::accumulate_std_full(source, target, &std_stats);
  const support::NormalEquation candidate_eq =
      support::accumulate_candidate_full(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}
