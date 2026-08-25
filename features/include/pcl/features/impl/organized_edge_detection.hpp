/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2012-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef PCL_FEATURES_IMPL_ORGANIZED_EDGE_DETECTION_H_
#define PCL_FEATURES_IMPL_ORGANIZED_EDGE_DETECTION_H_

#include <pcl/2d/edge.h>
#include <pcl/features/organized_edge_detection.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PCL_ORGANIZED_EDGE_RVV_NOINLINE __attribute__((noinline))
#else
#define PCL_ORGANIZED_EDGE_RVV_NOINLINE
#endif

namespace pcl
{
namespace detail
{
  struct OrganizedEdgeNeighbor
  {
    int d_x;
    int d_y;
    int d_index;
  };

  inline std::array<OrganizedEdgeNeighbor, 8>
  organizedEdgeNeighbors (const std::uint32_t width)
  {
    const int w = static_cast<int> (width);
    return {{{-1, 0, -1},
             {-1, -1, -w - 1},
             {0, -1, -w},
             {1, -1, -w + 1},
             {1, 0, 1},
             {1, 1, w + 1},
             {0, 1, w},
             {-1, 1, w - 1}}};
  }

  template<typename PointT>
  inline float
  organizedEdgeAbsZ (const PointT& point)
  {
    return std::abs (point.z);
  }

  template<typename PointT, typename PointLT>
  inline unsigned
  organizedEdgeClassifyDepthDiscontinuity (const float dist,
                                           const float curr_depth,
                                           const float threshold,
                                           const int detecting_edge_types)
  {
    unsigned label = 0U;
    if (std::abs (dist) <= threshold * std::abs (curr_depth))
      return label;

    if (dist > 0.f)
    {
      if (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDED)
        label |= pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDED;
    }
    else
    {
      if (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDING)
        label |= pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDING;
    }
    return label;
  }

  template<typename PointT, typename PointLT>
  unsigned
  organizedEdgeDepthLabelAtStandard (const pcl::PointCloud<PointT>& input,
                                     const std::array<OrganizedEdgeNeighbor, 8>& directions,
                                     const int row,
                                     const int col,
                                     const float th_depth_discon,
                                     const int max_search_neighbors,
                                     const int detecting_edge_types)
  {
    const int curr_idx = row * static_cast<int> (input.width) + col;
    if (!std::isfinite (input[curr_idx].z))
      return 0U;

    const float curr_depth = organizedEdgeAbsZ (input[curr_idx]);
    std::array<float, 8> nghr_dist{};
    bool found_invalid_neighbor = false;
    for (std::size_t d_idx = 0; d_idx < directions.size (); d_idx++)
    {
      const int nghr_idx = curr_idx + directions[d_idx].d_index;
      assert (nghr_idx >= 0 && static_cast<std::size_t>(nghr_idx) < input.size ());
      if (!std::isfinite (input[nghr_idx].z))
      {
        found_invalid_neighbor = true;
        break;
      }
      nghr_dist[d_idx] = curr_depth - organizedEdgeAbsZ (input[nghr_idx]);
    }

    if (!found_invalid_neighbor)
    {
      const auto minmax = std::minmax_element (nghr_dist.cbegin (), nghr_dist.cend ());
      const float nghr_dist_min = *minmax.first;
      const float nghr_dist_max = *minmax.second;
      const float dist_dominant =
          std::abs (nghr_dist_min) > std::abs (nghr_dist_max) ? nghr_dist_min : nghr_dist_max;
      return organizedEdgeClassifyDepthDiscontinuity<PointT, PointLT> (
          dist_dominant, curr_depth, th_depth_discon, detecting_edge_types);
    }

    int dx = 0;
    int dy = 0;
    int num_of_invalid_pt = 0;
    for (const auto& direction : directions)
    {
      const int nghr_idx = curr_idx + direction.d_index;
      assert (nghr_idx >= 0 && static_cast<std::size_t>(nghr_idx) < input.size ());
      if (!std::isfinite (input[nghr_idx].z))
      {
        dx += direction.d_x;
        dy += direction.d_y;
        num_of_invalid_pt++;
      }
    }

    assert (num_of_invalid_pt > 0);
    const float f_dx = static_cast<float> (dx) / static_cast<float> (num_of_invalid_pt);
    const float f_dy = static_cast<float> (dy) / static_cast<float> (num_of_invalid_pt);

    float corr_depth = std::numeric_limits<float>::quiet_NaN ();
    for (int s_idx = 1; s_idx < max_search_neighbors; s_idx++)
    {
      const int s_row = row + static_cast<int> (std::floor (f_dy * static_cast<float> (s_idx)));
      const int s_col = col + static_cast<int> (std::floor (f_dx * static_cast<float> (s_idx)));

      if (s_row < 0 || s_row >= static_cast<int>(input.height) || s_col < 0 || s_col >= static_cast<int>(input.width))
        break;

      const int candidate_idx = s_row * static_cast<int>(input.width) + s_col;
      if (std::isfinite (input[candidate_idx].z))
      {
        corr_depth = organizedEdgeAbsZ (input[candidate_idx]);
        break;
      }
    }

    if (!std::isnan (corr_depth))
      return organizedEdgeClassifyDepthDiscontinuity<PointT, PointLT> (
          curr_depth - corr_depth, curr_depth, th_depth_discon, detecting_edge_types);

    if (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_NAN_BOUNDARY)
      return pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_NAN_BOUNDARY;
    return 0U;
  }

