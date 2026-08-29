#pragma once

/*
 * 本文件做什么：
 * 这里保存 BRISK scale-space downsample（尺度空间下采样）的标量参考链路。
 * 它复刻 `Layer::halfsample()` 和 `Layer::twothirdsample()` 的图像尺寸变换、
 * 整数加权和 row tail（行尾）语义，供 gtest 与 bench 对真实 `brisk::Layer`
 * 构造结果做逐字节对拍。
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

namespace pcl::keypoints::rvv_test::brisk_2d
{

inline std::vector<std::uint8_t>
makeSyntheticImage(const std::size_t width, const std::size_t height)
{
  std::vector<std::uint8_t> image(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const std::uint32_t value =
          static_cast<std::uint32_t>(17 * x + 31 * y + 7 * (x % 5) * (y % 3) + 11);
      image[y * width + x] = static_cast<std::uint8_t>(value & 0xffu);
    }
  }
  return image;
}

inline void
halfsampleReference(const std::vector<std::uint8_t>& src,
                    const int src_width,
                    const int src_height,
                    std::vector<std::uint8_t>& dst)
{
  const int dst_width = src_width / 2;
  const int dst_height = src_height / 2;
  dst.assign(static_cast<std::size_t>(dst_width) * static_cast<std::size_t>(dst_height), 0);

  for (int row = 0; row < dst_height; ++row)
  {
    const int src_row = row * 2;
    for (int col = 0; col < dst_width; ++col)
    {
      const int src_col = col * 2;
      const int a = src[static_cast<std::size_t>(src_row) * src_width + src_col];
      const int b = src[static_cast<std::size_t>(src_row) * src_width + src_col + 1];
      const int c = src[static_cast<std::size_t>(src_row + 1) * src_width + src_col];
      const int d = src[static_cast<std::size_t>(src_row + 1) * src_width + src_col + 1];
      dst[static_cast<std::size_t>(row) * dst_width + col] =
          static_cast<std::uint8_t>((a + b + c + d) / 4);
    }
  }
}

inline void
halfsampleCandidate(const std::vector<std::uint8_t>& src,
                    const int src_width,
                    const int src_height,
                    std::vector<std::uint8_t>& dst)
{
  halfsampleReference(src, src_width, src_height, dst);
}

inline void
twothirdsampleReference(const std::vector<std::uint8_t>& src,
                        const int src_width,
                        const int src_height,
                        std::vector<std::uint8_t>& dst)
{
  const int dst_width = 2 * src_width / 3;
  const int dst_height = 2 * src_height / 3;
  dst.assign(static_cast<std::size_t>(dst_width) * static_cast<std::size_t>(dst_height), 0);

  for (int row = 0; row + 2 < src_height; row += 3)
  {
    const int dst_row = (row / 3) * 2;
    for (int col = 0; col + 2 < src_width; col += 3)
    {
      const int dst_col = (col / 3) * 2;
      const std::size_t a = static_cast<std::size_t>(row) * src_width + col;
      const std::size_t b = static_cast<std::size_t>(row + 1) * src_width + col;
      const std::size_t c = static_cast<std::size_t>(row + 2) * src_width + col;
      const int A1 = src[a + 0];
      const int A2 = src[a + 1];
      const int A3 = src[a + 2];
      const int B1 = src[b + 0];
      const int B2 = src[b + 1];
      const int B3 = src[b + 2];
      const int C1 = src[c + 0];
      const int C2 = src[c + 1];
      const int C3 = src[c + 2];
      const std::size_t out = static_cast<std::size_t>(dst_row) * dst_width + dst_col;
      dst[out + 0] = static_cast<std::uint8_t>((4 * A1 + 2 * (A2 + B1) + B2) / 9);
      dst[out + 1] = static_cast<std::uint8_t>((4 * A3 + 2 * (A2 + B3) + B2) / 9);
      dst[out + dst_width + 0] =
          static_cast<std::uint8_t>((4 * C1 + 2 * (C2 + B1) + B2) / 9);
      dst[out + dst_width + 1] =
          static_cast<std::uint8_t>((4 * C3 + 2 * (C2 + B3) + B2) / 9);
    }
  }
}

inline void
twothirdsampleCandidate(const std::vector<std::uint8_t>& src,
                        const int src_width,
                        const int src_height,
                        std::vector<std::uint8_t>& dst)
{
  twothirdsampleReference(src, src_width, src_height, dst);
}

inline std::uint64_t
sampledChecksum(const std::vector<std::uint8_t>& data)
{
  std::uint64_t checksum = 0;
  const std::size_t step = std::max<std::size_t>(1, data.size() / 257);
  for (std::size_t i = 0; i < data.size(); i += step)
    checksum += static_cast<std::uint64_t>(data[i]) * static_cast<std::uint64_t>((i % 29) + 1);
  return checksum;
}

inline pcl::PointCloud<pcl::PointXYZRGBA>::Ptr
makeSyntheticOrganizedCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZRGBA>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(width * height);

  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const auto intensity =
          static_cast<std::uint8_t>((17 * x + 31 * y + 23 * ((x / 8) % 2) +
                                     41 * ((y / 8) % 2)) &
                                    0xffu);
      auto& point = (*cloud)(static_cast<int>(x), static_cast<int>(y));
      point.x = static_cast<float>(x) * 0.01f;
      point.y = static_cast<float>(y) * 0.01f;
      point.z = 1.0f + static_cast<float>((x + y) % 11) * 0.001f;
      point.r = intensity;
      point.g = static_cast<std::uint8_t>((intensity * 3u + 17u) & 0xffu);
      point.b = static_cast<std::uint8_t>((intensity * 5u + 29u) & 0xffu);
      point.a = 255;
    }
  }

  return cloud;
}

inline std::uint64_t
keypointChecksum(const pcl::PointCloud<pcl::PointWithScale>& keypoints)
{
  std::uint64_t checksum = static_cast<std::uint64_t>(keypoints.size()) * 1469598103934665603ull;
  const std::size_t step = std::max<std::size_t>(1, keypoints.size() / 97);
  for (std::size_t i = 0; i < keypoints.size(); i += step)
  {
    const auto& point = keypoints[i];
    checksum ^= static_cast<std::uint64_t>(point.x * 16.0f + 0.5f) + 0x9e3779b97f4a7c15ull;
    checksum ^= (static_cast<std::uint64_t>(point.y * 16.0f + 0.5f) << 11);
    checksum ^= (static_cast<std::uint64_t>(point.scale * 1024.0f + 0.5f) << 23);
  }
  return checksum;
}

} // namespace pcl::keypoints::rvv_test::brisk_2d
