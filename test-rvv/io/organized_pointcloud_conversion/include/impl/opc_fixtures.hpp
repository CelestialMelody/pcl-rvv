/*
 * 本文件做什么：
 * 构造 PointXYZ organized cloud（有组织点云）输入。样本覆盖 dense finite（全部有限）
 * 和 mixed invalid（混合无效点）两类路径，让 correctness 和 bench 使用同一输入语义。
 */

#pragma once

#include "opc_types.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstdint>
#include <limits>

namespace pcl::io::rvv_test::organized_pointcloud_conversion {

inline pcl::PointCloud<pcl::PointXYZ>
makePointXYZCloud(const std::size_t size, const InvalidPattern invalid_pattern)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = invalid_pattern == InvalidPattern::finite_only;
  cloud.resize(size);

  for (std::size_t i = 0; i < size; ++i) {
    const float base = static_cast<float>((i % 4096) + 1);
    cloud[i].x = std::sin(base * 0.013f);
    cloud[i].y = std::cos(base * 0.017f);
    cloud[i].z = 0.35f + static_cast<float>((i * 37) % 2000) * 0.0025f;

    if (invalid_pattern == InvalidPattern::mixed_invalid) {
      if (i % 97 == 0)
        cloud[i].x = std::numeric_limits<float>::quiet_NaN();
      if (i % 131 == 0)
        cloud[i].z = std::numeric_limits<float>::infinity();
    }
  }

  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZ>
makeOrganizedPointXYZCloud(const std::size_t width,
                           const std::size_t height,
                           const InvalidPattern invalid_pattern)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = invalid_pattern == InvalidPattern::finite_only;
  cloud.resize(width * height);

  const int center_x = static_cast<int>(width / 2);
  const int center_y = static_cast<int>(height / 2);
  constexpr float focal = 525.0f;

