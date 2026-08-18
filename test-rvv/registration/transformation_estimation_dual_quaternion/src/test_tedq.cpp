/*
 * 本文件做什么：
 * 这些 gtest（单元测试）验证 transformation_estimation_dual_quaternion 的 test-only
 * RVV candidate（RVV 候选链路）是否与同构标量 reference（参考链路）和当前 public
 * dual-quaternion 入口保持数值一致。当前阶段把 ordered-cloud-pair（顺序点云对）、
 * source-indexed-cloud-pair（源索引点云对）、dual-indexed-cloud-pair（双索引点云对）
 * 和 correspondence-pair（对应关系点对）作为四类独立 row-source policy（行来源策略）记录。
 */

#include "tedq.h"

#include <pcl/rvv_point_traits.h>
#include <pcl/test/gtest.h>

namespace support = pcl::registration::rvv_tedq_support;

namespace {

template <typename CandidateMatrix, typename ReferenceMatrix>
void
expectMatrixNear(const CandidateMatrix& candidate,
                 const ReferenceMatrix& reference,
                 const double epsilon)
{
  ASSERT_EQ(candidate.rows(), reference.rows());
  ASSERT_EQ(candidate.cols(), reference.cols());
  for (int row = 0; row < candidate.rows(); ++row) {
    for (int col = 0; col < candidate.cols(); ++col) {
      EXPECT_NEAR(static_cast<double>(candidate(row, col)),
                  static_cast<double>(reference(row, col)),
                  epsilon)
          << "matrix mismatch at (" << row << ", " << col << ")";
    }
  }
}

} // namespace

// public dual-quaternion（公开双四元数入口）仍是当前 production truth（生产事实）。
// test-only scalar reference 复刻 production 中 C1/C2 累加、Eigen 求解和矩阵构造，
// 失败说明诊断 reference 已经偏离目标源码。
TEST(TransformationEstimationDualQuaternion, ScalarReferenceMatchesPublicOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  const Eigen::Matrix4f public_matrix = support::estimatePublicDualQuaternion(source, target);
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionStd(source, target);

  expectMatrixNear(scalar_matrix, public_matrix, 2e-5f);
}

// RVV candidate 只替换 ordered-cloud-pair 的 C1/C2 前置累加。这个测试保护
// vector reduction（向量规约）改变累加树后，最终刚体矩阵仍落在当前误差预算内。
TEST(TransformationEstimationDualQuaternion, CandidateMatchesScalarOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f scalar_matrix =
      support::estimateDualQuaternionStd(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCandidate(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, scalar_matrix, 7e-5f);
}

// 小规模输入走标量 fallback（回退路径）。这避免把 4x4 Eigen 求解周围的极小数组送进
// RVV setup cost（向量设置成本）更高的路径。
TEST(TransformationEstimationDualQuaternion, SmallInputFallsBack)
{
  const auto source = support::makePointXYZCloud(8);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCandidate(source, target, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, scalar_matrix, 2e-5f);
}

// PointXYZI 覆盖带额外字段的 xyz AoS（结构数组）布局。candidate 只读取 x/y/z，
// intensity 不在当前证据边界内，不能被写成完整泛型点类型 production 证明。
TEST(TransformationEstimationDualQuaternion, PointXYZILayoutMatchesScalar)
{
  const auto source = support::makePointXYZICloud(4096);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCandidate(source, target, &stats);

#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, scalar_matrix, 9e-5f);
}

// production ordered-cloud-pair public entry（生产顺序点云对公开入口）必须接受带额外字段的
// PointXYZI。这个边界曾用于 Phase 002 临时 production dispatch 尝试；当前结论仍只证明
// public entry 与同构 scalar reference 一致，intensity 不在当前优化语义内。
TEST(TransformationEstimationDualQuaternion, PublicPointXYZILayoutMatchesScalar)
{
  const auto source = support::makePointXYZICloud(4096);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  pcl::registration::TransformationEstimationDualQuaternion<pcl::PointXYZI,
                                                            pcl::PointXYZI,
                                                            float>
      estimator;
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionStd(source, target);

  expectMatrixNear(public_matrix, scalar_matrix, 9e-5f);
}

// 这个 gate probe（门控探针）只检查公共 xyz AoS layout trait（结构数组布局特征），
// 不代表 TEDQ production 已经接入 RVV。它把 Phase 002 的 production boundary
// 排查落成可复核测试：代表性 x/y/z float 点型本身不会被 `RVVXYZAoSFloatLayout`
// 静默挡在 RVV 分流之外。
TEST(TransformationEstimationDualQuaternion, PublicGateAllowsRepresentativeXYZLayouts)
{
  EXPECT_TRUE(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZ>::value);
  EXPECT_TRUE(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::value);
  EXPECT_TRUE(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGB>::value);

  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZ>::kX, 0u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZ>::kY, 4u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZ>::kZ, 8u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::kX, 0u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::kY, 4u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::kZ, 8u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGB>::kX, 0u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGB>::kY, 4u);
  EXPECT_EQ(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGB>::kZ, 8u);
}

