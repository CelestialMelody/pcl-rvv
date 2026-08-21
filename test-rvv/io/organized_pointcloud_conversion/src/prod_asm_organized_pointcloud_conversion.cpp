/*
 * 本文件做什么：
 * 这是 production asm probe（生产反汇编探针）。
 * 它调用 PCL public OrganizedConversion encode 入口，以及 Phase 080 新增的
 * analyze production-detail helper（生产 detail helper），不 include topic diagnostic helper。
 * `check_production_rvv_asm` 会在禁用 compiler auto-vectorization（编译器自动向量化）后
 * 检查 RVV build 是否真的从 production header 生成手写 RVV 指令。
 */

#include <pcl/compression/organized_pointcloud_conversion.h>
#include <pcl/compression/impl/organized_pointcloud_compression_analysis.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

constexpr float kFocalLength = 525.0f;
constexpr float kDisparityShift = 2.0f;
constexpr float kDisparityScale = 0.5f;

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

pcl::PointCloud<pcl::PointXYZ>
makePointXYZCloud(const std::size_t size)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(size);
  for (std::size_t i = 0; i < size; ++i) {
    cloud[i].x = static_cast<float>((i % 257) + 1) * 0.01f;
    cloud[i].y = static_cast<float>((i % 131) + 3) * 0.02f;
    cloud[i].z = 0.35f + static_cast<float>((i * 37) % 2000) * 0.0025f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZRGB>
makePointXYZRGBCloud(const std::size_t size)
{
  pcl::PointCloud<pcl::PointXYZRGB> cloud;
  cloud.width = static_cast<std::uint32_t>(size);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(size);
  for (std::size_t i = 0; i < size; ++i) {
    cloud[i].x = static_cast<float>((i % 257) + 1) * 0.01f;
    cloud[i].y = static_cast<float>((i % 131) + 3) * 0.02f;
    cloud[i].z = 0.35f + static_cast<float>((i * 37) % 2000) * 0.0025f;
    cloud[i].r = static_cast<std::uint8_t>((i * 3) & 0xffu);
    cloud[i].g = static_cast<std::uint8_t>((i * 5 + 17) & 0xffu);
    cloud[i].b = static_cast<std::uint8_t>((i * 7 + 29) & 0xffu);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
makeOrganizedPointXYZCloud(const std::size_t width, const std::size_t height)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = true;
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
    }
  }

  return cloud;
}

std::uint64_t
checksumDisparity(const std::vector<std::uint16_t>& disparity)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (std::size_t i = 0; i < disparity.size(); ++i)
    checksum ^= static_cast<std::uint64_t>(disparity[i]) + (i << 17);
  return checksum;
}

std::uint64_t
checksumBytes(const std::vector<std::uint8_t>& bytes)
{
  std::uint64_t checksum = 1099511628211ull;
  for (std::size_t i = 0; i < bytes.size(); ++i)
    checksum ^= static_cast<std::uint64_t>(bytes[i]) + (i << 11);
  return checksum;
}

std::uint64_t
checksumAnalyze(const float max_depth, const float focal_length)
{
  std::uint64_t checksum = 1469598103934665603ull;
  std::uint32_t max_bits = 0;
  std::uint32_t focal_bits = 0;
  std::memcpy(&max_bits, &max_depth, sizeof(max_bits));
  std::memcpy(&focal_bits, &focal_length, sizeof(focal_bits));
  checksum ^= static_cast<std::uint64_t>(max_bits) + 0x9e3779b97f4a7c15ull;
  checksum ^= static_cast<std::uint64_t>(focal_bits) + (checksum << 6) + (checksum >> 2);
  return checksum;
}

__attribute__((noinline)) std::uint64_t
runProductionAnalyzeProbe(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  float max_depth = 0.0f;
  float focal_length = 0.0f;
  pcl::io::organized_compression_detail::analyzeOrganizedCloud(
      cloud, max_depth, focal_length);
  return checksumAnalyze(max_depth, focal_length);
}

}  // namespace

int
main()
{
  const auto xyz = makePointXYZCloud(4096);
  const auto rgb = makePointXYZRGBCloud(4096);
  const auto organized = makeOrganizedPointXYZCloud(128, 96);

  std::vector<std::uint16_t> disparity;
  std::vector<std::uint8_t> color;
  pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(xyz,
                                                       kFocalLength,
                                                       kDisparityShift,
                                                       kDisparityScale,
                                                       false,
                                                       disparity,
                                                       color);
  const std::uint64_t xyz_checksum = checksumDisparity(disparity);

  disparity.clear();
  color.clear();
  pcl::io::OrganizedConversion<pcl::PointXYZRGB>::convert(rgb,
                                                          kFocalLength,
                                                          kDisparityShift,
                                                          kDisparityScale,
                                                          false,
                                                          disparity,
                                                          color);
  const std::uint64_t rgb_checksum = checksumDisparity(disparity) ^ checksumBytes(color);

  const std::uint64_t analyze_checksum = runProductionAnalyzeProbe(organized);
  const std::uint64_t checksum = xyz_checksum ^ (rgb_checksum << 1) ^ (analyze_checksum << 3);
  doNotOptimize(checksum);
  std::cout << checksum << '\n';
  return 0;
}
