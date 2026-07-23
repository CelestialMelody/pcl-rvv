#pragma once

#include <pcl/common/point_tests.h>
#include <pcl/rvv_point_load.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_grid_minimum {

struct GridCell {
  unsigned int idx;
  unsigned int source_index;
  int ix;
  int iy;
  float z;
};

inline int
floorToInt(float value)
{
  return static_cast<int>(std::floor(value));
}

inline bool
sameGridCellForSort(const GridCell& lhs, const GridCell& rhs)
{
  return lhs.idx < rhs.idx;
}

inline void
computeBoundsStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                 const pcl::Indices& indices,
                 Eigen::Vector4f& min_p,
                 Eigen::Vector4f& max_p)
{
  min_p.setConstant(std::numeric_limits<float>::max());
  max_p.setConstant(std::numeric_limits<float>::lowest());
  min_p[3] = max_p[3] = 0.0f;
  for (const int index : indices) {
    const auto& point = cloud[static_cast<std::size_t>(index)];
    if (!cloud.is_dense && !pcl::isXYZFinite(point))
      continue;
    min_p[0] = std::min(min_p[0], point.x);
    min_p[1] = std::min(min_p[1], point.y);
    min_p[2] = std::min(min_p[2], point.z);
    max_p[0] = std::max(max_p[0], point.x);
    max_p[1] = std::max(max_p[1], point.y);
    max_p[2] = std::max(max_p[2], point.z);
  }
}

inline void
computeGridShape(const Eigen::Vector4f& min_p,
                 const Eigen::Vector4f& max_p,
                 float inverse_resolution,
                 int& min_b0,
                 int& min_b1,
                 int& div_x)
{
  min_b0 = floorToInt(min_p[0] * inverse_resolution);
  min_b1 = floorToInt(min_p[1] * inverse_resolution);
  const int max_b0 = floorToInt(max_p[0] * inverse_resolution);
  div_x = max_b0 - min_b0 + 1;
}

inline std::vector<GridCell>
computeGridCellsStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const pcl::Indices& indices,
                    float inverse_resolution,
                    int min_b0,
                    int min_b1,
                    int div_x)
{
  std::vector<GridCell> cells;
  cells.reserve(indices.size());
  for (const int index : indices) {
    const auto source = static_cast<unsigned int>(index);
    const auto& point = cloud[static_cast<std::size_t>(index)];
    if (!cloud.is_dense && !pcl::isXYZFinite(point))
      continue;
    const int ix = floorToInt(point.x * inverse_resolution) - min_b0;
    const int iy = floorToInt(point.y * inverse_resolution) - min_b1;
    const int idx = ix + iy * div_x;
    cells.push_back({static_cast<unsigned int>(idx), source, ix, iy, point.z});
  }
  return cells;
}

inline pcl::Indices
selectMinimumZIndices(std::vector<GridCell> cells)
{
  std::sort(cells.begin(), cells.end(), sameGridCellForSort);

  pcl::Indices out;
  out.reserve(cells.size());
  std::size_t first = 0;
  while (first < cells.size()) {
    std::size_t last = first + 1;
    while (last < cells.size() && cells[last].idx == cells[first].idx)
      ++last;

    std::size_t best = first;
    float min_z = cells[first].z;
    for (std::size_t i = first + 1; i < last; ++i) {
      if (cells[i].z < min_z) {
        min_z = cells[i].z;
        best = i;
      }
    }
    out.push_back(static_cast<int>(cells[best].source_index));
    first = last;
  }
  return out;
}

inline pcl::Indices
gridMinimumPointXYZStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::Indices& indices,
                       float inverse_resolution)
{
  Eigen::Vector4f min_p;
  Eigen::Vector4f max_p;
  computeBoundsStd(cloud, indices, min_p, max_p);
  int min_b0 = 0;
  int min_b1 = 0;
  int div_x = 0;
  computeGridShape(min_p, max_p, inverse_resolution, min_b0, min_b1, div_x);
  return selectMinimumZIndices(
      computeGridCellsStd(cloud, indices, inverse_resolution, min_b0, min_b1, div_x));
}

#if defined(__RVV10__)

