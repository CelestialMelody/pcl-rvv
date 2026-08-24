/*
 * 本文件做什么：
 * 这是 ONIGrabber depth-only frame-to-cloud（帧到点云）topic 的
 * production-shaped diagnostic（生产形态诊断）bench 入口。Std build
 * 运行标量 reference（参考链路）；RVV build 在 `__RVV10__` 下运行测试
 * 专用 candidate（候选链路）。输出包含 checksum（校验和）和每个 case
 * 的平均耗时，供板卡 repeated summary（重复采集摘要）使用。
 *
 * 证据边界：
 * 这里不调用真实 ONIGrabber public entry（公开入口），因此结果只能支撑
 * 候选筛选。QEMU 只用于构建、correctness（正确性）和日志形状；性能结论
 * 必须来自板卡或目标硬件。
 */

#include "oni_grabber.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace grabber = pcl::io::rvv_oni_grabber_support;

extern "C" {
void
pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (const std::uint16_t* depth,
                                                  unsigned width,
                                                  unsigned height,
                                                  float constant,
                                                  int center_x,
                                                  int center_y,
                                                  std::uint64_t no_sample_value,
                                                  std::uint64_t shadow_value,
                                                  pcl::PointXYZ* cloud);
}

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
makeDepth(const unsigned width, const unsigned height)
{
  std::vector<std::uint16_t> depth(static_cast<std::size_t>(width) * height);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      depth[index] = static_cast<std::uint16_t>(500 + ((x * 23 + y * 41) % 5000));
      if ((x + y * 3) % 101 == 0)
        depth[index] = 0;
      if ((x * 5 + y) % 223 == 0)
        depth[index] = 2047;
      if ((x + y * 7) % 251 == 0)
        depth[index] = 65535;
    }
  }
  return depth;
}

std::uint64_t
fnvMix(const std::uint64_t hash, const std::uint32_t value)
{
  return (hash ^ value) * 1099511628211ull;
}

std::uint32_t
floatBits(const float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::uint64_t
checksumXYZ(const std::vector<pcl::PointXYZ>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto& point : cloud) {
    hash = fnvMix(hash, floatBits(point.x));
    hash = fnvMix(hash, floatBits(point.y));
    hash = fnvMix(hash, floatBits(point.z));
  }
  return hash;
}

template <typename Fill>
void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const unsigned output_pixels,
        const int iterations,
        const int warmup_iterations,
        Fill fill,
        const std::vector<pcl::PointXYZ>& cloud)
{
  if (!caseEnabled(argc, argv, label))
    return;

  std::uint64_t hash = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill();
    hash ^= checksumXYZ(cloud) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill();
    hash ^= checksumXYZ(cloud) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(42) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << hash << ", pixels: " << output_pixels << '\n';
  doNotOptimize(hash);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const grabber::CameraModel camera{1.0f / 525.0f, 320, 240, 2047u, 65535u};

  std::cout << "ONIGrabber depth-only production-shaped diagnostic bench\n";
  std::cout << "Dataset: synthetic depth frames with invalid depth pixels\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  const unsigned width = 640;
  const unsigned height = 480;
  const auto depth = makeDepth(width, height);
  std::vector<pcl::PointXYZ> cloud(static_cast<std::size_t>(width) * height);
  runCase(argc, argv, "xyz_depth_full_640x480", cloud.size(), iterations,
          warmup_iterations,
          [&] { grabber::fillXYZCloudCandidate(depth.data(), width, height, camera, cloud.data()); },
          cloud);
  runCase(argc, argv, "prod_xyz_depth_full_640x480", cloud.size(), iterations,
          warmup_iterations,
          [&] {
            pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (depth.data (),
                                                              width,
                                                              height,
                                                              1.0f / 525.0f,
                                                              static_cast<int> (width >> 1),
                                                              static_cast<int> (height >> 1),
                                                              2047u,
                                                              65535u,
                                                              cloud.data ());
          },
          cloud);

  return 0;
}
