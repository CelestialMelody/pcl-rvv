/*
 * 本文件做什么：
 * 这是 image_depth 的 production-shaped diagnostic（生产形态诊断）bench 入口。
 * Std build 运行标量参考链路；RVV build 在 __RVV10__ 下运行测试专用 RVV
 * candidate。输出包含 checksum（校验和）和每个 case 的耗时，供板卡 repeated
 * summary（重复采集摘要）和 Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * `prod_*` case 调用真实 DepthImage production dispatch（生产分流），用于
 * production-public（真实公开入口）证据；历史非 `prod_*` case 仍是
 * production-shaped diagnostic（生产形态诊断）。QEMU 只能用于构建、正确性和日志形状；
 * 性能结论必须来自板卡。
 */

#include "image_depth.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <pcl/io/image_depth.h>
#include <pcl/io/image_metadata_wrapper.h>

namespace depth = pcl::io::rvv_image_depth_support;

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

class VectorFrameWrapper : public pcl::io::FrameWrapper {
public:
  VectorFrameWrapper(const unsigned width,
                     const unsigned height,
                     std::vector<std::uint16_t> pixels)
  : width_(width), height_(height), pixels_(std::move(pixels))
  {}

  const void*
  getData() const override
  {
    return pixels_.data();
  }

  unsigned
  getDataSize() const override
  {
    return static_cast<unsigned>(pixels_.size() * sizeof(std::uint16_t));
  }

  unsigned
  getWidth() const override
  {
    return width_;
  }

  unsigned
  getHeight() const override
  {
    return height_;
  }

  unsigned
  getFrameID() const override
  {
    return 7;
  }

  std::uint64_t
  getTimestamp() const override
  {
    return 123456;
  }

private:
  unsigned width_;
  unsigned height_;
  std::vector<std::uint16_t> pixels_;
};

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

std::vector<std::uint16_t>
makePixels(const unsigned width, const unsigned height)
{
  std::vector<std::uint16_t> pixels(static_cast<std::size_t>(width) * height);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      pixels[index] = static_cast<std::uint16_t>(500 + ((x * 19 + y * 43) % 5000));
      if ((x + y * 5) % 97 == 0)
        pixels[index] = 0;
      if ((x * 3 + y) % 211 == 0)
        pixels[index] = 2047;
      if ((x + y * 7) % 257 == 0)
        pixels[index] = 65535;
    }
  }
  return pixels;
}

