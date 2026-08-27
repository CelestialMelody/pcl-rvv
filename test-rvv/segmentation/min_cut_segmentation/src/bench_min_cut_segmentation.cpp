#include "min_cut_segmentation.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
  std::size_t size = 65536;
  std::size_t foreground = 19;
  unsigned int neighbours = 14;
  int iterations = 8;
  int warmup = 2;
  std::string case_filter = "all";
};

Options
parseArgs(int argc, char** argv)
{
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&]() -> const char* {
      return (i + 1 < argc) ? argv[++i] : "";
    };
    if (arg == "--size")
      options.size = static_cast<std::size_t>(std::strtoull(next(), nullptr, 10));
    else if (arg == "--foreground")
      options.foreground = static_cast<std::size_t>(std::strtoull(next(), nullptr, 10));
    else if (arg == "--neighbours")
      options.neighbours = static_cast<unsigned int>(std::strtoul(next(), nullptr, 10));
    else if (arg == "--iterations")
      options.iterations = std::atoi(next());
    else if (arg == "--warmup")
      options.warmup = std::atoi(next());
    else if (arg == "--case-filter")
      options.case_filter = next();
  }
  return options;
}

pcl::PointCloud<pcl::PointXYZ>
makeCloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>((i * 17) % 4099) - 2048) * 0.013f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 29) % 4093) - 2046) * 0.017f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 43) % 4091) - 2045) * 0.011f;
  }
  return cloud;
}

std::vector<int>
makeIndices(const std::size_t n)
{
  std::vector<int> indices(n);
  for (std::size_t i = 0; i < n; ++i)
    indices[i] = static_cast<int>(i);
  return indices;
}

std::vector<pcl::PointXYZ>
makeForeground(const std::size_t n)
{
  std::vector<pcl::PointXYZ> foreground(n);
  for (std::size_t i = 0; i < n; ++i) {
    foreground[i].x = static_cast<float>(static_cast<int>((i * 31) % 1021) - 510) * 0.025f;
    foreground[i].y = static_cast<float>(static_cast<int>((i * 37) % 1031) - 515) * 0.021f;
    foreground[i].z = 0.0f;
  }
  return foreground;
}

void
makeEdges(const std::size_t n, std::vector<int>& sources, std::vector<int>& targets)
{
  sources.clear();
  targets.clear();
  sources.reserve(n * 2);
  targets.reserve(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    sources.push_back(static_cast<int>(i));
    targets.push_back(static_cast<int>((i + 1) % n));
    sources.push_back(static_cast<int>(i));
    targets.push_back(static_cast<int>((i * 7 + 13) % n));
  }
}

template <typename Func>
double
timeCase(const int warmup, const int iterations, Func&& func)
{
  for (int i = 0; i < warmup; ++i)
    func();
  const auto start = Clock::now();
  for (int i = 0; i < iterations; ++i)
    func();
  const auto end = Clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count() /
         static_cast<double>(iterations);
}

bool
caseEnabled(const std::string& filter, const std::string& name)
{
  return filter == "all" || filter == name;
}

} // namespace

int
main(int argc, char** argv)
{
  const auto options = parseArgs(argc, argv);
  const auto cloud = makeCloud(options.size);
  const auto indices = makeIndices(options.size);
  const auto foreground = makeForeground(options.foreground);
  std::vector<int> sources;
  std::vector<int> targets;
  makeEdges(options.size, sources, targets);
  double checksum = 0.0;

  std::cout << "Dataset: min_cut component ablation; size=" << options.size
            << "; foreground=" << options.foreground
            << "; neighbours=" << options.neighbours
            << "; case_filter=" << options.case_filter << '\n';
  std::cout << "Iterations: " << options.iterations << '\n';
  std::cout << "Warmup Iterations: " << options.warmup << '\n';

  if (caseEnabled(options.case_filter, "unary_min_distance")) {
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_min_cut::computeUnaryPotentialsRVV(
          cloud, indices, foreground, 3.8003856 * 3.8003856, 0.8, nullptr);
#else
      const auto summary = pcl_rvv_segmentation_min_cut::computeUnaryPotentialsStd(
          cloud, indices, foreground, 3.8003856 * 3.8003856, 0.8, nullptr);
#endif
      checksum += summary.sink_checksum + summary.source_checksum;
    });
    std::cout << "unary_min_distance: " << avg_ms << " ms / iter\n";
  }

  if (caseEnabled(options.case_filter, "binary_exp_weight")) {
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_min_cut::computeBinaryPotentialsRVV(
          cloud, sources, targets, 16.0, nullptr);
#else
      const auto summary = pcl_rvv_segmentation_min_cut::computeBinaryPotentialsStd(
          cloud, sources, targets, 16.0, nullptr);
#endif
      checksum += summary.weight_checksum;
    });
    std::cout << "binary_exp_weight: " << avg_ms << " ms / iter\n";
  }

  if (caseEnabled(options.case_filter, "buildgraph_potential_batch")) {
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_min_cut::computeBuildGraphPotentialBatchRVV(
          cloud,
          indices,
          foreground,
          3.8003856 * 3.8003856,
          0.8,
          16.0,
          options.neighbours);
#else
      const auto summary = pcl_rvv_segmentation_min_cut::computeBuildGraphPotentialBatchStd(
          cloud,
          indices,
          foreground,
          3.8003856 * 3.8003856,
          0.8,
          16.0,
          options.neighbours);
#endif
      checksum += summary.capacity_checksum + static_cast<double>(summary.edge_count);
    });
    std::cout << "buildgraph_potential_batch: " << avg_ms << " ms / iter\n";
  }

  std::cout << "Checksum: " << static_cast<std::uint64_t>(checksum * 1000000.0) << '\n';
  return 0;
}
