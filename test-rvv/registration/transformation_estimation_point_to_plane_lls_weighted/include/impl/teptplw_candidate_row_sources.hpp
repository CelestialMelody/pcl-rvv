/*
 * 本文件做什么：
 * weighted point-to-plane LLS diagnostic 的 source-indexed、dual-indices 和
 * correspondences candidate 入口。它们是 row source 诊断证据，不代表 production
 * 已批准这些入口进入 RVV dispatch。
 */

#pragma once

#include "teptplw_reductions.hpp"
#include "teptplw_row_sources.hpp"

#include <pcl/correspondence.h>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>


namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

inline NormalEquation
accumulate_candidate_source_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                    const pcl::Indices& source_indices,
                                    const pcl::PointCloud<pcl::PointNormal>& target,
                                    const std::vector<float>& weights,
                                    AccumulationStats* stats = nullptr)
{
  const std::size_t n =
      std::min(std::min(source_indices.size(), target.size()), weights.size());
#ifdef __RVV10__
  // source-indexed 路径：source 侧由 index stream 决定，所以用 gather 读取；
  // target 和 weights 仍按 row k 连续前进，分别用 stride load 和 contiguous load。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    src_indices.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
      if (source_indices[k] < 0)
        return accumulate_std_source_indices(source, source_indices, target, weights, stats);
      const auto src_index = static_cast<std::size_t>(source_indices[k]);
      if (src_index >= source.size())
        return accumulate_std_source_indices(source, source_indices, target, weights, stats);
      src_indices.push_back(static_cast<std::uint32_t>(src_index));
    }

    NormalEquation eq;
    const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vuint32m2_t v_src_idx =
          __riscv_vle32_v_u32m2(src_indices.data() + i, vl);
      const vuint32m2_t src_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_src_idx, vl);

      vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          src_base, src_offsets, vl, sx, sy, sz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, dx, dy, dz);
      pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
          tgt_base + i * sizeof(pcl::PointNormal), vl, normal_x, normal_y, normal_z);
      const vfloat32m2_t weight = __riscv_vle32_v_f32m2(weights.data() + i, vl);

      vbool16_t keep = finite_mask_f32m2(sx, vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_x, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_y, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_z, vl), vl);

      vfloat32m2_t a, b, c, d, nx, ny, nz;
      staged_weighted_formula(sx,
                              sy,
                              sz,
                              dx,
                              dy,
                              dz,
                              normal_x,
                              normal_y,
                              normal_z,
                              weight,
                              vl,
                              a,
                              b,
                              c,
                              d,
                              nx,
                              ny,
                              nz);
      accumulate_staged_rows(a, b, c, d, nx, ny, nz, keep, vl, eq);
      i += vl;
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_source_indices(source, source_indices, target, weights, stats);
}

inline NormalEquation
accumulate_candidate_dual_indices(const pcl::PointCloud<pcl::PointNormal>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<pcl::PointNormal>& target,
                                  const pcl::Indices& target_indices,
                                  const std::vector<float>& weights,
                                  AccumulationStats* stats = nullptr)
{
  const std::size_t n = std::min(
      std::min(source_indices.size(), target_indices.size()), weights.size());
#ifdef __RVV10__
  // dual-indices 路径：source/target 两侧都用独立 index stream gather，weight 仍按
  // row 序号连续读取。它专门把“双侧 gather”与 correspondences 的 query/match/weight
  // 展开成本分开审查。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      target.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    std::vector<std::uint32_t> tgt_indices;
    src_indices.reserve(n);
    tgt_indices.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
      if (source_indices[k] < 0 || target_indices[k] < 0)
        return accumulate_std_dual_indices(
            source, source_indices, target, target_indices, weights, stats);
      const auto src_index = static_cast<std::size_t>(source_indices[k]);
      const auto tgt_index = static_cast<std::size_t>(target_indices[k]);
      if (src_index >= source.size() || tgt_index >= target.size())
        return accumulate_std_dual_indices(
            source, source_indices, target, target_indices, weights, stats);
      src_indices.push_back(static_cast<std::uint32_t>(src_index));
      tgt_indices.push_back(static_cast<std::uint32_t>(tgt_index));
    }

    NormalEquation eq;
    const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
    const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vuint32m2_t v_src_idx =
          __riscv_vle32_v_u32m2(src_indices.data() + i, vl);
      const vuint32m2_t v_tgt_idx =
          __riscv_vle32_v_u32m2(tgt_indices.data() + i, vl);
      const vuint32m2_t src_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_src_idx, vl);
      const vuint32m2_t tgt_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_tgt_idx, vl);

      vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          src_base, src_offsets, vl, sx, sy, sz);
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
          tgt_base, tgt_offsets, vl, dx, dy, dz);
      pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
          tgt_base, tgt_offsets, vl, normal_x, normal_y, normal_z);
      const vfloat32m2_t weight = __riscv_vle32_v_f32m2(weights.data() + i, vl);

      vbool16_t keep = finite_mask_f32m2(sx, vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_x, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_y, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_z, vl), vl);

      vfloat32m2_t a, b, c, d, nx, ny, nz;
      staged_weighted_formula(sx,
                              sy,
                              sz,
                              dx,
                              dy,
                              dz,
                              normal_x,
                              normal_y,
                              normal_z,
                              weight,
                              vl,
                              a,
                              b,
                              c,
                              d,
                              nx,
                              ny,
                              nz);
      accumulate_staged_rows(a, b, c, d, nx, ny, nz, keep, vl, eq);
      i += vl;
    }
    if (stats) {
      stats->input_points = n;
      stats->accepted_points = eq.accepted_points;
      stats->used_rvv = true;
    }
    return eq;
  }
