/*
 * SHOT RVV topic fixtures and assertions.
 *
 * 本文件负责构造 SHOT descriptor（描述子）测试的确定性输入。当前阶段使用固定
 * local reference frame（局部参考系）把证据边界收窄到 shot.hpp 的 descriptor
 * binning（分箱）、interpolation（插值）、normalization（归一化）和 NaN fallback
 * （非法路径回退）语义；它不覆盖 LRF 自动估计，也不证明真实 production RVV 分流。
 */

#pragma once

#include <pcl/features/shot.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace pcl_rvv_shot {

inline pcl::ReferenceFrame
makeIdentityFrame()
{
  pcl::ReferenceFrame frame;
  for (float& value : frame.rf)
    value = 0.0f;
  frame.x_axis[0] = 1.0f;
  frame.y_axis[1] = 1.0f;
  frame.z_axis[2] = 1.0f;
  return frame;
}

inline pcl::PointCloud<pcl::ReferenceFrame>::Ptr
makeIdentityFrames(const std::size_t count)
{
  auto frames = pcl::PointCloud<pcl::ReferenceFrame>::Ptr(new pcl::PointCloud<pcl::ReferenceFrame>);
  frames->resize(count);
  for (auto& frame : *frames)
    frame = makeIdentityFrame();
  frames->width = static_cast<std::uint32_t>(frames->size());
  frames->height = 1;
  frames->is_dense = true;
  return frames;
}

inline pcl::PointCloud<pcl::PointXYZ>::Ptr
makeShotShapeCloud(const int side = 11, const float spacing = 0.018f)
{
  auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>);
  cloud->reserve(static_cast<std::size_t>(side * side));
  const float center = static_cast<float>(side - 1) * 0.5f;
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      const float fx = (static_cast<float>(x) - center) * spacing;
      const float fy = (static_cast<float>(y) - center) * spacing;
      const float fz = 0.01f * std::sin(7.0f * fx) + 0.006f * std::cos(5.0f * fy);
      cloud->push_back(pcl::PointXYZ(fx, fy, fz));
    }
  }
  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZRGBA>::Ptr
makeShotColorCloud(const int side = 11, const float spacing = 0.018f)
{
  auto cloud = pcl::PointCloud<pcl::PointXYZRGBA>::Ptr(new pcl::PointCloud<pcl::PointXYZRGBA>);
  cloud->reserve(static_cast<std::size_t>(side * side));
  const float center = static_cast<float>(side - 1) * 0.5f;
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      pcl::PointXYZRGBA point;
      point.x = (static_cast<float>(x) - center) * spacing;
      point.y = (static_cast<float>(y) - center) * spacing;
      point.z = 0.01f * std::sin(7.0f * point.x) + 0.006f * std::cos(5.0f * point.y);
      point.r = static_cast<std::uint8_t>((31 * x + 17 * y) & 0xff);
      point.g = static_cast<std::uint8_t>((11 * x + 47 * y + 29) & 0xff);
      point.b = static_cast<std::uint8_t>((73 * x + 19 * y + 91) & 0xff);
      point.a = 255;
      cloud->push_back(point);
    }
  }
  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline pcl::PointCloud<pcl::Normal>::Ptr
makeShotNormals(const std::size_t count)
{
  auto normals = pcl::PointCloud<pcl::Normal>::Ptr(new pcl::PointCloud<pcl::Normal>);
  normals->resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float wobble = static_cast<float>(i % 7) * 0.01f;
    (*normals)[i].normal_x = wobble;
    (*normals)[i].normal_y = -0.5f * wobble;
    (*normals)[i].normal_z = 1.0f;
    (*normals)[i].curvature = 0.01f;
  }
  normals->width = static_cast<std::uint32_t>(normals->size());
  normals->height = 1;
  normals->is_dense = true;
  return normals;
}

inline pcl::IndicesPtr
makeCenterIndices(const int side)
{
  auto indices = pcl::IndicesPtr(new pcl::Indices);
  const int center = side / 2;
  indices->push_back(center * side + center);
  indices->push_back(center * side + center - 1);
  indices->push_back((center - 1) * side + center);
  return indices;
}

template <typename PointOutT>
double
descriptorChecksum(const pcl::PointCloud<PointOutT>& descriptors)
{
  double checksum = 0.0;
  for (const auto& point : descriptors) {
    constexpr std::size_t kDescriptorSize =
        std::extent<std::remove_reference_t<decltype(point.descriptor)>>::value;
    for (std::size_t i = 0; i < kDescriptorSize; ++i) {
      const float value = point.descriptor[i];
      if (std::isfinite(value))
        checksum += static_cast<double>(value) * static_cast<double>((i % 17) + 1);
    }
  }
  return checksum;
}

template <typename PointOutT>
float
descriptorL2Norm(const PointOutT& point)
{
  float acc = 0.0f;
  for (const float value : point.descriptor) {
    if (std::isfinite(value))
      acc += value * value;
  }
  return std::sqrt(acc);
}

template <typename PointOutT>
bool
descriptorAllNaN(const PointOutT& point)
{
  for (const float value : point.descriptor) {
    if (!std::isnan(value))
      return false;
  }
  return true;
}

} // namespace pcl_rvv_shot
