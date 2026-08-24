/*
 * 本文件做什么：
 * 这个 bench（性能测试）只测 DON 逐点 normal 差 helper 的同边界 Std/RVV
 * 成本。它不包含前置 normal estimation，也不包含真实
 * DifferenceOfNormalsEstimation 对象状态，因此只能作为 diagnostic
 * （诊断）性能证据。
 */

#include "don.h"

#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace don = pcl::features::rvv_test::don;

namespace {

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

struct Args {
  std::size_t points = 262144;
  int iterations = 30;
  int warmup = 4;
  std::string case_filter = "all";
};

struct BenchResult {
  double avg_us = 0.0;
  std::uint64_t checksum = 0;
};

enum class DonCase {
  Full,
  FiniteOnlyNoMask,
  NoSqrtStoreZeroCurvature,
  NormalOnly
};

Args
parseArgs(int argc, char** argv)
{
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "--points" && i + 1 < argc)
      args.points = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
    else if (key == "--iterations" && i + 1 < argc)
      args.iterations = std::atoi(argv[++i]);
    else if (key == "--warmup-iterations" && i + 1 < argc)
      args.warmup = std::atoi(argv[++i]);
    else if (key == "--case-filter" && i + 1 < argc)
      args.case_filter = argv[++i];
  }
  return args;
}

void
fillNormals(std::vector<pcl::Normal>& small, std::vector<pcl::Normal>& large)
{
  for (std::size_t i = 0; i < small.size(); ++i) {
    small[i].normal_x = static_cast<float>(i % 97) * 0.03125f - 1.0f;
    small[i].normal_y = static_cast<float>(i % 53) * 0.0625f - 0.5f;
    small[i].normal_z = static_cast<float>(i % 31) * 0.125f + 0.25f;
    large[i].normal_x = static_cast<float>(i % 89) * 0.015625f - 0.75f;
    large[i].normal_y = static_cast<float>(i % 47) * 0.03125f - 0.25f;
    large[i].normal_z = static_cast<float>(i % 29) * 0.0625f + 0.125f;
  }
}

void
clearOutput(std::vector<pcl::Normal>& output)
{
  for (auto& normal : output) {
    normal.normal_x = 0.0f;
    normal.normal_y = 0.0f;
    normal.normal_z = 0.0f;
    normal.curvature = 0.0f;
  }
}

std::uint64_t
checksum(const std::vector<pcl::Normal>& output)
{
  std::uint64_t seed = 1469598103934665603ull;
  for (const auto& normal : output) {
    const auto qx = static_cast<std::int64_t>(normal.normal_x * 1000000.0f);
    const auto qy = static_cast<std::int64_t>(normal.normal_y * 1000000.0f);
    const auto qz = static_cast<std::int64_t>(normal.normal_z * 1000000.0f);
    const auto qc = static_cast<std::int64_t>(normal.curvature * 1000000.0f);
    seed ^= static_cast<std::uint64_t>(qx) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qy) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qz) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qc) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  }
  return seed;
}

void
computeCase(const DonCase don_case,
            const pcl::Normal* small,
            const pcl::Normal* large,
            pcl::Normal* output,
            const std::size_t count)
{
  switch (don_case) {
  case DonCase::Full:
    don::computeDoNRVV(small, large, output, count);
    break;
  case DonCase::FiniteOnlyNoMask:
    don::computeDoNFiniteOnlyNoMask(small, large, output, count);
    break;
  case DonCase::NoSqrtStoreZeroCurvature:
    don::computeDoNNoSqrtStoreZeroCurvature(small, large, output, count);
    break;
  case DonCase::NormalOnly:
    don::computeDoNNormalOnly(small, large, output, count);
    break;
  }
}

BenchResult
measureDoN(const Args& args, const DonCase don_case)
{
  std::vector<pcl::Normal> small(args.points);
  std::vector<pcl::Normal> large(args.points);
  std::vector<pcl::Normal> output(args.points);
  fillNormals(small, large);
  clearOutput(output);

  for (int i = 0; i < args.warmup; ++i)
    computeCase(don_case, small.data(), large.data(), output.data(), output.size());

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < args.iterations; ++i)
    computeCase(don_case, small.data(), large.data(), output.data(), output.size());
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
  doNotOptimize(output.data());
  const std::uint64_t fingerprint = checksum(output);
  doNotOptimize(fingerprint);
  return {total_us / static_cast<double>(std::max(args.iterations, 1)), fingerprint};
}

void
runCase(const Args& args, const std::string& label, const DonCase don_case)
{
  const BenchResult result = measureDoN(args, don_case);
  std::cout << std::left << std::setw(56) << label + ",points=" + std::to_string(args.points)
            << ": " << std::fixed << std::setprecision(4) << result.avg_us << " us/iter\n";
  std::cout << " Checksum: " << result.checksum << "\n";
}

bool
matchesCase(const std::string& filter, const std::string& label)
{
  return filter == "all" || filter == label ||
         (filter == "don_ablate_all" && label.rfind("don_ablate_", 0) == 0);
}

} // namespace

int
main(int argc, char** argv)
{
  const Args args = parseArgs(argc, argv);
  std::cout << "============================================================\n";
  std::cout << " PCL DON helper Benchmark\n";
  std::cout << " Iterations: " << args.iterations << "\n";
  std::cout << " Warmup Iterations: " << args.warmup << "\n";
  std::cout << " Dataset: normal-pair points=" << args.points << " iterations=" << args.iterations
            << " unit=us/iter\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ defined\n";
#else
  std::cout << " build: __RVV10__ NOT defined\n";
#endif
  std::cout << "============================================================\n";

  if (matchesCase(args.case_filter, "don_normal_pair"))
    runCase(args, "don_normal_pair", DonCase::Full);
  if (matchesCase(args.case_filter, "don_ablate_finite_only_no_mask"))
    runCase(args, "don_ablate_finite_only_no_mask", DonCase::FiniteOnlyNoMask);
  if (matchesCase(args.case_filter, "don_ablate_no_sqrt_store_zero_curvature"))
    runCase(args, "don_ablate_no_sqrt_store_zero_curvature", DonCase::NoSqrtStoreZeroCurvature);
  if (matchesCase(args.case_filter, "don_ablate_normal_only"))
    runCase(args, "don_ablate_normal_only", DonCase::NormalOnly);
  return 0;
}