  for (std::size_t row = 0; row < height; ++row) {
    const int y = static_cast<int>(row) - center_y;
    for (std::size_t col = 0; col < width; ++col) {
      const int x = static_cast<int>(col) - center_x;
      const std::size_t index = row * width + col;
      const float depth = 0.5f + static_cast<float>(index + 1) * 0.0005f;

      cloud[index].x = static_cast<float>(x) * depth / focal;
      cloud[index].y = static_cast<float>(y) * depth / focal;
      cloud[index].z = depth;

      if (invalid_pattern == InvalidPattern::mixed_invalid && index + 1 < cloud.size()) {
        if (index % 97 == 0)
          cloud[index].x = std::numeric_limits<float>::quiet_NaN();
        if (index % 131 == 0)
          cloud[index].z = std::numeric_limits<float>::infinity();
      }
    }
  }

  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZRGB>
makePointXYZRGBCloud(const std::size_t size, const InvalidPattern invalid_pattern)
{
  pcl::PointCloud<pcl::PointXYZRGB> cloud;
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = invalid_pattern == InvalidPattern::finite_only;
  cloud.resize(size);

  for (std::size_t i = 0; i < size; ++i) {
    const float base = static_cast<float>((i % 4096) + 1);
    cloud[i].x = std::sin(base * 0.013f);
    cloud[i].y = std::cos(base * 0.017f);
    cloud[i].z = 0.35f + static_cast<float>((i * 37) % 2000) * 0.0025f;
    cloud[i].r = static_cast<std::uint8_t>((i * 3) & 0xffu);
    cloud[i].g = static_cast<std::uint8_t>((i * 5 + 17) & 0xffu);
    cloud[i].b = static_cast<std::uint8_t>((i * 7 + 29) & 0xffu);

    if (invalid_pattern == InvalidPattern::mixed_invalid) {
      if (i % 97 == 0)
        cloud[i].x = std::numeric_limits<float>::quiet_NaN();
      if (i % 131 == 0)
        cloud[i].z = std::numeric_limits<float>::infinity();
    }
  }

  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZRGB>
makeOrganizedPointXYZRGBCloud(const std::size_t width,
                              const std::size_t height,
                              const InvalidPattern invalid_pattern)
{
  pcl::PointCloud<pcl::PointXYZRGB> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = invalid_pattern == InvalidPattern::finite_only;
  cloud.resize(width * height);

  const int center_x = static_cast<int>(width / 2);
  const int center_y = static_cast<int>(height / 2);
  constexpr float focal = 525.0f;

  for (std::size_t row = 0; row < height; ++row) {
    const int y = static_cast<int>(row) - center_y;
    for (std::size_t col = 0; col < width; ++col) {
      const int x = static_cast<int>(col) - center_x;
      const std::size_t index = row * width + col;
      const float depth = 0.5f + static_cast<float>(index + 1) * 0.0005f;

      cloud[index].x = static_cast<float>(x) * depth / focal;
      cloud[index].y = static_cast<float>(y) * depth / focal;
      cloud[index].z = depth;
      cloud[index].r = static_cast<std::uint8_t>((index * 3) & 0xffu);
      cloud[index].g = static_cast<std::uint8_t>((index * 5 + 17) & 0xffu);
      cloud[index].b = static_cast<std::uint8_t>((index * 7 + 29) & 0xffu);

      if (invalid_pattern == InvalidPattern::mixed_invalid && index + 1 < cloud.size()) {
        if (index % 97 == 0)
          cloud[index].x = std::numeric_limits<float>::quiet_NaN();
        if (index % 131 == 0)
          cloud[index].z = std::numeric_limits<float>::infinity();
      }
    }
  }

  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZI>
makePointXYZICloud(const std::size_t size, const InvalidPattern invalid_pattern)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = invalid_pattern == InvalidPattern::finite_only;
  cloud.resize(size);

  for (std::size_t i = 0; i < size; ++i) {
    const float base = static_cast<float>((i % 4096) + 1);
    cloud[i].x = std::sin(base * 0.013f);
    cloud[i].y = std::cos(base * 0.017f);
    cloud[i].z = 0.35f + static_cast<float>((i * 37) % 2000) * 0.0025f;
    cloud[i].intensity = static_cast<float>((i * 11) & 0xffu);

    if (invalid_pattern == InvalidPattern::mixed_invalid) {
      if (i % 97 == 0)
        cloud[i].x = std::numeric_limits<float>::quiet_NaN();
      if (i % 131 == 0)
        cloud[i].z = std::numeric_limits<float>::infinity();
    }
  }

  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZRGBA>
makePointXYZRGBACloud(const std::size_t size, const InvalidPattern invalid_pattern)
{
  pcl::PointCloud<pcl::PointXYZRGBA> cloud;
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = invalid_pattern == InvalidPattern::finite_only;
  cloud.resize(size);

  for (std::size_t i = 0; i < size; ++i) {
    const float base = static_cast<float>((i % 4096) + 1);
    cloud[i].x = std::sin(base * 0.013f);
    cloud[i].y = std::cos(base * 0.017f);
    cloud[i].z = 0.35f + static_cast<float>((i * 37) % 2000) * 0.0025f;
    cloud[i].r = static_cast<std::uint8_t>((i * 3) & 0xffu);
    cloud[i].g = static_cast<std::uint8_t>((i * 5 + 17) & 0xffu);
    cloud[i].b = static_cast<std::uint8_t>((i * 7 + 29) & 0xffu);
    cloud[i].a = static_cast<std::uint8_t>(255u - (i & 0xffu));

    if (invalid_pattern == InvalidPattern::mixed_invalid) {
      if (i % 97 == 0)
        cloud[i].x = std::numeric_limits<float>::quiet_NaN();
      if (i % 131 == 0)
        cloud[i].z = std::numeric_limits<float>::infinity();
    }
  }

  return cloud;
}

inline std::vector<std::uint16_t>
makeDisparityImage(const std::size_t width,
                   const std::size_t height,
                   const InvalidPattern invalid_pattern)
{
  const std::size_t size = width * height;
  std::vector<std::uint16_t> disparity(size, 0);

  for (std::size_t i = 0; i < size; ++i) {
    disparity[i] = static_cast<std::uint16_t>(120u + ((i * 37u) % 1800u));
    if (invalid_pattern == InvalidPattern::mixed_invalid) {
      if (i % 97 == 0)
        disparity[i] = 0;
      if (i % 257 == 0)
        disparity[i] = 0x7ffu;
    }
  }

  return disparity;
}

}  // namespace pcl::io::rvv_test::organized_pointcloud_conversion
