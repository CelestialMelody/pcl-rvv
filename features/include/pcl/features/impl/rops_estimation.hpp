/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
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
 *
 * Author : Sergey Ushakov
 * Email  : sergey.s.ushakov@mail.ru
 *
 */

#ifndef PCL_ROPS_ESTIMATION_HPP_
#define PCL_ROPS_ESTIMATION_HPP_

#include <pcl/features/rops_estimation.h>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#endif

#include <array>
#include <Eigen/Eigenvalues> // for EigenSolver
#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric> // for accumulate
#include <type_traits>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

#if defined(__RVV10__) && defined(PCL_RVV_ROPS_ENABLE_TEST_TRACE)
#include <cstddef>
extern "C" std::size_t pcl_rvv_rops_rotate_cloud_trace_hits;
extern "C" std::size_t pcl_rvv_rops_distribution_matrix_trace_hits;
#endif

#ifdef __RVV10__
namespace pcl::detail::rops
{
  constexpr std::size_t kRopsRVVMinPoints = 16;

  inline float
  reduceMinF32m2 (const vfloat32m2_t values, const std::size_t vl)
  {
    const vfloat32m1_t seed =
        __riscv_vfmv_s_f_f32m1 (std::numeric_limits<float>::max (), 1);
    const vfloat32m1_t reduced =
        __riscv_vfredmin_vs_f32m2_f32m1 (values, seed, vl);
    return (__riscv_vfmv_f_s_f32m1_f32 (reduced));
  }

  inline float
  reduceMaxF32m2 (const vfloat32m2_t values, const std::size_t vl)
  {
    const vfloat32m1_t seed =
        __riscv_vfmv_s_f_f32m1 (-std::numeric_limits<float>::max (), 1);
    const vfloat32m1_t reduced =
        __riscv_vfredmax_vs_f32m2_f32m1 (values, seed, vl);
    return (__riscv_vfmv_f_s_f32m1_f32 (reduced));
  }

  template <typename PointT> bool
  rotateCloudRVV (const PointT& axis,
                  const float angle,
                  const pcl::PointCloud<PointT>& cloud,
                  pcl::PointCloud<PointT>& rotated_cloud,
                  Eigen::Vector3f& min,
                  Eigen::Vector3f& max)
  {
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    if constexpr (!Layout::value)
      return (false);
    else
    {
      const std::size_t number_of_points = cloud.size ();
      if (number_of_points < kRopsRVVMinPoints || !cloud.is_dense)
        return (false);

      const float x = axis.x;
      const float y = axis.y;
      const float z = axis.z;
      const float rad = M_PI / 180.0f;
      const float cosine = std::cos (angle * rad);
      const float sine = std::sin (angle * rad);
      const float one_minus_cosine = 1.0f - cosine;
      const float r00 = cosine + one_minus_cosine * x * x;
      const float r01 = one_minus_cosine * x * y - sine * z;
      const float r02 = one_minus_cosine * x * z + sine * y;
      const float r10 = one_minus_cosine * y * x + sine * z;
      const float r11 = cosine + one_minus_cosine * y * y;
      const float r12 = one_minus_cosine * y * z - sine * x;
      const float r20 = one_minus_cosine * z * x - sine * y;
      const float r21 = one_minus_cosine * z * y + sine * x;
      const float r22 = cosine + one_minus_cosine * z * z;

      rotated_cloud.header = cloud.header;
      rotated_cloud.width = static_cast<std::uint32_t> (number_of_points);
      rotated_cloud.height = 1;
      rotated_cloud.is_dense = cloud.is_dense;
      rotated_cloud.clear ();
      rotated_cloud.resize (number_of_points);

      const std::size_t vlmax = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (-1));
      const float init_min = std::numeric_limits<float>::max ();
      const float init_max = -std::numeric_limits<float>::max ();
      vfloat32m2_t v_min_x = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
      vfloat32m2_t v_min_y = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
      vfloat32m2_t v_min_z = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
      vfloat32m2_t v_max_x = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
      vfloat32m2_t v_max_y = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
      vfloat32m2_t v_max_z = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);

