/*
 * 本文件做什么：
 * correspondence_rejection_poly 的 QEMU bench smoke（仿真器性能测试小型验证）
 * 和后续 board bench（板卡性能测试）薄入口。它测四个边界：连续数组 edge similarity
 * candidate、production-shaped gather staging（生产形态读点暂存）candidate、
 * accept rate + filter candidate，以及真实 public entry 的固定 seed 完整调用。
 * `production-direct` case（真实生产路径用例）计时生产源码里的公开入口分流；
 * Std build 走 Standard fallback，RVV build 在 gate 命中时走 production RVV helper。
 *
 * 证据边界：
 * QEMU timing（QEMU 计时）只证明构建、路径和日志形状，不提供真实性能结论。
 * 真实性能结论只能来自 board / target hardware repeated benchmark（板卡或目标硬件重复测试）。
 */

#include "correspondence_rejection_poly.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace support =
    pcl::registration::rvv_correspondence_rejection_poly_support;

namespace {

constexpr int kDefaultIterations = 20;
constexpr int kDefaultWarmupIterations = 3;
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
print_banner(const char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

int
parse_int_arg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == flag)
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

void
run_case(const std::string& name,
         const int iterations,
         const int warmup_iterations,
         const std::function<std::uint64_t()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  const auto fold_checksum = [&checksum](const std::uint64_t value,
                                         const int iteration_marker) {
    checksum = (checksum ^ value) * 1099511628211ull;
    checksum =
        (checksum ^ static_cast<std::uint64_t>(iteration_marker)) * 1099511628211ull;
  };
  for (int i = 0; i < warmup_iterations; ++i)
    fold_checksum(fn(), -1 - i);

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
    fold_checksum(fn(), i);
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(58) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  do_not_optimize(checksum);
}

std::vector<float>
make_distances(const std::size_t size, const float scale)
{
  std::vector<float> result(size);
  for (std::size_t i = 0; i < size; ++i)
    result[i] = 0.001f + scale * static_cast<float>((i * 37) % 1009);
  return result;
}

std::uint64_t
run_edge_batch(const std::size_t size)
{
  const auto src = make_distances(size, 0.0005f);
  auto tgt = make_distances(size, 0.00049f);
  for (std::size_t i = 0; i < tgt.size(); i += 17)
    tgt[i] *= 3.0f;
  const auto result = support::edge_similarity_batch_candidate(src, tgt, 0.64f);
  return support::checksum_u8(result.accepted);
}

std::uint64_t
run_edge_gather_staging(const std::size_t edge_count)
{
  const auto source = support::make_source_cloud(edge_count + 1024);
  const auto target = support::make_target_cloud_from_source(*source, edge_count / 8);
  const auto correspondences = support::make_identity_correspondences(source->size());
  const auto edge_pairs = support::make_edge_pairs(edge_count, correspondences.size());
  const auto result = support::edge_similarity_gather_candidate(
      *source, *target, correspondences, edge_pairs, 0.64f);
  return support::checksum_u8(result.accepted);
}

std::uint64_t
run_acceptance_filter(const std::size_t size)
{
  std::vector<int> samples(size);
  std::vector<int> accepted(size);
  for (std::size_t i = 0; i < size; ++i) {
    samples[i] = static_cast<int>((i % 31) + 1);
    accepted[i] = static_cast<int>((i * 13) % samples[i]);
  }
  const auto rates = support::compute_acceptance_rates_candidate(samples, accepted);
  const auto kept = support::filter_by_acceptance_rate_candidate(rates.rates, 0.45f);
  return support::checksum_floats(rates.rates) ^ support::checksum_indices(kept.kept_indices);
}

std::uint64_t
run_full_entry(const std::size_t size, const int rejection_iterations)
{
  const auto source = support::make_source_cloud(size);
  const auto target = support::make_target_cloud_from_source(*source, size / 4);
  const auto correspondences = support::make_identity_correspondences(size);

  pcl::registration::CorrespondenceRejectorPoly<pcl::PointXYZ, pcl::PointXYZ> rejector;
  rejector.setInputSource(source);
  rejector.setInputTarget(target);
  rejector.setIterations(rejection_iterations);
  rejector.setCardinality(3);
  rejector.setSimilarityThreshold(0.8f);

  std::srand(12345);
  pcl::Correspondences remaining;
  rejector.getRemainingCorrespondences(correspondences, remaining);

  std::vector<int> kept;
  kept.reserve(remaining.size());
  for (const auto& corr : remaining)
    kept.push_back(corr.index_query);
  return support::checksum_indices(kept) ^
         (static_cast<std::uint64_t>(remaining.size()) << 32) ^
         static_cast<std::uint64_t>(size);
}

std::uint64_t
run_production_direct(const std::size_t size,
                      const int rejection_iterations,
                      const unsigned int source_seed,
                      const unsigned int sampling_seed)
{
  const auto source = support::make_source_cloud(size);
  auto target = support::make_target_cloud_from_source(*source, size / 5);
  for (std::size_t i = 0; i < target->size(); i += 11) {
    (*target)[i].x += static_cast<float>((source_seed + i) % 17) * 0.011f;
    (*target)[i].y -= static_cast<float>((source_seed + i) % 13) * 0.007f;
  }

  pcl::Correspondences correspondences;
  correspondences.reserve(size);
  for (std::size_t i = 0; i < size; ++i)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);

