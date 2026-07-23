#pragma once

#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_extract_indices {

inline pcl::Indices
complementSetDifferenceStd(std::size_t input_size, const pcl::Indices& selected)
{
  pcl::Indices full(input_size);
  std::iota(full.begin(), full.end(), static_cast<pcl::index_t>(0));

  pcl::Indices sorted = selected;
  std::sort(sorted.begin(), sorted.end());

  pcl::Indices complement;
  complement.reserve(input_size > selected.size() ? input_size - selected.size() : 0);
  std::set_difference(full.begin(),
                      full.end(),
                      sorted.begin(),
                      sorted.end(),
                      std::back_inserter(complement));
  return complement;
}

inline std::vector<std::uint8_t>
buildMembershipBitmap(std::size_t input_size, const pcl::Indices& selected)
{
  std::vector<std::uint8_t> bitmap(input_size, 0);
  for (const int index : selected) {
    if (index >= 0 && static_cast<std::size_t>(index) < input_size)
      bitmap[static_cast<std::size_t>(index)] = 1;
  }
  return bitmap;
}

inline pcl::Indices
complementBitmapStd(std::size_t input_size, const pcl::Indices& selected)
{
  const auto bitmap = buildMembershipBitmap(input_size, selected);
  pcl::Indices complement;
  complement.reserve(input_size > selected.size() ? input_size - selected.size() : 0);
  for (std::size_t i = 0; i < bitmap.size(); ++i) {
    if (bitmap[i] == 0)
      complement.push_back(static_cast<int>(i));
  }
  return complement;
}

inline void
setPointXYZFieldsStd(pcl::PointCloud<pcl::PointXYZ>& cloud,
                     const pcl::Indices& indices,
                     float user_value)
{
  for (const int index : indices) {
    const auto point_index = static_cast<std::size_t>(index);
    cloud[point_index].x = user_value;
    cloud[point_index].y = user_value;
    cloud[point_index].z = user_value;
  }
  if (!std::isfinite(user_value))
    cloud.is_dense = false;
}

inline pcl::PointCloud<pcl::PointXYZ>
keepOrganizedPointXYZStd(const pcl::PointCloud<pcl::PointXYZ>& input,
                         const pcl::Indices& selected,
                         bool negative,
                         float user_value)
{
  pcl::PointCloud<pcl::PointXYZ> output = input;
  const pcl::Indices write_indices =
      negative ? selected : complementSetDifferenceStd(input.size(), selected);
  setPointXYZFieldsStd(output, write_indices, user_value);
  return output;
}

#if defined(__RVV10__)

inline bool
complementBitmapScanRVV(const std::vector<std::uint8_t>& bitmap, pcl::Indices& complement)
{
  const std::size_t n = bitmap.size();
  if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    return false;

  complement.resize(n);
  auto* out = reinterpret_cast<std::uint32_t*>(complement.data());
  std::size_t kept = 0;
  std::size_t i = 0;

  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e8m1(n - i);
    const vuint8m1_t flags = __riscv_vle8_v_u8m1(bitmap.data() + i, vl);
    const vbool8_t is_absent = __riscv_vmseq_vx_u8m1_b8(flags, 0, vl);
    const vuint32m4_t local = __riscv_vid_v_u32m4(vl);
    const vuint32m4_t global = __riscv_vadd_vx_u32m4(local, static_cast<std::uint32_t>(i), vl);
    const vuint32m4_t compact = __riscv_vcompress_vm_u32m4(global, is_absent, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b8(is_absent, vl);

    // The bitmap scan replaces the scalar full_indices/set_difference tail:
    // absent bits become monotonically increasing indices, and vcompress keeps
    // that order before one contiguous write to the staging vector.
    __riscv_vse32_v_u32m4(out + kept, compact, keep_count);
    kept += keep_count;
    i += vl;
  }

  complement.resize(kept);
  return true;
}

inline bool
complementBitmapRVV(std::size_t input_size, const pcl::Indices& selected, pcl::Indices& complement)
{
  if (input_size < 64)
    return false;

  const auto bitmap = buildMembershipBitmap(input_size, selected);
  return complementBitmapScanRVV(bitmap, complement);
}

inline bool
scatterSetPointXYZFieldsRVV(pcl::PointCloud<pcl::PointXYZ>& cloud,
                            const pcl::Indices& indices,
                            float user_value)
{
  const std::size_t n = indices.size();
  if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
      cloud.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    return false;

  auto* base = reinterpret_cast<std::uint8_t*>(cloud.data());
  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
    const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_indices_u, vl);
    const vfloat32m2_t values = __riscv_vfmv_v_f_f32m2(user_value, vl);

    // keep_organized/filterDirectly writes sparse point positions in-place.
    // The common scatter-store wrapper preserves the PointXYZ AoS field layout
    // and keeps this diagnostic focused on the indexed write cost.
    pcl::rvv_store::scatter_store3_f32m2<offsetof(pcl::PointXYZ, x),
                                         offsetof(pcl::PointXYZ, y),
                                         offsetof(pcl::PointXYZ, z)>(
        base, offsets, vl, values, values, values);
    i += vl;
  }

  if (!std::isfinite(user_value))
    cloud.is_dense = false;
  return true;
}

inline bool
keepOrganizedPointXYZRVV(const pcl::PointCloud<pcl::PointXYZ>& input,
                         const pcl::Indices& selected,
                         bool negative,
                         float user_value,
                         pcl::PointCloud<pcl::PointXYZ>& output)
{
  if (input.size() < 64)
    return false;

  output = input;
  pcl::Indices write_indices;
  if (negative) {
    write_indices = selected;
  } else {
    if (!complementBitmapRVV(input.size(), selected, write_indices))
      return false;
  }

  if (!scatterSetPointXYZFieldsRVV(output, write_indices, user_value))
    return false;
  return true;
}

#endif

inline std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int index : indices)
    sum = (sum ^ static_cast<std::uint32_t>(index)) * 1099511628211ull;
  return sum ^ static_cast<std::uint64_t>(indices.size());
}

inline std::uint64_t
checksumCloudXYZ(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const auto& point : cloud) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &point.x, sizeof(bits));
    sum = (sum ^ bits) * 1099511628211ull;
    std::memcpy(&bits, &point.y, sizeof(bits));
    sum = (sum ^ bits) * 1099511628211ull;
    std::memcpy(&bits, &point.z, sizeof(bits));
    sum = (sum ^ bits) * 1099511628211ull;
  }
  return sum ^ static_cast<std::uint64_t>(cloud.size());
}

} // namespace pcl_rvv_filters_extract_indices
