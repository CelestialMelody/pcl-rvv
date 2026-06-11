#pragma once

#include <pcl/common/common.h>
#include <pcl/common/point_tests.h>
#include <pcl/common/rvv_point_load.h>
#include <pcl/filters/convolution_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_convolution_3d {

struct KernelResult {
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};
  float total_weight{0.0f};
  int accepted{0};
};

struct ErrorStats {
  float max_abs{0.0f};
  float max_rel{0.0f};
  double rmse{0.0};
};

inline pcl::PointCloud<pcl::PointXYZ>
makeCloud(int width, int height, int depth, bool with_invalid)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(width * height * depth);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(static_cast<std::size_t>(width * height * depth));

  std::size_t offset = 0;
  for (int z = 0; z < depth; ++z) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x, ++offset) {
        cloud[offset].x = static_cast<float>(x) * 0.035f;
        cloud[offset].y = static_cast<float>(y) * 0.037f;
        cloud[offset].z = static_cast<float>(z) * 0.041f +
                          static_cast<float>((x * 13 + y * 7 + z * 3) % 17) * 0.0007f;
      }
    }
  }

  if (with_invalid) {
    for (std::size_t i = 19; i < cloud.size(); i += 997)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t i = 131; i < cloud.size(); i += 1543)
      cloud[i].z = std::numeric_limits<float>::infinity();
  }
  return cloud;
}

inline pcl::Indices
makeIndices(std::size_t n, bool subset)
{
  pcl::Indices indices;
  indices.reserve(subset ? n / 2 : n);
  for (std::size_t i = subset ? 1 : 0; i < n; i += subset ? 2 : 1)
    indices.push_back(static_cast<int>(i));
  return indices;
}

inline std::uint64_t
mixFloat(std::uint64_t checksum, float value)
{
  std::uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  checksum ^= bits;
  checksum *= 1099511628211ull;
  return checksum;
}

inline std::uint64_t
checksumKernelResult(const KernelResult& result)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixFloat(checksum, result.x);
  checksum = mixFloat(checksum, result.y);
  checksum = mixFloat(checksum, result.z);
  checksum = mixFloat(checksum, result.total_weight);
  checksum ^= static_cast<std::uint64_t>(result.accepted);
  checksum *= 1099511628211ull;
  return checksum;
}

inline std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto& point : cloud) {
    checksum = mixFloat(checksum, point.x);
    checksum = mixFloat(checksum, point.y);
    checksum = mixFloat(checksum, point.z);
  }
  return checksum;
}

inline bool
isFinitePoint(const pcl::PointXYZ& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline KernelResult
gaussianKernelPointXYZStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const pcl::Indices& indices,
                          const std::vector<float>& distances,
                          float sigma_sqr,
                          float threshold)
{
  KernelResult result;
  const std::size_t n = std::min(indices.size(), distances.size());
  for (std::size_t i = 0; i < n; ++i) {
    const int id = indices[i];
    if (id < 0 || static_cast<std::size_t>(id) >= cloud.size())
      continue;
    const auto& point = cloud[static_cast<std::size_t>(id)];
    if (distances[i] <= threshold && isFinitePoint(point)) {
      const float weight = std::exp(-0.5f * distances[i] / sigma_sqr);
      result.x += weight * point.x;
      result.y += weight * point.y;
      result.z += weight * point.z;
      result.total_weight += weight;
      ++result.accepted;
    }
  }
  if (result.total_weight != 0.0f) {
    const float inv = 1.0f / result.total_weight;
    result.x *= inv;
    result.y *= inv;
    result.z *= inv;
  }
  else {
    result.x = result.y = result.z = std::numeric_limits<float>::quiet_NaN();
  }
  return result;
}

