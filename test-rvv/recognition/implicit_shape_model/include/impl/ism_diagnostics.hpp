/*
 * 本文件做什么：
 * 这里保存 Implicit Shape Model（隐式形状模型，简称 ISM）topic 的测试专用
 * diagnostic helper（诊断辅助函数）。这些 helper 复刻 production 源码里的三个
 * 局部热点公式边界：descriptor-to-cluster distance（描述子到聚类中心距离）、
 * calculateSigmas 的 pairwise max-dot（两两点积最大值）和 vote density 的 Gaussian
 * weighted sum（高斯加权求和）。
 *
 * 证据边界：
 * 本文件不修改 `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`，
 * 也不证明 `trainISM()` 或 `findObjects()` 的完整入口收益。Std build 是标量参考链路；
 * RVV build 只证明这些局部公式能否被 RVV（RISC-V Vector，可变长度向量扩展）表达，
 * 后续是否进入 production integration loop（生产接入闭环）仍取决于板卡和入口级证据。
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <riscv_vector.h>
#endif

namespace pcl::test::implicit_shape_model_rvv
{

struct IsmPoint
{
  float x;
  float y;
  float z;
};

struct DistanceResult
{
  std::size_t index;
  float distance;
};

inline const char*
pathName()
{
#if defined(__RVV10__)
  return "rvv";
#else
  return "std";
#endif
}

inline std::uint64_t
mixChecksum(const std::uint64_t seed, const std::uint64_t value)
{
  return (seed ^ value) * 1099511628211ull;
}

inline std::uint64_t
checksumFloat(const float value)
{
  union
  {
    float f;
    std::uint32_t u;
  } bits{value};
  return static_cast<std::uint64_t>(bits.u);
}

inline std::vector<float>
makeDescriptor(const std::size_t dimensions)
{
  std::vector<float> descriptor(dimensions);
  for (std::size_t i = 0; i < dimensions; ++i)
    descriptor[i] = 0.25f + static_cast<float>((i * 37u + 11u) % 211u) * 0.0031f;
  return descriptor;
}

inline std::vector<float>
makeDescriptorBatch(const std::size_t descriptors, const std::size_t dimensions)
{
  std::vector<float> batch(descriptors * dimensions);
  for (std::size_t point = 0; point < descriptors; ++point)
  {
    const bool zero_descriptor = (point % 29u) == 0u;
    for (std::size_t dim = 0; dim < dimensions; ++dim)
    {
      batch[point * dimensions + dim] = zero_descriptor
                                            ? 0.0f
                                            : 0.18f + static_cast<float>((point * 43u + dim * 37u + 19u) % 251u) * 0.0027f;
    }
  }
  return batch;
}

inline std::vector<float>
makeClusterCenters(const std::size_t clusters, const std::size_t dimensions)
{
  std::vector<float> centers(clusters * dimensions);
  for (std::size_t c = 0; c < clusters; ++c)
    for (std::size_t d = 0; d < dimensions; ++d)
      centers[c * dimensions + d] =
          0.1f + static_cast<float>((c * 17u + d * 29u + 7u) % 257u) * 0.0023f;
  return centers;
}

inline std::vector<IsmPoint>
makeTrainingPoints(const std::size_t count)
{
  std::vector<IsmPoint> points(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    points[i].x = 0.25f + static_cast<float>((i * 13u + 3u) % 1009u) * 0.004f;
    points[i].y = 0.15f + static_cast<float>((i * 23u + 5u) % 887u) * 0.003f;
    points[i].z = 0.35f + static_cast<float>((i * 31u + 7u) % 769u) * 0.002f;
  }
  return points;
}

inline std::vector<float>
makeSquaredDistances(const std::size_t count)
{
  std::vector<float> distances(count);
  for (std::size_t i = 0; i < count; ++i)
    distances[i] = 0.0005f + static_cast<float>((i * 19u + 13u) % 997u) * 0.00003f;
  return distances;
}

inline std::vector<float>
makeVoteStrengths(const std::size_t count)
{
  std::vector<float> strengths(count);
  for (std::size_t i = 0; i < count; ++i)
    strengths[i] = 0.2f + static_cast<float>((i * 41u + 17u) % 251u) * 0.002f;
  return strengths;
}

inline DistanceResult
nearestClusterDistanceStd(const float* descriptor,
                          const float* centers,
                          const std::size_t clusters,
                          const std::size_t dimensions)
{
  DistanceResult best{0, std::numeric_limits<float>::max()};
  for (std::size_t c = 0; c < clusters; ++c)
  {
    float distance = 0.0f;
    const float* center = centers + c * dimensions;
    for (std::size_t d = 0; d < dimensions; ++d)
    {
      const float diff = descriptor[d] - center[d];
      distance += diff * diff;
    }
    if (distance < best.distance)
      best = {c, distance};
  }
  return best;
}

inline float
maxPairwiseDotSigmaStd(const IsmPoint* points, const std::size_t count)
{
  float max_distance = 0.0f;
  for (std::size_t i = 0; i + 1 < count; ++i)
  {
    for (std::size_t j = i + 1; j < count; ++j)
    {
      const float value =
          points[i].x * points[j].x + points[i].y * points[j].y + points[i].z * points[j].z;
      if (value > max_distance)
        max_distance = value;
    }
  }
  return std::sqrt(max_distance);
}

inline float
densityWeightedSumStd(const float* squared_distances,
                      const float* strengths,
                      const std::size_t count,
                      const float sigma)
{
  const float sigma2 = sigma * sigma;
  float sum = 0.0f;
  for (std::size_t i = 0; i < count; ++i)
    sum += strengths[i] * std::exp(-squared_distances[i] / sigma2);
  return sum;
}

#if defined(__RVV10__)
inline float
sumF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  const vfloat32m1_t reduced = __riscv_vfredusum_vs_f32m2_f32m1(values, zero, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline float
maxF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t init =
      __riscv_vfmv_s_f_f32m1(-std::numeric_limits<float>::infinity(), 1);
  const vfloat32m1_t reduced = __riscv_vfredmax_vs_f32m2_f32m1(values, init, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}

inline DistanceResult
nearestClusterDistanceRVV(const float* descriptor,
                          const float* centers,
                          const std::size_t clusters,
                          const std::size_t dimensions)
{
  DistanceResult best{0, std::numeric_limits<float>::max()};
  for (std::size_t c = 0; c < clusters; ++c)
  {
    const float* center = centers + c * dimensions;
    float distance = 0.0f;
    for (std::size_t d = 0; d < dimensions;)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2(dimensions - d);
      const vfloat32m2_t lhs = __riscv_vle32_v_f32m2(descriptor + d, vl);
      const vfloat32m2_t rhs = __riscv_vle32_v_f32m2(center + d, vl);
      const vfloat32m2_t diff = __riscv_vfsub_vv_f32m2(lhs, rhs, vl);
      const vfloat32m2_t squared = __riscv_vfmul_vv_f32m2(diff, diff, vl);
      distance += sumF32m2(squared, vl);
      d += vl;
    }
    if (distance < best.distance)
      best = {c, distance};
  }
  return best;
}

inline float
maxPairwiseDotSigmaRVV(const IsmPoint* points, const std::size_t count)
{
  float max_distance = 0.0f;
  for (std::size_t i = 0; i + 1 < count; ++i)
  {
    float row_max = -std::numeric_limits<float>::infinity();
    for (std::size_t j = i + 1; j < count;)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2(count - j);
      const auto stride = static_cast<std::ptrdiff_t>(sizeof(IsmPoint));
      const vfloat32m2_t x = __riscv_vlse32_v_f32m2(&points[j].x, stride, vl);
      const vfloat32m2_t y = __riscv_vlse32_v_f32m2(&points[j].y, stride, vl);
      const vfloat32m2_t z = __riscv_vlse32_v_f32m2(&points[j].z, stride, vl);
      vfloat32m2_t dot = __riscv_vfmul_vf_f32m2(x, points[i].x, vl);
      dot = __riscv_vfmacc_vf_f32m2(dot, points[i].y, y, vl);
      dot = __riscv_vfmacc_vf_f32m2(dot, points[i].z, z, vl);
      row_max = std::max(row_max, maxF32m2(dot, vl));
      j += vl;
    }
    max_distance = std::max(max_distance, row_max);
  }
  return std::sqrt(max_distance);
}

inline float
densityWeightedSumRVV(const float* squared_distances,
                      const float* strengths,
                      const std::size_t count,
                      const float sigma)
{
  const float inv_sigma2 = 1.0f / (sigma * sigma);
  float sum = 0.0f;
  for (std::size_t i = 0; i < count;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const vfloat32m2_t dist = __riscv_vle32_v_f32m2(squared_distances + i, vl);
    const vfloat32m2_t strength = __riscv_vle32_v_f32m2(strengths + i, vl);
    const vfloat32m2_t exponent = __riscv_vfmul_vf_f32m2(dist, -inv_sigma2, vl);
    const vfloat32m2_t weight = pcl::expf_RVV_f32m2(exponent, vl);
    sum += sumF32m2(__riscv_vfmul_vv_f32m2(strength, weight, vl), vl);
    i += vl;
  }
  return sum;
}
#endif

inline DistanceResult
nearestClusterDistanceCandidate(const float* descriptor,
                                const float* centers,
                                const std::size_t clusters,
                                const std::size_t dimensions)
{
#if defined(__RVV10__)
  return nearestClusterDistanceRVV(descriptor, centers, clusters, dimensions);
#else
  return nearestClusterDistanceStd(descriptor, centers, clusters, dimensions);
#endif
}

inline float
maxPairwiseDotSigmaCandidate(const IsmPoint* points, const std::size_t count)
{
#if defined(__RVV10__)
  return maxPairwiseDotSigmaRVV(points, count);
#else
  return maxPairwiseDotSigmaStd(points, count);
#endif
}

inline float
densityWeightedSumCandidate(const float* squared_distances,
                            const float* strengths,
                            const std::size_t count,
                            const float sigma)
{
#if defined(__RVV10__)
  return densityWeightedSumRVV(squared_distances, strengths, count, sigma);
#else
  return densityWeightedSumStd(squared_distances, strengths, count, sigma);
#endif
}

inline std::vector<int>
assignDescriptorBatchStd(const float* descriptors,
                         const float* centers,
                         const std::size_t descriptor_count,
                         const std::size_t clusters,
                         const std::size_t dimensions)
{
  std::vector<int> assignments(descriptor_count, -1);
  for (std::size_t point = 0; point < descriptor_count; ++point)
  {
    const float* descriptor = descriptors + point * dimensions;
    float descriptor_sum = 0.0f;
    for (std::size_t dim = 0; dim < dimensions; ++dim)
      descriptor_sum += descriptor[dim];
    if (descriptor_sum < std::numeric_limits<float>::epsilon())
      continue;

    assignments[point] =
        static_cast<int>(nearestClusterDistanceStd(descriptor, centers, clusters, dimensions).index);
  }
  return assignments;
}

#if defined(__RVV10__)
inline float
sumDescriptorRVV(const float* descriptor, const std::size_t dimensions)
{
  float sum = 0.0f;
  for (std::size_t dim = 0; dim < dimensions;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(dimensions - dim);
    const vfloat32m2_t values = __riscv_vle32_v_f32m2(descriptor + dim, vl);
    sum += sumF32m2(values, vl);
    dim += vl;
  }
  return sum;
}

inline std::vector<int>
assignDescriptorBatchRVV(const float* descriptors,
                         const float* centers,
                         const std::size_t descriptor_count,
                         const std::size_t clusters,
                         const std::size_t dimensions)
{
  std::vector<int> assignments(descriptor_count, -1);
  for (std::size_t point = 0; point < descriptor_count; ++point)
  {
    const float* descriptor = descriptors + point * dimensions;
    if (sumDescriptorRVV(descriptor, dimensions) < std::numeric_limits<float>::epsilon())
      continue;

    assignments[point] =
        static_cast<int>(nearestClusterDistanceRVV(descriptor, centers, clusters, dimensions).index);
  }
  return assignments;
}
#endif

inline std::vector<int>
assignDescriptorBatchCandidate(const float* descriptors,
                               const float* centers,
                               const std::size_t descriptor_count,
                               const std::size_t clusters,
                               const std::size_t dimensions)
{
#if defined(__RVV10__)
  return assignDescriptorBatchRVV(descriptors, centers, descriptor_count, clusters, dimensions);
#else
  return assignDescriptorBatchStd(descriptors, centers, descriptor_count, clusters, dimensions);
#endif
}

inline std::uint64_t
checksumDiagnostic(const DistanceResult distance_result,
                   const float sigma,
                   const float density)
{
  std::uint64_t value = 1469598103934665603ull;
  value = mixChecksum(value, static_cast<std::uint64_t>(distance_result.index));
  value = mixChecksum(value, checksumFloat(distance_result.distance));
  value = mixChecksum(value, checksumFloat(sigma));
  value = mixChecksum(value, checksumFloat(density));
  return value;
}

inline std::uint64_t
checksumAssignments(const std::vector<int>& assignments)
{
  std::uint64_t value = 1469598103934665603ull;
  for (const int assignment : assignments)
    value = mixChecksum(value, static_cast<std::uint64_t>(assignment + 1));
  return value;
}

struct PublicFindObjectsResult
{
  std::size_t votes;
  std::size_t class_of_interest;
  double strongest_peak_density;
  std::uint64_t peak_fingerprint;
};

inline std::uint64_t
checksumPublicFindObjects(const PublicFindObjectsResult result)
{
  std::uint64_t value = 1469598103934665603ull;
  value = mixChecksum(value, static_cast<std::uint64_t>(result.votes));
  value = mixChecksum(value, static_cast<std::uint64_t>(result.class_of_interest + 1));
  value = mixChecksum(value, static_cast<std::uint64_t>(result.strongest_peak_density * 1000000.0));
  value = mixChecksum(value, result.peak_fingerprint);
  return value;
}

} // namespace pcl::test::implicit_shape_model_rvv
