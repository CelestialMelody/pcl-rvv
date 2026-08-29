#pragma once

/*
 * 本文件做什么：
 * 这里放 Harris 2D 首阶段的 scalar reference（标量参考链路）和 RVV candidate
 * （RVV 候选链路）。标量参考复刻 harris_2d.hpp 中 organized intensity
 * derivative（有组织强度图导数）、second-moment（局部二阶矩）和四类 response
 * map（响应图）公式；NMS sort（非极大值抑制排序）、occupancy map（占用图）
 * 和 critical push_back 保持在本阶段外。
 *
 * 证据边界：
 * 当前 candidate 是 production-shaped diagnostic（生产形态诊断）：输入形状、
 * 窗口上界和公式复刻 production 语义，但不修改生产头文件，也不证明 public
 * dispatch（公开入口分流）已经接入 RVV。
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <pcl/keypoints/harris_2d.h>
#include <pcl/point_types.h>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::keypoints::rvv_test::harris_2d
{
enum class ResponseMethod
{
  Harris,
  Noble,
  Lowe,
  Tomasi
};

enum class ExecutionPath
{
  ScalarFallback,
  RvvDerivativeResponse
};

struct ResponseImage
{
  std::size_t width = 0;
  std::size_t height = 0;
  std::vector<float> x;
  std::vector<float> y;
  std::vector<float> z;
  std::vector<float> intensity;
};

inline pcl::PointCloud<pcl::PointXYZI>
makePublicInputCloud(const ResponseImage& image)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = image.width;
  cloud.height = image.height;
  cloud.is_dense = true;
  cloud.points.resize(image.intensity.size());
  for (std::size_t index = 0; index < image.intensity.size(); ++index)
  {
    cloud.points[index].x = image.x[index];
    cloud.points[index].y = image.y[index];
    cloud.points[index].z = image.z[index];
    cloud.points[index].intensity = image.intensity[index];
  }
  return cloud;
}

inline ResponseImage
makeResponseImage(const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  ResponseImage image;
  image.width = cloud.width;
  image.height = cloud.height;
  image.x.resize(cloud.points.size());
  image.y.resize(cloud.points.size());
  image.z.resize(cloud.points.size());
  image.intensity.resize(cloud.points.size());
  for (std::size_t index = 0; index < cloud.points.size(); ++index)
  {
    image.x[index] = cloud.points[index].x;
    image.y[index] = cloud.points[index].y;
    image.z[index] = cloud.points[index].z;
    image.intensity[index] = cloud.points[index].intensity;
  }
  return image;
}

inline std::size_t
indexOf(const std::size_t x, const std::size_t y, const std::size_t width)
{
  return y * width + x;
}

inline ResponseImage
makeSyntheticImage(const std::size_t width, const std::size_t height)
{
  ResponseImage image;
  image.width = width;
  image.height = height;
  const std::size_t n = width * height;
  image.x.resize(n);
  image.y.resize(n);
  image.z.resize(n);
  image.intensity.resize(n);

  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      const std::size_t index = indexOf(col, row, width);
      image.x[index] = static_cast<float>(col) * 0.25f;
      image.y[index] = static_cast<float>(row) * 0.5f;
      image.z[index] = 1.0f + static_cast<float>((index * 7) % 19) * 0.01f;
      image.intensity[index] =
          0.35f * static_cast<float>((col * col + 3 * row) % 23) +
          0.17f * static_cast<float>((row * row + 5 * col) % 17);
    }
  }
  return image;
}

inline void
injectNonFinitePoint(ResponseImage& image, const std::size_t index)
{
  if (index >= image.x.size())
    return;
  image.x[index] = std::numeric_limits<float>::quiet_NaN();
}

inline bool
isFinitePoint(const ResponseImage& image, const std::size_t index)
{
  return std::isfinite(image.x[index]) && std::isfinite(image.y[index]) &&
         std::isfinite(image.z[index]);
}

inline void
computeDerivativesScalar(const ResponseImage& input,
                         std::vector<float>& derivative_rows,
                         std::vector<float>& derivative_cols)
{
  derivative_rows.assign(input.width * input.height, 0.0f);
  derivative_cols.assign(input.width * input.height, 0.0f);
  if (input.width < 2 || input.height < 2)
    return;

  const int w = static_cast<int>(input.width) - 1;
  const int h = static_cast<int>(input.height) - 1;
  const auto intensity = [&](const int x, const int y) -> float {
    return input.intensity[indexOf(static_cast<std::size_t>(x),
                                   static_cast<std::size_t>(y),
                                   input.width)];
  };
  const auto store = [&](std::vector<float>& values, const int x, const int y, const float v) {
    values[indexOf(static_cast<std::size_t>(x), static_cast<std::size_t>(y), input.width)] = v;
  };

  store(derivative_cols, 0, 0, (intensity(0, 1) - intensity(0, 0)) * 0.5f);
  store(derivative_rows, 0, 0, (intensity(1, 0) - intensity(0, 0)) * 0.5f);

  for (int i = 1; i < w; ++i)
    store(derivative_cols, i, 0, (intensity(i, 1) - intensity(i, 0)) * 0.5f);

  store(derivative_rows, w, 0, (intensity(w, 0) - intensity(w - 1, 0)) * 0.5f);
  store(derivative_cols, w, 0, (intensity(w, 1) - intensity(w, 0)) * 0.5f);

  for (int j = 1; j < h; ++j)
  {
    store(derivative_rows, 0, j, (intensity(1, j) - intensity(0, j)) * 0.5f);
    for (int i = 1; i < w; ++i)
    {
      store(derivative_rows, i, j, (intensity(i + 1, j) - intensity(i - 1, j)) * 0.5f);
      store(derivative_cols, i, j, (intensity(i, j + 1) - intensity(i, j - 1)) * 0.5f);
    }
    store(derivative_rows, w, j, (intensity(w, j) - intensity(w - 1, j)) * 0.5f);
  }

  store(derivative_cols, 0, h, (intensity(0, h) - intensity(0, h - 1)) * 0.5f);
  store(derivative_rows, 0, h, (intensity(1, h) - intensity(0, h)) * 0.5f);

  for (int i = 1; i < w; ++i)
    store(derivative_cols, i, h, (intensity(i, h) - intensity(i, h - 1)) * 0.5f);
  store(derivative_rows, w, h, (intensity(w, h) - intensity(w - 1, h)) * 0.5f);
  store(derivative_cols, w, h, (intensity(w, h) - intensity(w, h - 1)) * 0.5f);
}

inline void
computeSecondMomentScalar(const std::vector<float>& derivative_rows,
                          const std::vector<float>& derivative_cols,
                          const std::size_t width,
                          const std::size_t height,
                          const std::size_t index,
                          const int half_window_width,
                          const int half_window_height,
                          float coefficients[3])
{
  coefficients[0] = coefficients[1] = coefficients[2] = 0.0f;
  const int x = static_cast<int>(index % width);
  const int y = static_cast<int>(index / width);
  const int endx = std::min(static_cast<int>(width), x + half_window_width);
  const int endy = std::min(static_cast<int>(height), y + half_window_height);
  for (int xx = std::max(0, x - half_window_width); xx < endx; ++xx)
  {
    for (int yy = std::max(0, y - half_window_height); yy < endy; ++yy)
    {
      const std::size_t offset = indexOf(static_cast<std::size_t>(xx),
                                         static_cast<std::size_t>(yy),
                                         width);
      const float ix = derivative_rows[offset];
      const float iy = derivative_cols[offset];
      coefficients[0] += ix * ix;
      coefficients[1] += ix * iy;
      coefficients[2] += iy * iy;
    }
  }
}

inline float
computeResponseValue(const ResponseMethod method, const float covar[3])
{
  const float trace = covar[0] + covar[2];
  if (method == ResponseMethod::Tomasi)
  {
    const float delta = covar[0] - covar[2];
    return (trace - std::sqrt(delta * delta + 4.0f * covar[1] * covar[1])) * 0.5f;
  }
  if (trace == 0.0f)
    return 0.0f;
  const float det = covar[0] * covar[2] - covar[1] * covar[1];
  if (method == ResponseMethod::Harris)
    return 0.04f + det - 0.04f * trace * trace;
  if (method == ResponseMethod::Noble)
    return det / trace;
  return det / (trace * trace);
}

inline void
computeResponsesScalar(const ResponseImage& input,
                       const ResponseMethod method,
                       const int window_width,
                       const int window_height,
                       ResponseImage& output)
{
  output.width = input.width;
  output.height = input.height;
  output.x = input.x;
  output.y = input.y;
  output.z = input.z;
  output.intensity.assign(input.width * input.height, 0.0f);

  std::vector<float> derivative_rows;
  std::vector<float> derivative_cols;
  computeDerivativesScalar(input, derivative_rows, derivative_cols);

  const int half_window_width = window_width / 2;
  const int half_window_height = window_height / 2;
  float covar[3] = {};
  for (std::size_t index = 0; index < output.intensity.size(); ++index)
  {
    if (!isFinitePoint(input, index))
      continue;
    computeSecondMomentScalar(derivative_rows,
                              derivative_cols,
                              input.width,
                              input.height,
                              index,
                              half_window_width,
                              half_window_height,
                              covar);
    output.intensity[index] = computeResponseValue(method, covar);
  }
}

#if defined(__RVV10__) && defined(__riscv_vector)
inline vfloat32m2_t
loadDerivativeOrZero(const std::vector<float>& values,
                     const std::size_t width,
                     const std::size_t height,
                     const int x,
                     const int y,
                     const std::size_t vl)
{
  if (x < 0 || y < 0 || y >= static_cast<int>(height) || x >= static_cast<int>(width))
    return __riscv_vfmv_v_f_f32m2(0.0f, vl);
  return __riscv_vle32_v_f32m2(values.data() + indexOf(static_cast<std::size_t>(x),
                                                       static_cast<std::size_t>(y),
                                                       width),
                               vl);
}

inline vfloat32m2_t
computeResponseValueRVV(const ResponseMethod method,
                        const vfloat32m2_t covar_xx,
                        const vfloat32m2_t covar_xy,
                        const vfloat32m2_t covar_yy,
                        const vbool16_t finite_mask,
                        const std::size_t vl)
{
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
  const vfloat32m2_t trace = __riscv_vfadd_vv_f32m2(covar_xx, covar_yy, vl);
  vfloat32m2_t response = zero;

  if (method == ResponseMethod::Tomasi)
  {
    const vfloat32m2_t delta = __riscv_vfsub_vv_f32m2(covar_xx, covar_yy, vl);
    const vfloat32m2_t delta2 = __riscv_vfmul_vv_f32m2(delta, delta, vl);
    const vfloat32m2_t xy2 = __riscv_vfmul_vv_f32m2(covar_xy, covar_xy, vl);
    const vfloat32m2_t sqrt_input =
        __riscv_vfadd_vv_f32m2(delta2, __riscv_vfmul_vf_f32m2(xy2, 4.0f, vl), vl);
    response = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(trace, __riscv_vfsqrt_v_f32m2(sqrt_input, vl), vl),
        0.5f,
        vl);
  }
  else
  {
    const vfloat32m2_t det = __riscv_vfsub_vv_f32m2(
        __riscv_vfmul_vv_f32m2(covar_xx, covar_yy, vl),
        __riscv_vfmul_vv_f32m2(covar_xy, covar_xy, vl),
        vl);
    const vbool16_t trace_nonzero = __riscv_vmfne_vf_f32m2_b16(trace, 0.0f, vl);
    const vbool16_t active = __riscv_vmand_mm_b16(finite_mask, trace_nonzero, vl);
    if (method == ResponseMethod::Harris)
    {
      const vfloat32m2_t trace2 = __riscv_vfmul_vv_f32m2(trace, trace, vl);
      response = __riscv_vfadd_vf_f32m2(
          __riscv_vfsub_vv_f32m2(det, __riscv_vfmul_vf_f32m2(trace2, 0.04f, vl), vl),
          0.04f,
          vl);
    }
    else if (method == ResponseMethod::Noble)
      response = __riscv_vfdiv_vv_f32m2(det, trace, vl);
    else
      response = __riscv_vfdiv_vv_f32m2(det, __riscv_vfmul_vv_f32m2(trace, trace, vl), vl);
    return __riscv_vmerge_vvm_f32m2(zero, response, active, vl);
  }

  return __riscv_vmerge_vvm_f32m2(zero, response, finite_mask, vl);
}

inline void
computeInteriorResponsesRVV(const ResponseImage& input,
                            const std::vector<float>& derivative_rows,
                            const std::vector<float>& derivative_cols,
                            const ResponseMethod method,
                            const int half_window_width,
                            const int half_window_height,
                            ResponseImage& output,
                            std::size_t* vector_chunks)
{
  const int width = static_cast<int>(input.width);
  const int height = static_cast<int>(input.height);
  const int x_begin = half_window_width;
  const int x_end = width - half_window_width;
  const int y_begin = half_window_height;
  const int y_end = height - half_window_height;
  if (x_begin >= x_end || y_begin >= y_end)
    return;

  for (int y = y_begin; y < y_end; ++y)
  {
    for (int x = x_begin; x < x_end;)
    {
      const std::size_t vl =
          __riscv_vsetvl_e32m2(static_cast<std::size_t>(x_end - x));
      vfloat32m2_t covar_xx = __riscv_vfmv_v_f_f32m2(0.0f, vl);
      vfloat32m2_t covar_xy = __riscv_vfmv_v_f_f32m2(0.0f, vl);
      vfloat32m2_t covar_yy = __riscv_vfmv_v_f_f32m2(0.0f, vl);
      for (int xx = x - half_window_width; xx < x + half_window_width; ++xx)
      {
        for (int yy = y - half_window_height; yy < y + half_window_height; ++yy)
        {
          const vfloat32m2_t ix =
              loadDerivativeOrZero(derivative_rows, input.width, input.height, xx, yy, vl);
          const vfloat32m2_t iy =
              loadDerivativeOrZero(derivative_cols, input.width, input.height, xx, yy, vl);
          covar_xx =
              __riscv_vfadd_vv_f32m2(covar_xx, __riscv_vfmul_vv_f32m2(ix, ix, vl), vl);
          covar_xy =
              __riscv_vfadd_vv_f32m2(covar_xy, __riscv_vfmul_vv_f32m2(ix, iy, vl), vl);
          covar_yy =
              __riscv_vfadd_vv_f32m2(covar_yy, __riscv_vfmul_vv_f32m2(iy, iy, vl), vl);
        }
      }

      const std::size_t offset = indexOf(static_cast<std::size_t>(x),
                                         static_cast<std::size_t>(y),
                                         input.width);
      const vfloat32m2_t px = __riscv_vle32_v_f32m2(input.x.data() + offset, vl);
      const vfloat32m2_t py = __riscv_vle32_v_f32m2(input.y.data() + offset, vl);
      const vfloat32m2_t pz = __riscv_vle32_v_f32m2(input.z.data() + offset, vl);
      const vfloat32m2_t max_finite =
          __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::max(), vl);
      const vbool16_t finite_x =
          __riscv_vmfle_vv_f32m2_b16(__riscv_vfsgnjx_vv_f32m2(px, px, vl), max_finite, vl);
      const vbool16_t finite_y =
          __riscv_vmfle_vv_f32m2_b16(__riscv_vfsgnjx_vv_f32m2(py, py, vl), max_finite, vl);
      const vbool16_t finite_z =
          __riscv_vmfle_vv_f32m2_b16(__riscv_vfsgnjx_vv_f32m2(pz, pz, vl), max_finite, vl);
      const vbool16_t finite_mask =
          __riscv_vmand_mm_b16(__riscv_vmand_mm_b16(finite_x, finite_y, vl), finite_z, vl);
      const vfloat32m2_t response =
          computeResponseValueRVV(method, covar_xx, covar_xy, covar_yy, finite_mask, vl);
      __riscv_vse32_v_f32m2(output.intensity.data() + offset, response, vl);

      if (vector_chunks)
        ++(*vector_chunks);
      x += static_cast<int>(vl);
    }
  }
}
#endif

inline ExecutionPath
computeResponsesCandidate(const ResponseImage& input,
                          const ResponseMethod method,
                          const int window_width,
                          const int window_height,
                          ResponseImage& output,
                          std::size_t* vector_chunks = nullptr)
{
  if (vector_chunks)
    *vector_chunks = 0;

#if defined(__RVV10__) && defined(__riscv_vector)
  output.width = input.width;
  output.height = input.height;
  output.x = input.x;
  output.y = input.y;
  output.z = input.z;
  output.intensity.assign(input.width * input.height, 0.0f);

  std::vector<float> derivative_rows;
  std::vector<float> derivative_cols;
  computeDerivativesScalar(input, derivative_rows, derivative_cols);

  const int half_window_width = window_width / 2;
  const int half_window_height = window_height / 2;
  float covar[3] = {};
  for (std::size_t index = 0; index < output.intensity.size(); ++index)
  {
    const int x = static_cast<int>(index % input.width);
    const int y = static_cast<int>(index / input.width);
    const bool interior = x >= half_window_width &&
                          x < static_cast<int>(input.width) - half_window_width &&
                          y >= half_window_height &&
                          y < static_cast<int>(input.height) - half_window_height;
    if (interior || !isFinitePoint(input, index))
      continue;
    computeSecondMomentScalar(derivative_rows,
                              derivative_cols,
                              input.width,
                              input.height,
                              index,
                              half_window_width,
                              half_window_height,
                              covar);
    output.intensity[index] = computeResponseValue(method, covar);
  }
  computeInteriorResponsesRVV(input,
                              derivative_rows,
                              derivative_cols,
                              method,
                              half_window_width,
                              half_window_height,
                              output,
                              vector_chunks);
  return ExecutionPath::RvvDerivativeResponse;
#else
  computeResponsesScalar(input, method, window_width, window_height, output);
  return ExecutionPath::ScalarFallback;
#endif
}

inline ExecutionPath
computeResponsesPublic(const ResponseImage& input,
                       const ResponseMethod method,
                       const int window_width,
                       const int window_height,
                       ResponseImage& output)
{
  const auto public_input = makePublicInputCloud(input);
  pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI> detector;
  detector.setInputCloud(public_input.makeShared());
  detector.setWindowWidth(window_width);
  detector.setWindowHeight(window_height);
  detector.setNonMaxSupression(false);
  detector.setKSearch(1);
  switch (method)
  {
    case ResponseMethod::Harris:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::HARRIS);
      break;
    case ResponseMethod::Noble:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::NOBLE);
      break;
    case ResponseMethod::Lowe:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::LOWE);
      break;
    case ResponseMethod::Tomasi:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::TOMASI);
      break;
  }

  pcl::PointCloud<pcl::PointXYZI> public_output;
  detector.compute(public_output);
  output = makeResponseImage(public_output);
#if defined(__RVV10__) && defined(__riscv_vector)
  return ExecutionPath::RvvDerivativeResponse;
#else
  return ExecutionPath::ScalarFallback;
#endif
}

inline std::uint64_t
checksumResponses(const ResponseImage& image)
{
  std::uint64_t hash = 1469598103934665603ull;
  const auto mix = [&](const std::int64_t bucket) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(bucket));
    std::memcpy(&bits, &bucket, sizeof(bits));
    hash ^= bits;
    hash *= 1099511628211ull;
  };
  for (const float value : image.intensity)
    mix(static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 1.0e4)));
  return hash;
}

inline const char*
methodName(const ResponseMethod method)
{
  switch (method)
  {
    case ResponseMethod::Harris:
      return "harris";
    case ResponseMethod::Noble:
      return "noble";
    case ResponseMethod::Lowe:
      return "lowe";
    case ResponseMethod::Tomasi:
      return "tomasi";
  }
  return "unknown";
}

inline ResponseMethod
parseMethod(const std::string& value)
{
  if (value == "noble")
    return ResponseMethod::Noble;
  if (value == "lowe")
    return ResponseMethod::Lowe;
  if (value == "tomasi")
    return ResponseMethod::Tomasi;
  return ResponseMethod::Harris;
}
} // namespace pcl::keypoints::rvv_test::harris_2d
