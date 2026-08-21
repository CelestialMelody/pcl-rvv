/*
 * 本文件做什么：
 * 提供首阶段 PointXYZ cloud -> disparity 的 test-only RVV candidate（测试专用 RVV
 * 候选）。它和 production 标量路径对拍，用来判断该局部公式是否值得继续推进到
 * colored / decode 或 production integration（生产接入）阶段。
 *
 * 证据边界：
 * RVV path 使用 resize + 临时数组保持连续写出，这不同于 production 当前的 push_back
 * 组织。因此它只能作为 diagnostic evidence（诊断证据），不能替代 production direct
 * evidence（真实生产路径证据）。
 */

#pragma once

#include "opc_types.hpp"

#include <pcl/compression/libpng_wrapper.h>
#include <pcl/compression/impl/organized_pointcloud_compression_analysis.hpp>
#include <pcl/compression/organized_pointcloud_conversion.h>
#include <pcl/common/point_tests.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_test::organized_pointcloud_conversion {

namespace detail {

template <typename PointT>
inline void
convertCloudToDisparityScalar(const pcl::PointCloud<PointT>& cloud,
                              const float focal_length,
                              const float disparity_shift,
                              const float disparity_scale,
                              std::vector<std::uint16_t>& disparity)
{
  disparity.clear();
  disparity.reserve(cloud.size());
  for (const auto& point : cloud) {
    if (pcl::isFinite(point)) {
      disparity.push_back(static_cast<std::uint16_t>(
          focal_length / (disparity_scale * point.z) +
          disparity_shift / disparity_scale));
    }
    else {
      disparity.push_back(0);
    }
  }
}

template <typename PointT>
inline void
analyzeOrganizedCloudScalar(const pcl::PointCloud<PointT>& cloud,
                            float& max_depth,
                            float& focal_length)
{
  const std::size_t width = cloud.width;
  const std::size_t height = cloud.height;
  const int center_x = static_cast<int>(width / 2);
  const int center_y = static_cast<int>(height / 2);

  max_depth = 0.0f;
  focal_length = 0.0f;

  std::size_t index = 0;
  for (int y = -center_y; y < center_y; ++y) {
    for (int x = -center_x; x < center_x; ++x) {
      const PointT& point = cloud[index++];
      if (pcl::isFinite(point) && max_depth < point.z) {
        max_depth = point.z;
        focal_length =
            2.0f / (point.x / (static_cast<float>(x) * point.z) +
                    point.y / (static_cast<float>(y) * point.z));
      }
    }
  }
}

#if defined(__RVV10__) && defined(__riscv_vector)
inline bool
rvvClassIsFinite(const std::uint32_t cls)
{
  constexpr std::uint32_t kNotFiniteBits =
      (1u << 0u) | (1u << 7u) | (1u << 8u) | (1u << 9u);
  return (cls & kNotFiniteBits) == 0u;
}

template <typename PointT>
inline bool
analyzeOrganizedCloudRvv(const pcl::PointCloud<PointT>& cloud,
                         float& max_depth,
                         float& focal_length)
{
  if (cloud.width <= 1 || cloud.height <= 1 || cloud.width * cloud.height != cloud.size())
    return false;

  const std::size_t size = cloud.size();
  max_depth = 0.0f;
  focal_length = 0.0f;
  if (size == 0)
    return true;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  std::vector<float> z_values(vlmax, 0.0f);
  std::vector<std::uint32_t> class_x(vlmax, 0);
  std::vector<std::uint32_t> class_y(vlmax, 0);
  std::vector<std::uint32_t> class_z(vlmax, 0);

  const auto* base =
      reinterpret_cast<const unsigned char*>(cloud.points.data());
  constexpr std::ptrdiff_t stride =
      static_cast<std::ptrdiff_t>(sizeof(PointT));
  constexpr std::ptrdiff_t offset_x =
      static_cast<std::ptrdiff_t>(offsetof(PointT, x));
  constexpr std::ptrdiff_t offset_y =
      static_cast<std::ptrdiff_t>(offsetof(PointT, y));
  constexpr std::ptrdiff_t offset_z =
      static_cast<std::ptrdiff_t>(offsetof(PointT, z));

  std::size_t max_index = 0;
  bool found = false;
  std::size_t i = 0;
  while (i < size) {
    const std::size_t vl = __riscv_vsetvl_e32m1(size - i);
    const auto* x_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(PointT) + offset_x);
    const auto* y_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(PointT) + offset_y);
    const auto* z_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(PointT) + offset_z);

    const vfloat32m1_t x = __riscv_vlse32_v_f32m1(x_ptr, stride, vl);
    const vfloat32m1_t y = __riscv_vlse32_v_f32m1(y_ptr, stride, vl);
    const vfloat32m1_t z = __riscv_vlse32_v_f32m1(z_ptr, stride, vl);

    __riscv_vse32_v_f32m1(z_values.data(), z, vl);
    __riscv_vse32_v_u32m1(class_x.data(), __riscv_vfclass_v_u32m1(x, vl), vl);
    __riscv_vse32_v_u32m1(class_y.data(), __riscv_vfclass_v_u32m1(y, vl), vl);
    __riscv_vse32_v_u32m1(class_z.data(), __riscv_vfclass_v_u32m1(z, vl), vl);

    for (std::size_t lane = 0; lane < vl; ++lane) {
      if (rvvClassIsFinite(class_x[lane]) && rvvClassIsFinite(class_y[lane]) &&
          rvvClassIsFinite(class_z[lane]) && (!found || max_depth < z_values[lane])) {
        found = true;
        max_depth = z_values[lane];
        max_index = i + lane;
      }
    }
    i += vl;
  }

  if (found) {
    const int center_x = static_cast<int>(cloud.width / 2);
    const int center_y = static_cast<int>(cloud.height / 2);
    const int x = static_cast<int>(max_index % cloud.width) - center_x;
    const int y = static_cast<int>(max_index / cloud.width) - center_y;
    const PointT& point = cloud[max_index];
    focal_length =
        2.0f / (point.x / (static_cast<float>(x) * point.z) +
                point.y / (static_cast<float>(y) * point.z));
  }

  return true;
}
#endif

