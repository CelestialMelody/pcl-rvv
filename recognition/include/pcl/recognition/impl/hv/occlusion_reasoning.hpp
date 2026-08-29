/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
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
 *   * Neither the name of Willow Garage, Inc. nor the names of its
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
 */

#ifndef PCL_RECOGNITION_OCCLUSION_REASONING_HPP_
#define PCL_RECOGNITION_OCCLUSION_REASONING_HPP_

#include <pcl/recognition/hv/occlusion_reasoning.h>
#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl
{
  namespace detail
  {
    template<typename ModelT>
    inline void
    zBufferingFilterStd (const pcl::PointCloud<ModelT>& model,
                         const float* depth,
                         const int width,
                         const int height,
                         const float focal,
                         const float threshold,
                         pcl::Indices& indices_to_keep)
    {
      const float cx = static_cast<float> (width) / 2.f - 0.5f;
      const float cy = static_cast<float> (height) / 2.f - 0.5f;

      indices_to_keep.resize (model.size ());
      int keep = 0;
      for (std::size_t i = 0; i < model.size (); i++)
      {
        float x = model[i].x;
        float y = model[i].y;
        float z = model[i].z;
        int u = static_cast<int> (focal * x / z + cx);
        int v = static_cast<int> (focal * y / z + cy);

        if (u >= width || v >= height || u < 0 || v < 0)
          continue;

        if ((z - threshold) > depth[u * height + v] || !std::isfinite (depth[u * height + v]))
          continue;

        indices_to_keep[keep] = static_cast<int> (i);
        keep++;
      }

      indices_to_keep.resize (keep);
    }

#if defined(__RVV10__)
    template<typename ModelT>
    inline bool
    zBufferingFilterRVV (const pcl::PointCloud<ModelT>& model,
                         const float* depth,
                         const int width,
                         const int height,
                         const float focal,
                         const float threshold,
                         pcl::Indices& indices_to_keep)
    {
      if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<ModelT>::value)
      {
        return (false);
      }
      else
      {
        if (depth == nullptr || !model.is_dense ||
            model.size () > static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max ()))
          return (false);

        using Layout = pcl::rvv::RVVXYZAoSFloatLayout<ModelT>;
        const float cx = static_cast<float> (width) / 2.f - 0.5f;
        const float cy = static_cast<float> (height) / 2.f - 0.5f;
        const std::size_t max_vl = __riscv_vsetvlmax_e32m2 ();
        std::vector<std::uint32_t> index_values (max_vl);
        std::vector<int> u_values (max_vl);
        std::vector<int> v_values (max_vl);
        std::vector<float> z_values (max_vl);

        indices_to_keep.clear ();
        indices_to_keep.reserve (model.size ());
        const auto* base_u8 = reinterpret_cast<const std::uint8_t*> (model.points.data ());
        for (std::size_t point_index = 0; point_index < model.size ();)
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (model.size () - point_index);
          const auto* chunk_u8 = base_u8 + point_index * sizeof (ModelT);
          vfloat32m2_t x;
          vfloat32m2_t y;
          vfloat32m2_t z;
          pcl::rvv_load::strided_load3_f32m2<sizeof (ModelT), Layout::kX, Layout::kY, Layout::kZ> (
              chunk_u8, vl, x, y, z);

          const vfloat32m2_t inf = __riscv_vfmv_v_f_f32m2 (
              std::numeric_limits<float>::infinity (), vl);
          const vfloat32m2_t neg_inf = __riscv_vfmv_v_f_f32m2 (
              -std::numeric_limits<float>::infinity (), vl);
          vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16 (x, x, vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (y, y, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfeq_vv_f32m2_b16 (z, z, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vv_f32m2_b16 (x, inf, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vv_f32m2_b16 (y, inf, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vv_f32m2_b16 (z, inf, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vv_f32m2_b16 (x, neg_inf, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vv_f32m2_b16 (y, neg_inf, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vv_f32m2_b16 (z, neg_inf, vl), vl);
          finite = __riscv_vmand_mm_b16 (finite, __riscv_vmfne_vf_f32m2_b16 (z, 0.0f, vl), vl);

          const vfloat32m2_t one_f = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
          const vfloat32m2_t zero_f = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
          const vfloat32m2_t safe_x = __riscv_vmerge_vvm_f32m2 (zero_f, x, finite, vl);
          const vfloat32m2_t safe_y = __riscv_vmerge_vvm_f32m2 (zero_f, y, finite, vl);
          const vfloat32m2_t safe_z = __riscv_vmerge_vvm_f32m2 (one_f, z, finite, vl);
          const vfloat32m2_t projected_u_raw = __riscv_vfadd_vf_f32m2 (
              __riscv_vfmul_vf_f32m2 (__riscv_vfdiv_vv_f32m2 (safe_x, safe_z, vl), focal, vl), cx, vl);
          const vfloat32m2_t projected_v_raw = __riscv_vfadd_vf_f32m2 (
              __riscv_vfmul_vf_f32m2 (__riscv_vfdiv_vv_f32m2 (safe_y, safe_z, vl), focal, vl), cy, vl);
          const vint32m2_t u = __riscv_vfcvt_rtz_x_f_v_i32m2 (projected_u_raw, vl);
          const vint32m2_t v = __riscv_vfcvt_rtz_x_f_v_i32m2 (projected_v_raw, vl);

          vbool16_t keep = finite;
          keep = __riscv_vmand_mm_b16 (keep, __riscv_vmsge_vx_i32m2_b16 (u, 0, vl), vl);
          keep = __riscv_vmand_mm_b16 (keep, __riscv_vmslt_vx_i32m2_b16 (u, width, vl), vl);
          keep = __riscv_vmand_mm_b16 (keep, __riscv_vmsge_vx_i32m2_b16 (v, 0, vl), vl);
          keep = __riscv_vmand_mm_b16 (keep, __riscv_vmslt_vx_i32m2_b16 (v, height, vl), vl);

          const std::size_t active = __riscv_vcpop_m_b16 (keep, vl);
          if (active == 0)
          {
            point_index += vl;
            continue;
          }

          const vuint32m2_t source_index = __riscv_vadd_vx_u32m2 (
              __riscv_vid_v_u32m2 (vl), static_cast<std::uint32_t> (point_index), vl);
          const vuint32m2_t kept_index = __riscv_vcompress_vm_u32m2 (source_index, keep, vl);
          const vint32m2_t kept_u = __riscv_vcompress_vm_i32m2 (u, keep, vl);
          const vint32m2_t kept_v = __riscv_vcompress_vm_i32m2 (v, keep, vl);
          const vfloat32m2_t kept_z = __riscv_vcompress_vm_f32m2 (z, keep, vl);
          __riscv_vse32_v_u32m2 (index_values.data (), kept_index, active);
          __riscv_vse32_v_i32m2 (u_values.data (), kept_u, active);
          __riscv_vse32_v_i32m2 (v_values.data (), kept_v, active);
          __riscv_vse32_v_f32m2 (z_values.data (), kept_z, active);

          for (std::size_t lane = 0; lane < active; ++lane)
          {
            const int u_lane = u_values[lane];
            const int v_lane = v_values[lane];
            const float depth_at_pixel = depth[u_lane * height + v_lane];
            if ((z_values[lane] - threshold) > depth_at_pixel || !std::isfinite (depth_at_pixel))
              continue;

            indices_to_keep.push_back (static_cast<int> (index_values[lane]));
          }

          point_index += vl;
        }
        return (true);
      }
    }
#endif

  } // namespace detail
} // namespace pcl

///////////////////////////////////////////////////////////////////////////////////////////
template<typename ModelT, typename SceneT>
pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::ZBuffering (int resx, int resy, float f) :
  f_ (f), cx_ (resx), cy_ (resy), depth_ (nullptr)
{
}

///////////////////////////////////////////////////////////////////////////////////////////
template<typename ModelT, typename SceneT>
pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::ZBuffering () :
  f_ (), cx_ (), cy_ (), depth_ (nullptr)
{
}

///////////////////////////////////////////////////////////////////////////////////////////
template<typename ModelT, typename SceneT>
pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::~ZBuffering ()
{
  delete[] depth_;
}

///////////////////////////////////////////////////////////////////////////////////////////
template<typename ModelT, typename SceneT> void
pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::filter (typename pcl::PointCloud<ModelT>::ConstPtr & model,
                                                              typename pcl::PointCloud<ModelT>::Ptr & filtered, float thres)
{
  pcl::Indices indices_to_keep;
  filter(model, indices_to_keep, thres);
  pcl::copyPointCloud (*model, indices_to_keep, *filtered);
}

///////////////////////////////////////////////////////////////////////////////////////////
template<typename ModelT, typename SceneT> void
pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::filter (typename pcl::PointCloud<ModelT>::ConstPtr & model,
                                                                      pcl::Indices & indices_to_keep, float thres)
{
#if defined(__RVV10__)
  if (pcl::detail::zBufferingFilterRVV (*model, depth_, cx_, cy_, f_, thres, indices_to_keep))
  {
    pcl::detail::recordOcclusionReasoningPath (pcl::detail::OcclusionReasoningPathHook::Rvv);
    return;
  }
#endif

  pcl::detail::zBufferingFilterStd (*model, depth_, cx_, cy_, f_, thres, indices_to_keep);
  pcl::detail::recordOcclusionReasoningPath (pcl::detail::OcclusionReasoningPathHook::Scalar);
}

///////////////////////////////////////////////////////////////////////////////////////////
template<typename ModelT, typename SceneT> void
pcl::occlusion_reasoning::ZBuffering<ModelT, SceneT>::computeDepthMap (typename pcl::PointCloud<SceneT>::ConstPtr & scene, bool compute_focal,
                                                                       bool smooth, int wsize)
{
  float cx, cy;
  cx = static_cast<float> (cx_) / 2.f - 0.5f;
  cy = static_cast<float> (cy_) / 2.f - 0.5f;

  //compute the focal length
  if (compute_focal)
  {

    float max_u, max_v, min_u, min_v;
    max_u = max_v = std::numeric_limits<float>::max () * -1;
    min_u = min_v = std::numeric_limits<float>::max ();

    for (const auto& point: *scene)
    {
      float b_x = point.x / point.z;
      if (b_x > max_u)
        max_u = b_x;
      if (b_x < min_u)
        min_u = b_x;

      float b_y = point.y / point.z;
      if (b_y > max_v)
        max_v = b_y;
      if (b_y < min_v)
        min_v = b_y;
    }

    float maxC = std::max (std::max (std::abs (max_u), std::abs (max_v)), std::max (std::abs (min_u), std::abs (min_v)));
    f_ = (cx) / maxC;
  }

  depth_ = new float[cx_ * cy_];
  std::fill_n(depth_, static_cast<std::size_t> (cx_) * static_cast<std::size_t> (cy_),
              std::numeric_limits<float>::quiet_NaN ());

  for (const auto& point: *scene)
  {
    const float& x = point.x;
    const float& y = point.y;
    const float& z = point.z;
    const int u = static_cast<int> (f_ * x / z + cx);
    const int v = static_cast<int> (f_ * y / z + cy);

    if (u >= cx_ || v >= cy_ || u < 0 || v < 0)
      continue;

    if ((z < depth_[u * cy_ + v]) || (!std::isfinite (depth_[u * cy_ + v])))
      depth_[u * cy_ + v] = z;
  }

  if (smooth)
  {
    //Dilate and smooth the depth map
    int ws = wsize;
    int ws2 = static_cast<int>(std::floor (static_cast<float> (ws) / 2.f));
    float * depth_smooth = new float[cx_ * cy_];
    for (int i = 0; i < (cx_ * cy_); i++)
      depth_smooth[i] = std::numeric_limits<float>::quiet_NaN ();

    for (int u = ws2; u < (cx_ - ws2); u++)
    {
      for (int v = ws2; v < (cy_ - ws2); v++)
      {
        float min = std::numeric_limits<float>::max ();
        for (int j = (u - ws2); j <= (u + ws2); j++)
        {
          for (int i = (v - ws2); i <= (v + ws2); i++)
          {
            if (std::isfinite (depth_[j * cy_ + i]) && (depth_[j * cy_ + i] < min))
            {
              min = depth_[j * cy_ + i];
            }
          }
        }

        if (min < (std::numeric_limits<float>::max () - 0.1))
        {
          depth_smooth[u * cy_ + v] = min;
        }
      }
    }

    std::copy(depth_smooth, depth_smooth + cx_ * cy_, depth_);
    delete[] depth_smooth;
  }
}

#endif    // PCL_RECOGNITION_OCCLUSION_REASONING_HPP_
