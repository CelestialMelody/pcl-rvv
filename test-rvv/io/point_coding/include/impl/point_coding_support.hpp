/*
 * 本文件做什么：
 * 这个内部头保存 point_coding 首阶段测试支撑：输入构造、标量参考链路、
 * RVV candidate（RVV 候选）和结果 checksum（校验和）。它复刻
 * PointCoding::encodePoints/decodePoints 的 leaf-level 公式，用于判断
 * indexed gather（离散加载）和连续 decode 是否值得进入后续生产形态诊断。
 *
 * 证据边界：
 * 这些 helper 只服务 test-rvv/io/point_coding，不修改 production 源码。
 * 正确性通过只能证明局部 component（组件）语义一致，不能证明完整
 * OctreePointCloudCompression public entry（公开入口）的收益。
 */

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/types.h>
#include <pcl/compression/octree_pointcloud_compression.h>
#include <pcl/compression/point_coding.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_test::point_coding {

using PointXYZ = pcl::PointXYZ;

struct ReferencePoint {
  double x;
  double y;
  double z;
};

struct MultiLeafDecodeCase {
  std::vector<char> encoded;
  std::vector<std::size_t> leaf_counts;
  std::vector<ReferencePoint> references;
  std::size_t begin_index{0};
};

struct PublicRoundtripResult {
  std::size_t input_size{0};
  std::size_t output_size{0};
  std::uint32_t output_height{0};
  bool output_is_dense{false};
  bool all_finite_xyz{false};
  std::uint64_t checksum{0};
  std::size_t compressed_bytes{0};
};

inline const double*
asArray(const ReferencePoint& reference)
{
  return &reference.x;
}

inline pcl::PointCloud<PointXYZ>
makePointCloud(const std::size_t size)
{
  pcl::PointCloud<PointXYZ> cloud;
  cloud.resize(size);
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = true;
  for (std::size_t i = 0; i < size; ++i) {
    const auto f = static_cast<float>(i);
    cloud[i].x = 1.25f + static_cast<float>((i * 17) % 97) * 0.03125f - f * 0.0005f;
    cloud[i].y = -2.5f + static_cast<float>((i * 29) % 83) * 0.015625f;
    cloud[i].z = 0.75f + static_cast<float>((i * 41) % 71) * 0.0078125f;
  }
  return cloud;
}

inline pcl::PointCloud<PointXYZ>
makePointCloudWithClampCases()
{
  pcl::PointCloud<PointXYZ> cloud;
  cloud.resize(6);
  cloud.width = 6;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud[0].x = 3.0f;
  cloud[0].y = -3.0f;
  cloud[0].z = 0.0f;
  cloud[1].x = 1.28f;
  cloud[1].y = -1.28f;
  cloud[1].z = 0.01f;
  cloud[2].x = 0.11f;
  cloud[2].y = -0.11f;
  cloud[2].z = 0.12f;
  cloud[3].x = -3.5f;
  cloud[3].y = 3.5f;
  cloud[3].z = -0.13f;
  cloud[4].x = 0.0f;
  cloud[4].y = 0.0f;
  cloud[4].z = 1.5f;
  cloud[5].x = -1.5f;
  cloud[5].y = 1.5f;
  cloud[5].z = -1.5f;
  return cloud;
}

inline pcl::PointCloud<PointXYZ>
makeOutputCloudWithPadding(const std::size_t point_count, const std::size_t begin_index)
{
  pcl::PointCloud<PointXYZ> cloud;
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

inline std::vector<char>
makeEncodedDiffs(const std::size_t point_count)
{
  std::vector<char> encoded(point_count * 3);
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    const auto value = static_cast<unsigned char>((i * 37 + 11) & 0xff);
    encoded[i] = static_cast<char>(value);
  }
  return encoded;
}