inline void
convertDisparityToPointXYZCloudScalar(const std::vector<std::uint16_t>& disparity,
                                      const std::size_t width,
                                      const std::size_t height,
                                      const float focal_length,
                                      const float disparity_shift,
                                      const float disparity_scale,
                                      pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  const std::size_t cloud_size = width * height;
  cloud.clear();
  cloud.reserve(cloud_size);
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = false;

  const int center_x = static_cast<int>(width / 2);
  const int center_y = static_cast<int>(height / 2);
  const float fl_const = 1.0f / focal_length;
  const float bad_point = std::numeric_limits<float>::quiet_NaN();

  std::size_t i = 0;
  for (int y = -center_y; y < center_y; ++y) {
    for (int x = -center_x; x < center_x; ++x) {
      pcl::PointXYZ point;
      const std::uint16_t pixel_disparity = disparity[i];
      ++i;
      if (pixel_disparity) {
        const float depth = focal_length /
                            (static_cast<float>(pixel_disparity) * disparity_scale +
                             disparity_shift);
        point.x = static_cast<float>(x) * depth * fl_const;
        point.y = static_cast<float>(y) * depth * fl_const;
        point.z = depth;
      }
      else {
        point.x = bad_point;
        point.y = bad_point;
        point.z = bad_point;
      }
      cloud.push_back(point);
    }
  }
}

