/*
 * 本文件做什么：
 * 这是 color_coding 的 component diagnostic（组件诊断）bench 入口。
 * Std build 运行标量 reference（参考链路）；RVV build 在 `__RVV10__`
 * 下运行测试专用 candidate（候选链路）。输出包含 checksum（校验和）
 * 和每个 case 的耗时，供板卡 repeated summary（重复采集摘要）和
 * Evidence Doctor（证据体检）使用。
 *
 * 证据边界：
 * 这些 case 只计 color coder 组件，不计 octree traversal（八叉树遍历）
 * 或 entropy coder（熵编码器）。QEMU 只用于构建和日志形状；性能结论
 * 必须来自板卡。
 */

#include "color_coding.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>
#include <pcl/compression/color_coding.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace cc = pcl::octree::rvv_color_coding_support;

namespace {

constexpr int kDefaultIterations = 20;
constexpr int kDefaultWarmupIterations = 3;
constexpr int kDefaultBatchRepeats = 64;
constexpr unsigned char kRgbaOffset = offsetof(cc::ColorPoint, rgba);
constexpr unsigned char kPclRgbaOffset = offsetof(pcl::PointXYZRGBA, rgba);

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
      if (selected == "production")
        return false;
      return selected == requested || selected == "all";
    }
  }
  return true;
}

std::vector<cc::ColorPoint>
makeCloud(const std::size_t count)
{
  std::vector<cc::ColorPoint> cloud(count);
  for (std::size_t i = 0; i < count; ++i) {
    cloud[i].x = static_cast<float>(i & 31u);
    cloud[i].y = static_cast<float>((i * 3u) & 31u);
    cloud[i].z = static_cast<float>((i * 7u) & 31u);
    cloud[i].rgba = cc::packColor(static_cast<std::uint8_t>((13u + i * 17u) & 0xffu),
                                  static_cast<std::uint8_t>((29u + i * 11u) & 0xffu),
                                  static_cast<std::uint8_t>((47u + i * 5u) & 0xffu),
                                  99);
  }
  return cloud;
}

std::vector<pcl::PointXYZRGBA>
makePclCloud(const std::size_t count)
{
  std::vector<pcl::PointXYZRGBA> cloud(count);
  for (std::size_t i = 0; i < count; ++i) {
    cloud[i].x = static_cast<float>(i & 31u);
    cloud[i].y = static_cast<float>((i * 3u) & 31u);
    cloud[i].z = static_cast<float>((i * 7u) & 31u);
    cloud[i].rgba = cc::packColor(static_cast<std::uint8_t>((13u + i * 17u) & 0xffu),
                                  static_cast<std::uint8_t>((29u + i * 11u) & 0xffu),
                                  static_cast<std::uint8_t>((47u + i * 5u) & 0xffu),
                                  99);
  }
  return cloud;
}

std::vector<std::uint32_t>
makeLeafIndices(const std::size_t cloud_size, const std::size_t leaf_size)
{
  std::vector<std::uint32_t> indices(leaf_size);
  for (std::size_t i = 0; i < leaf_size; ++i)
    indices[i] = static_cast<std::uint32_t>((i * 37u + 11u) % cloud_size);
  return indices;
}

std::uint64_t
checksumEncoded(const cc::EncodedColorData& encoded)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const char value : encoded.average) {
    h ^= static_cast<unsigned char>(value);
    h *= 1099511628211ull;
  }
  for (const char value : encoded.differential) {
    h ^= static_cast<unsigned char>(value);
    h *= 1099511628211ull;
  }
  return h;
}

std::uint64_t
checksumCloud(const std::vector<cc::ColorPoint>& cloud)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& point : cloud) {
    h ^= point.rgba;
    h *= 1099511628211ull;
  }
  return h;
}

template <typename PointT, typename Allocator>
std::uint64_t
checksumPointCloud(const std::vector<PointT, Allocator>& cloud)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& point : cloud) {
    h ^= point.rgba;
    h *= 1099511628211ull;
  }
  return h;
}

