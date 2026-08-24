/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2013-, Open Perception, Inc.
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

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <map>
#include <ctime>
#include <cstdlib>
#include <type_traits>

#include <pcl/common/io.h>
#include <pcl/common/colors.h>
#include <pcl/common/point_tests.h> // for pcl::isFinite

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::io::detail
{
  enum class PointCloudImageExtractorsPathHook
  {
    None = 0,
    RgbScalar = 1,
    RgbRvv = 2,
    ScalingScalar = 3,
    ScalingRvv = 4,
    LabelScalar = 5,
    LabelRvv = 6,
  };

#if defined(PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK)
#define PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK_ACTIVE 1
  inline int&
  pointCloudImageExtractorsLastTestHook ()
  {
    static int last_hook = static_cast<int> (PointCloudImageExtractorsPathHook::None);
    return (last_hook);
  }

  extern "C" inline void
  pcl_rvv_point_cloud_image_extractors_reset_test_hook ()
  {
    pointCloudImageExtractorsLastTestHook () =
        static_cast<int> (PointCloudImageExtractorsPathHook::None);
  }

  extern "C" inline int
  pcl_rvv_point_cloud_image_extractors_last_test_hook ()
  {
    return (pointCloudImageExtractorsLastTestHook ());
  }
#endif

  inline void
  recordPointCloudImageExtractorPath (const PointCloudImageExtractorsPathHook path)
  {
#if defined(PCL_RVV_POINT_CLOUD_IMAGE_EXTRACTORS_TEST_HOOK)
    pointCloudImageExtractorsLastTestHook () = static_cast<int> (path);
#else
    (void)path;
#endif
  }

  template <typename PointT> bool
  rgbFieldSupportsProductionRVV (const pcl::PCLPointField& field)
  {
    if constexpr (!(std::is_same_v<PointT, pcl::PointXYZRGB> ||
                    std::is_same_v<PointT, pcl::PointXYZRGBA>))
      return (false);
    return (field.count == 1 &&
            (field.datatype == pcl::PCLPointField::FLOAT32 ||
             field.datatype == pcl::PCLPointField::UINT32) &&
            field.offset % alignof (std::uint32_t) == 0 &&
            sizeof (PointT) % sizeof (std::uint32_t) == 0);
  }

  template <typename PointT> bool
  extractRgbFieldStd (const pcl::PointCloud<PointT>& cloud,
                      pcl::PCLImage& img,
                      const std::size_t offset)
  {
    img.encoding = "rgb8";
    img.width = cloud.width;
    img.height = cloud.height;
    img.step = img.width * sizeof (unsigned char) * 3;
    img.data.resize (img.step * img.height);

    recordPointCloudImageExtractorPath (PointCloudImageExtractorsPathHook::RgbScalar);
    for (std::size_t i = 0; i < cloud.size (); ++i)
    {
      std::uint32_t val;
      pcl::getFieldValue<PointT, std::uint32_t> (cloud[i], offset, val);
      img.data[i * 3 + 0] = (val >> 16) & 0x0000ff;
      img.data[i * 3 + 1] = (val >> 8) & 0x0000ff;
      img.data[i * 3 + 2] = (val) & 0x0000ff;
    }

    return (true);
  }

#if defined(__RVV10__) && defined(__riscv_vector)
  inline vuint8mf2_t
  narrowU32ToU8 (const vuint32m2_t value, const std::size_t vl)
  {
    const vuint16m1_t value_u16 = __riscv_vncvt_x_x_w_u16m1 (value, vl);
    return (__riscv_vncvt_x_x_w_u8mf2 (value_u16, vl));
  }

  template <typename PointT> bool
  extractRgbFieldRVV (const pcl::PointCloud<PointT>& cloud,
                      pcl::PCLImage& img,
                      const pcl::PCLPointField& field)
  {
    if (!rgbFieldSupportsProductionRVV<PointT> (field))
      return (false);

    img.encoding = "rgb8";
    img.width = cloud.width;
    img.height = cloud.height;
    img.step = img.width * sizeof (unsigned char) * 3;
    img.data.resize (img.step * img.height);

    recordPointCloudImageExtractorPath (PointCloudImageExtractorsPathHook::RgbRvv);
    const auto* base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());
    const auto* rgb_ptr = reinterpret_cast<const std::uint32_t*> (base + field.offset);
    constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t> (sizeof (PointT));

    std::size_t i = 0;
    while (i < cloud.size ())
    {
      const std::size_t vl = __riscv_vsetvl_e8mf2 (cloud.size () - i);
      const auto values =
          __riscv_vlse32_v_u32m2 (rgb_ptr + i * (stride / sizeof (std::uint32_t)), stride, vl);
      const auto r = narrowU32ToU8 (
          __riscv_vand_vx_u32m2 (__riscv_vsrl_vx_u32m2 (values, 16, vl), 0xff, vl), vl);
      const auto g = narrowU32ToU8 (
          __riscv_vand_vx_u32m2 (__riscv_vsrl_vx_u32m2 (values, 8, vl), 0xff, vl), vl);
      const auto b = narrowU32ToU8 (__riscv_vand_vx_u32m2 (values, 0xff, vl), vl);
      const vuint8mf2x3_t rgb = __riscv_vcreate_v_u8mf2x3 (r, g, b);
      __riscv_vsseg3e8_v_u8mf2x3 (img.data.data () + i * 3, rgb, vl);
      i += vl;
    }
    return (true);
  }
