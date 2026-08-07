/*
 * 本文件做什么：
 * transformation_estimation_point_to_plane_lls_weighted 的 gtest 断言、误差预算和
 * candidate 对拍 helper。它只服务专项测试，不进入 bench 计时边界。
 */

#pragma once

#include "teptplw_candidates.hpp"

#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_test {

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;
namespace prod_detail = pcl::registration::detail;

inline void
expect_matrix_near(const Eigen::Matrix4f& actual,
                   const Eigen::Matrix4f& expected,
                   const float tolerance)
{
  // 逐元素报错能直接指出旋转或平移项偏离。
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(actual(row, col), expected(row, col), tolerance)
          << "row=" << row << " col=" << col;
}

inline void
expect_normal_equation_within_budget(const diag::NormalEquation& actual,
                                     const diag::NormalEquation& expected,
                                     const double ata_abs_tolerance,
                                     const double ata_rel_tolerance,
                                     const double atb_abs_tolerance,
                                     const double atb_rel_tolerance)
{
  // block-reduction 会改变跨 lane 累加树，所以同时检查绝对预算和相对预算。
  const double ata_norm = std::max(expected.ata.norm(), 1.0);
  const double atb_norm = std::max(expected.atb.norm(), 1.0);
  EXPECT_EQ(actual.accepted_points, expected.accepted_points);
  EXPECT_LE((actual.ata - expected.ata).norm(),
            ata_abs_tolerance + ata_rel_tolerance * ata_norm);
  EXPECT_LE((actual.atb - expected.atb).norm(),
            atb_abs_tolerance + atb_rel_tolerance * atb_norm);
}

using WeightedFusedFormulaAccumulateFn =
    diag::NormalEquation (*)(const pcl::PointCloud<pcl::PointNormal>&,
                             const pcl::PointCloud<pcl::PointNormal>&,
                             const std::vector<float>&,
                             diag::AccumulationStats*);

struct WeightedFusedFormulaCase {
  const char* name;
  WeightedFusedFormulaAccumulateFn accumulate;
};

inline std::vector<WeightedFusedFormulaCase>
weighted_fused_formula_cases()
{
  return {{"abc-fused", diag::accumulate_candidate_full_block_fused_abc},
          {"abc-fused-ilp", diag::accumulate_candidate_full_block_fused_abc_ilp},
          {"d-six-term-fma", diag::accumulate_candidate_full_block_fused_d_six_term},
          {"d-six-term-fma-ilp",
           diag::accumulate_candidate_full_block_fused_d_six_term_ilp},
          {"d-displacement-fused",
           diag::accumulate_candidate_full_block_fused_d_displacement},
          {"d-displacement-fused-ilp",
           diag::accumulate_candidate_full_block_fused_d_displacement_ilp},
          {"abcd-fused", diag::accumulate_candidate_full_block_fused_abcd},
          {"abcd-fused-ilp", diag::accumulate_candidate_full_block_fused_abcd_ilp}};
}

