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
#include <string>
#include <type_traits>

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

pcl::Indices
makeStridedIndices(const std::size_t count,
                   const std::size_t storage_count,
                   const std::size_t stride = 3,
                   const std::size_t offset = 5)
{
  pcl::Indices indices(count);
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<pcl::index_t>((i * stride + offset) % storage_count);
  return indices;
}

pcl::Indices
makeReverseIndices(const std::size_t count)
{
  pcl::Indices indices(count);
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<pcl::index_t>(count - i - 1);
  return indices;
}

pcl::Indices
makeShuffledIndices(const std::size_t count,
                    const std::uint64_t multiplier,
                    const std::uint64_t increment)
{
  pcl::Indices indices(count);
  for (std::size_t i = 0; i < count; ++i) {
    const auto mixed =
        multiplier * static_cast<std::uint64_t>(i) + increment;
    indices[i] = static_cast<pcl::index_t>(mixed % count);
  }
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

pcl::Correspondences
makeStridedCorrespondences(const pcl::Indices& query_indices,
                           const pcl::Indices& match_indices)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(query_indices.size());
  for (std::size_t i = 0; i < query_indices.size(); ++i) {
    correspondences.emplace_back(
        static_cast<int>(query_indices[i]), static_cast<int>(match_indices[i]), 0.0f);
  }
  return correspondences;
}

template <typename PointSource, typename PointTarget>
pcl::PointCloud<PointTarget>
makeTargetForIndexedPairs(const pcl::PointCloud<PointSource>& source,
                          const pcl::Indices& source_indices,
                          const pcl::Indices& target_indices,
                          const Eigen::Matrix4f& transform,
                          const float target_seed = -5.0f)
{
  pcl::PointCloud<PointTarget> target;
  if constexpr (std::is_same_v<PointTarget, pcl::PointXYZ>) {
    target = support::makePointXYZCloud(source.size());
  }
  else {
    target = support::makeXYZLikeCloud<PointTarget>(source.size(), target_seed);
  }

  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto source_index = static_cast<std::size_t>(source_indices[i]);
    const auto target_index = static_cast<std::size_t>(target_indices[i]);
    const Eigen::Vector4f p(source[source_index].x,
                            source[source_index].y,
                            source[source_index].z,
                            1.0f);
    const Eigen::Vector4f q = transform * p;
    target[target_index].x = q.x();
    target[target_index].y = q.y();
    target[target_index].z = source[source_index].z;
  }
  return target;
}

template <typename PointT>
void
expectXYZLikeTraits(const char* label)
{
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
  using Pod = typename pcl::traits::POD<PointT>::type;
  constexpr std::size_t kLayoutX = Layout::kX;
  constexpr std::size_t kLayoutY = Layout::kY;
  constexpr std::size_t kLayoutZ = Layout::kZ;
  constexpr std::size_t kTraitsX = pcl::traits::offset<PointT, pcl::fields::x>::value;
  constexpr std::size_t kTraitsY = pcl::traits::offset<PointT, pcl::fields::y>::value;
  constexpr std::size_t kTraitsZ = pcl::traits::offset<PointT, pcl::fields::z>::value;

  SCOPED_TRACE(label);
  static_assert(pcl::traits::has_xyz<PointT>::value,
                "representative generic point type must expose xyz traits");
  static_assert(Layout::value,
                "representative generic point type must satisfy the AoS float gate");
  EXPECT_TRUE(Layout::value);
  EXPECT_TRUE((pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::x>::value));
  EXPECT_TRUE((pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::y>::value));
  EXPECT_TRUE((pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::z>::value));
  EXPECT_EQ(kLayoutX, kTraitsX);
  EXPECT_EQ(kLayoutY, kTraitsY);
  EXPECT_EQ(kLayoutZ, kTraitsZ);
  EXPECT_EQ(sizeof(PointT), sizeof(Pod));
  EXPECT_TRUE(std::is_standard_layout_v<Pod>);
  EXPECT_EQ(sizeof(PointT) % alignof(float), 0u);
  EXPECT_EQ(kLayoutX % alignof(float), 0u);
  EXPECT_EQ(kLayoutY % alignof(float), 0u);
  EXPECT_EQ(kLayoutZ % alignof(float), 0u);
}