#endif

  template <typename PointT> bool
  labelFieldSupportsProductionRVV (const pcl::PCLPointField& field)
  {
    if constexpr (!std::is_same_v<PointT, pcl::PointXYZL>)
      return (false);
    return (field.count == 1 &&
            field.datatype == pcl::PCLPointField::UINT32 &&
            field.offset % alignof (std::uint32_t) == 0 &&
            sizeof (PointT) % sizeof (std::uint32_t) == 0);
  }

  template <typename PointT> bool
  extractLabelMono16FieldStd (const pcl::PointCloud<PointT>& cloud,
                              pcl::PCLImage& img,
                              const std::size_t offset)
  {
    img.encoding = "mono16";
    img.width = cloud.width;
    img.height = cloud.height;
    img.step = img.width * sizeof (unsigned short);
    img.data.resize (img.step * img.height);
    auto* data = reinterpret_cast<unsigned short*> (img.data.data ());

    recordPointCloudImageExtractorPath (PointCloudImageExtractorsPathHook::LabelScalar);
    for (std::size_t i = 0; i < cloud.size (); ++i)
    {
      std::uint32_t val;
      pcl::getFieldValue<PointT, std::uint32_t> (cloud[i], offset, val);
      data[i] = static_cast<unsigned short> (val);
    }

    return (true);
  }

#if defined(__RVV10__) && defined(__riscv_vector)
  template <typename PointT> bool
  extractLabelMono16FieldRVV (const pcl::PointCloud<PointT>& cloud,
                              pcl::PCLImage& img,
                              const pcl::PCLPointField& field)
  {
    if (!labelFieldSupportsProductionRVV<PointT> (field))
      return (false);

    img.encoding = "mono16";
    img.width = cloud.width;
    img.height = cloud.height;
    img.step = img.width * sizeof (unsigned short);
    img.data.resize (img.step * img.height);
    auto* data = reinterpret_cast<unsigned short*> (img.data.data ());

    recordPointCloudImageExtractorPath (PointCloudImageExtractorsPathHook::LabelRvv);
    const auto* base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());
    const auto* label_ptr = reinterpret_cast<const std::uint32_t*> (base + field.offset);
    constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t> (sizeof (PointT));

    std::size_t i = 0;
    while (i < cloud.size ())
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (cloud.size () - i);
      const auto labels =
          __riscv_vlse32_v_u32m2 (label_ptr + i * (stride / sizeof (std::uint32_t)), stride, vl);
      const auto labels_u16 = __riscv_vncvt_x_x_w_u16m1 (labels, vl);
      __riscv_vse16_v_u16m1 (data + i, labels_u16, vl);
      i += vl;
    }
    return (true);
  }
