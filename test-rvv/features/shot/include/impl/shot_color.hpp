/*
 * SHOT color LAB distance component diagnostic helper.
 *
 * 本文件只服务 Phase 060 的 test-only component ablation（测试专用组件消融）。
 * 它复刻 `SHOTColorEstimation::computePointSHOT` 中 RGB2CIELAB 之后的归一化
 * LAB（颜色空间）距离和 color bin distance 算术。这里输入已经是连续 L/a/b
 * 数组，不覆盖 production 的 RGB LUT（查找表）离散加载、PointXYZRGBA 字段
 * 访问或 `std::vector::push_back` 成本。
 */

#pragma once

#include <pcl/common/colors.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_shot {

inline void
rgbToNormalizedLab(const std::uint8_t red,
                   const std::uint8_t green,
                   const std::uint8_t blue,
                   float& l,
                   float& a,
                   float& b)
{
  static const auto sRGB_LUT = pcl::RGB2sRGB_LUT<float, 8>();
  static const auto& sXYZ_LUT = pcl::XYZ2LAB_LUT<float, 4000>();

  const float fr = sRGB_LUT[red];
  const float fg = sRGB_LUT[green];
  const float fb = sRGB_LUT[blue];

  const float x = fr * 0.412453f + fg * 0.357580f + fb * 0.180423f;
  const float y = fr * 0.212671f + fg * 0.715160f + fb * 0.072169f;
  const float z = fr * 0.019334f + fg * 0.119193f + fb * 0.950227f;

  float vx = x / 0.95047f;
  float vy = y;
  float vz = z / 1.08883f;

  vx = sXYZ_LUT[static_cast<int>(vx * 4000)];
  vy = sXYZ_LUT[static_cast<int>(vy * 4000)];
  vz = sXYZ_LUT[static_cast<int>(vz * 4000)];

  l = 116.0f * vy - 16.0f;
  if (l > 100.0f)
    l = 100.0f;

  a = 500.0f * (vx - vy);
  if (a > 120.0f)
    a = 120.0f;
  else if (a < -120.0f)
    a = -120.0f;

  b = 200.0f * (vy - vz);
  if (b > 120.0f)
    b = 120.0f;
  else if (b < -120.0f)
    b = -120.0f;

  l /= 100.0f;
  a /= 120.0f;
  b /= 120.0f;
}

// 标量参考链路使用 float 中间值模拟 production 中归一化 LAB 分量的算术形态，
// 最后再写成 double bin distance，供 RVV same-chain（同构链路）对拍。
inline void
computeColorBinDistanceScalar(const float* l,
                              const float* a,
                              const float* b,
                              const std::size_t count,
                              const float l_ref,
                              const float a_ref,
                              const float b_ref,
                              const int nr_color_bins,
                              double* out)
{
  for (std::size_t i = 0; i < count; ++i) {
    const float color_distance =
        (std::fabs(l_ref - l[i]) + ((std::fabs(a_ref - a[i]) + std::fabs(b_ref - b[i])) * 0.5f)) *
        (1.0f / 3.0f);
    float clamped = color_distance;
    if (clamped > 1.0f)
      clamped = 1.0f;
    if (clamped < 0.0f)
      clamped = 0.0f;
    out[i] = static_cast<double>(clamped) * static_cast<double>(nr_color_bins);
  }
}

// Indexed RGB/LUT 参考链路复刻 production color loop 的前半段：按 indices
// 读取 `PointXYZRGBA` 的 RGB，转换到归一化 LAB，再计算 color bin distance。
inline void
computeColorBinDistanceIndexedRGBScalar(const pcl::PointCloud<pcl::PointXYZRGBA>& surface,
                                        const pcl::Indices& indices,
                                        const int reference_index,
                                        const int nr_color_bins,
                                        double* out)
{
  float l_ref = 0.0f, a_ref = 0.0f, b_ref = 0.0f;
  const pcl::PointXYZRGBA& reference = surface[static_cast<std::size_t>(reference_index)];
  rgbToNormalizedLab(reference.r, reference.g, reference.b, l_ref, a_ref, b_ref);

  for (std::size_t i = 0; i < indices.size(); ++i) {
    float l = 0.0f, a = 0.0f, b = 0.0f;
    const pcl::PointXYZRGBA& point = surface[static_cast<std::size_t>(indices[i])];
    rgbToNormalizedLab(point.r, point.g, point.b, l, a, b);
    computeColorBinDistanceScalar(&l, &a, &b, 1, l_ref, a_ref, b_ref, nr_color_bins, out + i);
  }
}