template <typename Fill>
void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const int iterations,
        const int warmup_iterations,
        const int batch_repeats,
        Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    for (int batch = 0; batch < batch_repeats; ++batch)
      checksum ^= fill() + static_cast<std::uint64_t>(i + batch);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    for (int batch = 0; batch < batch_repeats; ++batch)
      checksum ^= fill() + static_cast<std::uint64_t>(i + batch + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations =
      parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const int batch_repeats = parseIntArg(argc, argv, "--batch-repeats", kDefaultBatchRepeats);
  const auto cloud = makeCloud(8192);
  const auto pcl_cloud = makePclCloud(8192);
  const auto indices31 = makeLeafIndices(cloud.size(), 31);
  const auto indices257 = makeLeafIndices(cloud.size(), 257);
  const auto indices1024 = makeLeafIndices(cloud.size(), 1024);
  const auto indices4096 = makeLeafIndices(cloud.size(), 4096);

  std::cout << "Dataset: synthetic_color_points\n";
  std::cout << "Iterations: " << iterations << ", warmup: " << warmup_iterations << '\n';
  std::cout << "Batch repeats: " << batch_repeats << '\n';

  const auto run_encode_average = [&](const std::vector<std::uint32_t>& indices) {
    return [&] {
      cc::EncodedColorData encoded;
#if defined(__RVV10__)
      cc::encodeAverageOfPointsCandidate(cloud, indices, kRgbaOffset, 1, encoded);
#else
      cc::encodeAverageOfPointsReference(cloud, indices, kRgbaOffset, 1, encoded);
#endif
      return checksumEncoded(encoded);
    };
  };

  const auto run_encode_points = [&](const std::vector<std::uint32_t>& indices) {
    return [&] {
      cc::EncodedColorData encoded;
#if defined(__RVV10__)
      cc::encodePointsCandidate(cloud, indices, kRgbaOffset, 1, encoded);
#else
      cc::encodePointsReference(cloud, indices, kRgbaOffset, 1, encoded);
#endif
      return checksumEncoded(encoded);
    };
  };

  const auto run_decode_points = [&](const std::vector<std::uint32_t>& indices) {
    return [&] {
      cc::EncodedColorData encoded;
      cc::encodePointsReference(cloud, indices, kRgbaOffset, 1, encoded);
      std::vector<cc::ColorPoint> output(indices.size());
      std::size_t avg_cursor = 0;
      std::size_t diff_cursor = 0;
#if defined(__RVV10__)
      cc::decodePointsCandidate(
          encoded, avg_cursor, diff_cursor, output, 0, output.size(), kRgbaOffset, 1);
#else
      cc::decodePointsReference(
          encoded, avg_cursor, diff_cursor, output, 0, output.size(), kRgbaOffset, 1);
#endif
      return checksumCloud(output);
    };
  };

  const auto run_set_default = [&](const std::size_t count) {
    return [&, count] {
      std::vector<cc::ColorPoint> output(count);
#if defined(__RVV10__)
      cc::setDefaultColorCandidate(output, 0, output.size(), kRgbaOffset);
#else
      cc::setDefaultColorReference(output, 0, output.size(), kRgbaOffset);
#endif
      return checksumCloud(output);
    };
  };

  const auto run_ps_encode_average = [&](const std::vector<std::uint32_t>& indices) {
    return [&] {
      cc::EncodedColorData encoded;
#if defined(__RVV10__)
      cc::encodeAverageOfPointsCandidatePointVector(
          pcl_cloud, indices, kPclRgbaOffset, 1, encoded);
#else
      cc::encodeAverageOfPointsReferencePointVector(
          pcl_cloud, indices, kPclRgbaOffset, 1, encoded);
#endif
      return checksumEncoded(encoded);
    };
  };

  const auto run_ps_encode_points = [&](const std::vector<std::uint32_t>& indices) {
    return [&] {
      cc::EncodedColorData encoded;
#if defined(__RVV10__)
      cc::encodePointsCandidatePointVector(pcl_cloud, indices, kPclRgbaOffset, 1, encoded);
#else
      cc::encodePointsReferencePointVector(pcl_cloud, indices, kPclRgbaOffset, 1, encoded);
#endif
      return checksumEncoded(encoded);
    };
  };

  const auto run_ps_decode_points = [&](const std::vector<std::uint32_t>& indices) {
    return [&] {
      cc::EncodedColorData encoded;
      cc::encodePointsReferencePointVector(pcl_cloud, indices, kPclRgbaOffset, 1, encoded);
      std::vector<pcl::PointXYZRGBA> output(indices.size());
      std::size_t avg_cursor = 0;
      std::size_t diff_cursor = 0;
#if defined(__RVV10__)
      cc::decodePointsCandidatePointVector(
          encoded, avg_cursor, diff_cursor, output, 0, output.size(), kPclRgbaOffset, 1);
#else
      cc::decodePointsReferencePointVector(
          encoded, avg_cursor, diff_cursor, output, 0, output.size(), kPclRgbaOffset, 1);
#endif
      return checksumPointCloud(output);
    };
  };

  const auto run_ps_decode_points_staged = [&](const std::vector<std::uint32_t>& indices) {
    return [&, scratch = std::vector<std::uint32_t>{}]() mutable {
      cc::EncodedColorData encoded;
      cc::encodePointsReferencePointVector(pcl_cloud, indices, kPclRgbaOffset, 1, encoded);
      std::vector<pcl::PointXYZRGBA> output(indices.size());
      std::size_t avg_cursor = 0;
      std::size_t diff_cursor = 0;
#if defined(__RVV10__)
      cc::decodePointsCandidatePointVectorStagedStore(
          encoded, avg_cursor, diff_cursor, output, 0, output.size(), kPclRgbaOffset, 1, scratch);
#else
      cc::decodePointsReferencePointVector(
          encoded, avg_cursor, diff_cursor, output, 0, output.size(), kPclRgbaOffset, 1);
#endif
      return checksumPointCloud(output);
    };
  };

  const auto run_ps_set_default = [&](const std::size_t count) {
    return [&, count] {
      std::vector<pcl::PointXYZRGBA> output(count);
#if defined(__RVV10__)
      cc::setDefaultColorCandidatePointVector(output, 0, output.size(), kPclRgbaOffset);
#else
      cc::setDefaultColorReferencePointVector(output, 0, output.size(), kPclRgbaOffset);
#endif
      return checksumPointCloud(output);
    };
  };

  runCase(argc, argv, "encode_average_leaf31", iterations, warmup_iterations, batch_repeats, run_encode_average(indices31));
  runCase(argc, argv, "encode_average_leaf257", iterations, warmup_iterations, batch_repeats, run_encode_average(indices257));
  runCase(argc, argv, "encode_average_leaf1024", iterations, warmup_iterations, batch_repeats, run_encode_average(indices1024));
  runCase(argc, argv, "encode_average_leaf4096", iterations, warmup_iterations, batch_repeats, run_encode_average(indices4096));

  runCase(argc, argv, "encode_points_leaf31", iterations, warmup_iterations, batch_repeats, run_encode_points(indices31));
  runCase(argc, argv, "encode_points_leaf257", iterations, warmup_iterations, batch_repeats, run_encode_points(indices257));
  runCase(argc, argv, "encode_points_leaf1024", iterations, warmup_iterations, batch_repeats, run_encode_points(indices1024));
  runCase(argc, argv, "encode_points_leaf4096", iterations, warmup_iterations, batch_repeats, run_encode_points(indices4096));

  runCase(argc, argv, "decode_points_leaf31", iterations, warmup_iterations, batch_repeats, run_decode_points(indices31));
  runCase(argc, argv, "decode_points_leaf257", iterations, warmup_iterations, batch_repeats, run_decode_points(indices257));
  runCase(argc, argv, "decode_points_leaf1024", iterations, warmup_iterations, batch_repeats, run_decode_points(indices1024));
  runCase(argc, argv, "decode_points_leaf4096", iterations, warmup_iterations, batch_repeats, run_decode_points(indices4096));

  runCase(argc, argv, "set_default_color_4096", iterations, warmup_iterations, batch_repeats, run_set_default(4096));
  runCase(argc, argv, "set_default_color_16384", iterations, warmup_iterations, batch_repeats, run_set_default(16384));

  runCase(argc, argv, "ps_encode_average_leaf257", iterations, warmup_iterations, batch_repeats, run_ps_encode_average(indices257));
  runCase(argc, argv, "ps_encode_average_leaf4096", iterations, warmup_iterations, batch_repeats, run_ps_encode_average(indices4096));
  runCase(argc, argv, "ps_encode_points_leaf257", iterations, warmup_iterations, batch_repeats, run_ps_encode_points(indices257));
  runCase(argc, argv, "ps_encode_points_leaf4096", iterations, warmup_iterations, batch_repeats, run_ps_encode_points(indices4096));
  runCase(argc, argv, "ps_decode_points_leaf257", iterations, warmup_iterations, batch_repeats, run_ps_decode_points(indices257));
  runCase(argc, argv, "ps_decode_points_leaf4096", iterations, warmup_iterations, batch_repeats, run_ps_decode_points(indices4096));
  runCase(argc, argv, "ps_decode_points_staged_leaf257", iterations, warmup_iterations, batch_repeats, run_ps_decode_points_staged(indices257));
  runCase(argc, argv, "ps_decode_points_staged_leaf4096", iterations, warmup_iterations, batch_repeats, run_ps_decode_points_staged(indices4096));
  runCase(argc, argv, "ps_set_default_color_4096", iterations, warmup_iterations, batch_repeats, run_ps_set_default(4096));
  return 0;
}