#if defined(__RVV10__) && defined(__riscv_vector)
template <typename PointT>
inline void
convertCloudToDisparityRvv(const pcl::PointCloud<PointT>& cloud,
                           const float focal_length,
                           const float disparity_shift,
                           const float disparity_scale,
                           std::vector<std::uint16_t>& disparity)
{
  const std::size_t size = cloud.size();
  disparity.assign(size, 0);
  if (size == 0)
    return;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  std::vector<float> computed(vlmax, 0.0f);
  std::vector<std::uint32_t> class_x(vlmax, 0);
  std::vector<std::uint32_t> class_y(vlmax, 0);
  std::vector<std::uint32_t> class_z(vlmax, 0);

  const auto* base =
      reinterpret_cast<const unsigned char*>(cloud.points.data());
  constexpr std::ptrdiff_t stride =
      static_cast<std::ptrdiff_t>(sizeof(PointT));
  constexpr std::ptrdiff_t offset_x =
      static_cast<std::ptrdiff_t>(offsetof(PointT, x));
  constexpr std::ptrdiff_t offset_y =
      static_cast<std::ptrdiff_t>(offsetof(PointT, y));
  constexpr std::ptrdiff_t offset_z =
      static_cast<std::ptrdiff_t>(offsetof(PointT, z));

  const float shift_over_scale = disparity_shift / disparity_scale;
  std::size_t i = 0;
  while (i < size) {
    const std::size_t vl = __riscv_vsetvl_e32m1(size - i);
    const auto* x_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(PointT) + offset_x);
    const auto* y_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(PointT) + offset_y);
    const auto* z_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(PointT) + offset_z);

    const vfloat32m1_t x = __riscv_vlse32_v_f32m1(x_ptr, stride, vl);
    const vfloat32m1_t y = __riscv_vlse32_v_f32m1(y_ptr, stride, vl);
    const vfloat32m1_t z = __riscv_vlse32_v_f32m1(z_ptr, stride, vl);
    const vfloat32m1_t scaled_z = __riscv_vfmul_vf_f32m1(z, disparity_scale, vl);
    const vfloat32m1_t quotient =
        __riscv_vfrdiv_vf_f32m1(scaled_z, focal_length, vl);
    const vfloat32m1_t disp =
        __riscv_vfadd_vf_f32m1(quotient, shift_over_scale, vl);

    __riscv_vse32_v_f32m1(computed.data(), disp, vl);
    __riscv_vse32_v_u32m1(class_x.data(), __riscv_vfclass_v_u32m1(x, vl), vl);
    __riscv_vse32_v_u32m1(class_y.data(), __riscv_vfclass_v_u32m1(y, vl), vl);
    __riscv_vse32_v_u32m1(class_z.data(), __riscv_vfclass_v_u32m1(z, vl), vl);

    for (std::size_t lane = 0; lane < vl; ++lane) {
      if (rvvClassIsFinite(class_x[lane]) && rvvClassIsFinite(class_y[lane]) &&
          rvvClassIsFinite(class_z[lane])) {
        disparity[i + lane] = static_cast<std::uint16_t>(computed[lane]);
      }
    }
    i += vl;
  }
}

