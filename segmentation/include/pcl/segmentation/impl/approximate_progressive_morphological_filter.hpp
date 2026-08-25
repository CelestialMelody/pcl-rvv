/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2009-2012, Willow Garage, Inc.
 *  Copyright (c) 2012-, Open Perception, Inc.
 *  Copyright (c) 2014, RadiantBlue Technologies, Inc.
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
 */

#ifndef PCL_SEGMENTATION_APPROXIMATE_PROGRESSIVE_MORPHOLOGICAL_FILTER_HPP_
#define PCL_SEGMENTATION_APPROXIMATE_PROGRESSIVE_MORPHOLOGICAL_FILTER_HPP_

#include <pcl/common/common.h>
#include <pcl/common/io.h>
#include <pcl/common/point_tests.h> // for isFinite
#include <pcl/filters/morphological_filter.h>
#include <pcl/segmentation/approximate_progressive_morphological_filter.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>

#include <cstdint>
#include <riscv_vector.h>
#include <vector>
#endif

#include <limits>

#ifdef __RVV10__
namespace pcl {
namespace detail {
namespace apmf_rvv {

inline vint32m2_t
floorF32ToI32NoFrm(const vfloat32m2_t values, const std::size_t vl)
{
  const vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2(values, vl);
  const vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2(trunc, vl);
  const vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16(values, trunc_f, vl);
  const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
  const vint32m2_t adjust = __riscv_vmerge_vxm_i32m2(zero, 1, negative_fraction, vl);
  return __riscv_vsub_vv_i32m2(trunc, adjust, vl);
}

inline vbool16_t
finiteMask(const vfloat32m2_t values, const std::size_t vl)
{
  const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16(values, values, vl);
  const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2(values, vl);
  const vfloat32m2_t inf_v = __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16(abs_v, inf_v, vl);
  return __riscv_vmand_mm_b16(eq_self, not_inf, vl);
}

template <typename PointT>
bool
computeGridZMinRVV(const pcl::PointCloud<PointT>& cloud,
                   const Eigen::Vector4f& global_min,
                   const float cell_size,
                   Eigen::MatrixXf& grid)
{
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
  if constexpr (!Layout::value) {
    return false;
  }
  else {
    const std::size_t n = cloud.size();
    if (n < 64 || cell_size == 0.0f)
      return false;

    const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
    const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
    std::vector<int> rows(max_vl);
    std::vector<int> cols(max_vl);
    std::vector<float> zs(max_vl);

    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      vfloat32m2_t vx;
      vfloat32m2_t vy;
      vfloat32m2_t vz;
      pcl::rvv_load::strided_load3_f32m2<sizeof(PointT),
                                         Layout::kX,
                                         Layout::kY,
                                         Layout::kZ>(base + i * sizeof(PointT),
                                                     vl,
                                                     vx,
                                                     vy,
                                                     vz);

      const vfloat32m2_t shifted_x = __riscv_vfsub_vf_f32m2(vx, global_min.x (), vl);
      const vfloat32m2_t shifted_y = __riscv_vfsub_vf_f32m2(vy, global_min.y (), vl);
      const vint32m2_t v_cols =
          floorF32ToI32NoFrm(__riscv_vfdiv_vf_f32m2(shifted_x, cell_size, vl), vl);
      const vint32m2_t v_rows =
          floorF32ToI32NoFrm(__riscv_vfdiv_vf_f32m2(shifted_y, cell_size, vl), vl);

      vbool16_t keep = __riscv_vmset_m_b16(vl);
      if (!cloud.is_dense) {
        keep = __riscv_vmand_mm_b16(__riscv_vmand_mm_b16(finiteMask(vx, vl),
                                                        finiteMask(vy, vl),
                                                        vl),
                                    finiteMask(vz, vl),
                                    vl);
      }

      const vint32m2_t rows_kept = __riscv_vcompress_vm_i32m2(v_rows, keep, vl);
      const vint32m2_t cols_kept = __riscv_vcompress_vm_i32m2(v_cols, keep, vl);
      const vfloat32m2_t zs_kept = __riscv_vcompress_vm_f32m2(vz, keep, vl);
      const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);
      __riscv_vse32_v_i32m2(rows.data(), rows_kept, keep_count);
      __riscv_vse32_v_i32m2(cols.data(), cols_kept, keep_count);
      __riscv_vse32_v_f32m2(zs.data(), zs_kept, keep_count);

      for (std::size_t lane = 0; lane < keep_count; ++lane) {
        const int row = rows[lane];
        const int col = cols[lane];
        if (row < 0 || row >= grid.rows() || col < 0 || col >= grid.cols())
          continue;
        if (zs[lane] < grid (row, col) || std::isnan (grid (row, col)))
          grid (row, col) = zs[lane];
      }

      i += vl;
    }
    return true;
  }
}

template <typename PointT>
bool
thresholdGroundRVV(const pcl::PointCloud<PointT>& cloud,
                   const pcl::Indices& ground,
                   const Eigen::Vector4f& global_min,
                   const float cell_size,
                   const Eigen::MatrixXf& filtered,
                   const float height_threshold,
                   pcl::Indices& out)
{
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
  if constexpr (!Layout::value) {
    return false;
  }
  else {
    using Pod = typename pcl::traits::POD<PointT>::type;
    const std::size_t n = ground.size();
    if (n < 64 || cell_size == 0.0f ||
        cloud.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>())
      return false;

    out.clear();
    out.reserve(n);
    const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
    const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
    std::vector<int> rows(max_vl);
    std::vector<int> cols(max_vl);
    std::vector<float> zs(max_vl);
    std::vector<int> source(max_vl);

    for (std::size_t i = 0; i < n;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vint32m2_t v_indices = __riscv_vle32_v_i32m2(ground.data() + i, vl);
      const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
      const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<Pod>(v_indices_u, vl);

      vfloat32m2_t vx;
      vfloat32m2_t vy;
      vfloat32m2_t vz;
      pcl::rvv_load::indexed_load3_f32m2<Pod,
                                         Layout::kX,
                                         Layout::kY,
                                         Layout::kZ>(base, offsets, vl, vx, vy, vz);

      const vfloat32m2_t shifted_x = __riscv_vfsub_vf_f32m2(vx, global_min.x (), vl);
      const vfloat32m2_t shifted_y = __riscv_vfsub_vf_f32m2(vy, global_min.y (), vl);
      const vint32m2_t v_cols =
          floorF32ToI32NoFrm(__riscv_vfdiv_vf_f32m2(shifted_x, cell_size, vl), vl);
      const vint32m2_t v_rows =
          floorF32ToI32NoFrm(__riscv_vfdiv_vf_f32m2(shifted_y, cell_size, vl), vl);

      __riscv_vse32_v_i32m2(rows.data(), v_rows, vl);
      __riscv_vse32_v_i32m2(cols.data(), v_cols, vl);
      __riscv_vse32_v_f32m2(zs.data(), vz, vl);
      __riscv_vse32_v_i32m2(source.data(), v_indices, vl);

      for (std::size_t lane = 0; lane < vl; ++lane) {
        const int row = rows[lane];
        const int col = cols[lane];
        if (row < 0 || row >= filtered.rows() || col < 0 || col >= filtered.cols())
          continue;
        if (zs[lane] - filtered (row, col) < height_threshold)
          out.push_back (source[lane]);
      }

      i += vl;
    }
    return true;
  }
}

} // namespace apmf_rvv
} // namespace detail
} // namespace pcl

