/*
 * 本文件做什么：
 * 这些 gtest（单元测试）验证 transformation_estimation_svd 的 test-only fused
 * accumulation（融合累加）candidate 是否与同构标量 reference 和当前 public Umeyama
 * 入口保持数值一致。Phase 030 增加 source-indexed-cloud-pair（源索引点云对）诊断路径；
 * Phase 040 继续保护 source-indexed production direct（真实生产路径直连）接入边界。
 */

#include "tesvd.h"

#include <pcl/test/gtest.h>

namespace support = pcl::registration::rvv_tesvd_support;

namespace {

void
expectMatrixNear(const Eigen::Matrix4f& candidate,
                 const Eigen::Matrix4f& reference,
                 const float epsilon)
{
  ASSERT_EQ(candidate.rows(), reference.rows());
  ASSERT_EQ(candidate.cols(), reference.cols());
  for (int r = 0; r < candidate.rows(); ++r) {
    for (int c = 0; c < candidate.cols(); ++c) {
      EXPECT_NEAR(candidate(r, c), reference(r, c), epsilon)
          << "matrix mismatch at (" << r << ", " << c << ")";
    }
  }
}

template <typename PointT>
void
expectPublicOrderedCloudPairMatchesFusedReference(const pcl::PointCloud<PointT>& source,
                                                  const float epsilon)
{
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  const Eigen::Matrix4f public_matrix = support::estimatePublicUmeyama(source, target);
  const Eigen::Matrix4f fused_matrix = support::estimateFusedStd(source, target);
  expectMatrixNear(public_matrix, fused_matrix, epsilon);
}

#ifdef __RVV10__
template <typename PointT>
void
expectProductionDirectRVVMatchesFusedReference(const pcl::PointCloud<PointT>& source,
                                               const float epsilon)
{
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());
  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<PointT, PointT, float>(
          source, target, true, rvv_matrix);
  EXPECT_TRUE(used_rvv);
  const Eigen::Matrix4f fused_matrix = support::estimateFusedStd(source, target);
  expectMatrixNear(rvv_matrix, fused_matrix, epsilon);
}

template <typename PointT>
void
expectProductionSourceIndexedDirectRVVMatchesFusedReference(
    const pcl::PointCloud<PointT>& source,
    const float epsilon)
{
  const auto indices = support::makeSourceIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());
  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDSourceIndexedCloudPairRVV<PointT, PointT, float>(
          source, indices, target, true, rvv_matrix);
  EXPECT_TRUE(used_rvv);
  const Eigen::Matrix4f fused_matrix =
      support::estimateFusedSourceIndexedStd(source, indices, target);
  expectMatrixNear(rvv_matrix, fused_matrix, epsilon);
}

template <typename PointT>
void
expectProductionDualIndicesDirectRVVMatchesFusedReference(
    const pcl::PointCloud<PointT>& source,
    const float epsilon)
{
  const auto source_indices = support::makeSourceIndices(source.size(), 4096);
  const auto target_indices = support::makeTargetIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZByIndexedPairs(
      source, source_indices, target_indices, support::makeRigidTransform());
  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDDualIndicesCloudPairRVV<PointT, PointT, float>(
          source, source_indices, target, target_indices, true, rvv_matrix);
  EXPECT_TRUE(used_rvv);
  const Eigen::Matrix4f fused_matrix = support::estimateFusedDualIndicesStd(
      source, source_indices, target, target_indices);
  expectMatrixNear(rvv_matrix, fused_matrix, epsilon);
}

template <typename PointT>
void
expectProductionCorrespondenceDirectRVVMatchesFusedReference(
    const pcl::PointCloud<PointT>& source,
    const float epsilon)
{
  const auto source_indices = support::makeSourceIndices(source.size(), 4096);
  const auto target_indices = support::makeTargetIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZByIndexedPairs(
      source, source_indices, target_indices, support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source_indices, target_indices);
  Eigen::Matrix4f rvv_matrix = Eigen::Matrix4f::Identity();
  const bool used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDCorrespondencePairRVV<PointT, PointT, float>(
          source, target, correspondences, true, rvv_matrix);
  EXPECT_TRUE(used_rvv);
  const Eigen::Matrix4f fused_matrix =
      support::estimateFusedCorrespondencesStd(source, target, correspondences);
  expectMatrixNear(rvv_matrix, fused_matrix, epsilon);
}

