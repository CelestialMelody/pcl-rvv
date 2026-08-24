/*
 * 本文件做什么：
 * 这是 point_coding 首阶段 component ablation（组件消融）bench 入口。
 * Std build（标量构建）运行 test-only 标量参考链路；RVV build 在 __RVV10__
 * 下运行测试专用 RVV candidate。输出格式供 analyze_bench_compare.py 和
 * Evidence Doctor（证据体检）读取。
 *
 * 证据边界：
 * 这些 case 只测 PointCoding leaf-level helper（叶节点内 helper）的局部成本。
 * QEMU（仿真器）只用于构建和日志形状；真实性能结论只来自板卡或目标硬件。
 */

#include "point_coding.h"

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace pcoding = pcl::io::rvv_test::point_coding;

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
      if (selected == requested || selected == "all")
        return true;
      if (!selected.empty() && selected.back() == '*') {
        const std::string prefix = selected.substr(0, selected.size() - 1);
        return requested.rfind(prefix, 0) == 0;
      }
      return false;
    }
  }
  return true;
}

pcl::Indices
makeLeafIndices(const std::size_t cloud_size, const std::size_t leaf_size, const std::size_t salt)
{
  pcl::Indices indices;
  indices.reserve(leaf_size);
  for (std::size_t i = 0; i < leaf_size; ++i) {
    const std::size_t idx = (i * 37 + salt * 97 + (i % 7) * 11) % cloud_size;
    indices.push_back(static_cast<pcl::index_t>(idx));
  }
  return indices;
}

void
runCase(const std::string& name,
        const int iterations,
        const int warmup_iterations,
        const std::function<std::uint64_t()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i)
    checksum = pcoding::mixChecksum(checksum, fn() + static_cast<std::uint64_t>(i + 1));

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum = pcoding::mixChecksum(
        checksum, fn() + static_cast<std::uint64_t>(warmup_iterations + i + 1));
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}

std::uint64_t
runEncode(const pcl::PointCloud<pcoding::PointXYZ>& cloud,
          const pcl::Indices& indices,
          const pcoding::ReferencePoint& reference,
          const float resolution)
{
  std::vector<char> encoded;
#if defined(__RVV10__)
  pcoding::encodePointsCandidate(cloud, indices, reference, resolution, encoded);
#else
  pcoding::encodePointsScalar(cloud, indices, reference, resolution, encoded);
#endif
  return pcoding::checksumBytes(encoded);
}

std::uint64_t
runDecode(const std::vector<char>& encoded,
          const pcoding::ReferencePoint& reference,
          const float resolution)
{
  std::vector<pcoding::PointXYZ> decoded;
#if defined(__RVV10__)
  pcoding::decodePointsCandidate(encoded, reference, resolution, decoded);
#else
  pcoding::decodePointsScalar(encoded, reference, resolution, decoded);
#endif
  return pcoding::checksumPoints(decoded);
}

template <typename PointT>
std::uint64_t
checksumCloudXYZRange(const pcl::PointCloud<PointT>& cloud,
                      const std::size_t begin_index,
                      const std::size_t point_count)
{
  std::uint64_t h = 1469598103934665603ull;
  for (std::size_t i = 0; i < point_count; ++i) {
    const auto& point = cloud[begin_index + i];
    std::uint32_t bits = 0;
    std::memcpy(&bits, &point.x, sizeof(bits));
    h = pcoding::mixChecksum(h, bits);
    std::memcpy(&bits, &point.y, sizeof(bits));
    h = pcoding::mixChecksum(h, bits);
    std::memcpy(&bits, &point.z, sizeof(bits));
    h = pcoding::mixChecksum(h, bits);
  }
  return h;
}

template <typename PointT>
pcl::PointCloud<PointT>
makeTraitsOutputCloud(const std::size_t point_count, const std::size_t begin_index)
{
  pcl::PointCloud<PointT> cloud;
  const std::size_t total_size = begin_index + point_count + 3;
  cloud.resize(total_size);
  cloud.width = static_cast<std::uint32_t>(total_size);
  cloud.height = 1;
  cloud.is_dense = true;
  for (std::size_t i = 0; i < total_size; ++i) {
    const auto f = static_cast<float>(i);
    cloud[i].x = -1000.0f - f;
    cloud[i].y = 1000.0f + f * 0.5f;
    cloud[i].z = 17.0f - f * 0.25f;
  }
  return cloud;
}

