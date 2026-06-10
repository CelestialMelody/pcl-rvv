#pragma once

#include <pcl/common/point_tests.h>
#include <pcl/common/common.h>
#include <pcl/common/rvv_point_load.h>
#include <pcl/filters/bilateral.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_bilateral {

struct NeighborFeature {
  float spatial_distance;
  float intensity_distance;
  float intensity;
};

struct IntensityErrorStats {
  float max_abs{0.0f};
  float max_rel{0.0f};
  double rmse{0.0};
  float p95_abs{0.0f};
  float p99_abs{0.0f};
};

inline double
kernel(double x, double sigma)
{
  return std::exp(-(x * x) / (2.0 * sigma * sigma));
}

inline double
accumulateWeightFromFeatures(const std::vector<NeighborFeature>& features,
                             double sigma_s,
                             double sigma_r)
{
  double bf = 0.0;
  double w = 0.0;
  for (const auto& feature : features) {
    const double weight =
        kernel(feature.spatial_distance, sigma_s) * kernel(feature.intensity_distance, sigma_r);
    bf += weight * feature.intensity;
    w += weight;
  }
  return bf / w;
}

inline std::vector<NeighborFeature>
stageNeighborFeaturesStd(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                         int pid,
                         const pcl::Indices& indices,
                         const std::vector<float>& squared_distances)
{
  std::vector<NeighborFeature> features;
  features.reserve(indices.size());
  const float center_intensity = cloud[pid].intensity;
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const int id = indices[i];
    features.push_back({std::sqrt(squared_distances[i]),
                        std::abs(center_intensity - cloud[id].intensity),
                        cloud[id].intensity});
  }
  return features;
}

inline double
computePointWeightStd(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                      int pid,
                      const pcl::Indices& indices,
                      const std::vector<float>& squared_distances,
                      double sigma_s,
                      double sigma_r)
{
  return accumulateWeightFromFeatures(
      stageNeighborFeaturesStd(cloud, pid, indices, squared_distances), sigma_s, sigma_r);
}

inline bool
stageNeighborFeaturesRVV(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                         int pid,
                         const pcl::Indices& indices,
                         const std::vector<float>& squared_distances,
                         std::vector<NeighborFeature>& features)
{
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
  if (indices.size() < 16 || indices.size() != squared_distances.size())
    return false;

  features.resize(indices.size());
  const auto* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* raw_indices = reinterpret_cast<const std::uint32_t*>(indices.data());
  const float center_intensity = cloud[pid].intensity;

  std::size_t offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    // Bench-only staging: public point wrappers provide the intensity gather;
    // the neighbor id vector itself is a plain contiguous int array from radiusSearch.
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
    const vuint32m2_t v_point_offsets =
        pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZI>(v_ids, vl);
    const vfloat32m2_t v_intensity =
        pcl::rvv_load::gather_load_f32m2<pcl::PointXYZI, offsetof(pcl::PointXYZI, intensity)>(
            base_u8, v_point_offsets, vl);
    const vfloat32m2_t v_center = __riscv_vfmv_v_f_f32m2(center_intensity, vl);
    const vfloat32m2_t v_abs_delta =
        __riscv_vfabs_v_f32m2(__riscv_vfsub_vv_f32m2(v_center, v_intensity, vl), vl);
    // Scalar code does sqrt(distance) before kernel() squares it again.  The
    // RVV diagnostic preserves that staging boundary before the scalar exp/sum.
    const vfloat32m2_t v_squared = __riscv_vle32_v_f32m2(squared_distances.data() + offset, vl);
    const vfloat32m2_t v_spatial = __riscv_vfsqrt_v_f32m2(v_squared, vl);

    alignas(64) float spatial[64];
    alignas(64) float intensity_delta[64];
    alignas(64) float intensity[64];
    __riscv_vse32_v_f32m2(spatial, v_spatial, vl);
    __riscv_vse32_v_f32m2(intensity_delta, v_abs_delta, vl);
    __riscv_vse32_v_f32m2(intensity, v_intensity, vl);

    for (std::size_t lane = 0; lane < vl; ++lane)
      features[offset + lane] = {spatial[lane], intensity_delta[lane], intensity[lane]};
    offset += vl;
  }
  return true;
#else
  (void)cloud;
  (void)pid;
  (void)indices;
  (void)squared_distances;
  (void)features;
  return false;
#endif
}

inline double
computePointWeightRVV(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                      int pid,
                      const pcl::Indices& indices,
                      const std::vector<float>& squared_distances,
                      double sigma_s,
                      double sigma_r)
{
  std::vector<NeighborFeature> features;
  if (!stageNeighborFeaturesRVV(cloud, pid, indices, squared_distances, features))
    return computePointWeightStd(cloud, pid, indices, squared_distances, sigma_s, sigma_r);
  // exp() and double accumulation stay scalar and in radiusSearch order; this
  // isolates whether RVV gather/sqrt staging matters inside the true data flow.
  return accumulateWeightFromFeatures(features, sigma_s, sigma_r);
}

