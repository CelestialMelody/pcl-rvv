/*
 * 本文件做什么：
 * 这是 TEPTPL test-rvv support 的 RowSourcePolicy（行来源策略）层。它把 full-cloud、
 * source-indexed、dual-indices 和 correspondences 的 row source（每一行点对来源）从
 * shared math pipeline 中分离出来，便于审查不同数据流的取数成本和 valid-index-only
 * 边界。
 *
 * 证据边界：
 * RowSourcePolicy 不是 production dispatch。source-indexed、dual-indices 和
 * correspondences 诊断只覆盖有效索引，不改变非法 index 的 production 行为，也不批准
 * indexed/correspondences 进入生产 RVV 路径。
 */

#pragma once

#include "teptpl_rvv_math.hpp"

#include <pcl/rvv_point_load.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace pcl::registration::rvv_te_pt2plane_lls_support {

// RowSourcePolicy（行来源策略）只属于 test-rvv diagnostic（诊断框架），不是
// production dispatch（生产分流）。它把“row 从哪里来”与 shared math pipeline
//（共享数学流水线：finite mask、a/b/c/d、ATA/ATb 累加）分开，让同一数学路径
// 可以复核 full-cloud、source-indexed、dual-indices 和 correspondences 的取数边界。
//
// 每个 policy 都提供五个审查点：
// - Context：保存该数据流需要的 cloud、indices 或 correspondences 展开结果。
// - row_count：声明当前诊断要处理的 row 上界，避免把容器长度差异混入数学误差。
// - can_use_rvv：只表达测试候选能否进入 RVV 诊断路径；它不是生产 gate。
// - load_chunk：把该数据流的一段 row 映射为 sx/sy/sz、dx/dy/dz、nx/ny/nz。
// - fallback：回到同一 policy 的标量参考链路，用来证明 RVV 诊断没有改变 row 语义。
//
// source-indexed、dual-indices 和 correspondences 诊断均为 valid-index-only（只覆盖
// 有效索引）证据；非法 index 的生产行为不在这里重新定义，也不因诊断 helper 的
// 防御性过滤而改变。
struct FullCloudRowSource {
  // full-cloud：第 k 行固定来自 source[k] + target[k]。这是当前唯一 production
  // candidate 数据流；这里的 policy 仍只是 test-rvv 复用层。
  struct Context {
    const pcl::PointCloud<pcl::PointNormal>& source;
    const pcl::PointCloud<pcl::PointNormal>& target;
  };

  static std::size_t
  row_count(const Context& context)
  {
    return std::min(context.source.size(), context.target.size());
  }

#ifdef __RVV10__
  static bool
  can_use_rvv(const Context&, const std::size_t n)
  {
    return n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
           n <= std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal);
  }

  static void
  load_chunk(const Context&,
             const std::uint8_t* source_base,
             const std::uint8_t* target_base,
             const std::size_t i,
             const std::size_t vl,
             vfloat32m2_t& sx,
             vfloat32m2_t& sy,
             vfloat32m2_t& sz,
             vfloat32m2_t& dx,
             vfloat32m2_t& dy,
             vfloat32m2_t& dz,
             vfloat32m2_t& nx,
             vfloat32m2_t& ny,
             vfloat32m2_t& nz)
  {
    constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
    constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
    constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
    constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
    constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
    constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
        source_base + i * sizeof(pcl::PointNormal), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
        target_base + i * sizeof(pcl::PointNormal), vl, dx, dy, dz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
        target_base + i * sizeof(pcl::PointNormal), vl, nx, ny, nz);
  }
#endif // __RVV10__

  static NormalEquation
  fallback(const Context& context, AccumulationStats* stats = nullptr)
  {
    return accumulate_std_full(context.source, context.target, stats);
  }
};

struct SourceIndexedRowSource {
  // source-indexed：第 k 行来自 source[indices_src[k]] + target[k]。本诊断只覆盖
  // 有效 source index，用来观察单侧 gather 与 shared math pipeline 的组合成本。
  struct Context {
    const pcl::PointCloud<pcl::PointNormal>& source;
    const pcl::Indices& source_indices;
    const pcl::PointCloud<pcl::PointNormal>& target;
  };

  static std::size_t
  row_count(const Context& context)
  {
    return std::min(context.source_indices.size(), context.target.size());
  }

#ifdef __RVV10__
  static bool
  can_use_rvv(const Context& context, const std::size_t n)
  {
    return n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
           context.source.size() <=
               std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
           context.target.size() <=
               std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal);
  }

  static void
  load_chunk(const Context& context,
             const std::uint8_t* source_base,
             const std::uint8_t* target_base,
             const std::size_t i,
             const std::size_t vl,
             vfloat32m2_t& sx,
             vfloat32m2_t& sy,
             vfloat32m2_t& sz,
             vfloat32m2_t& dx,
             vfloat32m2_t& dy,
             vfloat32m2_t& dz,
             vfloat32m2_t& nx,
             vfloat32m2_t& ny,
             vfloat32m2_t& nz)
  {
    constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
    constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
    constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
    constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
    constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
    constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
    const vint32m2_t src_idx_i =
        __riscv_vle32_v_i32m2(context.source_indices.data() + i, vl);
    const vuint32m2_t src_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(
        __riscv_vreinterpret_v_i32m2_u32m2(src_idx_i), vl);
    pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
        source_base, src_offsets, vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kX, kY, kZ>(
        target_base + i * sizeof(pcl::PointNormal), vl, dx, dy, dz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(pcl::PointNormal), kNX, kNY, kNZ>(
        target_base + i * sizeof(pcl::PointNormal), vl, nx, ny, nz);
  }
#endif // __RVV10__

  static NormalEquation
  fallback(const Context& context, AccumulationStats* stats = nullptr)
  {
    return accumulate_std_source_indices(
        context.source, context.source_indices, context.target, stats);
  }
};

