/*
 * 本文件做什么：
 * 这里承载 point_cloud_image_extractors 首阶段 production-shaped diagnostic
 * （生产形态诊断）的输入构造、标量参考和 RVV candidate（RVV 候选）。它复刻
 * `impl/point_cloud_image_extractors.hpp` 中 RGB/RGBA unpack、scaling 写
 * mono16、label mono16、normal field 和 paint-NaNs post-pass 的热点语义。
 *
 * 证据边界：
 * 这些函数不修改真实 production header，也不证明公开入口会自动走 RVV。它们
 * 只回答“同一输入和同一字段布局下，候选数据流能否与标量语义一致并值得上板卡
 * 评估”。
 */

#pragma once

#include <pcl/common/io.h>
#include <pcl/common/point_tests.h>
#include <pcl/io/point_cloud_image_extractors.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::io::rvv_test::point_cloud_image_extractors {

enum class ScalingMode {
  NoScaling = 0,
  FixedFactor = 1,
  FullRange = 2,
};

struct ProductionProbeGate {
  bool accepted;
  const char* reason;
};

template <typename PointT>
ProductionProbeGate
rgbProductionProbeGate()
{
  if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB> ||
                std::is_same_v<PointT, pcl::PointXYZRGBA>) {
    return {true, "Phase 040 accepts exact RGB/RGBA point types"};
  }
  return {false, "Phase 040 keeps non exact RGB/RGBA point types on scalar fallback"};
}

template <typename PointT>
ProductionProbeGate
scalingProductionProbeGate(const std::string& field_name, const ScalingMode mode)
{
  if constexpr (std::is_same_v<PointT, pcl::PointXYZI>) {
    if (field_name == "intensity" && mode == ScalingMode::FullRange)
      return {true, "Phase 040 accepts exact PointXYZI intensity full-range scaling"};
    return {false, "Phase 040 only accepts intensity full-range scaling"};
  }
  return {false, "Phase 040 keeps non exact PointXYZI scaling paths on scalar fallback"};
}

