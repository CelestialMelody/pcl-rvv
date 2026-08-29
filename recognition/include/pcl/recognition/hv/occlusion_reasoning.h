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

#pragma once

#include <pcl/common/io.h>
#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#endif

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
    enum class OcclusionReasoningPathHook
    {
      None = 0,
      Scalar = 1,
      Rvv = 2,
    };

#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
    inline int&
    occlusionReasoningLastTestHook ()
    {
      static int last_hook = static_cast<int> (OcclusionReasoningPathHook::None);
      return (last_hook);
    }

    extern "C" inline void
    pcl_rvv_occlusion_reasoning_reset_test_hook ()
    {
      occlusionReasoningLastTestHook () =
          static_cast<int> (OcclusionReasoningPathHook::None);
    }

    extern "C" inline int
    pcl_rvv_occlusion_reasoning_last_test_hook ()
    {
      return (occlusionReasoningLastTestHook ());
    }
#endif

    inline void
    recordOcclusionReasoningPath (const OcclusionReasoningPathHook path)
    {
#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
      occlusionReasoningLastTestHook () = static_cast<int> (path);
#else
      (void)path;
#endif
    }

    template<typename SceneT, typename ModelT>
    inline void
    occlusionReasoningFilterStd (const pcl::PointCloud<SceneT>& organized_cloud,
                                 const pcl::PointCloud<ModelT>& to_be_filtered,
                                 const float f,
                                 const float threshold,
                                 const bool check_invalid_depth,
                                 const bool keep_occluded,
                                 pcl::Indices& indices_to_keep)
    {
      const float cx = (static_cast<float> (organized_cloud.width) / 2.f - 0.5f);
      const float cy = (static_cast<float> (organized_cloud.height) / 2.f - 0.5f);

      indices_to_keep.resize (to_be_filtered.size ());

      int keep = 0;
      for (std::size_t i = 0; i < to_be_filtered.size (); i++)
      {
        float x = to_be_filtered[i].x;
        float y = to_be_filtered[i].y;
        float z = to_be_filtered[i].z;
        int u = static_cast<int> (f * x / z + cx);
        int v = static_cast<int> (f * y / z + cy);

        if ((u >= static_cast<int> (organized_cloud.width)) ||
            (v >= static_cast<int> (organized_cloud.height)) || (u < 0) || (v < 0))
          continue;

        if (check_invalid_depth)
        {
          if (!std::isfinite (organized_cloud.at (u, v).x) ||
              !std::isfinite (organized_cloud.at (u, v).y) ||
              !std::isfinite (organized_cloud.at (u, v).z))
            continue;
        }

        float z_oc = organized_cloud.at (u, v).z;
        const bool is_occluded = (z - z_oc) > threshold;
        if (is_occluded != keep_occluded)
          continue;

        indices_to_keep[keep] = static_cast<int> (i);
        keep++;
      }

      indices_to_keep.resize (keep);
    }

#if defined(__RVV10__)
    template<typename SceneT, typename ModelT>
    inline bool
    occlusionReasoningFilterRVV (const pcl::PointCloud<SceneT>& organized_cloud,
                                 const pcl::PointCloud<ModelT>& to_be_filtered,
                                 const float f,
                                 const float threshold,
                                 const bool check_invalid_depth,
                                 const bool keep_occluded,
                                 pcl::Indices& indices_to_keep)
    {
      if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<ModelT>::value)
      {
        return (false);
      }
      else
      {
        if (!to_be_filtered.is_dense || organized_cloud.width == 0 ||
            organized_cloud.height == 0 ||
            organized_cloud.width > static_cast<std::uint32_t> (std::numeric_limits<int>::max ()) ||
            organized_cloud.height > static_cast<std::uint32_t> (std::numeric_limits<int>::max ()) ||
            to_be_filtered.size () > static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max ()))
          return (false);

        using Layout = pcl::rvv::RVVXYZAoSFloatLayout<ModelT>;
        const int width = static_cast<int> (organized_cloud.width);
        const int height = static_cast<int> (organized_cloud.height);
        const float cx = (static_cast<float> (organized_cloud.width) / 2.f - 0.5f);
        const float cy = (static_cast<float> (organized_cloud.height) / 2.f - 0.5f);
        const std::size_t max_vl = __riscv_vsetvlmax_e32m2 ();
        std::vector<std::uint32_t> index_values (max_vl);
        std::vector<int> u_values (max_vl);
        std::vector<int> v_values (max_vl);
        std::vector<float> z_values (max_vl);

        indices_to_keep.clear ();
        indices_to_keep.reserve (to_be_filtered.size ());
        const auto* base_u8 = reinterpret_cast<const std::uint8_t*> (to_be_filtered.points.data ());
        for (std::size_t point_index = 0; point_index < to_be_filtered.size ();)
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (to_be_filtered.size () - point_index);
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
              __riscv_vfmul_vf_f32m2 (__riscv_vfdiv_vv_f32m2 (safe_x, safe_z, vl), f, vl), cx, vl);
          const vfloat32m2_t projected_v_raw = __riscv_vfadd_vf_f32m2 (
              __riscv_vfmul_vf_f32m2 (__riscv_vfdiv_vv_f32m2 (safe_y, safe_z, vl), f, vl), cy, vl);
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
            const auto& scene_point = organized_cloud.at (u_values[lane], v_values[lane]);
            if (check_invalid_depth &&
                (!std::isfinite (scene_point.x) || !std::isfinite (scene_point.y) ||
                 !std::isfinite (scene_point.z)))
              continue;

            const bool is_occluded = (z_values[lane] - scene_point.z) > threshold;
            if (is_occluded != keep_occluded)
              continue;

            indices_to_keep.push_back (static_cast<int> (index_values[lane]));
          }

          point_index += vl;
        }
        return (true);
      }
    }