inline bool
gaussianKernelPointXYZRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const pcl::Indices& indices,
                          const std::vector<float>& distances,
                          float sigma_sqr,
                          float threshold,
                          KernelResult& result)
{
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
  if (indices.size() < 16 || indices.size() != distances.size() || sigma_sqr <= 0.0f)
    return false;

  result = {};
  const auto* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* raw_indices = reinterpret_cast<const std::uint32_t*>(indices.data());
  const float scale = -0.5f / sigma_sqr;

  std::size_t offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
    const vuint32m2_t v_point_offsets =
        pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_ids, vl);
    const vfloat32m2_t v_dist = __riscv_vle32_v_f32m2(distances.data() + offset, vl);
    const vbool16_t m_threshold = __riscv_vmfle_vf_f32m2_b16(v_dist, threshold, vl);

    // Convolution3D receives neighbor ids from radiusSearch, so the diagnostic
    // uses indexed AoS gathers for x/y/z.  Invalid lanes stay masked until the
    // scalar-order accumulation below, matching GaussianKernel's skip rule.
    const vfloat32m2_t vx =
        pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, x)>(
            base_u8, v_point_offsets, vl);
    const vfloat32m2_t vy =
        pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, y)>(
            base_u8, v_point_offsets, vl);
    const vfloat32m2_t vz =
        pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, z)>(
            base_u8, v_point_offsets, vl);
    const vbool16_t m_finite =
        __riscv_vmand_mm_b16(__riscv_vmfeq_vv_f32m2_b16(vx, vx, vl),
                             __riscv_vmand_mm_b16(__riscv_vmfeq_vv_f32m2_b16(vy, vy, vl),
                                                  __riscv_vmfeq_vv_f32m2_b16(vz, vz, vl),
                                                  vl),
                             vl);
    const vbool16_t mask = __riscv_vmand_mm_b16(m_threshold, m_finite, vl);
    const vfloat32m2_t v_weight =
        pcl::expf_RVV_f32m2(__riscv_vfmul_vf_f32m2(v_dist, scale, vl), vl);
    const vfloat32m2_t v_wx = __riscv_vfmul_vv_f32m2(v_weight, vx, vl);
    const vfloat32m2_t v_wy = __riscv_vfmul_vv_f32m2(v_weight, vy, vl);
    const vfloat32m2_t v_wz = __riscv_vfmul_vv_f32m2(v_weight, vz, vl);

    alignas(64) float weights[64];
    alignas(64) float wx[64];
    alignas(64) float wy[64];
    alignas(64) float wz[64];
    __riscv_vse32_v_f32m2(weights, v_weight, vl);
    __riscv_vse32_v_f32m2(wx, v_wx, vl);
    __riscv_vse32_v_f32m2(wy, v_wy, vl);
    __riscv_vse32_v_f32m2(wz, v_wz, vl);

    // Accumulate the masked lane data in radiusSearch order.  This keeps the
    // diagnostic focused on gather/weight vectorization and avoids introducing
    // a second semantic variable from vector reduction order.
    for (std::size_t lane = 0; lane < vl; ++lane) {
      (void)mask;
      const int id = indices[offset + lane];
      if (id < 0 || static_cast<std::size_t>(id) >= cloud.size() ||
          distances[offset + lane] > threshold ||
          !isFinitePoint(cloud[static_cast<std::size_t>(id)]))
        continue;
      result.x += wx[lane];
      result.y += wy[lane];
      result.z += wz[lane];
      result.total_weight += weights[lane];
      ++result.accepted;
    }
    offset += vl;
  }
  if (result.total_weight != 0.0f) {
    const float inv = 1.0f / result.total_weight;
    result.x *= inv;
    result.y *= inv;
    result.z *= inv;
  }
  else {
    result.x = result.y = result.z = std::numeric_limits<float>::quiet_NaN();
  }
  return true;
#else
  (void)cloud;
  (void)indices;
  (void)distances;
  (void)sigma_sqr;
  (void)threshold;
  (void)result;
  return false;
#endif
}

inline KernelResult
gaussianKernelPointXYZDispatch(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                               const pcl::Indices& indices,
                               const std::vector<float>& distances,
                               float sigma_sqr,
                               float threshold)
{
  KernelResult result;
  if (gaussianKernelPointXYZRVV(cloud, indices, distances, sigma_sqr, threshold, result))
    return result;
  return gaussianKernelPointXYZStd(cloud, indices, distances, sigma_sqr, threshold);
}