inline MultiLeafDecodeCase
makeMultiLeafDecodeCase(const std::size_t point_count)
{
  MultiLeafDecodeCase fixture;
  fixture.encoded = makeEncodedDiffs(point_count);
  fixture.begin_index = 7;

  const std::array<std::size_t, 8> pattern{{5, 17, 9, 64, 23, 128, 31, 256}};
  std::size_t remaining = point_count;
  std::size_t leaf_id = 0;
  while (remaining > 0) {
    const std::size_t requested = pattern[leaf_id % pattern.size()];
    const std::size_t count = std::min(requested, remaining);
    fixture.leaf_counts.push_back(count);
    const double step = static_cast<double>(leaf_id);
    fixture.references.push_back(ReferencePoint{
        -8.0 + step * 0.125,
        3.0 - step * 0.0625,
        1.5 + static_cast<double>((leaf_id * 3) % 11) * 0.03125});
    remaining -= count;
    ++leaf_id;
  }
  return fixture;
}

inline unsigned char
encodeOneScalar(const float value, const double reference, const float resolution)
{
  const int quantized = static_cast<int>((value - reference) / resolution);
  return static_cast<unsigned char>(std::max(-127, std::min(127, quantized)));
}

inline void
encodePointsScalar(const pcl::PointCloud<PointXYZ>& cloud,
                   const pcl::Indices& indices,
                   const ReferencePoint& reference,
                   const float resolution,
                   std::vector<char>& out)
{
  out.clear();
  out.reserve(indices.size() * 3);
  for (const auto idx : indices) {
    const auto& point = cloud[static_cast<std::size_t>(idx)];
    out.push_back(static_cast<char>(encodeOneScalar(point.x, reference.x, resolution)));
    out.push_back(static_cast<char>(encodeOneScalar(point.y, reference.y, resolution)));
    out.push_back(static_cast<char>(encodeOneScalar(point.z, reference.z, resolution)));
  }
}

inline void
decodePointsScalar(const std::vector<char>& encoded,
                   const ReferencePoint& reference,
                   const float resolution,
                   std::vector<PointXYZ>& out)
{
  const std::size_t point_count = encoded.size() / 3;
  out.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    const auto diff_x = static_cast<unsigned char>(encoded[i * 3 + 0]);
    const auto diff_y = static_cast<unsigned char>(encoded[i * 3 + 1]);
    const auto diff_z = static_cast<unsigned char>(encoded[i * 3 + 2]);
    out[i].x = static_cast<float>(reference.x + (diff_x + 0.5) * resolution);
    out[i].y = static_cast<float>(reference.y + (diff_y + 0.5) * resolution);
    out[i].z = static_cast<float>(reference.z + (diff_z + 0.5) * resolution);
  }
}

inline void
decodePointsScalarToCloudBytes(const char* encoded,
                               const std::size_t point_count,
                               const ReferencePoint& reference,
                               const float resolution,
                               pcl::PointCloud<PointXYZ>& output,
                               const std::size_t begin_index)
{
  for (std::size_t i = 0; i < point_count; ++i) {
    const auto diff_x = static_cast<unsigned char>(encoded[i * 3 + 0]);
    const auto diff_y = static_cast<unsigned char>(encoded[i * 3 + 1]);
    const auto diff_z = static_cast<unsigned char>(encoded[i * 3 + 2]);
    auto& point = output[begin_index + i];
    point.x = static_cast<float>(reference.x + (diff_x + 0.5) * resolution);
    point.y = static_cast<float>(reference.y + (diff_y + 0.5) * resolution);
    point.z = static_cast<float>(reference.z + (diff_z + 0.5) * resolution);
  }
}

inline void
decodePointsScalarToCloud(const std::vector<char>& encoded,
                          const ReferencePoint& reference,
                          const float resolution,
                          pcl::PointCloud<PointXYZ>& output,
                          const std::size_t begin_index)
{
  decodePointsScalarToCloudBytes(
      encoded.data(), encoded.size() / 3, reference, resolution, output, begin_index);
}

inline pcl::PointCloud<PointXYZ>
decodePointsProductionObject(const std::vector<char>& encoded,
                             const ReferencePoint& reference,
                             const float resolution,
                             const std::size_t begin_index)
{
  // production-shaped reference（生产形态参考链路）：这里使用真实 PointCoding
  // 对象保存 diff vector、重置 iterator，再调用 production 标量 decodePoints。
  // 它仍然只在 test-rvv 中使用，不代表 production 已有 RVV dispatch。
  pcl::octree::PointCoding<PointXYZ> coder;
  coder.setPrecision(resolution);
  coder.getDifferentialDataVector() = encoded;
  coder.initializeDecoding();

  auto output = pcl::PointCloud<PointXYZ>::Ptr(new pcl::PointCloud<PointXYZ>(
      makeOutputCloudWithPadding(encoded.size() / 3, begin_index)));
  coder.decodePoints(output,
                     asArray(reference),
                     static_cast<pcl::uindex_t>(begin_index),
                     static_cast<pcl::uindex_t>(begin_index + encoded.size() / 3));
  return *output;
}

