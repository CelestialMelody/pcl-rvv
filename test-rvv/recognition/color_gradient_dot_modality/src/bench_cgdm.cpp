/*
 * 本文件做什么：
 * 这是 color_gradient_dot_modality 的 production-shaped diagnostic bench
 *（生产形态诊断）入口。Std build 运行标量 reference；RVV build 运行 candidate。
 * 当前 case 覆盖 dominant map 生成和真实 public entry 的 processInputData()。
 *
 * 证据边界：
 * 这里的 bench 只证明 case label 写明的 helper 或 public entry 边界。QEMU 只用于
 * 构建、正确性和日志形状；性能结论必须来自 board（板卡）或目标硬件。
 */

#include "cgdm.h"

#include <pcl/recognition/color_gradient_dot_modality.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace cgdm = pcl::recognition::rvv_test::color_gradient_dot_modality;

namespace
{
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
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::string(argv[i]) == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::string(argv[i]) == "--case-filter")
    {
      const std::string selected(argv[i + 1]);
      if (selected == "all")
        return true;
      std::size_t begin = 0;
      while (begin <= selected.size())
      {
        const std::size_t end = selected.find(',', begin);
        const auto token = selected.substr(begin, end == std::string::npos ? end : end - begin);
        if (token == requested)
          return true;
        if (end == std::string::npos)
          break;
        begin = end + 1;
      }
      return false;
    }
  }
  return true;
}

std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGB>>
makeProductionCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      auto& point = (*cloud)(x, y);
      point.x = static_cast<float>(x);
      point.y = static_cast<float>(y);
      point.z = 0.1f * static_cast<float>((x + y) & 7);
      point.r = static_cast<std::uint8_t>((5 * x + 13 * y + 17) & 0xff);
      point.g = static_cast<std::uint8_t>((19 * x + 7 * y + 23) & 0xff);
      point.b = static_cast<std::uint8_t>((11 * x + 29 * y + 31) & 0xff);
    }
  }
  return cloud;
}

std::uint64_t
checksumMap(const pcl::QuantizedMap& map)
{
  const auto* data = map.getData();
  const auto size = map.getWidth() * map.getHeight();
  std::uint64_t h = 1469598103934665603ull;
  for (std::size_t i = 0; i < size; ++i)
  {
    h ^= data[i];
    h *= 1099511628211ull;
  }
  return h;
}

std::uint64_t
checksumArtifacts(const cgdm::DominantMapArtifacts& artifacts)
{
  std::uint64_t h = 1469598103934665603ull;
  h ^= checksumMap(artifacts.dominant_map);
  h *= 1099511628211ull;
  h ^= static_cast<std::uint64_t>(artifacts.gradients.size());
  h *= 1099511628211ull;
  return h;
}

void
runDominantCase(const int argc,
                char** argv,
                const std::string& label,
                const std::size_t width,
                const std::size_t height,
                const int bin_size,
                const int iterations,
                const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makeProductionCloud(width, height);
  cgdm::DominantMapArtifacts output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i)
  {
    cgdm::computeDominantMapCandidate(
        cloud->points.data(), width, height, static_cast<std::size_t>(bin_size), 20.0f, output);
    checksum ^= checksumArtifacts(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
  {
    cgdm::computeDominantMapCandidate(
        cloud->points.data(), width, height, static_cast<std::size_t>(bin_size), 20.0f, output);
    checksum ^= checksumArtifacts(output) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed << std::setprecision(4)
            << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (width * height) << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}

void
runProductionCase(const int argc,
                  char** argv,
                  const std::string& label,
                  const std::size_t width,
                  const std::size_t height,
                  const int bin_size,
                  const int iterations,
                  const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makeProductionCloud(width, height);
  pcl::ColorGradientDOTModality<pcl::PointXYZRGB> modality(static_cast<std::size_t>(bin_size));
  modality.setGradientMagnitudeThreshold(20.0f);
  modality.setInputCloud(cloud);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i)
  {
    modality.processInputData();
    checksum ^= checksumMap(modality.getDominantQuantizedMap()) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
  {
    modality.processInputData();
    checksum ^= checksumMap(modality.getDominantQuantizedMap()) + static_cast<std::uint64_t>(i + 31);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed << std::setprecision(4)
            << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (width * height) << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}
} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations = parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "ColorGradientDOTModality RVV bench\n";
  std::cout << "Dataset: synthetic CGDM diagnostic workloads; case label defines exact input shape\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  runDominantCase(argc, argv, "dominant_map_320x240", 320, 240, 4, iterations, warmup_iterations);
  runDominantCase(
      argc, argv, "dominant_map_641x481_tail", 641, 481, 4, iterations, warmup_iterations);
  runProductionCase(argc, argv, "process_input_320x240", 320, 240, 4, iterations, warmup_iterations);
  runProductionCase(
      argc, argv, "process_input_641x481_tail", 641, 481, 4, iterations, warmup_iterations);
  return 0;
}
