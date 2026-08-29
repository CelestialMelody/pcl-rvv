/*
 * 本文件做什么：
 * 这是 surface_normal_modality 的 bench（性能测试）入口。`depth_quantize_*`
 * case 是 production-shaped diagnostic（生产形态诊断）；`production_process_*`
 * case 调用真实 `SurfaceNormalModality::processInputData()`，作为 production direct
 *（真实生产路径证据）。Std build 运行标量 reference（参考链路）；RVV build 运行
 * candidate（候选链路）。输出包含 Dataset、Iterations、Total Time 和 checksum
 *（校验和），供板卡 repeated summary（重复板卡摘要）和 Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * `depth_quantize_*` 不调用真实 SurfaceNormalModality::processInputData()，只证明
 * depth-to-normal / quantize helper 边界。`production_process_*` 的计时边界包含
 * depth-to-normal / quantize、5x5 filter 和 spread，但不包含 `extractFeatures()`。
 * QEMU（仿真器）只用于构建、正确性和日志形状；性能结论必须来自 board（板卡）
 * 或目标硬件。
 */

#include "snm.h"

#include <pcl/recognition/surface_normal_modality.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace snm = pcl::recognition::rvv_test::surface_normal_modality;

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
      if (selected == "all")
        return true;
      std::size_t begin = 0;
      while (begin <= selected.size()) {
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

std::vector<pcl::PointXYZ>
makeDepthCloud(const std::size_t width, const std::size_t height)
{
  std::vector<pcl::PointXYZ> cloud(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      auto& point = cloud[y * width + x];
      point.x = static_cast<float>(x);
      point.y = static_cast<float>(y);
      point.z = 0.55f + 0.0008f * static_cast<float>(x) + 0.0013f * static_cast<float>(y);
    }
  }
  for (std::size_t y = 9; y + 9 < height; y += 17) {
    for (std::size_t x = 11; x + 11 < width; x += 19)
      cloud[y * width + x].z += 0.07f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZRGBA>::Ptr
makeProductionCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGBA>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      auto& point = (*cloud)(x, y);
      point.x = static_cast<float>(x);
      point.y = static_cast<float>(y);
      point.z = 0.55f + 0.0008f * static_cast<float>(x) + 0.0013f * static_cast<float>(y);
      point.r = static_cast<std::uint8_t>((5 * x + 13 * y + 17) & 0xff);
      point.g = static_cast<std::uint8_t>((19 * x + 7 * y + 23) & 0xff);
      point.b = static_cast<std::uint8_t>((11 * x + 29 * y + 31) & 0xff);
      point.a = 255;
    }
  }
  for (std::size_t y = 9; y + 9 < height; y += 17) {
    for (std::size_t x = 11; x + 11 < width; x += 19)
      (*cloud)(x, y).z += 0.07f;
  }
  return cloud;
}

std::uint64_t
checksumCells(const std::vector<snm::NormalCell>& cells)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& cell : cells) {
    h ^= cell.quantized;
    h *= 1099511628211ull;
    h ^= static_cast<std::uint64_t>(cell.angle_degrees * 1000.0f);
    h *= 1099511628211ull;
  }
  return h;
}

std::uint64_t
checksumMap(const pcl::QuantizedMap& map)
{
  const auto* data = map.getData();
  const auto size = map.getWidth() * map.getHeight();
  std::uint64_t h = 1469598103934665603ull;
  for (std::size_t i = 0; i < size; ++i) {
    h ^= data[i];
    h *= 1099511628211ull;
  }
  return h;
}

std::uint64_t
checksumOrientationMap(const pcl::LINEMOD_OrientationMap& map)
{
  std::uint64_t h = 1469598103934665603ull;
  for (std::size_t y = 0; y < map.getHeight(); ++y) {
    for (std::size_t x = 0; x < map.getWidth(); ++x) {
      h ^= static_cast<std::uint64_t>(map(x, y) * 1000.0f);
      h *= 1099511628211ull;
    }
  }
  return h;
}

std::uint64_t
checksumProductionModality(pcl::SurfaceNormalModality<pcl::PointXYZRGBA>& modality)
{
  std::uint64_t h = 1469598103934665603ull;
  h ^= checksumMap(modality.getQuantizedMap());
  h *= 1099511628211ull;
  h ^= checksumMap(modality.getSpreadedQuantizedMap());
  h *= 1099511628211ull;
  h ^= checksumOrientationMap(modality.getOrientationMap());
  h *= 1099511628211ull;
  return h;
}

void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const std::size_t width,
        const std::size_t height,
        const int iterations,
        const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makeDepthCloud(width, height);
  std::vector<snm::NormalCell> output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    snm::computeDepthNormalQuantizedCandidate(cloud.data(), width, height, output);
    checksum ^= checksumCells(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    snm::computeDepthNormalQuantizedCandidate(cloud.data(), width, height, output);
    checksum ^= checksumCells(output) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
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
                  const int iterations,
                  const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makeProductionCloud(width, height);
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    pcl::SurfaceNormalModality<pcl::PointXYZRGBA> modality;
    modality.setInputCloud(cloud);
    modality.processInputData();
    checksum ^= checksumProductionModality(modality) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    pcl::SurfaceNormalModality<pcl::PointXYZRGBA> modality;
    modality.setInputCloud(cloud);
    modality.processInputData();
    checksum ^= checksumProductionModality(modality) + static_cast<std::uint64_t>(i + 53);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
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

  std::cout << "Dataset: surface_normal_modality depth-to-normal diagnostic\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
#if defined(__RVV10__)
  std::cout << "Implementation: RVV candidate\n";
#else
  std::cout << "Implementation: Std scalar\n";
#endif

  runCase(argc, argv, "depth_quantize_320x240", 320, 240, iterations, warmup_iterations);
  runCase(argc, argv, "depth_quantize_641x481_tail", 641, 481, iterations, warmup_iterations);
  runProductionCase(argc, argv, "production_process_320x240", 320, 240, iterations, warmup_iterations);
  runProductionCase(argc, argv, "production_process_641x481_tail", 641, 481, iterations, warmup_iterations);
  return 0;
}