#if defined(__GNUC__)
#define PCL_APMF_RVV_NOINLINE [[gnu::noinline]]
#else
#define PCL_APMF_RVV_NOINLINE
#endif
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT>
pcl::ApproximateProgressiveMorphologicalFilter<PointT>::ApproximateProgressiveMorphologicalFilter () = default;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT>
pcl::ApproximateProgressiveMorphologicalFilter<PointT>::~ApproximateProgressiveMorphologicalFilter () = default;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ApproximateProgressiveMorphologicalFilter<PointT>::extract (Indices& ground)
{
  bool segmentation_is_possible = initCompute ();
  if (!segmentation_is_possible)
  {
    deinitCompute ();
    return;
  }

#ifdef __RVV10__
  if (apmfExtractRVV (ground))
  {
    deinitCompute ();
    return;
  }
#endif

  extractStd (ground);
  deinitCompute ();
}

#ifdef __RVV10__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> PCL_APMF_RVV_NOINLINE bool
pcl::ApproximateProgressiveMorphologicalFilter<PointT>::apmfExtractRVV (Indices& ground)
{
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
  if constexpr (!Layout::value) {
    return false;
  }
  else {
    if (input_->size () < 64 ||
        input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>())
      return false;

    // Compute the series of window sizes and height thresholds
    std::vector<float> height_thresholds;
    std::vector<float> window_sizes;
    std::vector<int> half_sizes;
    int iteration = 0;
    float window_size = 0.0f;

    while (window_size < max_window_size_)
    {
      // Determine the initial window size.
      int half_size = (exponential_) ? (static_cast<int> (std::pow (static_cast<float> (base_), iteration))) : ((iteration+1) * base_);

      window_size = 2 * half_size + 1;

      // Calculate the height threshold to be used in the next iteration.
      float height_threshold = (iteration == 0) ? (initial_distance_) : (slope_ * (window_size - window_sizes[iteration-1]) * cell_size_ + initial_distance_);

      // Enforce max distance on height threshold
      if (height_threshold > max_distance_)
        height_threshold = max_distance_;

      half_sizes.push_back (half_size);
      window_sizes.push_back (window_size);
      height_thresholds.push_back (height_threshold);

      iteration++;
    }

    // setup grid based on scale and extents
    Eigen::Vector4f global_max, global_min;
    pcl::getMinMax3D<PointT> (*input_, global_min, global_max);

    float xextent = global_max.x () - global_min.x ();
    float yextent = global_max.y () - global_min.y ();

    int rows = static_cast<int> (std::floor (yextent / cell_size_) + 1);
    int cols = static_cast<int> (std::floor (xextent / cell_size_) + 1);

    Eigen::MatrixXf A (rows, cols);
    A.setConstant (std::numeric_limits<float>::quiet_NaN ());

    Eigen::MatrixXf Z (rows, cols);
    Z.setConstant (std::numeric_limits<float>::quiet_NaN ());

    Eigen::MatrixXf Zf (rows, cols);
    Zf.setConstant (std::numeric_limits<float>::quiet_NaN ());

    if (!pcl::detail::apmf_rvv::computeGridZMinRVV<PointT> (*input_, global_min, cell_size_, A))
      return false;

    // Ground indices are initially limited to those points in the input cloud we
    // wish to process
    if (input_->is_dense) {
      ground = *indices_;
    }
    else {
      ground.clear();
      ground.reserve(indices_->size());
      for (const auto& index: *indices_)
        if (pcl::isFinite((*input_)[index]))
          ground.push_back(index);
    }

    // Progressively filter ground returns using morphological open
    for (std::size_t i = 0; i < window_sizes.size (); ++i)
    {
      PCL_DEBUG ("      Iteration %d (height threshold = %f, window size = %f, half size = %d)...",
                 i, height_thresholds[i], window_sizes[i], half_sizes[i]);

      // Apply the morphological opening operation at the current window size.
  #pragma omp parallel for \
    default(none) \
    shared(A, cols, half_sizes, i, rows, Z) \
    num_threads(threads_)
      for (int row = 0; row < rows; ++row)
      {
        int rs, re;
        rs = ((row - half_sizes[i]) < 0) ? 0 : row - half_sizes[i];
        re = ((row + half_sizes[i]) > (rows-1)) ? (rows-1) : row + half_sizes[i];

        for (int col = 0; col < cols; ++col)
        {
          int cs, ce;
          cs = ((col - half_sizes[i]) < 0) ? 0 : col - half_sizes[i];
          ce = ((col + half_sizes[i]) > (cols-1)) ? (cols-1) : col + half_sizes[i];

          float min_coeff = std::numeric_limits<float>::max ();

          for (int j = rs; j < (re + 1); ++j)
          {
            for (int k = cs; k < (ce + 1); ++k)
            {
              if (A (j, k) != std::numeric_limits<float>::quiet_NaN ())
              {
                if (A (j, k) < min_coeff)
                  min_coeff = A (j, k);
              }
            }
          }

          if (min_coeff != std::numeric_limits<float>::max ())
            Z(row, col) = min_coeff;
        }
      }

  #pragma omp parallel for \
    default(none) \
    shared(cols, half_sizes, i, rows, Z, Zf) \
    num_threads(threads_)
      for (int row = 0; row < rows; ++row)
      {
        int rs, re;
        rs = ((row - half_sizes[i]) < 0) ? 0 : row - half_sizes[i];
        re = ((row + half_sizes[i]) > (rows-1)) ? (rows-1) : row + half_sizes[i];

        for (int col = 0; col < cols; ++col)
        {
          int cs, ce;
          cs = ((col - half_sizes[i]) < 0) ? 0 : col - half_sizes[i];
          ce = ((col + half_sizes[i]) > (cols-1)) ? (cols-1) : col + half_sizes[i];

          float max_coeff = -std::numeric_limits<float>::max ();

          for (int j = rs; j < (re + 1); ++j)
          {
            for (int k = cs; k < (ce + 1); ++k)
            {
              if (Z (j, k) != std::numeric_limits<float>::quiet_NaN ())
              {
                if (Z (j, k) > max_coeff)
                  max_coeff = Z (j, k);
              }
            }
          }

          if (max_coeff != -std::numeric_limits<float>::max ())
            Zf (row, col) = max_coeff;
        }
      }

      Indices pt_indices;
      if (!pcl::detail::apmf_rvv::thresholdGroundRVV<PointT> (*input_,
                                                             ground,
                                                             global_min,
                                                             cell_size_,
                                                             Zf,
                                                             height_thresholds[i],
                                                             pt_indices))
      {
        typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
        pcl::copyPointCloud<PointT> (*input_, ground, *cloud);

        for (std::size_t p_idx = 0; p_idx < ground.size (); ++p_idx)
        {
          const PointT& p = (*cloud)[p_idx];
          int erow = static_cast<int> (std::floor ((p.y - global_min.y ()) / cell_size_));
          int ecol = static_cast<int> (std::floor ((p.x - global_min.x ()) / cell_size_));

          float diff = p.z - Zf (erow, ecol);
          if (diff < height_thresholds[i])
            pt_indices.push_back (ground[p_idx]);
        }
      }

      A.swap (Zf);

      // Ground is now limited to pt_indices
      ground.swap (pt_indices);

      PCL_DEBUG ("ground now has %d points\n", ground.size ());
    }

    return true;
  }
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ApproximateProgressiveMorphologicalFilter<PointT>::extractStd (Indices& ground)
{
  // Compute the series of window sizes and height thresholds
  std::vector<float> height_thresholds;
  std::vector<float> window_sizes;
  std::vector<int> half_sizes;
  int iteration = 0;
  float window_size = 0.0f;

  while (window_size < max_window_size_)
  {
    // Determine the initial window size.
    int half_size = (exponential_) ? (static_cast<int> (std::pow (static_cast<float> (base_), iteration))) : ((iteration+1) * base_);

    window_size = 2 * half_size + 1;

    // Calculate the height threshold to be used in the next iteration.
    float height_threshold = (iteration == 0) ? (initial_distance_) : (slope_ * (window_size - window_sizes[iteration-1]) * cell_size_ + initial_distance_);

    // Enforce max distance on height threshold
    if (height_threshold > max_distance_)
      height_threshold = max_distance_;

    half_sizes.push_back (half_size);
    window_sizes.push_back (window_size);
    height_thresholds.push_back (height_threshold);

    iteration++;
  }

  // setup grid based on scale and extents
  Eigen::Vector4f global_max, global_min;
  pcl::getMinMax3D<PointT> (*input_, global_min, global_max);

  float xextent = global_max.x () - global_min.x ();
  float yextent = global_max.y () - global_min.y ();

  int rows = static_cast<int> (std::floor (yextent / cell_size_) + 1);
  int cols = static_cast<int> (std::floor (xextent / cell_size_) + 1);

  Eigen::MatrixXf A (rows, cols);
  A.setConstant (std::numeric_limits<float>::quiet_NaN ());

  Eigen::MatrixXf Z (rows, cols);
  Z.setConstant (std::numeric_limits<float>::quiet_NaN ());

  Eigen::MatrixXf Zf (rows, cols);
  Zf.setConstant (std::numeric_limits<float>::quiet_NaN ());

  if (input_->is_dense) {
#pragma omp parallel for \
  default(none) \
  shared(A, global_min) \
  num_threads(threads_)
    for (int i = 0; i < static_cast<int>(input_->size ()); ++i) {
      // ...then test for lower points within the cell
      const PointT& p = (*input_)[i];
      int row = std::floor((p.y - global_min.y ()) / cell_size_);
      int col = std::floor((p.x - global_min.x ()) / cell_size_);

      if (p.z < A (row, col) || std::isnan (A (row, col)))
        A (row, col) = p.z;
    }
  }
  else {
#pragma omp parallel for \
  default(none) \
  shared(A, global_min) \
  num_threads(threads_)
    for (int i = 0; i < static_cast<int>(input_->size ()); ++i) {
      // ...then test for lower points within the cell
      const PointT& p = (*input_)[i];
      if (!pcl::isFinite(p))
        continue;
      int row = std::floor((p.y - global_min.y ()) / cell_size_);
      int col = std::floor((p.x - global_min.x ()) / cell_size_);

      if (p.z < A (row, col) || std::isnan (A (row, col)))
        A (row, col) = p.z;
    }
  }

  // Ground indices are initially limited to those points in the input cloud we
  // wish to process
  if (input_->is_dense) {
    ground = *indices_;
  }
  else {
    ground.clear();
    ground.reserve(indices_->size());
    for (const auto& index: *indices_)
      if (pcl::isFinite((*input_)[index]))
        ground.push_back(index);
  }

  // Progressively filter ground returns using morphological open
  for (std::size_t i = 0; i < window_sizes.size (); ++i)
  {
    PCL_DEBUG ("      Iteration %d (height threshold = %f, window size = %f, half size = %d)...",
               i, height_thresholds[i], window_sizes[i], half_sizes[i]);

    // Limit filtering to those points currently considered ground returns
    typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
    pcl::copyPointCloud<PointT> (*input_, ground, *cloud);

    // Apply the morphological opening operation at the current window size.
#pragma omp parallel for \
  default(none) \
  shared(A, cols, half_sizes, i, rows, Z) \
  num_threads(threads_)
    for (int row = 0; row < rows; ++row)
    {
      int rs, re;
      rs = ((row - half_sizes[i]) < 0) ? 0 : row - half_sizes[i];
      re = ((row + half_sizes[i]) > (rows-1)) ? (rows-1) : row + half_sizes[i];

      for (int col = 0; col < cols; ++col)
      {
        int cs, ce;
        cs = ((col - half_sizes[i]) < 0) ? 0 : col - half_sizes[i];
        ce = ((col + half_sizes[i]) > (cols-1)) ? (cols-1) : col + half_sizes[i];

        float min_coeff = std::numeric_limits<float>::max ();

        for (int j = rs; j < (re + 1); ++j)
        {
          for (int k = cs; k < (ce + 1); ++k)
          {
            if (A (j, k) != std::numeric_limits<float>::quiet_NaN ())
            {
              if (A (j, k) < min_coeff)
                min_coeff = A (j, k);
            }
          }
        }

        if (min_coeff != std::numeric_limits<float>::max ())
          Z(row, col) = min_coeff;
      }
    }

#pragma omp parallel for \
  default(none) \
  shared(cols, half_sizes, i, rows, Z, Zf) \
  num_threads(threads_)
    for (int row = 0; row < rows; ++row)
    {
      int rs, re;
      rs = ((row - half_sizes[i]) < 0) ? 0 : row - half_sizes[i];
      re = ((row + half_sizes[i]) > (rows-1)) ? (rows-1) : row + half_sizes[i];

      for (int col = 0; col < cols; ++col)
      {
        int cs, ce;
        cs = ((col - half_sizes[i]) < 0) ? 0 : col - half_sizes[i];
        ce = ((col + half_sizes[i]) > (cols-1)) ? (cols-1) : col + half_sizes[i];

        float max_coeff = -std::numeric_limits<float>::max ();

        for (int j = rs; j < (re + 1); ++j)
        {
          for (int k = cs; k < (ce + 1); ++k)
          {
            if (Z (j, k) != std::numeric_limits<float>::quiet_NaN ())
            {
              if (Z (j, k) > max_coeff)
                max_coeff = Z (j, k);
            }
          }
        }

        if (max_coeff != -std::numeric_limits<float>::max ())
          Zf (row, col) = max_coeff;
      }
    }

    // Find indices of the points whose difference between the source and
    // filtered point clouds is less than the current height threshold.
    Indices pt_indices;
    for (std::size_t p_idx = 0; p_idx < ground.size (); ++p_idx)
    {
      const PointT& p = (*cloud)[p_idx];
      int erow = static_cast<int> (std::floor ((p.y - global_min.y ()) / cell_size_));
      int ecol = static_cast<int> (std::floor ((p.x - global_min.x ()) / cell_size_));

      float diff = p.z - Zf (erow, ecol);
      if (diff < height_thresholds[i])
        pt_indices.push_back (ground[p_idx]);
    }

    A.swap (Zf);

    // Ground is now limited to pt_indices
    ground.swap (pt_indices);

    PCL_DEBUG ("ground now has %d points\n", ground.size ());
  }
}


#define PCL_INSTANTIATE_ApproximateProgressiveMorphologicalFilter(T) template class PCL_EXPORTS pcl::ApproximateProgressiveMorphologicalFilter<T>;

#ifdef __RVV10__
#undef PCL_APMF_RVV_NOINLINE
#endif

#endif    // PCL_SEGMENTATION_APPROXIMATE_PROGRESSIVE_MORPHOLOGICAL_FILTER_HPP_