inline pcl::PointCloud<PointXYZ>
decodePointsProductionObjectMultiLeaf(const MultiLeafDecodeCase& fixture, const float resolution)
{
  // 这条 reference path（参考链路）模拟 OctreePointCloudCompression 解码时
  // point_coder_ 的多 leaf 调用：diff vector 一次性放入真实 PointCoding 对象，
  // iterator 只初始化一次，然后每个 leaf 用自己的 lower voxel corner 作为 reference。
  // 它不包含 entropy decoding（熵解码）和真实 tree traversal（树遍历）。
  pcl::octree::PointCoding<PointXYZ> coder;
  coder.setPrecision(resolution);
  coder.getDifferentialDataVector() = fixture.encoded;
  coder.initializeDecoding();

  auto output = pcl::PointCloud<PointXYZ>::Ptr(new pcl::PointCloud<PointXYZ>(
      makeOutputCloudWithPadding(fixture.encoded.size() / 3, fixture.begin_index)));
  std::size_t begin = fixture.begin_index;
  for (std::size_t leaf = 0; leaf < fixture.leaf_counts.size(); ++leaf) {
    const std::size_t count = fixture.leaf_counts[leaf];
    coder.decodePoints(output,
                       asArray(fixture.references[leaf]),
                       static_cast<pcl::uindex_t>(begin),
                       static_cast<pcl::uindex_t>(begin + count));
    begin += count;
  }
  return *output;
}

inline std::uint64_t
mixChecksum(std::uint64_t h, const std::uint64_t value)
{
  h ^= value + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
  return h;
}

inline std::uint64_t
checksumBytes(const std::vector<char>& values)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const char value : values) {
    h ^= static_cast<unsigned char>(value);
    h *= 1099511628211ull;
  }
  return h;
}

inline std::uint64_t
checksumPoints(const std::vector<PointXYZ>& points)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto& point : points) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &point.x, sizeof(bits));
    h = mixChecksum(h, bits);
    std::memcpy(&bits, &point.y, sizeof(bits));
    h = mixChecksum(h, bits);
    std::memcpy(&bits, &point.z, sizeof(bits));
    h = mixChecksum(h, bits);
  }
  return h;
}

inline std::uint64_t
checksumCloudRange(const pcl::PointCloud<PointXYZ>& cloud,
                   const std::size_t begin_index,
                   const std::size_t point_count)
{
  std::uint64_t h = 1469598103934665603ull;
  for (std::size_t i = 0; i < point_count; ++i) {
    const auto& point = cloud[begin_index + i];
    std::uint32_t bits = 0;
    std::memcpy(&bits, &point.x, sizeof(bits));
    h = mixChecksum(h, bits);
    std::memcpy(&bits, &point.y, sizeof(bits));
    h = mixChecksum(h, bits);
    std::memcpy(&bits, &point.z, sizeof(bits));
    h = mixChecksum(h, bits);
  }
  return h;
}

