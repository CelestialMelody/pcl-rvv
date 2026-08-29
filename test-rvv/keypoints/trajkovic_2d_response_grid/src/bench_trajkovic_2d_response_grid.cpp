/*
 * 本文件做什么：
 * 这是 TrajkovicKeypoint2D response grid 的 production public benchmark（真实公开入口性能测试）。
 * 每个 case 都调用真实 `compute()`，计时包含 response grid、NMS 和输出构造。Std/RVV 两个构建
 * 的 case 名完全一致，供 `analyze_bench_compare.py` 和 Evidence Doctor（证据体检）做同边界对比。
 */

#include <pcl/keypoints/trajkovic_2d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using PointT = pcl::PointXYZI;
using Detector = pcl::TrajkovicKeypoint2D<PointT, PointT>;

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
  if (options.case_filter == "all")
    return true;
  std::size_t start = 0;
  while (start <= options.case_filter.size())
  {
    const std::size_t comma = options.case_filter.find(',', start);
    const std::string token = options.case_filter.substr(start, comma - start);
    if (token == name)
      return true;
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return false;
}

float
syntheticIntensity(const std::size_t row, const std::size_t col)
{
  float value = static_cast<float>((row * 19 + col * 23) % 257) * 0.0125f;
  value += static_cast<float>((row % 11) * (col % 9)) * 0.014f;
  if (row > 24 && row < 96 && col > 32 && col < 144)
    value += 3.8f;
  if (row > 118 && row < 188 && col > 177 && col < 265)
    value += 5.1f;
  return value;
}

pcl::PointCloud<PointT>::Ptr
makeCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(width * height);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      PointT& point = (*cloud)[row * width + col];
      point.x = static_cast<float>(col);
      point.y = static_cast<float>(row);
      point.z = 1.0f;
      point.intensity = syntheticIntensity(row, col);
    }
  }
  return cloud;
}

double
checksumOutput(const pcl::PointCloud<PointT>& output, const pcl::PointIndices& indices)
{
  double checksum = static_cast<double>(output.size()) * 17.0;
  for (std::size_t i = 0; i < output.size(); ++i)
  {
    checksum += static_cast<double>(output[i].intensity) * static_cast<double>((i % 19) + 1);
    checksum += static_cast<double>(indices.indices[i]) * 0.125;
  }
  return checksum;
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
runTrajkovicCase(const Options& options,
                 const std::string& name,
                 const Detector::ComputationMethod method,
                 const std::size_t width,
                 const std::size_t height)
{
  if (!caseEnabled(options, name))
    return;

  auto cloud = makeCloud(width, height);
  double checksum = 0.0;
  const double ms = timeCase(options, [&]() {
    Detector detector(method, 3, 0.02f, 0.20f);
    detector.setNumberOfThreads(1);
    detector.setInputCloud(cloud);
    pcl::PointCloud<PointT> output;
    detector.compute(output);
    return checksumOutput(output, *detector.getKeypointsIndices());
  }, checksum);
  printCase(name, ms, checksum);
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  std::cout << "Dataset: synthetic Trajkovic 2D organized intensity grid width="
            << options.width << " height=" << options.height << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  runTrajkovicCase(options,
                   "four_corners_320x240",
                   Detector::FOUR_CORNERS,
                   static_cast<std::size_t>(options.width),
                   static_cast<std::size_t>(options.height));
  runTrajkovicCase(options,
                   "eight_corners_320x240",
                   Detector::EIGHT_CORNERS,
                   static_cast<std::size_t>(options.width),
                   static_cast<std::size_t>(options.height));
  runTrajkovicCase(options, "four_corners_641x481_tail", Detector::FOUR_CORNERS, 641, 481);
  runTrajkovicCase(options, "eight_corners_641x481_tail", Detector::EIGHT_CORNERS, 641, 481);
  return 0;
}
