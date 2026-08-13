/*
 * 本文件做什么：
 * correspondence_types 的 QEMU bench smoke（仿真器性能测试烟测）和后续板卡 bench 薄入口。
 * 它分别测 index extraction（索引抽取）和 distance stats（距离统计）candidate，
 * 输出 analyze_bench_compare.py 可解析的 `ms/iter` 行与 checksum（校验和）。
 *
 * 证据边界：
 * QEMU timing（QEMU 计时）只用于证明构建、路径和日志形状。真实性能结论必须来自
 * board / target hardware repeated benchmark（板卡或目标硬件重复性能测试）。
 */

#include "correspondence_types.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace support = pcl::registration::rvv_correspondence_types_support;

namespace {

constexpr int kDefaultIterations = 30;
constexpr int kDefaultWarmupIterations = 5;
constexpr std::size_t kBannerWidth = 96;

template <typename T>
inline void
do_not_optimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

void
print_banner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

std::uint64_t
checksum_indices(const pcl::Indices& indices)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto index : indices)
    checksum = (checksum ^ static_cast<std::uint32_t>(index)) * 1099511628211ull;
  return checksum;
}

std::uint64_t
checksum_stats(const support::StatsResult& stats)
{
  const auto mean_scaled = static_cast<std::int64_t>(stats.mean * 1000000.0);
  const auto std_scaled = static_cast<std::int64_t>(stats.stddev * 1000000.0);
  return static_cast<std::uint64_t>(mean_scaled) ^
         (static_cast<std::uint64_t>(std_scaled) << 1);
}

void
run_case(const std::string& name,
         const int iterations,
         const int warmup_iterations,
         const std::function<std::uint64_t()>& fn)
{
  std::uint64_t checksum = 0;
  for (int i = 0; i < warmup_iterations; ++i)
    checksum ^= fn();

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum ^= fn();
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(56) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  do_not_optimize(checksum);
}

int
parse_int_arg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
case_enabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter")
      return std::string(argv[i + 1]) == requested || std::string(argv[i + 1]) == "all";
  }
  return true;
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parse_int_arg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parse_int_arg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const auto small = support::make_correspondences(4096);
  const auto medium = support::make_correspondences(65536);
  const auto large = support::make_correspondences(262144);

  print_banner('=');
  std::cout << "PCL registration/correspondence_types RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic pcl::Correspondence arrays; query/match extraction and distance stats smoke\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  print_banner('-');

  if (case_enabled(argc, argv, "production-index-extract")) {
    run_case("query-index production 4K",
             iterations,
             warmup_iterations,
             [&small]() {
               pcl::Indices indices;
               pcl::registration::getQueryIndices(small, indices);
               return checksum_indices(indices);
             });
    run_case("match-index production 64K",
             iterations,
             warmup_iterations,
             [&medium]() {
               pcl::Indices indices;
               pcl::registration::getMatchIndices(medium, indices);
               return checksum_indices(indices);
             });
    run_case("query+match production 256K",
             iterations,
             warmup_iterations,
             [&large]() {
               pcl::Indices query;
               pcl::Indices match;
               pcl::registration::getQueryIndices(large, query);
               pcl::registration::getMatchIndices(large, match);
               return checksum_indices(query) ^ (checksum_indices(match) << 1);
             });
  }

  if (case_enabled(argc, argv, "index-extract")) {
    run_case("query-index candidate 4K",
             iterations,
             warmup_iterations,
             [&small]() {
               pcl::Indices indices;
               support::query_indices_candidate(small, indices);
               return checksum_indices(indices);
             });
    run_case("match-index candidate 64K",
             iterations,
             warmup_iterations,
             [&medium]() {
               pcl::Indices indices;
               support::match_indices_candidate(medium, indices);
               return checksum_indices(indices);
             });
    run_case("query+match candidate 256K",
             iterations,
             warmup_iterations,
             [&large]() {
               pcl::Indices query;
               pcl::Indices match;
               support::query_indices_candidate(large, query);
               support::match_indices_candidate(large, match);
               return checksum_indices(query) ^ (checksum_indices(match) << 1);
             });
  }

  if (case_enabled(argc, argv, "distance-stats")) {
    run_case("distance-stats candidate 64K",
             iterations,
             warmup_iterations,
             [&medium]() {
               return checksum_stats(support::distance_stats_candidate(medium));
             });
    run_case("distance-stats candidate 256K",
             iterations,
             warmup_iterations,
             [&large]() {
               return checksum_stats(support::distance_stats_candidate(large));
             });
  }

  print_banner('=');
  return 0;
}