inline void
convertPointXYZRGBCloudToDisparityColorFusedRvv(
    const pcl::PointCloud<pcl::PointXYZRGB>& cloud,
    const float focal_length,
    const float disparity_shift,
    const float disparity_scale,
    const bool convert_to_mono,
    std::vector<std::uint16_t>& disparity,
    std::vector<std::uint8_t>& color)
{
  const std::size_t size = cloud.size();
  disparity.assign(size, 0);
  color.assign(convert_to_mono ? size : size * 3, 0);
  if (size == 0)
    return;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  std::vector<float> computed(vlmax, 0.0f);
  std::vector<std::uint32_t> class_x(vlmax, 0);
  std::vector<std::uint32_t> class_y(vlmax, 0);
  std::vector<std::uint32_t> class_z(vlmax, 0);

  const auto* base =
      reinterpret_cast<const unsigned char*>(cloud.points.data());
  constexpr std::ptrdiff_t stride =
      static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZRGB));
  constexpr std::ptrdiff_t offset_x =
      static_cast<std::ptrdiff_t>(offsetof(pcl::PointXYZRGB, x));
  constexpr std::ptrdiff_t offset_y =
      static_cast<std::ptrdiff_t>(offsetof(pcl::PointXYZRGB, y));
  constexpr std::ptrdiff_t offset_z =
      static_cast<std::ptrdiff_t>(offsetof(pcl::PointXYZRGB, z));

  const float shift_over_scale = disparity_shift / disparity_scale;
  std::size_t i = 0;
  while (i < size) {
    const std::size_t vl = __riscv_vsetvl_e32m1(size - i);
    const auto* x_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(pcl::PointXYZRGB) + offset_x);
    const auto* y_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(pcl::PointXYZRGB) + offset_y);
    const auto* z_ptr =
        reinterpret_cast<const float*>(base + i * sizeof(pcl::PointXYZRGB) + offset_z);

    const vfloat32m1_t x = __riscv_vlse32_v_f32m1(x_ptr, stride, vl);
    const vfloat32m1_t y = __riscv_vlse32_v_f32m1(y_ptr, stride, vl);
    const vfloat32m1_t z = __riscv_vlse32_v_f32m1(z_ptr, stride, vl);
    const vfloat32m1_t scaled_z = __riscv_vfmul_vf_f32m1(z, disparity_scale, vl);
    const vfloat32m1_t quotient =
        __riscv_vfrdiv_vf_f32m1(scaled_z, focal_length, vl);
    const vfloat32m1_t disp =
        __riscv_vfadd_vf_f32m1(quotient, shift_over_scale, vl);

    __riscv_vse32_v_f32m1(computed.data(), disp, vl);
    __riscv_vse32_v_u32m1(class_x.data(), __riscv_vfclass_v_u32m1(x, vl), vl);
    __riscv_vse32_v_u32m1(class_y.data(), __riscv_vfclass_v_u32m1(y, vl), vl);
    __riscv_vse32_v_u32m1(class_z.data(), __riscv_vfclass_v_u32m1(z, vl), vl);

    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t index = i + lane;
      if (!rvvClassIsFinite(class_x[lane]) ||
          !rvvClassIsFinite(class_y[lane]) ||
          !rvvClassIsFinite(class_z[lane])) {
        continue;
      }

      const auto& point = cloud[index];
      disparity[index] = static_cast<std::uint16_t>(computed[lane]);
      if (convert_to_mono) {
        color[index] = static_cast<std::uint8_t>(0.2989 * point.r +
                                                0.5870 * point.g +
                                                0.1140 * point.b);
      }
      else {
        const std::size_t color_index = index * 3;
        color[color_index + 0] = point.r;
        color[color_index + 1] = point.g;
        color[color_index + 2] = point.b;
      }
    }
    i += vl;
  }
}

inline void
convertDisparityToPointXYZCloudRvv(const std::vector<std::uint16_t>& disparity,
                                   const std::size_t width,
                                   const std::size_t height,
                                   const float focal_length,
                                   const float disparity_shift,
                                   const float disparity_scale,
                                   pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  const std::size_t cloud_size = width * height;
  cloud.clear();
  cloud.resize(cloud_size);
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = false;
  if (cloud_size == 0)
    return;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m1();
  std::vector<float> disparity_f(vlmax, 0.0f);
  std::vector<float> x_coord(vlmax, 0.0f);
  std::vector<float> y_coord(vlmax, 0.0f);
  std::vector<float> x_out(vlmax, 0.0f);
  std::vector<float> y_out(vlmax, 0.0f);
  std::vector<float> z_out(vlmax, 0.0f);

  const int center_x = static_cast<int>(width / 2);
  const int center_y = static_cast<int>(height / 2);
  const float fl_const = 1.0f / focal_length;
  const float bad_point = std::numeric_limits<float>::quiet_NaN();

  std::size_t i = 0;
  while (i < cloud_size) {
    const std::size_t vl = __riscv_vsetvl_e32m1(cloud_size - i);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t index = i + lane;
      disparity_f[lane] = static_cast<float>(disparity[index]);
      x_coord[lane] = static_cast<float>(static_cast<int>(index % width) - center_x);
      y_coord[lane] = static_cast<float>(static_cast<int>(index / width) - center_y);
    }

    const vfloat32m1_t disp = __riscv_vle32_v_f32m1(disparity_f.data(), vl);
    const vfloat32m1_t xs = __riscv_vle32_v_f32m1(x_coord.data(), vl);
    const vfloat32m1_t ys = __riscv_vle32_v_f32m1(y_coord.data(), vl);
    const vfloat32m1_t denom =
        __riscv_vfadd_vf_f32m1(__riscv_vfmul_vf_f32m1(disp, disparity_scale, vl),
                               disparity_shift,
                               vl);
    const vfloat32m1_t depth = __riscv_vfrdiv_vf_f32m1(denom, focal_length, vl);
    const vfloat32m1_t x_scaled =
        __riscv_vfmul_vf_f32m1(__riscv_vfmul_vv_f32m1(xs, depth, vl), fl_const, vl);
    const vfloat32m1_t y_scaled =
        __riscv_vfmul_vf_f32m1(__riscv_vfmul_vv_f32m1(ys, depth, vl), fl_const, vl);

    __riscv_vse32_v_f32m1(z_out.data(), depth, vl);
    __riscv_vse32_v_f32m1(x_out.data(), x_scaled, vl);
    __riscv_vse32_v_f32m1(y_out.data(), y_scaled, vl);

    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t index = i + lane;
      if (disparity[index]) {
        cloud[index].x = x_out[lane];
        cloud[index].y = y_out[lane];
        cloud[index].z = z_out[lane];
      }
      else {
        cloud[index].x = bad_point;
        cloud[index].y = bad_point;
        cloud[index].z = bad_point;
      }
    }
    i += vl;
  }
  cloud.width = static_cast<std::uint32_t>(cloud.size());
  cloud.height = 1;
}
#endif

}  // namespace detail