#endif

template <typename PointT>
void
expectDualIndicesCandidateMatchesScalar(const pcl::PointCloud<PointT>& source,
                                        const pcl::Indices& source_indices,
                                        const pcl::PointCloud<PointT>& target,
                                        const pcl::Indices& target_indices,
                                        const float epsilon)
{
  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedDualIndicesStd(
      source, source_indices, target, target_indices, &std_stats);
  const Eigen::Matrix4f candidate_matrix = support::estimateFusedDualIndicesCandidate(
      source, source_indices, target, target_indices, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, epsilon);
}

template <typename PointT>
void
expectCorrespondenceCandidateMatchesScalar(const pcl::PointCloud<PointT>& source,
                                           const pcl::PointCloud<PointT>& target,
                                           const pcl::Correspondences& correspondences,
                                           const float epsilon)
{
  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedCorrespondencesStd(
      source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix = support::estimateFusedCorrespondencesCandidate(
      source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, epsilon);
}
} // namespace

// public Umeyama（公开入口默认路径）仍是当前 production truth（生产事实）。
// fused scalar reference 用原始和 / 交叉和重建同一 no-scale Umeyama 矩阵，失败说明诊断公式
// 已偏离 `pcl::umeyama(src, tgt, false)` 的语义。
TEST(TransformationEstimationSVD, FusedStdMatchesPublicUmeyamaOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  const Eigen::Matrix4f public_matrix = support::estimatePublicUmeyama(source, target);
  const Eigen::Matrix4f fused_matrix = support::estimateFusedStd(source, target);

  expectMatrixNear(fused_matrix, public_matrix, 3e-4f);
}