template <typename PointSource, typename PointTarget>
void
expectGenericXYZPairMatchesReferences(const float candidate_epsilon = 7e-4f)
{
  constexpr std::size_t kPointCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.29f, -0.47f, 0.91f);
  const auto source = support::makeXYZLikeCloud<PointSource>(kPointCount, 3.0f);
  const auto target =
      support::transformCloud2DTo<PointSource, PointTarget>(source, expected, -8.0f);

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix = support::estimatePublic2D(source, target);
  const Eigen::Matrix4f std_matrix = support::estimateFused2DStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &stats);

  EXPECT_TRUE(stats.source_layout_supported);
  EXPECT_TRUE(stats.target_layout_supported);
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, kPointCount);
  EXPECT_EQ(stats.accepted_points, kPointCount);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(public_matrix, expected, 6e-4f);
  expectMatrixNear(std_matrix, public_matrix, 5e-4f);
  expectMatrixNear(candidate_matrix, std_matrix, candidate_epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectPublicGenericXYZFallbackMatchesDouble(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const double epsilon)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float>
      float_estimator;
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, double>
      double_estimator;
  Eigen::Matrix4f float_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();

  float_estimator.estimateRigidTransformation(source, target, float_matrix);
  double_estimator.estimateRigidTransformation(source, target, double_matrix);

  EXPECT_TRUE(float_matrix.allFinite());
  EXPECT_TRUE(double_matrix.allFinite());
  expectMatrixNear(float_matrix, double_matrix, epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectPublicSourceIndexedMatchesDouble(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const double epsilon)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float>
      float_estimator;
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, double>
      double_estimator;
  Eigen::Matrix4f float_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();

  float_estimator.estimateRigidTransformation(
      source, source_indices, target, float_matrix);
  double_estimator.estimateRigidTransformation(
      source, source_indices, target, double_matrix);

  EXPECT_TRUE(float_matrix.allFinite());
  EXPECT_TRUE(double_matrix.allFinite());
  expectMatrixNear(float_matrix, double_matrix, epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectPublicSourceIndexedGenericXYZPairMatchesExpected(
    const double epsilon = 8e-4)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.24f, -0.37f, 0.68f);
  const auto source =
      support::makeXYZLikeCloud<PointSource>(kStorageCount, 3.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<PointSource, PointTarget>(
          selected, expected, -8.0f);

  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float>
      float_estimator;
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, double>
      double_estimator;
  Eigen::Matrix4f float_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();
  float_estimator.estimateRigidTransformation(
      source, source_indices, target, float_matrix);
  double_estimator.estimateRigidTransformation(
      source, source_indices, target, double_matrix);

  EXPECT_TRUE(float_matrix.allFinite());
  EXPECT_TRUE(double_matrix.allFinite());
  expectMatrixNear(float_matrix, expected, epsilon);
  expectMatrixNear(float_matrix, double_matrix, epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectSourceIndexedGenericXYZPairMatchesReferences(
    const float candidate_epsilon = 8e-4f)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.24f, -0.37f, 0.68f);
  const auto source =
      support::makeXYZLikeCloud<PointSource>(kStorageCount, 3.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<PointSource, PointTarget>(
          selected, expected, -8.0f);

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix =
      support::estimatePublicSourceIndexed2D(source, source_indices, target);
  const Eigen::Matrix4f std_matrix =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DSourceIndexedDirectStd(
              source, source_indices, target));
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, source_indices, target, &stats);

  EXPECT_TRUE(stats.source_layout_supported);
  EXPECT_TRUE(stats.target_layout_supported);
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, kRowCount);
  EXPECT_EQ(stats.accepted_points, kRowCount);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(public_matrix, expected, 7e-4f);
  expectMatrixNear(std_matrix, public_matrix, 6e-4f);
  expectMatrixNear(candidate_matrix, std_matrix, candidate_epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectPublicDualIndexedMatchesDouble(const pcl::PointCloud<PointSource>& source,
                                     const pcl::Indices& source_indices,
                                     const pcl::PointCloud<PointTarget>& target,
                                     const pcl::Indices& target_indices,
                                     const double epsilon)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float>
      float_estimator;
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, double>
      double_estimator;
  Eigen::Matrix4f float_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();

  float_estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, float_matrix);
  double_estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, double_matrix);

  EXPECT_TRUE(float_matrix.allFinite());
  EXPECT_TRUE(double_matrix.allFinite());
  expectMatrixNear(float_matrix, double_matrix, epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectDualIndexedGenericXYZPairMatchesReferences(
    const float candidate_epsilon = 9e-4f)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.21f, 0.54f, -0.73f);
  const auto source =
      support::makeXYZLikeCloud<PointSource>(kStorageCount, 9.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto target_indices =
      makeStridedIndices(kRowCount, source.size(), 5, 7);
  const auto target =
      makeTargetForIndexedPairs<PointSource, PointTarget>(
          source, source_indices, target_indices, expected, -9.0f);

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix =
      support::estimatePublicDualIndexed2D(
          source, source_indices, target, target_indices);
  const Eigen::Matrix4f std_matrix =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DDualIndexedDirectStd(
              source, source_indices, target, target_indices));
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_TRUE(stats.source_layout_supported);
  EXPECT_TRUE(stats.target_layout_supported);
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, kRowCount);
  EXPECT_EQ(stats.accepted_points, kRowCount);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(public_matrix, expected, 7e-4f);
  expectMatrixNear(std_matrix, public_matrix, 6e-4f);
  expectMatrixNear(candidate_matrix, std_matrix, candidate_epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectCorrespondenceGenericXYZPairMatchesReferences(
    const float candidate_epsilon = 9e-4f)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.19f, -0.58f, 0.64f);
  const auto source =
      support::makeXYZLikeCloud<PointSource>(kStorageCount, 12.0f);
  const auto query_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto match_indices =
      makeStridedIndices(kRowCount, source.size(), 5, 7);
  const auto target =
      makeTargetForIndexedPairs<PointSource, PointTarget>(
          source, query_indices, match_indices, expected, -12.0f);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix =
      support::estimatePublicCorrespondence2D(source, target, correspondences);
  const Eigen::Matrix4f std_matrix =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DCorrespondenceDirectStd(
              source, target, correspondences));
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);

  EXPECT_TRUE(stats.source_layout_supported);
  EXPECT_TRUE(stats.target_layout_supported);
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, kRowCount);
  EXPECT_EQ(stats.accepted_points, kRowCount);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(public_matrix, expected, 7e-4f);
  expectMatrixNear(std_matrix, public_matrix, 6e-4f);
  expectMatrixNear(candidate_matrix, std_matrix, candidate_epsilon);
}

template <typename PointSource, typename PointTarget>
void
expectPublicCorrespondenceMatchesDouble(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    const double epsilon)
{
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, float>
      float_estimator;
  pcl::registration::TransformationEstimation2D<PointSource, PointTarget, double>
      double_estimator;
  Eigen::Matrix4f float_matrix = Eigen::Matrix4f::Identity();
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();

  float_estimator.estimateRigidTransformation(
      source, target, correspondences, float_matrix);
  double_estimator.estimateRigidTransformation(
      source, target, correspondences, double_matrix);

  EXPECT_TRUE(float_matrix.allFinite());
  EXPECT_TRUE(double_matrix.allFinite());
  expectMatrixNear(float_matrix, double_matrix, epsilon);
}

} // namespace

TEST(TransformationEstimation2D, GenericXYZTraitsGateCoversRepresentativePointTypes)
{
  expectXYZLikeTraits<pcl::PointXYZ>("PointXYZ");
  expectXYZLikeTraits<pcl::PointXYZI>("PointXYZI");
  expectXYZLikeTraits<pcl::PointNormal>("PointNormal");
  expectXYZLikeTraits<pcl::PointXYZINormal>("PointXYZINormal");
}

TEST(TransformationEstimation2D, GenericXYZCandidateMatchesRepresentativeSameTypePairs)
{
  expectGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZ>();
  expectGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZI>();
  expectGenericXYZPairMatchesReferences<pcl::PointNormal, pcl::PointNormal>();
  expectGenericXYZPairMatchesReferences<pcl::PointXYZINormal, pcl::PointXYZINormal>();
}

TEST(TransformationEstimation2D, GenericXYZCandidateMatchesMixedSourceTargetPairs)
{
  expectGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZ>();
  expectGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZI>();
  expectGenericXYZPairMatchesReferences<pcl::PointNormal, pcl::PointXYZINormal>();
  expectGenericXYZPairMatchesReferences<pcl::PointXYZINormal, pcl::PointNormal>();
}

TEST(TransformationEstimation2D, GenericXYZCandidateIgnoresExtraFields)
{
  constexpr std::size_t kPointCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.16f, 0.58f, -0.32f);
  const auto source_a =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kPointCount, 1.0f);
  const auto source_b =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kPointCount, 101.0f);
  const auto target_a =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          source_a, expected, 5.0f);
  const auto target_b =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          source_b, expected, 205.0f);

  const Eigen::Matrix4f matrix_a =
      support::estimateFused2DCandidate(source_a, target_a);
  const Eigen::Matrix4f matrix_b =
      support::estimateFused2DCandidate(source_b, target_b);

  expectMatrixNear(matrix_a, expected, 7e-4f);
  expectMatrixNear(matrix_b, expected, 7e-4f);
  expectMatrixNear(matrix_a, matrix_b, 1e-6f);
}

TEST(TransformationEstimation2D, GenericXYZCandidateSmallInputFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(8, 4.0f);
  const auto target =
      support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZINormal>(
          source, support::makeRigid2DTransform(), -4.0f);

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix = support::estimateFused2DStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &stats);

  EXPECT_TRUE(stats.source_layout_supported);
  EXPECT_TRUE(stats.target_layout_supported);
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

TEST(TransformationEstimation2D, PublicGenericXYZSmallInputUsesScalarFallback)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(8, 4.0f);
  const auto target =
      support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZINormal>(
          source, support::makeRigid2DTransform(), -4.0f);

  expectPublicGenericXYZFallbackMatchesDouble(source, target, 2e-4);
}

TEST(TransformationEstimation2D, GenericXYZCandidateNonDenseFiniteInputFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointNormal>(128, 2.0f);
  auto target =
      support::transformCloud2DTo<pcl::PointNormal, pcl::PointXYZI>(
          source, support::makeRigid2DTransform(0.22f), -2.0f);
  source.is_dense = false;
  target.is_dense = false;

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix = support::estimatePublic2D(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.source_finite_points, source.size());
  EXPECT_EQ(stats.target_finite_points, target.size());
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, public_matrix, 1e-6f);
}

