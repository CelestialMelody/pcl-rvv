/*
 * sac_model_sphere bench 的聚合入口。
 * 这里保存 CLI 之外的 bench harness（性能测试驱动）和点型选择逻辑；
 * src/bench_sac_model_sphere.cpp 只负责解析参数和调用对应点型。
 */

#pragma once

#include <impl/sac_model_sphere_access.hpp>

#include <pcl/common/utils.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

namespace pcl_rvv_sphere_test_support
{

struct Result
{
  double avg_ms_per_iter = 0.0;
  std::size_t checksum = 0;
};

inline Result
runTimed (const std::function<std::size_t()>& func, const int iterations, const int warmup)
{
  std::size_t checksum = 0;
  for (int i = 0; i < warmup; ++i)
    checksum ^= func () + static_cast<std::size_t> (i);

  const auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < iterations; ++i)
    checksum ^= func () + static_cast<std::size_t> (i + 17);
  const auto end = std::chrono::high_resolution_clock::now ();

  return {std::chrono::duration<double, std::milli> (end - start).count () / iterations,
          checksum};
}

template <typename PointT>
void
fillPoint (PointT& point, const std::size_t i)
{
  const float t = static_cast<float> (i % 8192) / 8192.0f;
  point.x = 1.0f + 2.0f * std::sin (t * 17.0f);
  point.y = -2.0f + 1.2f * std::cos (t * 19.0f);
  point.z = 0.5f + 0.8f * std::sin (t * 23.0f);
  if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
    point.intensity = static_cast<float> ((i * 17) % 1024) / 1024.0f;
  else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB>)
  {
    point.r = static_cast<std::uint8_t> ((i * 3) % 255);
    point.g = static_cast<std::uint8_t> ((i * 5) % 255);
    point.b = static_cast<std::uint8_t> ((i * 7) % 255);
  }
  else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
  {
    point.r = static_cast<std::uint8_t> ((i * 3) % 255);
    point.g = static_cast<std::uint8_t> ((i * 5) % 255);
    point.b = static_cast<std::uint8_t> ((i * 7) % 255);
    point.a = 255;
  }
}

template <typename PointT>
int
runBenchForPointType (const char* const point_type_label,
                      const std::size_t nr_points,
                      const int iterations)
{
  constexpr int warmup = 5;

  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (nr_points);
  for (std::size_t i = 0; i < cloud->size (); ++i)
    fillPoint ((*cloud)[i], i);

  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);

  Eigen::VectorXf coeffs (4);
  coeffs << 1.0f, -2.0f, 0.5f, 1.35f;
  constexpr double threshold = 0.18;

  SampleConsensusModelSphereAccess<PointT> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices inliers;
  std::vector<double> distances;

  const Result public_select = runTimed ([&]() {
    model.selectWithinDistance (coeffs, threshold, inliers);
    return inliers.size ();
  }, iterations, warmup);

  const Result public_count = runTimed ([&]() {
    return model.countWithinDistance (coeffs, threshold);
  }, iterations, warmup);

  const Result public_distances = runTimed ([&]() {
    model.getDistancesToModel (coeffs, distances);
    return distances.size ();
  }, iterations, warmup);

  const Result candidate_select = runTimed ([&]() {
    model.selectWithinDistanceCandidate (coeffs, threshold, inliers);
    return inliers.size ();
  }, iterations, warmup);

  const Result vcompress_candidate_select = runTimed ([&]() {
#if defined (__RVV10__)
    model.selectWithinDistanceVCompressCandidate (coeffs, threshold, inliers);
#else
    model.selectWithinDistanceCandidate (coeffs, threshold, inliers);
#endif
    return inliers.size ();
  }, iterations, warmup);

  const Result candidate_distances = runTimed ([&]() {
    model.getDistancesToModelCandidate (coeffs, distances);
    return distances.size ();
  }, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_sphere " << point_type_label
            << " direct indexed shell cloud (points="
            << nr_points << ", shuffled adjacent pairs)\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup << "\n";
  std::cout << "Build: "
#if defined (__RVV10__)
            << "RVV"
#else
            << "Std"
#endif
            << "\n";
  std::cout << "Checksum: "
            << (public_select.checksum ^ public_count.checksum ^ public_distances.checksum ^
                candidate_select.checksum ^ vcompress_candidate_select.checksum ^
                candidate_distances.checksum)
            << "\n";
  std::cout << std::fixed << std::setprecision (6);
  std::cout << "public selectWithinDistance : " << public_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public countWithinDistance : " << public_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public getDistancesToModel : " << public_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate selectWithinDistance : " << candidate_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "vcompress candidate selectWithinDistance : " << vcompress_candidate_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate getDistancesToModel : " << candidate_distances.avg_ms_per_iter << " ms/iter\n";

  pcl::utils::ignore (inliers, distances);
  return 0;
}

}  // namespace pcl_rvv_sphere_test_support
