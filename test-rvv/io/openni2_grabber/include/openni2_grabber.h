#pragma once

/*
 * 本文件做什么：
 * 这里放 OpenNI2Grabber frame-to-cloud（帧到点云）topic 的测试专用
 * reference（参考链路）和 RVV candidate（候选链路）。它复刻
 * `io/src/openni2_grabber.cpp` 中逐像素 depth projection（深度反投影）
 * 和 RGB overlay（颜色覆盖）的核心语义，供 gtest 和 bench 对拍。
 *
 * 证据边界：
 * 本文件不属于 production（生产源码），也不改变 OpenNI2Grabber 的公开
 * 行为。`__RVV10__` 关闭或候选暂未覆盖时，candidate 会回到同一份标量
 * 参考链路；因此它只能证明 production-shaped diagnostic（生产形态诊断）
 * 层面的 correctness（正确性），不能证明 production dispatch（生产分流）。
 */

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pcl/point_types.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_openni2_grabber_support {

struct CameraModel {
  float fx;
  float fy;
  float cx;
  float cy;
  std::uint16_t no_sample_value;
  std::uint16_t shadow_value;
};

inline bool
isInvalidDepthPixel(const std::uint16_t pixel, const CameraModel& camera)
{
  return pixel == 0 || pixel == camera.no_sample_value || pixel == camera.shadow_value;
}

inline void
fillXYZCloudScalar(const std::uint16_t* depth,
                   const unsigned width,
                   const unsigned height,
                   const CameraModel& camera,
                   pcl::PointXYZ* cloud)
{
  const float fx_inv = 1.0f / camera.fx;
  const float fy_inv = 1.0f / camera.fy;
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  std::size_t index = 0;

  for (unsigned v = 0; v < height; ++v) {
    for (unsigned u = 0; u < width; ++u, ++index) {
      auto& point = cloud[index];
      const std::uint16_t pixel = depth[index];
      if (isInvalidDepthPixel(pixel, camera)) {
        point.x = point.y = point.z = bad_point;
        continue;
      }
      point.z = static_cast<float>(pixel) * 0.001f;
      point.x = (static_cast<float>(u) - camera.cx) * point.z * fx_inv;
      point.y = (static_cast<float>(v) - camera.cy) * point.z * fy_inv;
    }
  }
}

template <typename PointT>
inline void
fillRGBOverlayScalar(const std::uint8_t* rgb,
                     const unsigned width,
                     const unsigned height,
                     const unsigned cloud_width,
                     PointT* cloud)
{
  const unsigned step = cloud_width / width;
  const unsigned skip = cloud_width - width * step;
  std::size_t rgb_index = 0;
  std::size_t point_index = 0;

  for (unsigned y = 0; y < height; ++y, point_index += skip) {
    for (unsigned x = 0; x < width; ++x, point_index += step, rgb_index += 3) {
      auto& point = cloud[point_index];
      point.r = rgb[rgb_index];
      point.g = rgb[rgb_index + 1];
      point.b = rgb[rgb_index + 2];
      point.a = 255;
    }
  }
}

template <typename PointT>
inline void
fillXYZRGBAFromDepthScalar(const std::uint16_t* depth,
                           const unsigned depth_width,
                           const unsigned height,
                           const unsigned cloud_width,
                           const CameraModel& camera,
                           PointT* cloud)
{
  const float fx_inv = 1.0f / camera.fx;
  const float fy_inv = 1.0f / camera.fy;
  const unsigned step = cloud_width / depth_width;
  const unsigned skip = cloud_width - depth_width * step;
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  std::size_t depth_index = 0;
  std::size_t point_index = 0;

  for (unsigned v = 0; v < height; ++v, point_index += skip) {
    for (unsigned u = 0; u < depth_width; ++u, ++depth_index, point_index += step) {
      auto& point = cloud[point_index];
      const std::uint16_t pixel = depth[depth_index];
      if (isInvalidDepthPixel(pixel, camera)) {
        point.x = point.y = point.z = bad_point;
        continue;
      }
      point.z = static_cast<float>(pixel) * 0.001f;
      point.x = (static_cast<float>(u) - camera.cx) * point.z * fx_inv;
      point.y = (static_cast<float>(v) - camera.cy) * point.z * fy_inv;
    }
  }
}

inline void
fillXYZRGBFromDepthScalar(const std::uint16_t* depth,
                          const unsigned depth_width,
                          const unsigned height,
                          const unsigned cloud_width,
                          const CameraModel& camera,
                          pcl::PointXYZRGB* cloud)
{
  fillXYZRGBAFromDepthScalar(depth, depth_width, height, cloud_width, camera, cloud);
}

inline void
fillXYZICloudScalar(const std::uint16_t* depth,
                    const std::uint16_t* ir,
                    const unsigned width,
                    const unsigned height,
                    const CameraModel& camera,
                    pcl::PointXYZI* cloud)
{
  const float fx_inv = 1.0f / camera.fx;
  const float fy_inv = 1.0f / camera.fy;
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  std::size_t index = 0;

  for (unsigned v = 0; v < height; ++v) {
    for (unsigned u = 0; u < width; ++u, ++index) {
      auto& point = cloud[index];
      const std::uint16_t pixel = depth[index];
      if (isInvalidDepthPixel(pixel, camera)) {
        point.x = point.y = point.z = bad_point;
      }
      else {
        point.z = static_cast<float>(pixel) * 0.001f;
        point.x = (static_cast<float>(u) - camera.cx) * point.z * fx_inv;
        point.y = (static_cast<float>(v) - camera.cy) * point.z * fy_inv;
      }

      point.data_c[0] = point.data_c[1] = point.data_c[2] = point.data_c[3] = 0;
      point.intensity = static_cast<float>(ir[index]);
    }
  }
}

#if defined(__RVV10__)
inline vbool8_t
invalidMask(const vuint16m2_t pixels, const CameraModel& camera, const std::size_t vl)
{
  vbool8_t invalid = __riscv_vmseq_vx_u16m2_b8(pixels, 0, vl);
  invalid = __riscv_vmor_mm_b8(
      invalid, __riscv_vmseq_vx_u16m2_b8(pixels, camera.no_sample_value, vl), vl);
  invalid = __riscv_vmor_mm_b8(
      invalid, __riscv_vmseq_vx_u16m2_b8(pixels, camera.shadow_value, vl), vl);
  return invalid;
}

inline vfloat32m4_t
pixelsToMeters(const vuint16m2_t pixels, const std::size_t vl)
{
  const vuint32m4_t widened = __riscv_vwaddu_vx_u32m4(pixels, 0, vl);
  return __riscv_vfmul_vf_f32m4(__riscv_vfcvt_f_xu_v_f32m4(widened, vl), 0.001f, vl);
}

template <typename PointT>
inline void
fillXYZMappedRVV(const std::uint16_t* depth,
                 const unsigned depth_width,
                 const unsigned height,
                 const unsigned cloud_width,
                 const CameraModel& camera,
                 PointT* cloud)
{
  const float fx_inv = 1.0f / camera.fx;
  const float fy_inv = 1.0f / camera.fy;
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  const unsigned step = cloud_width / depth_width;
  const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(PointT) * step);
  auto* base = reinterpret_cast<unsigned char*>(cloud);
  auto* x_base = reinterpret_cast<float*>(base + offsetof(PointT, x));
  auto* y_base = reinterpret_cast<float*>(base + offsetof(PointT, y));
  auto* z_base = reinterpret_cast<float*>(base + offsetof(PointT, z));

  for (unsigned v = 0; v < height; ++v) {
    const std::size_t depth_row_offset = static_cast<std::size_t>(v) * depth_width;
    const std::size_t point_row_offset = static_cast<std::size_t>(v) * cloud_width;
    for (unsigned u = 0; u < depth_width;) {
      const std::size_t vl = __riscv_vsetvl_e16m2(depth_width - u);
      const vuint16m2_t pixels = __riscv_vle16_v_u16m2(depth + depth_row_offset + u, vl);
      const vbool8_t invalid = invalidMask(pixels, camera, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8(invalid, vl);
      const vfloat32m4_t z = pixelsToMeters(pixels, vl);
      vuint32m4_t lane_u = __riscv_vid_v_u32m4(vl);
      lane_u = __riscv_vadd_vx_u32m4(lane_u, u, vl);
      const vfloat32m4_t uf = __riscv_vfcvt_f_xu_v_f32m4(lane_u, vl);
      const vfloat32m4_t x =
          __riscv_vfmul_vf_f32m4(
              __riscv_vfmul_vv_f32m4(
                  __riscv_vfsub_vf_f32m4(uf, camera.cx, vl), z, vl),
              fx_inv,
              vl);
      const vfloat32m4_t y =
          __riscv_vfmul_vf_f32m4(
              __riscv_vfmul_vv_f32m4(
                  __riscv_vfmv_v_f_f32m4(static_cast<float>(v) - camera.cy, vl),
                  z,
                  vl),
              fy_inv,
              vl);
      const vfloat32m4_t bad = __riscv_vfmv_v_f_f32m4(bad_point, vl);
      const std::size_t offset = point_row_offset + static_cast<std::size_t>(u) * step;

      __riscv_vsse32_v_f32m4(x_base + offset * (sizeof(PointT) / sizeof(float)),
                             point_stride,
                             bad,
                             vl);
      __riscv_vsse32_v_f32m4(y_base + offset * (sizeof(PointT) / sizeof(float)),
                             point_stride,
                             bad,
                             vl);
      __riscv_vsse32_v_f32m4(z_base + offset * (sizeof(PointT) / sizeof(float)),
                             point_stride,
                             bad,
                             vl);
      __riscv_vsse32_v_f32m4_m(valid,
                               x_base + offset * (sizeof(PointT) / sizeof(float)),
                               point_stride,
                               x,
                               vl);
      __riscv_vsse32_v_f32m4_m(valid,
                               y_base + offset * (sizeof(PointT) / sizeof(float)),
                               point_stride,
                               y,
                               vl);
      __riscv_vsse32_v_f32m4_m(valid,
                               z_base + offset * (sizeof(PointT) / sizeof(float)),
                               point_stride,
                               z,
                               vl);
      u += static_cast<unsigned>(vl);
    }
  }
}

inline void
fillXYZCloudRVV(const std::uint16_t* depth,
                const unsigned width,
                const unsigned height,
                const CameraModel& camera,
                pcl::PointXYZ* cloud)
{
  fillXYZMappedRVV(depth, width, height, width, camera, cloud);
}

inline void
fillXYZRGBAFromDepthRVV(const std::uint16_t* depth,
                        const unsigned depth_width,
                        const unsigned height,
                        const unsigned cloud_width,
                        const CameraModel& camera,
                        pcl::PointXYZRGBA* cloud)
{
  fillXYZMappedRVV(depth, depth_width, height, cloud_width, camera, cloud);
}

template <typename PointT>
inline void
fillRGBOverlayRVV(const std::uint8_t* rgb,
                  const unsigned width,
                  const unsigned height,
                  const unsigned cloud_width,
                  PointT* cloud)
{
  const unsigned step = cloud_width / width;
  const unsigned skip = cloud_width - width * step;
  const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(PointT) * step);
  std::size_t rgb_index = 0;
  std::size_t point_index = 0;

  for (unsigned y = 0; y < height; ++y, point_index += skip) {
    for (unsigned x = 0; x < width;) {
      const std::size_t vl = __riscv_vsetvl_e8m1(width - x);
      const vuint8m1x3_t channels = __riscv_vlseg3e8_v_u8m1x3(rgb + rgb_index, vl);
      const vuint16m2_t red16 =
          __riscv_vwaddu_vx_u16m2(__riscv_vget_v_u8m1x3_u8m1(channels, 0), 0, vl);
      const vuint16m2_t green16 =
          __riscv_vwaddu_vx_u16m2(__riscv_vget_v_u8m1x3_u8m1(channels, 1), 0, vl);
      const vuint16m2_t blue16 =
          __riscv_vwaddu_vx_u16m2(__riscv_vget_v_u8m1x3_u8m1(channels, 2), 0, vl);
      const vuint32m4_t red =
          __riscv_vwaddu_vx_u32m4(red16, 0, vl);
      const vuint32m4_t green =
          __riscv_vsll_vx_u32m4(__riscv_vwaddu_vx_u32m4(green16, 0, vl), 8, vl);
      const vuint32m4_t blue =
          __riscv_vwaddu_vx_u32m4(blue16, 0, vl);
      vuint32m4_t packed = __riscv_vor_vv_u32m4(blue, green, vl);
      packed = __riscv_vor_vv_u32m4(packed, __riscv_vsll_vx_u32m4(red, 16, vl), vl);
      packed = __riscv_vor_vx_u32m4(packed, 0xff000000u, vl);
      auto* rgba =
          reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(cloud + point_index) +
                                           offsetof(PointT, rgba));
      __riscv_vsse32_v_u32m4(rgba, point_stride, packed, vl);

      x += static_cast<unsigned>(vl);
      rgb_index += vl * 3;
      point_index += vl * step;
    }
  }
}
#endif

