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
 */

#ifndef PCL_FILTERS_IMPL_BOX_CLIPPER3D_HPP
#define PCL_FILTERS_IMPL_BOX_CLIPPER3D_HPP

#include <pcl/common/rvv_point_load.h>
#include <pcl/filters/box_clipper3D.h>
#include <pcl/point_types.h>

#if defined(__RVV10__)
#include <cstdint>
#include <limits>
#include <riscv_vector.h>
#endif

namespace pcl
{

template<typename PointT> void
clipPointCloud3DStd (const pcl::PointCloud<PointT>& cloud_in,
                     Indices& clipped,
                     const Indices& indices,
                     const pcl::BoxClipper3D<PointT>& clipper)
{
  clipped.clear ();
  if (indices.empty ())
  {
    clipped.reserve (cloud_in.size ());
    for (std::size_t pIdx = 0; pIdx < cloud_in.size (); ++pIdx)
      if (clipper.clipPoint3D (cloud_in[pIdx]))
        clipped.push_back (pIdx);
  }
  else
  {
    for (const auto &index : indices)
      if (clipper.clipPoint3D (cloud_in[index]))
        clipped.push_back (index);
  }
}

#if defined(__RVV10__)

inline constexpr std::size_t kBoxClipper3DMinPoints = 64;

template<typename PointT> bool
clipPointCloud3DRVV (const pcl::PointCloud<PointT>& cloud_in,
                     Indices& clipped,
                     const Indices& indices,
                     const Eigen::Matrix4f& transform)
{
  const std::size_t n = cloud_in.size ();
  if (!indices.empty () ||
      n < pcl::kBoxClipper3DMinPoints ||
      n > static_cast<std::size_t> (std::numeric_limits<int>::max ()))
    return false;

  clipped.clear ();
  clipped.resize (n);

  const auto* base = reinterpret_cast<const std::uint8_t*> (cloud_in.data ());
  int* out = clipped.data ();
  std::size_t kept = 0;
  std::size_t i = 0;

  const float m00 = transform (0, 0);
  const float m01 = transform (0, 1);
  const float m02 = transform (0, 2);
  const float m03 = transform (0, 3);
  const float m10 = transform (1, 0);
  const float m11 = transform (1, 1);
  const float m12 = transform (1, 2);
  const float m13 = transform (1, 3);
  const float m20 = transform (2, 0);
  const float m21 = transform (2, 1);
  const float m22 = transform (2, 2);
  const float m23 = transform (2, 3);
  const float m30 = transform (3, 0);
  const float m31 = transform (3, 1);
  const float m32 = transform (3, 2);
  const float m33 = transform (3, 3);

  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const auto* chunk = base + i * sizeof (PointT);

    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::strided_load3_f32m2<sizeof (PointT), offsetof (PointT, x), offsetof (PointT, y), offsetof (PointT, z)> (chunk, vl, vx, vy, vz);

    // The scalar predicate is (abs(T * [x y z 1]^T) <= 1).all().
    // Full-cloud XYZ-compatible inputs use AoS stride loads and vcompress keeps output
    // indices ordered; subset/generic point types fall back to the scalar path.
    vfloat32m2_t tx = __riscv_vfmul_vf_f32m2 (vx, m00, vl);
    tx = __riscv_vfmacc_vf_f32m2 (tx, m01, vy, vl);
    tx = __riscv_vfmacc_vf_f32m2 (tx, m02, vz, vl);
    tx = __riscv_vfadd_vf_f32m2 (tx, m03, vl);

    vfloat32m2_t ty = __riscv_vfmul_vf_f32m2 (vx, m10, vl);
    ty = __riscv_vfmacc_vf_f32m2 (ty, m11, vy, vl);
    ty = __riscv_vfmacc_vf_f32m2 (ty, m12, vz, vl);
    ty = __riscv_vfadd_vf_f32m2 (ty, m13, vl);

    vfloat32m2_t tz = __riscv_vfmul_vf_f32m2 (vx, m20, vl);
    tz = __riscv_vfmacc_vf_f32m2 (tz, m21, vy, vl);
    tz = __riscv_vfmacc_vf_f32m2 (tz, m22, vz, vl);
    tz = __riscv_vfadd_vf_f32m2 (tz, m23, vl);

    vfloat32m2_t tw = __riscv_vfmul_vf_f32m2 (vx, m30, vl);
    tw = __riscv_vfmacc_vf_f32m2 (tw, m31, vy, vl);
    tw = __riscv_vfmacc_vf_f32m2 (tw, m32, vz, vl);
    tw = __riscv_vfadd_vf_f32m2 (tw, m33, vl);

    vbool16_t keep = __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tx, vl), 1.0f, vl);
    keep = __riscv_vmand_mm_b16 (keep, __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (ty, vl), 1.0f, vl), vl);
    keep = __riscv_vmand_mm_b16 (keep, __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tz, vl), 1.0f, vl), vl);
    keep = __riscv_vmand_mm_b16 (keep, __riscv_vmfle_vf_f32m2_b16 (__riscv_vfabs_v_f32m2 (tw, vl), 1.0f, vl), vl);

    const vuint32m2_t local = __riscv_vid_v_u32m2 (vl);
    const vuint32m2_t source = __riscv_vadd_vx_u32m2 (local, static_cast<std::uint32_t> (i), vl);
    const vint32m2_t source_i32 = __riscv_vreinterpret_v_u32m2_i32m2 (source);
    const vint32m2_t compact = __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
    const std::size_t count = __riscv_vcpop_m_b16 (keep, vl);
    __riscv_vse32_v_i32m2 (out + kept, compact, count);

    kept += count;
    i += vl;
  }

  clipped.resize (kept);
  return true;
}