template <>
pcl::PointCloud<pcl::PointXYZI>
makeTraitsOutputCloud<pcl::PointXYZI>(const std::size_t point_count,
                                      const std::size_t begin_index)
{
  auto cloud = makeTraitsOutputCloud<pcoding::PointXYZ>(point_count, begin_index);
  pcl::PointCloud<pcl::PointXYZI> out;
  out.resize(cloud.size());
  out.width = cloud.width;
  out.height = cloud.height;
  out.is_dense = cloud.is_dense;
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i].x = cloud[i].x;
    out[i].y = cloud[i].y;
    out[i].z = cloud[i].z;
    out[i].intensity = static_cast<float>((i * 13) % 257);
  }
  return out;
}

template <>
pcl::PointCloud<pcl::PointXYZRGB>
makeTraitsOutputCloud<pcl::PointXYZRGB>(const std::size_t point_count,
                                        const std::size_t begin_index)
{
  auto cloud = makeTraitsOutputCloud<pcoding::PointXYZ>(point_count, begin_index);
  pcl::PointCloud<pcl::PointXYZRGB> out;
  out.resize(cloud.size());
  out.width = cloud.width;
  out.height = cloud.height;
  out.is_dense = cloud.is_dense;
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i].x = cloud[i].x;
    out[i].y = cloud[i].y;
    out[i].z = cloud[i].z;
    out[i].r = static_cast<std::uint8_t>((i * 3) & 0xff);
    out[i].g = static_cast<std::uint8_t>((i * 5) & 0xff);
    out[i].b = static_cast<std::uint8_t>((i * 7) & 0xff);
  }
  return out;
}

void
runEncodeCase(const int argc,
              char** argv,
              const std::string& label,
              const std::size_t cloud_size,
              const std::size_t leaf_size,
              const int iterations,
              const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = pcoding::makePointCloud(cloud_size);
  const auto indices = makeLeafIndices(cloud_size, leaf_size, leaf_size);
  const pcoding::ReferencePoint reference{1.25, -2.5, 0.75};
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud, &indices, &reference]() {
            return runEncode(cloud, indices, reference, 0.03125f);
          });
}

void
runDecodeCase(const int argc,
              char** argv,
              const std::string& label,
              const std::size_t point_count,
              const int iterations,
              const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};
  runCase(label,
          iterations,
          warmup_iterations,
          [&encoded, &reference]() {
            return runDecode(encoded, reference, 0.0625f);
          });
}

void
runDecodeContextCase(const int argc,
                     char** argv,
                     const std::string& label,
                     const std::size_t point_count,
                     const int iterations,
                     const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};
  const std::size_t begin_index = 5;
#if defined(__RVV10__)
  auto output = pcoding::makeOutputCloudWithPadding(point_count, begin_index);
  runCase(label,
          iterations,
          warmup_iterations,
          [&encoded, &reference, &output, begin_index]() {
            pcoding::decodePointsCandidateToCloud(encoded, reference, 0.0625f, output, begin_index);
            return pcoding::checksumCloudRange(output, begin_index, encoded.size() / 3);
          });
#else
  pcl::octree::PointCoding<pcoding::PointXYZ> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  auto output = pcl::PointCloud<pcoding::PointXYZ>::Ptr(
      new pcl::PointCloud<pcoding::PointXYZ>(
          pcoding::makeOutputCloudWithPadding(point_count, begin_index)));
  runCase(label,
          iterations,
          warmup_iterations,
          [&coder, &output, &reference, begin_index, point_count]() {
            coder.initializeDecoding();
            coder.decodePoints(output,
                               pcoding::asArray(reference),
                               static_cast<pcl::uindex_t>(begin_index),
                               static_cast<pcl::uindex_t>(begin_index + point_count));
            return pcoding::checksumCloudRange(*output, begin_index, point_count);
          });
#endif
}

void
runDecodeMultiLeafCase(const int argc,
                       char** argv,
                       const std::string& label,
                       const std::size_t point_count,
                       const int iterations,
                       const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto fixture = pcoding::makeMultiLeafDecodeCase(point_count);
#if defined(__RVV10__)
  auto output = pcoding::makeOutputCloudWithPadding(point_count, fixture.begin_index);
  runCase(label,
          iterations,
          warmup_iterations,
          [&fixture, &output]() {
            pcoding::decodePointsCandidateMultiLeafToCloud(fixture, 0.0625f, output);
            return pcoding::checksumCloudRange(
                output, fixture.begin_index, fixture.encoded.size() / 3);
          });
#else
  pcl::octree::PointCoding<pcoding::PointXYZ> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = fixture.encoded;
  auto output = pcl::PointCloud<pcoding::PointXYZ>::Ptr(
      new pcl::PointCloud<pcoding::PointXYZ>(
          pcoding::makeOutputCloudWithPadding(point_count, fixture.begin_index)));
  runCase(label,
          iterations,
          warmup_iterations,
          [&coder, &fixture, &output]() {
            coder.initializeDecoding();
            std::size_t begin = fixture.begin_index;
            for (std::size_t leaf = 0; leaf < fixture.leaf_counts.size(); ++leaf) {
              const std::size_t count = fixture.leaf_counts[leaf];
              coder.decodePoints(output,
                                 pcoding::asArray(fixture.references[leaf]),
                                 static_cast<pcl::uindex_t>(begin),
                                 static_cast<pcl::uindex_t>(begin + count));
              begin += count;
            }
            return pcoding::checksumCloudRange(
                *output, fixture.begin_index, fixture.encoded.size() / 3);
          });
#endif
}