template <typename PointT>
pcl::PointCloud<PointT>
makeRgbCloud(const std::uint32_t width, const std::uint32_t height)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = width;
  cloud.height = height;
  cloud.is_dense = true;
  cloud.resize(static_cast<std::size_t>(width) * height);

  for (std::size_t i = 0; i < cloud.size(); ++i) {
    auto& point = cloud[i];
    point.x = static_cast<float>(i % width);
    point.y = static_cast<float>(i / width);
    point.z = 1.0f + static_cast<float>(i % 17) * 0.01f;
    point.r = static_cast<std::uint8_t>((i * 13 + 7) & 0xff);
    point.g = static_cast<std::uint8_t>((i * 29 + 11) & 0xff);
    point.b = static_cast<std::uint8_t>((i * 47 + 19) & 0xff);
    if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
      point.a = static_cast<std::uint8_t>((i * 5 + 251) & 0xff);
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZI>
makeScalingCloud(const std::uint32_t width, const std::uint32_t height)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = width;
  cloud.height = height;
  cloud.is_dense = true;
  cloud.resize(static_cast<std::size_t>(width) * height);

  for (std::size_t i = 0; i < cloud.size(); ++i) {
    auto& point = cloud[i];
    point.x = static_cast<float>(i % width);
    point.y = static_cast<float>(i / width);
    point.z = 1.0f;
    point.intensity = 0.25f + static_cast<float>((i * 37) % 47) * 0.03125f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointNormal>
makeNormalCloud(const std::uint32_t width, const std::uint32_t height)
{
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.width = width;
  cloud.height = height;
  cloud.is_dense = true;
  cloud.resize(static_cast<std::size_t>(width) * height);

  for (std::size_t i = 0; i < cloud.size(); ++i) {
    auto& point = cloud[i];
    point.x = static_cast<float>(i % width);
    point.y = static_cast<float>(i / width);
    point.z = 1.0f;
    point.normal_x = -0.875f + static_cast<float>((i * 17) % 29) * 0.0625f;
    point.normal_y = -0.75f + static_cast<float>((i * 23) % 31) * 0.05f;
    point.normal_z = -0.5f + static_cast<float>((i * 41) % 37) * 0.03125f;
    point.curvature = 0.0f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZL>
makeLabelCloud(const std::uint32_t width, const std::uint32_t height)
{
  pcl::PointCloud<pcl::PointXYZL> cloud;
  cloud.width = width;
  cloud.height = height;
  cloud.is_dense = true;
  cloud.resize(static_cast<std::size_t>(width) * height);

  for (std::size_t i = 0; i < cloud.size(); ++i) {
    auto& point = cloud[i];
    point.x = static_cast<float>(i % width);
    point.y = static_cast<float>(i / width);
    point.z = 1.0f;
    // 覆盖 16 位边界上下的 label，确保 candidate 复刻 production 的截断语义。
    point.label = static_cast<std::uint32_t>((i * 65537u + 19u) ^ (i >> 3));
  }
  return cloud;
}

template <typename PointT>
bool
extractRgbScalar(const pcl::PointCloud<PointT>& cloud, std::vector<std::uint8_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  pcl::PCLImage image;
  pcl::io::PointCloudImageExtractorFromRGBField<PointT> extractor;
  if (!extractor.extract(cloud, image))
    return false;
  output = image.data;
  return true;
}

inline bool
extractNormalScalar(const pcl::PointCloud<pcl::PointNormal>& cloud,
                    std::vector<std::uint8_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  pcl::PCLImage image;
  pcl::io::PointCloudImageExtractorFromNormalField<pcl::PointNormal> extractor;
  if (!extractor.extract(cloud, image))
    return false;
  output = image.data;
  return true;
}

inline bool
extractLabelMono16Scalar(const pcl::PointCloud<pcl::PointXYZL>& cloud,
                         std::vector<std::uint16_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  pcl::PCLImage image;
  pcl::io::PointCloudImageExtractorFromLabelField<pcl::PointXYZL> extractor;
  extractor.setColorMode(extractor.COLORS_MONO);
  if (!extractor.extract(cloud, image))
    return false;
  output.resize(image.data.size() / sizeof(std::uint16_t));
  std::memcpy(output.data(), image.data.data(), image.data.size());
  return true;
}

inline bool
extractScalingScalar(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                     const ScalingMode mode,
                     const float fixed_factor,
                     std::vector<std::uint16_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  output.assign(cloud.size(), 0);
  float scaling_factor = fixed_factor;
  float data_min = 0.0f;
  if (mode == ScalingMode::FullRange) {
    float min_value = std::numeric_limits<float>::infinity();
    float max_value = -std::numeric_limits<float>::infinity();
    for (const auto& point : cloud) {
      const float value = point.intensity;
      if (value < min_value)
        min_value = value;
      if (value > max_value)
        max_value = value;
    }
    scaling_factor = min_value == max_value
                         ? 0.0f
                         : static_cast<float>(std::numeric_limits<std::uint16_t>::max()) /
                               (max_value - min_value);
    data_min = min_value;
  }

  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const float value = cloud[i].intensity;
    if (mode == ScalingMode::NoScaling)
      output[i] = static_cast<std::uint16_t>(value);
    else if (mode == ScalingMode::FullRange)
      output[i] = static_cast<std::uint16_t>((value - data_min) * scaling_factor);
    else
      output[i] = static_cast<std::uint16_t>(value * scaling_factor);
  }
  return true;
}

inline void
paintNaNsWithBlackScalar(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                         std::vector<std::uint16_t>& image)
{
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    if (!pcl::isFinite(cloud[i]))
      image[i] = 0;
  }
}

#if defined(__RVV10__) && defined(__riscv_vector)
inline vuint8mf2_t
narrowU32ToU8(const vuint32m2_t value, const std::size_t vl)
{
  const vuint16m1_t value_u16 = __riscv_vncvt_x_x_w_u16m1(value, vl);
  return __riscv_vncvt_x_x_w_u8mf2(value_u16, vl);
}

template <typename PointT>
bool
extractRgbRvv(const pcl::PointCloud<PointT>& cloud, std::vector<std::uint8_t>& output)
{
  std::vector<pcl::PCLPointField> fields;
  int field_idx = pcl::getFieldIndex<PointT>("rgb", fields);
  if (field_idx == -1)
    field_idx = pcl::getFieldIndex<PointT>("rgba", fields);
  if (field_idx == -1)
    return false;

  output.assign(cloud.size() * 3, 0);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* rgb_ptr =
      reinterpret_cast<const std::uint32_t*>(base + fields[field_idx].offset);
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(PointT));

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  std::vector<std::uint32_t> r(vlmax, 0);
  std::vector<std::uint32_t> g(vlmax, 0);
  std::vector<std::uint32_t> b(vlmax, 0);

  std::size_t i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const auto values = __riscv_vlse32_v_u32m2(rgb_ptr + i * (stride / sizeof(std::uint32_t)),
                                              stride,
                                              vl);
    __riscv_vse32_v_u32m2(r.data(), __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(values, 16, vl), 0xff, vl), vl);
    __riscv_vse32_v_u32m2(g.data(), __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(values, 8, vl), 0xff, vl), vl);
    __riscv_vse32_v_u32m2(b.data(), __riscv_vand_vx_u32m2(values, 0xff, vl), vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t out = (i + lane) * 3;
      output[out + 0] = static_cast<std::uint8_t>(r[lane]);
      output[out + 1] = static_cast<std::uint8_t>(g[lane]);
      output[out + 2] = static_cast<std::uint8_t>(b[lane]);
    }
    i += vl;
  }
  return true;
}

