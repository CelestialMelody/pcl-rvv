/*
 * 本文件做什么：
 * 这个 benchmark（性能测试）测 BRISK 2D downsample helper 的真实
 * `brisk::Layer` 构造路径。它既包含 isolated helper-shaped case（只构造
 * 单个派生层），也包含 construct-pyramid case（构造尺度金字塔），用于判断
 * downsample RVV path（RVV 路径）是否值得进入 production integration loop
 * （生产接入闭环）。
 *
 * 证据边界：
 * QEMU 运行只作为 log-shape smoke（日志形状小型验证），不能作为性能结论。
 * 板卡 repeated summary（重复摘要）才可用于是否采纳 production patch。
 */

#include "brisk_2d.h"

#include <pcl/keypoints/brisk_2d.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace brisk = pcl::keypoints::rvv_test::brisk_2d;

namespace
{
struct Options
{
  std::string case_filter = "all";
  int iterations = 100;
  int warmup_iterations = 10;
};

bool
matchesFilter(const std::string& label, const std::string& filter)
{
  return filter == "all" || label.find(filter) != std::string::npos;
}

Options
parseOptions(int argc, char** argv)
{
  Options options;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--case-filter" && i + 1 < argc)
      options.case_filter = argv[++i];
    else if (arg == "--iterations" && i + 1 < argc)
      options.iterations = std::atoi(argv[++i]);
    else if (arg == "--warmup-iterations" && i + 1 < argc)
      options.warmup_iterations = std::atoi(argv[++i]);
  }
  return options;
}

template <typename FuncT>
void
runCase(const std::string& label, const Options& options, FuncT&& func)
{
  if (!matchesFilter(label, options.case_filter))
    return;

  std::uint64_t checksum = 0;
  for (int iter = 0; iter < options.warmup_iterations; ++iter)
    checksum ^= func();

  const auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < options.iterations; ++iter)
    checksum ^= func();
  const auto stop = std::chrono::steady_clock::now();

  const double ms =
      std::chrono::duration<double, std::milli>(stop - start).count() /
      static_cast<double>(std::max(1, options.iterations));

  std::cout << label << ": " << ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << checksum
            << " iterations=" << options.iterations
            << " warmup=" << options.warmup_iterations
            << "\n";
}

std::uint64_t
runHalfSample(int width, int height)
{
  const auto image = brisk::makeSyntheticImage(width, height);
  pcl::keypoints::brisk::Layer source(image, width, height);
  pcl::keypoints::brisk::Layer derived(
      source, pcl::keypoints::brisk::Layer::CommonParams::HALFSAMPLE);
  return brisk::sampledChecksum(derived.getImage());
}

std::uint64_t
runTwoThirdSample(int width, int height)
{
  const auto image = brisk::makeSyntheticImage(width, height);
  pcl::keypoints::brisk::Layer source(image, width, height);
  pcl::keypoints::brisk::Layer derived(
      source, pcl::keypoints::brisk::Layer::CommonParams::TWOTHIRDSAMPLE);
  return brisk::sampledChecksum(derived.getImage());
}

std::uint64_t
runConstructPyramid(int width, int height, int octaves)
{
  const auto image = brisk::makeSyntheticImage(width, height);
  pcl::keypoints::brisk::ScaleSpace scale_space(octaves);
  scale_space.constructPyramid(image, width, height);
  return image.size();
}

std::uint64_t
runPublicCompute(int width, int height)
{
  const auto cloud = brisk::makeSyntheticOrganizedCloud(static_cast<std::size_t>(width),
                                                        static_cast<std::size_t>(height));
  pcl::BriskKeypoint2D<pcl::PointXYZRGBA> detector;
  detector.setThreshold(60);
  detector.setOctaves(4);
  detector.setRemoveInvalid3DKeypoints(false);
  detector.setInputCloud(cloud);

  pcl::PointCloud<pcl::PointWithScale> keypoints;
  detector.compute(keypoints);
  return brisk::keypointChecksum(keypoints);
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);

  runCase("brisk_halfsample_640x480", options, [] { return runHalfSample(640, 480); });
  runCase("brisk_halfsample_641x481_tail", options, [] { return runHalfSample(641, 481); });
  runCase("brisk_twothirdsample_640x480", options, [] { return runTwoThirdSample(640, 480); });
  runCase("brisk_construct_pyramid_640x480", options, [] { return runConstructPyramid(640, 480, 4); });
  runCase("brisk_public_compute_320x240", options, [] { return runPublicCompute(320, 240); });

  return 0;
}
