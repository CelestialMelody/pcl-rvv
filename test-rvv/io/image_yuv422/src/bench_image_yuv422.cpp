/*
 * 本文件做什么：
 * 这是 image_yuv422 的 production-shaped diagnostic（生产形态诊断）bench
 * 入口。Std build 运行标量参考链路；RVV build 在 __RVV10__ 下运行测试专用
 * RVV candidate。输出包含 checksum（校验和）和每个 case 的耗时，供板卡
 * repeated summary（重复采集摘要）和 Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * bench 不调用真实 ImageYUV422 production dispatch（生产分流），也不证明生产源码
 * 已接入 RVV。QEMU 只能用于构建、正确性和日志形状；性能结论必须来自板卡。
 */

#include "image_yuv422.h"
#include "impl/image_yuv422_production_fixture.hpp"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace yuv = pcl::io::rvv_image_yuv422_support;

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
makeYuyvPixels(const unsigned width, const unsigned height)
{
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 2);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; x += 2) {
      const auto pair = (static_cast<std::size_t>(y) * width + x) * 2;
      pixels[pair + 0] = static_cast<std::uint8_t>((23 * x + 41 * y + 17) & 0xff);
      pixels[pair + 1] = static_cast<std::uint8_t>((67 * x + 11 * y + 89) & 0xff);
      pixels[pair + 2] = static_cast<std::uint8_t>((5 * x + 101 * y + 203) & 0xff);
      pixels[pair + 3] = static_cast<std::uint8_t>((37 * x + 59 * y + 3) & 0xff);
    }
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

template <typename Fill>
void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const unsigned src_width,
        const unsigned src_height,
        const unsigned dst_width,
        const unsigned dst_height,
        const unsigned bytes_per_pixel,
        const unsigned padded_bytes,
        const int iterations,
        const int warmup_iterations,
        Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto pixels = makeYuyvPixels(src_width, src_height);
  const unsigned line_step = dst_width * bytes_per_pixel + padded_bytes;
  std::vector<std::uint8_t> output(static_cast<std::size_t>(line_step) * dst_height, 0x7d);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(pixels.data(), output.data(), line_step);
    doNotOptimize(output.data());
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(pixels.data(), output.data(), line_step);
    doNotOptimize(output.data());
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(iterations + warmup_iterations);

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (dst_width * dst_height)
            << ", line_step: " << line_step << '\n';
  doNotOptimize(checksum);
}

void
runProductionRgbCase(const int argc,
                     char** argv,
                     const std::string& label,
                     const unsigned src_width,
                     const unsigned src_height,
                     const unsigned dst_width,
                     const unsigned dst_height,
                     const unsigned padded_bytes,
                     const int iterations,
                     const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto pixels = makeYuyvPixels(src_width, src_height);
  const unsigned line_step = dst_width * 3 + padded_bytes;
  const auto image = yuv::makeProductionImage(src_width, src_height, pixels);
  std::vector<std::uint8_t> output(static_cast<std::size_t>(line_step) * dst_height, 0x7d);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    image.fillRGB(dst_width, dst_height, output.data(), line_step);
    doNotOptimize(output.data());
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    image.fillRGB(dst_width, dst_height, output.data(), line_step);
    doNotOptimize(output.data());
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(iterations + warmup_iterations);

  std::cout << std::left << std::setw(40) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (dst_width * dst_height)
            << ", line_step: " << line_step << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "ImageYUV422 color conversion diagnostic and production-direct bench\n";
  std::cout << "Dataset: synthetic_yuyv_640x480_and_downsample\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  runCase(argc, argv, "rgb_full_640x480", 640, 480, 640, 480, 3, 0, iterations,
          warmup_iterations, [](const std::uint8_t* pixels, std::uint8_t* output,
                                const unsigned line_step) {
            yuv::fillRgbCandidate(pixels, 640, 480, 640, 480, output, line_step);
          });
  runCase(argc, argv, "rgb_full_padded_640x480", 640, 480, 640, 480, 3, 7, iterations,
          warmup_iterations, [](const std::uint8_t* pixels, std::uint8_t* output,
                                const unsigned line_step) {
            yuv::fillRgbCandidate(pixels, 640, 480, 640, 480, output, line_step);
          });
  runCase(argc, argv, "rgb_downsample_640x480_to_320x240", 640, 480, 320, 240, 3, 0,
          iterations, warmup_iterations, [](const std::uint8_t* pixels, std::uint8_t* output,
                                            const unsigned line_step) {
            yuv::fillRgbCandidate(pixels, 640, 480, 320, 240, output, line_step);
          });
  runCase(argc, argv, "gray_full_640x480", 640, 480, 640, 480, 1, 0, iterations,
          warmup_iterations, [](const std::uint8_t* pixels, std::uint8_t* output,
                                const unsigned line_step) {
            yuv::fillGrayscaleCandidate(pixels, 640, 480, 640, 480, output, line_step);
          });
  runCase(argc, argv, "gray_downsample_640x480_to_320x240", 640, 480, 320, 240, 1, 0,
          iterations, warmup_iterations, [](const std::uint8_t* pixels, std::uint8_t* output,
                                            const unsigned line_step) {
            yuv::fillGrayscaleCandidate(pixels, 640, 480, 320, 240, output, line_step);
          });
  runProductionRgbCase(
      argc, argv, "prod_rgb_full_640x480", 640, 480, 640, 480, 0, iterations, warmup_iterations);
  runProductionRgbCase(argc,
                       argv,
                       "prod_rgb_full_padded_640x480",
                       640,
                       480,
                       640,
                       480,
                       7,
                       iterations,
                       warmup_iterations);
  runProductionRgbCase(argc,
                       argv,
                       "prod_rgb_downsample_640x480_to_320x240",
                       640,
                       480,
                       320,
                       240,
                       0,
                       iterations,
                       warmup_iterations);

  return 0;
}