inline PublicRoundtripResult
runPublicOctreeRoundtrip(const pcl::PointCloud<PointXYZ>& input)
{
  // public roundtrip（公开往返）使用真实 OctreePointCloudCompression public
  // API。Phase 060/070 之后，RVV build 的解码阶段会命中已采纳的
  // PointCoding::decodePoints dispatch；完整往返仍会包含 tree traversal
  // 和 entropy coding 成本，所以它只能作为更外层 public boundary 证据。
  pcl::io::OctreePointCloudCompression<PointXYZ> encoder(
      pcl::io::MED_RES_ONLINE_COMPRESSION_WITHOUT_COLOR, false);
  pcl::io::OctreePointCloudCompression<PointXYZ> decoder;
  auto input_ptr = input.makeShared();
  auto output = pcl::PointCloud<PointXYZ>::Ptr(new pcl::PointCloud<PointXYZ>());
  std::stringstream compressed;

  encoder.encodePointCloud(input_ptr, compressed);
  decoder.decodePointCloud(compressed, output);

  PublicRoundtripResult result;
  result.input_size = input.size();
  result.output_size = output->size();
  result.output_height = output->height;
  result.output_is_dense = output->is_dense;
  result.compressed_bytes = compressed.str().size();
  result.all_finite_xyz = true;
  for (const auto& point : output->points) {
    result.all_finite_xyz = result.all_finite_xyz && std::isfinite(point.x) &&
                             std::isfinite(point.y) && std::isfinite(point.z);
  }
  result.checksum = checksumCloudRange(*output, 0, output->size());
  return result;
}

#if defined(__RVV10__) && defined(PCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE)

inline void
storeQuantizedDoubleChunk(const vfloat32m2_t value,
                          const double reference,
                          const double resolution,
                          std::int32_t* temp,
                          const std::size_t vl)
{
  vfloat64m4_t widened = __riscv_vfwcvt_f_f_v_f64m4(value, vl);
  widened = __riscv_vfsub_vf_f64m4(widened, reference, vl);
  widened = __riscv_vfdiv_vf_f64m4(widened, resolution, vl);
  vint64m4_t quantized = __riscv_vfcvt_rtz_x_f_v_i64m4(widened, vl);
  quantized = __riscv_vmax_vx_i64m4(quantized, -127, vl);
  quantized = __riscv_vmin_vx_i64m4(quantized, 127, vl);
  __riscv_vse32_v_i32m2(temp, __riscv_vncvt_x_x_w_i32m2(quantized, vl), vl);
}

#endif // __RVV10__ && PCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE

#if defined(__RVV10__)

inline void
encodePointsRVV(const pcl::PointCloud<PointXYZ>& cloud,
                const pcl::Indices& indices,
                const ReferencePoint& reference,
                const float resolution,
                std::vector<char>& out)
{
  static_assert(sizeof(pcl::index_t) == sizeof(std::int32_t),
                "point_coding test support expects 32-bit pcl::Indices entries.");
#if defined(PCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE)
  if (indices.size() < 64) {
    // f64 quantize（双精度量化）能复刻 production 的截断语义，但 16 点 leaf
    // 的板卡结果 5/5 退化；这个 gate（验收/分流条件）把过小 leaf 保持在
    // 同构标量路径，避免把诊断候选外推到不划算的小规模。
    encodePointsScalar(cloud, indices, reference, resolution, out);
    return;
  }
#endif

  out.clear();
  out.resize(indices.size() * 3);

  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
#if defined(PCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE)
  std::vector<std::int32_t> x_values(vlmax);
  std::vector<std::int32_t> y_values(vlmax);
  std::vector<std::int32_t> z_values(vlmax);
  const double resolution_double = static_cast<double>(resolution);
#else
  std::vector<float> x_values(vlmax);
  std::vector<float> y_values(vlmax);
  std::vector<float> z_values(vlmax);
#endif

  std::size_t i = 0;
  while (i < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const vint32m2_t idx_i32 = __riscv_vle32_v_i32m2(indices.data() + i, vl);
    const vuint32m2_t idx_u32 = __riscv_vreinterpret_v_i32m2_u32m2(idx_i32);
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<PointXYZ>(idx_u32, vl);

    vfloat32m2_t x;
    vfloat32m2_t y;
    vfloat32m2_t z;
    pcl::rvv_load::indexed_load3_f32m2<PointXYZ, offsetof(PointXYZ, x), offsetof(PointXYZ, y), offsetof(PointXYZ, z)>(
        base, offsets, vl, x, y, z);

#if defined(PCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE)
    storeQuantizedDoubleChunk(x, reference.x, resolution_double, x_values.data(), vl);
    storeQuantizedDoubleChunk(y, reference.y, resolution_double, y_values.data(), vl);
    storeQuantizedDoubleChunk(z, reference.z, resolution_double, z_values.data(), vl);

    for (std::size_t lane = 0; lane < vl; ++lane) {
      out[(i + lane) * 3 + 0] = static_cast<char>(static_cast<unsigned char>(x_values[lane]));
      out[(i + lane) * 3 + 1] = static_cast<char>(static_cast<unsigned char>(y_values[lane]));
      out[(i + lane) * 3 + 2] = static_cast<char>(static_cast<unsigned char>(z_values[lane]));
    }
#else
    // Phase 020 的 f64 probe 证明“全向量双精度量化”成本过高；默认 candidate
    // 回到 Phase 010 的形态：RVV 只承担 indexed gather（按索引离散加载），
    // 量化继续走 same-chain scalar helper（同构标量 helper），保证边界语义。
    __riscv_vse32_v_f32m2(x_values.data(), x, vl);
    __riscv_vse32_v_f32m2(y_values.data(), y, vl);
    __riscv_vse32_v_f32m2(z_values.data(), z, vl);

    for (std::size_t lane = 0; lane < vl; ++lane) {
      out[(i + lane) * 3 + 0] =
          static_cast<char>(encodeOneScalar(x_values[lane], reference.x, resolution));
      out[(i + lane) * 3 + 1] =
          static_cast<char>(encodeOneScalar(y_values[lane], reference.y, resolution));
      out[(i + lane) * 3 + 2] =
          static_cast<char>(encodeOneScalar(z_values[lane], reference.z, resolution));
    }
#endif
    i += vl;
  }
}

