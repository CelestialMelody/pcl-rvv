/*
 * 本文件做什么：
 * 这是 color_gradient_modality 的 production-shaped diagnostic
 *（生产形态诊断）bench（性能测试）入口。Std build 运行标量 reference
 *（参考链路）；RVV build 运行 candidate（候选链路）。当前 case 覆盖
 * Sobel+quantize、3x3 dominant filter，以及后续 phase 串接出来的子链路。
 * 输出包含 Dataset、Iterations、Total Time 和 checksum（校验和），供板卡
 * repeated summary（重复板卡摘要）和 Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * sobel/filter/full_chain case 不调用真实 ColorGradientModality::processInputData()，
 * 只证明 case label 写明的 helper 边界。production_process case 会通过公开入口计时，
 * 包含 Gaussian smoothing 和 spread，但仍不包含 feature extraction。QEMU 只用于构建、
 * 正确性和日志形状；性能结论必须来自 board（板卡）或目标硬件。
 */

#include "cgm.h"

#include <pcl/recognition/color_gradient_modality.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace cgm = pcl::recognition::rvv_test::color_gradient_modality;

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

std::vector<pcl::RGB>
makeImage(const std::size_t width, const std::size_t height)
{
  std::vector<pcl::RGB> image(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      image[y * width + x] = pcl::RGB(static_cast<std::uint8_t>((5 * x + 13 * y + 17) & 0xff),
                                      static_cast<std::uint8_t>((19 * x + 7 * y + 23) & 0xff),
                                      static_cast<std::uint8_t>((11 * x + 29 * y + 31) & 0xff));
    }
  }
  return image;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr
makeProductionCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGB>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
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

std::vector<std::uint8_t>
makeQuantizedMap(const std::size_t width, const std::size_t height)
{
  std::vector<std::uint8_t> image(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    for (std::size_t x = 0; x < width; ++x) {
      image[y * width + x] = static_cast<std::uint8_t>(((x * 3 + y * 5 + (x / 7) + (y / 11)) % 9));
    }
  }
  for (std::size_t y = 8; y + 8 < height; y += 17) {
    for (std::size_t x = 8; x + 8 < width; x += 19) {
      for (std::size_t dy = y - 1; dy <= y + 1; ++dy) {
        for (std::size_t dx = x - 1; dx <= x + 1; ++dx) {
          image[dy * width + dx] = static_cast<std::uint8_t>(((x + y) % 8) + 1);
        }
      }
    }
  }
  return image;
}

std::uint64_t
checksumCells(const std::vector<cgm::GradientCell>& cells)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& cell : cells) {
    h ^= cell.quantized;
    h *= 1099511628211ull;
    h ^= static_cast<std::uint64_t>(cell.magnitude * 1000.0f);
    h *= 1099511628211ull;
  }
  return h;
}

std::uint64_t
checksumBytes(const std::vector<std::uint8_t>& cells)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto cell : cells) {
    h ^= cell;
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
checksumFeatureMaps(pcl::ColorGradientModality<pcl::PointXYZRGB>& modality)
{
  std::uint64_t h = 1469598103934665603ull;
  h ^= checksumMap(modality.getQuantizedMap());
  h *= 1099511628211ull;
  h ^= checksumMap(modality.getSpreadedQuantizedMap());
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

  const auto image = makeImage(width, height);
  std::vector<cgm::GradientCell> output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    cgm::computeSobelQuantizedCandidate(image.data(), width, height, 10.0f, output);
    checksum ^= checksumCells(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    cgm::computeSobelQuantizedCandidate(image.data(), width, height, 10.0f, output);
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
runFilterCase(const int argc,
              char** argv,
              const std::string& label,
              const std::size_t width,
              const std::size_t height,
              const int iterations,
              const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto image = makeQuantizedMap(width, height);
  std::vector<std::uint8_t> output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    cgm::filterQuantizedGradientsCandidate(image.data(), width, height, output);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    cgm::filterQuantizedGradientsCandidate(image.data(), width, height, output);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i + 31);
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
runFullChainCase(const int argc,
                 char** argv,
                 const std::string& label,
                 const std::size_t width,
                 const std::size_t height,
                 const int iterations,
                 const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto image = makeImage(width, height);
  std::vector<std::uint8_t> output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    cgm::computeSobelQuantizedFilteredCandidate(image.data(), width, height, 10.0f, output);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    cgm::computeSobelQuantizedFilteredCandidate(image.data(), width, height, 10.0f, output);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i + 47);
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
runFullStencilCase(const int argc,
                   char** argv,
                   const std::string& label,
                   const std::size_t width,
                   const std::size_t height,
                   const int iterations,
                   const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto image = makeImage(width, height);
  std::vector<std::uint8_t> output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    cgm::computeSobelQuantizedStencilCandidate(image.data(), width, height, 10.0f, output);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    cgm::computeSobelQuantizedStencilCandidate(image.data(), width, height, 10.0f, output);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i + 53);
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
  pcl::ColorGradientModality<pcl::PointXYZRGB> modality;
  modality.setGradientMagnitudeThreshold(10.0f);
  modality.setSpreadingSize(8);
  modality.setInputCloud(cloud);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    modality.processInputData();
    checksum ^= checksumFeatureMaps(modality) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    modality.processInputData();
    checksum ^= checksumFeatureMaps(modality) + static_cast<std::uint64_t>(i + 61);
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
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "ColorGradientModality RVV bench\n";
  std::cout << "Dataset: synthetic CGM diagnostic workloads; case label defines exact input shape\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  runCase(argc, argv, "sobel_quantize_320x240", 320, 240, iterations, warmup_iterations);
  runCase(argc, argv, "sobel_quantize_641x481_tail", 641, 481, iterations, warmup_iterations);
  runFilterCase(argc, argv, "filter_dominant_320x240", 320, 240, iterations, warmup_iterations);
  runFilterCase(
      argc, argv, "filter_dominant_641x481_tail", 641, 481, iterations, warmup_iterations);
  runFullChainCase(argc, argv, "full_chain_320x240", 320, 240, iterations, warmup_iterations);
  runFullChainCase(
      argc, argv, "full_chain_641x481_tail", 641, 481, iterations, warmup_iterations);
  runFullStencilCase(
      argc, argv, "full_chain_stencil_320x240", 320, 240, iterations, warmup_iterations);
  runFullStencilCase(
      argc, argv, "full_chain_stencil_641x481_tail", 641, 481, iterations, warmup_iterations);
  runProductionCase(argc, argv, "production_process_320x240", 320, 240, iterations, warmup_iterations);
  runProductionCase(
      argc, argv, "production_process_641x481_tail", 641, 481, iterations, warmup_iterations);
  return 0;
}
