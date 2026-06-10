#pragma once

#include <pcl/common/point_tests.h>
#include <pcl/common/rvv_point_load.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_approximate_voxel_grid {

struct LeafHash {
  int ix;
  int iy;
  int iz;
  unsigned int hash;
  std::uint32_t source_index;
};

struct HistoryEntry {
  int ix{0};
  int iy{0};
  int iz{0};
  int count{0};
  float sx{0.0f};
  float sy{0.0f};
  float sz{0.0f};
};

inline int
floorToInt(float value)
{
  return static_cast<int>(std::floor(value));
}

inline unsigned int
hashLeaf(int ix, int iy, int iz, std::size_t history_size)
{
  return static_cast<unsigned int>((ix * 7171 + iy * 3079 + iz * 4231) &
                                   (static_cast<int>(history_size) - 1));
}

inline void
flushHistoryEntry(pcl::PointCloud<pcl::PointXYZ>& output, const HistoryEntry& entry)
{
  const float inv_count = 1.0f / static_cast<float>(entry.count);
  output.push_back(pcl::PointXYZ(entry.sx * inv_count, entry.sy * inv_count, entry.sz * inv_count));
}

inline void
accumulateApproximateVoxelGridPointXYZ(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                       const std::vector<LeafHash>& leaves,
                                       std::size_t history_size,
                                       pcl::PointCloud<pcl::PointXYZ>& output)
{
  std::vector<HistoryEntry> history(history_size);
  output.clear();
  output.reserve(cloud.size());

  for (const auto& leaf : leaves) {
    const auto& point = cloud[leaf.source_index];
    auto& entry = history[leaf.hash];
    if (entry.count && (leaf.ix != entry.ix || leaf.iy != entry.iy || leaf.iz != entry.iz)) {
      flushHistoryEntry(output, entry);
      entry = HistoryEntry{};
    }
    entry.ix = leaf.ix;
    entry.iy = leaf.iy;
    entry.iz = leaf.iz;
    ++entry.count;
    entry.sx += point.x;
    entry.sy += point.y;
    entry.sz += point.z;
  }

  for (const auto& entry : history) {
    if (entry.count)
      flushHistoryEntry(output, entry);
  }

  output.width = static_cast<std::uint32_t>(output.size());
  output.height = 1;
  output.is_dense = true;
}

template <typename PointT>
std::vector<LeafHash>
computeLeafHashesStd(const pcl::PointCloud<PointT>& cloud,
                     const Eigen::Array3f& inverse_leaf_size,
                     std::size_t history_size)
{
  std::vector<LeafHash> out;
  out.reserve(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const auto& point = cloud[i];
    if (!pcl::isXYZFinite(point))
      continue;
    const int ix = floorToInt(point.x * inverse_leaf_size[0]);
    const int iy = floorToInt(point.y * inverse_leaf_size[1]);
    const int iz = floorToInt(point.z * inverse_leaf_size[2]);
    out.push_back({ix, iy, iz, hashLeaf(ix, iy, iz, history_size), static_cast<std::uint32_t>(i)});
  }
  return out;
}

inline pcl::PointCloud<pcl::PointXYZ>
approximateVoxelGridPointXYZStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                const Eigen::Array3f& inverse_leaf_size,
                                std::size_t history_size)
{
  const auto leaves = computeLeafHashesStd(cloud, inverse_leaf_size, history_size);
  pcl::PointCloud<pcl::PointXYZ> output;
  accumulateApproximateVoxelGridPointXYZ(cloud, leaves, history_size, output);
  return output;
}

#if defined(__RVV10__)

template <typename T>
using CoordScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT, typename = void>
struct HasXYZFloatLayout : std::false_type {};

template <typename PointT>
struct HasXYZFloatLayout<
    PointT,
    std::void_t<decltype(std::declval<PointT>().x),
                decltype(std::declval<PointT>().y),
                decltype(std::declval<PointT>().z)>>
