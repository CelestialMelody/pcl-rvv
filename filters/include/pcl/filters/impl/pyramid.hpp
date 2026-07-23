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
 * $Id$
 *
 */


#ifndef PCL_FILTERS_IMPL_PYRAMID_HPP
#define PCL_FILTERS_IMPL_PYRAMID_HPP

#include <pcl/common/distances.h>
#include <pcl/common/point_tests.h>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/filters/pyramid.h>
#include <pcl/console/print.h>
#include <pcl/point_types.h>

#include <type_traits>

namespace pcl
{
namespace filters
{
template <typename PointT> bool
Pyramid<PointT>::initCompute ()
{
  if (!input_->isOrganized ())
  {
    PCL_ERROR ("[pcl::filters::%s::initCompute] Number of levels should be at least 2!\n", getClassName ().c_str ());
    return (false);
  }

  if (levels_ < 2)
  {
    PCL_ERROR ("[pcl::filters::%s::initCompute] Number of levels should be at least 2!\n", getClassName ().c_str ());
    return (false);
  }

  // std::size_t ratio (std::pow (2, levels_));
  // std::size_t last_width = input_->width / ratio;
  // std::size_t last_height = input_->height / ratio;

  if (levels_ > 4)
  {
    PCL_ERROR ("[pcl::filters::%s::initCompute] Number of levels should not exceed 4!\n", getClassName ().c_str ());
    return (false);
  }

  if (large_)
  {
    Eigen::VectorXf k (5);
    k << 1.f/16.f, 1.f/4.f, 3.f/8.f, 1.f/4.f, 1.f/16.f;
    kernel_ = k * k.transpose ();
    if (threshold_ != std::numeric_limits<float>::infinity ())
      threshold_ *= 2 * threshold_;

  }
  else
  {
    Eigen::VectorXf k (3);
    k << 1.f/4.f, 1.f/2.f, 1.f/4.f;
    kernel_ = k * k.transpose ();
    if (threshold_ != std::numeric_limits<float>::infinity ())
      threshold_ *= threshold_;
  }

  return (true);
}

#ifdef __RVV10__
template <typename PointT>
inline constexpr bool kPyramidPointXYZDenseRVVCompatible =
    std::is_same_v<PointT, pcl::PointXYZ> &&
    pcl::rvv::kRVVXYZPointCompatible<PointT> &&
    pcl::rvv::RVVXYZFloatLayout<PointT>::value;

inline void
pyramidPointXYZDenseLevelRVV (const PointCloud<PointXYZ> &previous,
                              PointCloud<PointXYZ> &next,
                              const Eigen::MatrixXf &kernel,
                              const int kernel_rows,
                              const int kernel_cols,
                              const int kernel_center_x,
                              const int kernel_center_y)
{
  using Layout = pcl::rvv::RVVXYZFloatLayout<PointXYZ>;
  static_assert (kPyramidPointXYZDenseRVVCompatible<PointXYZ>,
                 "Pyramid RVV production path is intentionally exact PointXYZ with xyz float layout");

  constexpr std::size_t kStride = sizeof (PointXYZ);
  constexpr std::size_t kXOff = Layout::kX;
  constexpr std::size_t kYOff = Layout::kY;
  constexpr std::size_t kZOff = Layout::kZ;

  const std::uint8_t *previous_base = reinterpret_cast<const std::uint8_t*> (previous.points.data ());
  const std::uint8_t *next_base = reinterpret_cast<const std::uint8_t*> (next.points.data ());
  const int next_width = static_cast<int> (next.width);
  const int previous_width = static_cast<int> (previous.width);
  const int previous_height = static_cast<int> (previous.height);

  for (int i=0; i < static_cast<int> (next.height); ++i)
  {
    int j = 0;
    for (; j < next_width; )
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (next_width - j));
      vfloat32m2_t vx = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
      vfloat32m2_t vy = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
      vfloat32m2_t vz = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
      const vuint32m2_t vj = __riscv_vid_v_u32m2 (vl);

      for (int m=0; m < kernel_rows; ++m)
      {
        const int mm = kernel_rows - 1 - m;
        int ii = 2*i + (m - kernel_center_y);
        if (ii < 0) ii = 0;
        if (ii >= previous_height) ii = previous_height - 1;

        for (int n=0; n < kernel_cols; ++n)
        {
          const int nn = kernel_cols - 1 - n;
          int jj_base = 2*j + (n - kernel_center_x);
          const float k = kernel (mm, nn);

          vuint32m2_t vjj = __riscv_vadd_vv_u32m2 (vj, vj, vl);
          vjj = __riscv_vadd_vx_u32m2 (vjj, static_cast<unsigned> (jj_base), vl);
          const vbool16_t low = __riscv_vmslt_vx_i32m2_b16 (__riscv_vreinterpret_v_u32m2_i32m2 (vjj), 0, vl);
          vjj = __riscv_vmerge_vxm_u32m2 (vjj, 0u, low, vl);
          const vbool16_t high = __riscv_vmsgeu_vx_u32m2_b16 (vjj, static_cast<unsigned> (previous_width), vl);
          vjj = __riscv_vmerge_vxm_u32m2 (vjj, static_cast<unsigned> (previous_width - 1), high, vl);

          const vuint32m2_t v_index = __riscv_vadd_vx_u32m2 (vjj, static_cast<unsigned> (ii * previous_width), vl);
          const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<PointXYZ> (v_index, vl);
          vfloat32m2_t px, py, pz;
          pcl::rvv_load::indexed_load3_f32m2<PointXYZ, kXOff, kYOff, kZOff> (previous_base, v_off, vl, px, py, pz);
          vx = __riscv_vfmacc_vf_f32m2 (vx, k, px, vl);
          vy = __riscv_vfmacc_vf_f32m2 (vy, k, py, vl);
          vz = __riscv_vfmacc_vf_f32m2 (vz, k, pz, vl);
        }
      }

      const std::size_t out_offset = static_cast<std::size_t> (i * next_width + j) * sizeof (PointXYZ);
      // Each VL chunk writes consecutive output columns; input columns are 2*j+n and are gathered
      // because the downsampling stride is two and boundary clamping can duplicate edge samples.
      pcl::rvv_store::strided_store3_f32m2<kStride, kXOff, kYOff, kZOff> (next_base + out_offset, vl, vx, vy, vz);
      j += static_cast<int> (vl);
    }
  }
}

