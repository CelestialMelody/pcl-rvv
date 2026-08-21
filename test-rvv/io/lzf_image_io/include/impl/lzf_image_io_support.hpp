#pragma once

/*
 * 本文件做什么：
 * 这个内部头文件承载 lzf_image_io 的 production-shaped diagnostic
 *（生产形态诊断）实现。reference helper 逐句复刻当前 production
 * post-decompress conversion（解压后转换）语义；RVV helper 只在
 * `__RVV10__` 下启用，用来判断这些转换循环是否值得后续接入 production。
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_lzf_image_io_support {

struct PointXYZRGB {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  std::uint8_t b = 0;
  std::uint8_t g = 0;
  std::uint8_t r = 0;
  std::uint8_t a = 0;
};

struct DepthCameraParameters {
  float focal_length_x = 525.0f;
  float focal_length_y = 525.0f;
  float principal_point_x = 0.0f;
  float principal_point_y = 0.0f;
  float z_multiplication_factor = 0.001f;
};

inline std::uint8_t
clipByte(const int value)
{
  return static_cast<std::uint8_t>(value > 255 ? 255 : (value < 0 ? 0 : value));
}

inline std::uint32_t
floatBits(const float value)
{
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline void
convertDepthToCloudScalar(const std::uint16_t* depth,
                          const unsigned width,
                          const unsigned height,
                          const DepthCameraParameters& params,
                          PointXYZRGB* cloud,
                          bool* is_dense)
{
  *is_dense = true;
  const float inv_fx = 1.0f / params.focal_length_x;
  const float inv_fy = 1.0f / params.focal_length_y;
  for (unsigned v = 0; v < height; ++v) {
    for (unsigned u = 0; u < width; ++u) {
      const auto index = static_cast<std::size_t>(v) * width + u;
      PointXYZRGB& pt = cloud[index];
      const std::uint16_t val = depth[index];
      if (val == 0) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        pt.x = nan;
        pt.y = nan;
        pt.z = nan;
        *is_dense = false;
        continue;
      }
      pt.z = static_cast<float>(val) * params.z_multiplication_factor;
      pt.x = (static_cast<float>(u) - params.principal_point_x) * pt.z * inv_fx;
      pt.y = (static_cast<float>(v) - params.principal_point_y) * pt.z * inv_fy;
    }
  }
}

inline void
convertPlanarYuv422ToRgbScalar(const std::uint8_t* yuv,
                               const unsigned width,
                               const unsigned height,
                               PointXYZRGB* cloud)
{
  const auto pixels = static_cast<std::size_t>(width) * height;
  const auto pairs = pixels / 2;
  const auto* color_u = yuv;
  const auto* color_y = yuv + pairs;
  const auto* color_v = yuv + pairs + pixels;

  std::size_t y_idx = 0;
  for (std::size_t i = 0; i < pairs; ++i, y_idx += 2) {
    const int v = static_cast<int>(color_v[i]) - 128;
    const int u = static_cast<int>(color_u[i]) - 128;
    const int r_delta = (v * 18678 + 8192) >> 14;
    const int g_delta = (v * -9519 - u * 6472 + 8192) >> 14;
    const int b_delta = (u * 33292 + 8192) >> 14;

    PointXYZRGB& pt1 = cloud[y_idx + 0];
    pt1.r = clipByte(static_cast<int>(color_y[y_idx + 0]) + r_delta);
    pt1.g = clipByte(static_cast<int>(color_y[y_idx + 0]) + g_delta);
    pt1.b = clipByte(static_cast<int>(color_y[y_idx + 0]) + b_delta);

    PointXYZRGB& pt2 = cloud[y_idx + 1];
    pt2.r = clipByte(static_cast<int>(color_y[y_idx + 1]) + r_delta);
    pt2.g = clipByte(static_cast<int>(color_y[y_idx + 1]) + g_delta);
    pt2.b = clipByte(static_cast<int>(color_y[y_idx + 1]) + b_delta);
  }
}

inline void
copyRgbBufferToCloudScalar(const std::uint8_t* rgb,
                           const unsigned width,
                           const unsigned height,
                           PointXYZRGB* cloud)
{
  const auto pixels = static_cast<std::size_t>(width) * height;
  for (std::size_t i = 0; i < pixels; ++i) {
    PointXYZRGB& pt = cloud[i];
    const auto rgb_idx = i * 3;
    pt.b = rgb[rgb_idx + 2];
    pt.g = rgb[rgb_idx + 1];
    pt.r = rgb[rgb_idx + 0];
  }
}

#if defined(__RVV10__)
inline vint32m2_t
u8ToI32(const vuint8mf2_t value, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1(value, vl);
  return __riscv_vwadd_vx_i32m2(__riscv_vreinterpret_v_u16m1_i16m1(widened), 0, vl);
}

inline vint32m2_t
u8OffsetToI32(const vuint8mf2_t value, const int offset, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1(value, vl);
  const vuint16m1_t shifted = __riscv_vsub_vx_u16m1(widened, offset, vl);
  return __riscv_vwadd_vx_i32m2(__riscv_vreinterpret_v_u16m1_i16m1(shifted), 0, vl);
}

inline vuint8mf2_t
clipI32ToU8(vint32m2_t value, const std::size_t vl)
{
  value = __riscv_vmax_vx_i32m2(value, 0, vl);
  value = __riscv_vmin_vx_i32m2(value, 255, vl);
  const vuint32m2_t value_u32 = __riscv_vreinterpret_v_i32m2_u32m2(value);
  const vuint16m1_t value_u16 = __riscv_vncvt_x_x_w_u16m1(value_u32, vl);
  return __riscv_vncvt_x_x_w_u8mf2(value_u16, vl);
}

inline vuint8mf2_t
rgbChannel(const vint32m2_t y, vint32m2_t delta, const std::size_t vl)
{
  delta = __riscv_vadd_vx_i32m2(delta, 8192, vl);
  delta = __riscv_vsra_vx_i32m2(delta, 14, vl);
  return clipI32ToU8(__riscv_vadd_vv_i32m2(y, delta, vl), vl);
}

inline void
storeRgbFields(PointXYZRGB* points,
               const std::ptrdiff_t point_stride,
               const vuint8mf2_t r,
               const vuint8mf2_t g,
               const vuint8mf2_t b,
               const std::size_t vl)
{
  __riscv_vsse8_v_u8mf2(&points->r, point_stride, r, vl);
  __riscv_vsse8_v_u8mf2(&points->g, point_stride, g, vl);
  __riscv_vsse8_v_u8mf2(&points->b, point_stride, b, vl);
}

inline void
convertDepthToCloudRVV(const std::uint16_t* depth,
                       const unsigned width,
                       const unsigned height,
                       const DepthCameraParameters& params,
                       PointXYZRGB* cloud,
                       bool* is_dense)
{
  *is_dense = true;
  const float inv_fx = 1.0f / params.focal_length_x;
  const float inv_fy = 1.0f / params.focal_length_y;
  const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(PointXYZRGB));
  const auto nan = std::numeric_limits<float>::quiet_NaN();

  for (unsigned row = 0; row < height; ++row) {
    const auto row_base = static_cast<std::size_t>(row) * width;
    for (unsigned col = 0; col < width;) {
      const std::size_t vl = __riscv_vsetvl_e16m1(width - col);
      const auto index = row_base + col;
      const vuint16m1_t raw = __riscv_vle16_v_u16m1(depth + index, vl);
      const vuint32m2_t raw32 = __riscv_vzext_vf2_u32m2(raw, vl);
      const vfloat32m2_t z =
          __riscv_vfmul_vf_f32m2(__riscv_vfcvt_f_xu_v_f32m2(raw32, vl),
                                 params.z_multiplication_factor,
                                 vl);
      const vuint32m2_t u_idx =
          __riscv_vadd_vx_u32m2(__riscv_vid_v_u32m2(vl), col, vl);
      const vfloat32m2_t u =
          __riscv_vfsub_vf_f32m2(__riscv_vfcvt_f_xu_v_f32m2(u_idx, vl),
                                 params.principal_point_x,
                                 vl);
      const vfloat32m2_t y_offset = __riscv_vfmv_v_f_f32m2(
          static_cast<float>(row) - params.principal_point_y, vl);
      const vfloat32m2_t x =
          __riscv_vfmul_vf_f32m2(__riscv_vfmul_vv_f32m2(u, z, vl), inv_fx, vl);
      const vfloat32m2_t y = __riscv_vfmul_vf_f32m2(
          __riscv_vfmul_vv_f32m2(y_offset, z, vl), inv_fy, vl);

      PointXYZRGB* out = cloud + index;
      __riscv_vsse32_v_f32m2(&out->x, point_stride, x, vl);
      __riscv_vsse32_v_f32m2(&out->y, point_stride, y, vl);
      __riscv_vsse32_v_f32m2(&out->z, point_stride, z, vl);

      for (std::size_t lane = 0; lane < vl; ++lane) {
        if (depth[index + lane] == 0) {
          out[lane].x = nan;
          out[lane].y = nan;
          out[lane].z = nan;
          *is_dense = false;
        }
      }

      col += static_cast<unsigned>(vl);
    }
  }
}

inline void
convertPlanarYuv422ToRgbRVV(const std::uint8_t* yuv,
                            const unsigned width,
                            const unsigned height,
                            PointXYZRGB* cloud)
{
  const auto pixels = static_cast<std::size_t>(width) * height;
  const auto pairs = pixels / 2;
  const auto* color_u = yuv;
  const auto* color_y = yuv + pairs;
  const auto* color_v = yuv + pairs + pixels;
  const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(PointXYZRGB) * 2);

  for (std::size_t pair = 0; pair < pairs;) {
    const std::size_t vl = __riscv_vsetvl_e8mf2(pairs - pair);
    const vuint8mf2_t u8 = __riscv_vle8_v_u8mf2(color_u + pair, vl);
    const vuint8mf2_t v8 = __riscv_vle8_v_u8mf2(color_v + pair, vl);
    const vuint8mf2_t y1_8 = __riscv_vlse8_v_u8mf2(color_y + pair * 2, 2, vl);
    const vuint8mf2_t y2_8 = __riscv_vlse8_v_u8mf2(color_y + pair * 2 + 1, 2, vl);

    const vint32m2_t u = u8OffsetToI32(u8, 128, vl);
    const vint32m2_t v = u8OffsetToI32(v8, 128, vl);
    const vint32m2_t y1 = u8ToI32(y1_8, vl);
    const vint32m2_t y2 = u8ToI32(y2_8, vl);

    const vint32m2_t r_delta = __riscv_vmul_vx_i32m2(v, 18678, vl);
    vint32m2_t g_delta = __riscv_vmul_vx_i32m2(v, -9519, vl);
    g_delta = __riscv_vsub_vv_i32m2(g_delta, __riscv_vmul_vx_i32m2(u, 6472, vl), vl);
    const vint32m2_t b_delta = __riscv_vmul_vx_i32m2(u, 33292, vl);

    storeRgbFields(cloud + pair * 2,
                   point_stride,
                   rgbChannel(y1, r_delta, vl),
                   rgbChannel(y1, g_delta, vl),
                   rgbChannel(y1, b_delta, vl),
                   vl);
    storeRgbFields(cloud + pair * 2 + 1,
                   point_stride,
                   rgbChannel(y2, r_delta, vl),
                   rgbChannel(y2, g_delta, vl),
                   rgbChannel(y2, b_delta, vl),
                   vl);

    pair += vl;
  }
}

inline void
copyRgbBufferToCloudRVV(const std::uint8_t* rgb,
                        const unsigned width,
                        const unsigned height,
                        PointXYZRGB* cloud)
{
  const auto pixels = static_cast<std::size_t>(width) * height;
  const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(PointXYZRGB));
  for (std::size_t i = 0; i < pixels;) {
    const std::size_t vl = __riscv_vsetvl_e8mf2(pixels - i);
    const vuint8mf2x3_t triplet = __riscv_vlseg3e8_v_u8mf2x3(rgb + i * 3, vl);
    storeRgbFields(cloud + i,
                   point_stride,
                   __riscv_vget_v_u8mf2x3_u8mf2(triplet, 0),
                   __riscv_vget_v_u8mf2x3_u8mf2(triplet, 1),
                   __riscv_vget_v_u8mf2x3_u8mf2(triplet, 2),
                   vl);
    i += vl;
  }
}
#endif

inline void
convertDepthToCloudCandidate(const std::uint16_t* depth,
                             const unsigned width,
                             const unsigned height,
                             const DepthCameraParameters& params,
                             PointXYZRGB* cloud,
                             bool* is_dense)
{
#if defined(__RVV10__)
  convertDepthToCloudRVV(depth, width, height, params, cloud, is_dense);
#else
  convertDepthToCloudScalar(depth, width, height, params, cloud, is_dense);
#endif
}

inline void
convertPlanarYuv422ToRgbCandidate(const std::uint8_t* yuv,
                                  const unsigned width,
                                  const unsigned height,
                                  PointXYZRGB* cloud)
{
#if defined(__RVV10__)
  convertPlanarYuv422ToRgbRVV(yuv, width, height, cloud);
#else
  convertPlanarYuv422ToRgbScalar(yuv, width, height, cloud);
#endif
}

inline void
copyRgbBufferToCloudCandidate(const std::uint8_t* rgb,
                              const unsigned width,
                              const unsigned height,
                              PointXYZRGB* cloud)
{
#if defined(__RVV10__)
  copyRgbBufferToCloudRVV(rgb, width, height, cloud);
#else
  copyRgbBufferToCloudScalar(rgb, width, height, cloud);
#endif
}

} // namespace pcl::io::rvv_lzf_image_io_support
