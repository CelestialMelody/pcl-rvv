/*
 * 本文件做什么：
 * 这是 pcd_io_templated_writer 的 component ablation（组件消融）和
 * production-shaped diagnostic（生产形态诊断）bench 入口。
 * Std build 运行标量 reference（参考链路）；RVV build 在 `__RVV10__` 下运行
 * 测试专用 candidate（候选链路）。输出包含 checksum（校验和）和每个 case
 * 的耗时，供板卡 repeated summary（重复采集摘要）和 Evidence Doctor
 *（证据体检）使用。
 *
 * 证据边界：
 * `pointxyzrgb_*` case 只测 compressed writer 的 field-major 布局转换；
 * `compressed_*` case 计入 pack + LZF compression（LZF 压缩）；`binary_*`
 * case 测 binary writer 的 packed point-major output（按点连续的有效字段输出）。
 * `binary_tuple_*` case 是 Phase 090 test-only 诊断：compact 16B 走 memcpy
 * fast path，padding 20B 走 RVV segment store，验证能否避开 Phase 080 的
 * 跨步写出退化。
 * `production_compressed_*` 和 `production_binary_tuple_*` case 通过真实 PCDWriter
 * public entry（公开入口）写临时文件，计入 header、mmap/write 和 checksum 之外的热路径。
 * Phase 080 的 `production_binary_*` field-outer case 已随负收益 production patch 回滚，
 * 不再作为当前 bench 入口。
 * QEMU 只用于构建、正确性和日志形状；性能结论必须来自板卡。
 */

#include "pcdtw.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>

namespace pcdtw = pcl::io::rvv_test::pcd_io_templated_writer;

struct PCDTWPointXYZRGBPadding
{
  float x;
  float y;
  float z;
  std::uint32_t rgba;
  std::uint32_t ignored_padding;
};

struct PCDTWPointXYZRGBCompact
{
  float x;
  float y;
  float z;
  std::uint32_t rgba;
};

POINT_CLOUD_REGISTER_POINT_STRUCT(PCDTWPointXYZRGBPadding,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (std::uint32_t, rgba, rgba))

POINT_CLOUD_REGISTER_POINT_STRUCT(PCDTWPointXYZRGBCompact,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (std::uint32_t, rgba, rgba))

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
        const auto prefix = selected.substr(0, selected.size() - 1);
        return requested.rfind(prefix, 0) == 0;
      }
      std::istringstream stream(selected);
      std::string token;
      while (std::getline(stream, token, ',')) {
        if (token == requested)
          return true;
      }
      return false;
    }
  }
  return true;
}

std::uint64_t
checksumBytes(const std::vector<std::uint8_t>& data)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto byte : data) {
    h ^= byte;
    h *= 1099511628211ull;
  }
  return h;
}

std::uint64_t
checksumFile(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) {
    std::cerr << "failed to open output file for checksum: " << path << '\n';
    std::exit(2);
  }
  std::uint64_t h = 1469598103934665603ull;
  char byte = 0;
  while (file.get(byte)) {
    h ^= static_cast<unsigned char>(byte);
    h *= 1099511628211ull;
  }
  return h;
}

std::string
tempPathForCase(const std::string& label)
{
  std::string path = "/tmp/pcdtw_" + label + "_" + std::to_string(getpid()) + ".pcd";
  for (auto& ch : path) {
    if (ch == '*')
      ch = '_';
  }
  return path;
}

template <typename PointT>
pcl::PointCloud<PointT>
makeProductionCloud(const std::size_t point_count)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
  }
  return cloud;
}

template <>
pcl::PointCloud<pcl::PointXYZRGBA>
makeProductionCloud<pcl::PointXYZRGBA>(const std::size_t point_count)
{
  pcl::PointCloud<pcl::PointXYZRGBA> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
    point.r = static_cast<std::uint8_t>((i * 3) & 0xffu);
    point.g = static_cast<std::uint8_t>((i * 5) & 0xffu);
    point.b = static_cast<std::uint8_t>((i * 7) & 0xffu);
    point.a = 255;
  }
  return cloud;
}

template <>
pcl::PointCloud<PCDTWPointXYZRGBCompact>
makeProductionCloud<PCDTWPointXYZRGBCompact>(const std::size_t point_count)
{
  pcl::PointCloud<PCDTWPointXYZRGBCompact> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
    point.rgba = (static_cast<std::uint32_t>((i * 3) & 0xffu) << 16) |
                 (static_cast<std::uint32_t>((i * 5) & 0xffu) << 8) |
                 static_cast<std::uint32_t>((i * 7) & 0xffu);
  }
  return cloud;
}

