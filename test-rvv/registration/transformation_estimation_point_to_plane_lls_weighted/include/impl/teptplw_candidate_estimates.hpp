/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的 estimate wrapper。它只把 normal-equation
 * candidate 接到 solver 和 matrix checksum 证据，不新增 row source 或公式语义。
 */

#pragma once

#include "teptplw_candidate_full_cloud.hpp"
#include "teptplw_candidate_index_pair_family.hpp"
#include "teptplw_candidate_row_sources.hpp"

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

inline Matrix4f
estimate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                  const pcl::PointCloud<pcl::PointNormal>& target,
                  const std::vector<float>& weights,
                  AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_std_full(source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full(const pcl::PointCloud<pcl::PointNormal>& source,
                        const pcl::PointCloud<pcl::PointNormal>& target,
                        const std::vector<float>& weights,
                        AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full(source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_reduction(source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_reduction_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_reduction_layout_gated(
      source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_abc(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_abc(source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_abc_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_fused_abc_layout_gated(
      source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_abc_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_abc_ilp(source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_abc_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_fused_abc_ilp_layout_gated(
      source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_d_six_term(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_fused_d_six_term(
      source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_d_six_term_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_d_six_term_layout_gated(
          source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_d_six_term_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_fused_d_six_term_ilp(
      source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_d_six_term_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_d_six_term_ilp_layout_gated(
          source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_d_displacement(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_fused_d_displacement(
      source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_d_displacement_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_d_displacement_layout_gated(
          source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_d_displacement_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_d_displacement_ilp(
          source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_d_displacement_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_d_displacement_ilp_layout_gated(
          source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_abcd(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_abcd(source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_abcd_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_full_block_fused_abcd_layout_gated(
      source, target, weights, stats));
}

inline Matrix4f
estimate_candidate_full_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_abcd_ilp(source, target, weights, stats));
}

template <typename PointSource, typename PointTarget>
inline Matrix4f
estimate_candidate_full_block_fused_abcd_ilp_layout_gated(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_full_block_fused_abcd_ilp_layout_gated(
          source, target, weights, stats));
}

inline Matrix4f
estimate_std_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<pcl::PointNormal>& target,
                            const std::vector<float>& weights,
                            AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_std_source_indices(source, source_indices, target, weights, stats));
}

inline Matrix4f
estimate_candidate_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<pcl::PointNormal>& target,
                                  const std::vector<float>& weights,
                                  AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_source_indices(source, source_indices, target, weights, stats));
}

inline Matrix4f
estimate_candidate_source_indices_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_source_indices_block_reduction(
      source, source_indices, target, weights, stats));
}

inline Matrix4f
estimate_candidate_source_indices_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_source_indices_block_fused_abcd_ilp(
      source, source_indices, target, weights, stats));
}

inline Matrix4f
estimate_std_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                          const pcl::Indices& source_indices,
                          const pcl::PointCloud<pcl::PointNormal>& target,
                          const pcl::Indices& target_indices,
                          const std::vector<float>& weights,
                          AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_std_dual_indices(
      source, source_indices, target, target_indices, weights, stats));
}

inline Matrix4f
estimate_candidate_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                const pcl::Indices& source_indices,
                                const pcl::PointCloud<pcl::PointNormal>& target,
                                const pcl::Indices& target_indices,
                                const std::vector<float>& weights,
                                AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_dual_indices(
      source, source_indices, target, target_indices, weights, stats));
}

inline Matrix4f
estimate_candidate_dual_indices_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_dual_indices_block_reduction(
      source, source_indices, target, target_indices, weights, stats));
}

inline Matrix4f
estimate_candidate_dual_indices_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Indices& target_indices,
    const std::vector<float>& weights,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_dual_indices_block_fused_abcd_ilp(
          source, source_indices, target, target_indices, weights, stats));
}

inline Matrix4f
estimate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                             const pcl::PointCloud<pcl::PointNormal>& target,
                             const pcl::Correspondences& correspondences,
                             AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_std_correspondences(source, target, correspondences, stats));
}

inline Matrix4f
estimate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                   const pcl::PointCloud<pcl::PointNormal>& target,
                                   const pcl::Correspondences& correspondences,
                                   AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_correspondences(source, target, correspondences, stats));
}

inline Matrix4f
estimate_candidate_correspondences_block_reduction(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(accumulate_candidate_correspondences_block_reduction(
      source, target, correspondences, stats));
}

inline Matrix4f
estimate_candidate_correspondences_block_fused_abcd_ilp(
    const pcl::PointCloud<pcl::PointNormal>& source,
    const pcl::PointCloud<pcl::PointNormal>& target,
    const pcl::Correspondences& correspondences,
    AccumulationStats* stats = nullptr)
{
  return solve_normal_equation(
      accumulate_candidate_correspondences_block_fused_abcd_ilp(
          source, target, correspondences, stats));
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
