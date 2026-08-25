/*
 * 本文件做什么：
 * 这是 organized edge detection（有组织点云边缘检测）depth-label diagnostic
 * 的 benchmark（性能测试）入口。Std build（标量构建）中 `computeDepthLabelsRVV`
 * 会回退到同构标量 helper；RVV build（RVV 构建）中它会执行全有限邻域的 RVV
 * fast path。两边输出同名 case，供 `analyze_bench_compare.py` 做同边界对比。
 *
 * 证据边界：
 * 本 benchmark 只测 test-only helper（测试专用 helper），不能证明 production
 * `OrganizedEdgeBase::compute()` 已经接入 RVV。QEMU 运行只允许作为日志形状检查；
 * 性能结论必须来自板卡或目标硬件 repeated summary（重复采集摘要）。
 */

#include "organized_edge_detection.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace oed = pcl::features::rvv_test::organized_edge_detection;

namespace
{
using Clock = std::chrono::steady_clock;

struct Options
{
  int iterations = 20;
  int warmup = 3;
  int width = 320;
  int height = 240;
  std::string case_filter = "all";
};

bool
readIntArg(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
readStringArg(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = argv[i + 1];
      return true;
    }
  }
  return false;
}

Options
parseOptions(const int argc, char** argv)
{
  Options options;
  readIntArg(argc, argv, "--iterations", options.iterations);
  readIntArg(argc, argv, "--warmup", options.warmup);
  readIntArg(argc, argv, "--width", options.width);
  readIntArg(argc, argv, "--height", options.height);
  readStringArg(argc, argv, "--case-filter", options.case_filter);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  options.width = std::max(options.width, 16);
  options.height = std::max(options.height, 16);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  return options.case_filter == "all" || options.case_filter == name;
}

std::vector<float>
makeDepthGrid(const std::size_t width, const std::size_t height, const bool with_invalids)
{
  std::vector<float> z(width * height, 2.0f);
  const std::size_t left = width / 4;
  const std::size_t right = width - left;
  const std::size_t top = height / 4;
  const std::size_t bottom = height - top;
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      float depth = 2.0f + 0.00001f * static_cast<float>((row * 7 + col * 11) % 17);
      if (row >= top && row < bottom && col >= left && col < right)
        depth = 1.70f + 0.00002f * static_cast<float>((row * 5 + col * 3) % 13);
      z[row * width + col] = depth;
    }
  }

  if (with_invalids)
  {
    const std::size_t stride = std::max<std::size_t>(width / 17, 5);
    for (std::size_t row = 3; row + 3 < height; row += stride)
    {
      for (std::size_t col = 3; col + 3 < width; col += stride + 1)
      {
        z[row * width + col] = std::numeric_limits<float>::quiet_NaN();
        z[row * width + col + 1] = std::numeric_limits<float>::quiet_NaN();
      }
    }
  }
  return z;
}

template <typename Fn>
double
timeCase(const Options& options, Fn&& fn, double& checksum)
{
  for (int i = 0; i < options.warmup; ++i)
    checksum += fn();

  const auto t0 = Clock::now();
  for (int i = 0; i < options.iterations; ++i)
    checksum += fn();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() /
         static_cast<double>(options.iterations);
}

void
printCase(const std::string& name, const double ms_per_iter, const double checksum)
{
  std::cout << name << ": " << ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << checksum << "\n";
}

void
runDepthCase(const Options& options,
             const std::string& name,
             const std::size_t width,
             const std::size_t height,
             const bool with_invalids)
{
  if (!caseEnabled(options, name))
    return;

  const std::vector<float> z = makeDepthGrid(width, height, with_invalids);
  std::vector<std::uint32_t> labels(z.size(), 0);
  double checksum = 0.0;
  const double ms = timeCase(options, [&]() {
    oed::computeDepthLabelsRVV(z.data(),
                               width,
                               height,
                               0.02f,
                               12,
                               oed::kNanBoundary | oed::kOccluding | oed::kOccluded,
                               labels.data());
    return oed::checksumLabels(labels.data(), labels.size());
  }, checksum);
  printCase(name, ms, checksum);
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  std::cout << "Dataset: synthetic organized edge depth grid width=" << options.width
            << " height=" << options.height << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  runDepthCase(options,
               "depth_labels_finite_320x240",
               static_cast<std::size_t>(options.width),
               static_cast<std::size_t>(options.height),
               false);
  runDepthCase(options, "depth_labels_finite_641x481_tail", 641, 481, false);
  runDepthCase(options,
               "depth_labels_nan_boundary_320x240",
               static_cast<std::size_t>(options.width),
               static_cast<std::size_t>(options.height),
               true);
  return 0;
}