#endif

} // namespace pcl

template<typename PointT>
pcl::BoxClipper3D<PointT>::BoxClipper3D (const Eigen::Affine3f& transformation)
: transformation_ (transformation)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
pcl::BoxClipper3D<PointT>::BoxClipper3D (const Eigen::Vector3f& rodrigues, const Eigen::Vector3f& translation, const Eigen::Vector3f& box_size)
{
  setTransformation (rodrigues, translation, box_size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT>
pcl::BoxClipper3D<PointT>::~BoxClipper3D () noexcept
= default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> void
pcl::BoxClipper3D<PointT>::setTransformation (const Eigen::Affine3f& transformation)
{
  transformation_ = transformation;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> void
pcl::BoxClipper3D<PointT>::setTransformation (const Eigen::Vector3f& rodrigues, const Eigen::Vector3f& translation, const Eigen::Vector3f& box_size)
{
  transformation_ = (Eigen::Translation3f (translation) * Eigen::AngleAxisf(rodrigues.norm (), rodrigues.normalized ()) * Eigen::Scaling (0.5f * box_size)).inverse ();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> pcl::Clipper3D<PointT>*
pcl::BoxClipper3D<PointT>::clone () const
{
  return new BoxClipper3D<PointT> (transformation_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> void
pcl::BoxClipper3D<PointT>::transformPoint (const PointT& pointIn, PointT& pointOut) const
{
  const Eigen::Vector4f& point = pointIn.getVector4fMap ();
  pointOut.getVector4fMap () = transformation_ * point;

  // homogeneous value might not be 1
  if (point [3] != 1)
  {
    // homogeneous component might be uninitialized -> invalid
    if (point [3] != 0)
    {
      pointOut.x += (1 - point [3]) * transformation_.data () [ 9];
      pointOut.y += (1 - point [3]) * transformation_.data () [10];
      pointOut.z += (1 - point [3]) * transformation_.data () [11];
    }
    else
    {
      pointOut.x += transformation_.data () [ 9];
      pointOut.y += transformation_.data () [10];
      pointOut.z += transformation_.data () [11];
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> bool
pcl::BoxClipper3D<PointT>::clipPoint3D (const PointT& point) const
{
  Eigen::Vector4f point_coordinates (transformation_.matrix ()
    * point.getVector4fMap ());
  return (point_coordinates.array ().abs () <= 1).all ();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @attention untested code
 */
template<typename PointT> bool
pcl::BoxClipper3D<PointT>::clipLineSegment3D (PointT&, PointT&) const
{
  /*
  PointT pt1, pt2;
  transformPoint (point1, pt1);
  transformPoint (point2, pt2);

  //
  bool pt1InBox = (std::abs(pt1.x) <= 1.0 && std::abs (pt1.y) <= 1.0 && std::abs (pt1.z) <= 1.0);
  bool pt2InBox = (std::abs(pt2.x) <= 1.0 && std::abs (pt2.y) <= 1.0 && std::abs (pt2.z) <= 1.0);

  // one is outside the other one inside the box
  //if (pt1InBox ^ pt2InBox)
  if (pt1InBox && !pt2InBox)
  {
    PointT diff;
    PointT lambda;
    diff.getVector3fMap () = pt2.getVector3fMap () - pt1.getVector3fMap ();

    if (diff.x > 0)
      lambda.x = (1.0 - pt1.x) / diff.x;
    else
      lambda.x = (-1.0 - pt1.x) / diff.x;

    if (diff.y > 0)
      lambda.y = (1.0 - pt1.y) / diff.y;
    else
      lambda.y = (-1.0 - pt1.y) / diff.y;

    if (diff.z > 0)
      lambda.z = (1.0 - pt1.z) / diff.z;
    else
      lambda.z = (-1.0 - pt1.z) / diff.z;

    pt2 = pt1 + std::min(std::min(lambda.x, lambda.y), lambda.z) * diff;

    // inverse transformation
    inverseTransformPoint (pt2, point2);
    return true;
  }
  else if (!pt1InBox && pt2InBox)
  {
    return true;
  }
  */
  throw std::logic_error ("Not implemented");
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @attention untested code
 */
template<typename PointT> void
pcl::BoxClipper3D<PointT>::clipPlanarPolygon3D (const std::vector<PointT, Eigen::aligned_allocator<PointT> >&, std::vector<PointT, Eigen::aligned_allocator<PointT> >& clipped_polygon) const
{
  // not implemented -> clip everything
  clipped_polygon.clear ();
  throw std::logic_error ("Not implemented");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @attention untested code
 */
template<typename PointT> void
pcl::BoxClipper3D<PointT>::clipPlanarPolygon3D (std::vector<PointT, Eigen::aligned_allocator<PointT> >& polygon) const
{
  // not implemented -> clip everything
  polygon.clear ();
  throw std::logic_error ("Not implemented");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// /ToDo: write fast version using eigen map and single matrix vector multiplication, that uses advantages of eigens SSE operations.
template<typename PointT> void
pcl::BoxClipper3D<PointT>::clipPointCloud3D (const pcl::PointCloud<PointT>& cloud_in, Indices& clipped, const Indices& indices) const
{
#if defined(__RVV10__)
  if constexpr (pcl::rvv::kRVVXYZPointCompatible<PointT>)
  {
    if (pcl::clipPointCloud3DRVV (cloud_in, clipped, indices, transformation_.matrix ()))
      return;
  }
#endif

  pcl::clipPointCloud3DStd (cloud_in, clipped, indices, *this);
}
#endif //PCL_FILTERS_IMPL_BOX_CLIPPER3D_HPP
