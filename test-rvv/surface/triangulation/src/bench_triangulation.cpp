/*
 * 本文件做什么：
 * 这是 triangulation Phase 000 的接入前诊断 bench（性能测试）入口。
 * Std build 运行标量 reference path，RVV build 在 __RVV10__ 下只把规则参数
 * 网格写入换成 RVV candidate，后续 OpenNURBS Evaluate 保持同一标量调用。
 *
 * 证据边界：
 * QEMU（仿真器）只用于构建、correctness（正确性）和日志形状。真实性能
 * 结论必须来自板卡或目标硬件 repeated benchmark（重复性能测试）。
 */

#include "triangulation.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace tri = pcl::surface::rvv_triangulation_support;

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
runCase(const std::string& label,
        const int iterations,
        const int warmup_iterations,
        const std::function<tri::MeshStats()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  tri::MeshStats last;
  for (int i = 0; i < warmup_iterations; ++i) {
    last = fn();
    checksum = tri::mixChecksum(checksum, last.checksum + static_cast<std::uint64_t>(i + 1));
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    last = fn();
    checksum = tri::mixChecksum(
        checksum, last.checksum + static_cast<std::uint64_t>(warmup_iterations + i + 1));
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(56) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << last.points
            << ", polygons: " << last.polygons << '\n';
  doNotOptimize(checksum);
}

void
runGridCase(const int argc,
            char** argv,
            const std::string& label,
            const unsigned resolution,
            const int iterations,
            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto path = [resolution]() {
#if defined(__RVV10__)
    return tri::runParamGridCandidate(resolution);
#else
    return tri::runParamGridReference(resolution);
#endif
  };
  runCase(label, iterations, warmup_iterations, path);
}

void
runSurfaceCase(const int argc,
                char** argv,
                const std::string& label,
                const unsigned resolution,
                const int iterations,
                const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto path = [resolution]() {
#if defined(__RVV10__)
    return tri::runSurfaceCandidate(resolution);
#else
    return tri::runSurfaceReference(resolution);
#endif
  };
  runCase(label, iterations, warmup_iterations, path);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: synthetic clamped ON_NurbsSurface fixture for triangulation diagnostic\n";
  std::cout << "Image Size: synthetic square parameter grids\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build: RVV param-grid candidate (__RVV10__)\n";
#else
  std::cout << "Build: scalar triangulation reference\n";
#endif

  runGridCase(argc, argv, "tri_param_grid_256", 256, iterations, warmup_iterations);
  runGridCase(argc, argv, "tri_param_grid_512", 512, iterations, warmup_iterations);
  runSurfaceCase(argc, argv, "tri_surface_eval_128", 128, iterations, warmup_iterations);
  runSurfaceCase(argc, argv, "tri_surface_eval_256", 256, iterations, warmup_iterations);

  return 0;
}
