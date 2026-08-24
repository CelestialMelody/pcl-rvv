#pragma once

/*
 * 本文件做什么：
 * 这里放 ONIGrabber frame-to-cloud（帧到点云）topic 的测试专用
 * reference（参考链路）和 RVV candidate（候选链路）。它复刻
 * `io/src/oni_grabber.cpp` 中 `convertToXYZPointCloud` 的 depth-only
 * PointXYZ 语义，供 gtest 和 bench（性能测试）对拍。
 *
 * 证据边界：
 * 本文件不属于 production（生产源码），也不改变 ONI replay public entry
 *（公开入口）。它只能证明 production-shaped diagnostic（生产形态诊断）
 * 层面的 correctness（正确性）和板卡候选收益，不能证明真实 ONI 文件读取、
 * replay reader 调度或 production dispatch（生产分流）。
 */

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pcl/point_types.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_oni_grabber_support {

struct CameraModel {
  float constant;
  int center_x;
  int center_y;
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
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  std::size_t index = 0;

  for (int v = -camera.center_y; v < camera.center_y; ++v) {
    for (int u = -camera.center_x; u < camera.center_x; ++u, ++index) {
      auto& point = cloud[index];
      const std::uint16_t pixel = depth[index];
      if (isInvalidDepthPixel(pixel, camera)) {
        point.x = point.y = point.z = bad_point;
        continue;
      }
      point.z = static_cast<float>(pixel) * 0.001f;
      point.x = static_cast<float>(u) * point.z * camera.constant;
      point.y = static_cast<float>(v) * point.z * camera.constant;
    }
  }
}

void
fillXYZCloudCandidate(const std::uint16_t* depth,
                      unsigned width,
                      unsigned height,
                      const CameraModel& camera,
                      pcl::PointXYZ* cloud);

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

inline void
fillXYZCloudRVV(const std::uint16_t* depth,
                const unsigned width,
                const unsigned height,
                const CameraModel& camera,
                pcl::PointXYZ* cloud)
{
  const float bad_point = std::numeric_limits<float>::quiet_NaN();
  const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ));
  auto* base = reinterpret_cast<unsigned char*>(cloud);
  auto* x_base = reinterpret_cast<float*>(base + offsetof(pcl::PointXYZ, x));
  auto* y_base = reinterpret_cast<float*>(base + offsetof(pcl::PointXYZ, y));
  auto* z_base = reinterpret_cast<float*>(base + offsetof(pcl::PointXYZ, z));

  for (unsigned row = 0; row < height; ++row) {
    const std::size_t row_offset = static_cast<std::size_t>(row) * width;
    const float vf = static_cast<float>(static_cast<int>(row) - camera.center_y);
    for (unsigned col = 0; col < width;) {
      const std::size_t vl = __riscv_vsetvl_e16m2(width - col);
      const vuint16m2_t pixels = __riscv_vle16_v_u16m2(depth + row_offset + col, vl);
      const vbool8_t invalid = invalidMask(pixels, camera, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8(invalid, vl);
      const vfloat32m4_t z = pixelsToMeters(pixels, vl);

      vuint32m4_t lane_u = __riscv_vid_v_u32m4(vl);
      lane_u = __riscv_vadd_vx_u32m4(lane_u, col, vl);
      const vfloat32m4_t uf = __riscv_vfcvt_f_xu_v_f32m4(lane_u, vl);
      const vfloat32m4_t x =
          __riscv_vfmul_vf_f32m4(
              __riscv_vfmul_vv_f32m4(
                  __riscv_vfsub_vf_f32m4(uf, static_cast<float>(camera.center_x), vl),
                  z,
                  vl),
              camera.constant,
              vl);
      const vfloat32m4_t y =
          __riscv_vfmul_vf_f32m4(
              __riscv_vfmul_vv_f32m4(__riscv_vfmv_v_f_f32m4(vf, vl), z, vl),
              camera.constant,
              vl);
      const vfloat32m4_t bad = __riscv_vfmv_v_f_f32m4(bad_point, vl);
      const std::size_t offset = row_offset + col;

      __riscv_vsse32_v_f32m4(x_base + offset * (sizeof(pcl::PointXYZ) / sizeof(float)),
                             point_stride,
                             bad,
                             vl);
      __riscv_vsse32_v_f32m4(y_base + offset * (sizeof(pcl::PointXYZ) / sizeof(float)),
                             point_stride,
                             bad,
                             vl);
      __riscv_vsse32_v_f32m4(z_base + offset * (sizeof(pcl::PointXYZ) / sizeof(float)),
                             point_stride,
                             bad,
                             vl);
      __riscv_vsse32_v_f32m4_m(valid,
                               x_base + offset * (sizeof(pcl::PointXYZ) / sizeof(float)),
                               point_stride,
                               x,
                               vl);
      __riscv_vsse32_v_f32m4_m(valid,
                               y_base + offset * (sizeof(pcl::PointXYZ) / sizeof(float)),
                               point_stride,
                               y,
                               vl);
      __riscv_vsse32_v_f32m4_m(valid,
                               z_base + offset * (sizeof(pcl::PointXYZ) / sizeof(float)),
                               point_stride,
                               z,
                               vl);
      col += static_cast<unsigned>(vl);
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

} // namespace pcl::io::rvv_oni_grabber_support
