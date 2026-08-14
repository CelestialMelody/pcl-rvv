/*
 * 本文件做什么：
 * 这些 gtest（单元测试）刻画 TransformationEstimation2D 的 public semantics
 * （公开入口语义），并验证 test-only fused 2D correlation accumulator（融合 2D
 * 相关项累加器）在顺序点云对（ordered-cloud-pair）上与标量参考链路一致。
 *
 * 证据边界：
 * 测试可以作为 QEMU correctness（QEMU 正确性）和本地回归证据；它不证明真实
 * production dispatch 已经存在，也不提供目标硬件性能结论。
 */

#include "te2d.h"

#include <pcl/test/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>

namespace support = pcl::registration::rvv_te2d_support;

namespace {

template <typename CandidateMatrix, typename ReferenceMatrix>
void
expectMatrixNear(const CandidateMatrix& candidate,
                 const ReferenceMatrix& reference,
                 const double epsilon)
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

void
expectMatrixExactlySame(const Eigen::Matrix4f& candidate,
                        const Eigen::Matrix4f& reference)
{
  ASSERT_EQ(candidate.rows(), reference.rows());
  ASSERT_EQ(candidate.cols(), reference.cols());
  for (int r = 0; r < candidate.rows(); ++r) {
    for (int c = 0; c < candidate.cols(); ++c) {
      EXPECT_EQ(candidate(r, c), reference(r, c))
          << "matrix mismatch at (" << r << ", " << c << ")";
    }
  }
}

template <typename PointT>
pcl::PointCloud<PointT>
selectByIndices(const pcl::PointCloud<PointT>& source, const pcl::Indices& indices)
{
  pcl::PointCloud<PointT> selected;
  selected.width = static_cast<std::uint32_t>(indices.size());
  selected.height = 1;
  selected.is_dense = source.is_dense;
  selected.resize(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    selected[i] = source[indices[i]];
  return selected;
}

pcl::Indices
makePrefixIndices(const std::size_t count)
{
  pcl::Indices indices(count);
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<pcl::index_t>(i);
  return indices;
}

pcl::Correspondences
makePrefixCorrespondences(const std::size_t count)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.0f);
  }
  return correspondences;
}

} // namespace

// 这个测试走真实 public ordered-cloud-pair 入口，保护正常 2D 刚体变换语义。
// 如果它失败，后续 fused 诊断即使自洽也不能说明和 production 当前行为一致。
TEST(TransformationEstimation2D, PublicOrderedCloudPairRecoversRigid2DTransform)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform();
  const auto target = support::transformCloud2D(source, expected);

  const Eigen::Matrix4f actual = support::estimatePublic2D(source, target);

  expectMatrixNear(actual, expected, 2e-4f);
  EXPECT_FLOAT_EQ(actual(2, 3), 0.0f);
}

// 数量不匹配时 full ordered-cloud-pair public overload 会打印 PCL_ERROR 并直接返回。
// 输出矩阵必须保持调用前状态；candidate 不能把这个边界改写成 identity。
TEST(TransformationEstimation2D, PublicOrderedCloudPairSizeMismatchKeepsOutputMatrix)
{
  const auto source = support::makePointXYZCloud(9);
  const auto target = support::makePointXYZCloud(8);
  Eigen::Matrix4f matrix;
  matrix.setConstant(42.0f);
  const Eigen::Matrix4f before = matrix;

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, target, matrix);

  expectMatrixExactlySame(matrix, before);
}

// source-indexed-cloud-pair（源索引点云对）入口先检查 indices_src 和 target 的数量。
// 当前阶段只刻画安全 size mismatch；非法 index 不是本测试的安全合同。
TEST(TransformationEstimation2D, PublicSourceIndexedSizeMismatchKeepsOutputMatrix)
{
  const auto source = support::makePointXYZCloud(9);
  const auto target = support::makePointXYZCloud(4);
  const pcl::Indices indices{0, 1, 2, 3, 4};
  Eigen::Matrix4f matrix;
  matrix.setConstant(-7.0f);
  const Eigen::Matrix4f before = matrix;

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, matrix);

  expectMatrixExactlySame(matrix, before);
}