      const auto* input_base =
          reinterpret_cast<const std::uint8_t*> (cloud.points.data ());
      auto* output_base =
          reinterpret_cast<std::uint8_t*> (rotated_cloud.points.data ());

      for (std::size_t i = 0; i < number_of_points;)
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (number_of_points - i);
        vfloat32m2_t vx;
        vfloat32m2_t vy;
        vfloat32m2_t vz;
        pcl::rvv_load::strided_load3_f32m2<sizeof (PointT), Layout::kX, Layout::kY, Layout::kZ> (
            input_base + i * sizeof (PointT), vl, vx, vy, vz);

        vfloat32m2_t rx = __riscv_vfmul_vf_f32m2 (vx, r00, vl);
        rx = __riscv_vfmacc_vf_f32m2 (rx, r01, vy, vl);
        rx = __riscv_vfmacc_vf_f32m2 (rx, r02, vz, vl);
        vfloat32m2_t ry = __riscv_vfmul_vf_f32m2 (vx, r10, vl);
        ry = __riscv_vfmacc_vf_f32m2 (ry, r11, vy, vl);
        ry = __riscv_vfmacc_vf_f32m2 (ry, r12, vz, vl);
        vfloat32m2_t rz = __riscv_vfmul_vf_f32m2 (vx, r20, vl);
        rz = __riscv_vfmacc_vf_f32m2 (rz, r21, vy, vl);
        rz = __riscv_vfmacc_vf_f32m2 (rz, r22, vz, vl);

        pcl::rvv_store::strided_store3_f32m2<sizeof (PointT), Layout::kX, Layout::kY, Layout::kZ> (
            output_base + i * sizeof (PointT), vl, rx, ry, rz);

        v_min_x = __riscv_vfmin_vv_f32m2_tu (v_min_x, v_min_x, rx, vl);
        v_min_y = __riscv_vfmin_vv_f32m2_tu (v_min_y, v_min_y, ry, vl);
        v_min_z = __riscv_vfmin_vv_f32m2_tu (v_min_z, v_min_z, rz, vl);
        v_max_x = __riscv_vfmax_vv_f32m2_tu (v_max_x, v_max_x, rx, vl);
        v_max_y = __riscv_vfmax_vv_f32m2_tu (v_max_y, v_max_y, ry, vl);
        v_max_z = __riscv_vfmax_vv_f32m2_tu (v_max_z, v_max_z, rz, vl);

        i += vl;
      }

      min (0) = reduceMinF32m2 (v_min_x, vlmax);
      min (1) = reduceMinF32m2 (v_min_y, vlmax);
      min (2) = reduceMinF32m2 (v_min_z, vlmax);
      max (0) = reduceMaxF32m2 (v_max_x, vlmax);
      max (1) = reduceMaxF32m2 (v_max_y, vlmax);
      max (2) = reduceMaxF32m2 (v_max_z, vlmax);

#if defined(PCL_RVV_ROPS_ENABLE_TEST_TRACE)
      pcl_rvv_rops_rotate_cloud_trace_hits += number_of_points;
