/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011-2012, Willow Garage, Inc.
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
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>

#if defined(__RVV10__)
#include <pcl/rvv_point_store.h>
#include <riscv_vector.h>
#endif

namespace pcl
{
namespace octree
{
/** \brief @b PointCoding class
  * \note This class encodes 8-bit differential point information for octree-based point cloud compression.
  * \note typename: PointT: type of point used in pointcloud
  * \author Julius Kammerl (julius@kammerl.de)
  */
template<typename PointT>
class PointCoding
{
    // public typedefs
    using PointCloud = pcl::PointCloud<PointT>;
    using PointCloudPtr = typename PointCloud::Ptr;
    using PointCloudConstPtr = typename PointCloud::ConstPtr;

  public:
    /** \brief Constructor. */
    PointCoding () = default;

    /** \brief Empty class constructor. */
    virtual
    ~PointCoding () = default;

    /** \brief Define precision of point information
      * \param precision_arg: precision
      */
    inline void
    setPrecision (float precision_arg)
    {
      pointCompressionResolution_ = precision_arg;
    }

    /** \brief Retrieve precision of point information
      * \return precision
      */
    inline float
    getPrecision ()
    {
      return (pointCompressionResolution_);
    }

    /** \brief Set amount of points within point cloud to be encoded and reserve memory
      * \param pointCount_arg: amounts of points within point cloud
      */
    inline void
    setPointCount (unsigned int pointCount_arg)
    {
      pointDiffDataVector_.reserve (pointCount_arg * 3);
    }

    /** \brief Initialize encoding of differential point */
    void
    initializeEncoding ()
    {
      pointDiffDataVector_.clear ();
    }

    /** \brief Initialize decoding of differential point */
    void
    initializeDecoding ()
    {
      pointDiffDataVectorIterator_ = pointDiffDataVector_.begin ();
    }

    /** \brief Get reference to vector containing differential color data */
    std::vector<char>&
    getDifferentialDataVector ()
    {
      return (pointDiffDataVector_);
    }

    /** \brief Encode differential point information for a subset of points from point cloud
      * \param indexVector_arg indices defining a subset of points from points cloud
      * \param referencePoint_arg coordinates of reference point
      * \param inputCloud_arg input point cloud
      */
    void
    encodePoints (const Indices& indexVector_arg, const double* referencePoint_arg,
                  PointCloudConstPtr inputCloud_arg)
    {
      // iterate over points within current voxel
      for (const auto& idx: indexVector_arg)
      {
        unsigned char diffX, diffY, diffZ;

        // retrieve point from cloud
        const PointT& idxPoint = (*inputCloud_arg)[idx];

        // differentially encode point coordinates and truncate overflow
        diffX = static_cast<unsigned char> (std::max (-127, std::min<int>(127, static_cast<int> ((idxPoint.x - referencePoint_arg[0])  / pointCompressionResolution_))));
        diffY = static_cast<unsigned char> (std::max (-127, std::min<int>(127, static_cast<int> ((idxPoint.y - referencePoint_arg[1])  / pointCompressionResolution_))));
        diffZ = static_cast<unsigned char> (std::max (-127, std::min<int>(127, static_cast<int> ((idxPoint.z - referencePoint_arg[2])  / pointCompressionResolution_))));

        // store information in differential point vector
        pointDiffDataVector_.push_back (diffX);
        pointDiffDataVector_.push_back (diffY);
        pointDiffDataVector_.push_back (diffZ);
      }
    }

    /** \brief Decode differential point information
      * \param outputCloud_arg output point cloud
      * \param referencePoint_arg coordinates of reference point
      * \param beginIdx_arg index indicating first point to be assigned with color information
      * \param endIdx_arg index indicating last point to be assigned with color information
      */
    void
    decodePoints (PointCloudPtr outputCloud_arg, const double* referencePoint_arg, uindex_t beginIdx_arg,
                  uindex_t endIdx_arg)
    {
      assert (beginIdx_arg <= endIdx_arg);

      const uindex_t pointCount = endIdx_arg - beginIdx_arg;

#if defined(__RVV10__)
      if constexpr (pcl::rvv::kRVVXYZAoSPointCompatible<PointT>)
      {
        decodePointsRVV (outputCloud_arg, referencePoint_arg, beginIdx_arg, pointCount);
        return;
      }
#endif

      decodePointsStd (outputCloud_arg, referencePoint_arg, beginIdx_arg, pointCount);
    }

