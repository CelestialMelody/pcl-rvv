/*
 * 本文件做什么：
 * 这是 marching_cubes_rbf 的 component ablation（组件消融）bench 入口。
 * Std build 运行标量链路；RVV build 在 __RVV10__ 下运行 RVV matrix fill 和
 * voxel evaluation candidate，同时保留 Eigen solve 为同一标量库边界。
 *
 * 证据边界：
 * 输出里的 full_pipeline case 仍是 production-shaped diagnostic（生产形态诊断），
 * 因为它复刻 voxelizeData() 的主要计算阶段，但没有修改 production 源码，也没有
 * 走真实 public reconstruction dispatch。QEMU 只用于构建、正确性和日志形状；
 * 性能结论必须来自板卡或目标硬件。
 */

#include "marching_cubes_rbf.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace mcrbf = pcl::surface::rvv_marching_cubes_rbf_support;

namespace {

constexpr int kDefaultIterations = 6;
constexpr int kDefaultWarmupIterations = 1;

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
printRun(const std::string& label,
         const int iterations,
         const int warmup_iterations,
         const std::function<mcrbf::RunStats()>& fn)
{
  mcrbf::RunStats last;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    last = fn();
    checksum = mcrbf::mixChecksum(checksum, last.checksum + static_cast<std::uint64_t>(i));
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    last = fn();
    checksum = mcrbf::mixChecksum(checksum, last.checksum + static_cast<std::uint64_t>(i + 17));
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(56) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", active_cells: " << last.active_cells
            << ", grid_values: " << last.grid_values << '\n';
  std::cout << "  Components: prepare=" << std::fixed << std::setprecision(4)
            << last.times.prepare_ms << " ms, matrix=" << last.times.matrix_ms
            << " ms, solve=" << last.times.solve_ms << " ms, voxel="
            << last.times.voxel_ms << " ms, active_scan=" << last.times.active_scan_ms
            << " ms\n";
  doNotOptimize(checksum);
}

void
runFullCase(const int argc,
            char** argv,
            const std::string& label,
            const int points,
            const int resolution,
            const int iterations,
            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;
  printRun(label, iterations, warmup_iterations, [points, resolution]() {
    return mcrbf::runFullCandidatePipeline(points, resolution);
  });
}

void
runMatrixOnlyCase(const int argc,
                  char** argv,
                  const std::string& label,
                  const int points,
                  const int iterations,
                  const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;
  const auto input = mcrbf::makeInput(points);
  const auto centers = mcrbf::makeCenters(input);
  printRun(label, iterations, warmup_iterations, [&centers]() {
    mcrbf::RunStats stats;
    mcrbf::Matrix matrix;
    stats.times.matrix_ms = mcrbf::measureOnce([&]() { mcrbf::fillMatrixCandidate(centers, matrix); });
    stats.grid_values = static_cast<std::size_t>(matrix.rows() * matrix.cols());
    stats.checksum = mcrbf::mixChecksum(static_cast<std::uint64_t>(matrix.rows()),
                                        mcrbf::quantizedDoubleBits(matrix.sum()));
    return stats;
  });
}

template <typename PointT>
void
runProductionNormalPointCase(const int argc,
                             char** argv,
                             const std::string& label,
                             const int points,
                             const int resolution,
                             const int iterations,
                             const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;
  printRun(label, iterations, warmup_iterations, [points, resolution]() {
    return mcrbf::runProductionVoxelizePointNormalPipeline<PointT>(points, resolution);
  });
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "MarchingCubesRBF component diagnostic bench\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  runMatrixOnlyCase(argc, argv, "mcrbf_matrix_fill_n24", 24, iterations, warmup_iterations);
  runMatrixOnlyCase(argc, argv, "mcrbf_matrix_fill_n40", 40, iterations, warmup_iterations);
  runFullCase(argc, argv, "mcrbf_full_pipeline_n24_r18", 24, 18, iterations, warmup_iterations);
  runFullCase(argc, argv, "mcrbf_full_pipeline_n40_r20", 40, 20, iterations, warmup_iterations);
  runFullCase(argc, argv, "mcrbf_full_pipeline_n56_r18", 56, 18, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointNormal>(
      argc, argv, "mcrbf_prod_pointnormal_n24_r18", 24, 18, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointNormal>(
      argc, argv, "mcrbf_prod_pointnormal_n40_r20", 40, 20, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointNormal>(
      argc, argv, "mcrbf_prod_pointnormal_n56_r18", 56, 18, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointXYZINormal>(
      argc, argv, "mcrbf_prod_pointxyzinormal_n24_r18", 24, 18, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointXYZINormal>(
      argc, argv, "mcrbf_prod_pointxyzinormal_n40_r20", 40, 20, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointXYZRGBNormal>(
      argc, argv, "mcrbf_prod_pointxyzrgbnormal_n24_r18", 24, 18, iterations, warmup_iterations);
  runProductionNormalPointCase<pcl::PointXYZRGBNormal>(
      argc, argv, "mcrbf_prod_pointxyzrgbnormal_n40_r20", 40, 20, iterations, warmup_iterations);

  return 0;
}