#if defined(__RVV10__)
inline void
computeColorBinDistanceRvvKernel(const float* l,
                                 const float* a,
                                 const float* b,
                                 const std::size_t count,
                                 const float l_ref,
                                 const float a_ref,
                                 const float b_ref,
                                 const int nr_color_bins,
                                 double* out)
{
  const float one_third = 1.0f / 3.0f;
  const double bins = static_cast<double>(nr_color_bins);
  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const vfloat32m2_t vl_values = __riscv_vle32_v_f32m2(l + i, vl);
    const vfloat32m2_t va_values = __riscv_vle32_v_f32m2(a + i, vl);
    const vfloat32m2_t vb_values = __riscv_vle32_v_f32m2(b + i, vl);

    const vfloat32m2_t dl = __riscv_vfabs_v_f32m2(__riscv_vfsub_vf_f32m2(vl_values, l_ref, vl), vl);
    const vfloat32m2_t da = __riscv_vfabs_v_f32m2(__riscv_vfsub_vf_f32m2(va_values, a_ref, vl), vl);
    const vfloat32m2_t db = __riscv_vfabs_v_f32m2(__riscv_vfsub_vf_f32m2(vb_values, b_ref, vl), vl);

    vfloat32m2_t distance = __riscv_vfmul_vf_f32m2(__riscv_vfadd_vv_f32m2(da, db, vl), 0.5f, vl);
    distance = __riscv_vfmul_vf_f32m2(__riscv_vfadd_vv_f32m2(dl, distance, vl), one_third, vl);
    distance = __riscv_vfmin_vf_f32m2(__riscv_vfmax_vf_f32m2(distance, 0.0f, vl), 1.0f, vl);
    const vfloat64m4_t bin_values =
        __riscv_vfmul_vf_f64m4(__riscv_vfwcvt_f_f_v_f64m4(distance, vl), bins, vl);
    __riscv_vse64_v_f64m4(out + i, bin_values, vl);
    i += vl;
  }
}
#endif

inline void
computeColorBinDistanceRVV(const float* l,
                           const float* a,
                           const float* b,
                           const std::size_t count,
                           const float l_ref,
                           const float a_ref,
                           const float b_ref,
                           const int nr_color_bins,
                           double* out)
{
#if defined(__RVV10__)
  computeColorBinDistanceRvvKernel(l, a, b, count, l_ref, a_ref, b_ref, nr_color_bins, out);
#else
  computeColorBinDistanceScalar(l, a, b, count, l_ref, a_ref, b_ref, nr_color_bins, out);
#endif
}

inline void
computeColorBinDistanceIndexedRGBRVV(const pcl::PointCloud<pcl::PointXYZRGBA>& surface,
                                     const pcl::Indices& indices,
                                     const int reference_index,
                                     const int nr_color_bins,
                                     double* out)
{
  float l_ref = 0.0f, a_ref = 0.0f, b_ref = 0.0f;
  const pcl::PointXYZRGBA& reference = surface[static_cast<std::size_t>(reference_index)];
  rgbToNormalizedLab(reference.r, reference.g, reference.b, l_ref, a_ref, b_ref);

  std::vector<float> l_values(indices.size());
  std::vector<float> a_values(indices.size());
  std::vector<float> b_values(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const pcl::PointXYZRGBA& point = surface[static_cast<std::size_t>(indices[i])];
    rgbToNormalizedLab(point.r, point.g, point.b, l_values[i], a_values[i], b_values[i]);
  }

  computeColorBinDistanceRVV(
      l_values.data(), a_values.data(), b_values.data(), indices.size(), l_ref, a_ref, b_ref, nr_color_bins, out);
}

} // namespace pcl_rvv_shot
