/*
 * 本文件做什么：
 * correspondence_rejection_poly 的专项 correctness（正确性）测试。它覆盖输入 guard
 * （验收条件）、多边形边长阈值、accept rate（接受率）、histogram（直方图）、
 * Otsu threshold（Otsu 阈值）和 remaining_correspondences（剩余对应关系）输出顺序。
 *
 * 证据边界：
 * 这些 TEST 是 QEMU correctness 和本地回归证据。它们包含 test-only RVV candidate
 * （测试专用 RVV 候选）和真实 `CorrespondenceRejectorPoly` public entry（公开入口）
 * 的固定 seed smoke（小型验证），但不证明 production dispatch（生产分流）存在。
 */

#include "correspondence_rejection_poly.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace support =
    pcl::registration::rvv_correspondence_rejection_poly_support;

namespace {

template <typename T>
void
expect_vectors_equal(const std::vector<T>& lhs, const std::vector<T>& rhs)
{
  ASSERT_EQ(lhs.size(), rhs.size());
  for (std::size_t i = 0; i < lhs.size(); ++i)
    EXPECT_EQ(lhs[i], rhs[i]) << "mismatch at " << i;
}

void
expect_rates_near(const std::vector<float>& lhs, const std::vector<float>& rhs)
{
  ASSERT_EQ(lhs.size(), rhs.size());
  for (std::size_t i = 0; i < lhs.size(); ++i)
    EXPECT_NEAR(lhs[i], rhs[i], 1e-6f) << "mismatch at " << i;
}

std::vector<float>
make_rate_samples()
{
  return {0.0f, 0.05f, 0.10f, 0.20f, 0.33f, 0.50f,
          0.52f, 0.75f, 0.90f, 1.0f, 0.0f, 0.88f};
}

struct SeededPublicEntryCase {
  unsigned int cloud_seed;
  unsigned int sampling_seed;
  std::size_t size;
  int iterations;
  int cardinality;
  float similarity;
};

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_seeded_source_cloud(const std::size_t size, const unsigned int seed)
{
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> coord(-12.0f, 12.0f);

  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->resize(size);
  for (std::size_t i = 0; i < size; ++i) {
    (*cloud)[i].x = coord(rng) + 0.01f * static_cast<float>(i % 11);
    (*cloud)[i].y = coord(rng) - 0.02f * static_cast<float>(i % 7);
    (*cloud)[i].z = coord(rng) + 0.03f * static_cast<float>(i % 5);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_seeded_target_cloud(const pcl::PointCloud<pcl::PointXYZ>& source,
                         const unsigned int seed)
{
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> noise(-0.06f, 0.06f);

  auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  target->resize(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const std::size_t source_index =
        (i % 9 == 0) ? (i * 17u + 5u) % source.size() : i;
    (*target)[i] = source[source_index];
    (*target)[i].x += noise(rng);
    (*target)[i].y += noise(rng);
    (*target)[i].z += noise(rng);
  }
  return target;
}

pcl::Correspondences
make_seeded_correspondences(const std::size_t size, const unsigned int seed)
{
  std::vector<int> query(size);
  std::vector<int> match(size);
  std::iota(query.begin(), query.end(), 0);
  std::iota(match.begin(), match.end(), 0);

  std::mt19937 query_rng(seed);
  std::mt19937 match_rng(seed + 101u);
  std::shuffle(query.begin(), query.end(), query_rng);
  std::shuffle(match.begin(), match.end(), match_rng);

  pcl::Correspondences correspondences;
  correspondences.reserve(size);
  for (std::size_t i = 0; i < size; ++i)
    correspondences.emplace_back(query[i], match[i], 0.0f);
  return correspondences;
}

void
expect_correspondences_equal(const pcl::Correspondences& lhs,
                             const pcl::Correspondences& rhs)
{
  ASSERT_EQ(lhs.size(), rhs.size());
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    EXPECT_EQ(lhs[i].index_query, rhs[i].index_query) << "mismatch at " << i;
    EXPECT_EQ(lhs[i].index_match, rhs[i].index_match) << "mismatch at " << i;
  }
}

} // namespace

// 输入 guard 必须保持 production 语义：没有 source 或 target 时，输出先等于输入副本。
// 这个测试只覆盖 public entry 的 fallback 形态，不涉及 RVV candidate。
TEST(CorrespondenceRejectionPoly, MissingInputReturnsAllCorrespondences)
{
  const auto correspondences = support::make_identity_correspondences(8);
  pcl::registration::CorrespondenceRejectorPoly<pcl::PointXYZ, pcl::PointXYZ> rejector;
  pcl::Correspondences remaining;

  rejector.getRemainingCorrespondences(correspondences, remaining);

  ASSERT_EQ(remaining.size(), correspondences.size());
  for (std::size_t i = 0; i < remaining.size(); ++i) {
    EXPECT_EQ(remaining[i].index_query, correspondences[i].index_query);
    EXPECT_EQ(remaining[i].index_match, correspondences[i].index_match);
  }
}

// cardinality（多边形点数）和 similarity threshold（相似度阈值）是独立 guard。
// 每个 case 只触发一个主要原因，避免把多个 fallback 混成一条证据。
TEST(CorrespondenceRejectionPoly, CardinalityAndSimilarityGuardsReturnAll)
{
  const auto source = support::make_source_cloud(16);
  const auto target = support::make_target_cloud_from_source(*source, 0);
  const auto correspondences = support::make_identity_correspondences(16);

  pcl::registration::CorrespondenceRejectorPoly<pcl::PointXYZ, pcl::PointXYZ> rejector;
  rejector.setInputSource(source);
  rejector.setInputTarget(target);

  for (const auto config : {std::pair<int, float>{1, 0.8f},
                            std::pair<int, float>{16, 0.8f},
                            std::pair<int, float>{3, -0.1f},
                            std::pair<int, float>{3, 1.1f}}) {
    rejector.setCardinality(config.first);
    rejector.setSimilarityThreshold(config.second);
    pcl::Correspondences remaining;
    rejector.getRemainingCorrespondences(correspondences, remaining);
    EXPECT_EQ(remaining.size(), correspondences.size()) << "cardinality "
                                                        << config.first
                                                        << " threshold "
                                                        << config.second;
  }
}

// 这个局部测试保护 thresholdEdgeLength 的数学语义。零长度边会得到 0/0，
// 既有 production 语义下比较结果为 false；candidate 不能把它改成 true。
TEST(CorrespondenceRejectionPoly, EdgeSimilarityBatchMatchesReference)
{
  const std::vector<float> src = {1.0f, 4.0f, 9.0f, 0.25f, 0.0f, 16.0f};
  const std::vector<float> tgt = {1.0f, 1.0f, 4.0f, 1.0f, 0.0f, 64.0f};
  constexpr float kThresholdSquared = 0.64f;

  const auto reference =
      support::edge_similarity_batch_reference(src, tgt, kThresholdSquared);
  const auto candidate =
      support::edge_similarity_batch_candidate(src, tgt, kThresholdSquared);

  expect_vectors_equal(candidate.accepted, reference.accepted);
  EXPECT_EQ(candidate.stats.input_size, src.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate.stats.used_rvv);
#else
  EXPECT_FALSE(candidate.stats.used_rvv);
#endif
}

// production-shaped gather 诊断把 correspondence index 到 PointXYZ AoS 读点、
// squared distance staging 和 edge formula 放进同一个边界。它仍不证明完整
// getRemainingCorrespondences，因为随机采样、histogram 和输出阶段没有接入 RVV。
TEST(CorrespondenceRejectionPoly, EdgeGatherStagingCandidateMatchesReference)
{
  const auto source = support::make_source_cloud(64);
  const auto target = support::make_target_cloud_from_source(*source, 16);
  const auto correspondences = support::make_identity_correspondences(64);
  const std::vector<support::EdgePair> edge_pairs = {
      {0, 1}, {2, 7}, {8, 31}, {15, 48}, {63, 4}, {22, 23}, {5, 57}};
  constexpr float kThresholdSquared = 0.64f;

  const auto reference = support::edge_similarity_gather_reference(
      *source, *target, correspondences, edge_pairs, kThresholdSquared);
  const auto candidate = support::edge_similarity_gather_candidate(
      *source, *target, correspondences, edge_pairs, kThresholdSquared);

  expect_vectors_equal(candidate.accepted, reference.accepted);
  EXPECT_EQ(candidate.stats.input_size, edge_pairs.size());
#ifdef __RVV10__
  EXPECT_TRUE(candidate.stats.used_rvv);
#else
  EXPECT_FALSE(candidate.stats.used_rvv);
#endif
}

// accept rate 计算需要保留 num_samples==0 时输出 0 的语义。RVV candidate 用 mask
// 避免除以 0 的结果进入输出数组。
TEST(CorrespondenceRejectionPoly, AcceptanceRateCandidateKeepsZeroSampleSemantics)
{
  const std::vector<int> samples = {0, 1, 2, 4, 8, 16, 0, 32, 64};
  const std::vector<int> accepted = {0, 1, 1, 3, 6, 8, 5, 31, 0};

  const auto reference = support::compute_acceptance_rates_reference(samples, accepted);
  const auto candidate = support::compute_acceptance_rates_candidate(samples, accepted);

  expect_rates_near(candidate.rates, reference);
#ifdef __RVV10__
  EXPECT_TRUE(candidate.stats.used_rvv);
#else
  EXPECT_FALSE(candidate.stats.used_rvv);
#endif
}

// histogram 和 Otsu 阈值首阶段保留标量。测试记录 clamp 到最后一格、
// `>` cut 语义和 Otsu 空类跳过逻辑，作为后续候选的参考链路。
TEST(CorrespondenceRejectionPoly, HistogramOtsuAndFilterSemantics)
{
  const auto rates = make_rate_samples();
  const auto histogram = support::compute_histogram_reference(rates, 0.0f, 1.0f, 6);
  const int cut_idx = support::find_threshold_otsu_reference(histogram);
  const float cut = static_cast<float>(cut_idx) / 6.0f;

  const auto reference = support::filter_by_acceptance_rate_reference(rates, cut);
  const auto candidate = support::filter_by_acceptance_rate_candidate(rates, cut);

  EXPECT_EQ(std::accumulate(histogram.begin(), histogram.end(), 0),
            static_cast<int>(rates.size()));
  expect_vectors_equal(candidate.kept_indices, reference.kept_indices);
#ifdef __RVV10__
  EXPECT_TRUE(candidate.stats.used_rvv);
#else
  EXPECT_FALSE(candidate.stats.used_rvv);
#endif
}

// 固定 std::srand seed 后，test-only reference 和 production class 应得到同一输出。
// 这条 public-entry-shaped smoke 证明诊断输入能复现当前源码语义；它仍不是 production RVV 证据。
TEST(CorrespondenceRejectionPoly, FixedSeedReferenceMatchesProductionEntry)
{
  constexpr unsigned int kSeed = 12345;
  constexpr int kIterations = 512;
  constexpr int kCardinality = 3;
  constexpr float kSimilarity = 0.8f;

  const auto source = support::make_source_cloud(96);
  const auto target = support::make_target_cloud_from_source(*source, 24);
  const auto correspondences = support::make_identity_correspondences(96);

  std::srand(kSeed);
  const auto reference = support::remaining_correspondences_reference(
      *source, *target, correspondences, kIterations, kCardinality, kSimilarity);

  pcl::registration::CorrespondenceRejectorPoly<pcl::PointXYZ, pcl::PointXYZ> rejector;
  rejector.setInputSource(source);
  rejector.setInputTarget(target);
  rejector.setIterations(kIterations);
  rejector.setCardinality(kCardinality);
  rejector.setSimilarityThreshold(kSimilarity);

  std::srand(kSeed);
  pcl::Correspondences production_result;
  rejector.getRemainingCorrespondences(correspondences, production_result);

  ASSERT_EQ(production_result.size(), reference.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(production_result[i].index_query, reference[i].index_query);
    EXPECT_EQ(production_result[i].index_match, reference[i].index_match);
  }
}

// 固定 seed 随机压力样本补上 deterministic corpus（确定性样本集）之外的覆盖。
// 每个 case 都重置 std::srand，确保 production public entry 与 reference 使用同一采样序列。
TEST(CorrespondenceRejectionPoly, SeededRandomPublicEntryMatchesReference)
{
  const std::vector<SeededPublicEntryCase> cases = {
      {9001u, 7001u, 64u, 257, 2, 0.74f},
      {9002u, 7002u, 96u, 383, 3, 0.80f},
      {9003u, 7003u, 128u, 431, 4, 0.88f},
      {9004u, 7004u, 111u, 389, 3, 0.93f},
  };

  for (const auto& config : cases) {
    const auto source = make_seeded_source_cloud(config.size, config.cloud_seed);
    const auto target = make_seeded_target_cloud(*source, config.cloud_seed + 17u);
    const auto correspondences =
        make_seeded_correspondences(config.size, config.cloud_seed + 29u);

    std::srand(config.sampling_seed);
    const auto reference = support::remaining_correspondences_reference(
        *source,
        *target,
        correspondences,
        config.iterations,
        config.cardinality,
        config.similarity);

    pcl::registration::CorrespondenceRejectorPoly<pcl::PointXYZ, pcl::PointXYZ>
        rejector;
    rejector.setInputSource(source);
    rejector.setInputTarget(target);
    rejector.setIterations(config.iterations);
    rejector.setCardinality(config.cardinality);
    rejector.setSimilarityThreshold(config.similarity);

    std::srand(config.sampling_seed);
    pcl::Correspondences production_result;
    rejector.getRemainingCorrespondences(correspondences, production_result);

    expect_correspondences_equal(production_result, reference);
  }
}