  template<typename PointT, typename PointLT>
  void
  organizedEdgeDepthLabelsStandard (const pcl::PointCloud<PointT>& input,
                                    const float th_depth_discon,
                                    const int max_search_neighbors,
                                    const int detecting_edge_types,
                                    pcl::PointCloud<PointLT>& labels)
  {
    if (!((detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_NAN_BOUNDARY) ||
          (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDING) ||
          (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDED)))
      return;

    const auto directions = organizedEdgeNeighbors (input.width);
    for (int row = 1; row < static_cast<int>(input.height) - 1; row++)
    {
      for (int col = 1; col < static_cast<int>(input.width) - 1; col++)
      {
        const int curr_idx = row * static_cast<int>(input.width) + col;
        labels[curr_idx].label |= organizedEdgeDepthLabelAtStandard<PointT, PointLT> (
            input, directions, row, col, th_depth_discon, max_search_neighbors, detecting_edge_types);
      }
    }
  }

#if defined(__RVV10__)
  inline vbool16_t
  organizedEdgeFiniteF32Mask (const vfloat32m2_t value, const std::size_t vl)
  {
    return __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (value, vl),
                                       std::numeric_limits<float>::max (),
                                       vl);
  }
#endif

  template<typename PointT, typename PointLT>
  PCL_ORGANIZED_EDGE_RVV_NOINLINE bool
  organizedEdgeDepthLabelsRVV (const pcl::PointCloud<PointT>& input,
                               const float th_depth_discon,
                               const int max_search_neighbors,
                               const int detecting_edge_types,
                               pcl::PointCloud<PointLT>& labels)
  {
#if defined(__RVV10__)
    if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value ||
                  !std::is_same_v<PointLT, pcl::Label>)
    {
      return false;
    }
    else
    {
      if (!((detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_NAN_BOUNDARY) ||
            (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDING) ||
            (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDED)))
        return true;
      if (input.width < 3 || input.height < 3)
        return true;

      using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
      const auto directions = organizedEdgeNeighbors (input.width);
      const auto* base = reinterpret_cast<const std::uint8_t*> (input.points.data ());
      for (std::size_t row = 1; row + 1 < input.height; ++row)
      {
        std::size_t col = 1;
        while (col + 1 < input.width)
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (input.width - 1 - col);
          const std::size_t curr_idx = row * input.width + col;
          const std::uint8_t* curr_base = base + curr_idx * sizeof (PointT);
          const vfloat32m2_t center =
              pcl::rvv_load::strided_load_field_f32m2<PointT, pcl::fields::z> (curr_base, vl);
          const vfloat32m2_t center_abs = __riscv_vfabs_v_f32m2 (center, vl);
          vbool16_t all_finite = organizedEdgeFiniteF32Mask (center, vl);

          vfloat32m2_t min_dist =
              __riscv_vfmv_v_f_f32m2 (std::numeric_limits<float>::max (), vl);
          vfloat32m2_t max_dist =
              __riscv_vfmv_v_f_f32m2 (-std::numeric_limits<float>::max (), vl);
          for (const auto& direction : directions)
          {
            const auto neighbor_base = curr_base + static_cast<std::ptrdiff_t>(direction.d_index) *
                                                       static_cast<std::ptrdiff_t>(sizeof (PointT));
            const vfloat32m2_t neighbor_z =
                pcl::rvv_load::strided_load_f32m2<sizeof (PointT)> (
                    reinterpret_cast<const float*> (neighbor_base + Layout::kZ), vl);
            all_finite =
                __riscv_vmand_mm_b16 (all_finite, organizedEdgeFiniteF32Mask (neighbor_z, vl), vl);
            const vfloat32m2_t dist =
                __riscv_vfsub_vv_f32m2 (center_abs, __riscv_vfabs_v_f32m2 (neighbor_z, vl), vl);
            min_dist = __riscv_vfmin_vv_f32m2 (min_dist, dist, vl);
            max_dist = __riscv_vfmax_vv_f32m2 (max_dist, dist, vl);
          }

          const vbool16_t min_dominates = __riscv_vmfgt_vv_f32m2_b16 (
              __riscv_vfabs_v_f32m2 (min_dist, vl), __riscv_vfabs_v_f32m2 (max_dist, vl), vl);
          const vfloat32m2_t dominant =
              __riscv_vmerge_vvm_f32m2 (max_dist, min_dist, min_dominates, vl);
          const vfloat32m2_t threshold =
              __riscv_vfmul_vf_f32m2 (center_abs, th_depth_discon, vl);
          const vbool16_t discontinuity = __riscv_vmand_mm_b16 (
              all_finite,
              __riscv_vmfgt_vv_f32m2_b16 (__riscv_vfabs_v_f32m2 (dominant, vl), threshold, vl),
              vl);

          vuint32m2_t out = __riscv_vmv_v_x_u32m2 (0, vl);
          if (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDED)
          {
            const vbool16_t occluded = __riscv_vmand_mm_b16 (
                discontinuity, __riscv_vmfgt_vf_f32m2_b16 (dominant, 0.0f, vl), vl);
            out = __riscv_vmerge_vvm_u32m2 (
                out,
                __riscv_vor_vx_u32m2 (
                    out, pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDED, vl),
                occluded,
                vl);
          }
          if (detecting_edge_types & pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDING)
          {
            const vbool16_t occluding = __riscv_vmand_mm_b16 (
                discontinuity, __riscv_vmflt_vf_f32m2_b16 (dominant, 0.0f, vl), vl);
            out = __riscv_vmerge_vvm_u32m2 (
                out,
                __riscv_vor_vx_u32m2 (
                    out, pcl::OrganizedEdgeBase<PointT, PointLT>::EDGELABEL_OCCLUDING, vl),
                occluding,
                vl);
          }

          const vuint32m2_t existing =
              __riscv_vlse32_v_u32m2 (&labels[curr_idx].label, sizeof (PointLT), vl);
          __riscv_vsse32_v_u32m2 (
              &labels[curr_idx].label, sizeof (PointLT), __riscv_vor_vv_u32m2 (existing, out, vl), vl);

          if (__riscv_vcpop_m_b16 (all_finite, vl) != vl)
          {
            for (std::size_t lane = 0; lane < vl; ++lane)
            {
              const std::size_t lane_col = col + lane;
              labels[row * input.width + lane_col].label |=
                  organizedEdgeDepthLabelAtStandard<PointT, PointLT> (
                      input,
                      directions,
                      static_cast<int> (row),
                      static_cast<int> (lane_col),
                      th_depth_discon,
                      max_search_neighbors,
                      detecting_edge_types);
            }
          }
          col += vl;
        }
      }
      return true;
    }
