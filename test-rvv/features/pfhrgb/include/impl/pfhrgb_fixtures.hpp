#pragma once

/*
 * 本文件提供 PFHRGB topic 的 synthetic fixtures（合成测试夹具）。
 * 测试和 bench 共享同一批输入构造，避免 correctness（正确性）与
 * benchmark（性能测试）使用不同数据后产生 comparison-boundary mismatch
 * （比较边界不一致）。
 */

#include "impl/pfhrgb_reference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pcl::features::rvv_test::pfhrgb
{

inline CloudT::Ptr
makeFeatureCloud(const std::size_t side)
{
  auto cloud = pcl::make_shared<CloudT>();
  cloud->reserve(side * side);
  for (std::size_t y = 0; y < side; ++y)
  {
    for (std::size_t x = 0; x < side; ++x)
    {
      PointT point;
      point.x = static_cast<float>(x) * 0.17f + static_cast<float>(y % 3) * 0.011f;
      point.y = static_cast<float>(y) * 0.13f + static_cast<float>(x % 5) * 0.007f;
      point.z = 0.05f * static_cast<float>((x + 2 * y) % 11) + 0.03f;
      point.normal_x = 0.3f + 0.01f * static_cast<float>(x % 7);
      point.normal_y = 0.4f + 0.01f * static_cast<float>(y % 5);
      point.normal_z = std::sqrt(std::max(0.05f, 1.0f - point.normal_x * point.normal_x -
                                                  point.normal_y * point.normal_y));
      point.r = static_cast<std::uint8_t>(30 + (x * 17 + y * 3) % 210);
      point.g = static_cast<std::uint8_t>(20 + (x * 5 + y * 19) % 220);
      point.b = static_cast<std::uint8_t>(10 + (x * 11 + y * 7) % 230);
      cloud->push_back(point);
    }
  }
  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline pcl::Indices
makeWrappedNeighborhood(const CloudT& cloud, const std::size_t count, const std::size_t step)
{
  pcl::Indices indices;
  indices.reserve(count);
  std::size_t cursor = 0;
  for (std::size_t i = 0; i < count; ++i)
  {
    indices.push_back(static_cast<int>(cursor % cloud.size()));
    cursor += step;
  }
  return indices;
}

} // namespace pcl::features::rvv_test::pfhrgb