inline void
fillXYZCloudCandidate(const std::uint16_t* depth,
                      const unsigned width,
                      const unsigned height,
                      const CameraModel& camera,
                      pcl::PointXYZ* cloud)
{
#if defined(__RVV10__)
  fillXYZCloudRVV(depth, width, height, camera, cloud);
#else
  fillXYZCloudScalar(depth, width, height, camera, cloud);
#endif
}

inline void
fillRGBOverlayCandidate(const std::uint8_t* rgb,
                        const unsigned width,
                        const unsigned height,
                        const unsigned cloud_width,
                        pcl::PointXYZRGBA* cloud)
{
#if defined(__RVV10__)
  fillRGBOverlayRVV(rgb, width, height, cloud_width, cloud);
#else
  fillRGBOverlayScalar(rgb, width, height, cloud_width, cloud);
#endif
}

inline void
fillRGBOverlayCandidate(const std::uint8_t* rgb,
                        const unsigned width,
                        const unsigned height,
                        const unsigned cloud_width,
                        pcl::PointXYZRGB* cloud)
{
#if defined(__RVV10__)
  fillRGBOverlayRVV(rgb, width, height, cloud_width, cloud);
#else
  fillRGBOverlayScalar(rgb, width, height, cloud_width, cloud);
#endif
}