#else
    (void)input;
    (void)th_depth_discon;
    (void)max_search_neighbors;
    (void)detecting_edge_types;
    (void)labels;
    return false;
#endif
  }
} // namespace detail
} // namespace pcl

/**
 *  Directions: 1 2 3
 *              0 x 4
 *              7 6 5
 * e.g. direction y means we came from pixel with label y to the center pixel x
 */
//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointLT> void
pcl::OrganizedEdgeBase<PointT, PointLT>::compute (pcl::PointCloud<PointLT>& labels, std::vector<pcl::PointIndices>& label_indices) const
{
  pcl::Label invalid_pt;
  invalid_pt.label = static_cast<unsigned>(0);
  labels.resize (input_->size (), invalid_pt);
  labels.width = input_->width;
  labels.height = input_->height;
  
  extractEdges (labels);

  assignLabelIndices (labels, label_indices);
}

//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointLT> void
pcl::OrganizedEdgeBase<PointT, PointLT>::assignLabelIndices (pcl::PointCloud<PointLT>& labels, std::vector<pcl::PointIndices>& label_indices) const
{
  const auto invalid_label = static_cast<unsigned>(0);
  label_indices.resize (num_of_edgetype_);
  for (std::size_t idx = 0; idx < input_->size (); idx++)
  {
    if (labels[idx].label != invalid_label)
    {
      for (int edge_type = 0; edge_type < num_of_edgetype_; edge_type++)
      {
        if ((labels[idx].label >> edge_type) & 1)
          label_indices[edge_type].indices.push_back (idx);
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointLT> void
pcl::OrganizedEdgeBase<PointT, PointLT>::extractEdges (pcl::PointCloud<PointLT>& labels) const
{
  if (pcl::detail::organizedEdgeDepthLabelsRVV<PointT, PointLT> (
          *input_, th_depth_discon_, max_search_neighbors_, detecting_edge_types_, labels))
    return;

  pcl::detail::organizedEdgeDepthLabelsStandard<PointT, PointLT> (
      *input_, th_depth_discon_, max_search_neighbors_, detecting_edge_types_, labels);
}


//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointLT> void
pcl::OrganizedEdgeFromRGB<PointT, PointLT>::compute (pcl::PointCloud<PointLT>& labels, std::vector<pcl::PointIndices>& label_indices) const
{
  pcl::Label invalid_pt;
  invalid_pt.label = static_cast<unsigned>(0);
  labels.resize (input_->size (), invalid_pt);
  labels.width = input_->width;
  labels.height = input_->height;

  OrganizedEdgeBase<PointT, PointLT>::extractEdges (labels);
  extractEdges (labels);

  this->assignLabelIndices (labels, label_indices);
}

//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointLT> void
pcl::OrganizedEdgeFromRGB<PointT, PointLT>::extractEdges (pcl::PointCloud<PointLT>& labels) const
{
  if ((detecting_edge_types_ & EDGELABEL_RGB_CANNY))
  {
    pcl::PointCloud<PointXYZI>::Ptr gray (new pcl::PointCloud<PointXYZI>);
    gray->width = input_->width;
    gray->height = input_->height;
    gray->resize (input_->height*input_->width);

    for (std::size_t i = 0; i < input_->size (); ++i)
      (*gray)[i].intensity = static_cast<float>(((*input_)[i].r + (*input_)[i].g + (*input_)[i].b) / 3);

    pcl::PointCloud<pcl::PointXYZIEdge> img_edge_rgb;
    pcl::Edge<PointXYZI, pcl::PointXYZIEdge> edge;
    edge.setInputCloud (gray);
    edge.setHysteresisThresholdLow (th_rgb_canny_low_);
    edge.setHysteresisThresholdHigh (th_rgb_canny_high_);
    edge.detectEdgeCanny (img_edge_rgb);
    
    for (std::uint32_t row=0; row<labels.height; row++)
    {
      for (std::uint32_t col=0; col<labels.width; col++)
      {
        if (img_edge_rgb (col, row).magnitude == 255.f)
          labels[row * labels.width + col].label |= EDGELABEL_RGB_CANNY;
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointNT, typename PointLT> void
pcl::OrganizedEdgeFromNormals<PointT, PointNT, PointLT>::compute (pcl::PointCloud<PointLT>& labels, std::vector<pcl::PointIndices>& label_indices) const
{
  pcl::Label invalid_pt;
  invalid_pt.label = static_cast<unsigned>(0);
  labels.resize (input_->size (), invalid_pt);
  labels.width = input_->width;
  labels.height = input_->height;
  
  OrganizedEdgeBase<PointT, PointLT>::extractEdges (labels);
  extractEdges (labels);

  this->assignLabelIndices (labels, label_indices);
}

//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointNT, typename PointLT> void
pcl::OrganizedEdgeFromNormals<PointT, PointNT, PointLT>::extractEdges (pcl::PointCloud<PointLT>& labels) const
{
  if ((detecting_edge_types_ & EDGELABEL_HIGH_CURVATURE))
  {

    pcl::PointCloud<PointXYZI> nx, ny;
    nx.width = normals_->width;
    nx.height = normals_->height;
    nx.resize (normals_->height*normals_->width);

    ny.width = normals_->width;
    ny.height = normals_->height;
    ny.resize (normals_->height*normals_->width);

    for (std::uint32_t row=0; row<normals_->height; row++)
    {
      for (std::uint32_t col=0; col<normals_->width; col++)
      {
        nx (col, row).intensity = (*normals_)[row*normals_->width + col].normal_x;
        ny (col, row).intensity = (*normals_)[row*normals_->width + col].normal_y;
      }
    }

    pcl::PointCloud<pcl::PointXYZIEdge> img_edge;
    pcl::Edge<PointXYZI, pcl::PointXYZIEdge> edge;
    edge.setHysteresisThresholdLow (th_hc_canny_low_);
    edge.setHysteresisThresholdHigh (th_hc_canny_high_);
    edge.canny (nx, ny, img_edge);

    for (std::uint32_t row=0; row<labels.height; row++)
    {
      for (std::uint32_t col=0; col<labels.width; col++)
      {
        if (img_edge (col, row).magnitude == 255.f)
          labels[row * labels.width + col].label |= EDGELABEL_HIGH_CURVATURE;
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
template<typename PointT, typename PointNT, typename PointLT> void
pcl::OrganizedEdgeFromRGBNormals<PointT, PointNT, PointLT>::compute (pcl::PointCloud<PointLT>& labels, std::vector<pcl::PointIndices>& label_indices) const
{
  pcl::Label invalid_pt;
  invalid_pt.label = static_cast<unsigned>(0);
  labels.resize (input_->size (), invalid_pt);
  labels.width = input_->width;
  labels.height = input_->height;
  
  OrganizedEdgeBase<PointT, PointLT>::extractEdges (labels);
  OrganizedEdgeFromNormals<PointT, PointNT, PointLT>::extractEdges (labels);
  OrganizedEdgeFromRGB<PointT, PointLT>::extractEdges (labels);

  this->assignLabelIndices (labels, label_indices);
}

#define PCL_INSTANTIATE_OrganizedEdgeBase(T,LT)               template class PCL_EXPORTS pcl::OrganizedEdgeBase<T,LT>;
#define PCL_INSTANTIATE_OrganizedEdgeFromRGB(T,LT)            template class PCL_EXPORTS pcl::OrganizedEdgeFromRGB<T,LT>;
#define PCL_INSTANTIATE_OrganizedEdgeFromNormals(T,NT,LT)     template class PCL_EXPORTS pcl::OrganizedEdgeFromNormals<T,NT,LT>;
#define PCL_INSTANTIATE_OrganizedEdgeFromRGBNormals(T,NT,LT)  template class PCL_EXPORTS pcl::OrganizedEdgeFromRGBNormals<T,NT,LT>;

#undef PCL_ORGANIZED_EDGE_RVV_NOINLINE

#endif //#ifndef PCL_FEATURES_IMPL_ORGANIZED_EDGE_DETECTION_H_
