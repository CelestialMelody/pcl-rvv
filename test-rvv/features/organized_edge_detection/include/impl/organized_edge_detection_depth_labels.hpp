#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::organized_edge_detection
{
namespace detail
{
struct Neighbor
{
  int dx;
  int dy;
  int offset;
};

inline const std::array<Neighbor, 8>&
neighborsForWidth(const std::size_t width)
{
  thread_local std::array<Neighbor, 8> neighbors{};
  thread_local std::size_t cached_width = 0;
  if (cached_width != width)
  {
    const int w = static_cast<int>(width);
    neighbors = {{
        {-1, 0, -1},
        {-1, -1, -w - 1},
        {0, -1, -w},
        {1, -1, -w + 1},
        {1, 0, 1},
        {1, 1, w + 1},
        {0, 1, w},
        {-1, 1, w - 1},
    }};
    cached_width = width;
  }
  return neighbors;
}

inline bool
finiteDepth(const float value)
{
  return std::isfinite(value);
}

inline std::uint32_t
computeDepthLabelAt(const float* z,
                    const std::size_t width,
                    const std::size_t height,
                    const std::size_t row,
                    const std::size_t col,
                    const float depth_discon_threshold,
                    const int max_search_neighbors,
                    const std::uint32_t detecting_edge_types)
{
  const auto& neighbors = neighborsForWidth(width);
  const std::size_t curr_idx = row * width + col;
  if (!finiteDepth(z[curr_idx]))
    return kInvalidLabel;

  const float curr_depth = std::abs(z[curr_idx]);
  std::array<float, 8> neighbor_dist{};
  bool found_invalid_neighbor = false;
  for (std::size_t d_idx = 0; d_idx < neighbors.size(); ++d_idx)
  {
    const auto neighbor_idx =
        static_cast<std::ptrdiff_t>(curr_idx) + static_cast<std::ptrdiff_t>(neighbors[d_idx].offset);
    if (!finiteDepth(z[static_cast<std::size_t>(neighbor_idx)]))
    {
      found_invalid_neighbor = true;
      break;
    }
    neighbor_dist[d_idx] = curr_depth - std::abs(z[static_cast<std::size_t>(neighbor_idx)]);
  }

  auto classify_discontinuity = [&](const float dist) {
    std::uint32_t label = kInvalidLabel;
    if (std::abs(dist) <= depth_discon_threshold * std::abs(curr_depth))
      return label;
    if (dist > 0.0f)
    {
      if ((detecting_edge_types & kOccluded) != 0U)
        label |= kOccluded;
    }
    else
    {
      if ((detecting_edge_types & kOccluding) != 0U)
        label |= kOccluding;
    }
    return label;
  };

  if (!found_invalid_neighbor)
  {
    const auto minmax = std::minmax_element(neighbor_dist.cbegin(), neighbor_dist.cend());
    const float min_dist = *minmax.first;
    const float max_dist = *minmax.second;
    const float dominant = std::abs(min_dist) > std::abs(max_dist) ? min_dist : max_dist;
    return classify_discontinuity(dominant);
  }

  int dx = 0;
  int dy = 0;
  int invalid_count = 0;
  for (const Neighbor& neighbor : neighbors)
  {
    const auto neighbor_idx =
        static_cast<std::ptrdiff_t>(curr_idx) + static_cast<std::ptrdiff_t>(neighbor.offset);
    if (!finiteDepth(z[static_cast<std::size_t>(neighbor_idx)]))
    {
      dx += neighbor.dx;
      dy += neighbor.dy;
      ++invalid_count;
    }
  }

  const float f_dx = static_cast<float>(dx) / static_cast<float>(invalid_count);
  const float f_dy = static_cast<float>(dy) / static_cast<float>(invalid_count);
  float corr_depth = std::numeric_limits<float>::quiet_NaN();
  for (int s_idx = 1; s_idx < max_search_neighbors; ++s_idx)
  {
    const int s_row = static_cast<int>(row) + static_cast<int>(std::floor(f_dy * static_cast<float>(s_idx)));
    const int s_col = static_cast<int>(col) + static_cast<int>(std::floor(f_dx * static_cast<float>(s_idx)));
    if (s_row < 0 || s_row >= static_cast<int>(height) || s_col < 0 || s_col >= static_cast<int>(width))
      break;

    const float candidate = z[static_cast<std::size_t>(s_row) * width + static_cast<std::size_t>(s_col)];
    if (finiteDepth(candidate))
    {
      corr_depth = std::abs(candidate);
      break;
    }
  }

  if (!std::isnan(corr_depth))
    return classify_discontinuity(curr_depth - corr_depth);

  if ((detecting_edge_types & kNanBoundary) != 0U)
    return kNanBoundary;
  return kInvalidLabel;
}

#if defined(__RVV10__)
inline vbool32_t
finiteF32Mask(const vfloat32m1_t value, const std::size_t vl)
{
  return __riscv_vmfle_vf_f32m1_b32(__riscv_vfabs_v_f32m1(value, vl),
                                    std::numeric_limits<float>::max(),
                                    vl);
}
#endif
} // namespace detail

inline void
computeDepthLabelsScalar(const float* z,
                         const std::size_t width,
                         const std::size_t height,
                         const float depth_discon_threshold,
                         const int max_search_neighbors,
                         const std::uint32_t detecting_edge_types,
                         std::uint32_t* labels)
{
  std::fill_n(labels, width * height, kInvalidLabel);
  if (width < 3 || height < 3)
    return;

  for (std::size_t row = 1; row + 1 < height; ++row)
  {
    for (std::size_t col = 1; col + 1 < width; ++col)
    {
      labels[row * width + col] = detail::computeDepthLabelAt(z,
                                                              width,
                                                              height,
                                                              row,
                                                              col,
                                                              depth_discon_threshold,
                                                              max_search_neighbors,
                                                              detecting_edge_types);
    }
  }
}

inline void
computeDepthLabelsRVV(const float* z,
                      const std::size_t width,
                      const std::size_t height,
                      const float depth_discon_threshold,
                      const int max_search_neighbors,
                      const std::uint32_t detecting_edge_types,
                      std::uint32_t* labels)
{
  std::fill_n(labels, width * height, kInvalidLabel);
  if (width < 3 || height < 3)
    return;

#if defined(__RVV10__)
  const auto& neighbors = detail::neighborsForWidth(width);
  for (std::size_t row = 1; row + 1 < height; ++row)
  {
    std::size_t col = 1;
    while (col + 1 < width)
    {
      const std::size_t vl = __riscv_vsetvl_e32m1(width - 1 - col);
      const std::size_t curr_idx = row * width + col;
      const vfloat32m1_t center = __riscv_vle32_v_f32m1(z + curr_idx, vl);
      const vfloat32m1_t center_abs = __riscv_vfabs_v_f32m1(center, vl);
      vbool32_t all_finite = detail::finiteF32Mask(center, vl);

      vfloat32m1_t min_dist = __riscv_vfmv_v_f_f32m1(std::numeric_limits<float>::max(), vl);
      vfloat32m1_t max_dist = __riscv_vfmv_v_f_f32m1(-std::numeric_limits<float>::max(), vl);
      for (const detail::Neighbor& neighbor : neighbors)
      {
        const vfloat32m1_t neighbor_z = __riscv_vle32_v_f32m1(
            z + static_cast<std::size_t>(static_cast<std::ptrdiff_t>(curr_idx) + neighbor.offset), vl);
        all_finite = __riscv_vmand_mm_b32(all_finite, detail::finiteF32Mask(neighbor_z, vl), vl);
        const vfloat32m1_t dist =
            __riscv_vfsub_vv_f32m1(center_abs, __riscv_vfabs_v_f32m1(neighbor_z, vl), vl);
        min_dist = __riscv_vfmin_vv_f32m1(min_dist, dist, vl);
        max_dist = __riscv_vfmax_vv_f32m1(max_dist, dist, vl);
      }

      const vbool32_t min_dominates = __riscv_vmfgt_vv_f32m1_b32(
          __riscv_vfabs_v_f32m1(min_dist, vl), __riscv_vfabs_v_f32m1(max_dist, vl), vl);
      const vfloat32m1_t dominant = __riscv_vmerge_vvm_f32m1(max_dist, min_dist, min_dominates, vl);
      const vfloat32m1_t threshold =
          __riscv_vfmul_vf_f32m1(center_abs, depth_discon_threshold, vl);
      const vbool32_t discontinuity = __riscv_vmand_mm_b32(
          all_finite,
          __riscv_vmfgt_vv_f32m1_b32(__riscv_vfabs_v_f32m1(dominant, vl), threshold, vl),
          vl);

      vuint32m1_t out = __riscv_vmv_v_x_u32m1(0, vl);
      if ((detecting_edge_types & kOccluded) != 0U)
      {
        const vbool32_t occluded = __riscv_vmand_mm_b32(
            discontinuity, __riscv_vmfgt_vf_f32m1_b32(dominant, 0.0f, vl), vl);
        out = __riscv_vmerge_vvm_u32m1(out, __riscv_vor_vx_u32m1(out, kOccluded, vl), occluded, vl);
      }
      if ((detecting_edge_types & kOccluding) != 0U)
      {
        const vbool32_t occluding = __riscv_vmand_mm_b32(
            discontinuity, __riscv_vmflt_vf_f32m1_b32(dominant, 0.0f, vl), vl);
        out = __riscv_vmerge_vvm_u32m1(out, __riscv_vor_vx_u32m1(out, kOccluding, vl), occluding, vl);
      }
      __riscv_vse32_v_u32m1(labels + curr_idx, out, vl);

      if (__riscv_vcpop_m_b32(all_finite, vl) != vl)
      {
        for (std::size_t lane = 0; lane < vl; ++lane)
        {
          const std::size_t lane_col = col + lane;
          labels[row * width + lane_col] = detail::computeDepthLabelAt(z,
                                                                       width,
                                                                       height,
                                                                       row,
                                                                       lane_col,
                                                                       depth_discon_threshold,
                                                                       max_search_neighbors,
                                                                       detecting_edge_types);
        }
      }
      col += vl;
    }
  }
#else
  computeDepthLabelsScalar(z,
                           width,
                           height,
                           depth_discon_threshold,
                           max_search_neighbors,
                           detecting_edge_types,
                           labels);
#endif
}

inline std::vector<std::vector<std::size_t>>
assignLabelIndices(const std::uint32_t* labels, const std::size_t count)
{
  std::vector<std::vector<std::size_t>> label_indices(kNumEdgeTypes);
  for (std::size_t idx = 0; idx < count; ++idx)
  {
    if (labels[idx] == kInvalidLabel)
      continue;
    for (int edge_type = 0; edge_type < kNumEdgeTypes; ++edge_type)
    {
      if (((labels[idx] >> edge_type) & 1U) != 0U)
        label_indices[static_cast<std::size_t>(edge_type)].push_back(idx);
    }
  }
  return label_indices;
}

inline double
checksumLabels(const std::uint32_t* labels, const std::size_t count)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < count; ++i)
    checksum += static_cast<double>((i % 104729U) + 1U) * static_cast<double>(labels[i] + 1U);
  return checksum;
}
} // namespace pcl::features::rvv_test::organized_edge_detection