#endif
      return (true);
    }
  }

  template <typename PointT> bool
  getDistributionMatrixRVV (const unsigned int projection,
                            const Eigen::Vector3f& min,
                            const Eigen::Vector3f& max,
                            const pcl::PointCloud<PointT>& cloud,
                            const unsigned int number_of_bins,
                            Eigen::MatrixXf& matrix)
  {
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    if constexpr (!Layout::value)
      return (false);
    else
    {
      const std::size_t count = cloud.size ();
      if (count < kRopsRVVMinPoints || !cloud.is_dense || number_of_bins == 0 ||
          projection >= 3)
        return (false);

      const unsigned int coord[3][2] = {
        {0, 1},
        {0, 2},
        {1, 2}};
      const float u_min = min[coord[projection][0]];
      const float v_min = min[coord[projection][1]];
      const float u_extent = max (coord[projection][0]) - u_min;
      const float v_extent = max (coord[projection][1]) - v_min;
      if (u_extent <= std::numeric_limits<float>::epsilon () ||
          v_extent <= std::numeric_limits<float>::epsilon ())
        return (false);

      matrix.setZero ();
      const float inv_u_bin_length = static_cast<float> (number_of_bins) / u_extent;
      const float inv_v_bin_length = static_cast<float> (number_of_bins) / v_extent;
      std::vector<std::uint32_t> rows (count);
      std::vector<std::uint32_t> cols (count);
      const auto* base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());

      for (std::size_t i = 0; i < count;)
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (count - i);
        vfloat32m2_t vx;
        vfloat32m2_t vy;
        vfloat32m2_t vz;
        pcl::rvv_load::strided_load3_f32m2<sizeof (PointT), Layout::kX, Layout::kY, Layout::kZ> (
            base + i * sizeof (PointT), vl, vx, vy, vz);

        vfloat32m2_t v_u = vx;
        vfloat32m2_t v_v = vy;
        if (projection == 1)
          v_v = vz;
        else if (projection == 2)
        {
          v_u = vy;
          v_v = vz;
        }

        v_u = __riscv_vfmul_vf_f32m2 (
            __riscv_vfsub_vf_f32m2 (v_u, u_min, vl), inv_u_bin_length, vl);
        v_v = __riscv_vfmul_vf_f32m2 (
            __riscv_vfsub_vf_f32m2 (v_v, v_min, vl), inv_v_bin_length, vl);
        vuint32m2_t v_row = __riscv_vfcvt_rtz_xu_f_v_u32m2 (v_u, vl);
        vuint32m2_t v_col = __riscv_vfcvt_rtz_xu_f_v_u32m2 (v_v, vl);
        const vuint32m2_t v_last_bin = __riscv_vmv_v_x_u32m2 (number_of_bins - 1, vl);
        const vuint32m2_t v_bins = __riscv_vmv_v_x_u32m2 (number_of_bins, vl);
        v_row = __riscv_vminu_vv_u32m2 (v_row, v_last_bin, vl);
        v_col = __riscv_vminu_vv_u32m2 (v_col, v_last_bin, vl);
        v_row = __riscv_vmerge_vvm_u32m2 (
            v_row, v_last_bin, __riscv_vmsgeu_vv_u32m2_b16 (v_row, v_bins, vl), vl);
        v_col = __riscv_vmerge_vvm_u32m2 (
            v_col, v_last_bin, __riscv_vmsgeu_vv_u32m2_b16 (v_col, v_bins, vl), vl);
        __riscv_vse32_v_u32m2 (rows.data () + i, v_row, vl);
        __riscv_vse32_v_u32m2 (cols.data () + i, v_col, vl);
        i += vl;
      }

      for (std::size_t i = 0; i < count; ++i)
        matrix (rows[i], cols[i]) += 1.0f;
      matrix /= std::max<float> (1.0f, static_cast<float> (count));

#if defined(PCL_RVV_ROPS_ENABLE_TEST_TRACE)
      pcl_rvv_rops_distribution_matrix_trace_hits += count;
#endif
      return (true);
    }
  }
} // namespace pcl::detail::rops
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT>
pcl::ROPSEstimation <PointInT, PointOutT>::ROPSEstimation () :

  triangles_ (0),
  triangles_of_the_point_ (0)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT>