inline double
computePointWeightExpRVV(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                         int pid,
                         const pcl::Indices& indices,
                         const std::vector<float>& squared_distances,
                         double sigma_s,
                         double sigma_r)
{
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
  if (indices.size() < 16 || indices.size() != squared_distances.size())
    return computePointWeightStd(cloud, pid, indices, squared_distances, sigma_s, sigma_r);

  const auto* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* raw_indices = reinterpret_cast<const std::uint32_t*>(indices.data());
  const float center_intensity = cloud[pid].intensity;
  const float spatial_scale = static_cast<float>(-1.0 / (2.0 * sigma_s * sigma_s));
  const float intensity_scale = static_cast<float>(-1.0 / (2.0 * sigma_r * sigma_r));
  double bf = 0.0;
  double w = 0.0;

  std::size_t offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
    const vuint32m2_t v_point_offsets =
        pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZI>(v_ids, vl);
    const vfloat32m2_t v_intensity =
        pcl::rvv_load::gather_load_f32m2<pcl::PointXYZI, offsetof(pcl::PointXYZI, intensity)>(
            base_u8, v_point_offsets, vl);
    const vfloat32m2_t v_squared = __riscv_vle32_v_f32m2(squared_distances.data() + offset, vl);
    const vfloat32m2_t v_center = __riscv_vfmv_v_f_f32m2(center_intensity, vl);
    const vfloat32m2_t v_delta =
        __riscv_vfsub_vv_f32m2(v_center, v_intensity, vl);
    const vfloat32m2_t v_spatial_arg =
        __riscv_vfmul_vf_f32m2(v_squared, spatial_scale, vl);
    const vfloat32m2_t v_delta2 =
        __riscv_vfmul_vv_f32m2(v_delta, v_delta, vl);
    const vfloat32m2_t v_intensity_arg =
        __riscv_vfmul_vf_f32m2(v_delta2, intensity_scale, vl);
    // Use the common RVV math helper instead of local polynomial code.  This
    // diagnoses the realistic option for replacing the two scalar exp calls.
    const vfloat32m2_t v_weight =
        __riscv_vfmul_vv_f32m2(pcl::expf_RVV_f32m2(v_spatial_arg, vl),
                               pcl::expf_RVV_f32m2(v_intensity_arg, vl),
                               vl);
    const vfloat32m2_t v_contrib =
        __riscv_vfmul_vv_f32m2(v_weight, v_intensity, vl);

    alignas(64) float weights[64];
    alignas(64) float contribs[64];
    __riscv_vse32_v_f32m2(weights, v_weight, vl);
    __riscv_vse32_v_f32m2(contribs, v_contrib, vl);
    // Accumulate in neighbor order to isolate exp approximation and vector
    // staging effects from a separate vector reduction order change.
    for (std::size_t lane = 0; lane < vl; ++lane) {
      bf += static_cast<double>(contribs[lane]);
      w += static_cast<double>(weights[lane]);
    }
    offset += vl;
  }
  return bf / w;
#else
  return computePointWeightStd(cloud, pid, indices, squared_distances, sigma_s, sigma_r);
#endif
}

inline std::uint64_t
checksumCloudIntensity(const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto& point : cloud) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(point.intensity));
    std::memcpy(&bits, &point.intensity, sizeof(bits));
    checksum ^= bits;
    checksum *= 1099511628211ull;
  }
  return checksum;
}

inline IntensityErrorStats
compareCloudIntensity(const pcl::PointCloud<pcl::PointXYZI>& expected,
                      const pcl::PointCloud<pcl::PointXYZI>& actual)
{
  IntensityErrorStats stats;
  if (expected.empty() || expected.size() != actual.size())
    return stats;

  std::vector<float> abs_errors;
  abs_errors.reserve(expected.size());
  double sum_sq = 0.0;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const float abs_error = std::abs(expected[i].intensity - actual[i].intensity);
    const float denom = std::max(std::abs(expected[i].intensity), 1.0f);
    const float rel_error = abs_error / denom;
    stats.max_abs = std::max(stats.max_abs, abs_error);
    stats.max_rel = std::max(stats.max_rel, rel_error);
    sum_sq += static_cast<double>(abs_error) * abs_error;
    abs_errors.push_back(abs_error);
  }
  std::sort(abs_errors.begin(), abs_errors.end());
  const auto percentile = [&](double p) {
    const std::size_t pos = static_cast<std::size_t>(
        std::min<double>(abs_errors.size() - 1, std::ceil(p * abs_errors.size()) - 1.0));
    return abs_errors[pos];
  };
  stats.rmse = std::sqrt(sum_sq / static_cast<double>(expected.size()));
  stats.p95_abs = percentile(0.95);
  stats.p99_abs = percentile(0.99);
  return stats;
}

