/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010, Willow Garage, Inc.
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

#ifndef PCL_SEGMENTATION_IMPL_EXTRACT_POLYGONAL_PRISM_DATA_H_
#define PCL_SEGMENTATION_IMPL_EXTRACT_POLYGONAL_PRISM_DATA_H_

#include <pcl/segmentation/extract_polygonal_prism_data.h>
#include <pcl/sample_consensus/sac_model_plane.h> // for SampleConsensusModelPlane
#include <pcl/common/centroid.h>
#include <pcl/common/eigen.h>
#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#endif

#include <cstdint>

//////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::isPointIn2DPolygon (const PointT &point, const pcl::PointCloud<PointT> &polygon)
{
  // Compute the plane coefficients
  Eigen::Vector4f model_coefficients;
  EIGEN_ALIGN16 Eigen::Matrix3f covariance_matrix;
  Eigen::Vector4f xyz_centroid;

  computeMeanAndCovarianceMatrix (polygon, covariance_matrix, xyz_centroid);

  // Compute the model coefficients
  EIGEN_ALIGN16 Eigen::Vector3f::Scalar eigen_value;
  EIGEN_ALIGN16 Eigen::Vector3f eigen_vector;
  eigen33 (covariance_matrix, eigen_value, eigen_vector);

  model_coefficients[0] = eigen_vector [0];
  model_coefficients[1] = eigen_vector [1];
  model_coefficients[2] = eigen_vector [2];
  model_coefficients[3] = 0;

  // Hessian form (D = nc . p_plane (centroid here) + p)
  model_coefficients[3] = -1 * model_coefficients.dot (xyz_centroid);

  float distance_to_plane = model_coefficients[0] * point.x +
                            model_coefficients[1] * point.y +
                            model_coefficients[2] * point.z +
                            model_coefficients[3];
  PointT ppoint;
  // Calculate the projection of the point on the plane
  ppoint.x = point.x - distance_to_plane * model_coefficients[0];
  ppoint.y = point.y - distance_to_plane * model_coefficients[1];
  ppoint.z = point.z - distance_to_plane * model_coefficients[2];

  // Create a X-Y projected representation for within bounds polygonal checking
  int k0, k1, k2;
  // Determine the best plane to project points onto
  k0 = (std::abs (model_coefficients[0] ) > std::abs (model_coefficients[1])) ? 0  : 1;
  k0 = (std::abs (model_coefficients[k0]) > std::abs (model_coefficients[2])) ? k0 : 2;
  k1 = (k0 + 1) % 3;
  k2 = (k0 + 2) % 3;
  // Project the convex hull
  pcl::PointCloud<PointT> xy_polygon;
  xy_polygon.resize (polygon.size ());
  for (std::size_t i = 0; i < polygon.size (); ++i)
  {
    Eigen::Vector4f pt (polygon[i].x, polygon[i].y, polygon[i].z, 0);
    xy_polygon[i].x = pt[k1];
    xy_polygon[i].y = pt[k2];
    xy_polygon[i].z = 0;
  }
  PointT xy_point;
  xy_point.z = 0;
  Eigen::Vector4f pt (ppoint.x, ppoint.y, ppoint.z, 0);
  xy_point.x = pt[k1];
  xy_point.y = pt[k2];

  return (pcl::isXYPointIn2DXYPolygon (xy_point, xy_polygon));
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::isXYPointIn2DXYPolygon (const PointT &point, const pcl::PointCloud<PointT> &polygon)
{
  bool in_poly = false;
  double x1, x2, y1, y2;

  const auto nr_poly_points = polygon.size ();
  // start with the last point to make the check last point<->first point the first one
  double xold = polygon[nr_poly_points - 1].x;
  double yold = polygon[nr_poly_points - 1].y;
  for (std::size_t i = 0; i < nr_poly_points; i++)
  {
    double xnew = polygon[i].x;
    double ynew = polygon[i].y;
    if (xnew > xold)
    {
      x1 = xold;
      x2 = xnew;
      y1 = yold;
      y2 = ynew;
    }
    else
    {
      x1 = xnew;
      x2 = xold;
      y1 = ynew;
      y2 = yold;
    }

    if ( (xnew < point.x) == (point.x <= xold) && (point.y - y1) * (x2 - x1) < (y2 - y1) * (point.x - x1) )
    {
      in_poly = !in_poly;
    }
    xold = xnew;
    yold = ynew;
  }

  return (in_poly);
}

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ExtractPolygonalPrismData<PointT>::segment (pcl::PointIndices &output)
{
#ifdef __RVV10__
  if (segmentRvv (output))
    return;
#endif
  segmentStd (output);
}

#ifdef __RVV10__
//////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::ExtractPolygonalPrismData<PointT>::segmentRvv (pcl::PointIndices &output)
{
  if (!input_ || !planar_hull_)
    return (false);

  if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value)
  {
    return (false);
  }
  else
  {
    using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
    using Pod = typename Layout::Pod;
    static_assert(sizeof (int) == sizeof (std::uint32_t),
                  "RVV indexed scan expects 32-bit PCL indices.");

    // Large AoS strides were unstable on board in Phase 060; keep them on the scalar fallback.
    if constexpr (sizeof (PointT) > 32)
      return (false);

    output.header = input_->header;

    if (!initCompute ())
    {
      output.indices.clear ();
      return (true);
    }

    const auto fallback = [this] {
      deinitCompute ();
      return false;
    };

    if (static_cast<int> (planar_hull_->size ()) < min_pts_hull_ ||
        indices_->size () < 32 ||
        input_->size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>())
      return (fallback ());

    bool dense_ordered = true;
    for (std::size_t i = 0; i < indices_->size (); ++i)
    {
      const int index = (*indices_)[i];
      if (index < 0 || static_cast<std::size_t> (index) >= input_->size ())
        return (fallback ());
      if (index != static_cast<int> (i))
        dense_ordered = false;
    }

    // Compute the plane coefficients
    Eigen::Vector4f model_coefficients;
    EIGEN_ALIGN16 Eigen::Matrix3f covariance_matrix;
    Eigen::Vector4f xyz_centroid;

    computeMeanAndCovarianceMatrix (*planar_hull_, covariance_matrix, xyz_centroid);

    // Compute the model coefficients
    EIGEN_ALIGN16 Eigen::Vector3f::Scalar eigen_value;
    EIGEN_ALIGN16 Eigen::Vector3f eigen_vector;
    eigen33 (covariance_matrix, eigen_value, eigen_vector);

    model_coefficients[0] = eigen_vector [0];
    model_coefficients[1] = eigen_vector [1];
    model_coefficients[2] = eigen_vector [2];
    model_coefficients[3] = 0;

    // Hessian form (D = nc . p_plane (centroid here) + p)
    model_coefficients[3] = -1 * model_coefficients.dot (xyz_centroid);

    // Need to flip the plane normal towards the viewpoint
    Eigen::Vector4f vp (vpx_, vpy_, vpz_, 0);
    // See if we need to flip any plane normals
    vp -= (*planar_hull_)[0].getVector4fMap ();
    vp[3] = 0;
    // Dot product between the (viewpoint - point) and the plane normal
    float cos_theta = vp.dot (model_coefficients);
    // Flip the plane normal
    if (cos_theta < 0)
    {
      model_coefficients *= -1;
      model_coefficients[3] = 0;
      // Hessian form (D = nc . p_plane (centroid here) + p)
      model_coefficients[3] = -1 * (model_coefficients.dot ((*planar_hull_)[0].getVector4fMap ()));
    }

    // Project all points
    PointCloud projected_points;
    SampleConsensusModelPlane<PointT> sacmodel (input_);
    sacmodel.projectPoints (*indices_, model_coefficients, projected_points, false);
    if (projected_points.size () != indices_->size ())
      return (fallback ());

    // Create a X-Y projected representation for within bounds polygonal checking
    int k0, k1, k2;
    // Determine the best plane to project points onto
    k0 = (std::abs (model_coefficients[0] ) > std::abs (model_coefficients[1])) ? 0  : 1;
    k0 = (std::abs (model_coefficients[k0]) > std::abs (model_coefficients[2])) ? k0 : 2;
    k1 = (k0 + 1) % 3;
    k2 = (k0 + 2) % 3;
    // Project the convex hull
    pcl::PointCloud<PointT> polygon;
    polygon.resize (planar_hull_->size ());
    for (std::size_t i = 0; i < planar_hull_->size (); ++i)
    {
      Eigen::Vector4f pt ((*planar_hull_)[i].x, (*planar_hull_)[i].y, (*planar_hull_)[i].z, 0);
      polygon[i].x = pt[k1];
      polygon[i].y = pt[k2];
      polygon[i].z = 0;
    }

    std::vector<pcl::PointCloud<PointT>> active_polygons;
    if (polygons_.empty ())
    {
      active_polygons.push_back (polygon);
    }
    else
    {
      active_polygons.resize (polygons_.size ());
      for (std::size_t polygon_index = 0; polygon_index < polygons_.size (); ++polygon_index)
      {
        const auto& polygon_i = polygons_[polygon_index];
        auto& active_polygon = active_polygons[polygon_index];
        active_polygon.reserve (polygon_i.vertices.size ());
        for (const auto& pointIdx : polygon_i.vertices)
        {
          if (pointIdx >= polygon.size ())
            return (fallback ());
          active_polygon.points.push_back (polygon[pointIdx]);
        }
      }
    }
    for (const auto& active_polygon : active_polygons)
      if (active_polygon.size () < 3)
        return (fallback ());

    output.indices.resize (indices_->size ());
    int l = 0;
    const auto* input_base =
        reinterpret_cast<const std::uint8_t*> (input_->points.data ());
    const auto* projected_base =
        reinterpret_cast<const std::uint8_t*> (projected_points.points.data ());
    const std::size_t vlmax = __riscv_vsetvlmax_e32m2 ();
    std::vector<std::uint32_t> packed (vlmax);

    const auto load_projected_axis = [] (const std::uint8_t* base,
                                         int axis,
                                         std::size_t vl) -> vfloat32m2_t {
      switch (axis)
      {
        case 0:
          return pcl::rvv_load::strided_load_field_f32m2<PointT, pcl::fields::x> (base, vl);
        case 1:
          return pcl::rvv_load::strided_load_field_f32m2<PointT, pcl::fields::y> (base, vl);
        default:
          return pcl::rvv_load::strided_load_field_f32m2<PointT, pcl::fields::z> (base, vl);
      }
    };

    for (std::size_t i = 0; i < indices_->size ();)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (indices_->size () - i);

      vuint32m2_t source =
          __riscv_vadd_vx_u32m2 (__riscv_vid_v_u32m2 (vl), static_cast<std::uint32_t> (i), vl);
      vfloat32m2_t px;
      vfloat32m2_t py;
      vfloat32m2_t pz;
      if (dense_ordered)
      {
        pcl::rvv_load::strided_load3_f32m2<sizeof (PointT), Layout::kX, Layout::kY, Layout::kZ> (
            input_base + i * sizeof (PointT), vl, px, py, pz);
      }
      else
      {
        const vint32m2_t source_i32 = __riscv_vle32_v_i32m2 (indices_->data () + i, vl);
        source = __riscv_vreinterpret_v_i32m2_u32m2 (source_i32);
        const vuint32m2_t point_offsets = pcl::rvv_load::byte_offsets_u32m2<Pod> (source, vl);
        pcl::rvv_load::indexed_load3_f32m2<Pod, Layout::kX, Layout::kY, Layout::kZ> (
            input_base, point_offsets, vl, px, py, pz);
      }

      vfloat32m2_t distance =
          __riscv_vfmul_vf_f32m2 (px, model_coefficients[0], vl);
      distance = __riscv_vfmacc_vf_f32m2 (distance, model_coefficients[1], py, vl);
      distance = __riscv_vfmacc_vf_f32m2 (distance, model_coefficients[2], pz, vl);
      distance = __riscv_vfadd_vf_f32m2 (distance, model_coefficients[3], vl);

      vbool16_t height = __riscv_vmfge_vf_f32m2_b16 (
          distance, static_cast<float> (height_limit_min_), vl);
      height = __riscv_vmand_mm_b16 (
          height,
          __riscv_vmfle_vf_f32m2_b16 (distance, static_cast<float> (height_limit_max_), vl),
          vl);

      const auto* projected_chunk = projected_base + i * sizeof (PointT);
      const vfloat32m2_t vx = load_projected_axis (projected_chunk, k1, vl);
      const vfloat32m2_t vy = load_projected_axis (projected_chunk, k2, vl);

      vbool16_t in_any_polygon = __riscv_vmclr_m_b16 (vl);
      for (const auto& active_polygon : active_polygons)
      {
        vbool16_t in_poly = __riscv_vmclr_m_b16 (vl);
        double xold = active_polygon.back ().x;
        double yold = active_polygon.back ().y;
        for (const auto& vertex : active_polygon)
        {
          const double xnew = vertex.x;
          const double ynew = vertex.y;
          const bool new_greater = xnew > xold;
          const float x1 = static_cast<float> (new_greater ? xold : xnew);
          const float x2 = static_cast<float> (new_greater ? xnew : xold);
          const float y1 = static_cast<float> (new_greater ? yold : ynew);
          const float y2 = static_cast<float> (new_greater ? ynew : yold);

          const vbool16_t left =
              __riscv_vmfgt_vf_f32m2_b16 (vx, static_cast<float> (xnew), vl);
          const vbool16_t right =
              __riscv_vmfle_vf_f32m2_b16 (vx, static_cast<float> (xold), vl);
          const vbool16_t same_side =
              __riscv_vmnot_m_b16 (__riscv_vmxor_mm_b16 (left, right, vl), vl);

          vfloat32m2_t lhs = __riscv_vfsub_vf_f32m2 (vy, y1, vl);
          lhs = __riscv_vfmul_vf_f32m2 (lhs, x2 - x1, vl);
          vfloat32m2_t rhs = __riscv_vfsub_vf_f32m2 (vx, x1, vl);
          rhs = __riscv_vfmul_vf_f32m2 (rhs, y2 - y1, vl);
          const vbool16_t below = __riscv_vmflt_vv_f32m2_b16 (lhs, rhs, vl);
          const vbool16_t crosses = __riscv_vmand_mm_b16 (same_side, below, vl);

          in_poly = __riscv_vmxor_mm_b16 (in_poly, crosses, vl);
          xold = xnew;
          yold = ynew;
        }
        in_any_polygon = __riscv_vmxor_mm_b16 (in_any_polygon, in_poly, vl);
      }

      const vbool16_t keep = __riscv_vmand_mm_b16 (height, in_any_polygon, vl);
      const std::size_t kept = __riscv_vcpop_m_b16 (keep, vl);
      if (kept != 0)
      {
        const vuint32m2_t compact = __riscv_vcompress_vm_u32m2 (source, keep, vl);
        const std::size_t vl_store = __riscv_vsetvl_e32m2 (kept);
        __riscv_vse32_v_u32m2 (packed.data (), compact, vl_store);
        for (std::size_t lane = 0; lane < kept; ++lane)
          output.indices[l++] = static_cast<int> (packed[lane]);
      }

      i += vl;
    }

    output.indices.resize (l);
    deinitCompute ();
    return (true);
  }
}
#endif