std::uint64_t
checksumFloats(const std::vector<float>& values)
{
  std::uint64_t h = 1469598103934665603ull;
  for (float value : values) {
    std::uint32_t bits = depth::floatBits(value);
    h ^= bits;
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
        const unsigned padded_floats,
        const int iterations,
        const int warmup_iterations,
        Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto pixels = makePixels(src_width, src_height);
  const unsigned line_step =
      (dst_width + padded_floats) * static_cast<unsigned>(sizeof(float));
  std::vector<float> output(static_cast<std::size_t>(line_step / sizeof(float)) * dst_height,
                            -7.0f);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(pixels.data(), output.data(), line_step);
    checksum ^= checksumFloats(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(pixels.data(), output.data(), line_step);
    checksum ^= checksumFloats(output) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (dst_width * dst_height)
            << ", line_step: " << line_step << '\n';
  doNotOptimize(checksum);
}

template <typename Fill>
void
runProductionCase(const int argc,
                  char** argv,
                  const std::string& label,
                  const unsigned src_width,
                  const unsigned src_height,
                  const unsigned dst_width,
                  const unsigned dst_height,
                  const unsigned padded_floats,
                  const int iterations,
                  const int warmup_iterations,
                  Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto pixels = makePixels(src_width, src_height);
  const unsigned line_step =
      (dst_width + padded_floats) * static_cast<unsigned>(sizeof(float));
  std::vector<float> output(static_cast<std::size_t>(line_step / sizeof(float)) * dst_height,
                            -7.0f);
  pcl::io::FrameWrapper::Ptr wrapper =
      std::make_shared<VectorFrameWrapper>(src_width, src_height, pixels);
  pcl::io::DepthImage image(wrapper, 0.075f, 525.0f, 65535, 2047);

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(image, output.data(), line_step);
    checksum ^= checksumFloats(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(image, output.data(), line_step);
    checksum ^= checksumFloats(output) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
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

  std::cout << "ImageDepth depth/disparity diagnostic and production-public bench\n";
  std::cout << "Dataset: synthetic 16-bit depth image; invalid zero/no-sample/shadow pixels\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  runCase(argc, argv, "depth_full_640x480", 640, 480, 640, 480, 0, iterations, warmup_iterations,
          [](const std::uint16_t* pixels, float* output, unsigned line_step) {
            depth::fillDepthMetersCandidate(
                pixels, 640, 480, 2047, 65535, 640, 480, output, line_step);
          });
  runCase(argc, argv, "depth_full_padded_640x480", 640, 480, 640, 480, 5, iterations,
          warmup_iterations, [](const std::uint16_t* pixels, float* output, unsigned line_step) {
            depth::fillDepthMetersCandidate(
                pixels, 640, 480, 2047, 65535, 640, 480, output, line_step);
          });
  runCase(argc, argv, "depth_downsample_640x480_to_320x240", 640, 480, 320, 240, 0, iterations,
          warmup_iterations, [](const std::uint16_t* pixels, float* output, unsigned line_step) {
            depth::fillDepthMetersCandidate(
                pixels, 640, 480, 2047, 65535, 320, 240, output, line_step);
          });
  runCase(argc, argv, "disparity_full_640x480", 640, 480, 640, 480, 0, iterations,
          warmup_iterations, [](const std::uint16_t* pixels, float* output, unsigned line_step) {
            depth::fillDisparityCandidate(
                pixels, 640, 480, 2047, 65535, 0.075f, 525.0f, 640, 480, output, line_step);
          });
  runCase(argc, argv, "disparity_downsample_640x480_to_320x240", 640, 480, 320, 240, 0,
          iterations, warmup_iterations,
          [](const std::uint16_t* pixels, float* output, unsigned line_step) {
            depth::fillDisparityCandidate(
                pixels, 640, 480, 2047, 65535, 0.075f, 525.0f, 320, 240, output, line_step);
          });
  runProductionCase(argc,
                    argv,
                    "prod_depth_full_640x480",
                    640,
                    480,
                    640,
                    480,
                    0,
                    iterations,
                    warmup_iterations,
                    [](const pcl::io::DepthImage& image, float* output, unsigned line_step) {
                      image.fillDepthImage(640, 480, output, line_step);
                    });
  runProductionCase(argc,
                    argv,
                    "prod_depth_full_padded_640x480",
                    640,
                    480,
                    640,
                    480,
                    5,
                    iterations,
                    warmup_iterations,
                    [](const pcl::io::DepthImage& image, float* output, unsigned line_step) {
                      image.fillDepthImage(640, 480, output, line_step);
                    });
  runProductionCase(argc,
                    argv,
                    "prod_depth_downsample_640x480_to_320x240",
                    640,
                    480,
                    320,
                    240,
                    0,
                    iterations,
                    warmup_iterations,
                    [](const pcl::io::DepthImage& image, float* output, unsigned line_step) {
                      image.fillDepthImage(320, 240, output, line_step);
                    });
  runProductionCase(argc,
                    argv,
                    "prod_disparity_full_640x480",
                    640,
                    480,
                    640,
                    480,
                    0,
                    iterations,
                    warmup_iterations,
                    [](const pcl::io::DepthImage& image, float* output, unsigned line_step) {
                      image.fillDisparityImage(640, 480, output, line_step);
                    });
  runProductionCase(argc,
                    argv,
                    "prod_disparity_downsample_640x480_to_320x240",
                    640,
                    480,
                    320,
                    240,
                    0,
                    iterations,
                    warmup_iterations,
                    [](const pcl::io::DepthImage& image, float* output, unsigned line_step) {
                      image.fillDisparityImage(320, 240, output, line_step);
                    });

  return 0;
}
