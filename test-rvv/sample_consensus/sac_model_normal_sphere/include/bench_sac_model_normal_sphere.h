/*
 * sac_model_normal_sphere bench 的聚合入口。
 * 这里保存 CLI 之外的 bench harness（性能测试驱动）和点型选择逻辑；
 * src/bench_sac_model_normal_sphere.cpp 只负责解析参数和调用对应点型。
 */

#pragma once

#include <test_sac_model_normal_sphere.h>

#include <pcl/common/utils.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace pcl_rvv_normal_sphere_test_support
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
typename pcl::PointCloud<PointT>::Ptr
makeBenchCloud (const std::size_t nr_points)
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (nr_points);
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 8192) / 8192.0f;
    (*cloud)[i].x = 1.0f + 2.0f * std::sin (t * 17.0f);
    (*cloud)[i].y = -2.0f + 1.2f * std::cos (t * 19.0f);
    (*cloud)[i].z = 0.5f + 0.8f * std::sin (t * 23.0f);
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> ((i * 17) % 1024) / 1024.0f;
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB>)
    {
      (*cloud)[i].r = static_cast<std::uint8_t> ((i * 3) % 255);
      (*cloud)[i].g = static_cast<std::uint8_t> ((i * 5) % 255);
      (*cloud)[i].b = static_cast<std::uint8_t> ((i * 7) % 255);
    }
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
    {
      (*cloud)[i].r = static_cast<std::uint8_t> ((i * 3) % 255);
      (*cloud)[i].g = static_cast<std::uint8_t> ((i * 5) % 255);
      (*cloud)[i].b = static_cast<std::uint8_t> ((i * 7) % 255);
      (*cloud)[i].a = 255;
    }
  }
  return cloud;
}

template <typename PointT>
int
runBenchForPointType (const char* const point_type_label,
                      const std::size_t nr_points,
                      const int iterations)
{
  constexpr int warmup = 5;

  auto cloud = makeBenchCloud<PointT> (nr_points);
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);

  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);

  const Eigen::VectorXf coeffs = normalSphereCoefficients ();
  constexpr double threshold = 0.22;

  SampleConsensusModelNormalSphereAccess<PointT, pcl::Normal> model (cloud, true);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.35);

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

  const Result vcompress_select = runTimed ([&]() {
    model.selectWithinDistanceVCompressCandidate (coeffs, threshold, inliers);
    return inliers.size ();
  }, iterations, warmup);

  const Result candidate_count = runTimed ([&]() {
    return model.countWithinDistanceCandidate (coeffs, threshold);
  }, iterations, warmup);

  const Result candidate_distances = runTimed ([&]() {
    model.getDistancesToModelCandidate (coeffs, distances);
    return distances.size ();
  }, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_normal_sphere " << point_type_label
            << " direct indexed normal-sphere cloud (points="
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
                candidate_select.checksum ^ vcompress_select.checksum ^
                candidate_count.checksum ^ candidate_distances.checksum)
            << "\n";
  std::cout << std::fixed << std::setprecision (6);
  std::cout << "public selectWithinDistance : " << public_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public countWithinDistance : " << public_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public getDistancesToModel : " << public_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate selectWithinDistance : " << candidate_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate vcompress selectWithinDistance : " << vcompress_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate countWithinDistance : " << candidate_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate getDistancesToModel : " << candidate_distances.avg_ms_per_iter << " ms/iter\n";

  pcl::utils::ignore (inliers, distances);
  return 0;
}

}  // namespace pcl_rvv_normal_sphere_test_support