inline void
fillXYZRGBAFromDepthCandidate(const std::uint16_t* depth,
                              const unsigned depth_width,
                              const unsigned height,
                              const unsigned cloud_width,
                              const CameraModel& camera,
                              pcl::PointXYZRGBA* cloud)
{
#if defined(__RVV10__)
  fillXYZRGBAFromDepthRVV(depth, depth_width, height, cloud_width, camera, cloud);
#else
  fillXYZRGBAFromDepthScalar(depth, depth_width, height, cloud_width, camera, cloud);
#endif
}

inline void
fillXYZRGBFromDepthCandidate(const std::uint16_t* depth,
                             const unsigned depth_width,
                             const unsigned height,
                             const unsigned cloud_width,
                             const CameraModel& camera,
                             pcl::PointXYZRGB* cloud)
{
#if defined(__RVV10__)
  fillXYZMappedRVV(depth, depth_width, height, cloud_width, camera, cloud);
#else
  fillXYZRGBFromDepthScalar(depth, depth_width, height, cloud_width, camera, cloud);
#endif
}

inline void
fillXYZICloudCandidate(const std::uint16_t* depth,
                       const std::uint16_t* ir,
                       const unsigned width,
                       const unsigned height,
                       const CameraModel& camera,
                       pcl::PointXYZI* cloud)
{
  fillXYZICloudScalar(depth, ir, width, height, camera, cloud);
}

} // namespace pcl::io::rvv_openni2_grabber_support
