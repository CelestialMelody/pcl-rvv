/*
 * 本文件做什么：
 * 这是 lzf_image_io 的 production-shaped diagnostic（生产形态诊断）
 * bench 入口。Std build 运行标量 reference（参考链路）；RVV build 在
 * `__RVV10__` 下运行测试专用 candidate（候选链路）。输出包含 checksum
 *（校验和）和每个 case 的耗时，供板卡 repeated summary（重复采集摘要）
 * 和 Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * 这些 case 直接喂解压后的 buffer，不计入 file read、header parse 或
 * LZF decompress 成本。QEMU 只用于构建、正确性和日志形状；性能结论必须
 * 来自板卡。
 */

#include "lzf_image_io.h"

#include <pcl/io/lzf_image_io.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace lzf = pcl::io::rvv_lzf_image_io_support;

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

std::vector<std::uint16_t>
makeDepthValues(const unsigned width, const unsigned height)
{
  std::vector<std::uint16_t> values(static_cast<std::size_t>(width) * height);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      values[index] = static_cast<std::uint16_t>(400 + ((x * 23 + y * 41) % 6000));
      if ((x + y * 3) % 97 == 0)
        values[index] = 0;
    }
  }
  return values;
}

std::vector<std::uint8_t>
makePlanarYuv422(const unsigned width, const unsigned height)
{
  const auto pixels = static_cast<std::size_t>(width) * height;
  const auto pairs = pixels / 2;
  std::vector<std::uint8_t> data(pairs + pixels + pairs);
  auto* u_plane = data.data();
  auto* y_plane = data.data() + pairs;
  auto* v_plane = data.data() + pairs + pixels;
  for (std::size_t i = 0; i < pairs; ++i) {
    u_plane[i] = static_cast<std::uint8_t>((17 + i * 13) & 0xffu);
    v_plane[i] = static_cast<std::uint8_t>((223 + i * 7) & 0xffu);
  }
  for (std::size_t i = 0; i < pixels; ++i)
    y_plane[i] = static_cast<std::uint8_t>((31 + i * 5) & 0xffu);
  return data;
}

std::vector<std::uint8_t>
makeRgbBuffer(const unsigned width, const unsigned height)
{
  std::vector<std::uint8_t> data(static_cast<std::size_t>(width) * height * 3);
  for (std::size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<std::uint8_t>((11 + i * 29) & 0xffu);
  return data;
}

std::uint64_t
checksumCloud(const std::vector<lzf::PointXYZRGB>& cloud)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& pt : cloud) {
    h ^= lzf::floatBits(pt.x);
    h *= 1099511628211ull;
    h ^= lzf::floatBits(pt.y);
    h *= 1099511628211ull;
    h ^= lzf::floatBits(pt.z);
    h *= 1099511628211ull;
    h ^= pt.r;
    h *= 1099511628211ull;
    h ^= pt.g;
    h *= 1099511628211ull;
    h ^= pt.b;
    h *= 1099511628211ull;
  }
  return h;
}

template <typename PointT>
std::uint64_t
checksumRgbCloud(const std::vector<PointT>& cloud)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& pt : cloud) {
    h ^= pt.r;
    h *= 1099511628211ull;
    h ^= pt.g;
    h *= 1099511628211ull;
    h ^= pt.b;
    h *= 1099511628211ull;
  }
  return h;
}

template <typename Fill>
void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const unsigned width,
        const unsigned height,
        const int iterations,
        const int warmup_iterations,
        Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  std::vector<lzf::PointXYZRGB> cloud(static_cast<std::size_t>(width) * height);
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(cloud.data());
    checksum ^= checksumCloud(cloud) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(cloud.data());
    checksum ^= checksumCloud(cloud) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (width * height)
            << '\n';
  doNotOptimize(checksum);
}

template <typename Fill>
void
runRgbCase(const int argc,
           char** argv,
           const std::string& label,
           const unsigned width,
           const unsigned height,
           const int iterations,
           const int warmup_iterations,
           Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  std::vector<pcl::PointXYZRGB> cloud(static_cast<std::size_t>(width) * height);
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(cloud.data());
    checksum ^= checksumRgbCloud(cloud) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(cloud.data());
    checksum ^= checksumRgbCloud(cloud) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (width * height)
            << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const unsigned width = 640;
  const unsigned height = 480;

  std::cout << "Dataset: lzf_image_io synthetic 640x480 buffers\n";
  std::cout << "Iterations: " << iterations << ", warmup: " << warmup_iterations
            << '\n';

  const auto depth = makeDepthValues(width, height);
  const auto yuv = makePlanarYuv422(width, height);
  const auto rgb = makeRgbBuffer(width, height);
  const lzf::DepthCameraParameters params{525.0f, 530.0f, 319.5f, 239.5f, 0.001f};

  runCase(argc,
          argv,
          "depth_xyz_640x480",
          width,
          height,
          iterations,
          warmup_iterations,
          [&](lzf::PointXYZRGB* cloud) {
            bool is_dense = true;
            lzf::convertDepthToCloudCandidate(depth.data(),
                                              width,
                                              height,
                                              params,
                                              cloud,
                                              &is_dense);
            doNotOptimize(is_dense);
          });

  runCase(argc,
          argv,
          "yuv422_planar_rgb_640x480",
          width,
          height,
          iterations,
          warmup_iterations,
          [&](lzf::PointXYZRGB* cloud) {
            lzf::convertPlanarYuv422ToRgbCandidate(yuv.data(), width, height, cloud);
          });

  runRgbCase(argc,
             argv,
             "yuv422_planar_rgb_production_640x480",
             width,
             height,
             iterations,
             warmup_iterations,
             [&](pcl::PointXYZRGB* cloud) {
#if defined(__RVV10__)
               const bool used_rvv = pcl::io::detail::convertPlanarYuv422ToPointCloudRVV(
                   yuv.data(), width, height, cloud);
               doNotOptimize(used_rvv);
#else
               pcl::io::detail::convertPlanarYuv422ToPointCloudStd(
                   yuv.data(), width, height, cloud);
#endif
             });

  runCase(argc,
          argv,
          "rgb_buffer_to_cloud_640x480",
          width,
          height,
          iterations,
          warmup_iterations,
          [&](lzf::PointXYZRGB* cloud) {
            lzf::copyRgbBufferToCloudCandidate(rgb.data(), width, height, cloud);
          });

  return 0;
}