TEST(TransformationEstimation2D, PublicGenericXYZNonDenseFiniteUsesScalarFallback)
{
  auto source = support::makeXYZLikeCloud<pcl::PointNormal>(128, 2.0f);
  auto target =
      support::transformCloud2DTo<pcl::PointNormal, pcl::PointXYZI>(
          source, support::makeRigid2DTransform(0.22f), -2.0f);
  source.is_dense = false;
  target.is_dense = false;

  expectPublicGenericXYZFallbackMatchesDouble(source, target, 2e-4);
}

TEST(TransformationEstimation2D, GenericXYZCandidateNonFiniteInputFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointXYZINormal>(128, 7.0f);
  auto target =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          source, support::makeRigid2DTransform(0.22f), -7.0f);
  source.is_dense = false;
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f public_matrix = support::estimatePublic2D(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCandidate(source, target, &stats);

  EXPECT_TRUE(public_matrix.allFinite());
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.source_finite_points, source.size() - 1);
  EXPECT_EQ(stats.target_finite_points, target.size());
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, public_matrix, 1e-6f);
}

TEST(TransformationEstimation2D, PublicGenericXYZNonFiniteUsesScalarFallback)
{
  auto source = support::makeXYZLikeCloud<pcl::PointXYZINormal>(128, 7.0f);
  auto target =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          source, support::makeRigid2DTransform(0.22f), -7.0f);
  source.is_dense = false;
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  expectPublicGenericXYZFallbackMatchesDouble(source, target, 2e-4);
}

TEST(TransformationEstimation2D, GenericXYZPublicDoubleScalarUsesFallbackBoundary)
{
  constexpr std::size_t kPointCount = 4096;
  const Eigen::Matrix4f expected_float =
      support::makeRigid2DTransform(0.18f, -0.41f, 0.73f);
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(kPointCount, 6.0f);
  const auto target =
      support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZINormal>(
          source, expected_float, -6.0f);

  pcl::registration::TransformationEstimation2D<
      pcl::PointXYZI, pcl::PointXYZINormal, double>
      estimator;
  Eigen::Matrix4d actual = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, actual);

  const Eigen::Matrix4d expected = expected_float.cast<double>();
  expectMatrixNear(actual, expected, 7e-4);
}

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

// Phase 091 source-indexed production probe（源索引生产探针）只接入合法
// `PointXYZ -> PointXYZ` / `Scalar=float` 公开入口。本测试保护真实 public overload
// 的 `source[indices[i]]` / `target[i]` 配对和 2D 刚体结果。
TEST(TransformationEstimation2D, PublicSourceIndexedProductionProbeRecoversRigid2DTransform)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto indices = makeStridedIndices(1536, source.size(), 3, 5);
  const auto selected = selectByIndices(source, indices);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.21f, 0.41f, -0.63f);
  const auto target = support::transformCloud2D(selected, expected);
  Eigen::Matrix4f actual = Eigen::Matrix4f::Identity();

  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, actual);

  expectMatrixNear(actual, expected, 4e-4f);
}

// Phase 092 family A/B 的 materialize baseline 先把 source[indices[i]]
// 连续化，再调用真实 ordered-cloud-pair public overload。这里先锁住它与当前
// source-indexed public direct path 的输出一致；否则后续 board B/A 不能解释为性能差异。
TEST(TransformationEstimation2D, PublicSourceIndexedFamilyMaterializedOrderedMatchesDirectPublic)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto indices = makeStridedIndices(4096, source.size(), 3, 5);
  const auto selected = selectByIndices(source, indices);
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.19f, 0.57f, -0.43f);
  const auto target = support::transformCloud2D(selected, expected);

  const Eigen::Matrix4f direct =
      support::estimatePublicSourceIndexed2D(source, indices, target);
  const Eigen::Matrix4f materialized =
      support::estimatePublicSourceIndexedMaterializedOrdered2D(
          source, indices, target);

  expectMatrixNear(direct, expected, 4e-4f);
  expectMatrixNear(materialized, direct, 1e-6f);
  EXPECT_EQ(support::matrixChecksum(materialized), support::matrixChecksum(direct))
      << "max_abs_diff=" << support::matrixMaxAbsDiff(materialized, direct);
}

// 小规模 source-indexed 输入必须保持既有 iterator 标量 fallback。这里不要求
// RVV 命中，只要求 public float overload 与同入口 double 标量链路一致。
TEST(TransformationEstimation2D, PublicSourceIndexedSmallInputUsesScalarFallback)
{
  const auto source = support::makePointXYZCloud(16);
  const auto indices = makeStridedIndices(8, source.size(), 3, 1);
  const auto selected = selectByIndices(source, indices);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.12f, 0.36f, 0.44f);
  const auto target = support::transformCloud2D(selected, expected);

  expectPublicSourceIndexedMatchesDouble(source, indices, target, 2e-4);
}

// non-dense 但有限的 selected rows 要回到 public 标量路径；这保护 fallback
// 语义，而不是把 Phase 091 的 RVV gate 扩大到非 dense 输入。
TEST(TransformationEstimation2D, PublicSourceIndexedNonDenseFiniteUsesScalarFallback)
{
  auto source = support::makePointXYZCloud(128);
  const auto indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto selected = selectByIndices(source, indices);
  auto target = support::transformCloud2D(
      selected, support::makeRigid2DTransform(0.17f, -0.25f, 0.62f));
  source.is_dense = false;
  target.is_dense = false;

  expectPublicSourceIndexedMatchesDouble(source, indices, target, 2e-4);
}

// selected source row 中出现非有限 z 时，Phase 091 helper 必须 fallback。
// 当前 public 标量链路的 2D 结果仍为有限矩阵，因此用 double 标量链路锁住语义。
TEST(TransformationEstimation2D, PublicSourceIndexedNonFiniteSelectedZUsesScalarFallback)
{
  auto source = support::makePointXYZCloud(128);
  const auto indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto selected = selectByIndices(source, indices);
  const auto target = support::transformCloud2D(
      selected, support::makeRigid2DTransform(0.11f, 0.42f, -0.18f));
  source.is_dense = false;
  source[static_cast<std::size_t>(indices[7])].z =
      std::numeric_limits<float>::quiet_NaN();

  expectPublicSourceIndexedMatchesDouble(source, indices, target, 3e-4);
}

// Phase 103 扩大 source-indexed production probe 后，代表性 PointXYZ-like
// 模板实例应通过真实 public overload 保持与 double scalar reference 一致。
TEST(TransformationEstimation2D, PublicSourceIndexedGenericXYZMatchesExpected)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(256, 3.0f);
  const auto indices = makeStridedIndices(96, source.size(), 5, 7);
  const auto selected = selectByIndices(source, indices);
  const auto target = support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZI>(
      selected, support::makeRigid2DTransform(-0.09f, 0.27f, -0.51f), -3.0f);

  expectPublicSourceIndexedMatchesDouble(source, indices, target, 2e-4);
}

TEST(TransformationEstimation2D, PublicSourceIndexedGenericXYZMatchesSameTypePairs)
{
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<pcl::PointXYZ,
                                                         pcl::PointXYZ>();
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<pcl::PointXYZI,
                                                         pcl::PointXYZI>();
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<pcl::PointNormal,
                                                         pcl::PointNormal>();
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<
      pcl::PointXYZINormal,
      pcl::PointXYZINormal>();
}

