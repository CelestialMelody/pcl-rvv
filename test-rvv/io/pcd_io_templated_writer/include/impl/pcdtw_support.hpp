/*
 * 本文件做什么：
 * 这些 helper 复刻 `io/include/pcl/io/impl/pcd_io.hpp` 中
 * `PCDWriter::writeBinaryCompressed<PointT>` 的压缩前置布局转换。scalar
 * reference（标量参考链路）按生产源码的 point-major 到 field-major
 * 双重循环复制字段；candidate（候选链路）后续只在 `__RVV10__` 下替换为
 * RVV 实现。这里不调用 LZF，也不处理 mmap、文件锁或 header 生成。
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <pcl/io/lzf.h>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_test::pcd_io_templated_writer {

struct FieldLayout {
  std::size_t offset;
  std::size_t size;
};

enum class PathKind {
  None,
  PackScalarFallback,
  PackRvv,
  BinaryScalarFallback,
  BinaryRvv,
  BinaryTupleScalarFallback,
  BinaryTupleMemcpy,
  BinaryTupleSegmentRvv,
};

inline PathKind&
lastPathStorage()
{
  static PathKind path = PathKind::None;
  return path;
}

inline void
resetLastPath()
{
  lastPathStorage() = PathKind::None;
}

inline PathKind
lastPath()
{
  return lastPathStorage();
}

inline std::size_t
fieldMajorSize(const std::size_t point_count, const std::vector<FieldLayout>& fields)
{
  std::size_t total = 0;
  for (const auto& field : fields)
    total += field.size * point_count;
  return total;
}

inline std::vector<std::uint8_t>
makePointMajorCloud(const std::size_t point_count,
                    const std::size_t point_step,
                    const std::vector<FieldLayout>& fields)
{
  std::vector<std::uint8_t> data(point_count * point_step, 0xa5);
  for (std::size_t point = 0; point < point_count; ++point) {
    for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
      const auto& field = fields[field_index];
      for (std::size_t byte = 0; byte < field.size; ++byte) {
        data[point * point_step + field.offset + byte] =
            static_cast<std::uint8_t>((point * 29 + field_index * 61 + byte * 17) & 0xffu);
      }
    }
  }
  return data;
}

inline void
packFieldsScalar(const std::uint8_t* point_major,
                 const std::size_t point_count,
                 const std::size_t point_step,
                 const std::vector<FieldLayout>& fields,
                 std::vector<std::uint8_t>& field_major)
{
  field_major.resize(fieldMajorSize(point_count, fields));
  std::vector<std::uint8_t*> plane_ptrs(fields.size());
  std::size_t plane_offset = 0;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    plane_ptrs[i] = field_major.data() + plane_offset;
    plane_offset += fields[i].size * point_count;
  }

  for (std::size_t point = 0; point < point_count; ++point) {
    for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
      const auto& field = fields[field_index];
      std::memcpy(plane_ptrs[field_index],
                  point_major + point * point_step + field.offset,
                  field.size);
      plane_ptrs[field_index] += field.size;
    }
  }
}

inline bool
canUseRvvFourByteFields(const std::size_t point_count,
                        const std::size_t point_step,
                        const std::vector<FieldLayout>& fields)
{
  if (point_count == 0 || fields.empty() || point_step % sizeof(std::uint32_t) != 0)
    return false;

  for (const auto& field : fields) {
    if (field.size != sizeof(std::uint32_t) || field.offset % sizeof(std::uint32_t) != 0)
      return false;
  }
  return true;
}

inline std::size_t
packedBinarySize(const std::size_t point_count, const std::vector<FieldLayout>& fields)
{
  std::size_t point_size = 0;
  for (const auto& field : fields)
    point_size += field.size;
  return point_size * point_count;
}

inline void
packBinaryFieldsScalar(const std::uint8_t* point_major,
                       const std::size_t point_count,
                       const std::size_t point_step,
                       const std::vector<FieldLayout>& fields,
                       std::vector<std::uint8_t>& packed_binary)
{
  packed_binary.resize(packedBinarySize(point_count, fields));
  std::uint8_t* out = packed_binary.data();
  for (std::size_t point = 0; point < point_count; ++point) {
    for (const auto& field : fields) {
      std::memcpy(out, point_major + point * point_step + field.offset, field.size);
      out += field.size;
    }
  }
}

inline void
packBinaryFieldsCandidate(const std::uint8_t* point_major,
                          const std::size_t point_count,
                          const std::size_t point_step,
                          const std::vector<FieldLayout>& fields,
                          std::vector<std::uint8_t>& packed_binary)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  if (canUseRvvFourByteFields(point_count, point_step, fields)) {
    packed_binary.resize(packedBinarySize(point_count, fields));
    const auto source_stride_bytes = static_cast<std::ptrdiff_t>(point_step);
    const auto output_point_step = fields.size() * sizeof(std::uint32_t);
    const auto output_stride_bytes = static_cast<std::ptrdiff_t>(output_point_step);
    std::size_t output_field_offset = 0;
    for (const auto& field : fields) {
      const auto* src =
          reinterpret_cast<const std::uint32_t*>(point_major + field.offset);
      auto* dst =
          reinterpret_cast<std::uint32_t*>(packed_binary.data() + output_field_offset);
      for (std::size_t point = 0; point < point_count;) {
        const std::size_t vl = __riscv_vsetvl_e32m4(point_count - point);
        const vuint32m4_t values =
            __riscv_vlse32_v_u32m4(src + point * (point_step / 4),
                                   source_stride_bytes,
                                   vl);
        __riscv_vsse32_v_u32m4(dst + point * fields.size(),
                               output_stride_bytes,
                               values,
                               vl);
        point += vl;
      }
      output_field_offset += sizeof(std::uint32_t);
    }
    lastPathStorage() = PathKind::BinaryRvv;
    return;
  }
#endif
  packBinaryFieldsScalar(point_major, point_count, point_step, fields, packed_binary);
  lastPathStorage() = PathKind::BinaryScalarFallback;
}

inline bool
canUseBinaryTuple4Fields(const std::size_t point_count,
                         const std::size_t point_step,
                         const std::vector<FieldLayout>& fields)
{
  if (point_count == 0 || fields.size() != 4 || point_step % sizeof(std::uint32_t) != 0)
    return false;

  for (std::size_t i = 0; i < fields.size(); ++i) {
    if (fields[i].size != sizeof(std::uint32_t) ||
        fields[i].offset != i * sizeof(std::uint32_t))
      return false;
  }
  return point_step >= fields.size() * sizeof(std::uint32_t);
}

inline void
packBinaryFieldsTupleCandidate(const std::uint8_t* point_major,
                               const std::size_t point_count,
                               const std::size_t point_step,
                               const std::vector<FieldLayout>& fields,
                               std::vector<std::uint8_t>& packed_binary)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  if (canUseBinaryTuple4Fields(point_count, point_step, fields)) {
    constexpr std::size_t kPackedPointStep = 4 * sizeof(std::uint32_t);
    packed_binary.resize(point_count * kPackedPointStep);
    if (point_step == kPackedPointStep) {
      std::memcpy(packed_binary.data(), point_major, packed_binary.size());
      lastPathStorage() = PathKind::BinaryTupleMemcpy;
      return;
    }

    const auto stride_bytes = static_cast<std::ptrdiff_t>(point_step);
    const std::size_t stride_words = point_step / sizeof(std::uint32_t);
    const auto* src0 = reinterpret_cast<const std::uint32_t*>(point_major);
    const auto* src1 = reinterpret_cast<const std::uint32_t*>(point_major + sizeof(std::uint32_t));
    const auto* src2 = reinterpret_cast<const std::uint32_t*>(point_major + 2 * sizeof(std::uint32_t));
    const auto* src3 = reinterpret_cast<const std::uint32_t*>(point_major + 3 * sizeof(std::uint32_t));
    auto* dst = reinterpret_cast<std::uint32_t*>(packed_binary.data());

    for (std::size_t point = 0; point < point_count;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(point_count - point);
      const vuint32m2_t v0 =
          __riscv_vlse32_v_u32m2(src0 + point * stride_words, stride_bytes, vl);
      const vuint32m2_t v1 =
          __riscv_vlse32_v_u32m2(src1 + point * stride_words, stride_bytes, vl);
      const vuint32m2_t v2 =
          __riscv_vlse32_v_u32m2(src2 + point * stride_words, stride_bytes, vl);
      const vuint32m2_t v3 =
          __riscv_vlse32_v_u32m2(src3 + point * stride_words, stride_bytes, vl);
      vuint32m2x4_t tuple =
          __riscv_vset_v_u32m2_u32m2x4(__riscv_vundefined_u32m2x4(), 0, v0);
      tuple = __riscv_vset_v_u32m2_u32m2x4(tuple, 1, v1);
      tuple = __riscv_vset_v_u32m2_u32m2x4(tuple, 2, v2);
      tuple = __riscv_vset_v_u32m2_u32m2x4(tuple, 3, v3);
      __riscv_vsseg4e32_v_u32m2x4(dst + point * 4, tuple, vl);
      point += vl;
    }
    lastPathStorage() = PathKind::BinaryTupleSegmentRvv;
    return;
  }
#endif
  packBinaryFieldsScalar(point_major, point_count, point_step, fields, packed_binary);
  lastPathStorage() = PathKind::BinaryTupleScalarFallback;
}

inline void
packFieldsCandidate(const std::uint8_t* point_major,
                    const std::size_t point_count,
                    const std::size_t point_step,
                    const std::vector<FieldLayout>& fields,
                    std::vector<std::uint8_t>& field_major)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  if (canUseRvvFourByteFields(point_count, point_step, fields)) {
    field_major.resize(fieldMajorSize(point_count, fields));
    const auto stride_bytes = static_cast<std::ptrdiff_t>(point_step);
    std::size_t plane_offset = 0;
    for (const auto& field : fields) {
      const auto* src =
          reinterpret_cast<const std::uint32_t*>(point_major + field.offset);
      auto* dst = reinterpret_cast<std::uint32_t*>(field_major.data() + plane_offset);
      for (std::size_t point = 0; point < point_count;) {
        const std::size_t vl = __riscv_vsetvl_e32m4(point_count - point);
        const vuint32m4_t values =
            __riscv_vlse32_v_u32m4(src + point * (point_step / 4),
                                   stride_bytes,
                                   vl);
        __riscv_vse32_v_u32m4(dst + point, values, vl);
        point += vl;
      }
      plane_offset += field.size * point_count;
    }
    lastPathStorage() = PathKind::PackRvv;
    return;
  }
#endif
  packFieldsScalar(point_major, point_count, point_step, fields, field_major);
  lastPathStorage() = PathKind::PackScalarFallback;
}

inline bool
compressPackedBytes(const std::vector<std::uint8_t>& packed,
                    std::vector<std::uint8_t>& compressed_payload)
{
  const auto data_size = static_cast<unsigned int>(packed.size());
  compressed_payload.assign(static_cast<std::size_t>(data_size) * 3 / 2 + 8, 0);
  if (data_size == 0) {
    compressed_payload.resize(8);
    return true;
  }

  const unsigned int compressed_size =
      pcl::lzfCompress(packed.data(),
                       data_size,
                       compressed_payload.data() + 8,
                       static_cast<unsigned int>(compressed_payload.size() - 8));
  if (compressed_size == 0)
    return false;

  std::memcpy(compressed_payload.data(), &compressed_size, sizeof(compressed_size));
  std::memcpy(compressed_payload.data() + 4, &data_size, sizeof(data_size));
  compressed_payload.resize(static_cast<std::size_t>(compressed_size) + 8);
  return true;
}

inline bool
packAndCompressScalar(const std::uint8_t* point_major,
                      const std::size_t point_count,
                      const std::size_t point_step,
                      const std::vector<FieldLayout>& fields,
                      std::vector<std::uint8_t>& compressed_payload)
{
  std::vector<std::uint8_t> packed;
  packFieldsScalar(point_major, point_count, point_step, fields, packed);
  return compressPackedBytes(packed, compressed_payload);
}

inline bool
packAndCompressCandidate(const std::uint8_t* point_major,
                         const std::size_t point_count,
                         const std::size_t point_step,
                         const std::vector<FieldLayout>& fields,
                         std::vector<std::uint8_t>& compressed_payload)
{
  std::vector<std::uint8_t> packed;
  packFieldsCandidate(point_major, point_count, point_step, fields, packed);
  return compressPackedBytes(packed, compressed_payload);
}

} // namespace pcl::io::rvv_test::pcd_io_templated_writer