inline vfloat32m2_t
diffBytesToDecoded(vuint8mf2_t bytes, const float reference, const float resolution, const std::size_t vl)
{
  const vuint16m1_t u16 = __riscv_vzext_vf2_u16m1(bytes, vl);
  const vuint32m2_t u32 = __riscv_vwaddu_vx_u32m2(u16, 0, vl);
  vfloat32m2_t value = __riscv_vfcvt_f_xu_v_f32m2(u32, vl);
  value = __riscv_vfadd_vf_f32m2(value, 0.5f, vl);
  value = __riscv_vfmul_vf_f32m2(value, resolution, vl);
  return __riscv_vfadd_vf_f32m2(value, reference, vl);
}

inline void
decodePointsRVV(const std::vector<char>& encoded,
                const ReferencePoint& reference,
                const float resolution,
                std::vector<PointXYZ>& out)
{
  const std::size_t point_count = encoded.size() / 3;
  out.resize(point_count);
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(encoded.data());
  const ptrdiff_t diff_stride = 3;
  const ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(PointXYZ));

  std::size_t i = 0;
  while (i < point_count) {
    const std::size_t vl = __riscv_vsetvl_e8mf2(point_count - i);
    const vuint8mf2_t dx = __riscv_vlse8_v_u8mf2(bytes + i * 3 + 0, diff_stride, vl);
    const vuint8mf2_t dy = __riscv_vlse8_v_u8mf2(bytes + i * 3 + 1, diff_stride, vl);
    const vuint8mf2_t dz = __riscv_vlse8_v_u8mf2(bytes + i * 3 + 2, diff_stride, vl);

    __riscv_vsse32_v_f32m2(&out[i].x,
                           point_stride,
                           diffBytesToDecoded(dx, static_cast<float>(reference.x), resolution, vl),
                           vl);
    __riscv_vsse32_v_f32m2(&out[i].y,
                           point_stride,
                           diffBytesToDecoded(dy, static_cast<float>(reference.y), resolution, vl),
                           vl);
    __riscv_vsse32_v_f32m2(&out[i].z,
                           point_stride,
                           diffBytesToDecoded(dz, static_cast<float>(reference.z), resolution, vl),
                           vl);
    i += vl;
  }
}