inline vint32m2_t
floorF32ToI32NoFrm(vfloat32m2_t values, std::size_t vl)
{
  const vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2(values, vl);
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

inline bool
computeGridCellsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const pcl::Indices& indices,
                    float inverse_resolution,
                    int min_b0,
                    int min_b1,
                    int div_x,
                    std::vector<GridCell>& out)
{
  const std::size_t n = indices.size();
  if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
      cloud.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    return false;

  out.resize(n);
  auto* out_cells = out.data();
  std::size_t kept = 0;
  std::size_t i = 0;
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.data());

  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
    const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
    // Indices are explicit source point ids; convert them to AoS byte offsets
    // and gather x/y/z through the common point-load wrapper.
    const vuint32m2_t v_offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_indices_u, vl);

    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::indexed_load3_f32m2<pcl::PointXYZ,
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(base, v_offsets, vl, vx, vy, vz);

    const vfloat32m2_t sx = __riscv_vfmul_vf_f32m2(vx, inverse_resolution, vl);
    const vfloat32m2_t sy = __riscv_vfmul_vf_f32m2(vy, inverse_resolution, vl);
    const vint32m2_t ix = __riscv_vsub_vx_i32m2(floorF32ToI32NoFrm(sx, vl), min_b0, vl);
    const vint32m2_t iy = __riscv_vsub_vx_i32m2(floorF32ToI32NoFrm(sy, vl), min_b1, vl);
    const vint32m2_t idx_i32 = __riscv_vmacc_vx_i32m2(ix, div_x, iy, vl);
    const vuint32m2_t idx = __riscv_vreinterpret_v_i32m2_u32m2(idx_i32);

    // GridMinimum only skips invalid xyz on non-dense input.  The dense case
    // keeps the scalar contract and uses all lanes; non-dense combines the
    // three finite masks before compression.
    vbool16_t keep = __riscv_vmset_m_b16(vl);
    if (!cloud.is_dense) {
      keep = __riscv_vmand_mm_b16(__riscv_vmand_mm_b16(finiteMask(vx, vl), finiteMask(vy, vl), vl),
                                  finiteMask(vz, vl),
                                  vl);
    }

    // Compress preserves the relative order of kept lanes, so the later scalar
    // sort/min-z stage still sees the same source_index and z pairs.
    const vuint32m2_t idx_kept = __riscv_vcompress_vm_u32m2(idx, keep, vl);
    const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(v_indices_u, keep, vl);
    const vint32m2_t ix_kept = __riscv_vcompress_vm_i32m2(ix, keep, vl);
    const vint32m2_t iy_kept = __riscv_vcompress_vm_i32m2(iy, keep, vl);
    const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(vz, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

    // Staging is a scalar vector because the final result is one minimum-z
    // source index per grid cell, not one output per active lane.
    for (std::size_t lane = 0; lane < keep_count; ++lane) {
      out_cells[kept + lane].idx =
          __riscv_vmv_x_s_u32m2_u32(__riscv_vslidedown_vx_u32m2(idx_kept, lane, keep_count));
      out_cells[kept + lane].source_index =
          __riscv_vmv_x_s_u32m2_u32(__riscv_vslidedown_vx_u32m2(source_kept, lane, keep_count));
      out_cells[kept + lane].ix =
          __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(ix_kept, lane, keep_count));
      out_cells[kept + lane].iy =
          __riscv_vmv_x_s_i32m2_i32(__riscv_vslidedown_vx_i32m2(iy_kept, lane, keep_count));
      out_cells[kept + lane].z =
          __riscv_vfmv_f_s_f32m2_f32(__riscv_vslidedown_vx_f32m2(z_kept, lane, keep_count));
    }

    kept += keep_count;
    i += vl;
  }

  out.resize(kept);
  return true;
}

inline bool
gridMinimumPointXYZRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::Indices& indices,
                       float inverse_resolution,
                       pcl::Indices& out_indices)
{
  Eigen::Vector4f min_p;
  Eigen::Vector4f max_p;
  computeBoundsStd(cloud, indices, min_p, max_p);
  int min_b0 = 0;
  int min_b1 = 0;
  int div_x = 0;
  computeGridShape(min_p, max_p, inverse_resolution, min_b0, min_b1, div_x);

  std::vector<GridCell> cells;
  if (!computeGridCellsRVV(cloud, indices, inverse_resolution, min_b0, min_b1, div_x, cells))
    return false;
  out_indices = selectMinimumZIndices(std::move(cells));
  return true;
}

#endif

inline std::uint64_t
checksumGridCells(const std::vector<GridCell>& cells)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const auto& cell : cells) {
    sum = (sum ^ cell.idx) * 1099511628211ull;
    sum = (sum ^ cell.source_index) * 1099511628211ull;
    sum = (sum ^ static_cast<std::uint32_t>(cell.ix)) * 1099511628211ull;
    sum = (sum ^ static_cast<std::uint32_t>(cell.iy)) * 1099511628211ull;
    sum = (sum ^ static_cast<std::uint32_t>(std::lround((cell.z + 64.0f) * 100000.0f))) *
          1099511628211ull;
  }
  return sum ^ static_cast<std::uint64_t>(cells.size());
}

inline std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int index : indices)
    sum = (sum ^ static_cast<std::uint32_t>(index)) * 1099511628211ull;
  return sum ^ static_cast<std::uint64_t>(indices.size());
}

inline bool
operator==(const GridCell& lhs, const GridCell& rhs)
{
  return lhs.idx == rhs.idx && lhs.source_index == rhs.source_index && lhs.ix == rhs.ix &&
         lhs.iy == rhs.iy && lhs.z == rhs.z;
}

} // namespace pcl_rvv_filters_grid_minimum
