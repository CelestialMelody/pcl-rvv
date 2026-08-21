/*
 * 本文件做什么：
 * 这是 debayer topic 的 production-shaped diagnostic（生产形态诊断）bench
 * 入口。Std build 运行标量内区 reference；RVV build 运行测试专用
 * bilinear interior candidate（双线性内区候选）。输出包含 checksum（校验和）
 * 和每个 case 的耗时，供板卡 repeated summary（重复采集摘要）和 Evidence
 * Doctor（证据体检）使用。
 *
 * 证据边界：
 * bench 不调用真实 DeBayer production dispatch（生产分流），也不证明生产源码
 * 已接入 RVV。QEMU 只能用于构建、正确性和日志形状；性能结论必须来自板卡。
 */

#include "debayer.h"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace debayer = pcl::io::rvv_debayer_support;

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

std::vector<std::uint8_t>
makeBayerPattern(const unsigned width, const unsigned height)
{
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height, 0);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x)
      pixels[static_cast<std::size_t>(y) * width + x] =
          static_cast<std::uint8_t>((23 * x + 41 * y + 5 * ((x + y) & 7) + 13) & 0xff);
  }
  return pixels;
}

std::uint64_t
checksumBytes(const std::vector<std::uint8_t>& values)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto value : values) {
    h ^= value;
    h *= 1099511628211ull;
  }
  return h;
}

void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const unsigned width,
        const unsigned height,
        const unsigned padded_bytes,
        const int iterations,
        const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto bayer = makeBayerPattern(width, height);
  const unsigned rgb_line_step = width * 3 + padded_bytes;
  std::vector<std::uint8_t> output(static_cast<std::size_t>(rgb_line_step) * height, 0x7d);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    debayer::debayerBilinearInteriorCandidate(
        bayer.data(), output.data(), width, height, width, width * 2, rgb_line_step);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    debayer::debayerBilinearInteriorCandidate(
        bayer.data(), output.data(), width, height, width, width * 2, rgb_line_step);
    checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (width * height)
            << ", line_step: " << rgb_line_step << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Debayer bilinear interior production-shaped diagnostic bench\n";
  std::cout << "Dataset: synthetic GRBG Bayer image, full-size bilinear interior only\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  runCase(argc, argv, "bilinear_inner_640x480", 640, 480, 0, iterations, warmup_iterations);
  runCase(argc, argv, "bilinear_inner_padded_640x480", 640, 480, 13, iterations, warmup_iterations);
  return 0;
}
