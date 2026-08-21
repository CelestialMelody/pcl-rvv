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

#ifndef PCL_LZF_IMAGE_IO_HPP_
#define PCL_LZF_IMAGE_IO_HPP_

#include <pcl/console/print.h>
#include <pcl/common/utils.h> // pcl::utils::ignore
#include <pcl/io/debayer.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

#define CLIP_CHAR(c) static_cast<unsigned char> ((c)>255?255:(c)<0?0:(c))


namespace pcl
{

namespace io
{

namespace detail
{

template <typename, typename = void>
struct HasByteRgbFields : std::false_type {};

template <typename PointT>
struct HasByteRgbFields<PointT,
                        std::void_t<decltype (std::declval<PointT&> ().r),
                                    decltype (std::declval<PointT&> ().g),
                                    decltype (std::declval<PointT&> ().b)>>
    : std::bool_constant<std::is_standard_layout<PointT>::value &&
                         std::is_same<std::remove_cv_t<std::remove_reference_t<decltype (std::declval<PointT&> ().r)>>, std::uint8_t>::value &&
                         std::is_same<std::remove_cv_t<std::remove_reference_t<decltype (std::declval<PointT&> ().g)>>, std::uint8_t>::value &&
                         std::is_same<std::remove_cv_t<std::remove_reference_t<decltype (std::declval<PointT&> ().b)>>, std::uint8_t>::value> {};

template <typename PointT> inline void
convertPlanarYuv422ToPointCloudStd (const std::uint8_t* yuv,
                                    const unsigned width,
                                    const unsigned height,
                                    PointT* cloud)
{
  const auto pixels = static_cast<std::size_t> (width) * height;
  const auto pairs = pixels / 2;
  const auto *color_u = yuv;
  const auto *color_y = yuv + pairs;
  const auto *color_v = yuv + pairs + pixels;

  std::size_t y_idx = 0;
  for (std::size_t i = 0; i < pairs; ++i, y_idx += 2)
  {
    const int v = color_v[i] - 128;
    const int u = color_u[i] - 128;

    PointT &pt1 = cloud[y_idx + 0];
    pt1.r =  CLIP_CHAR (color_y[y_idx + 0] + ((v * 18678 + 8192 ) >> 14));
    pt1.g =  CLIP_CHAR (color_y[y_idx + 0] + ((v * -9519 - u * 6472 + 8192) >> 14));
    pt1.b =  CLIP_CHAR (color_y[y_idx + 0] + ((u * 33292 + 8192 ) >> 14));

    PointT &pt2 = cloud[y_idx + 1];
    pt2.r =  CLIP_CHAR (color_y[y_idx + 1] + ((v * 18678 + 8192 ) >> 14));
    pt2.g =  CLIP_CHAR (color_y[y_idx + 1] + ((v * -9519 - u * 6472 + 8192) >> 14));
    pt2.b =  CLIP_CHAR (color_y[y_idx + 1] + ((u * 33292 + 8192 ) >> 14));
  }
}

template <typename PointT> inline void
convertPlanarYuv422ToPointCloudStdOMP (const std::uint8_t* yuv,
                                       const unsigned width,
                                       const unsigned height,
                                       PointT* cloud,
                                       unsigned int num_threads)
{
  const auto pixels = static_cast<std::size_t> (width) * height;
  const auto pairs = pixels / 2;
  const auto *color_u = yuv;
  const auto *color_y = yuv + pairs;
  const auto *color_v = yuv + pairs + pixels;

#ifdef _OPENMP
#pragma omp parallel for                          \
  default(none)                                   \
  shared(cloud, color_u, color_v, color_y, pairs) \
  num_threads(num_threads)
#else
  pcl::utils::ignore(num_threads); //suppress warning if OMP is not present
#endif//_OPENMP
  for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t> (pairs); ++i)
  {
    const std::size_t y_idx = static_cast<std::size_t> (2 * i);
    const int v = color_v[i] - 128;
    const int u = color_u[i] - 128;

    PointT &pt1 = cloud[y_idx + 0];
    pt1.r =  CLIP_CHAR (color_y[y_idx + 0] + ((v * 18678 + 8192 ) >> 14));
    pt1.g =  CLIP_CHAR (color_y[y_idx + 0] + ((v * -9519 - u * 6472 + 8192) >> 14));
    pt1.b =  CLIP_CHAR (color_y[y_idx + 0] + ((u * 33292 + 8192 ) >> 14));

    PointT &pt2 = cloud[y_idx + 1];
    pt2.r =  CLIP_CHAR (color_y[y_idx + 1] + ((v * 18678 + 8192 ) >> 14));
    pt2.g =  CLIP_CHAR (color_y[y_idx + 1] + ((v * -9519 - u * 6472 + 8192) >> 14));
    pt2.b =  CLIP_CHAR (color_y[y_idx + 1] + ((u * 33292 + 8192 ) >> 14));
  }
}

#if defined(__RVV10__)
inline vint32m2_t
lzfU8ToI32 (const vuint8mf2_t value, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1 (value, vl);
  return __riscv_vwadd_vx_i32m2 (__riscv_vreinterpret_v_u16m1_i16m1 (widened), 0, vl);
}

inline vint32m2_t
lzfU8OffsetToI32 (const vuint8mf2_t value, const int offset, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1 (value, vl);
  const vuint16m1_t shifted = __riscv_vsub_vx_u16m1 (widened, offset, vl);
  return __riscv_vwadd_vx_i32m2 (__riscv_vreinterpret_v_u16m1_i16m1 (shifted), 0, vl);
}

inline vuint8mf2_t
lzfClipI32ToU8 (vint32m2_t value, const std::size_t vl)
{
  value = __riscv_vmax_vx_i32m2 (value, 0, vl);
  value = __riscv_vmin_vx_i32m2 (value, 255, vl);
  const vuint32m2_t value_u32 = __riscv_vreinterpret_v_i32m2_u32m2 (value);
  const vuint16m1_t value_u16 = __riscv_vncvt_x_x_w_u16m1 (value_u32, vl);
  return __riscv_vncvt_x_x_w_u8mf2 (value_u16, vl);
}

inline vuint8mf2_t
lzfRgbChannel (const vint32m2_t y, vint32m2_t delta, const std::size_t vl)
{
  delta = __riscv_vadd_vx_i32m2 (delta, 8192, vl);
  delta = __riscv_vsra_vx_i32m2 (delta, 14, vl);
  return lzfClipI32ToU8 (__riscv_vadd_vv_i32m2 (y, delta, vl), vl);
}

template <typename PointT> inline void
lzfStoreRgbFields (PointT* points,
                   const std::ptrdiff_t point_stride,
                   const vuint8mf2_t r,
                   const vuint8mf2_t g,
                   const vuint8mf2_t b,
                   const std::size_t vl)
{
  __riscv_vsse8_v_u8mf2 (&points->r, point_stride, r, vl);
  __riscv_vsse8_v_u8mf2 (&points->g, point_stride, g, vl);
  __riscv_vsse8_v_u8mf2 (&points->b, point_stride, b, vl);
}

template <typename PointT> inline bool
convertPlanarYuv422ToPointCloudRVV (const std::uint8_t* yuv,
                                    const unsigned width,
                                    const unsigned height,
                                    PointT* cloud)
{
  if constexpr (!HasByteRgbFields<PointT>::value)
  {
    return (false);
  }
  else
  {
    const auto pixels = static_cast<std::size_t> (width) * height;
    if ((pixels & 1u) != 0u)
      return (false);

    const auto pairs = pixels / 2;
    const auto *color_u = yuv;
    const auto *color_y = yuv + pairs;
    const auto *color_v = yuv + pairs + pixels;
    // PCLZF YUV422 stores U and V once per two Y samples; each lane writes two RGB points.
    const auto point_stride = static_cast<std::ptrdiff_t> (sizeof (PointT) * 2);

    for (std::size_t pair = 0; pair < pairs;)
    {
      const std::size_t vl = __riscv_vsetvl_e8mf2 (pairs - pair);
      const vuint8mf2_t u8 = __riscv_vle8_v_u8mf2 (color_u + pair, vl);
      const vuint8mf2_t v8 = __riscv_vle8_v_u8mf2 (color_v + pair, vl);
      const vuint8mf2_t y1_8 = __riscv_vlse8_v_u8mf2 (color_y + pair * 2, 2, vl);
      const vuint8mf2_t y2_8 = __riscv_vlse8_v_u8mf2 (color_y + pair * 2 + 1, 2, vl);

      const vint32m2_t u = lzfU8OffsetToI32 (u8, 128, vl);
      const vint32m2_t v = lzfU8OffsetToI32 (v8, 128, vl);
      const vint32m2_t y1 = lzfU8ToI32 (y1_8, vl);
      const vint32m2_t y2 = lzfU8ToI32 (y2_8, vl);

      const vint32m2_t r_delta = __riscv_vmul_vx_i32m2 (v, 18678, vl);
      vint32m2_t g_delta = __riscv_vmul_vx_i32m2 (v, -9519, vl);
      g_delta = __riscv_vsub_vv_i32m2 (g_delta, __riscv_vmul_vx_i32m2 (u, 6472, vl), vl);
      const vint32m2_t b_delta = __riscv_vmul_vx_i32m2 (u, 33292, vl);

      lzfStoreRgbFields (cloud + pair * 2,
                         point_stride,
                         lzfRgbChannel (y1, r_delta, vl),
                         lzfRgbChannel (y1, g_delta, vl),
                         lzfRgbChannel (y1, b_delta, vl),
                         vl);
      lzfStoreRgbFields (cloud + pair * 2 + 1,
                         point_stride,
                         lzfRgbChannel (y2, r_delta, vl),
                         lzfRgbChannel (y2, g_delta, vl),
                         lzfRgbChannel (y2, b_delta, vl),
                         vl);

      pair += vl;
    }
    return (true);
  }
}
#endif

} // namespace detail

template <typename PointT> bool
LZFDepth16ImageReader::read (
    const std::string &filename, pcl::PointCloud<PointT> &cloud)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFDepth16ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight () * 2)
  {
    PCL_DEBUG ("[pcl::io::LZFDepth16ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFDepth16ImageReader::read] Are you sure %s is a 16-bit depth PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight () * 2, filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFDepth16ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Copy to PointT
  cloud.width    = getWidth ();
  cloud.height   = getHeight ();
  cloud.is_dense = true;
  cloud.resize (getWidth () * getHeight ());
  int depth_idx = 0, point_idx = 0;
  double constant_x = 1.0 / parameters_.focal_length_x,
         constant_y = 1.0 / parameters_.focal_length_y;
  for (std::uint32_t v = 0; v < cloud.height; ++v)
  {
    for (std::uint32_t u = 0; u < cloud.width; ++u, ++point_idx, depth_idx += 2)
    {
      PointT &pt = cloud[point_idx];
      unsigned short val;
      memcpy (&val, &uncompressed_data[depth_idx], sizeof (unsigned short));
      if (val == 0)
      {
        pt.x = pt.y = pt.z = std::numeric_limits<float>::quiet_NaN ();
        cloud.is_dense = false;
        continue;
      }

      pt.z = static_cast<float> (val * z_multiplication_factor_);
      pt.x = (static_cast<float> (u) - static_cast<float> (parameters_.principal_point_x))
        * pt.z * static_cast<float> (constant_x);
      pt.y = (static_cast<float> (v) - static_cast<float> (parameters_.principal_point_y))
        * pt.z * static_cast<float> (constant_y);
    }
  }
  cloud.sensor_origin_.setZero ();
  cloud.sensor_orientation_.w () = 1.0f;
  cloud.sensor_orientation_.x () = 0.0f;
  cloud.sensor_orientation_.y () = 0.0f;
  cloud.sensor_orientation_.z () = 0.0f;
  return (true);
}


template <typename PointT> bool
LZFDepth16ImageReader::readOMP (const std::string &filename,
                                pcl::PointCloud<PointT> &cloud,
                                unsigned int num_threads)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFDepth16ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight () * 2)
  {
    PCL_DEBUG ("[pcl::io::LZFDepth16ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFDepth16ImageReader::read] Are you sure %s is a 16-bit depth PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight () * 2, filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFDepth16ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Copy to PointT
  cloud.width    = getWidth ();
  cloud.height   = getHeight ();
  cloud.is_dense = true;
  cloud.resize (getWidth () * getHeight ());
  double constant_x = 1.0 / parameters_.focal_length_x,
         constant_y = 1.0 / parameters_.focal_length_y;
#ifdef _OPENMP
#pragma omp parallel for                                   \
  default(none)                                            \
  shared(cloud, constant_x, constant_y, uncompressed_data) \
  num_threads(num_threads)
#else
  pcl::utils::ignore(num_threads); // suppress warning if OMP is not present
#endif
  for (int i = 0; i < static_cast< int> (cloud.size ()); ++i)
  {
    int u = i % cloud.width;
    int v = i / cloud.width;
    PointT &pt = cloud[i];
    int depth_idx = 2*i;
    unsigned short val;
    memcpy (&val, &uncompressed_data[depth_idx], sizeof (unsigned short));
    if (val == 0)
    {
      pt.x = pt.y = pt.z = std::numeric_limits<float>::quiet_NaN ();
      if (cloud.is_dense)
      {
#pragma omp critical
        {
          if (cloud.is_dense)
            cloud.is_dense = false;
        }
      }
      continue;
    }

    pt.z = static_cast<float> (val * z_multiplication_factor_);
    pt.x = (static_cast<float> (u) - static_cast<float> (parameters_.principal_point_x)) 
      * pt.z * static_cast<float> (constant_x);
    pt.y = (static_cast<float> (v) - static_cast<float> (parameters_.principal_point_y)) 
      * pt.z * static_cast<float> (constant_y);
  }

  cloud.sensor_origin_.setZero ();
  cloud.sensor_orientation_.w () = 1.0f;
  cloud.sensor_orientation_.x () = 0.0f;
  cloud.sensor_orientation_.y () = 0.0f;
  cloud.sensor_orientation_.z () = 0.0f;
  return (true);
}


template <typename PointT> bool
LZFRGB24ImageReader::read (
    const std::string &filename, pcl::PointCloud<PointT> &cloud)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFRGB24ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight () * 3)
  {
    PCL_DEBUG ("[pcl::io::LZFRGB24ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFRGB24ImageReader::read] Are you sure %s is a 24-bit RGB PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight () * 3, filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFRGB24ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Copy to PointT
  cloud.width  = getWidth ();
  cloud.height = getHeight ();
  cloud.resize (getWidth () * getHeight ());

  int rgb_idx = 0;
  auto *color_r = reinterpret_cast<unsigned char*> (uncompressed_data.data());
  auto *color_g = reinterpret_cast<unsigned char*> (&uncompressed_data[getWidth () * getHeight ()]);
  auto *color_b = reinterpret_cast<unsigned char*> (&uncompressed_data[2 * getWidth () * getHeight ()]);

  for (std::size_t i = 0; i < cloud.size (); ++i, ++rgb_idx)
  {
    PointT &pt = cloud[i];

    pt.b = color_b[rgb_idx];
    pt.g = color_g[rgb_idx];
    pt.r = color_r[rgb_idx];
  }
  return (true);
}


template <typename PointT> bool
LZFRGB24ImageReader::readOMP (
    const std::string &filename, pcl::PointCloud<PointT> &cloud, unsigned int num_threads)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFRGB24ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight () * 3)
  {
    PCL_DEBUG ("[pcl::io::LZFRGB24ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFRGB24ImageReader::read] Are you sure %s is a 24-bit RGB PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight () * 3, filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFRGB24ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Copy to PointT
  cloud.width  = getWidth ();
  cloud.height = getHeight ();
  cloud.resize (getWidth () * getHeight ());

  auto *color_r = reinterpret_cast<unsigned char*> (uncompressed_data.data());
  auto *color_g = reinterpret_cast<unsigned char*> (&uncompressed_data[getWidth () * getHeight ()]);
  auto *color_b = reinterpret_cast<unsigned char*> (&uncompressed_data[2 * getWidth () * getHeight ()]);

#ifdef _OPENMP
#pragma omp parallel for                   \
  default(none)                            \
  shared(cloud, color_b, color_g, color_r) \
  num_threads(num_threads)
#else
  pcl::utils::ignore(num_threads); // suppress warning if OMP is not present
#endif//_OPENMP
  for (long int i = 0; i < cloud.size (); ++i)
  {
    PointT &pt = cloud[i];

    pt.b = color_b[i];
    pt.g = color_g[i];
    pt.r = color_r[i];
  }
  return (true);
}


template <typename PointT> bool
LZFYUV422ImageReader::read (
    const std::string &filename, pcl::PointCloud<PointT> &cloud)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFYUV422ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight () * 2)
  {
    PCL_DEBUG ("[pcl::io::LZFYUV422ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFYUV422ImageReader::read] Are you sure %s is a 16-bit YUV422 PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight (), filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFYUV422ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Convert YUV422 to RGB24 and copy to PointT
  cloud.width  = getWidth ();
  cloud.height = getHeight ();
  cloud.resize (getWidth () * getHeight ());
  const auto *yuv = reinterpret_cast<const std::uint8_t*> (uncompressed_data.data ());
#if defined(__RVV10__)
  if (detail::convertPlanarYuv422ToPointCloudRVV (yuv, getWidth (), getHeight (), cloud.data ()))
    return (true);
#endif
  detail::convertPlanarYuv422ToPointCloudStd (yuv, getWidth (), getHeight (), cloud.data ());

  return (true);
}


template <typename PointT> bool
LZFYUV422ImageReader::readOMP (
    const std::string &filename, pcl::PointCloud<PointT> &cloud, unsigned int num_threads)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFYUV422ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight () * 2)
  {
    PCL_DEBUG ("[pcl::io::LZFYUV422ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFYUV422ImageReader::read] Are you sure %s is a 16-bit YUV422 PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight (), filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFYUV422ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Convert YUV422 to RGB24 and copy to PointT
  cloud.width  = getWidth ();
  cloud.height = getHeight ();
  cloud.resize (getWidth () * getHeight ());
  const auto *yuv = reinterpret_cast<const std::uint8_t*> (uncompressed_data.data ());
#if defined(__RVV10__)
  if (detail::convertPlanarYuv422ToPointCloudRVV (yuv, getWidth (), getHeight (), cloud.data ()))
    return (true);
#endif
  detail::convertPlanarYuv422ToPointCloudStdOMP (yuv, getWidth (), getHeight (), cloud.data (), num_threads);

  return (true);
}


template <typename PointT> bool
LZFBayer8ImageReader::read (
    const std::string &filename, pcl::PointCloud<PointT> &cloud)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFBayer8ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight ())
  {
    PCL_DEBUG ("[pcl::io::LZFBayer8ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFBayer8ImageReader::read] Are you sure %s is a 8-bit Bayer PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight (), filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFBayer8ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Convert Bayer8 to RGB24
  std::vector<unsigned char> rgb_buffer (getWidth () * getHeight () * 3);
  DeBayer i;
  i.debayerEdgeAware (reinterpret_cast<unsigned char*> (uncompressed_data.data()),
                     static_cast<unsigned char*> (rgb_buffer.data()),
                     getWidth (), getHeight ());
  // Copy to PointT
  cloud.width  = getWidth ();
  cloud.height = getHeight ();
  cloud.resize (getWidth () * getHeight ());
  int rgb_idx = 0;
  for (std::size_t i = 0; i < cloud.size (); ++i, rgb_idx += 3)
  {
    PointT &pt = cloud[i];

    pt.b = rgb_buffer[rgb_idx + 2];
    pt.g = rgb_buffer[rgb_idx + 1];
    pt.r = rgb_buffer[rgb_idx + 0];
  }
  return (true);
}


template <typename PointT> bool
LZFBayer8ImageReader::readOMP (
    const std::string &filename, pcl::PointCloud<PointT> &cloud, unsigned int num_threads)
{
  std::uint32_t uncompressed_size;
  std::vector<char> compressed_data;
  if (!loadImageBlob (filename, compressed_data, uncompressed_size))
  {
    PCL_ERROR ("[pcl::io::LZFBayer8ImageReader::read] Unable to read image data from %s.\n", filename.c_str ());
    return (false);
  }

  if (uncompressed_size != getWidth () * getHeight ())
  {
    PCL_DEBUG ("[pcl::io::LZFBayer8ImageReader::read] Uncompressed data has wrong size (%u), while in fact it should be %u bytes. \n[pcl::io::LZFBayer8ImageReader::read] Are you sure %s is a 8-bit Bayer PCLZF file? Identifier says: %s\n", uncompressed_size, getWidth () * getHeight (), filename.c_str (), getImageType ().c_str ());
    return (false);
  }

  std::vector<char> uncompressed_data (uncompressed_size);
  decompress (compressed_data, uncompressed_data);

  if (uncompressed_data.empty ())
  {
    PCL_ERROR ("[pcl::io::LZFBayer8ImageReader::read] Error uncompressing data stored in %s!\n", filename.c_str ());
    return (false);
  }

  // Convert Bayer8 to RGB24
  std::vector<unsigned char> rgb_buffer (getWidth () * getHeight () * 3);
  DeBayer i;
  i.debayerEdgeAware (reinterpret_cast<unsigned char*> (uncompressed_data.data()),
                     static_cast<unsigned char*> (rgb_buffer.data()),
                     getWidth (), getHeight ());
  // Copy to PointT
  cloud.width  = getWidth ();
  cloud.height = getHeight ();
  cloud.resize (getWidth () * getHeight ());
#ifdef _OPENMP
#pragma omp parallel for \
  default(none)          \
  num_threads(num_threads)
#else
  pcl::utils::ignore(num_threads); //suppress warning if OMP is not present
#endif//_OPENMP
  for (long int i = 0; i < cloud.size (); ++i)
  {
    PointT &pt = cloud[i];
    long int rgb_idx = 3*i;
    pt.b = rgb_buffer[rgb_idx + 2];
    pt.g = rgb_buffer[rgb_idx + 1];
    pt.r = rgb_buffer[rgb_idx + 0];
  }
  return (true);
}

} // namespace io
} // namespace pcl

#endif  //#ifndef PCL_LZF_IMAGE_IO_HPP_