TEST(TransformationEstimation2D, PublicSourceIndexedGenericXYZMatchesMixedPairs)
{
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<pcl::PointXYZI,
                                                         pcl::PointXYZ>();
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<pcl::PointXYZ,
                                                         pcl::PointXYZI>();
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<pcl::PointNormal,
                                                         pcl::PointXYZINormal>();
  expectPublicSourceIndexedGenericXYZPairMatchesExpected<
      pcl::PointXYZINormal,
      pcl::PointNormal>();
}

TEST(TransformationEstimation2D,
     PublicSourceIndexedGenericXYZIgnoresExtraFields)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.15f, 0.52f, -0.29f);
  const auto source_a =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 1.0f);
  const auto source_b =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 101.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source_a.size(), 5, 7);
  const auto selected_a = selectByIndices(source_a, source_indices);
  const auto selected_b = selectByIndices(source_b, source_indices);
  const auto target_a =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          selected_a, expected, 5.0f);
  const auto target_b =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          selected_b, expected, 205.0f);

  pcl::registration::TransformationEstimation2D<
      pcl::PointXYZINormal,
      pcl::PointNormal,
      float>
      estimator;
  Eigen::Matrix4f matrix_a = Eigen::Matrix4f::Identity();
  Eigen::Matrix4f matrix_b = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(
      source_a, source_indices, target_a, matrix_a);
  estimator.estimateRigidTransformation(
      source_b, source_indices, target_b, matrix_b);

  expectMatrixNear(matrix_a, expected, 8e-4f);
  expectMatrixNear(matrix_b, expected, 8e-4f);
  expectMatrixNear(matrix_a, matrix_b, 1e-6f);
}

