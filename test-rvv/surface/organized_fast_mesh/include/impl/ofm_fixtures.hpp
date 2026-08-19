/*
 * 本文件负责构造 organized_fast_mesh 的合成 organized cloud（有序点云）。
 * 样本包含规则 z 起伏、周期性 NaN 和少量 Inf，用来验证 finite mask
 * （有限值掩码）和 adaptive cut（自适应对角线选择）不会改变输出顺序。
 */

#pragma once

#include "ofm_types.hpp"

#include <cmath>
#include <limits>

namespace pcl::surface::rvv_ofm_support
{

inline pcl::PointCloud<pcl::PointXYZ>
makeOrganizedCloud(const int width,
                   const int height,
                   const bool inject_invalid = true)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = !inject_invalid;
  cloud.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto& p = cloud[static_cast<std::size_t>(y * width + x)];
      p.x = static_cast<float>(x) * 0.01f;
      p.y = static_cast<float>(y) * 0.01f;
      p.z = 4.0f + static_cast<float>((x * 13 + y * 7) % 29) * 0.03125f;
    }
  }

  if (!inject_invalid)
    return cloud;

  for (int y = 3; y < height; y += 17) {
    for (int x = 5; x < width; x += 23) {
      auto& p = cloud[static_cast<std::size_t>(y * width + x)];
      p.x = std::numeric_limits<float>::quiet_NaN();
      p.y = std::numeric_limits<float>::quiet_NaN();
      p.z = std::numeric_limits<float>::quiet_NaN();
    }
  }
  for (int y = 11; y < height; y += 41) {
    for (int x = 7; x < width; x += 37) {
      auto& p = cloud[static_cast<std::size_t>(y * width + x)];
      p.z = std::numeric_limits<float>::infinity();
    }
  }
  return cloud;
}

inline MeshOptions
makeOptions(const int width, const int height, const MeshKind kind)
{
  MeshOptions options;
  options.width = width;
  options.height = height;
  options.kind = kind;
  return options;
}

}  // namespace pcl::surface::rvv_ofm_support
