/*
 * 仅测试使用的 RangeImageSpherical sincos 接入原型。
 *
 * 阅读提示：
 *   - 这是 test-rvv 专用的测试专用头文件覆盖，只给
 *     run_sincos_range_image_spherical_integration_test 使用。
 *   - 它复刻生产版 RangeImageSpherical::calculate3DPoint 的接入位置，
 *     但不会修改 common/include 下的真实生产头文件。
 *   - RVV 执行链路使用 pcl::sincos_finite_domain_RVV_f32m2，
 *     合同来自 RangeImageSpherical::getAnglesFromImagePoint：
 *     angle_x in [-pi, pi]，angle_y in [-pi/2, pi/2]。
 *   - vl != 2 的异常路径会输出 NaN，让验收条件失败；这样测试不会悄悄
 *     退回标量路径。
 */

#pragma once

#define PCL_RVV_SINCOS_RANGE_IMAGE_SPHERICAL_TEST_ONLY_PROTOTYPE_ACTIVE 1

#include <pcl/common/eigen.h>
#include <pcl/range_image/range_image_spherical.h>
#include <pcl/pcl_macros.h>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <cstddef>
#include <limits>
#include <riscv_vector.h>
#endif

namespace pcl
{

/////////////////////////////////////////////////////////////////////////
void
RangeImageSpherical::calculate3DPoint (float image_x, float image_y, float range, Eigen::Vector3f& point) const
{
  float angle_x, angle_y;
  getAnglesFromImagePoint (image_x, image_y, angle_x, angle_y);

#if defined(__RVV10__)
  float angles[2] = {angle_x, angle_y};
  float sins[2] = {0.0f, 0.0f};
  float coss[2] = {1.0f, 1.0f};
  const std::size_t vl = __riscv_vsetvl_e32m2 (2);
  if (vl != 2) {
    const float qnan = std::numeric_limits<float>::quiet_NaN ();
    point = Eigen::Vector3f (qnan, qnan, qnan);
    point = to_world_system_ * point;
    return;
  }

  const vfloat32m2_t v_angles = __riscv_vle32_v_f32m2 (angles, vl);
  vfloat32m2_t v_s;
  vfloat32m2_t v_c;
  pcl::sincos_finite_domain_RVV_f32m2 (v_angles, v_s, v_c, vl);
  __riscv_vse32_v_f32m2 (sins, v_s, vl);
  __riscv_vse32_v_f32m2 (coss, v_c, vl);
  point = Eigen::Vector3f (range * sins[0] * coss[1], range * sins[1], range * coss[0] * coss[1]);
#else
  float cosY = std::cos (angle_y);
  point = Eigen::Vector3f (range * sinf (angle_x) * cosY, range * sinf (angle_y), range * std::cos (angle_x)*cosY);
#endif
  point = to_world_system_ * point;
}

/////////////////////////////////////////////////////////////////////////
inline void
RangeImageSpherical::getImagePoint (const Eigen::Vector3f& point, float& image_x, float& image_y, float& range) const
{
  Eigen::Vector3f transformedPoint = to_range_image_system_ * point;
  range = transformedPoint.norm ();
  float angle_x = atan2LookUp (transformedPoint[0], transformedPoint[2]),
        angle_y = asinLookUp (transformedPoint[1]/range);
  getImagePointFromAngles (angle_x, angle_y, image_x, image_y);
}

/////////////////////////////////////////////////////////////////////////
void
RangeImageSpherical::getAnglesFromImagePoint (float image_x, float image_y, float& angle_x, float& angle_y) const
{
  angle_y = (image_y+static_cast<float> (image_offset_y_))*angular_resolution_y_ - 0.5f*static_cast<float> (M_PI);
  angle_x = ((image_x+ static_cast<float> (image_offset_x_))*angular_resolution_x_ - static_cast<float> (M_PI));
}

/////////////////////////////////////////////////////////////////////////
void
RangeImageSpherical::getImagePointFromAngles (float angle_x, float angle_y, float& image_x, float& image_y) const
{
  image_x = (angle_x + static_cast<float> (M_PI))*angular_resolution_x_reciprocal_ - static_cast<float> (image_offset_x_);
  image_y = (angle_y + 0.5f*static_cast<float> (M_PI))*angular_resolution_y_reciprocal_ - static_cast<float> (image_offset_y_);
}
}  // namespace pcl