void
runDecodeProductionDirectCase(const int argc,
                              char** argv,
                              const std::string& label,
                              const std::size_t point_count,
                              const int iterations,
                              const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};
  const std::size_t begin_index = 5;
  pcl::octree::PointCoding<pcoding::PointXYZ> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  auto output = pcl::PointCloud<pcoding::PointXYZ>::Ptr(
      new pcl::PointCloud<pcoding::PointXYZ>(
          pcoding::makeOutputCloudWithPadding(point_count, begin_index)));

  runCase(label,
          iterations,
          warmup_iterations,
          [&coder, &output, &reference, begin_index, point_count]() {
            coder.initializeDecoding();
            coder.decodePoints(output,
                               pcoding::asArray(reference),
                               static_cast<pcl::uindex_t>(begin_index),
                               static_cast<pcl::uindex_t>(begin_index + point_count));
            return pcoding::checksumCloudRange(*output, begin_index, point_count);
          });
}

template <typename PointT>
void
runDecodeProductionDirectTraitsCase(const int argc,
                                    char** argv,
                                    const std::string& label,
                                    const std::size_t point_count,
                                    const int iterations,
                                    const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};
  const std::size_t begin_index = 5;
  pcl::octree::PointCoding<PointT> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  typename pcl::PointCloud<PointT>::Ptr output(
      new pcl::PointCloud<PointT>(makeTraitsOutputCloud<PointT>(point_count, begin_index)));

  runCase(label,
          iterations,
          warmup_iterations,
          [&coder, &output, &reference, begin_index, point_count]() {
            coder.initializeDecoding();
            coder.decodePoints(output,
                               pcoding::asArray(reference),
                               static_cast<pcl::uindex_t>(begin_index),
                               static_cast<pcl::uindex_t>(begin_index + point_count));
            return checksumCloudXYZRange(*output, begin_index, point_count);
          });
}

std::string
makePublicCompressedStream(const pcl::PointCloud<pcoding::PointXYZ>& input)
{
  pcl::io::OctreePointCloudCompression<pcoding::PointXYZ> encoder(
      pcl::io::MED_RES_ONLINE_COMPRESSION_WITHOUT_COLOR, false);
  std::stringstream compressed;
  encoder.encodePointCloud(input.makeShared(), compressed);
  return compressed.str();
}

std::uint64_t
runPublicDecodeOnce(const std::string& compressed_data)
{
  pcl::io::OctreePointCloudCompression<pcoding::PointXYZ> decoder;
  auto output = pcl::PointCloud<pcoding::PointXYZ>::Ptr(new pcl::PointCloud<pcoding::PointXYZ>());
  std::stringstream compressed(compressed_data);
  decoder.decodePointCloud(compressed, output);
  return pcoding::checksumCloudRange(*output, 0, output->size());
}

void
runPublicOctreeRoundtripCase(const int argc,
                             char** argv,
                             const std::string& label,
                             const std::size_t point_count,
                             const int iterations,
                             const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto input = pcoding::makePointCloud(point_count);
  runCase(label,
          iterations,
          warmup_iterations,
          [&input]() {
            const auto result = pcoding::runPublicOctreeRoundtrip(input);
            std::uint64_t h = result.checksum;
            h = pcoding::mixChecksum(h, static_cast<std::uint64_t>(result.output_size));
            h = pcoding::mixChecksum(h, static_cast<std::uint64_t>(result.compressed_bytes));
            return h;
          });
}