template <typename PointT>
bool
extractRgbSegmentStoreRvv(const pcl::PointCloud<PointT>& cloud, std::vector<std::uint8_t>& output)
{
  std::vector<pcl::PCLPointField> fields;
  int field_idx = pcl::getFieldIndex<PointT>("rgb", fields);
  if (field_idx == -1)
    field_idx = pcl::getFieldIndex<PointT>("rgba", fields);
  if (field_idx == -1)
    return false;

  output.assign(cloud.size() * 3, 0);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* rgb_ptr =
      reinterpret_cast<const std::uint32_t*>(base + fields[field_idx].offset);
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(PointT));

  std::size_t i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e8mf2(cloud.size() - i);
    const auto values = __riscv_vlse32_v_u32m2(rgb_ptr + i * (stride / sizeof(std::uint32_t)),
                                              stride,
                                              vl);
    const auto r = narrowU32ToU8(
        __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(values, 16, vl), 0xff, vl), vl);
    const auto g = narrowU32ToU8(
        __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(values, 8, vl), 0xff, vl), vl);
    const auto b = narrowU32ToU8(__riscv_vand_vx_u32m2(values, 0xff, vl), vl);
    const vuint8mf2x3_t rgb = __riscv_vcreate_v_u8mf2x3(r, g, b);
    __riscv_vsseg3e8_v_u8mf2x3(output.data() + i * 3, rgb, vl);
    i += vl;
  }
  return true;
}

inline bool
extractScalingRvv(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                  const ScalingMode mode,
                  const float fixed_factor,
                  std::vector<std::uint16_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  output.assign(cloud.size(), 0);
  float scaling_factor = fixed_factor;
  float data_min = 0.0f;
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* intensity_ptr =
      reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZI, intensity));
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZI));
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  std::vector<float> values(vlmax, 0.0f);

  if (mode == ScalingMode::FullRange) {
    float min_value = std::numeric_limits<float>::infinity();
    float max_value = -std::numeric_limits<float>::infinity();
    std::size_t i = 0;
    while (i < cloud.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
      const auto v = __riscv_vlse32_v_f32m2(intensity_ptr + i * (stride / sizeof(float)), stride, vl);
      __riscv_vse32_v_f32m2(values.data(), v, vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        if (values[lane] < min_value)
          min_value = values[lane];
        if (values[lane] > max_value)
          max_value = values[lane];
      }
      i += vl;
    }
    scaling_factor = min_value == max_value
                         ? 0.0f
                         : static_cast<float>(std::numeric_limits<std::uint16_t>::max()) /
                               (max_value - min_value);
    data_min = min_value;
  }

  std::size_t i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    auto v = __riscv_vlse32_v_f32m2(intensity_ptr + i * (stride / sizeof(float)), stride, vl);
    if (mode == ScalingMode::FullRange)
      v = __riscv_vfmul_vf_f32m2(__riscv_vfsub_vf_f32m2(v, data_min, vl), scaling_factor, vl);
    else if (mode == ScalingMode::FixedFactor)
      v = __riscv_vfmul_vf_f32m2(v, scaling_factor, vl);
    __riscv_vse32_v_f32m2(values.data(), v, vl);
    for (std::size_t lane = 0; lane < vl; ++lane)
      output[i + lane] = static_cast<std::uint16_t>(values[lane]);
    i += vl;
  }
  return true;
}