TEST(TransformationEstimation2D,
     PublicSourceIndexedGenericXYZNonDenseTargetUsesScalarFallback)
{
  auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(128, 7.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto selected = selectByIndices(source, source_indices);
  auto target =
      support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZINormal>(
          selected, support::makeRigid2DTransform(0.22f), -7.0f);
  target.is_dense = false;

  expectPublicSourceIndexedMatchesDouble(source, source_indices, target, 3e-4);
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZCandidateMatchesSameTypePairs)
{
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZ>();
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZI>();
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointNormal, pcl::PointNormal>();
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointXYZINormal,
                                                     pcl::PointXYZINormal>();
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZCandidateMatchesMixedPairs)
{
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZ>();
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZI>();
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointNormal,
                                                     pcl::PointXYZINormal>();
  expectSourceIndexedGenericXYZPairMatchesReferences<pcl::PointXYZINormal,
                                                     pcl::PointNormal>();
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZCandidateIgnoresExtraFields)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.15f, 0.52f, -0.29f);
  const auto source_a =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 1.0f);
  const auto source_b =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 101.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source_a.size(), 5, 7);
  const auto selected_a = selectByIndices(source_a, source_indices);
  const auto selected_b = selectByIndices(source_b, source_indices);
  const auto target_a =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          selected_a, expected, 5.0f);
  const auto target_b =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          selected_b, expected, 205.0f);

  const Eigen::Matrix4f matrix_a =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source_a, source_indices, target_a);
  const Eigen::Matrix4f matrix_b =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source_b, source_indices, target_b);

  expectMatrixNear(matrix_a, expected, 8e-4f);
  expectMatrixNear(matrix_b, expected, 8e-4f);
  expectMatrixNear(matrix_a, matrix_b, 1e-6f);
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZSmallInputFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(32, 4.0f);
  const auto source_indices = makePrefixIndices(8);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZINormal>(
          selected, support::makeRigid2DTransform(), -4.0f);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, source_indices, target, &stats);
  const Eigen::Matrix4f reference =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DSourceIndexedDirectStd(
              source, source_indices, target));

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(candidate_matrix, reference, 2e-4f);
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZNonDenseFiniteFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointNormal>(128, 2.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto selected = selectByIndices(source, source_indices);
  auto target =
      support::transformCloud2DTo<pcl::PointNormal, pcl::PointXYZINormal>(
          selected, support::makeRigid2DTransform(0.22f), -2.0f);
  source.is_dense = false;
  target.is_dense = false;

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, source_indices, target, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZNonFiniteSelectedSourceFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointXYZINormal>(128, 7.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
          selected, support::makeRigid2DTransform(0.22f), -7.0f);
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, source_indices, target, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.source_finite_points, source_indices.size() - 1);
  EXPECT_EQ(stats.target_finite_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, SourceIndexedGenericXYZNonFiniteTargetPrefixFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(128, 7.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto selected = selectByIndices(source, source_indices);
  auto target =
      support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZINormal>(
          selected, support::makeRigid2DTransform(0.22f), -7.0f);
  target[9].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, source_indices, target, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.source_finite_points, source_indices.size());
  EXPECT_EQ(stats.target_finite_points, source_indices.size() - 1);
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, PublicSourceIndexedGenericXYZDoubleUsesScalarBoundary)
{
  constexpr std::size_t kStorageCount = 256;
  constexpr std::size_t kRowCount = 96;
  const Eigen::Matrix4f expected_float =
      support::makeRigid2DTransform(0.18f, -0.41f, 0.73f);
  const auto source =
      support::makeXYZLikeCloud<pcl::PointNormal>(kStorageCount, 6.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<pcl::PointNormal, pcl::PointXYZINormal>(
          selected, expected_float, -6.0f);

  pcl::registration::TransformationEstimation2D<
      pcl::PointNormal, pcl::PointXYZINormal, double>
      estimator;
  Eigen::Matrix4d actual = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, source_indices, target, actual);

  expectMatrixNear(actual, expected_float.cast<double>(), 7e-4);
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZCandidateMatchesSameTypePairs)
{
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZ>();
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZI>();
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointNormal, pcl::PointNormal>();
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointXYZINormal,
                                                   pcl::PointXYZINormal>();
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZCandidateMatchesMixedPairs)
{
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZ>();
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZI>();
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointNormal,
                                                   pcl::PointXYZINormal>();
  expectDualIndexedGenericXYZPairMatchesReferences<pcl::PointXYZINormal,
                                                   pcl::PointNormal>();
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZCandidateIgnoresExtraFields)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.17f, -0.49f, 0.36f);
  const auto source_a =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 1.0f);
  const auto source_b =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 101.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source_a.size(), 5, 7);
  const auto target_indices =
      makeStridedIndices(kRowCount, source_a.size(), 7, 11);
  const auto target_a =
      makeTargetForIndexedPairs<pcl::PointXYZINormal, pcl::PointNormal>(
          source_a, source_indices, target_indices, expected, 5.0f);
  const auto target_b =
      makeTargetForIndexedPairs<pcl::PointXYZINormal, pcl::PointNormal>(
          source_b, source_indices, target_indices, expected, 205.0f);

  const Eigen::Matrix4f matrix_a =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source_a, source_indices, target_a, target_indices);
  const Eigen::Matrix4f matrix_b =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source_b, source_indices, target_b, target_indices);

  expectMatrixNear(matrix_a, expected, 9e-4f);
  expectMatrixNear(matrix_b, expected, 9e-4f);
  expectMatrixNear(matrix_a, matrix_b, 1e-6f);
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZSmallInputFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(32, 4.0f);
  const auto source_indices = makePrefixIndices(8);
  const auto target_indices = makeReverseIndices(8);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointXYZI, pcl::PointXYZINormal>(
          source, source_indices, target_indices, support::makeRigid2DTransform(), -4.0f);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);
  const Eigen::Matrix4f reference =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DDualIndexedDirectStd(
              source, source_indices, target, target_indices));

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(candidate_matrix, reference, 2e-4f);
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZNonDenseFiniteFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointNormal>(128, 2.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto target_indices = makeReverseIndices(64);
  auto target =
      makeTargetForIndexedPairs<pcl::PointNormal, pcl::PointXYZINormal>(
          source,
          source_indices,
          target_indices,
          support::makeRigid2DTransform(0.22f),
          -2.0f);
  source.is_dense = false;
  target.is_dense = false;

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZNonFiniteSelectedSourceFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointXYZINormal>(128, 7.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto target_indices = makeReverseIndices(64);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointXYZINormal, pcl::PointNormal>(
          source,
          source_indices,
          target_indices,
          support::makeRigid2DTransform(0.22f),
          -7.0f);
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.source_finite_points, source_indices.size() - 1);
  EXPECT_EQ(stats.target_finite_points, target_indices.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, DualIndexedGenericXYZNonFiniteSelectedTargetFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(128, 7.0f);
  const auto source_indices = makePrefixIndices(64);
  const auto target_indices = makeReverseIndices(64);
  auto target =
      makeTargetForIndexedPairs<pcl::PointXYZI, pcl::PointXYZINormal>(
          source,
          source_indices,
          target_indices,
          support::makeRigid2DTransform(0.22f),
          -7.0f);
  target[static_cast<std::size_t>(target_indices[9])].z =
      std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.source_finite_points, source_indices.size());
  EXPECT_EQ(stats.target_finite_points, target_indices.size() - 1);
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, PublicDualIndexedGenericXYZDoubleUsesScalarBoundary)
{
  constexpr std::size_t kStorageCount = 256;
  constexpr std::size_t kRowCount = 96;
  const Eigen::Matrix4f expected_float =
      support::makeRigid2DTransform(-0.16f, 0.38f, -0.57f);
  const auto source =
      support::makeXYZLikeCloud<pcl::PointNormal>(kStorageCount, 6.0f);
  const auto source_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto target_indices =
      makeStridedIndices(kRowCount, source.size(), 5, 7);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointNormal, pcl::PointXYZINormal>(
          source, source_indices, target_indices, expected_float, -6.0f);

  pcl::registration::TransformationEstimation2D<
      pcl::PointNormal, pcl::PointXYZINormal, double>
      estimator;
  Eigen::Matrix4d actual = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, actual);

  expectMatrixNear(actual, expected_float.cast<double>(), 7e-4);
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZCandidateMatchesSameTypePairs)
{
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZ>();
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZI>();
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointNormal,
                                                      pcl::PointNormal>();
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointXYZINormal,
                                                      pcl::PointXYZINormal>();
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZCandidateMatchesMixedPairs)
{
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointXYZI, pcl::PointXYZ>();
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointXYZ, pcl::PointXYZI>();
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointNormal,
                                                      pcl::PointXYZINormal>();
  expectCorrespondenceGenericXYZPairMatchesReferences<pcl::PointXYZINormal,
                                                      pcl::PointNormal>();
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZCandidateIgnoresExtraFields)
{
  constexpr std::size_t kStorageCount = 8192;
  constexpr std::size_t kRowCount = 4096;
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.12f, 0.31f, -0.44f);
  const auto source_a =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 1.0f);
  const auto source_b =
      support::makeXYZLikeCloud<pcl::PointXYZINormal>(kStorageCount, 101.0f);
  const auto query_indices =
      makeStridedIndices(kRowCount, source_a.size(), 5, 7);
  const auto match_indices =
      makeStridedIndices(kRowCount, source_a.size(), 7, 11);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);
  const auto target_a =
      makeTargetForIndexedPairs<pcl::PointXYZINormal, pcl::PointNormal>(
          source_a, query_indices, match_indices, expected, 5.0f);
  const auto target_b =
      makeTargetForIndexedPairs<pcl::PointXYZINormal, pcl::PointNormal>(
          source_b, query_indices, match_indices, expected, 205.0f);

  const Eigen::Matrix4f matrix_a =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source_a, target_a, correspondences);
  const Eigen::Matrix4f matrix_b =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source_b, target_b, correspondences);

  expectMatrixNear(matrix_a, expected, 9e-4f);
  expectMatrixNear(matrix_b, expected, 9e-4f);
  expectMatrixNear(matrix_a, matrix_b, 1e-6f);
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZSmallInputFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(32, 4.0f);
  const auto query_indices = makePrefixIndices(8);
  const auto match_indices = makeReverseIndices(8);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointXYZI, pcl::PointXYZINormal>(
          source, query_indices, match_indices, support::makeRigid2DTransform(), -4.0f);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);
  const Eigen::Matrix4f reference =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DCorrespondenceDirectStd(
              source, target, correspondences));

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  expectMatrixNear(candidate_matrix, reference, 2e-4f);
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZNonDenseFiniteFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointNormal>(128, 2.0f);
  const auto query_indices = makePrefixIndices(64);
  const auto match_indices = makeReverseIndices(64);
  auto target =
      makeTargetForIndexedPairs<pcl::PointNormal, pcl::PointXYZINormal>(
          source,
          query_indices,
          match_indices,
          support::makeRigid2DTransform(0.22f),
          -2.0f);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);
  source.is_dense = false;
  target.is_dense = false;

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZNonFiniteQueryFallsBack)
{
  auto source = support::makeXYZLikeCloud<pcl::PointXYZINormal>(128, 7.0f);
  const auto query_indices = makePrefixIndices(64);
  const auto match_indices = makeReverseIndices(64);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointXYZINormal, pcl::PointNormal>(
          source,
          query_indices,
          match_indices,
          support::makeRigid2DTransform(0.22f),
          -7.0f);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.source_finite_points, correspondences.size() - 1);
  EXPECT_EQ(stats.target_finite_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, CorrespondenceGenericXYZNonFiniteMatchFallsBack)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(128, 7.0f);
  const auto query_indices = makePrefixIndices(64);
  const auto match_indices = makeReverseIndices(64);
  auto target =
      makeTargetForIndexedPairs<pcl::PointXYZI, pcl::PointXYZINormal>(
          source,
          query_indices,
          match_indices,
          support::makeRigid2DTransform(0.22f),
          -7.0f);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);
  target[static_cast<std::size_t>(match_indices[9])].z =
      std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);

  EXPECT_TRUE(stats.layout_supported);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.source_finite_points, correspondences.size());
  EXPECT_EQ(stats.target_finite_points, correspondences.size() - 1);
  EXPECT_EQ(stats.accepted_points, 0u);
  expectMatrixExactlySame(candidate_matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, PublicCorrespondenceGenericXYZDoubleUsesScalarBoundary)
{
  constexpr std::size_t kStorageCount = 256;
  constexpr std::size_t kRowCount = 96;
  const Eigen::Matrix4f expected_float =
      support::makeRigid2DTransform(0.21f, -0.36f, 0.52f);
  const auto source =
      support::makeXYZLikeCloud<pcl::PointNormal>(kStorageCount, 6.0f);
  const auto query_indices =
      makeStridedIndices(kRowCount, source.size(), 3, 5);
  const auto match_indices =
      makeStridedIndices(kRowCount, source.size(), 5, 7);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointNormal, pcl::PointXYZINormal>(
          source, query_indices, match_indices, expected_float, -6.0f);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);

  pcl::registration::TransformationEstimation2D<
      pcl::PointNormal, pcl::PointXYZINormal, double>
      estimator;
  Eigen::Matrix4d actual = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, actual);

  expectMatrixNear(actual, expected_float.cast<double>(), 7e-4);
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

