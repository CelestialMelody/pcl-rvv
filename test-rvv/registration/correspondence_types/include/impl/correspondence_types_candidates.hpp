/*
 * 本文件做什么：
 * 这里保存 correspondence_types topic 的 test-only reference（测试参考链路）
 * 和 RVV candidate（RVV 候选链路）。candidate 只复刻
 * registration/include/pcl/registration/impl/correspondence_types.hpp 中三个 helper
 * 的局部循环语义，用于 QEMU correctness（QEMU 正确性验证）、bench smoke
 * （小型性能测试日志形状验证）和 asm smoke（反汇编路径验证）。
 *
 * 证据边界：
 * 本文件不修改 production（生产源码），也不证明真实 public helper 已经命中 RVV。
 * distance stats（距离统计）候选刻意保持标量累加顺序，只把 chunk 内字段加载和
 * float-square staging（单精度平方暂存）前移到 RVV，避免首阶段引入 reduction tree
 * （规约树）差异。
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include <pcl/correspondence.h>
#include <pcl/registration/correspondence_types.h>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_correspondence_types_support {

struct StatsResult {
  double mean{0.0};
  double stddev{0.0};
};

struct CandidateStats {
  bool used_rvv{false};
  std::size_t input_size{0};
};

inline constexpr bool
correspondence_layout_supported()
{
  return sizeof(pcl::index_t) == sizeof(std::int32_t) &&
         std::is_standard_layout<pcl::Correspondence>::value &&
         offsetof(pcl::Correspondence, index_query) == 0 &&
         offsetof(pcl::Correspondence, index_match) == sizeof(std::int32_t) &&
         offsetof(pcl::Correspondence, distance) == sizeof(std::int32_t) * 2;
}

inline StatsResult
distance_stats_std(const pcl::Correspondences& correspondences)
{
  StatsResult result;
  pcl::registration::getCorDistMeanStd(
      correspondences, result.mean, result.stddev);
  return result;
}

inline void
extract_field_scalar(const pcl::Correspondences& correspondences,
                     const std::size_t field_offset,
                     pcl::Indices& indices,
                     CandidateStats* stats)
{
  indices.resize(correspondences.size());
  if (stats) {
    stats->used_rvv = false;
    stats->input_size = correspondences.size();
  }

  for (std::size_t i = 0; i < correspondences.size(); ++i) {
    const auto* base = reinterpret_cast<const std::uint8_t*>(&correspondences[i]);
    const auto* field = reinterpret_cast<const pcl::index_t*>(base + field_offset);
    indices[i] = *field;
  }
}

inline void
query_indices_std(const pcl::Correspondences& correspondences, pcl::Indices& indices)
{
  extract_field_scalar(correspondences, offsetof(pcl::Correspondence, index_query), indices, nullptr);
}

inline void
match_indices_std(const pcl::Correspondences& correspondences, pcl::Indices& indices)
{
  extract_field_scalar(correspondences, offsetof(pcl::Correspondence, index_match), indices, nullptr);
}

inline void
query_indices_candidate(const pcl::Correspondences& correspondences,
                        pcl::Indices& indices,
                        CandidateStats* stats = nullptr)
{
  indices.resize(correspondences.size());
  if (stats) {
    stats->used_rvv = false;
    stats->input_size = correspondences.size();
  }

  if (correspondences.empty())
    return;

#ifdef __RVV10__
  if (correspondence_layout_supported()) {
    constexpr std::ptrdiff_t kStride = static_cast<std::ptrdiff_t>(sizeof(pcl::Correspondence));
    const auto* base = reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(correspondences.data()) +
        offsetof(pcl::Correspondence, index_query));
    auto* out = reinterpret_cast<std::int32_t*>(indices.data());
    std::size_t i = 0;
    while (i < correspondences.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(correspondences.size() - i);
      const auto* chunk_base = reinterpret_cast<const std::int32_t*>(
          reinterpret_cast<const std::uint8_t*>(base) + i * sizeof(pcl::Correspondence));
      const vint32m2_t values = __riscv_vlse32_v_i32m2(chunk_base, kStride, vl);
      __riscv_vse32_v_i32m2(out + i, values, vl);
      i += vl;
    }
    if (stats)
      stats->used_rvv = true;
    return;
  }
#endif

  extract_field_scalar(correspondences, offsetof(pcl::Correspondence, index_query), indices, stats);
}

inline void
match_indices_candidate(const pcl::Correspondences& correspondences,
                        pcl::Indices& indices,
                        CandidateStats* stats = nullptr)
{
  indices.resize(correspondences.size());
  if (stats) {
    stats->used_rvv = false;
    stats->input_size = correspondences.size();
  }

  if (correspondences.empty())
    return;

#ifdef __RVV10__
  if (correspondence_layout_supported()) {
    constexpr std::ptrdiff_t kStride = static_cast<std::ptrdiff_t>(sizeof(pcl::Correspondence));
    const auto* base = reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(correspondences.data()) +
        offsetof(pcl::Correspondence, index_match));
    auto* out = reinterpret_cast<std::int32_t*>(indices.data());
    std::size_t i = 0;
    while (i < correspondences.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(correspondences.size() - i);
      const auto* chunk_base = reinterpret_cast<const std::int32_t*>(
          reinterpret_cast<const std::uint8_t*>(base) + i * sizeof(pcl::Correspondence));
      const vint32m2_t values = __riscv_vlse32_v_i32m2(chunk_base, kStride, vl);
      __riscv_vse32_v_i32m2(out + i, values, vl);
      i += vl;
    }
    if (stats)
      stats->used_rvv = true;
    return;
  }
#endif

  extract_field_scalar(correspondences, offsetof(pcl::Correspondence, index_match), indices, stats);
}

inline StatsResult
distance_stats_candidate(const pcl::Correspondences& correspondences,
                         CandidateStats* stats = nullptr)
{
  if (stats) {
    stats->used_rvv = false;
    stats->input_size = correspondences.size();
  }

  StatsResult result;
  if (correspondences.empty())
    return result;

#ifdef __RVV10__
  if (correspondence_layout_supported()) {
    constexpr std::ptrdiff_t kStride = static_cast<std::ptrdiff_t>(sizeof(pcl::Correspondence));
    const float* distance_base = reinterpret_cast<const float*>(
        reinterpret_cast<const std::uint8_t*>(correspondences.data()) +
        offsetof(pcl::Correspondence, distance));
    const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
    std::vector<float> distance_chunk(vlmax);
    std::vector<float> square_chunk(vlmax);
    double sum = 0.0;
    double sq_sum = 0.0;
    std::size_t i = 0;
    while (i < correspondences.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(correspondences.size() - i);
      const vfloat32m2_t distance =
          __riscv_vlse32_v_f32m2(
              reinterpret_cast<const float*>(
                  reinterpret_cast<const std::uint8_t*>(distance_base) +
                  i * sizeof(pcl::Correspondence)),
              kStride,
              vl);
      const vfloat32m2_t square = __riscv_vfmul_vv_f32m2(distance, distance, vl);
      __riscv_vse32_v_f32m2(distance_chunk.data(), distance, vl);
      __riscv_vse32_v_f32m2(square_chunk.data(), square, vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        sum += distance_chunk[lane];
        sq_sum += square_chunk[lane];
      }
      i += vl;
    }
    result.mean = sum / static_cast<double>(correspondences.size());
    const double variance =
        (sq_sum - sum * sum / static_cast<double>(correspondences.size())) /
        static_cast<double>(correspondences.size() - 1);
    result.stddev = std::sqrt(variance);
    if (stats)
      stats->used_rvv = true;
    return result;
  }
#endif

  return distance_stats_std(correspondences);
}

inline pcl::Correspondences
make_correspondences(const std::size_t n)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const auto query = static_cast<pcl::index_t>((i * 7) % 100003);
    pcl::index_t match = static_cast<pcl::index_t>((i * 11 + 3) % 200003);
    if (i % 17 == 0)
      match = pcl::UNAVAILABLE;
    const float distance =
        static_cast<float>((i % 251) * 0.125f + ((i / 251) % 7) * 0.03125f);
    correspondences.emplace_back(query, match, distance);
  }
  return correspondences;
}

inline pcl::Correspondences
make_distance_stress_correspondences()
{
  pcl::Correspondences correspondences;
  const float values[] = {
      0.0f,
      1.0e-6f,
      -1.0e-6f,
      1.0f,
      -1.0f,
      64.5f,
      -127.25f,
      4096.0f,
      -4096.0f,
      std::numeric_limits<float>::min(),
      -std::numeric_limits<float>::min(),
      std::numeric_limits<float>::max() / 4096.0f};
  for (std::size_t i = 0; i < 256; ++i) {
    correspondences.emplace_back(static_cast<pcl::index_t>(i),
                                 static_cast<pcl::index_t>(255 - i),
                                 values[i % (sizeof(values) / sizeof(values[0]))]);
  }
  return correspondences;
}

} // namespace pcl::registration::rvv_correspondence_types_support