struct DualIndexedRowSource {
  // dual-indices：第 k 行来自 source[indices_src[k]] + target[indices_tgt[k]]。
  // 它拆开双侧 gather 成本，不包含 pcl::Correspondence 的 query/match 展开。
  struct Context {
    const pcl::PointCloud<pcl::PointNormal>& source;
    const pcl::Indices& source_indices;
    const pcl::PointCloud<pcl::PointNormal>& target;
    const pcl::Indices& target_indices;
  };

  static std::size_t
  row_count(const Context& context)
  {
    return std::min(context.source_indices.size(), context.target_indices.size());
  }

#ifdef __RVV10__
  static bool
  can_use_rvv(const Context& context, const std::size_t n)
  {
    return n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
           context.source.size() <=
               std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal) &&
           context.target.size() <=
               std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal);
  }

  static void
  load_chunk(const Context& context,
             const std::uint8_t* source_base,
             const std::uint8_t* target_base,
             const std::size_t i,
             const std::size_t vl,
             vfloat32m2_t& sx,
             vfloat32m2_t& sy,
             vfloat32m2_t& sz,
             vfloat32m2_t& dx,
             vfloat32m2_t& dy,
             vfloat32m2_t& dz,
             vfloat32m2_t& nx,
             vfloat32m2_t& ny,
             vfloat32m2_t& nz)
  {
    constexpr std::size_t kX = offsetof(pcl::PointNormal, x);
    constexpr std::size_t kY = offsetof(pcl::PointNormal, y);
    constexpr std::size_t kZ = offsetof(pcl::PointNormal, z);
    constexpr std::size_t kNX = offsetof(pcl::PointNormal, normal_x);
    constexpr std::size_t kNY = offsetof(pcl::PointNormal, normal_y);
    constexpr std::size_t kNZ = offsetof(pcl::PointNormal, normal_z);
    const vint32m2_t src_idx_i =
        __riscv_vle32_v_i32m2(context.source_indices.data() + i, vl);
    const vint32m2_t tgt_idx_i =
        __riscv_vle32_v_i32m2(context.target_indices.data() + i, vl);
    const vuint32m2_t src_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(
        __riscv_vreinterpret_v_i32m2_u32m2(src_idx_i), vl);
    const vuint32m2_t tgt_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(
        __riscv_vreinterpret_v_i32m2_u32m2(tgt_idx_i), vl);
    pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
        source_base, src_offsets, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kX, kY, kZ>(
        target_base, tgt_offsets, vl, dx, dy, dz);
    pcl::rvv_load::indexed_load3_fields_f32m2<pcl::PointNormal, kNX, kNY, kNZ>(
        target_base, tgt_offsets, vl, nx, ny, nz);
  }
#endif // __RVV10__

  static NormalEquation
  fallback(const Context& context, AccumulationStats* stats = nullptr)
  {
    return accumulate_std_dual_indices(context.source,
                                       context.source_indices,
                                       context.target,
                                       context.target_indices,
                                       stats);
  }
};

template <typename RowSourcePolicy>
inline NormalEquation
accumulate_row_source(typename RowSourcePolicy::Context context,
                      AccumulationStats* stats = nullptr)
{
  // 这是 RowSourcePolicy 的普通诊断入口：policy 决定 row source，shared math
  // pipeline 统一执行 finite mask、逐点公式 staging、保序压缩和标量 tail 累加。
  // 它可以解释历史 indexed/correspondences 方案的语义边界，但不能替代真实
  // public overload 的 production direct evidence。
  const std::size_t n = RowSourcePolicy::row_count(context);
#ifdef __RVV10__
  if (RowSourcePolicy::can_use_rvv(context, n)) {
    NormalEquation eq;
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(context.source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(context.target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      vfloat32m2_t sx, sy, sz, dx, dy, dz, nx, ny, nz;
      RowSourcePolicy::load_chunk(
          context, source_base, target_base, i, vl, sx, sy, sz, dx, dy, dz, nx, ny, nz);
      accumulate_loaded_rows(sx, sy, sz, dx, dy, dz, nx, ny, nz, vl, eq);
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
  return RowSourcePolicy::fallback(context, stats);
}

template <typename RowSourcePolicy>
inline NormalEquation
accumulate_row_source_trusted_dense(typename RowSourcePolicy::Context context,
                                    AccumulationStats* stats = nullptr)
{
  // trusted_dense 诊断要求 source/target 都声明 is_dense，并据此跳过 finite mask
  // 与 vcompress 成本。它只回答“若未来信任 dense 合同会怎样”，不复刻当前
  // production full-cloud 语义，也不证明 invalid lane 合同。
  const std::size_t n = RowSourcePolicy::row_count(context);
#ifdef __RVV10__
  if (RowSourcePolicy::can_use_rvv(context, n) && context.source.is_dense &&
      context.target.is_dense) {
    NormalEquation eq;
    const auto* source_base =
        reinterpret_cast<const std::uint8_t*>(context.source.points.data());
    const auto* target_base =
        reinterpret_cast<const std::uint8_t*>(context.target.points.data());
    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      vfloat32m2_t sx, sy, sz, dx, dy, dz, nx, ny, nz;
      RowSourcePolicy::load_chunk(
          context, source_base, target_base, i, vl, sx, sy, sz, dx, dy, dz, nx, ny, nz);
      accumulate_loaded_rows_trusted_dense(
          sx, sy, sz, dx, dy, dz, nx, ny, nz, vl, eq);
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
  return RowSourcePolicy::fallback(context, stats);
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_support