inline bool
extractLabelMono16Rvv(const pcl::PointCloud<pcl::PointXYZL>& cloud,
                      std::vector<std::uint16_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  std::vector<pcl::PCLPointField> fields;
  const int field_idx = pcl::getFieldIndex<pcl::PointXYZL>("label", fields);
  if (field_idx == -1)
    return false;

  output.assign(cloud.size(), 0);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* label_ptr =
      reinterpret_cast<const std::uint32_t*>(base + fields[field_idx].offset);
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZL));

  std::size_t i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const auto labels =
        __riscv_vlse32_v_u32m2(label_ptr + i * (stride / sizeof(std::uint32_t)), stride, vl);
    const auto labels_u16 = __riscv_vncvt_x_x_w_u16m1(labels, vl);
    __riscv_vse16_v_u16m1(output.data() + i, labels_u16, vl);
    i += vl;
  }
  return true;
}
#endif

inline bool
extractLabelMono16Candidate(const pcl::PointCloud<pcl::PointXYZL>& cloud,
                            std::vector<std::uint16_t>& output)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  return extractLabelMono16Rvv(cloud, output);
#else
  return extractLabelMono16Scalar(cloud, output);
#endif
}

#if defined(__RVV10__) && defined(__riscv_vector)
inline bool
extractScalingFullRangeReductionRvv(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                                    std::vector<std::uint16_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  output.assign(cloud.size(), 0);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* intensity_ptr =
      reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZI, intensity));
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZI));

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const float min_seed = std::numeric_limits<float>::infinity();
  const float max_seed = -std::numeric_limits<float>::infinity();
  auto v_min = __riscv_vfmv_v_f_f32m2(min_seed, vlmax);
  auto v_max = __riscv_vfmv_v_f_f32m2(max_seed, vlmax);

  std::size_t i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const auto v = __riscv_vlse32_v_f32m2(intensity_ptr + i * (stride / sizeof(float)), stride, vl);
    v_min = __riscv_vfmin_vv_f32m2_tu(v_min, v_min, v, vl);
    v_max = __riscv_vfmax_vv_f32m2_tu(v_max, v_max, v, vl);
    i += vl;
  }

  const auto red_min_seed = __riscv_vfmv_s_f_f32m1(min_seed, 1);
  const auto red_max_seed = __riscv_vfmv_s_f_f32m1(max_seed, 1);
  const float min_value =
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1(v_min, red_min_seed, vlmax));
  const float max_value =
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1(v_max, red_max_seed, vlmax));
  const float scaling_factor = min_value == max_value
                                   ? 0.0f
                                   : static_cast<float>(std::numeric_limits<std::uint16_t>::max()) /
                                         (max_value - min_value);

  std::vector<float> values(vlmax, 0.0f);
  i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    auto v = __riscv_vlse32_v_f32m2(intensity_ptr + i * (stride / sizeof(float)), stride, vl);
    v = __riscv_vfmul_vf_f32m2(__riscv_vfsub_vf_f32m2(v, min_value, vl), scaling_factor, vl);
    __riscv_vse32_v_f32m2(values.data(), v, vl);
    for (std::size_t lane = 0; lane < vl; ++lane)
      output[i + lane] = static_cast<std::uint16_t>(values[lane]);
    i += vl;
  }
  return true;
}

