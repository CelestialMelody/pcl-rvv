/*
 * 本文件做什么：
 * 这是 pcd_io 的 component ablation（组件消融）bench。Std build 运行
 * 标量参考链路；RVV build 在 `__RVV10__` 下运行 4 字节字段 stride load/store
 * 候选，以及 reader shaped unpack + finite scan。输出包含 checksum（校验和）
 * 和每个 case 的耗时，供 repeated board summary（重复板卡摘要）和 Evidence
 * Doctor（证据体检）使用。
 *
 * 证据边界：
 * 这些 case 只测 compressed PCD 前后的 field layout conversion（字段布局转换）
 * 组件，不包含 LZF compression、mmap、stream I/O、header 生成或 finite scan。
 */

#include "pcd_io.h"

#include <pcl/io/pcd_io.h>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>
#include <string>
#include <vector>

namespace pcd = pcl::io::rvv_test::pcd_io;

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
      return selected == requested || selected == "all" ||
             (selected == "component" &&
              (requested.rfind("pack_", 0) == 0 || requested.rfind("unpack_", 0) == 0)) ||
             (selected == "writer_payload" && requested.rfind("writer_payload_", 0) == 0) ||
             (selected == "reader_payload" && requested.rfind("reader_payload_", 0) == 0) ||
             (selected == "production_writer" &&
              requested.rfind("production_writer_", 0) == 0);
    }
  }
  return true;
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

std::uint64_t
checksumString(const std::string& values)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto value : values) {
    h ^= static_cast<std::uint8_t>(value);
    h *= 1099511628211ull;
  }
  return h;
}

pcl::PCLPointField
makePclField(const std::string& name,
             const std::uint32_t offset,
             const std::uint8_t datatype,
             const std::uint32_t count = 1)
{
  pcl::PCLPointField field;
  field.name = name;
  field.offset = offset;
  field.datatype = datatype;
  field.count = count;
  return field;
}

pcl::PCLPointCloud2
makePclCloud(const std::size_t point_count,
             const std::uint32_t point_step,
             const std::vector<pcl::PCLPointField>& pcl_fields,
             const std::vector<pcd::FieldLayout>& layouts)
{
  pcl::PCLPointCloud2 cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.point_step = point_step;
  cloud.row_step = point_step * cloud.width;
  cloud.is_dense = true;
  cloud.fields = pcl_fields;
  cloud.data = pcd::makeInterleavedCloud(point_count, point_step, layouts);
  return cloud;
}

void
runPackCase(const int argc,
            char** argv,
            const std::string& label,
            const std::size_t point_count,
            const std::size_t point_step,
            const std::vector<pcd::FieldLayout>& fields,
            const int iterations,
            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);
  std::vector<std::uint8_t> packed;
  std::uint64_t checksum = 1469598103934665603ull;

  for (int i = 0; i < warmup_iterations; ++i) {
    pcd::packFieldsCandidate(cloud.data(), point_count, point_step, fields, packed);
    checksum ^= checksumBytes(packed) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    pcd::packFieldsCandidate(cloud.data(), point_count, point_step, fields, packed);
    checksum ^= checksumBytes(packed) + static_cast<std::uint64_t>(i + 17);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << point_step << '\n';
  doNotOptimize(checksum);
}

void
runUnpackCase(const int argc,
              char** argv,
              const std::string& label,
              const std::size_t point_count,
              const std::size_t point_step,
              const std::vector<pcd::FieldLayout>& fields,
              const int iterations,
              const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);
  std::vector<std::uint8_t> packed;
  pcd::packFieldsScalar(cloud.data(), point_count, point_step, fields, packed);

  std::vector<std::uint8_t> unpacked(point_count * point_step, 0xcc);
  std::uint64_t checksum = 1469598103934665603ull;

  for (int i = 0; i < warmup_iterations; ++i) {
    pcd::unpackFieldsCandidate(packed.data(), point_count, point_step, fields, unpacked);
    checksum ^= checksumBytes(unpacked) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    pcd::unpackFieldsCandidate(packed.data(), point_count, point_step, fields, unpacked);
    checksum ^= checksumBytes(unpacked) + static_cast<std::uint64_t>(i + 31);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << point_step << '\n';
  doNotOptimize(checksum);
}