inline void
expect_weighted_fused_candidate_matches_block_and_std(
    const WeightedFusedFormulaCase& candidate,
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance,
    const float matrix_tolerance)
{
  // fused formula diagnostic 先以当前 block-reduction 为 A，再与标量 reference 对拍。
  SCOPED_TRACE(candidate.name);
  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(source, target, weights, &block_stats);
  const diag::NormalEquation candidate_eq =
      candidate.accumulate(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_stats.accepted_points, block_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, block_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expect_normal_equation_within_budget(candidate_eq,
                                       std_eq,
                                       ata_abs_tolerance,
                                       ata_rel_tolerance,
                                       atb_abs_tolerance,
                                       atb_rel_tolerance);
  expect_normal_equation_within_budget(candidate_eq,
                                       block_eq,
                                       ata_abs_tolerance,
                                       ata_rel_tolerance,
                                       atb_abs_tolerance,
                                       atb_rel_tolerance);
  expect_matrix_near(
      diag::solve_normal_equation(candidate_eq),
      diag::solve_normal_equation(std_eq),
      matrix_tolerance);
  expect_matrix_near(
      diag::solve_normal_equation(candidate_eq),
      diag::solve_normal_equation(block_eq),
      matrix_tolerance);
}

template <typename PointSource, typename PointTarget>
void
expect_generic_abc_fused_candidates_match_std(
    const char* case_name,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance,
    const float matrix_tolerance)
{
  // generic abc-fused candidate 只扩展 source/target layout gate。
  SCOPED_TRACE(case_name);
  diag::AccumulationStats std_stats;
  diag::AccumulationStats abc_stats;
  diag::AccumulationStats ilp_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation abc_eq =
      diag::accumulate_candidate_full_block_fused_abc_layout_gated(
          source, target, weights, &abc_stats);
  const diag::NormalEquation ilp_eq =
      diag::accumulate_candidate_full_block_fused_abc_ilp_layout_gated(
          source, target, weights, &ilp_stats);

  EXPECT_EQ(abc_stats.input_points, std_stats.input_points);
  EXPECT_EQ(ilp_stats.input_points, std_stats.input_points);
  EXPECT_EQ(abc_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(ilp_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(abc_eq.accepted_points, std_eq.accepted_points);
  EXPECT_EQ(ilp_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(abc_stats.used_rvv);
  EXPECT_TRUE(ilp_stats.used_rvv);
#else
  EXPECT_FALSE(abc_stats.used_rvv);
  EXPECT_FALSE(ilp_stats.used_rvv);
#endif
  expect_normal_equation_within_budget(abc_eq,
                                       std_eq,
                                       ata_abs_tolerance,
                                       ata_rel_tolerance,
                                       atb_abs_tolerance,
                                       atb_rel_tolerance);
  expect_normal_equation_within_budget(ilp_eq,
                                       std_eq,
                                       ata_abs_tolerance,
                                       ata_rel_tolerance,
                                       atb_abs_tolerance,
                                       atb_rel_tolerance);
  expect_matrix_near(diag::solve_normal_equation(abc_eq),
                     diag::solve_normal_equation(std_eq),
                     matrix_tolerance);
  expect_matrix_near(diag::solve_normal_equation(ilp_eq),
                     diag::solve_normal_equation(std_eq),
                     matrix_tolerance);
}

template <typename PointSource, typename PointTarget>
void
expect_generic_d_and_abcd_fused_candidates_match_std(
    const char* case_name,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance,
    const float matrix_tolerance)
{
  // D 类 generic candidate 覆盖 d-six-term、d-displacement 以及 abcd 组合。
  SCOPED_TRACE(case_name);
  diag::AccumulationStats std_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);

  const auto check = [&](const char* candidate_name,
                         const diag::NormalEquation& candidate_eq,
                         const diag::AccumulationStats& candidate_stats) {
    SCOPED_TRACE(candidate_name);
    EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
    EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
    EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  #ifdef __RVV10__
    EXPECT_TRUE(candidate_stats.used_rvv);
  #else
    EXPECT_FALSE(candidate_stats.used_rvv);
  #endif
    expect_normal_equation_within_budget(candidate_eq,
                                         std_eq,
                                         ata_abs_tolerance,
                                         ata_rel_tolerance,
                                         atb_abs_tolerance,
                                         atb_rel_tolerance);
    expect_matrix_near(diag::solve_normal_equation(candidate_eq),
                       diag::solve_normal_equation(std_eq),
                       matrix_tolerance);
  };

  diag::AccumulationStats d_six_stats;
  check("d-six-term-fma",
        diag::accumulate_candidate_full_block_fused_d_six_term_layout_gated(
            source, target, weights, &d_six_stats),
        d_six_stats);

  diag::AccumulationStats d_six_ilp_stats;
  check("d-six-term-fma-ilp",
        diag::accumulate_candidate_full_block_fused_d_six_term_ilp_layout_gated(
            source, target, weights, &d_six_ilp_stats),
        d_six_ilp_stats);

  diag::AccumulationStats d_displacement_stats;
  check("d-displacement-fused",
        diag::accumulate_candidate_full_block_fused_d_displacement_layout_gated(
            source, target, weights, &d_displacement_stats),
        d_displacement_stats);

  diag::AccumulationStats d_displacement_ilp_stats;
  check("d-displacement-fused-ilp",
        diag::accumulate_candidate_full_block_fused_d_displacement_ilp_layout_gated(
            source, target, weights, &d_displacement_ilp_stats),
        d_displacement_ilp_stats);

  diag::AccumulationStats abcd_stats;
  check("abcd-fused",
        diag::accumulate_candidate_full_block_fused_abcd_layout_gated(
            source, target, weights, &abcd_stats),
        abcd_stats);

  diag::AccumulationStats abcd_ilp_stats;
  check("abcd-fused-ilp",
        diag::accumulate_candidate_full_block_fused_abcd_ilp_layout_gated(
            source, target, weights, &abcd_ilp_stats),
        abcd_ilp_stats);
}

inline void
expect_production_equation_within_budget(
    const prod_detail::PointToPlaneLLSWeightedNormalEquation& actual,
    const prod_detail::PointToPlaneLLSWeightedNormalEquation& expected,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance)
{
  // production block-reduction 也通过 float lane partial sums 再落到 double。
  const double ata_norm = std::max(expected.ata.norm(), 1.0);
  const double atb_norm = std::max(expected.atb.norm(), 1.0);
  EXPECT_EQ(actual.accepted_points, expected.accepted_points);
  EXPECT_LE((actual.ata - expected.ata).norm(),
            ata_abs_tolerance + ata_rel_tolerance * ata_norm);
  EXPECT_LE((actual.atb - expected.atb).norm(),
            atb_abs_tolerance + atb_rel_tolerance * atb_norm);
}

inline Eigen::Matrix4f
solve_production_equation(prod_detail::PointToPlaneLLSWeightedNormalEquation eq)
{
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  prod_detail::solvePointToPlaneLLSWeightedNormalEquation(eq, matrix);
  return matrix;
}

#if defined(__RVV10__)
template <typename PointSource, typename PointTarget>
void
expect_production_default_fused_abcd_ilp_matches_std(
    const char* case_name,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance,
    const float matrix_tolerance)
{
  SCOPED_TRACE(case_name);
  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  prod_detail::PointToPlaneLLSWeightedNormalEquation candidate_eq;
  const bool used_fused =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudBlockRVV(
          source, target, weights, candidate_eq, &candidate_stats);

  EXPECT_TRUE(used_fused);
  EXPECT_TRUE(candidate_stats.used_rvv);
  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  expect_production_equation_within_budget(candidate_eq,
                                           std_eq,
                                           ata_abs_tolerance,
                                           ata_rel_tolerance,
                                           atb_abs_tolerance,
                                           atb_rel_tolerance);
  expect_matrix_near(solve_production_equation(candidate_eq),
                     solve_production_equation(std_eq),
                     matrix_tolerance);
}
#endif

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_test