template <typename PointT>
inline void
analyzeOrganizedCloudReference(const pcl::PointCloud<PointT>& cloud,
                               float& max_depth,
                               float& focal_length)
{
  detail::analyzeOrganizedCloudScalar(cloud, max_depth, focal_length);
}

template <typename PointT>
inline void
analyzeOrganizedCloudDiagnostic(const pcl::PointCloud<PointT>& cloud,
                                float& max_depth,
                                float& focal_length)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  if (detail::analyzeOrganizedCloudRvv(cloud, max_depth, focal_length))
    return;
#endif
  pcl::io::organized_compression_detail::analyzeOrganizedCloud(
      cloud, max_depth, focal_length);
}

inline void
convertPointXYZCloudToDisparityDiagnostic(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                          const float focal_length,
                                          const float disparity_shift,
                                          const float disparity_scale,
                                          std::vector<std::uint16_t>& disparity)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  detail::convertCloudToDisparityRvv(
      cloud, focal_length, disparity_shift, disparity_scale, disparity);
#else
  detail::convertCloudToDisparityScalar(
      cloud, focal_length, disparity_shift, disparity_scale, disparity);
#endif
}

inline void
convertPointXYZRGBCloudToDisparityColorDiagnostic(
    const pcl::PointCloud<pcl::PointXYZRGB>& cloud,
    const float focal_length,
    const float disparity_shift,
    const float disparity_scale,
    const bool convert_to_mono,
    std::vector<std::uint16_t>& disparity,
    std::vector<std::uint8_t>& color)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  detail::convertCloudToDisparityRvv(
      cloud, focal_length, disparity_shift, disparity_scale, disparity);
#else
  detail::convertCloudToDisparityScalar(
      cloud, focal_length, disparity_shift, disparity_scale, disparity);
#endif

  color.clear();
  if (convert_to_mono) {
    color.reserve(cloud.size());
  }
  else {
    color.reserve(cloud.size() * 3);
  }

  for (const auto& point : cloud) {
    if (pcl::isFinite(point)) {
      if (convert_to_mono) {
        color.push_back(static_cast<std::uint8_t>(0.2989 * point.r +
                                                  0.5870 * point.g +
                                                  0.1140 * point.b));
      }
      else {
        color.push_back(point.r);
        color.push_back(point.g);
        color.push_back(point.b);
      }
    }
    else {
      if (convert_to_mono) {
        color.push_back(0);
      }
      else {
        color.push_back(0);
        color.push_back(0);
        color.push_back(0);
      }
    }
  }
}

inline void
convertPointXYZRGBCloudToDisparityColorFusedDiagnostic(
    const pcl::PointCloud<pcl::PointXYZRGB>& cloud,
    const float focal_length,
    const float disparity_shift,
    const float disparity_scale,
    const bool convert_to_mono,
    std::vector<std::uint16_t>& disparity,
    std::vector<std::uint8_t>& color)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  detail::convertPointXYZRGBCloudToDisparityColorFusedRvv(cloud,
                                                          focal_length,
                                                          disparity_shift,
                                                          disparity_scale,
                                                          convert_to_mono,
                                                          disparity,
                                                          color);