inline pcl::PointCloud<pcl::PointXYZ>
convolveDiagnosticStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      const pcl::Indices& query_indices,
                      double radius,
                      float sigma_sqr,
                      float threshold)
{
  pcl::search::KdTree<pcl::PointXYZ> tree;
  tree.setInputCloud(cloud.makeShared());
  pcl::PointCloud<pcl::PointXYZ> output;
  output.resize(query_indices.size());
  output.width = static_cast<std::uint32_t>(query_indices.size());
  output.height = 1;
  output.is_dense = cloud.is_dense;

  pcl::Indices nn_indices;
  std::vector<float> nn_distances;
  for (std::size_t out_i = 0; out_i < query_indices.size(); ++out_i) {
    const int point_idx = query_indices[out_i];
    if (point_idx >= 0 && static_cast<std::size_t>(point_idx) < cloud.size() &&
        isFinitePoint(cloud[static_cast<std::size_t>(point_idx)]) &&
        tree.radiusSearch(cloud[static_cast<std::size_t>(point_idx)], radius, nn_indices, nn_distances)) {
      const auto result =
          gaussianKernelPointXYZStd(cloud, nn_indices, nn_distances, sigma_sqr, threshold);
      output[out_i].x = result.x;
      output[out_i].y = result.y;
      output[out_i].z = result.z;
    }
    else {
      output[out_i].x = output[out_i].y = output[out_i].z =
          std::numeric_limits<float>::quiet_NaN();
      output.is_dense = false;
    }
  }
  return output;
}

inline pcl::PointCloud<pcl::PointXYZ>
convolveDiagnosticRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      const pcl::Indices& query_indices,
                      double radius,
                      float sigma_sqr,
                      float threshold)
{
  pcl::search::KdTree<pcl::PointXYZ> tree;
  tree.setInputCloud(cloud.makeShared());
  pcl::PointCloud<pcl::PointXYZ> output;
  output.resize(query_indices.size());
  output.width = static_cast<std::uint32_t>(query_indices.size());
  output.height = 1;
  output.is_dense = cloud.is_dense;

  pcl::Indices nn_indices;
  std::vector<float> nn_distances;
  for (std::size_t out_i = 0; out_i < query_indices.size(); ++out_i) {
    const int point_idx = query_indices[out_i];
    if (point_idx >= 0 && static_cast<std::size_t>(point_idx) < cloud.size() &&
        isFinitePoint(cloud[static_cast<std::size_t>(point_idx)]) &&
        tree.radiusSearch(cloud[static_cast<std::size_t>(point_idx)], radius, nn_indices, nn_distances)) {
      const auto result =
          gaussianKernelPointXYZDispatch(cloud, nn_indices, nn_distances, sigma_sqr, threshold);
      output[out_i].x = result.x;
      output[out_i].y = result.y;
      output[out_i].z = result.z;
    }
    else {
      output[out_i].x = output[out_i].y = output[out_i].z =
          std::numeric_limits<float>::quiet_NaN();
      output.is_dense = false;
    }
  }
  return output;
}

inline pcl::PointCloud<pcl::PointXYZ>
convolveProductionUnchanged(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                            double radius,
                            float sigma)
{
  pcl::filters::GaussianKernel<pcl::PointXYZ, pcl::PointXYZ> kernel;
  kernel.setSigma(sigma);
  kernel.setThreshold(std::numeric_limits<float>::infinity());
  pcl::filters::Convolution3D<pcl::PointXYZ,
                              pcl::PointXYZ,
                              pcl::filters::GaussianKernel<pcl::PointXYZ, pcl::PointXYZ>>
      convolution;
  convolution.setInputCloud(cloud.makeShared());
  convolution.setSearchSurface(cloud.makeShared());
  convolution.setRadiusSearch(radius);
  convolution.setKernel(kernel);
  convolution.setNumberOfThreads(1);
  pcl::PointCloud<pcl::PointXYZ> output;
  convolution.convolve(output);
  return output;
}

inline ErrorStats
compareCloud(const pcl::PointCloud<pcl::PointXYZ>& expected,
             const pcl::PointCloud<pcl::PointXYZ>& actual)
{
  ErrorStats stats;
  if (expected.size() != actual.size() || expected.empty())
    return stats;
  double sum_sq = 0.0;
  std::size_t count = 0;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const float expected_values[3] = {expected[i].x, expected[i].y, expected[i].z};
    const float actual_values[3] = {actual[i].x, actual[i].y, actual[i].z};
    for (int j = 0; j < 3; ++j) {
      if (!std::isfinite(expected_values[j]) && !std::isfinite(actual_values[j]))
        continue;
      const float abs_error = std::abs(expected_values[j] - actual_values[j]);
      const float denom = std::max(std::abs(expected_values[j]), 1.0f);
      stats.max_abs = std::max(stats.max_abs, abs_error);
      stats.max_rel = std::max(stats.max_rel, abs_error / denom);
      sum_sq += static_cast<double>(abs_error) * abs_error;
      ++count;
    }
  }
  stats.rmse = count == 0 ? 0.0 : std::sqrt(sum_sq / static_cast<double>(count));
  return stats;
}

} // namespace pcl_rvv_filters_convolution_3d