// RVV candidate（RVV 候选链路）只替换 dense ordered-cloud-pair 前端累加，最终 3x3 SVD 仍走 Eigen。
// 这条测试保护 RVV reduction tree（规约树）和标量 fused reference 的误差预算。
TEST(TransformationEstimationSVD, FusedCandidateMatchesScalarOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedStd(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFusedCandidate(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 7e-4f);
}

// source-indexed-cloud-pair（源索引点云对）对应 public overload：
// `source[indices_src[i]]` 与 `target[i]` 配对。这个测试先用 public iterator 标量链路
// 对拍 source-indexed fused reference，避免把 ordered-cloud-pair 的语义直接外推过来。
TEST(TransformationEstimationSVD, SourceIndexedPublicMatchesFusedReference)
{
  const auto source = support::makePointXYZCloud(8193);
  const auto indices = support::makeSourceIndices(source.size(), 4096);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  const Eigen::Matrix4f public_matrix =
      support::estimatePublicSourceIndexedUmeyama(source, indices, target);
  const Eigen::Matrix4f fused_matrix =
      support::estimateFusedSourceIndexedStd(source, indices, target);

  expectMatrixNear(fused_matrix, public_matrix, 5e-4f);
}

// RVV source-indexed candidate 只把 source 侧换成 indexed gather（离散加载），target
// 仍是连续 ordered target。失败说明 gather offset、row pairing 或 reduction tree
// 与同构标量 reference 不一致。
TEST(TransformationEstimationSVD, SourceIndexedCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(131073);
  const auto indices = support::makeSourceIndices(source.size(), 65536);
  const auto target = support::transformCloudXYZBySourceIndices(
      source, indices, support::makeRigidTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimateFusedSourceIndexedStd(source, indices, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix = support::estimateFusedSourceIndexedCandidate(
      source, indices, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 2e-3f);
}

TEST(TransformationEstimationSVD, DualIndicesPublicMatchesFusedReference)
{
  const auto source = support::makePointXYZCloud(524289);
  const auto source_indices = support::makeSourceIndices(source.size(), 262144);
  const auto target_indices = support::makeTargetIndices(source.size(), 262144);
  const auto target = support::transformCloudXYZByIndexedPairs(
      source, source_indices, target_indices, support::makeRigidTransform());

  const Eigen::Matrix4f public_matrix =
      support::estimatePublicDualIndicesUmeyama(source, source_indices, target, target_indices);
  const Eigen::Matrix4f fused_matrix =
      support::estimateFusedDualIndicesStd(source, source_indices, target, target_indices);

  expectMatrixNear(fused_matrix, public_matrix, 4e-4f);
}

TEST(TransformationEstimationSVD, DualIndicesCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(524289);
  const auto source_indices = support::makeSourceIndices(source.size(), 262144);
  const auto target_indices = support::makeTargetIndices(source.size(), 262144);
  const auto target = support::transformCloudXYZByIndexedPairs(
      source, source_indices, target_indices, support::makeRigidTransform());

  expectDualIndicesCandidateMatchesScalar(
      source, source_indices, target, target_indices, 2e-3f);
}

TEST(TransformationEstimationSVD, CorrespondencePublicMatchesFusedReference)
{
  const auto source = support::makePointXYZCloud(524289);
  const auto source_indices = support::makeSourceIndices(source.size(), 262144);
  const auto target_indices = support::makeTargetIndices(source.size(), 262144);
  const auto target = support::transformCloudXYZByIndexedPairs(
      source, source_indices, target_indices, support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source_indices, target_indices);

  const Eigen::Matrix4f public_matrix =
      support::estimatePublicCorrespondencesUmeyama(source, target, correspondences);
  const Eigen::Matrix4f fused_matrix =
      support::estimateFusedCorrespondencesStd(source, target, correspondences);

  expectMatrixNear(fused_matrix, public_matrix, 4e-4f);
}

TEST(TransformationEstimationSVD, CorrespondenceCandidateMatchesScalar)
{
  const auto source = support::makePointXYZCloud(524289);
  const auto source_indices = support::makeSourceIndices(source.size(), 262144);
  const auto target_indices = support::makeTargetIndices(source.size(), 262144);
  const auto target = support::transformCloudXYZByIndexedPairs(
      source, source_indices, target_indices, support::makeRigidTransform());
  const auto correspondences = support::makeCorrespondences(source_indices, target_indices);

  expectCorrespondenceCandidateMatchesScalar(source, target, correspondences, 2e-3f);
}

TEST(TransformationEstimationSVD, DualIndicesCandidateRejectsOutOfScopeGates)
{
#ifdef __RVV10__
  const auto small_source = support::makePointXYZCloud(8);
  const auto small_source_indices = support::makeSourceIndices(small_source.size(), 4);
  const auto small_target_indices = support::makeTargetIndices(small_source.size(), 4);
  const auto small_target = support::transformCloudXYZByIndexedPairs(
      small_source, small_source_indices, small_target_indices, support::makeRigidTransform());
  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedDualIndicesStd(
      small_source, small_source_indices, small_target, small_target_indices, &stats);
  const Eigen::Matrix4f candidate_matrix = support::estimateFusedDualIndicesCandidate(
      small_source, small_source_indices, small_target, small_target_indices, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);

  auto regular_source = support::makePointXYZCloud(8193);
  const auto regular_source_indices = support::makeSourceIndices(regular_source.size(), 4096);
  const auto regular_target_indices = support::makeTargetIndices(regular_source.size(), 4096);
  auto regular_target = support::transformCloudXYZByIndexedPairs(
      regular_source,
      regular_source_indices,
      regular_target_indices,
      support::makeRigidTransform());
  regular_target.is_dense = false;
  const Eigen::Matrix4f non_dense_matrix = support::estimateFusedDualIndicesCandidate(
      regular_source, regular_source_indices, regular_target, regular_target_indices, &stats);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(non_dense_matrix,
                   support::estimateFusedDualIndicesStd(regular_source,
                                                        regular_source_indices,
                                                        regular_target,
                                                        regular_target_indices),
                   3e-4f);

  Eigen::Matrix4d matrix_d = Eigen::Matrix4d::Identity();
  const bool double_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<pcl::PointXYZ,
                                                        pcl::PointXYZ,
                                                        double>(regular_source,
                                                                 regular_target,
                                                                 true,
                                                                 matrix_d);
  EXPECT_FALSE(double_used_rvv);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

TEST(TransformationEstimationSVD, CorrespondenceCandidateRejectsOutOfScopeGates)
{
#ifdef __RVV10__
  const auto small_source = support::makePointXYZCloud(8);
  const auto small_source_indices = support::makeSourceIndices(small_source.size(), 4);
  const auto small_target_indices = support::makeTargetIndices(small_source.size(), 4);
  const auto small_target = support::transformCloudXYZByIndexedPairs(
      small_source, small_source_indices, small_target_indices, support::makeRigidTransform());
  const auto small_correspondences =
      support::makeCorrespondences(small_source_indices, small_target_indices);
  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedCorrespondencesStd(
      small_source, small_target, small_correspondences, &stats);
  const Eigen::Matrix4f candidate_matrix = support::estimateFusedCorrespondencesCandidate(
      small_source, small_target, small_correspondences, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);

  auto regular_source = support::makePointXYZCloud(8193);
  const auto regular_source_indices = support::makeSourceIndices(regular_source.size(), 4096);
  const auto regular_target_indices = support::makeTargetIndices(regular_source.size(), 4096);
  auto regular_target = support::transformCloudXYZByIndexedPairs(
      regular_source,
      regular_source_indices,
      regular_target_indices,
      support::makeRigidTransform());
  auto invalid_correspondences =
      support::makeCorrespondences(regular_source_indices, regular_target_indices);
  invalid_correspondences[3].index_match = -1;
  const Eigen::Matrix4f invalid_matrix = support::estimateFusedCorrespondencesCandidate(
      regular_source, regular_target, invalid_correspondences, &stats);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(invalid_matrix,
                   support::estimateFusedCorrespondencesStd(regular_source,
                                                            regular_target,
                                                            invalid_correspondences),
                   3e-4f);

  Eigen::Matrix4d matrix_d = Eigen::Matrix4d::Identity();
  const bool double_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<pcl::PointXYZ,
                                                        pcl::PointXYZ,
                                                        double>(regular_source,
                                                                 regular_target,
                                                                 true,
                                                                 matrix_d);
  EXPECT_FALSE(double_used_rvv);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

// 小规模输入走标量 fallback（回退路径）。这避免把一次 3x3 SVD 调用周围的极小数组送进
// RVV setup cost（向量设置成本）更高的路径。
TEST(TransformationEstimationSVD, SmallInputFallsBack)
{
  const auto source = support::makePointXYZCloud(8);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFusedCandidate(source, target, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// PointXYZI 覆盖带额外字段的 xyz AoS（结构数组）布局。candidate 只读取 x/y/z，
// intensity 必须保持证据边界外，不能被写成完整泛型点类型 production 证明。
TEST(TransformationEstimationSVD, PointXYZILayoutMatchesScalar)
{
  const auto source = support::makePointXYZICloud(4096);
  const auto target = support::transformCloudXYZ(source, support::makeRigidTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateFusedStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFusedCandidate(source, target, &stats);

#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 8e-4f);
}

// `PointXYZI` 和 `PointXYZRGB` 都是生产入口里常见的 mixed-field（混合字段）xyz AoS
// 点型。这个 public-entry correctness（公开入口正确性）测试保护泛型 layout gate 不是
// exact `PointXYZ` 窄门；颜色和 intensity 仍不参与 SVD 数学语义。
TEST(TransformationEstimationSVD, PublicOrderedCloudPairRepresentativeLayoutsMatchFused)
{
  expectPublicOrderedCloudPairMatchesFusedReference(support::makePointXYZICloud(4096),
                                                   2e-3f);
  expectPublicOrderedCloudPairMatchesFusedReference(support::makePointXYZRGBCloud(4096),
                                                   2e-3f);
}

// RVV 构建下直接调用 production helper（生产内部 helper），确认代表性 xyz AoS 点型真的命中
// ordered-cloud-pair RVV path（顺序点云对 RVV 路径），而不是只通过数值结果间接猜测。
TEST(TransformationEstimationSVD, ProductionDirectRVVAcceptsRepresentativeXYZLayouts)
{
#ifdef __RVV10__
  expectProductionDirectRVVMatchesFusedReference(support::makePointXYZCloud(4096), 8e-4f);
  expectProductionDirectRVVMatchesFusedReference(support::makePointXYZICloud(4096), 2e-3f);
  expectProductionDirectRVVMatchesFusedReference(support::makePointXYZRGBCloud(4096), 2e-3f);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

// source-indexed production direct（源索引生产直连）必须真实命中 gather RVV helper。
// 这里覆盖 `PointXYZ` 以及 gate-allowed mixed-field xyz AoS 点型；性能仍只由板卡
// production summary 判断，不能从 correctness 外推出逐点型性能。
TEST(TransformationEstimationSVD, ProductionDirectRVVAcceptsSourceIndexedCloudPair)
{
#ifdef __RVV10__
  expectProductionSourceIndexedDirectRVVMatchesFusedReference(
      support::makePointXYZCloud(8193), 2e-3f);
  expectProductionSourceIndexedDirectRVVMatchesFusedReference(
      support::makePointXYZICloud(8193), 3e-3f);
  expectProductionSourceIndexedDirectRVVMatchesFusedReference(
      support::makePointXYZRGBCloud(8193), 3e-3f);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

// 这些 gate（门控条件）失败时必须自然回到原标量路径：小规模输入、非 dense、
// `use_umeyama_ == false` 和 `Scalar=double` 都不能误入当前 f32 ordered-cloud-pair RVV path。
TEST(TransformationEstimationSVD, ProductionDirectRVVRejectsOutOfScopeGates)
{
#ifdef __RVV10__
  const auto small = support::makePointXYZCloud(8);
  const auto small_target = support::transformCloudXYZ(small, support::makeRigidTransform());
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  const bool small_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(small, small_target, true, matrix);
  EXPECT_FALSE(small_used_rvv);

  auto non_dense = support::makePointXYZCloud(4096);
  auto non_dense_target =
      support::transformCloudXYZ(non_dense, support::makeRigidTransform());
  non_dense.is_dense = false;
  const bool non_dense_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(non_dense, non_dense_target, true, matrix);
  EXPECT_FALSE(non_dense_used_rvv);

  const auto regular = support::makePointXYZCloud(4096);
  const auto regular_target =
      support::transformCloudXYZ(regular, support::makeRigidTransform());
  const bool disabled_umeyama_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(regular, regular_target, false, matrix);
  EXPECT_FALSE(disabled_umeyama_used_rvv);

  Eigen::Matrix4d matrix_d = Eigen::Matrix4d::Identity();
  const bool double_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDOrderedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          double>(regular, regular_target, true, matrix_d);
  EXPECT_FALSE(double_used_rvv);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

// source-indexed RVV gather 使用 32-bit byte offset，因此生产 helper 进入 RVV 前必须检查
// 点云规模和每个 index 的合法性。这里直接调用 helper 确认失败 gate 不会误命中 RVV。
TEST(TransformationEstimationSVD, ProductionDirectRVVRejectsSourceIndexedOutOfScopeGates)
{
#ifdef __RVV10__
  const auto small_source = support::makePointXYZCloud(32);
  const auto small_indices = support::makeSourceIndices(small_source.size(), 8);
  const auto small_target = support::transformCloudXYZBySourceIndices(
      small_source, small_indices, support::makeRigidTransform());
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  const bool small_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDSourceIndexedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(small_source, small_indices, small_target, true, matrix);
  EXPECT_FALSE(small_used_rvv);

  auto regular_source = support::makePointXYZCloud(8193);
  const auto regular_indices = support::makeSourceIndices(regular_source.size(), 4096);
  const auto regular_target = support::transformCloudXYZBySourceIndices(
      regular_source, regular_indices, support::makeRigidTransform());

  regular_source.is_dense = false;
  const bool non_dense_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDSourceIndexedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(regular_source, regular_indices, regular_target, true, matrix);
  EXPECT_FALSE(non_dense_used_rvv);
  regular_source.is_dense = true;

  auto invalid_indices = regular_indices;
  invalid_indices[3] = -1;
  const bool invalid_index_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDSourceIndexedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(regular_source, invalid_indices, regular_target, true, matrix);
  EXPECT_FALSE(invalid_index_used_rvv);

  const bool disabled_umeyama_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDSourceIndexedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          float>(regular_source, regular_indices, regular_target, false, matrix);
  EXPECT_FALSE(disabled_umeyama_used_rvv);

  Eigen::Matrix4d matrix_d = Eigen::Matrix4d::Identity();
  const bool double_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDSourceIndexedCloudPairRVV<
          pcl::PointXYZ,
          pcl::PointXYZ,
          double>(regular_source, regular_indices, regular_target, true, matrix_d);
  EXPECT_FALSE(double_used_rvv);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

TEST(TransformationEstimationSVD, ProductionDirectRVVAcceptsDualIndicesCloudPair)
{
#ifdef __RVV10__
  expectProductionDualIndicesDirectRVVMatchesFusedReference(
      support::makePointXYZCloud(524289), 2e-3f);
  expectProductionDualIndicesDirectRVVMatchesFusedReference(
      support::makePointXYZICloud(524289), 3e-3f);
  expectProductionDualIndicesDirectRVVMatchesFusedReference(
      support::makePointXYZRGBCloud(524289), 3e-3f);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

TEST(TransformationEstimationSVD, ProductionDirectRVVAcceptsCorrespondencePair)
{
#ifdef __RVV10__
  expectProductionCorrespondenceDirectRVVMatchesFusedReference(
      support::makePointXYZCloud(524289), 2e-3f);
  expectProductionCorrespondenceDirectRVVMatchesFusedReference(
      support::makePointXYZICloud(524289), 3e-3f);
  expectProductionCorrespondenceDirectRVVMatchesFusedReference(
      support::makePointXYZRGBCloud(524289), 3e-3f);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

TEST(TransformationEstimationSVD, ProductionDirectRVVRejectsDualIndicesOutOfScopeGates)
{
#ifdef __RVV10__
  const auto small_source = support::makePointXYZCloud(8);
  const auto small_source_indices = support::makeSourceIndices(small_source.size(), 4);
  const auto small_target_indices = support::makeTargetIndices(small_source.size(), 4);
  const auto small_target = support::transformCloudXYZByIndexedPairs(
      small_source, small_source_indices, small_target_indices, support::makeRigidTransform());
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  const bool small_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDDualIndicesCloudPairRVV<pcl::PointXYZ,
                                                             pcl::PointXYZ,
                                                             float>(small_source,
                                                                    small_source_indices,
                                                                    small_target,
                                                                    small_target_indices,
                                                                    true,
                                                                    matrix);
  EXPECT_FALSE(small_used_rvv);

  auto regular_source = support::makePointXYZCloud(8193);
  const auto regular_source_indices = support::makeSourceIndices(regular_source.size(), 4096);
  const auto regular_target_indices = support::makeTargetIndices(regular_source.size(), 4096);
  auto regular_target = support::transformCloudXYZByIndexedPairs(
      regular_source,
      regular_source_indices,
      regular_target_indices,
      support::makeRigidTransform());
  regular_target.is_dense = false;
  const bool non_dense_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDDualIndicesCloudPairRVV<pcl::PointXYZ,
                                                             pcl::PointXYZ,
                                                             float>(regular_source,
                                                                    regular_source_indices,
                                                                    regular_target,
                                                                    regular_target_indices,
                                                                    true,
                                                                    matrix);
  EXPECT_FALSE(non_dense_used_rvv);

  auto invalid_source_indices = regular_source_indices;
  invalid_source_indices[3] = -1;
  const bool invalid_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDDualIndicesCloudPairRVV<pcl::PointXYZ,
                                                             pcl::PointXYZ,
                                                             float>(regular_source,
                                                                    invalid_source_indices,
                                                                    regular_target,
                                                                    regular_target_indices,
                                                                    true,
                                                                    matrix);
  EXPECT_FALSE(invalid_used_rvv);

  const bool disabled_umeyama_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDDualIndicesCloudPairRVV<pcl::PointXYZ,
                                                             pcl::PointXYZ,
                                                             float>(regular_source,
                                                                    regular_source_indices,
                                                                    regular_target,
                                                                    regular_target_indices,
                                                                    false,
                                                                    matrix);
  EXPECT_FALSE(disabled_umeyama_used_rvv);

  Eigen::Matrix4d matrix_d = Eigen::Matrix4d::Identity();
  const bool double_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDDualIndicesCloudPairRVV<pcl::PointXYZ,
                                                             pcl::PointXYZ,
                                                             double>(regular_source,
                                                                    regular_source_indices,
                                                                    regular_target,
                                                                    regular_target_indices,
                                                                    true,
                                                                    matrix_d);
  EXPECT_FALSE(double_used_rvv);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

TEST(TransformationEstimationSVD, ProductionDirectRVVRejectsCorrespondenceOutOfScopeGates)
{
#ifdef __RVV10__
  const auto small_source = support::makePointXYZCloud(8);
  const auto small_source_indices = support::makeSourceIndices(small_source.size(), 4);
  const auto small_target_indices = support::makeTargetIndices(small_source.size(), 4);
  const auto small_target = support::transformCloudXYZByIndexedPairs(
      small_source, small_source_indices, small_target_indices, support::makeRigidTransform());
  const auto small_correspondences =
      support::makeCorrespondences(small_source_indices, small_target_indices);
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  const bool small_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDCorrespondencePairRVV<pcl::PointXYZ,
                                                          pcl::PointXYZ,
                                                          float>(small_source,
                                                                 small_target,
                                                                 small_correspondences,
                                                                 true,
                                                                 matrix);
  EXPECT_FALSE(small_used_rvv);

  auto regular_source = support::makePointXYZCloud(8193);
  const auto regular_source_indices = support::makeSourceIndices(regular_source.size(), 4096);
  const auto regular_target_indices = support::makeTargetIndices(regular_source.size(), 4096);
  auto regular_target = support::transformCloudXYZByIndexedPairs(
      regular_source,
      regular_source_indices,
      regular_target_indices,
      support::makeRigidTransform());
  regular_target.is_dense = false;
  const auto regular_correspondences =
      support::makeCorrespondences(regular_source_indices, regular_target_indices);
  const bool non_dense_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDCorrespondencePairRVV<pcl::PointXYZ,
                                                          pcl::PointXYZ,
                                                          float>(regular_source,
                                                                 regular_target,
                                                                 regular_correspondences,
                                                                 true,
                                                                 matrix);
  EXPECT_FALSE(non_dense_used_rvv);

  auto invalid_correspondences = regular_correspondences;
  invalid_correspondences[3].index_match = -1;
  const bool invalid_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDCorrespondencePairRVV<pcl::PointXYZ,
                                                          pcl::PointXYZ,
                                                          float>(regular_source,
                                                                 regular_target,
                                                                 invalid_correspondences,
                                                                 true,
                                                                 matrix);
  EXPECT_FALSE(invalid_used_rvv);

  const bool disabled_umeyama_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDCorrespondencePairRVV<pcl::PointXYZ,
                                                          pcl::PointXYZ,
                                                          float>(regular_source,
                                                                 regular_target,
                                                                 regular_correspondences,
                                                                 false,
                                                                 matrix);
  EXPECT_FALSE(disabled_umeyama_used_rvv);

  Eigen::Matrix4d matrix_d = Eigen::Matrix4d::Identity();
  const bool double_used_rvv = pcl::registration::detail::
      estimateRigidTransformationSVDCorrespondencePairRVV<pcl::PointXYZ,
                                                          pcl::PointXYZ,
                                                          double>(regular_source,
                                                                 regular_target,
                                                                 regular_correspondences,
                                                                 true,
                                                                 matrix_d);
  EXPECT_FALSE(double_used_rvv);
#else
  SUCCEED() << "production RVV helper only exists in __RVV10__ builds";
#endif
}

// Degenerate reflection stress（退化反射压力样本）触发 determinant sign fix
// （行列式符号修正）附近的路径。它不代表常规 production dataset，只保护 SVD 后处理语义。
TEST(TransformationEstimationSVD, ReflectionStressMatchesPublicWithinBudget)
{
  auto source = support::makePointXYZCloud(2048);
  auto target = source;
  for (std::size_t i = 0; i < target.size(); ++i) {
    target[i].x = -source[i].x + 0.25f;
    target[i].y = source[i].y - 0.5f;
    target[i].z = source[i].z + 0.75f;
  }

  const Eigen::Matrix4f public_matrix = support::estimatePublicUmeyama(source, target);
  const Eigen::Matrix4f candidate_matrix = support::estimateFusedCandidate(source, target);

  expectMatrixNear(candidate_matrix, public_matrix, 2e-3f);
}
