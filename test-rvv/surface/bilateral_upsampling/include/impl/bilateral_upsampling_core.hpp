#pragma once

/*
 * 本文件做什么：
 * 这里复刻 BilateralUpsampling::performProcessing 的 organized image（有序图像式点云）
 * 热点循环，并提供一个 staged-window-reduction（窗口暂存规约）RVV 候选。候选把每个
 * 像素窗口内已经查表得到的 weight（权重）和 depth（深度）暂存为连续 float 数组，再用
 * RVV 做乘法与规约。它能回答“当前窗口累加形态是否值得接 production（生产源码）”，
 * 但不能证明真实 PointXYZRGB / PointXYZRGBA production dispatch（生产分流）已经成立。
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_surface_bilateral_upsampling {

struct RgbPoint {
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};
  std::uint8_t r{0};
  std::uint8_t g{0};
  std::uint8_t b{0};
  std::uint8_t a{255};
};

struct Matrix3f {
  std::array<float, 9> v{};

  float operator()(std::size_t row, std::size_t col) const { return v[row * 3 + col]; }
};

struct Tables {
  int window_size{0};
  int diameter{0};
  std::vector<float> depth;
  std::vector<float> rgb;
};

struct ErrorStats {
  float max_abs_xyz{0.0f};
  float max_abs_z{0.0f};
  float rmse_xyz{0.0f};
  std::size_t finite_points{0};
  std::size_t nan_points{0};
  bool rgb_equal{true};
};

inline Matrix3f
makeSimpleUnprojection()
{
  return Matrix3f{{0.002f, 0.0f, -0.64f, 0.0f, 0.002f, -0.48f, 0.0f, 0.0f, 1.0f}};
}

inline Tables
computeTables(int window_size, float sigma_depth, float sigma_color)
{
  Tables tables;
  tables.window_size = window_size;
  tables.diameter = 2 * window_size + 1;
  tables.depth.resize(static_cast<std::size_t>(tables.diameter * tables.diameter));
  tables.rgb.resize(3 * 255 + 1);

  for (int dx = -window_size; dx <= window_size; ++dx) {
    for (int dy = -window_size; dy <= window_size; ++dy) {
      const float arg =
          -static_cast<float>(dx * dx + dy * dy) /
          (2.0f * static_cast<float>(sigma_depth * sigma_depth));
      tables.depth[static_cast<std::size_t>((dx + window_size) * tables.diameter +
                                            (dy + window_size))] = std::exp(arg);
    }
  }

  for (int d_color = 0; d_color <= 3 * 255; ++d_color) {
    const float arg =
        -static_cast<float>(d_color * d_color) / (2.0f * sigma_color * sigma_color);
    tables.rgb[static_cast<std::size_t>(d_color)] = std::exp(arg);
  }

  return tables;
}

inline std::vector<RgbPoint>
makeCloud(int width, int height, bool with_holes)
{
  std::vector<RgbPoint> cloud(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto& p = cloud[static_cast<std::size_t>(y * width + x)];
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      p.z = 0.85f + 0.0025f * fx + 0.0015f * fy +
            0.025f * std::sin(0.071f * fx) * std::cos(0.053f * fy);
      p.r = static_cast<std::uint8_t>((x * 7 + y * 3) & 255);
      p.g = static_cast<std::uint8_t>((x * 5 + y * 11 + 17) & 255);
      p.b = static_cast<std::uint8_t>((x * 13 + y * 2 + 41) & 255);
      if (with_holes && ((x + 3 * y) % 37 == 0))
        p.z = std::numeric_limits<float>::quiet_NaN();
    }
  }
  return cloud;
}

inline void
writeProjectedPoint(RgbPoint& out, int x, int y, float depth, const Matrix3f& unprojection)
{
  const float pc0 = static_cast<float>(x) * depth;
  const float pc1 = static_cast<float>(y) * depth;
  const float pc2 = depth;
  out.x = unprojection(0, 0) * pc0 + unprojection(0, 1) * pc1 + unprojection(0, 2) * pc2;
  out.y = unprojection(1, 0) * pc0 + unprojection(1, 1) * pc1 + unprojection(1, 2) * pc2;
  out.z = unprojection(2, 0) * pc0 + unprojection(2, 1) * pc1 + unprojection(2, 2) * pc2;
}

inline int
colorDistance(const RgbPoint& a, const RgbPoint& b)
{
  return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) +
         std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) +
         std::abs(static_cast<int>(a.b) - static_cast<int>(b.b));
}

inline float
depthWeight(const Tables& tables, int dx, int dy)
{
  return tables.depth[static_cast<std::size_t>((dx + tables.window_size) * tables.diameter +
                                               (dy + tables.window_size))];
}

inline void
processScalar(const std::vector<RgbPoint>& input,
              int width,
              int height,
              const Tables& tables,
              const Matrix3f& unprojection,
              std::vector<RgbPoint>& output)
{
  output.resize(input.size());
  const float nan = std::numeric_limits<float>::quiet_NaN();

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      const int center = y * width + x;
      const int start_x = std::max(x - tables.window_size, 0);
      const int start_y = std::max(y - tables.window_size, 0);
      const int end_x = std::min(x + tables.window_size, width);
      const int end_y = std::min(y + tables.window_size, height);

      float sum = 0.0f;
      float norm_sum = 0.0f;
      for (int xw = start_x; xw < end_x; ++xw) {
        for (int yw = start_y; yw < end_y; ++yw) {
          const int id = yw * width + xw;
          if (!std::isfinite(input[static_cast<std::size_t>(id)].z))
            continue;
          const float w_depth = depthWeight(tables, x - xw, y - yw);
          const float w_rgb =
              tables.rgb[static_cast<std::size_t>(colorDistance(input[center], input[id]))];
          const float weight = w_depth * w_rgb;
          sum += weight * input[static_cast<std::size_t>(id)].z;
          norm_sum += weight;
        }
      }

      auto& out = output[static_cast<std::size_t>(center)];
      out.r = input[static_cast<std::size_t>(center)].r;
      out.g = input[static_cast<std::size_t>(center)].g;
      out.b = input[static_cast<std::size_t>(center)].b;
      out.a = input[static_cast<std::size_t>(center)].a;
      if (norm_sum != 0.0f) {
        writeProjectedPoint(out, x, y, sum / norm_sum, unprojection);
      }
      else {
        out.x = nan;
        out.y = nan;
        out.z = nan;
      }
    }
  }
}

struct Accumulation {
  float sum{0.0f};
  float norm_sum{0.0f};
};

inline Accumulation
accumulateWindowScalarTail(const float* weights, const float* depths, std::size_t n)
{
  Accumulation acc;
  for (std::size_t i = 0; i < n; ++i) {
    acc.sum += weights[i] * depths[i];
    acc.norm_sum += weights[i];
  }
  return acc;
}

#if defined(__RVV10__)
inline float
reduceSumF32M2(vfloat32m2_t values, std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, 1);
  const vfloat32m1_t sum = __riscv_vfredusum_vs_f32m2_f32m1(values, zero, vl);
  return __riscv_vfmv_f_s_f32m1_f32(sum);
}

inline Accumulation
accumulateWindowRVV(const float* weights, const float* depths, std::size_t n)
{
  Accumulation acc;
  std::size_t offset = 0;
  while (offset < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - offset);
    const vfloat32m2_t w = __riscv_vle32_v_f32m2(weights + offset, vl);
    const vfloat32m2_t z = __riscv_vle32_v_f32m2(depths + offset, vl);
    const vfloat32m2_t contrib = __riscv_vfmul_vv_f32m2(w, z, vl);
    acc.sum += reduceSumF32M2(contrib, vl);
    acc.norm_sum += reduceSumF32M2(w, vl);
    offset += vl;
  }
  return acc;
}
#endif

inline bool
processRVV(const std::vector<RgbPoint>& input,
           int width,
           int height,
           const Tables& tables,
           const Matrix3f& unprojection,
           std::vector<RgbPoint>& output)
{
#if defined(__RVV10__)
  output.resize(input.size());
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const std::size_t max_window =
      static_cast<std::size_t>(tables.diameter) * static_cast<std::size_t>(tables.diameter);
  std::vector<float> weights(max_window);
  std::vector<float> depths(max_window);

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      const int center = y * width + x;
      const int start_x = std::max(x - tables.window_size, 0);
      const int start_y = std::max(y - tables.window_size, 0);
      const int end_x = std::min(x + tables.window_size, width);
      const int end_y = std::min(y + tables.window_size, height);

      std::size_t n = 0;
      for (int xw = start_x; xw < end_x; ++xw) {
        for (int yw = start_y; yw < end_y; ++yw) {
          const int id = yw * width + xw;
          const float z = input[static_cast<std::size_t>(id)].z;
          if (!std::isfinite(z))
            continue;
          const float w_depth = depthWeight(tables, x - xw, y - yw);
          const float w_rgb =
              tables.rgb[static_cast<std::size_t>(colorDistance(input[center], input[id]))];
          weights[n] = w_depth * w_rgb;
          depths[n] = z;
          ++n;
        }
      }

      const Accumulation acc = accumulateWindowRVV(weights.data(), depths.data(), n);
      auto& out = output[static_cast<std::size_t>(center)];
      out.r = input[static_cast<std::size_t>(center)].r;
      out.g = input[static_cast<std::size_t>(center)].g;
      out.b = input[static_cast<std::size_t>(center)].b;
      out.a = input[static_cast<std::size_t>(center)].a;
      if (acc.norm_sum != 0.0f) {
        writeProjectedPoint(out, x, y, acc.sum / acc.norm_sum, unprojection);
      }
      else {
        out.x = nan;
        out.y = nan;
        out.z = nan;
      }
    }
  }
  return true;
#else
  (void)input;
  (void)width;
  (void)height;
  (void)tables;
  (void)unprojection;
  (void)output;
  return false;
#endif
}

inline bool
processColumnStrideDepthRVV(const std::vector<RgbPoint>& input,
                            int width,
                            int height,
                            const Tables& tables,
                            const Matrix3f& unprojection,
                            std::vector<RgbPoint>& output)
{
#if defined(__RVV10__)
  output.resize(input.size());
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const auto* base = input.data();
  const ptrdiff_t point_stride_bytes = static_cast<ptrdiff_t>(width) *
                                       static_cast<ptrdiff_t>(sizeof(RgbPoint));

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      const int center = y * width + x;
      const int start_x = std::max(x - tables.window_size, 0);
      const int start_y = std::max(y - tables.window_size, 0);
      const int end_x = std::min(x + tables.window_size, width);
      const int end_y = std::min(y + tables.window_size, height);

      float sum = 0.0f;
      float norm_sum = 0.0f;

      for (int xw = start_x; xw < end_x; ++xw) {
        int yw = start_y;
        while (yw < end_y) {
          const std::size_t vl = __riscv_vsetvl_e32m2(static_cast<std::size_t>(end_y - yw));
          alignas(64) float weights[64];
          for (std::size_t lane = 0; lane < vl; ++lane) {
            const int yy = yw + static_cast<int>(lane);
            const int id = yy * width + xw;
            const float w_depth = depthWeight(tables, x - xw, y - yy);
            const float w_rgb =
                tables.rgb[static_cast<std::size_t>(colorDistance(input[center], input[id]))];
            weights[lane] = w_depth * w_rgb;
          }

          const auto* z_ptr = &base[static_cast<std::size_t>(yw * width + xw)].z;
          const vfloat32m2_t z = __riscv_vlse32_v_f32m2(z_ptr, point_stride_bytes, vl);
          vfloat32m2_t w = __riscv_vle32_v_f32m2(weights, vl);
          vbool16_t z_finite = __riscv_vmfeq_vv_f32m2_b16(z, z, vl);
          z_finite = __riscv_vmand_mm_b16(
              z_finite,
              __riscv_vmflt_vf_f32m2_b16(
                  __riscv_vfabs_v_f32m2(z, vl), std::numeric_limits<float>::infinity(), vl),
              vl);
          const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
          w = __riscv_vmerge_vvm_f32m2(zero, w, z_finite, vl);
          const vfloat32m2_t z_safe = __riscv_vmerge_vvm_f32m2(zero, z, z_finite, vl);
          sum += reduceSumF32M2(__riscv_vfmul_vv_f32m2(w, z_safe, vl), vl);
          norm_sum += reduceSumF32M2(w, vl);
          yw += static_cast<int>(vl);
        }
      }

      auto& out = output[static_cast<std::size_t>(center)];
      out.r = input[static_cast<std::size_t>(center)].r;
      out.g = input[static_cast<std::size_t>(center)].g;
      out.b = input[static_cast<std::size_t>(center)].b;
      out.a = input[static_cast<std::size_t>(center)].a;
      if (norm_sum != 0.0f) {
        writeProjectedPoint(out, x, y, sum / norm_sum, unprojection);
      }
      else {
        out.x = nan;
        out.y = nan;
        out.z = nan;
      }
    }
  }
  return true;
#else
  (void)input;
  (void)width;
  (void)height;
  (void)tables;
  (void)unprojection;
  (void)output;
  return false;
#endif
}

inline void
processCandidate(const std::vector<RgbPoint>& input,
                 int width,
                 int height,
                 const Tables& tables,
                 const Matrix3f& unprojection,
                 std::vector<RgbPoint>& output)
{
#if defined(__RVV10__)
  if (processRVV(input, width, height, tables, unprojection, output))
    return;
#endif
  processScalar(input, width, height, tables, unprojection, output);
}

inline void
processDirectDepthCandidate(const std::vector<RgbPoint>& input,
                            int width,
                            int height,
                            const Tables& tables,
                            const Matrix3f& unprojection,
                            std::vector<RgbPoint>& output)
{
#if defined(__RVV10__)
  if (processColumnStrideDepthRVV(input, width, height, tables, unprojection, output))
    return;
#endif
  processScalar(input, width, height, tables, unprojection, output);
}

inline ErrorStats
compareClouds(const std::vector<RgbPoint>& expected, const std::vector<RgbPoint>& actual)
{
  ErrorStats stats;
  double sum_sq = 0.0;
  std::size_t terms = 0;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    stats.rgb_equal = stats.rgb_equal && expected[i].r == actual[i].r &&
                      expected[i].g == actual[i].g && expected[i].b == actual[i].b;
    const bool expected_finite = std::isfinite(expected[i].z);
    const bool actual_finite = std::isfinite(actual[i].z);
    if (!expected_finite || !actual_finite) {
      if (!expected_finite && !actual_finite)
        ++stats.nan_points;
      else
        stats.max_abs_xyz = std::numeric_limits<float>::infinity();
      continue;
    }
    ++stats.finite_points;
    const float dx = std::abs(expected[i].x - actual[i].x);
    const float dy = std::abs(expected[i].y - actual[i].y);
    const float dz = std::abs(expected[i].z - actual[i].z);
    stats.max_abs_z = std::max(stats.max_abs_z, dz);
    stats.max_abs_xyz = std::max(stats.max_abs_xyz, std::max(dx, std::max(dy, dz)));
    sum_sq += static_cast<double>(dx) * dx + static_cast<double>(dy) * dy +
              static_cast<double>(dz) * dz;
    terms += 3;
  }
  stats.rmse_xyz = terms == 0 ? 0.0f : static_cast<float>(std::sqrt(sum_sq / terms));
  return stats;
}

inline std::uint64_t
checksumCloud(const std::vector<RgbPoint>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  auto mix = [&](std::uint32_t word) {
    hash ^= word;
    hash *= 1099511628211ull;
  };
  for (const auto& p : cloud) {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
    static_assert(sizeof(float) == sizeof(std::uint32_t), "float checksum assumes 32-bit float");
    std::memcpy(&x, &p.x, sizeof(float));
    std::memcpy(&y, &p.y, sizeof(float));
    std::memcpy(&z, &p.z, sizeof(float));
    mix(x);
    mix(y);
    mix(z);
    mix((static_cast<std::uint32_t>(p.r) << 16) |
        (static_cast<std::uint32_t>(p.g) << 8) | static_cast<std::uint32_t>(p.b));
  }
  return hash;
}

inline bool
errorWithinTolerance(const ErrorStats& stats, float max_abs_xyz, float rmse_xyz)
{
  return stats.rgb_equal && stats.max_abs_xyz <= max_abs_xyz && stats.rmse_xyz <= rmse_xyz;
}

} // namespace pcl_rvv_surface_bilateral_upsampling
