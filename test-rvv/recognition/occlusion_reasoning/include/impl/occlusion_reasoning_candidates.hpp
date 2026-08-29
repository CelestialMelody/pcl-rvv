#pragma once

/*
 * 本文件做什么：
 * 这里放 ZBuffering::filter() 首阶段的 scalar reference（标量参考链路）
 * 和 RVV candidate（RVV 候选链路）。两条链路都使用测试专用 raw depth
 * buffer，复刻 production filter 读取 `depth_[u * cy_ + v]` 后决定是否保留
 * point index 的语义。
 *
 * 证据边界：
 * 这些 helper 是 production-shaped diagnostic（生产形态诊断），不是
 * production direct（真实生产路径证据）。它们不覆盖 computeDepthMap() 的
 * depth buffer 构建，也不覆盖 copyPointCloud 后续成本。
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::test::occlusion_reasoning_rvv
{

enum class ExecutionPath
{
  ScalarFallback,
  RvvProjectionFilter
};

struct ProjectionPoint
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 1.0f;
};

inline bool
projectPoint(const ProjectionPoint& point,
             const float focal,
             const int depth_width,
             const int depth_height,
             int& u,
             int& v)
{
  if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) ||
      point.z == 0.0f)
    return false;

  const float cx = static_cast<float>(depth_width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(depth_height) / 2.0f - 0.5f;
  const float uf = focal * point.x / point.z + cx;
  const float vf = focal * point.y / point.z + cy;
  if (!std::isfinite(uf) || !std::isfinite(vf))
    return false;

  u = static_cast<int>(uf);
  v = static_cast<int>(vf);
  return !(u >= depth_width || v >= depth_height || u < 0 || v < 0);
}

inline void
filterIndicesScalarReference(const ProjectionPoint* points,
                             const std::size_t point_count,
                             const float* depth,
                             const int depth_width,
                             const int depth_height,
                             const float focal,
                             const float threshold,
                             std::vector<int>& indices_to_keep)
{
  indices_to_keep.clear();
  indices_to_keep.reserve(point_count);
  for (std::size_t point_index = 0; point_index < point_count; ++point_index)
  {
    int u = 0;
    int v = 0;
    if (!projectPoint(points[point_index], focal, depth_width, depth_height, u, v))
      continue;

    const float depth_at_pixel = depth[static_cast<std::size_t>(u) * depth_height +
                                       static_cast<std::size_t>(v)];
    if ((points[point_index].z - threshold) > depth_at_pixel || !std::isfinite(depth_at_pixel))
      continue;

    indices_to_keep.push_back(static_cast<int>(point_index));
  }
}

inline ExecutionPath
filterIndicesCandidate(const ProjectionPoint* points,
                       const std::size_t point_count,
                       const float* depth,
                       const int depth_width,
                       const int depth_height,
                       const float focal,
                       const float threshold,
                       std::vector<int>& indices_to_keep)
{
#if defined(__RVV10__)
  if (points == nullptr || depth == nullptr || point_count == 0 || depth_width <= 0 ||
      depth_height <= 0)
  {
    filterIndicesScalarReference(
        points, point_count, depth, depth_width, depth_height, focal, threshold, indices_to_keep);
    return ExecutionPath::ScalarFallback;
  }

  indices_to_keep.clear();
  indices_to_keep.reserve(point_count);

  const float cx = static_cast<float>(depth_width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(depth_height) / 2.0f - 0.5f;
  const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<std::uint32_t> index_values(max_vl);
  std::vector<int> u_values(max_vl);
  std::vector<int> v_values(max_vl);
  std::vector<float> z_values(max_vl);

  for (std::size_t point_index = 0; point_index < point_count;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(point_count - point_index);
    const auto xyz = __riscv_vlseg3e32_v_f32m2x3(
        reinterpret_cast<const float*>(points + point_index), vl);
    const vfloat32m2_t x = __riscv_vget_v_f32m2x3_f32m2(xyz, 0);
    const vfloat32m2_t y = __riscv_vget_v_f32m2x3_f32m2(xyz, 1);
    const vfloat32m2_t z = __riscv_vget_v_f32m2x3_f32m2(xyz, 2);

    const vfloat32m2_t inf_f =
        __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
    vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(x, x, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmflt_vv_f32m2_b16(__riscv_vfabs_v_f32m2(x, vl), inf_f, vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(y, y, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmflt_vv_f32m2_b16(__riscv_vfabs_v_f32m2(y, vl), inf_f, vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmfeq_vv_f32m2_b16(z, z, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmflt_vv_f32m2_b16(__riscv_vfabs_v_f32m2(z, vl), inf_f, vl), vl);
    finite = __riscv_vmand_mm_b16(finite, __riscv_vmfne_vf_f32m2_b16(z, 0.0f, vl), vl);

    const vfloat32m2_t one_f = __riscv_vfmv_v_f_f32m2(1.0f, vl);
    const vfloat32m2_t zero_f = __riscv_vfmv_v_f_f32m2(0.0f, vl);
    const vfloat32m2_t safe_x = __riscv_vmerge_vvm_f32m2(zero_f, x, finite, vl);
    const vfloat32m2_t safe_y = __riscv_vmerge_vvm_f32m2(zero_f, y, finite, vl);
    const vfloat32m2_t safe_z = __riscv_vmerge_vvm_f32m2(one_f, z, finite, vl);
    const vfloat32m2_t projected_u = __riscv_vfadd_vf_f32m2(
        __riscv_vfmul_vf_f32m2(__riscv_vfdiv_vv_f32m2(safe_x, safe_z, vl), focal, vl), cx, vl);
    const vfloat32m2_t projected_v = __riscv_vfadd_vf_f32m2(
        __riscv_vfmul_vf_f32m2(__riscv_vfdiv_vv_f32m2(safe_y, safe_z, vl), focal, vl), cy, vl);
    const vint32m2_t u = __riscv_vfcvt_rtz_x_f_v_i32m2(projected_u, vl);
    const vint32m2_t v = __riscv_vfcvt_rtz_x_f_v_i32m2(projected_v, vl);

    vbool16_t keep = finite;
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmsge_vx_i32m2_b16(u, 0, vl), vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmslt_vx_i32m2_b16(u, depth_width, vl), vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmsge_vx_i32m2_b16(v, 0, vl), vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmslt_vx_i32m2_b16(v, depth_height, vl), vl);

    const std::size_t active = __riscv_vcpop_m_b16(keep, vl);
    if (active == 0)
    {
      point_index += vl;
      continue;
    }

    const vuint32m2_t source_index =
        __riscv_vadd_vx_u32m2(__riscv_vid_v_u32m2(vl), static_cast<std::uint32_t>(point_index), vl);
    const vuint32m2_t kept_index = __riscv_vcompress_vm_u32m2(source_index, keep, vl);
    const vint32m2_t kept_u = __riscv_vcompress_vm_i32m2(u, keep, vl);
    const vint32m2_t kept_v = __riscv_vcompress_vm_i32m2(v, keep, vl);
    const vfloat32m2_t kept_z = __riscv_vcompress_vm_f32m2(z, keep, vl);
    __riscv_vse32_v_u32m2(index_values.data(), kept_index, active);
    __riscv_vse32_v_i32m2(u_values.data(), kept_u, active);
    __riscv_vse32_v_i32m2(v_values.data(), kept_v, active);
    __riscv_vse32_v_f32m2(z_values.data(), kept_z, active);

    for (std::size_t lane = 0; lane < active; ++lane)
    {
      const int u_lane = u_values[lane];
      const int v_lane = v_values[lane];
      const float depth_at_pixel = depth[static_cast<std::size_t>(u_lane) * depth_height +
                                         static_cast<std::size_t>(v_lane)];
      if ((z_values[lane] - threshold) > depth_at_pixel || !std::isfinite(depth_at_pixel))
        continue;

      indices_to_keep.push_back(static_cast<int>(index_values[lane]));
    }

    point_index += vl;
  }

  return ExecutionPath::RvvProjectionFilter;
#else
  filterIndicesScalarReference(
      points, point_count, depth, depth_width, depth_height, focal, threshold, indices_to_keep);
  return ExecutionPath::ScalarFallback;
#endif
}

inline std::uint64_t
checksumIndices(const std::vector<int>& indices)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto index : indices)
  {
    result ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(index));
    result *= 1099511628211ull;
  }
  return result;
}

} // namespace pcl::test::occlusion_reasoning_rvv