// Scalar=double（double 输出矩阵）不是本阶段 RVV 范围。这个测试保护模板实例化和
// 保守语义：即使未来再次尝试 RVV production dispatch，double 输出也必须保持标量路径。
TEST(TransformationEstimationDualQuaternion, DoubleScalarOutputMatchesScalar)
{
  const auto source = support::makePointXYZCloud(2048);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  pcl::registration::TransformationEstimationDualQuaternion<pcl::PointXYZ,
                                                            pcl::PointXYZ,
                                                            double>
      estimator;
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionStd(source, target);

  expectMatrixNear(public_matrix, scalar_matrix, 2e-5);
}

// source-indexed-cloud-pair（源索引点云对）的 identity indices 应和 ordered
// public entry 给出同一刚体变换；该测试保护 row-source policy 的公开入口语义。
TEST(TransformationEstimationDualQuaternion, PublicSourceIndexedIdentityMatchesOrdered)
{
  const auto source = support::makePointXYZCloud(2048);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  pcl::Indices indices(source.size());
  for (std::size_t i = 0; i < source.size(); ++i)
    indices[i] = static_cast<pcl::index_t>(i);

  pcl::registration::TransformationEstimationDualQuaternion<pcl::PointXYZ,
                                                            pcl::PointXYZ,
                                                            float>
      estimator;
  Eigen::Matrix4f ordered_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4f indexed_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, ordered_matrix);
  estimator.estimateRigidTransformation(source, indices, target, indexed_matrix);

  expectMatrixNear(indexed_matrix, ordered_matrix, 9e-5f);
}

// source-indexed-cloud-pair（源索引点云对）的真实 row pairing 是
// `source[indices[i]] -> target[i]`。这个非 identity case 先证明 public iterator
// 链路与 staged scalar reference（先暂存成顺序点云的标量参考链路）一致。
TEST(TransformationEstimationDualQuaternion,
     PublicSourceIndexedNonIdentityMatchesStagedScalar)
{
  const auto source = support::makePointXYZCloud(8193);
  const auto indices = support::makeSourceIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  const Eigen::Matrix4f public_matrix =
      support::estimatePublicSourceIndexedDualQuaternion(source, indices, target);
  const Eigen::Matrix4f staged_matrix =
      support::estimateDualQuaternionSourceIndexedStd(source, indices, target);

  expectMatrixNear(public_matrix, staged_matrix, 9e-5f);
}