inline void
decodePointsRVVToCloudBytes(const char* encoded,
                            const std::size_t point_count,
                            const ReferencePoint& reference,
                            const float resolution,
                            pcl::PointCloud<PointXYZ>& output,
                            const std::size_t begin_index)
{
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(encoded);
  const ptrdiff_t diff_stride = 3;
  const ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(PointXYZ));

  std::size_t i = 0;
  while (i < point_count) {
    const std::size_t vl = __riscv_vsetvl_e8mf2(point_count - i);
    const vuint8mf2_t dx = __riscv_vlse8_v_u8mf2(bytes + i * 3 + 0, diff_stride, vl);
    const vuint8mf2_t dy = __riscv_vlse8_v_u8mf2(bytes + i * 3 + 1, diff_stride, vl);
    const vuint8mf2_t dz = __riscv_vlse8_v_u8mf2(bytes + i * 3 + 2, diff_stride, vl);

    PointXYZ* out = &output[begin_index + i];
    __riscv_vsse32_v_f32m2(&out->x,
                           point_stride,
                           diffBytesToDecoded(dx, static_cast<float>(reference.x), resolution, vl),
                           vl);
    __riscv_vsse32_v_f32m2(&out->y,
                           point_stride,
                           diffBytesToDecoded(dy, static_cast<float>(reference.y), resolution, vl),
                           vl);
    __riscv_vsse32_v_f32m2(&out->z,
                           point_stride,
                           diffBytesToDecoded(dz, static_cast<float>(reference.z), resolution, vl),
                           vl);
    i += vl;
  }
}

inline void
decodePointsRVVToCloud(const std::vector<char>& encoded,
                       const ReferencePoint& reference,
                       const float resolution,
                       pcl::PointCloud<PointXYZ>& output,
                       const std::size_t begin_index)
{
  decodePointsRVVToCloudBytes(
      encoded.data(), encoded.size() / 3, reference, resolution, output, begin_index);
}

#endif // __RVV10__

inline void
encodePointsCandidate(const pcl::PointCloud<PointXYZ>& cloud,
                      const pcl::Indices& indices,
                      const ReferencePoint& reference,
                      const float resolution,
                      std::vector<char>& out)
{
#if defined(__RVV10__)
  encodePointsRVV(cloud, indices, reference, resolution, out);
#else
  encodePointsScalar(cloud, indices, reference, resolution, out);
#endif
}

inline void
decodePointsCandidate(const std::vector<char>& encoded,
                      const ReferencePoint& reference,
                      const float resolution,
                      std::vector<PointXYZ>& out)
{
#if defined(__RVV10__)
  decodePointsRVV(encoded, reference, resolution, out);
#else
  decodePointsScalar(encoded, reference, resolution, out);
#endif
}

inline void
decodePointsCandidateToCloud(const std::vector<char>& encoded,
                             const ReferencePoint& reference,
                             const float resolution,
                             pcl::PointCloud<PointXYZ>& output,
                             const std::size_t begin_index)
{
#if defined(__RVV10__)
  decodePointsRVVToCloud(encoded, reference, resolution, output, begin_index);
#else
  decodePointsScalarToCloud(encoded, reference, resolution, output, begin_index);
#endif
}

inline void
decodePointsCandidateToCloudBytes(const char* encoded,
                                  const std::size_t point_count,
                                  const ReferencePoint& reference,
                                  const float resolution,
                                  pcl::PointCloud<PointXYZ>& output,
                                  const std::size_t begin_index)
{
#if defined(__RVV10__)
  decodePointsRVVToCloudBytes(encoded, point_count, reference, resolution, output, begin_index);
#else
  decodePointsScalarToCloudBytes(encoded, point_count, reference, resolution, output, begin_index);
#endif
}

inline void
decodePointsCandidateMultiLeafToCloud(const MultiLeafDecodeCase& fixture,
                                      const float resolution,
                                      pcl::PointCloud<PointXYZ>& output)
{
  // RVV side（RVV 链路）仍是 test-only helper：按 production 解码的 leaf
  // 顺序推进 begin/end offset 和 diff byte offset，但不模拟 entropy 或 tree callback。
  std::size_t begin = fixture.begin_index;
  std::size_t byte_offset = 0;
  for (std::size_t leaf = 0; leaf < fixture.leaf_counts.size(); ++leaf) {
    const std::size_t count = fixture.leaf_counts[leaf];
    decodePointsCandidateToCloudBytes(fixture.encoded.data() + byte_offset,
                                      count,
                                      fixture.references[leaf],
                                      resolution,
                                      output,
                                      begin);
    begin += count;
    byte_offset += count * 3;
  }
}

} // namespace pcl::io::rvv_test::point_coding