#endif

  template <typename PointT> bool
  scalingFieldSupportsProductionRVV (const pcl::PCLPointField& field)
  {
    if constexpr (!std::is_same_v<PointT, pcl::PointXYZI>)
      return (false);
    return (field.count == 1 &&
            field.datatype == pcl::PCLPointField::FLOAT32 &&
            field.offset % alignof (float) == 0 &&
            sizeof (PointT) % sizeof (float) == 0);
  }

  template <typename PointT> bool
  extractScalingFieldStd (
      const pcl::PointCloud<PointT>& cloud,
      pcl::PCLImage& img,
      const std::size_t offset,
      const typename pcl::io::PointCloudImageExtractorWithScaling<PointT>::ScalingMethod
          scaling_method,
      const float scaling_factor_arg)
  {
    using Extractor = pcl::io::PointCloudImageExtractorWithScaling<PointT>;

    img.encoding = "mono16";
    img.width = cloud.width;
    img.height = cloud.height;
    img.step = img.width * sizeof (unsigned short);
    img.data.resize (img.step * img.height);
    auto* data = reinterpret_cast<unsigned short*> (img.data.data ());

    recordPointCloudImageExtractorPath (PointCloudImageExtractorsPathHook::ScalingScalar);
    float scaling_factor = scaling_factor_arg;
    float data_min = 0.0f;
    if (scaling_method == Extractor::SCALING_FULL_RANGE)
    {
      float min = std::numeric_limits<float>::infinity ();
      float max = -std::numeric_limits<float>::infinity ();
      for (const auto& point: cloud)
      {
        float val;
        pcl::getFieldValue<PointT, float> (point, offset, val);
        if (val < min)
          min = val;
        if (val > max)
          max = val;
      }
      scaling_factor = min == max ? 0 : std::numeric_limits<unsigned short>::max () / (max - min);
      data_min = min;
    }

    for (std::size_t i = 0; i < cloud.size (); ++i)
    {
      float val;
      pcl::getFieldValue<PointT, float> (cloud[i], offset, val);
      if (scaling_method == Extractor::SCALING_NO)
      {
        data[i] = val;
      }
      else if (scaling_method == Extractor::SCALING_FULL_RANGE)
      {
        data[i] = (val - data_min) * scaling_factor;
      }
      else if (scaling_method == Extractor::SCALING_FIXED_FACTOR)
      {
        data[i] = val * scaling_factor;
      }
    }

    return (true);
  }

#if defined(__RVV10__) && defined(__riscv_vector)
  template <typename PointT> bool
  extractScalingFullRangeIntensityRVV (const pcl::PointCloud<PointT>& cloud,
                                       pcl::PCLImage& img,
                                       const pcl::PCLPointField& field)
  {
    if (!scalingFieldSupportsProductionRVV<PointT> (field))
      return (false);

    img.encoding = "mono16";
    img.width = cloud.width;
    img.height = cloud.height;
    img.step = img.width * sizeof (unsigned short);
    img.data.resize (img.step * img.height);
    auto* data = reinterpret_cast<unsigned short*> (img.data.data ());

    recordPointCloudImageExtractorPath (PointCloudImageExtractorsPathHook::ScalingRvv);
    const auto* base = reinterpret_cast<const std::uint8_t*> (cloud.points.data ());
    const auto* intensity_ptr = reinterpret_cast<const float*> (base + field.offset);
    constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t> (sizeof (PointT));

    const std::size_t vlmax = __riscv_vsetvlmax_e32m2 ();
    const float min_seed = std::numeric_limits<float>::infinity ();
    const float max_seed = -std::numeric_limits<float>::infinity ();
    auto v_min = __riscv_vfmv_v_f_f32m2 (min_seed, vlmax);
    auto v_max = __riscv_vfmv_v_f_f32m2 (max_seed, vlmax);

    std::size_t i = 0;
    while (i < cloud.size ())
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (cloud.size () - i);
      const auto v =
          __riscv_vlse32_v_f32m2 (intensity_ptr + i * (stride / sizeof (float)), stride, vl);
      v_min = __riscv_vfmin_vv_f32m2_tu (v_min, v_min, v, vl);
      v_max = __riscv_vfmax_vv_f32m2_tu (v_max, v_max, v, vl);
      i += vl;
    }

    const auto red_min_seed = __riscv_vfmv_s_f_f32m1 (min_seed, 1);
    const auto red_max_seed = __riscv_vfmv_s_f_f32m1 (max_seed, 1);
    const float min_value =
        __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_min, red_min_seed, vlmax));
    const float max_value =
        __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_max, red_max_seed, vlmax));
    const float scaling_factor = min_value == max_value
                                     ? 0.0f
                                     : std::numeric_limits<unsigned short>::max () /
                                           (max_value - min_value);

    std::vector<float> values (vlmax, 0.0f);
    i = 0;
    while (i < cloud.size ())
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (cloud.size () - i);
      auto v = __riscv_vlse32_v_f32m2 (
          intensity_ptr + i * (stride / sizeof (float)), stride, vl);
      v = __riscv_vfmul_vf_f32m2 (__riscv_vfsub_vf_f32m2 (v, min_value, vl), scaling_factor, vl);
      __riscv_vse32_v_f32m2 (values.data (), v, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        data[i + lane] = static_cast<unsigned short> (values[lane]);
      i += vl;
    }
    return (true);
  }