void
runPublicOctreeDecodeCase(const int argc,
                          char** argv,
                          const std::string& label,
                          const std::size_t point_count,
                          const int iterations,
                          const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto input = pcoding::makePointCloud(point_count);
  const auto compressed = makePublicCompressedStream(input);
  runCase(label,
          iterations,
          warmup_iterations,
          [&compressed]() { return runPublicDecodeOnce(compressed); });
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: synthetic PointXYZ leaf component ablation\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  std::cout << "Build: " << (
#if defined(__RVV10__)
                   "RVV"
#else
                   "Std"
#endif
                   )
            << '\n';

  runEncodeCase(argc, argv, "encode_indexed_16", 2048, 16, iterations, warmup_iterations);
  runEncodeCase(argc, argv, "encode_indexed_64", 4096, 64, iterations, warmup_iterations);
  runEncodeCase(argc, argv, "encode_indexed_256", 8192, 256, iterations, warmup_iterations);
  runEncodeCase(argc, argv, "encode_indexed_1024", 8192, 1024, iterations, warmup_iterations);
  runEncodeCase(argc, argv, "encode_indexed_4096", 32768, 4096, iterations, warmup_iterations);
  runEncodeCase(argc, argv, "encode_indexed_16384", 65536, 16384, iterations, warmup_iterations);
  runDecodeCase(argc, argv, "decode_contiguous_16", 16, iterations, warmup_iterations);
  runDecodeCase(argc, argv, "decode_contiguous_64", 64, iterations, warmup_iterations);
  runDecodeCase(argc, argv, "decode_contiguous_256", 256, iterations, warmup_iterations);
  runDecodeCase(argc, argv, "decode_contiguous_1024", 1024, iterations, warmup_iterations);
  runDecodeCase(argc, argv, "decode_contiguous_4096", 4096, iterations, warmup_iterations);
  runDecodeCase(argc, argv, "decode_contiguous_16384", 16384, iterations, warmup_iterations);
  runDecodeContextCase(argc, argv, "decode_context_16", 16, iterations, warmup_iterations);
  runDecodeContextCase(argc, argv, "decode_context_64", 64, iterations, warmup_iterations);
  runDecodeContextCase(argc, argv, "decode_context_256", 256, iterations, warmup_iterations);
  runDecodeContextCase(argc, argv, "decode_context_1024", 1024, iterations, warmup_iterations);
  runDecodeContextCase(argc, argv, "decode_context_4096", 4096, iterations, warmup_iterations);
  runDecodeContextCase(argc, argv, "decode_context_16384", 16384, iterations, warmup_iterations);
  runDecodeMultiLeafCase(argc, argv, "decode_multileaf_256", 256, iterations, warmup_iterations);
  runDecodeMultiLeafCase(argc, argv, "decode_multileaf_1024", 1024, iterations, warmup_iterations);
  runDecodeMultiLeafCase(argc, argv, "decode_multileaf_4096", 4096, iterations, warmup_iterations);
  runDecodeMultiLeafCase(argc, argv, "decode_multileaf_16384", 16384, iterations, warmup_iterations);
  runDecodeProductionDirectCase(
      argc, argv, "decode_production_direct_256", 256, iterations, warmup_iterations);
  runDecodeProductionDirectCase(
      argc, argv, "decode_production_direct_1024", 1024, iterations, warmup_iterations);
  runDecodeProductionDirectCase(
      argc, argv, "decode_production_direct_4096", 4096, iterations, warmup_iterations);
  runDecodeProductionDirectCase(
      argc, argv, "decode_production_direct_16384", 16384, iterations, warmup_iterations);
  runDecodeProductionDirectTraitsCase<pcl::PointXYZI>(
      argc,
      argv,
      "decode_production_direct_traits_xyzi_1024",
      1024,
      iterations,
      warmup_iterations);
  runDecodeProductionDirectTraitsCase<pcl::PointXYZI>(
      argc,
      argv,
      "decode_production_direct_traits_xyzi_4096",
      4096,
      iterations,
      warmup_iterations);
  runDecodeProductionDirectTraitsCase<pcl::PointXYZRGB>(
      argc,
      argv,
      "decode_production_direct_traits_xyzrgb_1024",
      1024,
      iterations,
      warmup_iterations);
  runDecodeProductionDirectTraitsCase<pcl::PointXYZRGB>(
      argc,
      argv,
      "decode_production_direct_traits_xyzrgb_4096",
      4096,
      iterations,
      warmup_iterations);
  runPublicOctreeRoundtripCase(
      argc, argv, "octree_roundtrip_public_256", 256, iterations, warmup_iterations);
  runPublicOctreeRoundtripCase(
      argc, argv, "octree_roundtrip_public_1024", 1024, iterations, warmup_iterations);
  runPublicOctreeDecodeCase(
      argc, argv, "octree_decode_public_256", 256, iterations, warmup_iterations);
  runPublicOctreeDecodeCase(
      argc, argv, "octree_decode_public_1024", 1024, iterations, warmup_iterations);
  return 0;
}