#endif

    template<typename SceneT, typename ModelT>
    inline void
    occlusionReasoningFilterDispatch (const pcl::PointCloud<SceneT>& organized_cloud,
                                      const pcl::PointCloud<ModelT>& to_be_filtered,
                                      const float f,
                                      const float threshold,
                                      const bool check_invalid_depth,
                                      const bool keep_occluded,
                                      pcl::Indices& indices_to_keep)
    {
#if defined(__RVV10__)
      if (occlusionReasoningFilterRVV (organized_cloud, to_be_filtered, f, threshold,
                                       check_invalid_depth, keep_occluded, indices_to_keep))
      {
        recordOcclusionReasoningPath (OcclusionReasoningPathHook::Rvv);
        return;
      }
#endif

      occlusionReasoningFilterStd (organized_cloud, to_be_filtered, f, threshold,
                                   check_invalid_depth, keep_occluded, indices_to_keep);
      recordOcclusionReasoningPath (OcclusionReasoningPathHook::Scalar);
    }
  } // namespace detail

  namespace occlusion_reasoning
  {
    /**
     * \brief Class to reason about occlusions
     * \author Aitor Aldoma
     */

    template<typename ModelT, typename SceneT>
      class ZBuffering
      {
      private:
        float f_;
        int cx_, cy_;
        float * depth_;

      public:

        ZBuffering ();
        ZBuffering (int resx, int resy, float f);
        ~ZBuffering ();
        void
        computeDepthMap (typename pcl::PointCloud<SceneT>::ConstPtr & scene, bool compute_focal = false, bool smooth = false, int wsize = 3);
        void
        filter (typename pcl::PointCloud<ModelT>::ConstPtr & model, typename pcl::PointCloud<ModelT>::Ptr & filtered, float thres = 0.01);
        void filter (typename pcl::PointCloud<ModelT>::ConstPtr & model, pcl::Indices & indices, float thres = 0.01);
      };

    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    filter (typename pcl::PointCloud<SceneT>::ConstPtr & organized_cloud, typename pcl::PointCloud<ModelT>::ConstPtr & to_be_filtered, float f,
            float threshold)
    {
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      pcl::Indices indices_to_keep;
      pcl::detail::occlusionReasoningFilterDispatch (
          *organized_cloud, *to_be_filtered, f, threshold, true, false, indices_to_keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }

    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    filter (typename pcl::PointCloud<SceneT>::Ptr & organized_cloud, typename pcl::PointCloud<ModelT>::Ptr & to_be_filtered, float f,
            float threshold, bool check_invalid_depth = true)
    {
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      pcl::Indices indices_to_keep;
      pcl::detail::occlusionReasoningFilterDispatch (
          *organized_cloud, *to_be_filtered, f, threshold, check_invalid_depth, false, indices_to_keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }

    template<typename ModelT, typename SceneT> typename pcl::PointCloud<ModelT>::Ptr
    getOccludedCloud (typename pcl::PointCloud<SceneT>::Ptr & organized_cloud, typename pcl::PointCloud<ModelT>::Ptr & to_be_filtered, float f,
                      float threshold, bool check_invalid_depth = true)
    {
      typename pcl::PointCloud<ModelT>::Ptr filtered (new pcl::PointCloud<ModelT> ());

      pcl::Indices indices_to_keep;
      pcl::detail::occlusionReasoningFilterDispatch (
          *organized_cloud, *to_be_filtered, f, threshold, check_invalid_depth, true, indices_to_keep);
      pcl::copyPointCloud (*to_be_filtered, indices_to_keep, *filtered);
      return filtered;
    }
  }
}

#ifdef PCL_NO_PRECOMPILE
#include <pcl/recognition/impl/hv/occlusion_reasoning.hpp>
#endif