template <>
pcl::PointCloud<PCDTWPointXYZRGBPadding>
makeProductionCloud<PCDTWPointXYZRGBPadding>(const std::size_t point_count)
{
  pcl::PointCloud<PCDTWPointXYZRGBPadding> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
    point.rgba = (static_cast<std::uint32_t>((i * 3) & 0xffu) << 16) |
                 (static_cast<std::uint32_t>((i * 5) & 0xffu) << 8) |
                 static_cast<std::uint32_t>((i * 7) & 0xffu);
    point.ignored_padding = 0xa5a50000u | static_cast<std::uint32_t>(i & 0xffffu);
  }
  return cloud;
}

template <typename Pack>
void
runCase(const int argc,
        char** argv,
        const std::string& label,
        const std::size_t point_count,
        const std::size_t point_step,
        const std::vector<pcdtw::FieldLayout>& fields,
        const int iterations,
        const int warmup_iterations,
        Pack pack)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);
  std::vector<std::uint8_t> output;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    if (!pack(cloud.data(), point_count, point_step, fields, output)) {
      std::cerr << "case failed during warmup: " << label << '\n';
      std::exit(2);
    }
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    if (!pack(cloud.data(), point_count, point_step, fields, output)) {
      std::cerr << "case failed during timed run: " << label << '\n';
      std::exit(2);
    }
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  checksum ^= checksumBytes(output) + static_cast<std::uint64_t>(iterations + 17);

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << point_step << '\n';
  doNotOptimize(checksum);
}

template <typename PointT>
void
runProductionCompressedCase(const int argc,
                            char** argv,
                            const std::string& label,
                            const std::size_t point_count,
                            const int iterations,
                            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makeProductionCloud<PointT>(point_count);
  const auto path = tempPathForCase(label);
  pcl::PCDWriter writer;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    if (writer.writeBinaryCompressed<PointT>(path, cloud) != 0) {
      std::cerr << "production compressed case failed during warmup: " << label << '\n';
      std::exit(2);
    }
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    if (writer.writeBinaryCompressed<PointT>(path, cloud) != 0) {
      std::cerr << "production compressed case failed during timed run: " << label << '\n';
      std::exit(2);
    }
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  checksum ^= checksumFile(path) + static_cast<std::uint64_t>(iterations + 17);
  std::remove(path.c_str());

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << sizeof(PointT) << '\n';
  doNotOptimize(checksum);
}

