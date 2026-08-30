#pragma once

/*
 * 本文件做什么：
 * 这里放 SIFT scale-space 的测试专用 reference、RVV candidate 和固定形态 fixture。
 * scalar reference 复刻 `sift_keypoint.hpp` 里 computeScaleSpace / findScaleSpaceExtrema
 * 的核心边界；RVV candidate 先只接管 Gaussian 权重与局部规约，作为 profile
 * prerequisite 阶段的诊断候选。
 *
 * 证据边界：
 * 本文件不修改 production 源码，不接 public dispatch，也不尝试把整条 SIFT
 * detector pipeline 写成 production direct。它只验证局部 kernel 的可测性、可解释性
 * 和 RVV 反汇编归属。
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <pcl/common/impl/rvv_math.hpp>
#include <riscv_vector.h>
#endif

namespace pcl::keypoints::rvv_test::sift_keypoint
{
enum class ExecutionPath
{
  ScalarReference,
  RvvScaleSpaceKernel
};

struct NeighborBatch
{
  std::vector<float> distance_sqr;
  std::vector<float> value;
};

struct ScaleSpacePoint
{
  NeighborBatch neighborhood;
};

struct ScaleSpaceCase
{
  float base_scale = 0.5f;
  int nr_scales_per_octave = 3;
  std::size_t extrema_radius = 2;
  float contrast_threshold = 0.02f;
  std::vector<float> scales;
  std::vector<ScaleSpacePoint> points;
};

struct ScaleSpaceResponse
{
  std::size_t rows = 0;
  std::size_t cols = 0;
  std::vector<float> dog;
};

inline std::size_t
indexOf(const std::size_t row, const std::size_t col, const std::size_t cols)
{
  return row * cols + col;
}

inline float
responseAt(const ScaleSpaceResponse& response, const std::size_t row, const std::size_t col)
{
  return response.dog[indexOf(row, col, response.cols)];
}

inline ScaleSpaceCase
makeSyntheticScaleSpaceCase(const std::size_t point_count,
                            const std::size_t neighbor_count,
                            const int nr_scales_per_octave)
{
  ScaleSpaceCase data;
  data.nr_scales_per_octave = std::max(1, nr_scales_per_octave);
  data.extrema_radius = std::min<std::size_t>(3, point_count > 1 ? point_count - 1 : 1);
  data.contrast_threshold = 0.005f;
  data.scales.resize(static_cast<std::size_t>(data.nr_scales_per_octave + 3));
  for (int i_scale = 0; i_scale <= data.nr_scales_per_octave + 2; ++i_scale)
  {
    data.scales[static_cast<std::size_t>(i_scale)] =
        data.base_scale *
        std::pow(2.0f,
                 (static_cast<float>(i_scale) - 1.0f) / static_cast<float>(data.nr_scales_per_octave));
  }

  data.points.resize(point_count);
  for (std::size_t point = 0; point < point_count; ++point)
  {
    auto& batch = data.points[point].neighborhood;
    batch.distance_sqr.resize(neighbor_count);
    batch.value.resize(neighbor_count);
    for (std::size_t neighbor = 0; neighbor < neighbor_count; ++neighbor)
    {
      const float ordered_rank = static_cast<float>(neighbor);
      batch.distance_sqr[neighbor] =
          0.015f * ordered_rank + 0.0175f * static_cast<float>(point % 7);
      batch.value[neighbor] =
          0.85f +
          0.08f * std::sin(0.19f * static_cast<float>(point)) +
          0.02f * std::cos(0.31f * static_cast<float>(neighbor)) +
          0.01f * static_cast<float>((point + neighbor) % 11);
    }
  }
  return data;
}

inline ScaleSpaceResponse
computeScaleSpaceScalar(const ScaleSpaceCase& data)
{
  ScaleSpaceResponse response;
  response.rows = data.points.size();
  response.cols = data.scales.size() > 0 ? data.scales.size() - 1 : 0;
  response.dog.assign(response.rows * response.cols, 0.0f);

  for (std::size_t point = 0; point < data.points.size(); ++point)
  {
    const auto& batch = data.points[point].neighborhood;
    float previous_filter_response = 0.0f;
    for (std::size_t i_scale = 0; i_scale < data.scales.size(); ++i_scale)
    {
      const float sigma_sqr = data.scales[i_scale] * data.scales[i_scale];
      float numerator = 0.0f;
      float denominator = 0.0f;
      for (std::size_t neighbor = 0; neighbor < batch.distance_sqr.size(); ++neighbor)
      {
        const float dist_sqr = batch.distance_sqr[neighbor];
        if (dist_sqr <= 9.0f * sigma_sqr)
        {
          const float weight = std::exp(-0.5f * dist_sqr / sigma_sqr);
          numerator += batch.value[neighbor] * weight;
          denominator += weight;
        }
        else
        {
          break;
        }
      }

      const float filter_response = numerator / denominator;
      if (i_scale > 0)
      {
        response.dog[indexOf(point, i_scale - 1, response.cols)] =
            filter_response - previous_filter_response;
      }
      previous_filter_response = filter_response;
    }
  }

  return response;
}

inline ScaleSpaceResponse
computeScaleSpaceCandidate(const ScaleSpaceCase& data, std::size_t* rvv_vector_chunks = nullptr)
{
  ScaleSpaceResponse response;
  response.rows = data.points.size();
  response.cols = data.scales.size() > 0 ? data.scales.size() - 1 : 0;
  response.dog.assign(response.rows * response.cols, 0.0f);
  if (rvv_vector_chunks)
    *rvv_vector_chunks = 0;

#if defined(__RVV10__) && defined(__riscv_vector)
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  (void)vlmax;
#endif

  for (std::size_t point = 0; point < data.points.size(); ++point)
  {
    const auto& batch = data.points[point].neighborhood;
    float previous_filter_response = 0.0f;
    for (std::size_t i_scale = 0; i_scale < data.scales.size(); ++i_scale)
    {
      const float sigma_sqr = data.scales[i_scale] * data.scales[i_scale];
      const float cutoff_sqr = 9.0f * sigma_sqr;
      const float exponent_scale = -0.5f / sigma_sqr;
      float numerator = 0.0f;
      float denominator = 0.0f;

#if defined(__RVV10__) && defined(__riscv_vector)
      for (std::size_t neighbor = 0; neighbor < batch.distance_sqr.size();)
      {
        const std::size_t vl = __riscv_vsetvl_e32m2(batch.distance_sqr.size() - neighbor);
        const vfloat32m2_t dist = __riscv_vle32_v_f32m2(batch.distance_sqr.data() + neighbor, vl);
        const vfloat32m2_t value = __riscv_vle32_v_f32m2(batch.value.data() + neighbor, vl);
        const vbool16_t outside = __riscv_vmfgt_vf_f32m2_b16(dist, cutoff_sqr, vl);
        const long first_outside = __riscv_vfirst_m_b16(outside, vl);
        if (first_outside == 0)
          break;
        const std::size_t active_vl =
            first_outside > 0 ? static_cast<std::size_t>(first_outside) : vl;
        const vfloat32m2_t exponent = __riscv_vfmul_vf_f32m2(dist, exponent_scale, active_vl);
        const vfloat32m2_t weights = pcl::expf_RVV_f32m2(exponent, active_vl);
        const vfloat32m2_t weighted_values =
            __riscv_vfmul_vv_f32m2(value, weights, active_vl);
        const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
        numerator += __riscv_vfmv_f_s_f32m1_f32(
            __riscv_vfredosum_vs_f32m2_f32m1(weighted_values, zero, active_vl));
        denominator += __riscv_vfmv_f_s_f32m1_f32(
            __riscv_vfredosum_vs_f32m2_f32m1(weights, zero, active_vl));
        if (first_outside > 0)
          break;
        neighbor += active_vl;
        if (rvv_vector_chunks)
          ++(*rvv_vector_chunks);
      }
#else
      for (std::size_t neighbor = 0; neighbor < batch.distance_sqr.size(); ++neighbor)
      {
        const float dist_sqr = batch.distance_sqr[neighbor];
        if (dist_sqr <= cutoff_sqr)
        {
          const float weight = std::exp(dist_sqr * exponent_scale);
          numerator += batch.value[neighbor] * weight;
          denominator += weight;
        }
        else
        {
          break;
        }
      }
#endif

      const float filter_response = numerator / denominator;
      if (i_scale > 0)
      {
        response.dog[indexOf(point, i_scale - 1, response.cols)] =
            filter_response - previous_filter_response;
      }
      previous_filter_response = filter_response;
    }
  }

  return response;
}

inline std::vector<int>
findScaleSpaceExtremaScalar(const ScaleSpaceResponse& response,
                            const std::size_t radius,
                            const float contrast_threshold)
{
  std::vector<int> extrema_indices;
  if (response.rows == 0 || response.cols < 3)
    return extrema_indices;

  for (std::size_t point = 0; point < response.rows; ++point)
  {
    const std::size_t row_begin = point > radius ? point - radius : 0;
    const std::size_t row_end = std::min(response.rows, point + radius + 1);
    for (std::size_t scale = 1; scale + 1 < response.cols; ++scale)
    {
      const float current = responseAt(response, point, scale);
      if (std::abs(current) < contrast_threshold)
        continue;

      float min_value = responseAt(response, row_begin, scale);
      float max_value = min_value;
      for (std::size_t row = row_begin; row < row_end; ++row)
      {
        const float value = responseAt(response, row, scale);
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
      }

      if ((current == min_value && current < responseAt(response, point, scale - 1) &&
           current < responseAt(response, point, scale + 1)) ||
          (current == max_value && current > responseAt(response, point, scale - 1) &&
           current > responseAt(response, point, scale + 1)))
      {
        extrema_indices.push_back(static_cast<int>(point));
      }
    }
  }

  return extrema_indices;
}

inline std::uint64_t
checksumResponses(const ScaleSpaceResponse& response)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const float value : response.dog)
  {
    const auto quantized = static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 100000.0));
    std::uint64_t bits = 0;
    std::memcpy(&bits, &quantized, sizeof(bits));
    hash ^= bits;
    hash *= 1099511628211ull;
  }
  return hash;
}

inline std::uint64_t
checksumIndices(const std::vector<int>& indices)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const int index : indices)
  {
    const std::uint64_t bits = static_cast<std::uint64_t>(index);
    hash ^= bits + 0x9e3779b97f4a7c15ull + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ull;
  }
  return hash;
}

} // namespace pcl::keypoints::rvv_test::sift_keypoint