#endif // __RVV10__
  return accumulate_std_dual_indices(
      source, source_indices, target, target_indices, weights, stats);
}

inline NormalEquation
accumulate_candidate_correspondences(const pcl::PointCloud<pcl::PointNormal>& source,
                                     const pcl::PointCloud<pcl::PointNormal>& target,
                                     const pcl::Correspondences& correspondences,
                                     AccumulationStats* stats = nullptr)
{
#ifdef __RVV10__
  // 对应关系索引路径（correspondences data flow）：production 入口通过
  // ConstCloudIterator 隐藏 index_query/index_match 的间接访问；RVV intrinsic 需要
  // 明确的索引数组才能 gather（离散加载）点字段，所以诊断先标量展开 index/weight。
  constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
  constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
  constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
  constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
  constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
  constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
  if (correspondences.size() >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
      source.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
      target.size() <=
          std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal)) {
    std::vector<std::uint32_t> src_indices;
    std::vector<std::uint32_t> tgt_indices;
    std::vector<float> weights;
    src_indices.reserve(correspondences.size());
    tgt_indices.reserve(correspondences.size());
    weights.reserve(correspondences.size());
    for (const auto& correspondence : correspondences) {
      if (correspondence.index_query < 0 || correspondence.index_match < 0)
        continue;
      const auto src_index = static_cast<std::size_t>(correspondence.index_query);
      const auto tgt_index = static_cast<std::size_t>(correspondence.index_match);
      if (src_index >= source.size() || tgt_index >= target.size())
        continue;
      src_indices.push_back(static_cast<std::uint32_t>(src_index));
      tgt_indices.push_back(static_cast<std::uint32_t>(tgt_index));
      weights.push_back(correspondence.weight);
    }
    if (src_indices.size() >= 64) {
      NormalEquation eq;
      const auto* src_base = reinterpret_cast<const std::uint8_t*>(source.points.data());
      const auto* tgt_base = reinterpret_cast<const std::uint8_t*>(target.points.data());
      for (std::size_t i = 0; i < src_indices.size();) {
        const std::size_t vl = __riscv_vsetvl_e32m2(src_indices.size() - i);
        const vuint32m2_t v_src_idx =
            __riscv_vle32_v_u32m2(src_indices.data() + i, vl);
        const vuint32m2_t v_tgt_idx =
            __riscv_vle32_v_u32m2(tgt_indices.data() + i, vl);
        const vuint32m2_t src_offsets =
            pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_src_idx, vl);
        const vuint32m2_t tgt_offsets =
            pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_tgt_idx, vl);

        vfloat32m2_t sx, sy, sz, dx, dy, dz, normal_x, normal_y, normal_z;
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            src_base, src_offsets, vl, sx, sy, sz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
            tgt_base, tgt_offsets, vl, dx, dy, dz);
        pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
            tgt_base, tgt_offsets, vl, normal_x, normal_y, normal_z);
        const vfloat32m2_t weight = __riscv_vle32_v_f32m2(weights.data() + i, vl);

        vbool16_t keep = finite_mask_f32m2(sx, vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sy, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(sz, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dx, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dy, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(dz, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_x, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_y, vl), vl);
        keep = __riscv_vmand_mm_b16(keep, finite_mask_f32m2(normal_z, vl), vl);

        vfloat32m2_t a, b, c, d, nx, ny, nz;
        staged_weighted_formula(sx,
                                sy,
                                sz,
                                dx,
                                dy,
                                dz,
                                normal_x,
                                normal_y,
                                normal_z,
                                weight,
                                vl,
                                a,
                                b,
                                c,
                                d,
                                nx,
                                ny,
                                nz);
        accumulate_staged_rows(a, b, c, d, nx, ny, nz, keep, vl, eq);
        i += vl;
      }
      if (stats) {
        stats->input_points = correspondences.size();
        stats->accepted_points = eq.accepted_points;
        stats->used_rvv = true;
      }
      return eq;
    }
  }
#endif // __RVV10__
  return accumulate_std_correspondences(source, target, correspondences, stats);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