inline bool
extractNormalRvv(const pcl::PointCloud<pcl::PointNormal>& cloud,
                 std::vector<std::uint8_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;

  output.assign(cloud.size() * 3, 0);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* normal_x_ptr =
      reinterpret_cast<const float*>(base + offsetof(pcl::PointNormal, normal_x));
  const auto* normal_y_ptr =
      reinterpret_cast<const float*>(base + offsetof(pcl::PointNormal, normal_y));
  const auto* normal_z_ptr =
      reinterpret_cast<const float*>(base + offsetof(pcl::PointNormal, normal_z));
  constexpr std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointNormal));

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  std::vector<float> x_values(vlmax, 0.0f);
  std::vector<float> y_values(vlmax, 0.0f);
  std::vector<float> z_values(vlmax, 0.0f);

  std::size_t i = 0;
  while (i < cloud.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    auto x = __riscv_vlse32_v_f32m2(normal_x_ptr + i * (stride / sizeof(float)), stride, vl);
    auto y = __riscv_vlse32_v_f32m2(normal_y_ptr + i * (stride / sizeof(float)), stride, vl);
    auto z = __riscv_vlse32_v_f32m2(normal_z_ptr + i * (stride / sizeof(float)), stride, vl);
    x = __riscv_vfmul_vf_f32m2(__riscv_vfadd_vf_f32m2(x, 1.0f, vl), 127.0f, vl);
    y = __riscv_vfmul_vf_f32m2(__riscv_vfadd_vf_f32m2(y, 1.0f, vl), 127.0f, vl);
    z = __riscv_vfmul_vf_f32m2(__riscv_vfadd_vf_f32m2(z, 1.0f, vl), 127.0f, vl);
    __riscv_vse32_v_f32m2(x_values.data(), x, vl);
    __riscv_vse32_v_f32m2(y_values.data(), y, vl);
    __riscv_vse32_v_f32m2(z_values.data(), z, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t out = (i + lane) * 3;
      output[out + 0] = static_cast<std::uint8_t>(x_values[lane]);
      output[out + 1] = static_cast<std::uint8_t>(y_values[lane]);
      output[out + 2] = static_cast<std::uint8_t>(z_values[lane]);
    }
    i += vl;
  }
  return true;
}
#endif

template <typename PointT>
bool
extractRgbCandidate(const pcl::PointCloud<PointT>& cloud, std::vector<std::uint8_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;
#if defined(__RVV10__) && defined(__riscv_vector)
  return extractRgbRvv(cloud, output);
#else
  return extractRgbScalar(cloud, output);
#endif
}

template <typename PointT>
bool
extractRgbSegmentStoreCandidate(const pcl::PointCloud<PointT>& cloud,
                                std::vector<std::uint8_t>& output)
{
  if (!cloud.isOrganized() || cloud.size() != cloud.width * cloud.height)
    return false;
#if defined(__RVV10__) && defined(__riscv_vector)
  return extractRgbSegmentStoreRvv(cloud, output);
#else
  return extractRgbScalar(cloud, output);
#endif
}

inline bool
extractScalingCandidate(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                        const ScalingMode mode,
                        const float fixed_factor,
                        std::vector<std::uint16_t>& output)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  return extractScalingRvv(cloud, mode, fixed_factor, output);
#else
  return extractScalingScalar(cloud, mode, fixed_factor, output);
#endif
}

inline bool
extractScalingFullRangeReductionCandidate(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                                          std::vector<std::uint16_t>& output)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  return extractScalingFullRangeReductionRvv(cloud, output);
#else
  return extractScalingScalar(cloud, ScalingMode::FullRange, 1.0f, output);
#endif
}

inline bool
extractNormalCandidate(const pcl::PointCloud<pcl::PointNormal>& cloud,
                       std::vector<std::uint8_t>& output)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  return extractNormalRvv(cloud, output);
#else
  return extractNormalScalar(cloud, output);
#endif
}

inline void
paintNaNsWithBlackCandidate(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                            std::vector<std::uint16_t>& image)
{
  paintNaNsWithBlackScalar(cloud, image);
}

} // namespace pcl::io::rvv_test::point_cloud_image_extractors