// source-indexed-cloud-pair（源索引点云对）不属于 PI2 production RVV 范围。
// 这条 valid-case 测试保护该 public overload 仍按 `source[indices[i]]` 与 `target[i]`
// 配对，不能因为 ordered-cloud-pair 诊断或生产接入尝试而被隐式改写。
TEST(TransformationEstimation2D, PublicSourceIndexedValidCaseStaysOnScalarBoundary)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto indices = makePrefixIndices(1024);
  const auto selected = selectByIndices(source, indices);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.21f, 0.41f, -0.63f);
  const auto target = support::transformCloud2D(selected, expected);
  Eigen::Matrix4f actual = Eigen::Matrix4f::Identity();

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, actual);

  expectMatrixNear(actual, expected, 4e-4f);
}

// 这个测试把 source-indexed row source 物化后复用同一个 fused math pipeline。
// materialize 成本不属于 correctness 断言，但必须由后续 bench 计时；这里先证明
// candidate 和真实 public scalar overload 使用了相同的 source[indices[i]] / target[i] 行。
TEST(TransformationEstimation2D, SourceIndexedFusedCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto indices = makePrefixIndices(1024);
  const auto selected = selectByIndices(source, indices);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.21f, 0.41f, -0.63f);
  const auto target = support::transformCloud2D(selected, expected);

  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedCandidate(source, indices, target, &stats);

  EXPECT_EQ(stats.input_points, indices.size());
  EXPECT_EQ(stats.accepted_points, indices.size());
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 6e-4f);
}

// dual-indexed-cloud-pair（双索引点云对）入口只在两侧 index 数量不等时安全返回。
// 有效但乱序 / 重复 index 的行来源证据留给后续 row-source carry-over phase。
TEST(TransformationEstimation2D, PublicDualIndexedSizeMismatchKeepsOutputMatrix)
{
  const auto source = support::makePointXYZCloud(9);
  const auto target = support::makePointXYZCloud(9);
  const pcl::Indices source_indices{0, 1, 2, 3, 4};
  const pcl::Indices target_indices{0, 1, 2, 3};
  Eigen::Matrix4f matrix;
  matrix.setConstant(13.0f);
  const Eigen::Matrix4f before = matrix;

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, matrix);

  expectMatrixExactlySame(matrix, before);
}

// dual-indexed-cloud-pair 同样保持标量边界。本测试只证明 public row pairing
// 仍可恢复 2D 变换；是否值得为双侧 gather 做 RVV 留给 row-source carry-over phase。
TEST(TransformationEstimation2D, PublicDualIndexedValidCaseStaysOnScalarBoundary)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.18f, 0.75f, 0.29f);
  const auto target = support::transformCloud2D(source, expected);
  const auto indices = makePrefixIndices(1536);
  Eigen::Matrix4f actual = Eigen::Matrix4f::Identity();

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, indices, actual);

  expectMatrixNear(actual, expected, 5e-4f);
}

// 双侧 row source 使用相同的合法索引，先物化 source / target，再进入 fused candidate。
// 这个测试只证明数学行配对和 public scalar overload 一致，不证明 gather 已经适合 production。
TEST(TransformationEstimation2D, DualIndexedFusedCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.18f, 0.75f, 0.29f);
  const auto target = support::transformCloud2D(source, expected);
  const auto indices = makePrefixIndices(1536);

  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, indices, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedCandidate(
          source, indices, target, indices, &stats);

  EXPECT_EQ(stats.input_points, indices.size());
  EXPECT_EQ(stats.accepted_points, indices.size());
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 7e-4f);
}

// correspondence-pair（对应关系点对）入口不属于 ordered-cloud-pair 诊断候选。
// 这里用 identity correspondence 只锁住 public 语义，避免后续误触 query / match 路径。
TEST(TransformationEstimation2D, PublicCorrespondencePairValidCaseStaysOnScalarBoundary)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.09f, -0.24f, 0.52f);
  const auto target = support::transformCloud2D(source, expected);
  const auto correspondences = makePrefixCorrespondences(2048);
  Eigen::Matrix4f actual = Eigen::Matrix4f::Identity();

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, target, correspondences, actual);

  expectMatrixNear(actual, expected, 5e-4f);
}

