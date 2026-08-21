/*
 * 本文件做什么：
 * 保存 organized_pointcloud_conversion 测试支撑里跨 test / bench 共用的轻量类型、
 * checksum（校验和）和枚举。它不包含候选算法，避免 fixture、candidate 和 bench
 * harness 职责互相缠绕。
 */

#pragma once

#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

namespace pcl::io::rvv_test::organized_pointcloud_conversion {

enum class InvalidPattern {
  finite_only,
  mixed_invalid,
};

inline std::uint64_t
mixChecksum(std::uint64_t seed, const std::uint64_t value)
{
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

inline std::uint64_t
checksumDisparity(const std::vector<std::uint16_t>& disparity)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, disparity.size());
  for (std::size_t i = 0; i < disparity.size(); ++i)
    checksum = mixChecksum(checksum, static_cast<std::uint64_t>(disparity[i]) +
                                         (static_cast<std::uint64_t>(i) << 17));
  return checksum;
}

inline std::uint64_t
checksumBytes(const std::vector<std::uint8_t>& bytes)
{
  std::uint64_t checksum = 1099511628211ull;
  checksum = mixChecksum(checksum, bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i)
    checksum = mixChecksum(checksum, static_cast<std::uint64_t>(bytes[i]) +
                                         (static_cast<std::uint64_t>(i) << 11));
  return checksum;
}

inline std::uint64_t
checksumStringBytes(const std::string& bytes)
{
  std::uint64_t checksum = 1099511628211ull;
  checksum = mixChecksum(checksum, bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i)
    checksum = mixChecksum(checksum, static_cast<std::uint8_t>(bytes[i]) +
                                         (static_cast<std::uint64_t>(i) << 11));
  return checksum;
}

inline std::uint64_t
checksumAnalyzeResult(const float max_depth, const float focal_length)
{
  std::uint32_t max_depth_bits = 0;
  std::uint32_t focal_length_bits = 0;
  std::memcpy(&max_depth_bits, &max_depth, sizeof(max_depth_bits));
  std::memcpy(&focal_length_bits, &focal_length, sizeof(focal_length_bits));

  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, max_depth_bits);
  checksum = mixChecksum(checksum, focal_length_bits);
  return checksum;
}

inline std::uint64_t
checksumPointXYZCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, cloud.size());
  checksum = mixChecksum(checksum, cloud.width);
  checksum = mixChecksum(checksum, cloud.height);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    std::uint32_t x_bits = 0;
    std::uint32_t y_bits = 0;
    std::uint32_t z_bits = 0;
    std::memcpy(&x_bits, &cloud[i].x, sizeof(x_bits));
    std::memcpy(&y_bits, &cloud[i].y, sizeof(y_bits));
    std::memcpy(&z_bits, &cloud[i].z, sizeof(z_bits));
    checksum = mixChecksum(checksum, static_cast<std::uint64_t>(x_bits) +
                                         (static_cast<std::uint64_t>(i) << 7));
    checksum = mixChecksum(checksum, static_cast<std::uint64_t>(y_bits) +
                                         (static_cast<std::uint64_t>(i) << 11));
    checksum = mixChecksum(checksum, static_cast<std::uint64_t>(z_bits) +
                                         (static_cast<std::uint64_t>(i) << 17));
  }
  return checksum;
}

}  // namespace pcl::io::rvv_test::organized_pointcloud_conversion