inline void
pyramidPointXYZDenseRVV (const PointCloud<PointXYZ> &input,
                         const int levels,
                         const Eigen::MatrixXf &kernel,
                         std::vector<Pyramid<PointXYZ>::PointCloudPtr>& output)
{
  const int kernel_rows = static_cast<int> (kernel.rows ());
  const int kernel_cols = static_cast<int> (kernel.cols ());
  const int kernel_center_x = kernel_cols / 2;
  const int kernel_center_y = kernel_rows / 2;

  output.resize (levels + 1);
  output[0].reset (new pcl::PointCloud<PointXYZ>);
  *(output[0]) = input;

  for (int l = 1; l <= levels; ++l)
  {
    output[l].reset (new pcl::PointCloud<PointXYZ> (output[l-1]->width/2, output[l-1]->height/2));
    pyramidPointXYZDenseLevelRVV (*output[l-1],
                                  *output[l],
                                  kernel,
                                  kernel_rows,
                                  kernel_cols,
                                  kernel_center_x,
                                  kernel_center_y);
  }
}
#endif

template <typename PointT> void
Pyramid<PointT>::computeStd (std::vector<PointCloudPtr>& output)
{
  using namespace pcl::common;

  int kernel_rows = static_cast<int> (kernel_.rows ());
  int kernel_cols = static_cast<int> (kernel_.cols ());
  int kernel_center_x = kernel_cols / 2;
  int kernel_center_y = kernel_rows / 2;

  output.resize (levels_ + 1);
  output[0].reset (new pcl::PointCloud<PointT>);
  *(output[0]) = *input_;

  if (input_->is_dense)
  {
    for (int l = 1; l <= levels_; ++l)
    {
      output[l].reset (new pcl::PointCloud<PointT> (output[l-1]->width/2, output[l-1]->height/2));
      const PointCloud<PointT> &previous = *output[l-1];
      PointCloud<PointT> &next = *output[l];
#pragma omp parallel for \
  default(none)          \
  shared(next)           \
  num_threads(threads_)
      for(int i=0; i < next.height; ++i)
      {
        for(int j=0; j < next.width; ++j)
        {
          for(int m=0; m < kernel_rows; ++m)
          {
            int mm = kernel_rows - 1 - m;
            for(int n=0; n < kernel_cols; ++n)
            {
              int nn = kernel_cols - 1 - n;

              int ii = 2*i + (m - kernel_center_y);
              int jj = 2*j + (n - kernel_center_x);

              if (ii < 0) ii = 0;
              if (ii >= previous.height) ii = previous.height - 1;
              if (jj < 0) jj = 0;
              if (jj >= previous.width) jj = previous.width - 1;
              next.at (j,i) += previous.at (jj,ii) * kernel_ (mm,nn);
            }
          }
        }
      }
    }
  }
  else
  {
    for (int l = 1; l <= levels_; ++l)
    {
      output[l].reset (new pcl::PointCloud<PointT> (output[l-1]->width/2, output[l-1]->height/2));
      const PointCloud<PointT> &previous = *output[l-1];
      PointCloud<PointT> &next = *output[l];
#pragma omp parallel for \
  default(none)          \
  shared(next)           \
  num_threads(threads_)
      for(int i=0; i < next.height; ++i)
      {
        for(int j=0; j < next.width; ++j)
        {
          float weight = 0;
          for(int m=0; m < kernel_rows; ++m)
          {
            int mm = kernel_rows - 1 - m;
            for(int n=0; n < kernel_cols; ++n)
            {
              int nn = kernel_cols - 1 - n;
              int ii = 2*i + (m - kernel_center_y);
              int jj = 2*j + (n - kernel_center_x);
              if (ii < 0) ii = 0;
              if (ii >= previous.height) ii = previous.height - 1;
              if (jj < 0) jj = 0;
              if (jj >= previous.width) jj = previous.width - 1;
              if (!pcl::isFinite (previous.at (jj,ii)))
                continue;
              if (pcl::squaredEuclideanDistance (previous.at (2*j,2*i), previous.at (jj,ii)) < threshold_)
              {
                next.at (j,i) += previous.at (jj,ii).x * kernel_ (mm,nn);
                weight+= kernel_ (mm,nn);
              }
            }
          }
          if (weight == 0)
            nullify (next.at (j,i));
          else
          {
            weight = 1.f/weight;
            next.at (j,i)*= weight;
          }
        }
      }
    }
  }
}