  pcl::registration::CorrespondenceRejectorPoly<pcl::PointXYZ, pcl::PointXYZ> rejector;
  rejector.setInputSource(source);
  rejector.setInputTarget(target);
  rejector.setIterations(rejection_iterations);
  rejector.setCardinality(3);
  rejector.setSimilarityThreshold(0.82f);

  std::srand(sampling_seed);
  pcl::Correspondences remaining;
  rejector.getRemainingCorrespondences(correspondences, remaining);

  std::vector<int> kept;
  kept.reserve(remaining.size() * 2);
  for (const auto& corr : remaining) {
    kept.push_back(corr.index_query);
    kept.push_back(corr.index_match);
  }
  return support::checksum_indices(kept) ^
         (static_cast<std::uint64_t>(remaining.size()) << 32) ^
         static_cast<std::uint64_t>(size);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parse_int_arg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parse_int_arg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const int rejection_iterations =
      parse_int_arg(argc, argv, "--rejection-iterations", 512);

  print_banner('=');
  std::cout << "PCL registration/correspondence_rejection_poly RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ correspondences; edge batch, edge gather staging, acceptance filter, and fixed-seed public entry smoke\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  std::cout << "Rejection Iterations: " << rejection_iterations << '\n';
  print_banner('-');

  if (case_enabled(argc, argv, "edge-batch")) {
    run_case("edge-batch candidate 64K",
             iterations,
             warmup_iterations,
             []() { return run_edge_batch(65536); });
    run_case("edge-batch candidate 256K",
             iterations,
             warmup_iterations,
             []() { return run_edge_batch(262144); });
  }

  if (case_enabled(argc, argv, "edge-gather-staging")) {
    run_case("edge-gather-staging candidate 64K",
             iterations,
             warmup_iterations,
             []() { return run_edge_gather_staging(65536); });
    run_case("edge-gather-staging candidate 256K",
             iterations,
             warmup_iterations,
             []() { return run_edge_gather_staging(262144); });
  }

  if (case_enabled(argc, argv, "acceptance-filter")) {
    run_case("accept-rate filter candidate 64K",
             iterations,
             warmup_iterations,
             []() { return run_acceptance_filter(65536); });
    run_case("accept-rate filter candidate 256K",
             iterations,
             warmup_iterations,
             []() { return run_acceptance_filter(262144); });
  }

  if (case_enabled(argc, argv, "full-entry")) {
    run_case("fixed-seed public entry 512 correspondences",
             iterations,
             warmup_iterations,
             [rejection_iterations]() {
               return run_full_entry(512, rejection_iterations);
             });
  }

  if (case_enabled(argc, argv, "production-direct")) {
    run_case("production-direct public entry 2048 correspondences",
             iterations,
             warmup_iterations,
             [rejection_iterations]() {
               return run_production_direct(2048, rejection_iterations, 4242u, 9901u);
             });
    run_case("production-direct public entry 8192 correspondences",
             iterations,
             warmup_iterations,
             [rejection_iterations]() {
               return run_production_direct(8192, rejection_iterations, 5252u, 9902u);
             });
  }

  print_banner('=');
  return 0;
}