  protected:
    void
    decodePointsStd (PointCloudPtr outputCloud_arg, const double* referencePoint_arg, uindex_t beginIdx_arg,
                     uindex_t pointCount)
    {
      // iterate over points within current voxel
      for (uindex_t i = 0; i < pointCount; i++)
      {
        // retrieve differential point information
        const unsigned char& diffX = static_cast<unsigned char> (*(pointDiffDataVectorIterator_++));
        const unsigned char& diffY = static_cast<unsigned char> (*(pointDiffDataVectorIterator_++));
        const unsigned char& diffZ = static_cast<unsigned char> (*(pointDiffDataVectorIterator_++));

        // retrieve point from point cloud
        PointT& point = (*outputCloud_arg)[beginIdx_arg + i];

        // decode point position
        point.x = static_cast<float> (referencePoint_arg[0] + (diffX+0.5) * pointCompressionResolution_);
        point.y = static_cast<float> (referencePoint_arg[1] + (diffY+0.5) * pointCompressionResolution_);
        point.z = static_cast<float> (referencePoint_arg[2] + (diffZ+0.5) * pointCompressionResolution_);
      }
    }

#if defined(__RVV10__)
    static inline vfloat32m2_t
    decodeDiffBytesToFloat (vuint8mf2_t bytes, double reference, double resolution, std::size_t vl)
    {
      const vuint16m1_t u16 = __riscv_vzext_vf2_u16m1 (bytes, vl);
      const vuint32m2_t u32 = __riscv_vwaddu_vx_u32m2 (u16, 0, vl);
      vfloat64m4_t value = __riscv_vfwcvt_f_xu_v_f64m4 (u32, vl);
      value = __riscv_vfadd_vf_f64m4 (value, 0.5, vl);
      value = __riscv_vfmul_vf_f64m4 (value, resolution, vl);
      value = __riscv_vfadd_vf_f64m4 (value, reference, vl);
      return __riscv_vfncvt_f_f_w_f32m2 (value, vl);
    }

    void
    decodePointsRVV (PointCloudPtr outputCloud_arg, const double* referencePoint_arg, uindex_t beginIdx_arg,
                     uindex_t pointCount)
    {
      if (pointCount == 0)
        return;

      const auto* bytes = reinterpret_cast<const std::uint8_t*> (&(*pointDiffDataVectorIterator_));
      const ptrdiff_t diffStride = 3;
      const double resolution = static_cast<double> (pointCompressionResolution_);
      constexpr std::size_t xOffset = pcl::rvv::RVVXYZAoSFloatLayout<PointT>::kX;
      constexpr std::size_t yOffset = pcl::rvv::RVVXYZAoSFloatLayout<PointT>::kY;
      constexpr std::size_t zOffset = pcl::rvv::RVVXYZAoSFloatLayout<PointT>::kZ;

      uindex_t i = 0;
      while (i < pointCount)
      {
        const std::size_t vl = __riscv_vsetvl_e8mf2 (pointCount - i);
        const vuint8mf2_t diffX = __riscv_vlse8_v_u8mf2 (bytes + i * 3 + 0, diffStride, vl);
        const vuint8mf2_t diffY = __riscv_vlse8_v_u8mf2 (bytes + i * 3 + 1, diffStride, vl);
        const vuint8mf2_t diffZ = __riscv_vlse8_v_u8mf2 (bytes + i * 3 + 2, diffStride, vl);

        auto* pointBytes = reinterpret_cast<std::uint8_t*> (&(*outputCloud_arg)[beginIdx_arg + i]);
        pcl::rvv_store::strided_store3_f32m2<sizeof (PointT), xOffset, yOffset, zOffset> (
            pointBytes,
            vl,
            decodeDiffBytesToFloat (diffX, referencePoint_arg[0], resolution, vl),
            decodeDiffBytesToFloat (diffY, referencePoint_arg[1], resolution, vl),
            decodeDiffBytesToFloat (diffZ, referencePoint_arg[2], resolution, vl));
        i += static_cast<uindex_t> (vl);
      }

      pointDiffDataVectorIterator_ += pointCount * 3;
    }
#endif

    /** \brief Pointer to output point cloud dataset. */
    PointCloudPtr output_{nullptr};

    /** \brief Vector for storing differential point information  */
    std::vector<char> pointDiffDataVector_;

    /** \brief Iterator on differential point information vector */
    std::vector<char>::const_iterator pointDiffDataVectorIterator_;

    /** \brief Precision of point coding*/
    float pointCompressionResolution_{0.001f};
};

} // namespace octree
} // namespace pcl

#define PCL_INSTANTIATE_ColorCoding(T) template class PCL_EXPORTS pcl::octree::ColorCoding<T>;