// correspondence row source 先展开 query / match，再复用 ordered fused candidate。
// 这里保留 identity correspondence，避免把 correspondence 生成策略和数学 candidate 混在一起。
TEST(TransformationEstimation2D, CorrespondenceFusedCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.09f, -0.24f, 0.52f);
  const auto target = support::transformCloud2D(source, expected);
  const auto correspondences = makePrefixCorrespondences(2048);

  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceCandidate(
          source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 7e-4f);
}

// 当前标量 production 链路的 centroid（质心）计算会过滤非有限点，但 demean matrix
// （去中心化矩阵）仍写出 iterator 中每一行。x/y 非有限值会进入 2D correlation，并传播为
// 非有限矩阵；本阶段只记录该行为，不把它当作 RVV 要修复的 bug。
TEST(TransformationEstimation2D, PublicNonFiniteXYProducesNonFiniteMatrix)
{
  auto source = support::makePointXYZCloud(64);
  const auto target = support::transformCloud2D(source, support::makeRigid2DTransform());
  source.is_dense = false;
  source[7].x = std::numeric_limits<float>::quiet_NaN();

  const Eigen::Matrix4f matrix = support::estimatePublic2D(source, target);

  EXPECT_FALSE(matrix.allFinite());
}

// z 非有限值会影响 centroid 的 accepted rows，但 2D angle 只使用 x/y correlation。
// candidate 因此必须把这类输入退回 public path，不能只因为 x/y 有限就走 dense fused path。
TEST(TransformationEstimation2D, PublicNonFiniteZForcesCandidateFallback)
{
  auto source = support::makePointXYZCloud(128);
  auto target = support::transformCloud2D(source, support::makeRigid2DTransform());
  source.is_dense = false;
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix = support::estimatePublic2D(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &stats);

  EXPECT_TRUE(public_matrix.allFinite());
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.source_finite_points, source.size() - 1);
  expectMatrixNear(candidate_matrix, public_matrix, 1e-6f);
}

// 标量 fused reference 先求 x/y 质心，再直接累加中心化后的 2x2 correlation。
// 它必须先对齐当前 public ordered-cloud-pair 入口，后续 RVV reduction tree 才有可信 reference。
TEST(TransformationEstimation2D, FusedStdMatchesPublicOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto target = support::transformCloud2D(source, support::makeRigid2DTransform());

  const Eigen::Matrix4f public_matrix = support::estimatePublic2D(source, target);
  const Eigen::Matrix4f fused_matrix = support::estimateFused2DStd(source, target);

  expectMatrixNear(fused_matrix, public_matrix, 4e-4f);
}

// RVV candidate（RVV 候选链路）只替换 dense finite ordered-cloud-pair 的累加前端。
// 这条测试保护 reduction tree（规约树）和标量 fused reference 的误差预算。
TEST(TransformationEstimation2D, FusedCandidateMatchesScalarOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target = support::transformCloud2D(source, support::makeRigid2DTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      support::estimateFused2DStd(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_TRUE(candidate_stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 6e-4f);
}

// 小规模输入走标量 fallback（回退路径）。这里保护的是 test-only candidate gate，
// 不是 production 公开入口的新语义。
TEST(TransformationEstimation2D, FusedCandidateSmallInputFallsBack)
{
  const auto source = support::makePointXYZCloud(8);
  const auto target = support::transformCloud2D(source, support::makeRigid2DTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateFused2DStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

// near-cancellation（近抵消）样本让中心化 correlation 在较大公共偏移上工作。
// 它不证明生产性能，只保护 Phase 020 前需要保留的数值预算。
TEST(TransformationEstimation2D, FusedCandidateNearCancellationWithinBudget)
{
  const auto source = support::makeNearCancellationCloud(2048);
  const auto target = support::transformCloud2D(source, support::makeRigid2DTransform(0.12f));

  const Eigen::Matrix4f std_matrix = support::estimateFused2DStd(source, target);
  const Eigen::Matrix4f candidate_matrix = support::estimateFused2DCandidate(source, target);

  expectMatrixNear(candidate_matrix, std_matrix, 5e-3f);
}