// Phase 093 dual-indexed production probe 只接入合法 `PointXYZ -> PointXYZ` /
// `Scalar=float` 公开入口。本测试保护真实 public overload 的
// source[indices_src[i]] / target[indices_tgt[i]] 配对和 2D 刚体结果。
TEST(TransformationEstimation2D, PublicDualIndexedProductionProbeRecoversRigid2DTransform)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.18f, 0.75f, 0.29f);
  const auto source_indices = makeStridedIndices(1536, source.size(), 3, 5);
  const auto target_indices = makeStridedIndices(1536, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, source_indices, target_indices, expected);

  const Eigen::Matrix4f actual =
      support::estimatePublicDualIndexed2D(source, source_indices, target, target_indices);

  expectMatrixNear(actual, expected, 5e-4f);
}

// Phase 093 family A/B 的 materialize baseline 先把 source/target 两侧 selected rows
// 连续化，再调用真实 ordered-cloud-pair public overload。这里先锁住它与当前
// dual-indexed public direct path 的输出一致；否则后续 board B/A 不能解释为性能差异。
TEST(TransformationEstimation2D, PublicDualIndexedFamilyMaterializedOrderedMatchesDirectPublic)
{
  const auto source = support::makePointXYZCloud(8192);
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.23f, -0.61f, 0.37f);
  const auto source_indices = makeStridedIndices(4096, source.size(), 3, 5);
  const auto target_indices = makeStridedIndices(4096, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, source_indices, target_indices, expected);

  const Eigen::Matrix4f direct =
      support::estimatePublicDualIndexed2D(source, source_indices, target, target_indices);
  const Eigen::Matrix4f materialized =
      support::estimatePublicDualIndexedMaterializedOrdered2D(
          source, source_indices, target, target_indices);

  expectMatrixNear(direct, expected, 5e-4f);
  expectMatrixNear(materialized, direct, 1e-6f);
  EXPECT_EQ(support::matrixChecksum(materialized), support::matrixChecksum(direct))
      << "max_abs_diff=" << support::matrixMaxAbsDiff(materialized, direct);
}

TEST(TransformationEstimation2D, PublicDualIndexedSmallInputUsesScalarFallback)
{
  const auto source = support::makePointXYZCloud(16);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.22f, 0.34f, 0.53f);
  const auto source_indices = makeStridedIndices(8, source.size(), 3, 1);
  const auto target_indices = makeStridedIndices(8, source.size(), 5, 2);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, source_indices, target_indices, expected);

  expectPublicDualIndexedMatchesDouble(
      source, source_indices, target, target_indices, 2e-4);
}

TEST(TransformationEstimation2D, PublicDualIndexedNonDenseFiniteUsesScalarFallback)
{
  auto source = support::makePointXYZCloud(128);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.14f, -0.48f, 0.26f);
  const auto source_indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto target_indices = makeStridedIndices(64, source.size(), 5, 7);
  auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, source_indices, target_indices, expected);
  source.is_dense = false;
  target.is_dense = false;

  expectPublicDualIndexedMatchesDouble(
      source, source_indices, target, target_indices, 2e-4);
}

TEST(TransformationEstimation2D, PublicDualIndexedNonFiniteSelectedZUsesScalarFallback)
{
  const auto source = support::makePointXYZCloud(128);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.17f, 0.45f, -0.36f);
  const auto source_indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto target_indices = makeStridedIndices(64, source.size(), 5, 7);
  auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, source_indices, target_indices, expected);
  target[static_cast<std::size_t>(target_indices[7])].z =
      std::numeric_limits<float>::quiet_NaN();

  expectPublicDualIndexedMatchesDouble(
      source, source_indices, target, target_indices, 3e-4);
}

TEST(TransformationEstimation2D, PublicDualIndexedPointTypeOutsidePhaseScopeUsesScalarFallback)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(256, 2.0f);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.08f, 0.29f, -0.58f);
  const auto source_indices = makeStridedIndices(96, source.size(), 5, 3);
  const auto target_indices = makeStridedIndices(96, source.size(), 7, 9);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointXYZI, pcl::PointXYZI>(
          source, source_indices, target_indices, expected, -4.0f);

  expectPublicDualIndexedMatchesDouble(
      source, source_indices, target, target_indices, 2e-4);
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

// Phase 094 correspondence-pair（对应关系点对）production probe 只接入合法
// `PointXYZ -> PointXYZ` / `Scalar=float` 公开入口。本测试使用不同的 query / match
// stride，保护 `index_query -> source`、`index_match -> target` 的真实配对语义。
TEST(TransformationEstimation2D, PublicCorrespondenceProductionProbeRecoversRigid2DTransform)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.07f, 0.33f, 0.48f);
  const auto query_indices = makeStridedIndices(1536, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(1536, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);

  const Eigen::Matrix4f actual =
      support::estimatePublicCorrespondence2D(source, target, correspondences);

  expectMatrixNear(actual, expected, 5e-4f);
}

// Phase 094 若 public probe 正向，还需要同一 production boundary 内的 family A/B。
// 这里先证明 materialize query/match rows 后调用真实 ordered public overload，和当前
// direct correspondence public path 在生产 RVV 误差预算内一致。ordered public 在 RVV
// 构建中可能命中已采纳的向量规约路径，因此不能要求与 correspondence scalar fallback
// 逐位 checksum 相同。
TEST(TransformationEstimation2D, PublicCorrespondenceFamilyMaterializedOrderedMatchesDirectPublic)
{
  const auto source = support::makePointXYZCloud(8192);
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.16f, -0.52f, 0.41f);
  const auto query_indices = makeStridedIndices(4096, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(4096, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);

  const Eigen::Matrix4f direct =
      support::estimatePublicCorrespondence2D(source, target, correspondences);
  const Eigen::Matrix4f materialized =
      support::estimatePublicCorrespondenceMaterializedOrdered2D(
          source, target, correspondences);

  expectMatrixNear(direct, expected, 5e-4f);
  expectMatrixNear(materialized, direct, 2e-6f);
}

// Phase 095 staging/profile 候选：先把 correspondence query/match 拷成连续
// source/target indices，再调用 dual-indexed public overload。它只证明同输入输出一致，
// 不把 Phase 093 dual-indexed 或 Phase 094 correspondence 候选写成已采纳。
TEST(TransformationEstimation2D, PublicCorrespondenceFamilyStagedDualIndexedMatchesDirectPublic)
{
  const auto source = support::makePointXYZCloud(8192);
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(-0.19f, 0.62f, -0.35f);
  const auto query_indices = makeStridedIndices(4096, source.size(), 7, 11);
  const auto match_indices = makeStridedIndices(4096, source.size(), 9, 13);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);

  const Eigen::Matrix4f direct =
      support::estimatePublicCorrespondence2D(source, target, correspondences);
  const Eigen::Matrix4f staged =
      support::estimatePublicCorrespondenceStagedDualIndexed2D(
          source, target, correspondences);
  const Eigen::Matrix4f materialized =
      support::estimatePublicCorrespondenceMaterializedOrdered2D(
          source, target, correspondences);

  expectMatrixNear(direct, expected, 5e-4f);
  expectMatrixNear(staged, direct, 2e-6f);
  expectMatrixNear(materialized, direct, 2e-6f);
}