: std::bool_constant<std::is_standard_layout_v<PointT> &&
                     std::is_same_v<CoordScalar<decltype(std::declval<PointT>().x)>, float> &&
                     std::is_same_v<CoordScalar<decltype(std::declval<PointT>().y)>, float> &&
                     std::is_same_v<CoordScalar<decltype(std::declval<PointT>().z)>, float>> {};

template <typename PointT>
inline constexpr bool kHasXYZFloatLayout = HasXYZFloatLayout<PointT>::value;

inline vint32m2_t
floorF32ToI32NoFrm(vfloat32m2_t values, std::size_t vl)
{
  // vfcvt.rtz does not depend on or change FRM.  Correct floor() for negative
  // non-integers by subtracting one from the truncation result.
  vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2(values, vl);
  const vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2(trunc, vl);
  const vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16(values, trunc_f, vl);
  const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
  const vint32m2_t adjust = __riscv_vmerge_vxm_i32m2(zero, 1, negative_fraction, vl);
  return __riscv_vsub_vv_i32m2(trunc, adjust, vl);
}

inline vbool16_t
finiteMask(vfloat32m2_t values, std::size_t vl)
{
  const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16(values, values, vl);
  const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2(values, vl);
  const vfloat32m2_t inf_v = __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16(abs_v, inf_v, vl);
  return __riscv_vmand_mm_b16(eq_self, not_inf, vl);
}

template <typename PointT>
bool
computeLeafHashesRVV(const pcl::PointCloud<PointT>& cloud,
                     const Eigen::Array3f& inverse_leaf_size,
                     std::size_t history_size,
                     std::vector<LeafHash>& out)
{
  if constexpr (!kHasXYZFloatLayout<PointT>) {
    return false;
  } else {
    const std::size_t n = cloud.size();
    if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        history_size == 0 || (history_size & (history_size - 1)) != 0)
      return false;

    out.resize(n);
    auto* out_leaf = out.data();
    std::size_t kept = 0;
    std::size_t i = 0;
    const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.data());

    while (i < n) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const auto* chunk = base + i * sizeof(PointT);
      vfloat32m2_t vx;
      vfloat32m2_t vy;
      vfloat32m2_t vz;
      pcl::rvv_load::strided_load3_f32m2<sizeof(PointT),
                                         offsetof(PointT, x),
                                         offsetof(PointT, y),
                                         offsetof(PointT, z)>(chunk, vl, vx, vy, vz);

      // This diagnostic isolates the ApproximateVoxelGrid pre-hash stage:
      // finite xyz, floor(xyz * inverse_leaf_size) and hash bucket.  History
      // collisions, flush and field accumulation remain outside this helper.
      const vbool16_t finite =
          __riscv_vmand_mm_b16(__riscv_vmand_mm_b16(finiteMask(vx, vl), finiteMask(vy, vl), vl),
                               finiteMask(vz, vl),
                               vl);
      const vfloat32m2_t sx = __riscv_vfmul_vf_f32m2(vx, inverse_leaf_size[0], vl);
      const vfloat32m2_t sy = __riscv_vfmul_vf_f32m2(vy, inverse_leaf_size[1], vl);
      const vfloat32m2_t sz = __riscv_vfmul_vf_f32m2(vz, inverse_leaf_size[2], vl);
      const vint32m2_t ix = floorF32ToI32NoFrm(sx, vl);
      const vint32m2_t iy = floorF32ToI32NoFrm(sy, vl);
      const vint32m2_t iz = floorF32ToI32NoFrm(sz, vl);

      vint32m2_t hash = __riscv_vmul_vx_i32m2(ix, 7171, vl);
      hash = __riscv_vmacc_vx_i32m2(hash, 3079, iy, vl);
      hash = __riscv_vmacc_vx_i32m2(hash, 4231, iz, vl);
      const vuint32m2_t hash_u = __riscv_vreinterpret_v_i32m2_u32m2(hash);
      const vuint32m2_t masked_hash =
          __riscv_vand_vx_u32m2(hash_u, static_cast<std::uint32_t>(history_size - 1), vl);

      const vuint32m2_t local = __riscv_vid_v_u32m2(vl);
      const vuint32m2_t source = __riscv_vadd_vx_u32m2(local, static_cast<std::uint32_t>(i), vl);

      const vint32m2_t ix_kept = __riscv_vcompress_vm_i32m2(ix, finite, vl);
      const vint32m2_t iy_kept = __riscv_vcompress_vm_i32m2(iy, finite, vl);
      const vint32m2_t iz_kept = __riscv_vcompress_vm_i32m2(iz, finite, vl);
      const vuint32m2_t hash_kept = __riscv_vcompress_vm_u32m2(masked_hash, finite, vl);
      const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(source, finite, vl);
      const std::size_t keep_count = __riscv_vcpop_m_b16(finite, vl);

      for (std::size_t lane = 0; lane < keep_count; ++lane) {
        out_leaf[kept + lane].ix = __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(ix_kept, lane, keep_count));
        out_leaf[kept + lane].iy = __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(iy_kept, lane, keep_count));
        out_leaf[kept + lane].iz = __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(iz_kept, lane, keep_count));
        out_leaf[kept + lane].hash =
            __riscv_vmv_x_s_u32m2_u32(__riscv_vslidedown_vx_u32m2(hash_kept, lane, keep_count));
        out_leaf[kept + lane].source_index =
            __riscv_vmv_x_s_u32m2_u32(__riscv_vslidedown_vx_u32m2(source_kept, lane, keep_count));
      }

      kept += keep_count;
      i += vl;
    }

    out.resize(kept);
    return true;
  }
}