inline bool
errorWithinTolerance(const IntensityErrorStats& stats,
                     float max_abs,
                     float max_rel,
                     double rmse)
{
  return stats.max_abs <= max_abs && stats.max_rel <= max_rel && stats.rmse <= rmse;
}

inline pcl::PointCloud<pcl::PointXYZI>
makeCloud(std::size_t width, std::size_t height, bool with_invalid)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = !with_invalid;
  cloud.points.resize(width * height);
  for (std::size_t r = 0; r < height; ++r) {
    for (std::size_t c = 0; c < width; ++c) {
      const std::size_t i = r * width + c;
      cloud[i].x = static_cast<float>(c) * 0.015f;
      cloud[i].y = static_cast<float>(r) * 0.015f;
      cloud[i].z = static_cast<float>((c * 13 + r * 7) % 17) * 0.002f;
      cloud[i].intensity = 20.0f + static_cast<float>((c * 5 + r * 11) % 251) * 0.125f;
    }
  }
  if (with_invalid) {
    for (std::size_t i = 17; i < cloud.size(); i += 997)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZI>
makeHighContrastCloud(std::size_t width, std::size_t height)
{
  auto cloud = makeCloud(width, height, false);
  for (std::size_t r = 0; r < height; ++r) {
    for (std::size_t c = 0; c < width; ++c) {
      const std::size_t i = r * width + c;
      const float checker = ((r / 4 + c / 4) % 2 == 0) ? 35.0f : 205.0f;
      cloud[i].intensity = checker + static_cast<float>((c * 17 + r * 29) % 31) * 0.25f;
    }
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

inline pcl::PointCloud<pcl::PointXYZI>
filterStd(const pcl::PointCloud<pcl::PointXYZI>& cloud,
          const pcl::Indices& indices,
          double sigma_s,
          double sigma_r)
{
  auto output = cloud;
  auto cloud_ptr = cloud.makeShared();
  pcl::search::KdTree<pcl::PointXYZI> tree;
  tree.setInputCloud(cloud_ptr);
  pcl::Indices k_indices;
  std::vector<float> k_distances;
  for (const int idx : indices) {
    if (cloud.is_dense || pcl::isXYZFinite(cloud[idx])) {
      tree.radiusSearch(idx, sigma_s * 2.0, k_indices, k_distances);
      output[idx].intensity = static_cast<float>(
          computePointWeightStd(cloud, idx, k_indices, k_distances, sigma_s, sigma_r));
    }
  }
  return output;
}

inline pcl::PointCloud<pcl::PointXYZI>
filterRVV(const pcl::PointCloud<pcl::PointXYZI>& cloud,
          const pcl::Indices& indices,
          double sigma_s,
          double sigma_r)
{
  auto output = cloud;
  auto cloud_ptr = cloud.makeShared();
  pcl::search::KdTree<pcl::PointXYZI> tree;
  tree.setInputCloud(cloud_ptr);
  pcl::Indices k_indices;
  std::vector<float> k_distances;
  for (const int idx : indices) {
    if (cloud.is_dense || pcl::isXYZFinite(cloud[idx])) {
      tree.radiusSearch(idx, sigma_s * 2.0, k_indices, k_distances);
      output[idx].intensity = static_cast<float>(
          computePointWeightRVV(cloud, idx, k_indices, k_distances, sigma_s, sigma_r));
    }
  }
  return output;
}

inline pcl::PointCloud<pcl::PointXYZI>
filterExpRVV(const pcl::PointCloud<pcl::PointXYZI>& cloud,
             const pcl::Indices& indices,
             double sigma_s,
             double sigma_r)
{
  auto output = cloud;
  auto cloud_ptr = cloud.makeShared();
  pcl::search::KdTree<pcl::PointXYZI> tree;
  tree.setInputCloud(cloud_ptr);
  pcl::Indices k_indices;
  std::vector<float> k_distances;
  for (const int idx : indices) {
    if (cloud.is_dense || pcl::isXYZFinite(cloud[idx])) {
      tree.radiusSearch(idx, sigma_s * 2.0, k_indices, k_distances);
      output[idx].intensity = static_cast<float>(
          computePointWeightExpRVV(cloud, idx, k_indices, k_distances, sigma_s, sigma_r));
    }
  }
  return output;
}

inline pcl::PointCloud<pcl::PointXYZI>
filterProduction(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                 double sigma_s,
                 double sigma_r)
{
  pcl::BilateralFilter<pcl::PointXYZI> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setHalfSize(sigma_s);
  filter.setStdDev(sigma_r);
  pcl::PointCloud<pcl::PointXYZI> output;
  filter.filter(output);
  return output;
}

} // namespace pcl_rvv_filters_bilateral