// Phase 096 locality/order profile 只改变 query/match 的访问分布。每个分布必须先证明
// direct correspondence public、staged-dual public 和 materialize+ordered public 的输出在
// 生产 RVV 误差预算内一致，后续 board repeated 才能把差异解释成 locality / staging
// 成本，而不是语义偏差。
TEST(TransformationEstimation2D, PublicCorrespondenceLocalityProfilesMatchAcrossFamilies)
{
  const auto source = support::makePointXYZCloud(8192);
  const Eigen::Matrix4f expected =
      support::makeRigid2DTransform(0.24f, -0.58f, 0.31f);

  const auto run_profile = [&](const std::string& profile,
                               const pcl::Indices& query_indices,
                               const pcl::Indices& match_indices) {
    SCOPED_TRACE(profile);
    const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
        source, query_indices, match_indices, expected);
    const auto correspondences =
        makeStridedCorrespondences(query_indices, match_indices);

    const Eigen::Matrix4f direct =
        support::estimatePublicCorrespondence2D(source, target, correspondences);
    const Eigen::Matrix4f staged =
        support::estimatePublicCorrespondenceStagedDualIndexed2D(
            source, target, correspondences);
    const Eigen::Matrix4f materialized =
        support::estimatePublicCorrespondenceMaterializedOrdered2D(
            source, target, correspondences);

    expectMatrixNear(direct, expected, 5e-4f);
    expectMatrixNear(staged, direct, 2e-6f);
    expectMatrixNear(materialized, direct, 2e-6f);
  };

  const auto identity = makePrefixIndices(4096);
  const auto strided_query = makeStridedIndices(4096, source.size(), 3, 5);
  const auto strided_match = makeStridedIndices(4096, source.size(), 5, 7);
  const auto reverse = makeReverseIndices(4096);
  const auto shuffled_query = makeShuffledIndices(4096, 1103515245ull, 12345ull);
  const auto shuffled_match = makeShuffledIndices(4096, 1664525ull, 1013904223ull);

  run_profile("identity", identity, identity);
  run_profile("strided", strided_query, strided_match);
  run_profile("reverse", reverse, reverse);
  run_profile("shuffled", shuffled_query, shuffled_match);
}

TEST(TransformationEstimation2D, PublicCorrespondenceSmallInputUsesScalarFallback)
{
  const auto source = support::makePointXYZCloud(16);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.12f, -0.31f, 0.49f);
  const auto query_indices = makeStridedIndices(8, source.size(), 3, 1);
  const auto match_indices = makeStridedIndices(8, source.size(), 5, 2);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);

  expectPublicCorrespondenceMatchesDouble(source, target, correspondences, 2e-4);
}

TEST(TransformationEstimation2D, PublicCorrespondenceNonDenseFiniteUsesScalarFallback)
{
  auto source = support::makePointXYZCloud(128);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.15f, 0.43f, -0.27f);
  const auto query_indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(64, source.size(), 5, 7);
  auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);
  source.is_dense = false;
  target.is_dense = false;

  expectPublicCorrespondenceMatchesDouble(source, target, correspondences, 2e-4);
}

TEST(TransformationEstimation2D, PublicCorrespondenceNonFiniteSelectedSourceZUsesScalarFallback)
{
  auto source = support::makePointXYZCloud(128);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.18f, 0.38f, -0.33f);
  const auto query_indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(64, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);
  source.is_dense = false;
  source[static_cast<std::size_t>(query_indices[7])].z =
      std::numeric_limits<float>::quiet_NaN();

  expectPublicCorrespondenceMatchesDouble(source, target, correspondences, 3e-4);
}

TEST(TransformationEstimation2D, PublicCorrespondenceNonFiniteSelectedTargetZUsesScalarFallback)
{
  const auto source = support::makePointXYZCloud(128);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.13f, 0.39f, 0.28f);
  const auto query_indices = makeStridedIndices(64, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(64, source.size(), 5, 7);
  auto target = makeTargetForIndexedPairs<pcl::PointXYZ, pcl::PointXYZ>(
      source, query_indices, match_indices, expected);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);
  target.is_dense = false;
  target[static_cast<std::size_t>(match_indices[7])].z =
      std::numeric_limits<float>::quiet_NaN();

  expectPublicCorrespondenceMatchesDouble(source, target, correspondences, 3e-4);
}

TEST(TransformationEstimation2D, PublicCorrespondencePointTypeOutsidePhaseScopeUsesScalarFallback)
{
  const auto source = support::makeXYZLikeCloud<pcl::PointXYZI>(256, 2.0f);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.07f, -0.28f, 0.56f);
  const auto query_indices = makeStridedIndices(96, source.size(), 5, 3);
  const auto match_indices = makeStridedIndices(96, source.size(), 7, 9);
  const auto target =
      makeTargetForIndexedPairs<pcl::PointXYZI, pcl::PointXYZI>(
          source, query_indices, match_indices, expected, -4.0f);
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);

  expectPublicCorrespondenceMatchesDouble(source, target, correspondences, 2e-4);
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

// Phase 090 direct gather source-indexed candidate 不再 materialize 整点云，
// 直接用 source index byte offset gather source，并顺序读取 target 行。
TEST(TransformationEstimation2D, SourceIndexedDirectGatherCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const auto indices = makeStridedIndices(1536, source.size());
  const auto selected = selectByIndices(source, indices);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.13f, 0.61f, -0.37f);
  const auto target = support::transformCloud2D(selected, expected);

  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, indices, target, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, indices, target, &stats);

  EXPECT_EQ(stats.input_points, indices.size());
  EXPECT_EQ(stats.accepted_points, indices.size());
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 8e-4f);
}

// dual-indexed direct gather 同时读取 source_indices 和 target_indices。这里两侧
// 使用不同 stride，让测试能发现 source / target offset stream 混用问题。
TEST(TransformationEstimation2D, DualIndexedDirectGatherCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.19f, -0.42f, 0.57f);
  auto target = support::makePointXYZCloud(4096);
  const auto source_indices = makeStridedIndices(1536, source.size(), 3, 5);
  const auto target_indices = makeStridedIndices(1536, target.size(), 5, 7);
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto source_index = static_cast<std::size_t>(source_indices[i]);
    const auto target_index = static_cast<std::size_t>(target_indices[i]);
    const Eigen::Vector4f p(source[source_index].x,
                            source[source_index].y,
                            source[source_index].z,
                            1.0f);
    const Eigen::Vector4f q = expected * p;
    target[target_index].x = q.x();
    target[target_index].y = q.y();
    target[target_index].z = source[source_index].z;
  }

  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 9e-4f);
}

// correspondence direct gather 的证据边界包含从 correspondence 结构体读取 query/match
// 两条 index stream，再分别 gather source/target xyz。
TEST(TransformationEstimation2D, CorrespondenceDirectGatherCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.07f, 0.33f, 0.48f);
  auto target = support::makePointXYZCloud(4096);
  const auto query_indices = makeStridedIndices(1536, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(1536, target.size(), 5, 7);
  for (std::size_t i = 0; i < query_indices.size(); ++i) {
    const auto query_index = static_cast<std::size_t>(query_indices[i]);
    const auto match_index = static_cast<std::size_t>(match_indices[i]);
    const Eigen::Vector4f p(source[query_index].x,
                            source[query_index].y,
                            source[query_index].z,
                            1.0f);
    const Eigen::Vector4f q = expected * p;
    target[match_index].x = q.x();
    target[match_index].y = q.y();
    target[match_index].z = source[query_index].z;
  }
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);

  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 9e-4f);
}

