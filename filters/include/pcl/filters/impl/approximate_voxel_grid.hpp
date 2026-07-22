/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
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
 * $Id: voxel_grid.hpp 1600 2011-07-07 16:55:51Z shapovalov $
 *
 */

#ifndef PCL_FILTERS_IMPL_FAST_VOXEL_GRID_H_
#define PCL_FILTERS_IMPL_FAST_VOXEL_GRID_H_

#include <pcl/common/io.h>
#include <pcl/common/point_tests.h>
#include <pcl/common/rvv_point_load.h>
#include <pcl/common/rvv_point_traits.h>
#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/point_types.h>
#include <boost/mpl/size.hpp> // for size

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ApproximateVoxelGrid<PointT>::flush (PointCloud &output, std::size_t op, he *hhe, int rgba_index, int centroid_size)
{
  hhe->centroid /= static_cast<float> (hhe->count);
  pcl::for_each_type <FieldList> (pcl::xNdCopyEigenPointFunctor <PointT> (hhe->centroid, output[op]));
  // ---[ RGB special case
  if (rgba_index >= 0)
  {
    // pack r/g/b into rgb
    float r = hhe->centroid[centroid_size-3], 
          g = hhe->centroid[centroid_size-2], 
          b = hhe->centroid[centroid_size-1];
    int rgb = (static_cast<int> (r)) << 16 | (static_cast<int> (g)) << 8 | (static_cast<int> (b));
    memcpy (reinterpret_cast<char*> (&output[op]) + rgba_index, &rgb, sizeof (float));
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ApproximateVoxelGrid<PointT>::applyFilterStd (PointCloud &output)
{
  int centroid_size = 4;
  if (downsample_all_data_)
    centroid_size = boost::mpl::size<FieldList>::value;

  // ---[ RGB special case
  std::vector<pcl::PCLPointField> fields;
  int rgba_index = -1;
  rgba_index = pcl::getFieldIndex<PointT> ("rgb", fields);
  if (rgba_index == -1)
    rgba_index = pcl::getFieldIndex<PointT> ("rgba", fields);
  if (rgba_index >= 0)
  {
    rgba_index = fields[rgba_index].offset;
    centroid_size += 3;
  }

  for (std::size_t i = 0; i < histsize_; i++) 
  {
    history_[i].count = 0;
    history_[i].centroid = Eigen::VectorXf::Zero (centroid_size);
  }
  Eigen::VectorXf scratch = Eigen::VectorXf::Zero (centroid_size);

  output.resize (input_->size ());   // size output for worst case
  std::size_t op = 0;    // output pointer
  for (const auto& point: *input_)
  {
    if(!pcl::isXYZFinite(point))
      continue;
    int ix = static_cast<int> (std::floor (point.x * inverse_leaf_size_[0]));
    int iy = static_cast<int> (std::floor (point.y * inverse_leaf_size_[1]));
    int iz = static_cast<int> (std::floor (point.z * inverse_leaf_size_[2]));
    auto hash = static_cast<unsigned int> ((ix * 7171 + iy * 3079 + iz * 4231) & (histsize_ - 1));
    he *hhe = &history_[hash];
    if (hhe->count && ((ix != hhe->ix) || (iy != hhe->iy) || (iz != hhe->iz))) 
    {
      flush (output, op++, hhe, rgba_index, centroid_size);
      hhe->count = 0;
      hhe->centroid.setZero ();// = Eigen::VectorXf::Zero (centroid_size);
    }
    hhe->ix = ix;
    hhe->iy = iy;
    hhe->iz = iz;
    hhe->count++;

    // Unpack the point into scratch, then accumulate
    // ---[ RGB special case
    if (rgba_index >= 0)
    {
      // fill r/g/b data
      pcl::RGB rgb;
      memcpy (&rgb, (reinterpret_cast<const char *> (&point)) + rgba_index, sizeof (RGB));
      scratch[centroid_size-3] = rgb.r;
      scratch[centroid_size-2] = rgb.g;
      scratch[centroid_size-1] = rgb.b;
    }
    pcl::for_each_type <FieldList> (xNdCopyPointEigenFunctor <PointT> (point, scratch));
    hhe->centroid += scratch;
  }
  for (std::size_t i = 0; i < histsize_; i++) 
  {
    he *hhe = &history_[i];
    if (hhe->count)
      flush (output, op++, hhe, rgba_index, centroid_size);
  }
  output.resize (op);
  output.width = output.size ();
  output.height       = 1;                    // downsampling breaks the organized structure
  output.is_dense     = true;                 // we filter out invalid points
}

#if defined(__RVV10__)
namespace pcl
{
  namespace approximate_voxel_grid_rvv
  {
    struct LeafHash
    {
      int ix;
      int iy;
      int iz;
      unsigned int hash;
      std::uint32_t source_index;
    };

    struct PointXYZHistoryEntry
    {
      int ix{0};
      int iy{0};
      int iz{0};
      int count{0};
      float sx{0.0f};
      float sy{0.0f};
      float sz{0.0f};
    };

    inline vint32m2_t
    floorF32ToI32NoFrm (vfloat32m2_t values, std::size_t vl)
    {
      // vfcvt.rtz is independent of FRM. Negative non-integers need one
      // extra step to match std::floor used by the scalar path.
      vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2 (values, vl);
      const vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2 (trunc, vl);
      const vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16 (values, trunc_f, vl);
      const vint32m2_t zero = __riscv_vmv_v_x_i32m2 (0, vl);
      const vint32m2_t adjust = __riscv_vmerge_vxm_i32m2 (zero, 1, negative_fraction, vl);
      return __riscv_vsub_vv_i32m2 (trunc, adjust, vl);
    }

    inline vbool16_t
    finiteMask (vfloat32m2_t values, std::size_t vl)
    {
      const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16 (values, values, vl);
      const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2 (values, vl);
      const vfloat32m2_t inf_v = __riscv_vfmv_v_f_f32m2 (std::numeric_limits<float>::infinity (), vl);
      const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16 (abs_v, inf_v, vl);
      return __riscv_vmand_mm_b16 (eq_self, not_inf, vl);
    }

    template <typename PointT>
    bool
    computeXYZLeafHashes (const pcl::PointCloud<PointT>& cloud,
                          const Eigen::Array3f& inverse_leaf_size,
                          std::size_t history_size,
                          std::vector<LeafHash>& out)
    {
      if constexpr (!pcl::rvv::kRVVXYZPointCompatible<PointT>)
      {
        return false;
      }
      else
      {
        const std::size_t n = cloud.size ();
        if (n < 64 || n > static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max ()) ||
            history_size == 0 || (history_size & (history_size - 1)) != 0)
          return false;

        out.resize (n);
        auto* out_leaf = out.data ();
        std::size_t kept = 0;
        std::size_t i = 0;
        const auto* base = reinterpret_cast<const std::uint8_t*> (cloud.data ());

        while (i < n)
        {
          const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
          const auto* chunk = base + i * sizeof (PointT);
          vfloat32m2_t vx;
          vfloat32m2_t vy;
          vfloat32m2_t vz;
          pcl::rvv_load::strided_load3_f32m2<sizeof (PointT),
                                             offsetof (PointT, x),
                                             offsetof (PointT, y),
                                             offsetof (PointT, z)> (chunk, vl, vx, vy, vz);

          const vbool16_t finite =
              __riscv_vmand_mm_b16 (__riscv_vmand_mm_b16 (finiteMask (vx, vl), finiteMask (vy, vl), vl),
                                    finiteMask (vz, vl),
                                    vl);
          const vfloat32m2_t sx = __riscv_vfmul_vf_f32m2 (vx, inverse_leaf_size[0], vl);
          const vfloat32m2_t sy = __riscv_vfmul_vf_f32m2 (vy, inverse_leaf_size[1], vl);
          const vfloat32m2_t sz = __riscv_vfmul_vf_f32m2 (vz, inverse_leaf_size[2], vl);
          const vint32m2_t ix = floorF32ToI32NoFrm (sx, vl);
          const vint32m2_t iy = floorF32ToI32NoFrm (sy, vl);
          const vint32m2_t iz = floorF32ToI32NoFrm (sz, vl);

          vint32m2_t hash = __riscv_vmul_vx_i32m2 (ix, 7171, vl);
          hash = __riscv_vmacc_vx_i32m2 (hash, 3079, iy, vl);
          hash = __riscv_vmacc_vx_i32m2 (hash, 4231, iz, vl);
          const vuint32m2_t hash_u = __riscv_vreinterpret_v_i32m2_u32m2 (hash);
          const vuint32m2_t masked_hash =
              __riscv_vand_vx_u32m2 (hash_u, static_cast<std::uint32_t> (history_size - 1), vl);

          const vuint32m2_t local = __riscv_vid_v_u32m2 (vl);
          const vuint32m2_t source = __riscv_vadd_vx_u32m2 (local, static_cast<std::uint32_t> (i), vl);

          const vint32m2_t ix_kept = __riscv_vcompress_vm_i32m2 (ix, finite, vl);
          const vint32m2_t iy_kept = __riscv_vcompress_vm_i32m2 (iy, finite, vl);
          const vint32m2_t iz_kept = __riscv_vcompress_vm_i32m2 (iz, finite, vl);
          const vuint32m2_t hash_kept = __riscv_vcompress_vm_u32m2 (masked_hash, finite, vl);
          const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2 (source, finite, vl);
          const std::size_t keep_count = __riscv_vcpop_m_b16 (finite, vl);

          for (std::size_t lane = 0; lane < keep_count; ++lane)
          {
            out_leaf[kept + lane].ix =
                __riscv_vmv_x_s_i32m2_i32 (__riscv_vslidedown_vx_i32m2 (ix_kept, lane, keep_count));
            out_leaf[kept + lane].iy =
                __riscv_vmv_x_s_i32m2_i32 (__riscv_vslidedown_vx_i32m2 (iy_kept, lane, keep_count));
            out_leaf[kept + lane].iz =
                __riscv_vmv_x_s_i32m2_i32 (__riscv_vslidedown_vx_i32m2 (iz_kept, lane, keep_count));
            out_leaf[kept + lane].hash =
                __riscv_vmv_x_s_u32m2_u32 (__riscv_vslidedown_vx_u32m2 (hash_kept, lane, keep_count));
            out_leaf[kept + lane].source_index =
                __riscv_vmv_x_s_u32m2_u32 (__riscv_vslidedown_vx_u32m2 (source_kept, lane, keep_count));
          }

          kept += keep_count;
          i += vl;
        }

        out.resize (kept);
        return true;
      }
    }

    inline void
    flushPointXYZHistoryEntry (pcl::PointCloud<pcl::PointXYZ>& output,
                               const PointXYZHistoryEntry& entry)
    {
      const float inv_count = 1.0f / static_cast<float> (entry.count);
      output.push_back (pcl::PointXYZ (entry.sx * inv_count,
                                       entry.sy * inv_count,
                                       entry.sz * inv_count));
    }
  }
}

template <typename PointT> bool
pcl::ApproximateVoxelGrid<PointT>::applyFilterPointXYZRVV (PointCloud &output)
{
  if constexpr (!std::is_same_v<PointT, pcl::PointXYZ>)
  {
    return false;
  }
  else
  {
    std::vector<approximate_voxel_grid_rvv::LeafHash> leaves;
    if (!approximate_voxel_grid_rvv::computeXYZLeafHashes (*input_,
                                                           inverse_leaf_size_,
                                                           histsize_,
                                                           leaves))
      return false;

    // PointXYZ has no extra fields or RGB packing. The stateful history table
    // remains scalar so collision flush order and centroid semantics match the
    // original ApproximateVoxelGrid loop while leaf/hash generation runs in VL
    // chunks.
    std::vector<approximate_voxel_grid_rvv::PointXYZHistoryEntry> history (histsize_);
    output.clear ();
    output.reserve (input_->size ());

    for (const auto& leaf : leaves)
    {
      const auto& point = (*input_)[leaf.source_index];
      auto& entry = history[leaf.hash];
      if (entry.count && ((leaf.ix != entry.ix) || (leaf.iy != entry.iy) || (leaf.iz != entry.iz)))
      {
        approximate_voxel_grid_rvv::flushPointXYZHistoryEntry (output, entry);
        entry = approximate_voxel_grid_rvv::PointXYZHistoryEntry {};
      }
      entry.ix = leaf.ix;
      entry.iy = leaf.iy;
      entry.iz = leaf.iz;
      entry.count++;
      entry.sx += point.x;
      entry.sy += point.y;
      entry.sz += point.z;
    }

    for (const auto& entry : history)
    {
      if (entry.count)
        approximate_voxel_grid_rvv::flushPointXYZHistoryEntry (output, entry);
    }

    output.width = output.size ();
    output.height = 1;
    output.is_dense = true;
    return true;
  }
}

template <typename PointT> bool
pcl::ApproximateVoxelGrid<PointT>::applyFilterXYZStagedRVV (PointCloud &output)
{
  if constexpr (!pcl::rvv::kRVVXYZPointCompatible<PointT> ||
                std::is_same_v<PointT, pcl::PointXYZ>)
  {
    return false;
  }
  else
  {
    std::vector<approximate_voxel_grid_rvv::LeafHash> leaves;
    if (!approximate_voxel_grid_rvv::computeXYZLeafHashes (*input_,
                                                           inverse_leaf_size_,
                                                           histsize_,
                                                           leaves))
      return false;

    int centroid_size = 4;
    if (downsample_all_data_)
      centroid_size = boost::mpl::size<FieldList>::value;

    std::vector<pcl::PCLPointField> fields;
    int rgba_index = pcl::getFieldIndex<PointT> ("rgb", fields);
    if (rgba_index == -1)
      rgba_index = pcl::getFieldIndex<PointT> ("rgba", fields);
    if (rgba_index >= 0)
    {
      rgba_index = fields[rgba_index].offset;
      centroid_size += 3;
    }

    for (std::size_t i = 0; i < histsize_; i++)
    {
      history_[i].count = 0;
      history_[i].centroid = Eigen::VectorXf::Zero (centroid_size);
    }
    Eigen::VectorXf scratch = Eigen::VectorXf::Zero (centroid_size);

    output.resize (input_->size ());
    std::size_t op = 0;
    for (const auto& leaf : leaves)
    {
      const auto& point = (*input_)[leaf.source_index];
      he *hhe = &history_[leaf.hash];
      if (hhe->count && ((leaf.ix != hhe->ix) || (leaf.iy != hhe->iy) || (leaf.iz != hhe->iz)))
      {
        flush (output, op++, hhe, rgba_index, centroid_size);
        hhe->count = 0;
        hhe->centroid.setZero ();
      }
      hhe->ix = leaf.ix;
      hhe->iy = leaf.iy;
      hhe->iz = leaf.iz;
      hhe->count++;

      if (rgba_index >= 0)
      {
        pcl::RGB rgb;
        memcpy (&rgb, (reinterpret_cast<const char *> (&point)) + rgba_index, sizeof (RGB));
        scratch[centroid_size-3] = rgb.r;
        scratch[centroid_size-2] = rgb.g;
        scratch[centroid_size-1] = rgb.b;
      }
      pcl::for_each_type <FieldList> (xNdCopyPointEigenFunctor <PointT> (point, scratch));
      hhe->centroid += scratch;
    }

    for (std::size_t i = 0; i < histsize_; i++)
    {
      he *hhe = &history_[i];
      if (hhe->count)
        flush (output, op++, hhe, rgba_index, centroid_size);
    }
    output.resize (op);
    output.width = output.size ();
    output.height = 1;
    output.is_dense = true;
    return true;
  }
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ApproximateVoxelGrid<PointT>::applyFilter (PointCloud &output)
{
#if defined(__RVV10__)
  if constexpr (std::is_same_v<PointT, pcl::PointXYZ>)
  {
    if (applyFilterPointXYZRVV (output))
      return;
  }
  else if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)
  {
    if (applyFilterXYZStagedRVV (output))
      return;
  }
#endif
  applyFilterStd (output);
}

#define PCL_INSTANTIATE_ApproximateVoxelGrid(T) template class PCL_EXPORTS pcl::ApproximateVoxelGrid<T>;

#endif    // PCL_FILTERS_IMPL_FAST_VOXEL_GRID_H_
