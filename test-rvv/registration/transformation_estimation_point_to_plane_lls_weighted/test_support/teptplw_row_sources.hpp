/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的标量 RowSourcePolicy / WeightPolicy 参考路径。
 */

#pragma once

#include "teptplw_common.hpp"

#include <pcl/correspondence.h>
#include <pcl/types.h>

#include <algorithm>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

inline NormalEquation
accumulate_std_full(const pcl::PointCloud<pcl::PointNormal>& source,
                    const pcl::PointCloud<pcl::PointNormal>& target,
                    const std::vector<float>& weights,
                    AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  const std::size_t n = std::min(std::min(source.size(), target.size()), weights.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (!finite_point_and_normal(source[i], target[i]))
      continue;
    accumulate_weighted_row(source[i].x,
                            source[i].y,
                            source[i].z,
                            target[i].x,
                            target[i].y,
                            target[i].z,
                            target[i].normal_x,
                            target[i].normal_y,
                            target[i].normal_z,
                            weights[i],
                            eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

inline NormalEquation
accumulate_std_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                               const pcl::PointCloud<pcl::PointNormal>& target,
                               const pcl::Correspondences& correspondences,
                               AccumulationStats* stats = nullptr)
{
  NormalEquation eq;
  for (const auto& correspondence : correspondences) {
    if (correspondence.index_query < 0 || correspondence.index_match < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(correspondence.index_query);
    const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    if (!finite_point_and_normal(source[src_index], target[tgt_index]))
      continue;
    accumulate_weighted_row(source[src_index].x,
                            source[src_index].y,
                            source[src_index].z,
                            target[tgt_index].x,
                            target[tgt_index].y,
                            target[tgt_index].z,
                            target[tgt_index].normal_x,
                            target[tgt_index].normal_y,
                            target[tgt_index].normal_z,
                            correspondence.weight,
                            eq);
  }
  if (stats) {
    stats->input_points = correspondences.size();
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

inline NormalEquation
accumulate_std_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                              const pcl::Indices& source_indices,
                              const pcl::PointCloud<pcl::PointNormal>& target,
                              const std::vector<float>& weights,
                              AccumulationStats* stats = nullptr)
{
  // source-indexed（源索引路径）对应公开 overload 中的 source index stream：
  // 第 k 行使用 source[indices[k]]、target[k] 和 weights[k]。这里保留防御性
  // index 检查，测试/bench 合同仍只声明 valid-index-only（只覆盖有效索引）。
  NormalEquation eq;
  const std::size_t n =
      std::min(std::min(source_indices.size(), target.size()), weights.size());
  for (std::size_t k = 0; k < n; ++k) {
    if (source_indices[k] < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(source_indices[k]);
    if (src_index >= source.size())
      continue;
    if (!finite_point_and_normal(source[src_index], target[k]))
      continue;
    accumulate_weighted_row(source[src_index].x,
                            source[src_index].y,
                            source[src_index].z,
                            target[k].x,
                            target[k].y,
                            target[k].z,
                            target[k].normal_x,
                            target[k].normal_y,
                            target[k].normal_z,
                            weights[k],
                            eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

inline NormalEquation
accumulate_std_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<pcl::PointNormal>& target,
                            const pcl::Indices& target_indices,
                            const std::vector<float>& weights,
                            AccumulationStats* stats = nullptr)
{
  // dual-indices（双索引路径）把 source 和 target 的 row source 拆成两条独立
  // index stream；weight 仍按 row 序号 k 从连续 weights[k] 读取。
  NormalEquation eq;
  const std::size_t n = std::min(
      std::min(source_indices.size(), target_indices.size()), weights.size());
  for (std::size_t k = 0; k < n; ++k) {
    if (source_indices[k] < 0 || target_indices[k] < 0)
      continue;
    const auto src_index = static_cast<std::size_t>(source_indices[k]);
    const auto tgt_index = static_cast<std::size_t>(target_indices[k]);
    if (src_index >= source.size() || tgt_index >= target.size())
      continue;
    if (!finite_point_and_normal(source[src_index], target[tgt_index]))
      continue;
    accumulate_weighted_row(source[src_index].x,
                            source[src_index].y,
                            source[src_index].z,
                            target[tgt_index].x,
                            target[tgt_index].y,
                            target[tgt_index].z,
                            target[tgt_index].normal_x,
                            target[tgt_index].normal_y,
                            target[tgt_index].normal_z,
                            weights[k],
                            eq);
  }
  if (stats) {
    stats->input_points = n;
    stats->accepted_points = eq.accepted_points;
    stats->used_rvv = false;
  }
  return eq;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