TEST(TransformationEstimation2D, CorrespondenceChunkedXYZStagingCandidateMatchesPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.11f, -0.37f, 0.44f);
  auto target = support::makePointXYZCloud(4096);
  const auto query_indices = makeStridedIndices(2048, source.size(), 3, 5);
  const auto match_indices = makeStridedIndices(2048, target.size(), 5, 7);
  for (std::size_t i = 0; i < query_indices.size(); ++i) {
    const auto query_index = static_cast<std::size_t>(query_indices[i]);
    const auto match_index = static_cast<std::size_t>(match_indices[i]);
    const Eigen::Vector4f p(source[query_index].x,
                            source[query_index].y,
                            source[query_index].z,
                            1.0f);
    const Eigen::Vector4f q = expected * p;
    target[match_index].x = q.x();
    target[match_index].y = q.y();
    target[match_index].z = source[query_index].z;
  }
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);
  const Eigen::Matrix4f public_matrix =
      support::estimatePublicCorrespondence2D(source, target, correspondences);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
          source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 9e-4f);
}

TEST(TransformationEstimation2D, CorrespondenceChunkedXYZStagingCandidateMatchesShuffledPublic)
{
  const auto source = support::makePointXYZCloud(4096);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(-0.08f, 0.27f, -0.61f);
  auto target = support::makePointXYZCloud(4096);
  const auto query_indices = makeShuffledIndices(2048, 1103515245ull, 12345ull);
  const auto match_indices = makeShuffledIndices(2048, 1664525ull, 1013904223ull);
  for (std::size_t i = 0; i < query_indices.size(); ++i) {
    const auto query_index = static_cast<std::size_t>(query_indices[i]);
    const auto match_index = static_cast<std::size_t>(match_indices[i]);
    const Eigen::Vector4f p(source[query_index].x,
                            source[query_index].y,
                            source[query_index].z,
                            1.0f);
    const Eigen::Vector4f q = expected * p;
    target[match_index].x = q.x();
    target[match_index].y = q.y();
    target[match_index].z = source[query_index].z;
  }
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);
  const Eigen::Matrix4f public_matrix =
      support::estimatePublicCorrespondence2D(source, target, correspondences);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
          source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  EXPECT_TRUE(stats.layout_supported);
  EXPECT_TRUE(stats.dense_finite_input);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
#else
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, public_matrix, 9e-4f);
}

TEST(TransformationEstimation2D, CorrespondenceChunkedXYZStagingSmallInputFallsBack)
{
  const auto source = support::makePointXYZCloud(16);
  auto target = support::makePointXYZCloud(16);
  const auto query_indices = makePrefixIndices(8);
  const auto match_indices = makeReverseIndices(8);
  const Eigen::Matrix4f expected = support::makeRigid2DTransform(0.05f, 0.13f, -0.21f);
  for (std::size_t i = 0; i < query_indices.size(); ++i) {
    const auto query_index = static_cast<std::size_t>(query_indices[i]);
    const auto match_index = static_cast<std::size_t>(match_indices[i]);
    const Eigen::Vector4f p(source[query_index].x,
                            source[query_index].y,
                            source[query_index].z,
                            1.0f);
    const Eigen::Vector4f q = expected * p;
    target[match_index].x = q.x();
    target[match_index].y = q.y();
    target[match_index].z = source[query_index].z;
  }
  const auto correspondences = makeStridedCorrespondences(query_indices, match_indices);
  const Eigen::Matrix4f reference =
      support::estimatePublicCorrespondence2D(source, target, correspondences);

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
          source, target, correspondences, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, correspondences.size());
  expectMatrixNear(candidate_matrix, reference, 2e-4f);
}

TEST(TransformationEstimation2D, CorrespondenceChunkedXYZStagingNonFiniteSelectedRowFallsBack)
{
  const auto source = support::makePointXYZCloud(64);
  auto target = support::makePointXYZCloud(64);
  const auto correspondences = makePrefixCorrespondences(32);
  target[9].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f matrix =
      support::estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
          source, target, correspondences, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_EQ(stats.source_finite_points, correspondences.size());
  EXPECT_EQ(stats.target_finite_points, correspondences.size() - 1);
  expectMatrixExactlySame(matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, SourceIndexedDirectGatherSmallInputFallsBack)
{
  const auto source = support::makePointXYZCloud(16);
  const auto indices = makePrefixIndices(8);
  const auto selected = selectByIndices(source, indices);
  const auto target =
      support::transformCloud2D(selected, support::makeRigid2DTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f std_matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, indices, target, &stats);
  const Eigen::Matrix4f reference =
      support::solveTransform2DFromAccumulation(
          support::accumulateFused2DSourceIndexedDirectStd(source, indices, target));

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_EQ(stats.input_points, indices.size());
  EXPECT_EQ(stats.accepted_points, indices.size());
  expectMatrixNear(std_matrix, reference, 2e-4f);
}

TEST(TransformationEstimation2D, SourceIndexedDirectGatherNonFiniteSelectedRowFallsBack)
{
  auto source = support::makePointXYZCloud(64);
  const auto indices = makePrefixIndices(32);
  const auto selected = selectByIndices(source, indices);
  const auto target =
      support::transformCloud2D(selected, support::makeRigid2DTransform());
  source[5].z = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f matrix =
      support::estimateFused2DSourceIndexedDirectGatherCandidate(
          source, indices, target, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, indices.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_EQ(stats.source_finite_points, indices.size() - 1);
  EXPECT_EQ(stats.target_finite_points, indices.size());
  expectMatrixExactlySame(matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, DualIndexedDirectGatherNonFiniteSelectedRowFallsBack)
{
  const auto source = support::makePointXYZCloud(64);
  auto target = support::makePointXYZCloud(64);
  const auto source_indices = makePrefixIndices(32);
  const auto target_indices = makePrefixIndices(32);
  target[7].x = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f matrix =
      support::estimateFused2DDualIndexedDirectGatherCandidate(
          source, source_indices, target, target_indices, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_EQ(stats.source_finite_points, source_indices.size());
  EXPECT_EQ(stats.target_finite_points, target_indices.size() - 1);
  expectMatrixExactlySame(matrix, Eigen::Matrix4f::Identity());
}

TEST(TransformationEstimation2D, CorrespondenceDirectGatherNonFiniteSelectedRowFallsBack)
{
  const auto source = support::makePointXYZCloud(64);
  auto target = support::makePointXYZCloud(64);
  const auto correspondences = makePrefixCorrespondences(32);
  target[9].y = std::numeric_limits<float>::quiet_NaN();

  support::CandidateStats stats;
  const Eigen::Matrix4f matrix =
      support::estimateFused2DCorrespondenceDirectGatherCandidate(
          source, target, correspondences, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  EXPECT_FALSE(stats.dense_finite_input);
  EXPECT_EQ(stats.input_points, correspondences.size());
  EXPECT_EQ(stats.accepted_points, 0u);
  EXPECT_EQ(stats.source_finite_points, correspondences.size());
  EXPECT_EQ(stats.target_finite_points, correspondences.size() - 1);
  expectMatrixExactlySame(matrix, Eigen::Matrix4f::Identity());
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