void
runWriterPayloadCase(const int argc,
                     char** argv,
                     const std::string& label,
                     const std::size_t point_count,
                     const std::size_t point_step,
                     const std::vector<pcd::FieldLayout>& fields,
                     const int iterations,
                     const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);
  std::vector<std::uint8_t> payload;
  std::uint64_t checksum = 1469598103934665603ull;

  for (int i = 0; i < warmup_iterations; ++i) {
    if (!pcd::makeCompressedWriterPayloadCandidate(
            cloud.data(), point_count, point_step, fields, payload)) {
      std::cerr << "failed to generate compressed writer payload for " << label << '\n';
      std::exit(2);
    }
    checksum ^= checksumBytes(payload) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    if (!pcd::makeCompressedWriterPayloadCandidate(
            cloud.data(), point_count, point_step, fields, payload)) {
      std::cerr << "failed to generate compressed writer payload for " << label << '\n';
      std::exit(2);
    }
    checksum ^= checksumBytes(payload) + static_cast<std::uint64_t>(i + 47);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << point_step << '\n';
  doNotOptimize(checksum);
}

void
runReaderPayloadCase(const int argc,
                     char** argv,
                     const std::string& label,
                     const std::size_t point_count,
                     const std::size_t point_step,
                     const std::vector<pcd::FieldLayout>& fields,
                     const int iterations,
                     const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  auto cloud = pcd::makeInterleavedFloatCloud(point_count, point_step, fields);
  pcd::writeFloat32(cloud, point_step, fields[1].offset, 17, std::numeric_limits<float>::quiet_NaN());
  std::vector<std::uint8_t> payload;
  if (!pcd::makeCompressedWriterPayloadScalar(
          cloud.data(), point_count, point_step, fields, payload)) {
    std::cerr << "failed to generate compressed reader payload input for " << label << '\n';
    std::exit(2);
  }

  std::vector<std::uint8_t> unpacked(point_count * point_step, 0xcc);
  bool is_dense = true;
  std::uint64_t checksum = 1469598103934665603ull;

  for (int i = 0; i < warmup_iterations; ++i) {
    if (!pcd::readCompressedBodyCandidate(
            payload.data(), payload.size(), point_count, point_step, fields, unpacked, is_dense)) {
      std::cerr << "failed to read compressed reader payload for " << label << '\n';
      std::exit(2);
    }
    checksum ^= checksumBytes(unpacked) + static_cast<std::uint64_t>(is_dense ? 1 : 0) +
                static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    if (!pcd::readCompressedBodyCandidate(
            payload.data(), payload.size(), point_count, point_step, fields, unpacked, is_dense)) {
      std::cerr << "failed to read compressed reader payload for " << label << '\n';
      std::exit(2);
    }
    checksum ^= checksumBytes(unpacked) + static_cast<std::uint64_t>(is_dense ? 1 : 0) +
                static_cast<std::uint64_t>(i + 61);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << point_step << '\n';
  doNotOptimize(checksum);
}

void
runProductionWriterCase(const int argc,
                        char** argv,
                        const std::string& label,
                        const std::size_t point_count,
                        const std::uint32_t point_step,
                        const std::vector<pcl::PCLPointField>& pcl_fields,
                        const std::vector<pcd::FieldLayout>& fields,
                        const int iterations,
                        const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = makePclCloud(point_count, point_step, pcl_fields, fields);
  pcl::PCDWriter writer;
  std::string output;
  std::uint64_t checksum = 1469598103934665603ull;

  for (int i = 0; i < warmup_iterations; ++i) {
    std::ostringstream stream(std::ios::binary);
    if (writer.writeBinaryCompressed(stream, cloud) != 0) {
      std::cerr << "failed to write production compressed PCD for " << label << '\n';
      std::exit(2);
    }
    output = stream.str();
    checksum ^= checksumString(output) + static_cast<std::uint64_t>(i);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    std::ostringstream stream(std::ios::binary);
    if (writer.writeBinaryCompressed(stream, cloud) != 0) {
      std::cerr << "failed to write production compressed PCD for " << label << '\n';
      std::exit(2);
    }
    output = stream.str();
    checksum ^= checksumString(output) + static_cast<std::uint64_t>(i + 73);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << point_count
            << ", point_step: " << point_step << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const std::vector<pcd::FieldLayout> xyzi = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const std::vector<pcd::FieldLayout> padded_xyzi = {
      {0, 4},
      {4, 4},
      {8, 4},
      {16, 4},
  };
  const std::vector<pcd::FieldLayout> finite_xyzi = {
      {0, 4, pcd::FiniteKind::Float32},
      {4, 4, pcd::FiniteKind::Float32},
      {8, 4, pcd::FiniteKind::Float32},
      {12, 4, pcd::FiniteKind::None},
  };
  const std::vector<pcd::FieldLayout> finite_padded_xyzi = {
      {0, 4, pcd::FiniteKind::Float32},
      {4, 4, pcd::FiniteKind::Float32},
      {8, 4, pcd::FiniteKind::Float32},
      {16, 4, pcd::FiniteKind::None},
  };

  std::cout << "Dataset: synthetic PCLPointCloud2 compressed layout conversion component\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';

  runPackCase(argc, argv, "pack_xyzi_307k", 307200, 16, xyzi, iterations, warmup_iterations);
  runUnpackCase(argc, argv, "unpack_xyzi_307k", 307200, 16, xyzi, iterations, warmup_iterations);
  runPackCase(argc, argv, "pack_padded_xyzi_307k", 307200, 24, padded_xyzi, iterations, warmup_iterations);
  runUnpackCase(argc, argv, "unpack_padded_xyzi_307k", 307200, 24, padded_xyzi, iterations, warmup_iterations);
  runWriterPayloadCase(argc,
                       argv,
                       "writer_payload_xyzi_307k",
                       307200,
                       16,
                       xyzi,
                       iterations,
                       warmup_iterations);
  runWriterPayloadCase(argc,
                       argv,
                       "writer_payload_padded_xyzi_307k",
                       307200,
                       24,
                       padded_xyzi,
                       iterations,
                       warmup_iterations);
  runReaderPayloadCase(argc,
                       argv,
                       "reader_payload_xyzi_307k",
                       307200,
                       16,
                       finite_xyzi,
                       iterations,
                       warmup_iterations);
  runReaderPayloadCase(argc,
                       argv,
                       "reader_payload_padded_xyzi_307k",
                       307200,
                       24,
                       finite_padded_xyzi,
                       iterations,
                       warmup_iterations);
  runProductionWriterCase(argc,
                          argv,
                          "production_writer_xyzi_307k",
                          307200,
                          16,
                          {makePclField("x", 0, pcl::PCLPointField::FLOAT32),
                           makePclField("y", 4, pcl::PCLPointField::FLOAT32),
                           makePclField("z", 8, pcl::PCLPointField::FLOAT32),
                           makePclField("intensity", 12, pcl::PCLPointField::FLOAT32)},
                          xyzi,
                          iterations,
                          warmup_iterations);
  runProductionWriterCase(argc,
                          argv,
                          "production_writer_padded_xyzi_307k",
                          307200,
                          24,
                          {makePclField("x", 0, pcl::PCLPointField::FLOAT32),
                           makePclField("y", 4, pcl::PCLPointField::FLOAT32),
                           makePclField("z", 8, pcl::PCLPointField::FLOAT32),
                           makePclField("_", 12, pcl::PCLPointField::UINT32),
                           makePclField("intensity", 16, pcl::PCLPointField::FLOAT32)},
                          padded_xyzi,
                          iterations,
                          warmup_iterations);

  return 0;
}
