/*
 * 本文件做什么：
 * 本文件负责 TE2D 矩阵差异和 checksum helper。
 *
 * checksum 只用于测试 / bench 输出形状和结果指纹，不替代 numerical correctness。
 */

#pragma once

#include "te2d_core_types.hpp"

namespace pcl::registration::rvv_te2d_support {

inline float
matrixMaxAbsDiff(const Eigen::Matrix4f& a, const Eigen::Matrix4f& b)
{
  return (a - b).cwiseAbs().maxCoeff();
}

inline std::uint64_t
matrixChecksum(const Eigen::Matrix4f& matrix)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      const auto scaled = static_cast<std::int64_t>(matrix(r, c) * 1000000.0f);
      checksum = (checksum ^ static_cast<std::uint64_t>(scaled)) * 1099511628211ull;
    }
  }
  return checksum;
}

} // namespace pcl::registration::rvv_te2d_support