template <typename PointT>
void
runProductionBinaryCase(const int argc,
                        char** argv,
                        const std::string& label,
                        const std::size_t point_count,
                        const int iterations,
                        const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makeProductionCloud<PointT>(point_count);
  const auto path = tempPathForCase(label);
  pcl::PCDWriter writer;
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    if (writer.writeBinary<PointT>(path, cloud) != 0) {
      std::cerr << "production binary case failed during warmup: " << label << '\n';
      std::exit(2);
    }
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    if (writer.writeBinary<PointT>(path, cloud) != 0) {
      std::cerr << "production binary case failed during timed run: " << label << '\n';
      std::exit(2);
    }
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  checksum ^= checksumFile(path) + static_cast<std::uint64_t>(iterations + 17);
  std::remove(path.c_str());

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << sizeof(PointT) << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: pcd_io_templated_writer synthetic PointCloud field packing\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';

#if defined(__RVV10__)
  auto pack = [](const std::uint8_t* cloud,
                 const std::size_t point_count,
                 const std::size_t point_step,
                 const std::vector<pcdtw::FieldLayout>& fields,
                 std::vector<std::uint8_t>& packed) {
    pcdtw::packFieldsCandidate(cloud, point_count, point_step, fields, packed);
    return true;
  };
  auto pack_and_compress = [](const std::uint8_t* cloud,
                              const std::size_t point_count,
                              const std::size_t point_step,
                              const std::vector<pcdtw::FieldLayout>& fields,
                              std::vector<std::uint8_t>& compressed) {
    return pcdtw::packAndCompressCandidate(
        cloud, point_count, point_step, fields, compressed);
  };
  auto binary_pack = [](const std::uint8_t* cloud,
                        const std::size_t point_count,
                        const std::size_t point_step,
                        const std::vector<pcdtw::FieldLayout>& fields,
                        std::vector<std::uint8_t>& packed) {
    pcdtw::packBinaryFieldsCandidate(cloud, point_count, point_step, fields, packed);
    return true;
  };
  auto binary_tuple_pack = [](const std::uint8_t* cloud,
                              const std::size_t point_count,
                              const std::size_t point_step,
                              const std::vector<pcdtw::FieldLayout>& fields,
                              std::vector<std::uint8_t>& packed) {
    pcdtw::packBinaryFieldsTupleCandidate(
        cloud, point_count, point_step, fields, packed);
    return true;
  };
#else
  auto pack = [](const std::uint8_t* cloud,
                 const std::size_t point_count,
                 const std::size_t point_step,
                 const std::vector<pcdtw::FieldLayout>& fields,
                 std::vector<std::uint8_t>& packed) {
    pcdtw::packFieldsScalar(cloud, point_count, point_step, fields, packed);
    return true;
  };
  auto pack_and_compress = [](const std::uint8_t* cloud,
                              const std::size_t point_count,
                              const std::size_t point_step,
                              const std::vector<pcdtw::FieldLayout>& fields,
                              std::vector<std::uint8_t>& compressed) {
    return pcdtw::packAndCompressScalar(
        cloud, point_count, point_step, fields, compressed);
  };
  auto binary_pack = [](const std::uint8_t* cloud,
                        const std::size_t point_count,
                        const std::size_t point_step,
                        const std::vector<pcdtw::FieldLayout>& fields,
                        std::vector<std::uint8_t>& packed) {
    pcdtw::packBinaryFieldsScalar(cloud, point_count, point_step, fields, packed);
    return true;
  };
  auto binary_tuple_pack = [](const std::uint8_t* cloud,
                              const std::size_t point_count,
                              const std::size_t point_step,
                              const std::vector<pcdtw::FieldLayout>& fields,
                              std::vector<std::uint8_t>& packed) {
    pcdtw::packBinaryFieldsScalar(cloud, point_count, point_step, fields, packed);
    return true;
  };
#endif

  const std::vector<pcdtw::FieldLayout> xyzrgb_fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  runCase(argc,
          argv,
          "pointxyzrgb_4f_262k",
          262144,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          pack);

  runCase(argc,
          argv,
          "pointxyzrgb_4f_padding_262k",
          262144,
          20,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          pack);

  runCase(argc,
          argv,
          "pointxyzrgb_4f_small_512",
          512,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          pack);

  runCase(argc,
          argv,
          "compressed_pointxyzrgb_4f_262k",
          262144,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          pack_and_compress);

  runCase(argc,
          argv,
          "compressed_pointxyzrgb_4f_padding_262k",
          262144,
          20,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          pack_and_compress);

  runCase(argc,
          argv,
          "compressed_pointxyzrgb_4f_small_512",
          512,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          pack_and_compress);

  runCase(argc,
          argv,
          "binary_pointxyzrgb_4f_262k",
          262144,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          binary_pack);

  runCase(argc,
          argv,
          "binary_pointxyzrgb_4f_padding_262k",
          262144,
          20,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          binary_pack);

  runCase(argc,
          argv,
          "binary_pointxyzrgb_4f_small_512",
          512,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          binary_pack);

  runCase(argc,
          argv,
          "binary_tuple_pointxyzrgb_4f_262k",
          262144,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          binary_tuple_pack);

  runCase(argc,
          argv,
          "binary_tuple_pointxyzrgb_4f_padding_262k",
          262144,
          20,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          binary_tuple_pack);

  runCase(argc,
          argv,
          "binary_tuple_pointxyzrgb_4f_small_512",
          512,
          16,
          xyzrgb_fields,
          iterations,
          warmup_iterations,
          binary_tuple_pack);

  runProductionCompressedCase<PCDTWPointXYZRGBCompact>(
      argc,
      argv,
      "production_compressed_pointxyzrgba_4f_compact_262k",
      262144,
      iterations,
      warmup_iterations);

  runProductionCompressedCase<PCDTWPointXYZRGBPadding>(
      argc,
      argv,
      "production_compressed_pointxyzrgba_4f_padding_262k",
      262144,
      iterations,
      warmup_iterations);

  runProductionCompressedCase<PCDTWPointXYZRGBCompact>(
      argc,
      argv,
      "production_compressed_pointxyzrgba_4f_compact_small_512",
      512,
      iterations,
      warmup_iterations);

  runProductionBinaryCase<PCDTWPointXYZRGBCompact>(
      argc,
      argv,
      "production_binary_tuple_pointxyzrgba_4f_compact_262k",
      262144,
      iterations,
      warmup_iterations);

  runProductionBinaryCase<PCDTWPointXYZRGBPadding>(
      argc,
      argv,
      "production_binary_tuple_pointxyzrgba_4f_padding_262k",
      262144,
      iterations,
      warmup_iterations);

  runProductionBinaryCase<PCDTWPointXYZRGBCompact>(
      argc,
      argv,
      "production_binary_tuple_pointxyzrgba_4f_compact_small_512",
      512,
      iterations,
      warmup_iterations);

  return 0;
}
