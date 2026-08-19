/*
 * 本文件做什么：
 * 这是 organized_fast_mesh 首阶段 production-shaped diagnostic（生产形态诊断）
 * 的 bench 入口。它比较公开入口在当前 build 中的标量路径和 RVV path，
 * 输出
 * analyze_bench_compare.py 可解析的 label、平均耗时和 checksum（校验和）。
 *
 * 证据边界：
 * QEMU（仿真器）只用于构建、正确性和日志形状；真实性能结论只看板卡或目标硬件
 * repeated benchmark（重复性能测试）。
 */

#include "organized_fast_mesh.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace support = pcl::surface::rvv_ofm_support;

namespace {

constexpr int kDefaultIterations = 20;
constexpr int kDefaultWarmupIterations = 3;

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

int
parseIntArg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter") {
      const std::string selected(argv[i + 1]);
      return selected == requested || selected == "all";
    }
  }
  return true;
}

void
runCase(const std::string& name,
        const int iterations,
        const int warmup_iterations,
        const std::function<std::uint64_t()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i)
    checksum = support::mixChecksum(checksum, fn() + static_cast<std::uint64_t>(i + 1));

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum = support::mixChecksum(
        checksum, fn() + static_cast<std::uint64_t>(warmup_iterations + i + 1));
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(56) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}

void
runMeshCase(const int argc,
            char** argv,
            const std::string& label,
            const int width,
            const int height,
            const support::MeshKind kind,
            const int iterations,
            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = support::makeOrganizedCloud(width, height, true);
  const auto options = support::makeOptions(width, height, kind);
  const auto path = [&cloud, &options]() {
    const auto result = support::generateMeshPublicPath(cloud, options, true);
    return result.checksum;
  };
  runCase(label, iterations, warmup_iterations, path);
}

}  // namespace

int
main(int argc, char** argv)
{
  const int iterations =
      parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: synthetic organized PointXYZ grid, storeShadowedFaces(true), "
               "triangle_pixel_size=1\n";
  std::cout << "Image Size: 1024 x 512\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';

#if defined(__RVV10__)
  std::cout << "Build: RVV public-path candidate (__RVV10__)\n";
#else
  std::cout << "Build: scalar public path\n";
#endif

  runMeshCase(argc,
              argv,
              "ofm_public_quad",
              1024,
              512,
              support::MeshKind::quad,
              iterations,
              warmup_iterations);
  runMeshCase(argc,
              argv,
              "ofm_public_right_cut",
              1024,
              512,
              support::MeshKind::right_cut,
              iterations,
              warmup_iterations);
  runMeshCase(argc,
              argv,
              "ofm_public_left_cut",
              1024,
              512,
              support::MeshKind::left_cut,
              iterations,
              warmup_iterations);
  runMeshCase(argc,
              argv,
              "ofm_public_adaptive_cut",
              1024,
              512,
              support::MeshKind::adaptive_cut,
              iterations,
              warmup_iterations);

  return 0;
}