// staged source-indexed candidate（暂存源索引候选）把离散 source rows 先复制成紧凑
// ordered cloud，再复用现有 ordered RVV C1/C2 candidate。这个测试验证 staging
// 没有改变 row pairing，失败时说明 index 展开或后续 RVV 累加边界不可靠。
TEST(TransformationEstimationDualQuaternion, SourceIndexedStagedCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto indices = support::makeSourceIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimateDualQuaternionSourceIndexedStd(source, indices, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionSourceIndexedCandidate(
          source, indices, target, &candidate_stats);

  EXPECT_TRUE(candidate_stats.used_staging);
  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

// direct source-indexed gather candidate（直接源索引离散加载候选）不 materialize
// 紧凑点云，而是用索引字节偏移读取 source 的 x/y/z，再复用同一套 C1/C2
// widen-to-f64 reduction。这个测试同时保护 row pairing、gather gate 和 scalar
// fallback；production dispatch 是否命中由下方 production detail path-hit 测试覆盖。
TEST(TransformationEstimationDualQuaternion, SourceIndexedDirectGatherCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto indices = support::makeSourceIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimateDualQuaternionSourceIndexedStd(source, indices, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionSourceIndexedDirectCandidate(
          source, indices, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, indices.size());
  EXPECT_EQ(candidate_stats.accepted_points, indices.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_gather);
  EXPECT_FALSE(candidate_stats.used_staging);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

template <typename PointT, typename Factory>
void
expectSourceIndexedDirectGatherPointTypeMatchesScalar(Factory make_cloud)
{
  const auto source = make_cloud(131073);
  const auto indices = support::makeSourceIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix =
      support::estimateDualQuaternionSourceIndexedStd(source, indices, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionSourceIndexedDirectCandidate(
          source, indices, target, &stats);

  EXPECT_EQ(stats.input_points, indices.size());
  EXPECT_EQ(stats.accepted_points, indices.size());
  EXPECT_TRUE(stats.layout_supported);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_TRUE(stats.used_gather);
  EXPECT_FALSE(stats.used_staging);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

TEST(TransformationEstimationDualQuaternion,
     SourceIndexedDirectGatherPointXYZILayoutMatchesScalar)
{
  expectSourceIndexedDirectGatherPointTypeMatchesScalar<pcl::PointXYZI>(
      support::makePointXYZICloud);
}

TEST(TransformationEstimationDualQuaternion,
     SourceIndexedDirectGatherPointXYZRGBLayoutMatchesScalar)
{
  expectSourceIndexedDirectGatherPointTypeMatchesScalar<pcl::PointXYZRGB>(
      support::makePointXYZRGBCloud);
}

// dual-indexed-cloud-pair（源和目标都有索引的点云对）同样保持标量路径。
// 这个 identity case 保护未覆盖 row source policy 不被 ordered-cloud-pair RVV 结论误扩展。
TEST(TransformationEstimationDualQuaternion, PublicDualIndexedIdentityMatchesOrdered)
{
  const auto source = support::makePointXYZCloud(2048);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  pcl::Indices indices(source.size());
  for (std::size_t i = 0; i < source.size(); ++i)
    indices[i] = static_cast<pcl::index_t>(i);

  pcl::registration::TransformationEstimationDualQuaternion<pcl::PointXYZ,
                                                            pcl::PointXYZ,
                                                            float>
      estimator;
  Eigen::Matrix4f ordered_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4f dual_indexed_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, ordered_matrix);
  estimator.estimateRigidTransformation(
      source, indices, target, indices, dual_indexed_matrix);

  expectMatrixNear(dual_indexed_matrix, ordered_matrix, 9e-5f);
}

// dual-indexed-cloud-pair 同时展开 source 和 target 两侧索引。这里的 public
// 入口、staged scalar reference 和 staged RVV candidate 必须对齐同一 row 顺序；
// 否则后续任何双侧 gather 或 staging bench 都没有语义基础。
TEST(TransformationEstimationDualQuaternion, DualIndexedStagedCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(8193);
  const auto source_indices = support::makeSourceIndices(source.size(), 4096);
  const auto target_indices = support::makeTargetIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f public_matrix = support::estimatePublicDualIndexedDualQuaternion(
      source, source_indices, target, target_indices);
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionDualIndexedStd(
      source, source_indices, target, target_indices, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionDualIndexedCandidate(
          source, source_indices, target, target_indices, &candidate_stats);

  EXPECT_TRUE(candidate_stats.used_staging);
  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(public_matrix, std_matrix, 9e-5f);
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

// dual-indexed direct gather candidate（双索引直接离散加载候选）同时从 source 和
// target 两侧读取 index vector，再用两组 `vluxei32` 取得 x/y/z。它保持 staged
// candidate 的 row pairing、C1/C2 公式和 Eigen solve，只替换两侧 row ingress。
TEST(TransformationEstimationDualQuaternion,
     DualIndexedDirectGatherCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto source_indices = support::makeSourceIndices(source.size(), 65536);
  const auto target_indices = support::makeTargetIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());

  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionDualIndexedStd(
      source, source_indices, target, target_indices);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionDualIndexedDirectCandidate(
          source, source_indices, target, target_indices, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, source_indices.size());
  EXPECT_EQ(candidate_stats.accepted_points, source_indices.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_gather);
  EXPECT_FALSE(candidate_stats.used_staging);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

template <typename PointT, typename Factory>
void
expectDualIndexedDirectGatherPointTypeMatchesScalar(Factory make_cloud)
{
  const auto source = make_cloud(131073);
  const auto source_indices = support::makeSourceIndices(source.size(), 65536);
  const auto target_indices = support::makeTargetIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionDualIndexedStd(
      source, source_indices, target, target_indices);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionDualIndexedDirectCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  EXPECT_TRUE(stats.layout_supported);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_TRUE(stats.used_gather);
  EXPECT_FALSE(stats.used_staging);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

TEST(TransformationEstimationDualQuaternion,
     DualIndexedDirectGatherPointXYZILayoutMatchesScalar)
{
  expectDualIndexedDirectGatherPointTypeMatchesScalar<pcl::PointXYZI>(
      support::makePointXYZICloud);
}

TEST(TransformationEstimationDualQuaternion,
     DualIndexedDirectGatherPointXYZRGBLayoutMatchesScalar)
{
  expectDualIndexedDirectGatherPointTypeMatchesScalar<pcl::PointXYZRGB>(
      support::makePointXYZRGBCloud);
}

// correspondence-pair（对应关系点对）公开入口仍走 production 当前 iterator 链路。
// 这里仅证明 identity correspondence 与 ordered-cloud-pair public entry 输出一致，
// 不把它升级成 RVV candidate 或 production dispatch 证据。
TEST(TransformationEstimationDualQuaternion, PublicCorrespondenceEntryMatchesOrdered)
{
  const auto source = support::makePointXYZCloud(1024);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  pcl::Correspondences correspondences;
  correspondences.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);

  pcl::registration::TransformationEstimationDualQuaternion<pcl::PointXYZ,
                                                            pcl::PointXYZ,
                                                            float>
      estimator;
  Eigen::Matrix4f ordered_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4f correspondence_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, ordered_matrix);
  estimator.estimateRigidTransformation(source, target, correspondences, correspondence_matrix);

  expectMatrixNear(correspondence_matrix, ordered_matrix, 9e-5f);
}

// correspondence-pair（对应关系点对）使用 query / match 两列索引。这个非 identity
// case 把 correspondence 展开成 staged ordered clouds 后再跑现有 RVV candidate，
// 只证明 test-rvv 诊断语义；它仍不是 production dispatch 证据。
TEST(TransformationEstimationDualQuaternion,
     CorrespondenceStagedCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(8193);
  const auto source_indices = support::makeSourceIndices(source.size(), 4096);
  const auto target_indices = support::makeTargetIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source.size(), target.size(), 4096);

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f public_matrix =
      support::estimatePublicCorrespondenceDualQuaternion(source, target, correspondences);
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionCorrespondenceStd(
      source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCorrespondenceCandidate(
          source, target, correspondences, &candidate_stats);

  EXPECT_TRUE(candidate_stats.used_staging);
  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(public_matrix, std_matrix, 9e-5f);
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

// correspondence direct gather candidate（对应关系直接离散加载候选）把 query / match
// 两列转换为双侧 index stream，再复用 dual-indexed direct gather。这个测试只证明
// correspondence 语义与 direct candidate 数值一致，不把它外推为 production dispatch。
TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectGatherCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto source_indices = support::makeSourceIndices(source.size(), 65536);
  const auto target_indices = support::makeTargetIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source.size(), target.size(), 65536);

  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionCorrespondenceStd(
      source, target, correspondences);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCorrespondenceDirectCandidate(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, correspondences.size());
  EXPECT_EQ(candidate_stats.accepted_points, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_gather);
  EXPECT_FALSE(candidate_stats.used_staging);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

// correspondence direct index stream（对应关系直接索引流）不再先构造两个临时
// `pcl::Indices`，而是从 `Correspondence` AoS 记录按固定 stride 读取 query/match。
// 这个测试验证 correspondence direct index stream 与标量 reference 一致；
// local-window / contiguous / strided 是 correspondence-pair 内部的索引模式。
TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectIndexStreamCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto source_indices = support::makeSourceIndices(source.size(), 65536);
  const auto target_indices = support::makeTargetIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source.size(), target.size(), 65536);

  support::CandidateStats stream_stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionCorrespondenceStd(
      source, target, correspondences);
  const Eigen::Matrix4f stream_matrix =
      support::estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate(
          source, target, correspondences, &stream_stats);

  EXPECT_EQ(stream_stats.input_points, correspondences.size());
  EXPECT_EQ(stream_stats.accepted_points, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(stream_stats.used_rvv);
  EXPECT_TRUE(stream_stats.used_gather);
  EXPECT_TRUE(stream_stats.used_correspondence_index_stream);
  EXPECT_FALSE(stream_stats.used_staging);
#else
  EXPECT_FALSE(stream_stats.used_rvv);
  EXPECT_TRUE(stream_stats.used_fallback);
#endif
  expectMatrixNear(stream_matrix, std_matrix, 1e-4f);
}

// correspondence segment index stream（对应关系分段索引流）用一次 `vlseg3e32`
// 读取连续的 query/match/distance 三列，只消费前两列再进入双侧 gather。
// 这个测试保护字段提取、row pairing 和 correspondence direct index stream 的
// 数值一致性；distance 列不是 TEDQ 当前数学输入。
TEST(TransformationEstimationDualQuaternion,
     CorrespondenceSegmentIndexStreamCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto source_indices = support::makeSourceIndices(source.size(), 65536);
  const auto target_indices = support::makeTargetIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source.size(), target.size(), 65536);

  support::CandidateStats segment_stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionCorrespondenceStd(
      source, target, correspondences);
  const Eigen::Matrix4f segment_matrix =
      support::estimateDualQuaternionCorrespondenceSegmentIndexStreamCandidate(
          source, target, correspondences, &segment_stats);

  EXPECT_EQ(segment_stats.input_points, correspondences.size());
  EXPECT_EQ(segment_stats.accepted_points, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(segment_stats.used_rvv);
  EXPECT_TRUE(segment_stats.used_gather);
  EXPECT_TRUE(segment_stats.used_correspondence_segment_stream);
  EXPECT_FALSE(segment_stats.used_staging);
#else
  EXPECT_FALSE(segment_stats.used_rvv);
  EXPECT_TRUE(segment_stats.used_fallback);
#endif
  expectMatrixNear(segment_matrix, std_matrix, 1e-4f);
}

// 这个测试固定 correspondence direct index stream，只改变 query/match 的索引分布。
// 目标云按相同下标变换，因此三种 pattern（连续、局部窗口、跨步）只改变
// gather locality（离散加载局部性），不改变点对数学语义。
void
expectCorrespondenceIndexPatternMatchesScalar(
    const support::CorrespondenceIndexPattern pattern)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondencesForPattern(
      source.size(), target.size(), 65536, pattern);

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionCorrespondenceStd(
      source, target, correspondences);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate(
          source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_TRUE(stats.used_gather);
  EXPECT_TRUE(stats.used_correspondence_index_stream);
  EXPECT_FALSE(stats.used_staging);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

template <typename PointT, typename Factory>
void
expectCorrespondenceDirectIndexStreamPointTypeMatchesScalar(Factory make_cloud)
{
  const auto source = make_cloud(131073);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondencesForPattern(
      source.size(),
      target.size(),
      65536,
      support::CorrespondenceIndexPattern::strided);

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateDualQuaternionCorrespondenceStd(
      source, target, correspondences);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate(
          source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  EXPECT_TRUE(stats.layout_supported);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_TRUE(stats.used_gather);
  EXPECT_TRUE(stats.used_correspondence_index_stream);
  EXPECT_FALSE(stats.used_staging);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 1e-4f);
}

TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectIndexStreamContiguousPatternMatchesScalar)
{
  expectCorrespondenceIndexPatternMatchesScalar(
      support::CorrespondenceIndexPattern::contiguous);
}

TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectIndexStreamLocalWindowPatternMatchesScalar)
{
  expectCorrespondenceIndexPatternMatchesScalar(
      support::CorrespondenceIndexPattern::local_window);
}

TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectIndexStreamStridedPatternMatchesScalar)
{
  expectCorrespondenceIndexPatternMatchesScalar(
      support::CorrespondenceIndexPattern::strided);
}

// Phase 012 从 correspondence-pair direct index stream 的代表性 xyz AoS 点型开始。
// PointXYZI 的 intensity 和 PointXYZRGB 的颜色字段都不是 TEDQ 数学输入；这里验证
// candidate 只按 x/y/z offset 和 sizeof(PointT) 做离散加载，不污染附加字段语义。
TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectIndexStreamPointXYZILayoutMatchesScalar)
{
  expectCorrespondenceDirectIndexStreamPointTypeMatchesScalar<pcl::PointXYZI>(
      support::makePointXYZICloud);
}

TEST(TransformationEstimationDualQuaternion,
     CorrespondenceDirectIndexStreamPointXYZRGBLayoutMatchesScalar)
{
  expectCorrespondenceDirectIndexStreamPointTypeMatchesScalar<pcl::PointXYZRGB>(
      support::makePointXYZRGBCloud);
}

#ifdef __RVV10__
// production detail path-hit（生产内部路径命中）测试直接调用 header 内的 RVV helper。
// 它不新增 public API；失败说明公开入口即使编译为 RVV，也无法在当前 gate 下命中生产 RVV 主路径。
TEST(TransformationEstimationDualQuaternion,
     ProductionRVVOrderedCloudPairPathHitMatchesScalar)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv =
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(
              source, target, rvv_matrix);
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionStd(source, target);

  EXPECT_TRUE(used_rvv);
  expectMatrixNear(rvv_matrix, scalar_matrix, 9e-5f);
}

TEST(TransformationEstimationDualQuaternion,
     ProductionRVVSourceIndexedCloudPairPathHitMatchesScalar)
{
  const auto source = support::makePointXYZCloud(8193);
  const auto indices = support::makeSourceIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv =
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionSourceIndexedCloudPairRVV(
              source, indices, target, rvv_matrix);
  const Eigen::Matrix4f scalar_matrix =
      support::estimateDualQuaternionSourceIndexedStd(source, indices, target);

  EXPECT_TRUE(used_rvv);
  expectMatrixNear(rvv_matrix, scalar_matrix, 1e-4f);
}

TEST(TransformationEstimationDualQuaternion,
     ProductionRVVDualIndexedCloudPairPathHitMatchesScalar)
{
  const auto source = support::makePointXYZCloud(8193);
  const auto source_indices = support::makeSourceIndices(source.size(), 4096);
  const auto target_indices = support::makeTargetIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZByPairedIndices(
      source, source_indices, target_indices, source.size(), support::makeRigidTransform());

  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv =
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionDualIndexedCloudPairRVV(
              source, source_indices, target, target_indices, rvv_matrix);
  const Eigen::Matrix4f scalar_matrix = support::estimateDualQuaternionDualIndexedStd(
      source, source_indices, target, target_indices);

  EXPECT_TRUE(used_rvv);
  expectMatrixNear(rvv_matrix, scalar_matrix, 1e-4f);
}

TEST(TransformationEstimationDualQuaternion, ProductionRVVFallbackGatesRejectOutOfScope)
{
  const auto small_source = support::makePointXYZCloud(8);
  const auto small_target =
      support::transformCloudXYZ(small_source, support::makeRigidTransform());
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  EXPECT_FALSE(
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(
              small_source, small_target, matrix));

  auto non_dense_source = support::makePointXYZCloud(4096);
  const auto non_dense_target =
      support::transformCloudXYZ(non_dense_source, support::makeRigidTransform());
  non_dense_source.is_dense = false;
  EXPECT_FALSE(
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(
              non_dense_source, non_dense_target, matrix));

  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();
  const auto dense_source = support::makePointXYZCloud(4096);
  const auto dense_target =
      support::transformCloudXYZ(dense_source, support::makeRigidTransform());
  EXPECT_FALSE(
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(
              dense_source, dense_target, double_matrix));

  pcl::Indices invalid_indices(4096, 0);
  invalid_indices.back() = static_cast<pcl::index_t>(dense_source.size());
  EXPECT_FALSE(
      pcl::registration::detail::
          estimateRigidTransformationDualQuaternionSourceIndexedCloudPairRVV(
              dense_source, invalid_indices, dense_target, matrix));
}
#endif // __RVV10__