#else
  convertPointXYZRGBCloudToDisparityColorDiagnostic(cloud,
                                                    focal_length,
                                                    disparity_shift,
                                                    disparity_scale,
                                                    convert_to_mono,
                                                    disparity,
                                                    color);
#endif
}

inline void
convertDisparityToPointXYZCloudDiagnostic(const std::vector<std::uint16_t>& disparity,
                                          const std::size_t width,
                                          const std::size_t height,
                                          const float focal_length,
                                          const float disparity_shift,
                                          const float disparity_scale,
                                          pcl::PointCloud<pcl::PointXYZ>& cloud)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  detail::convertDisparityToPointXYZCloudRvv(
      disparity, width, height, focal_length, disparity_shift, disparity_scale, cloud);
#else
  detail::convertDisparityToPointXYZCloudScalar(
      disparity, width, height, focal_length, disparity_shift, disparity_scale, cloud);
#endif
}

template <typename PointT>
inline void
encodePointCloudShaped(const pcl::PointCloud<PointT>& cloud,
                       std::ostream& compressed_output,
                       const bool do_color_encoding,
                       const bool convert_to_mono,
                       const int png_level)
{
  float max_depth = 0.0f;
  float focal_length = 0.0f;
  float disparity_scale = 1.0f;
  float disparity_shift = 0.0f;
  detail::analyzeOrganizedCloudScalar(cloud, max_depth, focal_length);

  static constexpr const char* kFrameHeader = "<PCL-ORG-COMPRESSED>";
  const std::uint32_t cloud_width = cloud.width;
  const std::uint32_t cloud_height = cloud.height;
  compressed_output.write(kFrameHeader, std::char_traits<char>::length(kFrameHeader));
  compressed_output.write(reinterpret_cast<const char*>(&cloud_width), sizeof(cloud_width));
  compressed_output.write(reinterpret_cast<const char*>(&cloud_height), sizeof(cloud_height));
  compressed_output.write(reinterpret_cast<const char*>(&max_depth), sizeof(max_depth));
  compressed_output.write(reinterpret_cast<const char*>(&focal_length), sizeof(focal_length));
  compressed_output.write(reinterpret_cast<const char*>(&disparity_scale), sizeof(disparity_scale));
  compressed_output.write(reinterpret_cast<const char*>(&disparity_shift), sizeof(disparity_shift));

  std::vector<std::uint16_t> disparity;
  std::vector<std::uint8_t> color;
  std::vector<std::uint8_t> compressed_disparity;
  std::vector<std::uint8_t> compressed_color;

  pcl::io::OrganizedConversion<PointT>::convert(cloud,
                                                focal_length,
                                                disparity_shift,
                                                disparity_scale,
                                                convert_to_mono,
                                                disparity,
                                                color);
  pcl::io::encodeMonoImageToPNG(
      disparity, cloud_width, cloud_height, compressed_disparity, png_level);

  const auto disparity_size = static_cast<std::uint32_t>(compressed_disparity.size());
  compressed_output.write(reinterpret_cast<const char*>(&disparity_size),
                          sizeof(disparity_size));
  compressed_output.write(reinterpret_cast<const char*>(compressed_disparity.data()),
                          compressed_disparity.size() * sizeof(std::uint8_t));

  if (pcl::io::CompressionPointTraits<PointT>::hasColor && do_color_encoding) {
    if (convert_to_mono) {
      pcl::io::encodeMonoImageToPNG(
          color, cloud_width, cloud_height, compressed_color, 1 /* Z_BEST_SPEED */);
    }
    else {
      pcl::io::encodeRGBImageToPNG(
          color, cloud_width, cloud_height, compressed_color, 1 /* Z_BEST_SPEED */);
    }
  }

  const auto color_size = static_cast<std::uint32_t>(compressed_color.size());
  compressed_output.write(reinterpret_cast<const char*>(&color_size), sizeof(color_size));
  compressed_output.write(reinterpret_cast<const char*>(compressed_color.data()),
                          compressed_color.size() * sizeof(std::uint8_t));
  compressed_output.flush();
}

}  // namespace pcl::io::rvv_test::organized_pointcloud_conversion
