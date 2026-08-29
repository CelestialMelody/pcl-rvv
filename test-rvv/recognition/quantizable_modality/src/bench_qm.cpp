/*
 * 本文件做什么：
 * 这是 quantizable_modality topic 的 bench（性能测试）入口。case 名中的
 * `shared_spread_*` 表示直接调用公共 `QuantizedMap::spreadQuantizedMap()`；
 * Std build 是标量参考链路，RVV build 在 production 源码接入后应命中
 * RVV 链路。输出包含 Dataset、Iterations、Warmup Iterations、每 iter 时间
 * 和 checksum（校验和），供 repeated board summary（重复板卡摘要）和
 * Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * 这个 bench 证明公共 helper 的 production-detail（生产细节 helper）性能，
 * 不单独证明 `ColorModality`、`ColorGradientModality` 或
 * `SurfaceNormalModality` 的完整 `processInputData()` 总收益。
 */

#include "qm.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace qm = pcl::recognition::rvv_test::quantizable_modality;

namespace {
constexpr int kDefaultIterations = 50;
constexpr int kDefaultWarmupIterations = 5;

template <typename T>
inline void
doNotOptimize (const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile ("" : : "r,m" (value) : "memory");
#else
  (void)value;
#endif
}

int
parseIntArg (const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::string (argv[i]) == flag)
      return std::atoi (argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled (const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::string (argv[i]) == "--case-filter")
    {
      const std::string selected (argv[i + 1]);
      if (selected == "all")
        return true;
      std::size_t begin = 0;
      while (begin <= selected.size ())
      {
        const std::size_t end = selected.find (',', begin);
        const auto token = selected.substr (begin, end == std::string::npos ? end : end - begin);
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

void
runSpreadCase (const int argc,
               char** argv,
               const std::string& label,
               const std::size_t width,
               const std::size_t height,
               const std::size_t spread,
               const int iterations,
               const int warmup_iterations)
{
  if (!caseEnabled (argc, argv, label))
    return;

  const auto input = qm::makePatternMap (width, height);
  pcl::QuantizedMap output;
  std::uint64_t checksum = 1469598103934665603ull;

  for (int i = 0; i < warmup_iterations; ++i)
  {
    pcl::QuantizedMap::spreadQuantizedMap (input, output, spread);
    checksum ^= qm::checksumMap (output) + static_cast<std::uint64_t> (i);
  }

  const auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < iterations; ++i)
  {
    pcl::QuantizedMap::spreadQuantizedMap (input, output, spread);
    checksum ^= qm::checksumMap (output) + static_cast<std::uint64_t> (i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now ();
  const double total_ms = std::chrono::duration<double, std::milli> (end - start).count ();

  std::cout << std::left << std::setw (40) << label << ": " << std::fixed
            << std::setprecision (4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision (4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << (width * height)
            << ", spread: " << spread << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
  doNotOptimize (checksum);
}
} // namespace

int
main (int argc, char** argv)
{
  const int iterations = parseIntArg (argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg (argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: synthetic quantized byte maps for QuantizedMap::spreadQuantizedMap\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  std::cout << "Build Path: " << qm::pathName () << '\n';

  runSpreadCase (argc, argv, "shared_spread_320x240", 320, 240, 8, iterations, warmup_iterations);
  runSpreadCase (argc, argv, "shared_spread_641x481_tail", 641, 481, 8, iterations, warmup_iterations);
  return 0;
}