template <typename PointT> void
Pyramid<PointT>::compute (std::vector<PointCloudPtr>& output)
{
  if (!initCompute ())
  {
    PCL_ERROR ("[pcl::%s::compute] initCompute failed!\n", getClassName ().c_str ());
    return;
  }

#ifdef __RVV10__
  if constexpr (kPyramidPointXYZDenseRVVCompatible<PointT>)
  {
    if (input_->is_dense && !large_ && threads_ <= 1 && input_->width >= 8 && input_->height >= 2)
    {
      // Dense PointXYZ small-kernel keeps scalar semantics: same flipped 3x3 kernel and boundary clamp;
      // explicit multi-thread, 5x5 large-kernel, non-dense threshold/weight paths and RGB specializations
      // remain scalar fallbacks.
      pyramidPointXYZDenseRVV (*input_, levels_, kernel_, output);
      return;
    }
  }
#endif

  computeStd (output);
}

template <> void
Pyramid<pcl::PointXYZRGB>::compute (std::vector<Pyramid<pcl::PointXYZRGB>::PointCloudPtr> &output);

template <> void
Pyramid<pcl::PointXYZRGBA>::compute (std::vector<Pyramid<pcl::PointXYZRGBA>::PointCloudPtr> &output);

template<> void
Pyramid<pcl::RGB>::nullify (pcl::RGB& p) const
{
  p.r = 0; p.g = 0; p.b = 0;
}

template <> void
Pyramid<pcl::RGB>::compute (std::vector<Pyramid<pcl::RGB>::PointCloudPtr> &output);

} // namespace filters
} // namespace pcl

#endif