//////////////////////////////////////////////////////////////////////////
template <typename PointT> void
pcl::ExtractPolygonalPrismData<PointT>::segmentStd (pcl::PointIndices &output)
{
  output.header = input_->header;

  if (!initCompute ())
  {
    output.indices.clear ();
    return;
  }

  if (static_cast<int> (planar_hull_->size ()) < min_pts_hull_)
  {
    PCL_ERROR("[pcl::%s::segment] Not enough points (%zu) in the hull!\n",
              getClassName().c_str(),
              static_cast<std::size_t>(planar_hull_->size()));
    output.indices.clear ();
    return;
  }

  // Compute the plane coefficients
  Eigen::Vector4f model_coefficients;
  EIGEN_ALIGN16 Eigen::Matrix3f covariance_matrix;
  Eigen::Vector4f xyz_centroid;

  computeMeanAndCovarianceMatrix (*planar_hull_, covariance_matrix, xyz_centroid);

  // Compute the model coefficients
  EIGEN_ALIGN16 Eigen::Vector3f::Scalar eigen_value;
  EIGEN_ALIGN16 Eigen::Vector3f eigen_vector;
  eigen33 (covariance_matrix, eigen_value, eigen_vector);

  model_coefficients[0] = eigen_vector [0];
  model_coefficients[1] = eigen_vector [1];
  model_coefficients[2] = eigen_vector [2];
  model_coefficients[3] = 0;

  // Hessian form (D = nc . p_plane (centroid here) + p)
  model_coefficients[3] = -1 * model_coefficients.dot (xyz_centroid);

  // Need to flip the plane normal towards the viewpoint
  Eigen::Vector4f vp (vpx_, vpy_, vpz_, 0);
  // See if we need to flip any plane normals
  vp -= (*planar_hull_)[0].getVector4fMap ();
  vp[3] = 0;
  // Dot product between the (viewpoint - point) and the plane normal
  float cos_theta = vp.dot (model_coefficients);
  // Flip the plane normal
  if (cos_theta < 0)
  {
    model_coefficients *= -1;
    model_coefficients[3] = 0;
    // Hessian form (D = nc . p_plane (centroid here) + p)
    model_coefficients[3] = -1 * (model_coefficients.dot ((*planar_hull_)[0].getVector4fMap ()));
  }

  // Project all points
  PointCloud projected_points;
  SampleConsensusModelPlane<PointT> sacmodel (input_);
  sacmodel.projectPoints (*indices_, model_coefficients, projected_points, false);

  // Create a X-Y projected representation for within bounds polygonal checking
  int k0, k1, k2;
  // Determine the best plane to project points onto
  k0 = (std::abs (model_coefficients[0] ) > std::abs (model_coefficients[1])) ? 0  : 1;
  k0 = (std::abs (model_coefficients[k0]) > std::abs (model_coefficients[2])) ? k0 : 2;
  k1 = (k0 + 1) % 3;
  k2 = (k0 + 2) % 3;
  // Project the convex hull
  pcl::PointCloud<PointT> polygon;
  polygon.resize (planar_hull_->size ());
  for (std::size_t i = 0; i < planar_hull_->size (); ++i)
  {
    Eigen::Vector4f pt ((*planar_hull_)[i].x, (*planar_hull_)[i].y, (*planar_hull_)[i].z, 0);
    polygon[i].x = pt[k1];
    polygon[i].y = pt[k2];
    polygon[i].z = 0;
  }

  PointT pt_xy;
  pt_xy.z = 0;

  std::vector<pcl::PointCloud<PointT>> polygons(polygons_.size());
  if (polygons_.empty()) {
    polygons.push_back(polygon);
  }
  else { // incase of concave hull, prepare separate polygons
    for (size_t i = 0; i < polygons_.size(); i++) {
      const auto& polygon_i = polygons_[i];
      polygons[i].reserve(polygon_i.vertices.size());
      for (const auto& pointIdx : polygon_i.vertices) {
        polygons[i].points.push_back(polygon[pointIdx]);
      }
    }
  }

  output.indices.resize (indices_->size ());
  int l = 0;
  for (std::size_t i = 0; i < projected_points.size (); ++i)
  {
    // Check the distance to the user imposed limits from the table planar model
    double distance = pointToPlaneDistanceSigned ((*input_)[(*indices_)[i]], model_coefficients);
    if (distance < height_limit_min_ || distance > height_limit_max_)
      continue;

    // Check what points are inside the hull
    Eigen::Vector4f pt (projected_points[i].x,
                         projected_points[i].y,
                         projected_points[i].z, 0);
    pt_xy.x = pt[k1];
    pt_xy.y = pt[k2];

    bool in_poly = false;
    for (const auto& poly : polygons) {
      in_poly ^= pcl::isXYPointIn2DXYPolygon(pt_xy, poly);
    }

    if (!in_poly) {
      continue;
    }

    output.indices[l++] = (*indices_)[i];
  }
  output.indices.resize (l);

  deinitCompute ();
}

#define PCL_INSTANTIATE_ExtractPolygonalPrismData(T) template class PCL_EXPORTS pcl::ExtractPolygonalPrismData<T>;
#define PCL_INSTANTIATE_isPointIn2DPolygon(T) template bool PCL_EXPORTS pcl::isPointIn2DPolygon<T>(const T&, const pcl::PointCloud<T> &);
#define PCL_INSTANTIATE_isXYPointIn2DXYPolygon(T) template bool PCL_EXPORTS pcl::isXYPointIn2DXYPolygon<T>(const T &, const pcl::PointCloud<T> &);

#endif    // PCL_SEGMENTATION_IMPL_EXTRACT_POLYGONAL_PRISM_DATA_H_
