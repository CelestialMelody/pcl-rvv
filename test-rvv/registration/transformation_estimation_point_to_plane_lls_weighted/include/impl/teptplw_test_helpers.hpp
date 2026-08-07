/*
 * 本文件做什么：
 * transformation_estimation_point_to_plane_lls_weighted gtest 源码的薄 helper 层。
 * 它把 fixtures/assertions 暴露成 test body 易读的名字，不包含新的证据逻辑。
 */

#pragma once

#include "teptplw_assertions.hpp"
#include "teptplw_fixtures.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <cstddef>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_test {

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;
namespace prod_detail = pcl::registration::detail;

inline pcl::PointCloud<pcl::PointNormal>
makeSurfaceCloud(const int grid_radius, const float step)
{
  return diag::make_surface_cloud(grid_radius, step);
}

inline pcl::PointCloud<pcl::PointNormal>
makeTargetCloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  return diag::make_target_cloud(source);
}

inline std::vector<float>
makeWeights(const std::size_t n)
{
  return diag::make_weights(n);
}

inline pcl::PointCloud<pcl::PointXYZ>
copySourceAsXYZ(const pcl::PointCloud<pcl::PointNormal>& source)
{
  return diag::copy_source_as_xyz(source);
}

inline pcl::PointCloud<pcl::PointXYZINormal>
copyTargetAsXYZINormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  return diag::copy_target_as_xyzinormal(target);
}

inline pcl::PointCloud<TEPTPLWDoubleNormalTarget>
copyTargetAsDoubleNormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  return diag::copy_target_as_double_normal(target);
}

inline pcl::Correspondences
makeWeightedCorrespondences(const std::size_t n)
{
  return diag::make_weighted_correspondences(n);
}

inline pcl::Indices
makeSourceIndices(const std::size_t n)
{
  return diag::make_source_indices(n);
}

inline pcl::Indices
makeTargetIndices(const std::size_t n)
{
  return diag::make_target_indices(n);
}

inline void
expectMatrixNear(const Eigen::Matrix4f& actual,
                 const Eigen::Matrix4f& expected,
                 const float tolerance)
{
  expect_matrix_near(actual, expected, tolerance);
}

inline void
expectNormalEquationWithinBudget(const diag::NormalEquation& actual,
                                 const diag::NormalEquation& expected,
                                 const double ata_abs_tolerance,
                                 const double ata_rel_tolerance,
                                 const double atb_abs_tolerance,
                                 const double atb_rel_tolerance)
{
  expect_normal_equation_within_budget(actual,
                                       expected,
                                       ata_abs_tolerance,
                                       ata_rel_tolerance,
                                       atb_abs_tolerance,
                                       atb_rel_tolerance);
}

inline std::vector<WeightedFusedFormulaCase>
weightedFusedFormulaCases()
{
  return weighted_fused_formula_cases();
}

inline void
expectWeightedFusedCandidateMatchesBlockAndStd(
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
  expect_weighted_fused_candidate_matches_block_and_std(candidate,
                                                        source,
                                                        target,
                                                        weights,
                                                        ata_abs_tolerance,
                                                        ata_rel_tolerance,
                                                        atb_abs_tolerance,
                                                        atb_rel_tolerance,
                                                        matrix_tolerance);
}

template <typename PointSource, typename PointTarget>
void
expectGenericAbcFusedCandidatesMatchStd(
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
  expect_generic_abc_fused_candidates_match_std(case_name,
                                                source,
                                                target,
                                                weights,
                                                ata_abs_tolerance,
                                                ata_rel_tolerance,
                                                atb_abs_tolerance,
                                                atb_rel_tolerance,
                                                matrix_tolerance);
}

template <typename PointSource, typename PointTarget>
void
expectGenericDAndAbcdFusedCandidatesMatchStd(
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
  expect_generic_d_and_abcd_fused_candidates_match_std(case_name,
                                                       source,
                                                       target,
                                                       weights,
                                                       ata_abs_tolerance,
                                                       ata_rel_tolerance,
                                                       atb_abs_tolerance,
                                                       atb_rel_tolerance,
                                                       matrix_tolerance);
}

inline void
expectProductionEquationWithinBudget(
    const prod_detail::PointToPlaneLLSWeightedNormalEquation& actual,
    const prod_detail::PointToPlaneLLSWeightedNormalEquation& expected,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance)
{
  expect_production_equation_within_budget(actual,
                                           expected,
                                           ata_abs_tolerance,
                                           ata_rel_tolerance,
                                           atb_abs_tolerance,
                                           atb_rel_tolerance);
}

inline Eigen::Matrix4f
solveProductionEquation(prod_detail::PointToPlaneLLSWeightedNormalEquation eq)
{
  return solve_production_equation(eq);
}

#if defined(__RVV10__)
template <typename PointSource, typename PointTarget>
void
expectProductionDefaultFusedAbcdIlpMatchesStd(
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
  expect_production_default_fused_abcd_ilp_matches_std(case_name,
                                                       source,
                                                       target,
                                                       weights,
                                                       ata_abs_tolerance,
                                                       ata_rel_tolerance,
                                                       atb_abs_tolerance,
                                                       atb_rel_tolerance,
                                                       matrix_tolerance);
}
#endif

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_test