#endif
} // namespace pcl::io::detail

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::io::PointCloudImageExtractor<PointT>::extract (const PointCloud& cloud, pcl::PCLImage& img) const
{
  if (!cloud.isOrganized () || cloud.size () != cloud.width * cloud.height)
    return (false);

  bool result = this->extractImpl (cloud, img);

  if (paint_nans_with_black_ && result)
  {
    std::size_t size = img.encoding == "mono16" ? 2 : 3;
    for (std::size_t i = 0; i < cloud.size (); ++i)
      if (!pcl::isFinite (cloud[i])) {
        std::fill_n(&img.data[i * size], size, 0);
      }
  }

  return (result);
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::io::PointCloudImageExtractorFromNormalField<PointT>::extractImpl (const PointCloud& cloud, pcl::PCLImage& img) const
{
  std::vector<pcl::PCLPointField> fields;
  int field_x_idx = pcl::getFieldIndex<PointT> ("normal_x", fields);
  int field_y_idx = pcl::getFieldIndex<PointT> ("normal_y", fields);
  int field_z_idx = pcl::getFieldIndex<PointT> ("normal_z", fields);
  if (field_x_idx == -1 || field_y_idx == -1 || field_z_idx == -1)
    return (false);
  const std::size_t offset_x = fields[field_x_idx].offset;
  const std::size_t offset_y = fields[field_y_idx].offset;
  const std::size_t offset_z = fields[field_z_idx].offset;

  img.encoding = "rgb8";
  img.width = cloud.width;
  img.height = cloud.height;
  img.step = img.width * sizeof (unsigned char) * 3;
  img.data.resize (img.step * img.height);

  for (std::size_t i = 0; i < cloud.size (); ++i)
  {
    float x;
    float y;
    float z;
    pcl::getFieldValue<PointT, float> (cloud[i], offset_x, x);
    pcl::getFieldValue<PointT, float> (cloud[i], offset_y, y);
    pcl::getFieldValue<PointT, float> (cloud[i], offset_z, z);
    img.data[i * 3 + 0] = static_cast<unsigned char>((x + 1.0) * 127);
    img.data[i * 3 + 1] = static_cast<unsigned char>((y + 1.0) * 127);
    img.data[i * 3 + 2] = static_cast<unsigned char>((z + 1.0) * 127);
  }

  return (true);
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::io::PointCloudImageExtractorFromRGBField<PointT>::extractImpl (const PointCloud& cloud, pcl::PCLImage& img) const
{
  std::vector<pcl::PCLPointField> fields;
  int field_idx = pcl::getFieldIndex<PointT> ("rgb", fields);
  if (field_idx == -1)
  {
    field_idx = pcl::getFieldIndex<PointT> ("rgba", fields);
    if (field_idx == -1)
      return (false);
  }
  const std::size_t offset = fields[field_idx].offset;

#if defined(__RVV10__) && defined(__riscv_vector)
  if (pcl::io::detail::extractRgbFieldRVV<PointT> (cloud, img, fields[field_idx]))
    return (true);
#endif

  return (pcl::io::detail::extractRgbFieldStd<PointT> (cloud, img, offset));
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::io::PointCloudImageExtractorFromLabelField<PointT>::extractImpl (const PointCloud& cloud, pcl::PCLImage& img) const
{
  std::vector<pcl::PCLPointField> fields;
  int field_idx = pcl::getFieldIndex<PointT> ("label", fields);
  if (field_idx == -1)
    return (false);
  const std::size_t offset = fields[field_idx].offset;

  switch (color_mode_)
  {
    case COLORS_MONO:
    {
#if defined(__RVV10__) && defined(__riscv_vector)
      if (pcl::io::detail::extractLabelMono16FieldRVV<PointT> (cloud, img, fields[field_idx]))
        break;
#endif
      pcl::io::detail::extractLabelMono16FieldStd<PointT> (cloud, img, offset);
      break;
    }
    case COLORS_RGB_RANDOM:
    {
      img.encoding = "rgb8";
      img.width = cloud.width;
      img.height = cloud.height;
      img.step = img.width * sizeof (unsigned char) * 3;
      img.data.resize (img.step * img.height);

      std::srand(std::time(nullptr));
      std::map<std::uint32_t, std::size_t> colormap;

      for (std::size_t i = 0; i < cloud.size (); ++i)
      {
        std::uint32_t val;
        pcl::getFieldValue<PointT, std::uint32_t> (cloud[i], offset, val);
        if (colormap.count (val) == 0)
        {
          colormap[val] = i * 3;
          img.data[i * 3 + 0] = static_cast<std::uint8_t> ((std::rand () % 256));
          img.data[i * 3 + 1] = static_cast<std::uint8_t> ((std::rand () % 256));
          img.data[i * 3 + 2] = static_cast<std::uint8_t> ((std::rand () % 256));
        }
        else
        {
          memcpy (&img.data[i * 3], &img.data[colormap[val]], 3);
        }
      }
      break;
    }
    case COLORS_RGB_GLASBEY:
    {
      img.encoding = "rgb8";
      img.width = cloud.width;
      img.height = cloud.height;
      img.step = img.width * sizeof (unsigned char) * 3;
      img.data.resize (img.step * img.height);

      std::srand(std::time(nullptr));
      std::set<std::uint32_t> labels;
      std::map<std::uint32_t, std::size_t> colormap;

      // First pass: find unique labels
      for (const auto& point: cloud)
      {
        // If we need to paint NaN points with black do not waste colors on them
        if (paint_nans_with_black_ && !pcl::isFinite (point))
          continue;
        std::uint32_t val;
        pcl::getFieldValue<PointT, std::uint32_t> (point, offset, val);
        labels.insert (val);
      }

      // Assign Glasbey colors in ascending order of labels
      // Note: the color LUT has a finite size (256 colors), therefore when
      // there are more labels the colors will repeat
      std::size_t color = 0;
      for (const std::uint32_t &label : labels)
      {
        colormap[label] = color % GlasbeyLUT::size ();
        ++color;
      }

      // Second pass: copy colors from the LUT
      for (std::size_t i = 0; i < cloud.size (); ++i)
      {
        std::uint32_t val;
        pcl::getFieldValue<PointT, std::uint32_t> (cloud[i], offset, val);
        memcpy (&img.data[i * 3], GlasbeyLUT::data () + colormap[val] * 3, 3);
      }

      break;
    }
  }

  return (true);
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> bool
pcl::io::PointCloudImageExtractorWithScaling<PointT>::extractImpl (const PointCloud& cloud, pcl::PCLImage& img) const
{
  std::vector<pcl::PCLPointField> fields;
  int field_idx = pcl::getFieldIndex<PointT> (field_name_, fields);
  if (field_idx == -1)
    return (false);
  const std::size_t offset = fields[field_idx].offset;

#if defined(__RVV10__) && defined(__riscv_vector)
  if (field_name_ == "intensity" && scaling_method_ == SCALING_FULL_RANGE &&
      pcl::io::detail::extractScalingFullRangeIntensityRVV<PointT> (cloud, img, fields[field_idx]))
    return (true);
#endif

  return (pcl::io::detail::extractScalingFieldStd<PointT> (
      cloud, img, offset, scaling_method_, scaling_factor_));
}
