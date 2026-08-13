/*
 * 本文件做什么：
 * correspondence_types 的专项 correctness（正确性）测试。它对比 production 标量 helper
 * 和 test-only RVV candidate，验证 index extraction（索引抽取）、sentinel（哨兵值）保留、
 * 空输入、单元素 distance stats（距离统计）边界和数值预算。
 *
 * 证据边界：
 * 这些 TEST 是 QEMU correctness 和本地回归证据；它们不证明真实 production dispatch
 * 已经存在，也不提供目标硬件性能结论。
 */

#include "correspondence_types.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace pcl::registration::rvv_correspondence_types_support;

namespace {

void
expect_indices_equal(const pcl::Indices& lhs, const pcl::Indices& rhs)
{
  ASSERT_EQ(lhs.size(), rhs.size());
  for (std::size_t i = 0; i < lhs.size(); ++i)
    EXPECT_EQ(lhs[i], rhs[i]) << "mismatch at " << i;
}

void
expect_stats_same_or_both_nan(const StatsResult& candidate, const StatsResult& reference)
{
  EXPECT_DOUBLE_EQ(candidate.mean, reference.mean);
  if (std::isnan(reference.stddev)) {
    EXPECT_TRUE(std::isnan(candidate.stddev));
  }
  else if (std::isinf(reference.stddev)) {
    EXPECT_EQ(std::signbit(candidate.stddev), std::signbit(reference.stddev));
    EXPECT_TRUE(std::isinf(candidate.stddev));
  }
  else {
    EXPECT_NEAR(candidate.stddev, reference.stddev, 1e-9);
  }
}

} // namespace

// 这个测试保护 query index 的顺序语义。重复 query 值和负值都必须原样写出，
// 因为 production helper 只是字段抽取，不做过滤或去重。
TEST(CorrespondenceTypes, QueryIndicesCandidateKeepsInputOrder)
{
  auto correspondences = make_correspondences(257);
  correspondences[5].index_query = -42;
  correspondences[6].index_query = correspondences[5].index_query;

  pcl::Indices reference;
  pcl::Indices candidate;
  CandidateStats stats;
  query_indices_std(correspondences, reference);
  query_indices_candidate(correspondences, candidate, &stats);

  expect_indices_equal(candidate, reference);
  EXPECT_EQ(stats.input_size, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_FALSE(stats.used_rvv);
#endif
}

// match index 中的 UNAVAILABLE=-1 是公开语义的一部分。RVV candidate 用跨步加载时
// 不能把它当无效 lane 过滤，也不能改变输出顺序。
TEST(CorrespondenceTypes, MatchIndicesCandidateKeepsSentinelAndOrder)
{
  auto correspondences = make_correspondences(513);
  correspondences[0].index_match = pcl::UNAVAILABLE;
  correspondences[101].index_match = pcl::UNAVAILABLE;
  correspondences[102].index_match = correspondences[101].index_match;

  pcl::Indices reference;
  pcl::Indices candidate;
  CandidateStats stats;
  match_indices_std(correspondences, reference);
  match_indices_candidate(correspondences, candidate, &stats);

  expect_indices_equal(candidate, reference);
  EXPECT_EQ(stats.input_size, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_FALSE(stats.used_rvv);
#endif
}

// 这个测试走真实 production helper（生产源码 helper），不用 test-only candidate。
// 它证明 public helper 的输出仍和标量参考链路一致；RVV 是否真的命中由反汇编和 bench 归属补证。
TEST(CorrespondenceTypes, QueryIndicesProductionDirectMatchesScalarReference)
{
  auto correspondences = make_correspondences(769);
  correspondences[11].index_query = -314;
  correspondences[512].index_query = correspondences[11].index_query;

  pcl::Indices reference;
  pcl::Indices production;
  query_indices_std(correspondences, reference);
  pcl::registration::getQueryIndices(correspondences, production);

  expect_indices_equal(production, reference);
}

// match index 的 production direct（真实生产路径）测试保护 `UNAVAILABLE=-1`
// 和重复值。失败说明生产 helper 的真实入口语义已经偏离标量字段抽取。
TEST(CorrespondenceTypes, MatchIndicesProductionDirectMatchesScalarReference)
{
  auto correspondences = make_correspondences(1027);
  correspondences[3].index_match = pcl::UNAVAILABLE;
  correspondences[700].index_match = -99;
  correspondences[701].index_match = correspondences[700].index_match;

  pcl::Indices reference;
  pcl::Indices production;
  match_indices_std(correspondences, reference);
  pcl::registration::getMatchIndices(correspondences, production);

  expect_indices_equal(production, reference);
}

// 空输入时 production helper 直接返回，不写 mean/stddev。测试支撑 wrapper 返回默认值，
// 用来确保 candidate 不因为空数组访问而崩溃；它不是 production 输出合同的扩展。
TEST(CorrespondenceTypes, EmptyInputDoesNotUseRVV)
{
  const pcl::Correspondences correspondences;
  pcl::Indices query;
  pcl::Indices match;
  CandidateStats query_stats;
  CandidateStats match_stats;
  CandidateStats dist_stats;

  query_indices_candidate(correspondences, query, &query_stats);
  match_indices_candidate(correspondences, match, &match_stats);
  const StatsResult stats = distance_stats_candidate(correspondences, &dist_stats);

  EXPECT_TRUE(query.empty());
  EXPECT_TRUE(match.empty());
  EXPECT_EQ(stats.mean, 0.0);
  EXPECT_EQ(stats.stddev, 0.0);
  EXPECT_FALSE(query_stats.used_rvv);
  EXPECT_FALSE(match_stats.used_rvv);
  EXPECT_FALSE(dist_stats.used_rvv);
}

// distance stats candidate 保持 float-square same-chain：平方先在 float 中完成，
// 再按原输入顺序累加到 double。首阶段不使用 vector reduction，避免改变加法树。
TEST(CorrespondenceTypes, DistanceStatsMatchesStdForRepresentativeInputs)
{
  const auto correspondences = make_correspondences(1025);
  CandidateStats stats;

  const StatsResult reference = distance_stats_std(correspondences);
  const StatsResult candidate = distance_stats_candidate(correspondences, &stats);

  expect_stats_same_or_both_nan(candidate, reference);
  EXPECT_EQ(stats.input_size, correspondences.size());
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_FALSE(stats.used_rvv);
#endif
}

// 高动态范围样本用于保护数值预算：这里仍要求 same-chain 对齐，而不是用更高精度
// 或不同 reduction tree 得到“更好但不同”的结果。
TEST(CorrespondenceTypes, DistanceStatsStressInputMatchesStd)
{
  const auto correspondences = make_distance_stress_correspondences();
  CandidateStats stats;

  const StatsResult reference = distance_stats_std(correspondences);
  const StatsResult candidate = distance_stats_candidate(correspondences, &stats);

  expect_stats_same_or_both_nan(candidate, reference);
}

// n==1 时 production 代码会用 size - 1 作分母，这个既有边界可能得到 NaN。
// 测试只记录并保护同构行为，不把它解释成 RVV 修复目标。
TEST(CorrespondenceTypes, DistanceStatsDocumentsSingleElementBoundary)
{
  pcl::Correspondences correspondences;
  correspondences.emplace_back(7, 9, 3.5f);

  const StatsResult reference = distance_stats_std(correspondences);
  const StatsResult candidate = distance_stats_candidate(correspondences);

  EXPECT_DOUBLE_EQ(candidate.mean, reference.mean);
  EXPECT_TRUE(std::isnan(reference.stddev));
  EXPECT_TRUE(std::isnan(candidate.stddev));
}