pcl::ROPSEstimation <PointInT, PointOutT>::~ROPSEstimation ()
{
  triangles_.clear ();
  triangles_of_the_point_.clear ();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::setNumberOfPartitionBins (unsigned int number_of_bins)
{
  if (number_of_bins != 0)
    number_of_bins_ = number_of_bins;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> unsigned int
pcl::ROPSEstimation <PointInT, PointOutT>::getNumberOfPartitionBins () const
{
  return (number_of_bins_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::setNumberOfRotations (unsigned int number_of_rotations)
{
  if (number_of_rotations != 0)
  {
    number_of_rotations_ = number_of_rotations;
    step_ = 90.0f / static_cast <float> (number_of_rotations_ + 1);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> unsigned int
pcl::ROPSEstimation <PointInT, PointOutT>::getNumberOfRotations () const
{
  return (number_of_rotations_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::setSupportRadius (float support_radius)
{
  if (support_radius > 0.0f)
  {
    support_radius_ = support_radius;
    sqr_support_radius_ = support_radius * support_radius;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> float
pcl::ROPSEstimation <PointInT, PointOutT>::getSupportRadius () const
{
  return (support_radius_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::setTriangles (const std::vector <pcl::Vertices>& triangles)
{
  triangles_ = triangles;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::getTriangles (std::vector <pcl::Vertices>& triangles) const
{
  triangles = triangles_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::computeFeature (PointCloudOut &output)
{
  if (triangles_.empty ())
  {
    output.clear ();
    return;
  }

  buildListOfPointsTriangles ();

  //feature size = number_of_rotations * number_of_axis_to_rotate_around * number_of_projections * number_of_central_moments
  unsigned int feature_size = number_of_rotations_ * 3 * 3 * 5;
  const auto number_of_points = indices_->size ();
  output.clear ();
  output.reserve (number_of_points);

  for (const auto& idx: *indices_)
  {
    std::set <unsigned int> local_triangles;
    pcl::Indices local_points;
    getLocalSurface ((*input_)[idx], local_triangles, local_points);

    Eigen::Matrix3f lrf_matrix;
    computeLRF ((*input_)[idx], local_triangles, lrf_matrix);

    PointCloudIn transformed_cloud;
    transformCloud ((*input_)[idx], lrf_matrix, local_points, transformed_cloud);

    std::array<PointInT, 3> axes;
    axes[0].x = 1.0f; axes[0].y = 0.0f; axes[0].z = 0.0f;
    axes[1].x = 0.0f; axes[1].y = 1.0f; axes[1].z = 0.0f;
    axes[2].x = 0.0f; axes[2].y = 0.0f; axes[2].z = 1.0f;
    std::vector <float> feature;
    for (const auto &axis : axes)
    {
      float theta = step_;
      do
      {
        //rotate local surface and get bounding box
        PointCloudIn rotated_cloud;
        Eigen::Vector3f min, max;
        rotateCloud (axis, theta, transformed_cloud, rotated_cloud, min, max);

        //for each projection (XY, XZ and YZ) compute distribution matrix and central moments
        for (unsigned int i_proj = 0; i_proj < 3; i_proj++)
        {
          Eigen::MatrixXf distribution_matrix;
          distribution_matrix.resize (number_of_bins_, number_of_bins_);
          getDistributionMatrix (i_proj, min, max, rotated_cloud, distribution_matrix);

          // TODO remove this needless copy due to API design
          std::vector <float> moments;
          computeCentralMoments (distribution_matrix, moments);

          feature.insert (feature.end (), moments.begin (), moments.end ());
        }

        theta += step_;
      } while (theta < 90.0f);
    }

    const float norm = std::accumulate(
        feature.cbegin(), feature.cend(), 0.f, [](const auto& sum, const auto& val) {
          return sum + std::abs(val);
        });
    float invert_norm;
    if (norm < std::numeric_limits <float>::epsilon ())
      invert_norm = 1.0f;
    else
      invert_norm = 1.0f / norm;

    output.emplace_back ();
    for (std::size_t i_dim = 0; i_dim < feature_size; i_dim++)
      output.back().histogram[i_dim] = feature[i_dim] * invert_norm;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::buildListOfPointsTriangles ()
{
  triangles_of_the_point_.clear ();

  std::vector <unsigned int> dummy;
  dummy.reserve (100);
  triangles_of_the_point_.resize (surface_->points. size (), dummy);

  for (std::size_t i_triangle = 0; i_triangle < triangles_.size (); i_triangle++)
    for (const auto& vertex: triangles_[i_triangle].vertices)
      triangles_of_the_point_[vertex].push_back (i_triangle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::getLocalSurface (const PointInT& point, std::set <unsigned int>& local_triangles, pcl::Indices& local_points) const
{
  std::vector <float> distances;
  tree_->radiusSearch (point, support_radius_, local_points, distances);

  for (const auto& pt: local_points)
    local_triangles.insert (triangles_of_the_point_[pt].begin (),
                            triangles_of_the_point_[pt].end ());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::computeLRF (const PointInT& point, const std::set <unsigned int>& local_triangles, Eigen::Matrix3f& lrf_matrix) const
{
  std::size_t number_of_triangles = local_triangles.size ();

  std::vector<Eigen::Matrix3f, Eigen::aligned_allocator<Eigen::Matrix3f> > scatter_matrices;
  std::vector <float> triangle_area (number_of_triangles), distance_weight (number_of_triangles);

  scatter_matrices.reserve (number_of_triangles);
  triangle_area.clear ();
  distance_weight.clear ();

  float total_area = 0.0f;
  const float coeff = 1.0f / 12.0f;
  const float coeff_1_div_3 = 1.0f / 3.0f;

  Eigen::Vector3f feature_point (point.x, point.y, point.z);

  for (const auto& triangle: local_triangles)
  {
    Eigen::Vector3f pt[3];
    for (unsigned int i_vertex = 0; i_vertex < 3; i_vertex++)
    {
      const unsigned int index = triangles_[triangle].vertices[i_vertex];
      pt[i_vertex] (0) = (*surface_)[index].x;
      pt[i_vertex] (1) = (*surface_)[index].y;
      pt[i_vertex] (2) = (*surface_)[index].z;
    }

    const float curr_area = ((pt[1] - pt[0]).cross (pt[2] - pt[0])).norm ();
    triangle_area.push_back (curr_area);
    total_area += curr_area;

    distance_weight.push_back (std::pow (support_radius_ - (feature_point - (pt[0] + pt[1] + pt[2]) * coeff_1_div_3).norm (), 2.0f));

    Eigen::Matrix3f curr_scatter_matrix;
    curr_scatter_matrix.setZero ();
    for (const auto &i_pt : pt)
    {
      Eigen::Vector3f vec = i_pt - feature_point;
      curr_scatter_matrix.noalias() += vec * (vec.transpose ());
      for (const auto &j_pt : pt)
        curr_scatter_matrix.noalias() += vec * ((j_pt - feature_point).transpose ());
    }
    scatter_matrices.emplace_back (coeff * curr_scatter_matrix);
  }

  if (std::abs (total_area) < std::numeric_limits <float>::epsilon ())
    total_area = 1.0f;
  else
    total_area = 1.0f / total_area;

  Eigen::Matrix3f overall_scatter_matrix;
  overall_scatter_matrix.setZero ();
  std::vector<float> total_weight (number_of_triangles);
  const float denominator = 1.0f / 6.0f;
  for (std::size_t i_triangle = 0; i_triangle < number_of_triangles; i_triangle++)
  {
    const float factor = distance_weight[i_triangle] * triangle_area[i_triangle] * total_area;
    overall_scatter_matrix += factor * scatter_matrices[i_triangle];
    total_weight[i_triangle] = factor * denominator;
  }

  Eigen::Vector3f v1, v2, v3;
  computeEigenVectors (overall_scatter_matrix, v1, v2, v3);

  float h1 = 0.0f;
  float h3 = 0.0f;
  std::size_t i_triangle = 0;
  for (const auto& triangle: local_triangles)
  {
    Eigen::Vector3f pt[3];
    for (unsigned int i_vertex = 0; i_vertex < 3; i_vertex++)
    {
      const unsigned int index = triangles_[triangle].vertices[i_vertex];
      pt[i_vertex] (0) = (*surface_)[index].x;
      pt[i_vertex] (1) = (*surface_)[index].y;
      pt[i_vertex] (2) = (*surface_)[index].z;
    }

    float factor1 = 0.0f;
    float factor3 = 0.0f;
    for (const auto &i_pt : pt)
    {
      Eigen::Vector3f vec = i_pt - feature_point;
      factor1 += vec.dot (v1);
      factor3 += vec.dot (v3);
    }
    h1 += total_weight[i_triangle] * factor1;
    h3 += total_weight[i_triangle] * factor3;
    i_triangle++;
  }

  if (h1 < 0.0f) v1 = -v1;
  if (h3 < 0.0f) v3 = -v3;

  v2 = v3.cross (v1);

  lrf_matrix.row (0) = v1;
  lrf_matrix.row (1) = v2;
  lrf_matrix.row (2) = v3;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::computeEigenVectors (const Eigen::Matrix3f& matrix,
  Eigen::Vector3f& major_axis, Eigen::Vector3f& middle_axis, Eigen::Vector3f& minor_axis) const
{
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(matrix, Eigen::ComputeEigenvectors);

  const Eigen::SelfAdjointEigenSolver <Eigen::Matrix3f>::EigenvectorsType& eigen_vectors = eigen_solver.eigenvectors ();
  const Eigen::SelfAdjointEigenSolver <Eigen::Matrix3f>::RealVectorType& eigen_values = eigen_solver.eigenvalues ();

  unsigned int temp = 0;
  unsigned int major_index = 0;
  unsigned int middle_index = 1;
  unsigned int minor_index = 2;

  if (eigen_values (major_index) < eigen_values (middle_index))
  {
    temp = major_index;
    major_index = middle_index;
    middle_index = temp;
  }

  if (eigen_values (major_index) < eigen_values (minor_index))
  {
    temp = major_index;
    major_index = minor_index;
    minor_index = temp;
  }

  if (eigen_values (middle_index) < eigen_values (minor_index))
  {
    temp = minor_index;
    minor_index = middle_index;
    middle_index = temp;
  }

  major_axis = eigen_vectors.col (major_index);
  middle_axis = eigen_vectors.col (middle_index);
  minor_axis = eigen_vectors.col (minor_index);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::transformCloud (const PointInT& point, const Eigen::Matrix3f& matrix, const pcl::Indices& local_points, PointCloudIn& transformed_cloud) const
{
  const auto number_of_points = local_points.size ();
  transformed_cloud.clear ();
  transformed_cloud.is_dense = surface_->is_dense;
  transformed_cloud.reserve (number_of_points);

  for (const auto& idx: local_points)
  {
    Eigen::Vector3f transformed_point ((*surface_)[idx].x - point.x,
                                       (*surface_)[idx].y - point.y,
                                       (*surface_)[idx].z - point.z);

    transformed_point = matrix * transformed_point;

    PointInT new_point;
    new_point.x = transformed_point (0);
    new_point.y = transformed_point (1);
    new_point.z = transformed_point (2);
    transformed_cloud.emplace_back (new_point);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::rotateCloud (const PointInT& axis, const float angle, const PointCloudIn& cloud, PointCloudIn& rotated_cloud, Eigen::Vector3f& min, Eigen::Vector3f& max) const
{
#ifdef __RVV10__
  if (pcl::detail::rops::rotateCloudRVV (axis, angle, cloud, rotated_cloud, min, max))
    return;
#endif

  Eigen::Matrix3f rotation_matrix;
  const float x = axis.x;
  const float y = axis.y;
  const float z = axis.z;
  const float rad = M_PI / 180.0f;
  const float cosine = std::cos (angle * rad);
  const float sine = std::sin (angle * rad);
  rotation_matrix << cosine + (1 - cosine) * x * x,      (1 - cosine) * x * y - sine * z,    (1 - cosine) * x * z + sine * y,
                     (1 - cosine) * y * x + sine * z,    cosine + (1 - cosine) * y * y,      (1 - cosine) * y * z - sine * x,
                     (1 - cosine) * z * x - sine * y,    (1 - cosine) * z * y + sine * x,    cosine + (1 - cosine) * z * z;

  const auto number_of_points = cloud.size ();

  rotated_cloud.header = cloud.header;
  rotated_cloud.width = number_of_points;
  rotated_cloud.height = 1;
  rotated_cloud.is_dense = cloud.is_dense;
  rotated_cloud.clear ();
  rotated_cloud.reserve (number_of_points);

  min (0) = std::numeric_limits <float>::max ();
  min (1) = std::numeric_limits <float>::max ();
  min (2) = std::numeric_limits <float>::max ();
  max (0) = -std::numeric_limits <float>::max ();
  max (1) = -std::numeric_limits <float>::max ();
  max (2) = -std::numeric_limits <float>::max ();

  for (const auto& pt: cloud.points)
  {
    Eigen::Vector3f point (pt.x, pt.y, pt.z);
    point = rotation_matrix * point;

    PointInT rotated_point;
    rotated_point.x = point (0);
    rotated_point.y = point (1);
    rotated_point.z = point (2);
    rotated_cloud.emplace_back (rotated_point);

    for (int i = 0; i < 3; ++i)
    {
      min(i) = std::min(min(i), point(i));
      max(i) = std::max(max(i), point(i));
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::getDistributionMatrix (const unsigned int projection, const Eigen::Vector3f& min, const Eigen::Vector3f& max, const PointCloudIn& cloud, Eigen::MatrixXf& matrix) const
{
#ifdef __RVV10__
  if (pcl::detail::rops::getDistributionMatrixRVV (projection, min, max, cloud, number_of_bins_, matrix))
    return;
#endif

  matrix.setZero ();

  const unsigned int coord[3][2] = {
    {0, 1},
    {0, 2},
    {1, 2}};

  const float u_bin_length = (max (coord[projection][0]) - min (coord[projection][0])) / number_of_bins_;
  const float v_bin_length = (max (coord[projection][1]) - min (coord[projection][1])) / number_of_bins_;

  for (const auto& pt: cloud.points)
  {
    Eigen::Vector3f point (pt.x, pt.y, pt.z);

    const float u_length = point (coord[projection][0]) - min[coord[projection][0]];
    const float v_length = point (coord[projection][1]) - min[coord[projection][1]];

    const float u_ratio = u_length / u_bin_length;
    auto row = static_cast <unsigned int> (u_ratio);
    if (row == number_of_bins_) row--;

    const float v_ratio = v_length / v_bin_length;
    auto col = static_cast <unsigned int> (v_ratio);
    if (col == number_of_bins_) col--;

    matrix (row, col) += 1.0f;
  }

  matrix /= std::max<float> (1, cloud.size ());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::ROPSEstimation <PointInT, PointOutT>::computeCentralMoments (const Eigen::MatrixXf& matrix, std::vector <float>& moments) const
{
  float mean_i = 0.0f;
  float mean_j = 0.0f;

  for (unsigned int i = 0; i < number_of_bins_; i++)
    for (unsigned int j = 0; j < number_of_bins_; j++)
    {
      const float m = matrix (i, j);
      mean_i += static_cast <float> (i + 1) * m;
      mean_j += static_cast <float> (j + 1) * m;
    }

  const unsigned int number_of_moments_to_compute = 4;
  const float power[number_of_moments_to_compute][2] = {
    {1.0f, 1.0f},
    {2.0f, 1.0f},
    {1.0f, 2.0f},
    {2.0f, 2.0f}};

  float entropy = 0.0f;
  moments.resize (number_of_moments_to_compute + 1, 0.0f);
  for (unsigned int i = 0; i < number_of_bins_; i++)
  {
    const float i_factor = static_cast <float> (i + 1) - mean_i;
    for (unsigned int j = 0; j < number_of_bins_; j++)
    {
      const float j_factor = static_cast <float> (j + 1) - mean_j;
      const float m = matrix (i, j);
      if (m > 0.0f)
        entropy -= m * std::log (m);
      for (unsigned int i_moment = 0; i_moment < number_of_moments_to_compute; i_moment++)
        moments[i_moment] += std::pow (i_factor, power[i_moment][0]) * std::pow (j_factor, power[i_moment][1]) * m;
    }
  }

  moments[number_of_moments_to_compute] = entropy;
}

#endif    // PCL_ROPS_ESTIMATION_HPP_
