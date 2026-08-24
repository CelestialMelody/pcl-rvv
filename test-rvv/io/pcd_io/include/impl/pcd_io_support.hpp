/*
 * 本文件做什么：
 * 这些 helper 复刻 `io/src/pcd_io.cpp` 的 binary_compressed 字段布局转换。
 * scalar reference（标量参考链路）按生产源码的双重循环复制字段；candidate
 * （候选链路）在本阶段先保持同语义入口，后续只在 `__RVV10__` 下替换为 RVV
 * 实现。这里不调用 LZF，也不处理 mmap / stream I/O。
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <pcl/io/lzf.h>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_test::pcd_io {

enum class FiniteKind {
  None,
  Float32,
  Float64,
};

struct FieldLayout {
  std::size_t offset;
  std::size_t size;
  FiniteKind finite_kind = FiniteKind::None;
};

enum class PathKind {
  None,
  PackScalarFallback,
  PackRvv,
  UnpackScalarFallback,
  UnpackRvv,
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
packedSize(const std::size_t point_count, const std::vector<FieldLayout>& fields)
{
  std::size_t total = 0;
  for (const auto& field : fields)
    total += field.size * point_count;
  return total;
}

inline std::vector<std::uint8_t>
makeInterleavedCloud(const std::size_t point_count,
                     const std::size_t point_step,
                     const std::vector<FieldLayout>& fields)
{
  std::vector<std::uint8_t> data(point_count * point_step, 0x5a);
  for (std::size_t point = 0; point < point_count; ++point) {
    for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
      const auto& field = fields[field_index];
      for (std::size_t byte = 0; byte < field.size; ++byte) {
        data[point * point_step + field.offset + byte] =
            static_cast<std::uint8_t>((point * 37 + field_index * 53 + byte * 17) & 0xff);
      }
    }
  }
  return data;
}

inline std::vector<std::uint8_t>
makeInterleavedFloatCloud(const std::size_t point_count,
                          const std::size_t point_step,
                          const std::vector<FieldLayout>& fields)
{
  auto data = makeInterleavedCloud(point_count, point_step, fields);
  for (std::size_t point = 0; point < point_count; ++point) {
    for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
      const auto& field = fields[field_index];
      if (field.finite_kind == FiniteKind::Float32) {
        const float value =
            static_cast<float>(point % 1024) * 0.25f + static_cast<float>(field_index);
        std::memcpy(data.data() + point * point_step + field.offset, &value, sizeof(value));
      }
      else if (field.finite_kind == FiniteKind::Float64) {
        const double value =
            static_cast<double>(point % 1024) * 0.25 + static_cast<double>(field_index);
        std::memcpy(data.data() + point * point_step + field.offset, &value, sizeof(value));
      }
    }
  }
  return data;
}

inline void
writeFloat32(std::vector<std::uint8_t>& cloud,
             const std::size_t point_step,
             const std::size_t field_offset,
             const std::size_t point_index,
             const float value)
{
  std::memcpy(cloud.data() + point_index * point_step + field_offset, &value, sizeof(value));
}

inline void
packFieldsScalar(const std::uint8_t* interleaved,
                 const std::size_t point_count,
                 const std::size_t point_step,
                 const std::vector<FieldLayout>& fields,
                 std::vector<std::uint8_t>& packed)
{
  packed.resize(packedSize(point_count, fields));
  std::size_t packed_offset = 0;
  for (const auto& field : fields) {
    auto* dst = packed.data() + packed_offset;
    for (std::size_t point = 0; point < point_count; ++point) {
      std::memcpy(dst,
                  interleaved + point * point_step + field.offset,
                  field.size);
      dst += field.size;
    }
    packed_offset += field.size * point_count;
  }
}

inline void
unpackFieldsScalar(const std::uint8_t* packed,
                   const std::size_t point_count,
                   const std::size_t point_step,
                   const std::vector<FieldLayout>& fields,
                   std::vector<std::uint8_t>& interleaved)
{
  std::size_t packed_offset = 0;
  for (const auto& field : fields) {
    const auto* src = packed + packed_offset;
    for (std::size_t point = 0; point < point_count; ++point) {
      std::memcpy(interleaved.data() + point * point_step + field.offset,
                  src,
                  field.size);
      src += field.size;
    }
    packed_offset += field.size * point_count;
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

inline void
packFieldsCandidate(const std::uint8_t* interleaved,
                    const std::size_t point_count,
                    const std::size_t point_step,
                    const std::vector<FieldLayout>& fields,
                    std::vector<std::uint8_t>& packed)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  if (canUseRvvFourByteFields(point_count, point_step, fields)) {
    packed.resize(packedSize(point_count, fields));
    const auto stride_bytes = static_cast<std::ptrdiff_t>(point_step);
    std::size_t packed_offset = 0;
    for (const auto& field : fields) {
      auto* dst =
          reinterpret_cast<std::uint32_t*>(packed.data() + packed_offset);
      const auto* src = reinterpret_cast<const std::uint32_t*>(interleaved + field.offset);
      for (std::size_t point = 0; point < point_count;) {
        const std::size_t vl = __riscv_vsetvl_e32m4(point_count - point);
        const vuint32m4_t values = __riscv_vlse32_v_u32m4(src + point * (point_step / 4),
                                                          stride_bytes,
                                                          vl);
        __riscv_vse32_v_u32m4(dst + point, values, vl);
        point += vl;
      }
      packed_offset += field.size * point_count;
    }
    lastPathStorage() = PathKind::PackRvv;
    return;
  }
#endif
  packFieldsScalar(interleaved, point_count, point_step, fields, packed);
  lastPathStorage() = PathKind::PackScalarFallback;
}

inline void
unpackFieldsCandidate(const std::uint8_t* packed,
                      const std::size_t point_count,
                      const std::size_t point_step,
                      const std::vector<FieldLayout>& fields,
                      std::vector<std::uint8_t>& interleaved)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  if (canUseRvvFourByteFields(point_count, point_step, fields)) {
    const auto stride_bytes = static_cast<std::ptrdiff_t>(point_step);
    std::size_t packed_offset = 0;
    for (const auto& field : fields) {
      const auto* src =
          reinterpret_cast<const std::uint32_t*>(packed + packed_offset);
      auto* dst = reinterpret_cast<std::uint32_t*>(interleaved.data() + field.offset);
      for (std::size_t point = 0; point < point_count;) {
        const std::size_t vl = __riscv_vsetvl_e32m4(point_count - point);
        const vuint32m4_t values = __riscv_vle32_v_u32m4(src + point, vl);
        __riscv_vsse32_v_u32m4(dst + point * (point_step / 4),
                               stride_bytes,
                               values,
                               vl);
        point += vl;
      }
      packed_offset += field.size * point_count;
    }
    lastPathStorage() = PathKind::UnpackRvv;
    return;
  }
#endif
  unpackFieldsScalar(packed, point_count, point_step, fields, interleaved);
  lastPathStorage() = PathKind::UnpackScalarFallback;
}

inline bool
compressPackedPayload(const std::vector<std::uint8_t>& packed,
                      std::vector<std::uint8_t>& payload)
{
  if (packed.size() * 3 / 2 > std::numeric_limits<std::uint32_t>::max())
    return false;

  payload.assign(8, 0);
  if (packed.empty())
    return true;

  std::vector<std::uint8_t> temp(packed.size() * 3 / 2 + 8);
  const auto uncompressed_size = static_cast<std::uint32_t>(packed.size());
  const auto compressed_size = pcl::lzfCompress(packed.data(),
                                                uncompressed_size,
                                                temp.data() + 8,
                                                static_cast<unsigned int>(temp.size() - 8));
  if (compressed_size == 0)
    return false;

  std::memcpy(temp.data(), &compressed_size, sizeof(compressed_size));
  std::memcpy(temp.data() + 4, &uncompressed_size, sizeof(uncompressed_size));
  temp.resize(static_cast<std::size_t>(compressed_size) + 8);
  payload = std::move(temp);
  return true;
}

inline bool
makeCompressedWriterPayloadScalar(const std::uint8_t* interleaved,
                                  const std::size_t point_count,
                                  const std::size_t point_step,
                                  const std::vector<FieldLayout>& fields,
                                  std::vector<std::uint8_t>& payload)
{
  std::vector<std::uint8_t> packed;
  packFieldsScalar(interleaved, point_count, point_step, fields, packed);
  return compressPackedPayload(packed, payload);
}

inline bool
makeCompressedWriterPayloadCandidate(const std::uint8_t* interleaved,
                                     const std::size_t point_count,
                                     const std::size_t point_step,
                                     const std::vector<FieldLayout>& fields,
                                     std::vector<std::uint8_t>& payload)
{
  std::vector<std::uint8_t> packed;
  packFieldsCandidate(interleaved, point_count, point_step, fields, packed);
  return compressPackedPayload(packed, payload);
}

template <typename UnpackFn>
inline bool
readCompressedBodyImpl(const std::uint8_t* payload,
                       const std::size_t payload_size,
                       const std::size_t point_count,
                       const std::size_t point_step,
                       const std::vector<FieldLayout>& fields,
                       std::vector<std::uint8_t>& cloud_data,
                       bool& is_dense,
                       UnpackFn&& unpack)
{
  is_dense = true;
  if (payload_size < 8)
    return false;

  std::uint32_t compressed_size = 0;
  std::uint32_t uncompressed_size = 0;
  std::memcpy(&compressed_size, payload + 0, sizeof(compressed_size));
  std::memcpy(&uncompressed_size, payload + 4, sizeof(uncompressed_size));
  if (payload_size < static_cast<std::size_t>(compressed_size) + 8)
    return false;

  const auto expected_packed_size = packedSize(point_count, fields);
  if (uncompressed_size != expected_packed_size)
    return false;

  std::vector<std::uint8_t> packed(expected_packed_size);
  if (uncompressed_size != 0) {
    const unsigned int tmp_size = pcl::lzfDecompress(payload + 8,
                                                     compressed_size,
                                                     packed.data(),
                                                     uncompressed_size);
    if (tmp_size != uncompressed_size)
      return false;
  }

  cloud_data.assign(point_count * point_step, 0xcc);
  unpack(packed.data(), point_count, point_step, fields, cloud_data);

  for (std::size_t point = 0; point < point_count; ++point) {
    const auto* point_data = cloud_data.data() + point * point_step;
    for (const auto& field : fields) {
      switch (field.finite_kind) {
        case FiniteKind::Float32: {
          float value = 0.0f;
          std::memcpy(&value, point_data + field.offset, sizeof(value));
          if (!std::isfinite(value))
            is_dense = false;
          break;
        }
        case FiniteKind::Float64: {
          double value = 0.0;
          std::memcpy(&value, point_data + field.offset, sizeof(value));
          if (!std::isfinite(value))
            is_dense = false;
          break;
        }
        case FiniteKind::None:
          break;
      }
    }
  }
  return true;
}

inline bool
readCompressedBodyScalar(const std::uint8_t* payload,
                         const std::size_t payload_size,
                         const std::size_t point_count,
                         const std::size_t point_step,
                         const std::vector<FieldLayout>& fields,
                         std::vector<std::uint8_t>& cloud_data,
                         bool& is_dense)
{
  return readCompressedBodyImpl(payload,
                                payload_size,
                                point_count,
                                point_step,
                                fields,
                                cloud_data,
                                is_dense,
                                unpackFieldsScalar);
}

inline bool
readCompressedBodyCandidate(const std::uint8_t* payload,
                            const std::size_t payload_size,
                            const std::size_t point_count,
                            const std::size_t point_step,
                            const std::vector<FieldLayout>& fields,
                            std::vector<std::uint8_t>& cloud_data,
                            bool& is_dense)
{
  return readCompressedBodyImpl(payload,
                                payload_size,
                                point_count,
                                point_step,
                                fields,
                                cloud_data,
                                is_dense,
                                unpackFieldsCandidate);
}

} // namespace pcl::io::rvv_test::pcd_io