#endif

#if defined(__RVV10__)

inline bool
approximateVoxelGridPointXYZRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                const Eigen::Array3f& inverse_leaf_size,
                                std::size_t history_size,
                                pcl::PointCloud<pcl::PointXYZ>& output)
{
  std::vector<LeafHash> leaves;
  if (!computeLeafHashesRVV(cloud, inverse_leaf_size, history_size, leaves))
    return false;
  // Full diagnostic path: only leaf/hash generation is RVV.  Bucket collision,
  // flush order and centroid accumulation intentionally stay scalar so this
  // measures the same downstream work as ApproximateVoxelGrid::applyFilter.
  accumulateApproximateVoxelGridPointXYZ(cloud, leaves, history_size, output);
  return true;
}

#endif

inline std::uint64_t
checksumLeafHashes(const std::vector<LeafHash>& leaves)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const auto& leaf : leaves) {
    sum = (sum ^ static_cast<std::uint32_t>(leaf.ix)) * 1099511628211ull;
    sum = (sum ^ static_cast<std::uint32_t>(leaf.iy)) * 1099511628211ull;
    sum = (sum ^ static_cast<std::uint32_t>(leaf.iz)) * 1099511628211ull;
    sum = (sum ^ leaf.hash) * 1099511628211ull;
    sum = (sum ^ leaf.source_index) * 1099511628211ull;
  }
  return sum ^ static_cast<std::uint64_t>(leaves.size());
}

inline std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const auto& point : cloud) {
    const auto xi = static_cast<std::uint32_t>(std::lround((point.x + 32.0f) * 100000.0f));
    const auto yi = static_cast<std::uint32_t>(std::lround((point.y + 32.0f) * 100000.0f));
    const auto zi = static_cast<std::uint32_t>(std::lround((point.z + 32.0f) * 100000.0f));
    sum = (sum ^ xi) * 1099511628211ull;
    sum = (sum ^ yi) * 1099511628211ull;
    sum = (sum ^ zi) * 1099511628211ull;
  }
  return sum ^ static_cast<std::uint64_t>(cloud.size());
}

inline bool
operator==(const LeafHash& lhs, const LeafHash& rhs)
{
  return lhs.ix == rhs.ix && lhs.iy == rhs.iy && lhs.iz == rhs.iz &&
         lhs.hash == rhs.hash && lhs.source_index == rhs.source_index;
}

} // namespace pcl_rvv_filters_approximate_voxel_grid
