/*
 * 本文件做什么：
 * 这些 gtest 验证 geometric_consistency topic 的 pairwise consistency
 * diagnostic helper 是否和标量参考一致。RVV build 必须命中 RVV candidate path；
 * 否则测试失败，避免把纯标量 fallback 误写成 RVV 证据。
 */

#include "gc.h"

#include <gtest/gtest.h>

#include <vector>

namespace gc = pcl::test::geometric_consistency_rvv;

namespace
{

std::vector<int>
makeConsensusIndices()
{
  return {1, 4, 7, 10, 13, 16};
}

} // namespace

TEST(GeometricConsistencyDiagnostic, ScalarReferenceMarksExpectedGoodAndBadCandidates)
{
  auto cloud = gc::makeSyntheticPackedCloud(24);
  const auto consensus = makeConsensusIndices();
  const float gc_size = 0.03f;

  EXPECT_TRUE(gc::pairwiseConsistencyScalarReference(cloud, consensus, 18, gc_size));
  gc::injectSceneNoise(cloud, 19, 0.15f, 0.0f, 0.0f);
  EXPECT_FALSE(gc::pairwiseConsistencyScalarReference(cloud, consensus, 19, gc_size));
}

TEST(GeometricConsistencyDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarReference)
{
  auto cloud = gc::makeSyntheticPackedCloud(24);
  const auto consensus = makeConsensusIndices();
  const float gc_size = 0.03f;

  const auto expected_good = gc::pairwiseConsistencyScalarReference(cloud, consensus, 18, gc_size);
  gc::injectSceneNoise(cloud, 19, 0.15f, 0.0f, 0.0f);
  const auto expected_bad = gc::pairwiseConsistencyScalarReference(cloud, consensus, 19, gc_size);

  bool actual_good = false;
  bool actual_bad = false;
  std::size_t vector_chunks = 0;
  const auto good_path = gc::pairwiseConsistencyCandidate(
      cloud, consensus, 18, gc_size, actual_good, &vector_chunks);
  const auto bad_path = gc::pairwiseConsistencyCandidate(
      cloud, consensus, 19, gc_size, actual_bad, &vector_chunks);

  EXPECT_EQ(actual_good, expected_good);
  EXPECT_EQ(actual_bad, expected_bad);
  EXPECT_TRUE(actual_good);
  EXPECT_FALSE(actual_bad);
#if defined(__RVV10__)
  EXPECT_EQ(good_path, gc::ExecutionPath::RvvPairwisePredicate);
  EXPECT_EQ(bad_path, gc::ExecutionPath::RvvPairwisePredicate);
  EXPECT_GT(vector_chunks, 0u);
#else
  EXPECT_EQ(good_path, gc::ExecutionPath::ScalarFallback);
  EXPECT_EQ(bad_path, gc::ExecutionPath::ScalarFallback);
#endif
}

TEST(GeometricConsistencyDiagnostic, BatchCandidateCountMatchesScalarReference)
{
  auto cloud = gc::makeSyntheticPackedCloud(128);
  const auto consensus = makeConsensusIndices();
  const float gc_size = 0.03f;
  gc::injectSceneNoise(cloud, 90, 0.15f, 0.0f, 0.0f);
  gc::injectSceneNoise(cloud, 101, 0.12f, 0.0f, 0.0f);

  const auto expected = gc::countConsistentCandidatesScalarReference(cloud, consensus, gc_size);
  std::size_t actual = 0;
  std::size_t vector_chunks = 0;
  const auto path = gc::countConsistentCandidatesCandidate(
      cloud, consensus, gc_size, actual, &vector_chunks);

  EXPECT_EQ(actual, expected);
  EXPECT_NE(gc::checksumCount(actual), 0u);
#if defined(__RVV10__)
  EXPECT_EQ(path, gc::ExecutionPath::RvvPairwisePredicate);
  EXPECT_GT(vector_chunks, 0u);
#else
  EXPECT_EQ(path, gc::ExecutionPath::ScalarFallback);
  EXPECT_EQ(vector_chunks, 0u);
#endif
}

TEST(GeometricConsistencyDiagnostic, ClusterGrowthCandidateMatchesScalarReference)
{
  auto cloud = gc::makeSyntheticPackedCloud(192);
  const float gc_size = 0.03f;
  const int gc_threshold = 3;
  for (const auto index : {18u, 53u, 90u, 127u, 160u})
    gc::injectSceneNoise(cloud, index, 0.18f, 0.0f, 0.0f);

  gc::ClusterGrowthResult expected;
  gc::ClusterGrowthResult actual;
  gc::clusterGrowthScalarReference(cloud, gc_size, gc_threshold, expected);
  const auto path = gc::clusterGrowthCandidate(cloud, gc_size, gc_threshold, actual);

  EXPECT_EQ(actual.clusters, expected.clusters);
  EXPECT_EQ(actual.accepted_correspondences, expected.accepted_correspondences);
  EXPECT_EQ(gc::checksumClusters(actual), gc::checksumClusters(expected));
#if defined(__RVV10__)
  EXPECT_EQ(path, gc::ExecutionPath::RvvPairwisePredicate);
  EXPECT_GT(actual.vector_chunks, 0u);
#else
  EXPECT_EQ(path, gc::ExecutionPath::ScalarFallback);
  EXPECT_EQ(actual.vector_chunks, 0u);
#endif
}

