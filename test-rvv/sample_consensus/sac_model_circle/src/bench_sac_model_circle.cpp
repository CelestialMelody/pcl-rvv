/*
 * 本文件做什么：
 * 这个 bench（性能测试）直接调用 SampleConsensusModelCircle2D 的公开距离入口。
 * 计时边界只包含入口调用本身，不包含点云、indices（索引）和模型系数构造。
 * QEMU 只能把本文件作为 build 或日志形状 smoke（小型验证）使用；真实性能结论
 * 必须来自板卡或目标硬件。
 */

#include "sac_model_circle_test_support.h"

#include <pcl/common/utils.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using PointT = pcl::PointXYZ;
using SampleConsensusModelCircleBench =
    pcl_rvv_test::sac_model_circle::SampleConsensusModelCircleAccess<PointT>;

struct Result
{
  double avg_ms_per_iter = 0.0;
  std::size_t checksum = 0;
};

static std::size_t
hashDoubleVector (const std::vector<double>& values)
{
  std::size_t hash = values.size ();
  for (const double value : values)
  {
    const long long scaled = std::llround (value * 1000000.0);
    hash ^= static_cast<std::size_t> (scaled) + 0x9e3779b97f4a7c15ULL +
            (hash << 6) + (hash >> 2);
  }
  return hash;
}

static std::size_t
hashIndicesAndErrors (const pcl::Indices& indices, const std::vector<double>& errors)
{
  std::size_t hash = indices.size ();
  for (const pcl::index_t index : indices)
  {
    hash ^= static_cast<std::size_t> (index) + 0x9e3779b97f4a7c15ULL +
            (hash << 6) + (hash >> 2);
  }
  return hash ^ (hashDoubleVector (errors) << 1);
}

static Result
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

int
main (int argc, char** argv)
{
  const std::size_t nr_points =
      (argc >= 2) ? std::max<std::size_t> (1, std::strtoull (argv[1], nullptr, 10)) : 65536;
  const int iterations = (argc >= 3) ? std::max (1, std::atoi (argv[2])) : 200;
  const std::string index_mode = (argc >= 4) ? argv[3] : "shuffled";
  if (index_mode != "shuffled" && index_mode != "identity")
  {
    std::cerr << "Usage: " << argv[0] << " [points] [iterations] [identity|shuffled]\n";
    return 2;
  }
  constexpr int warmup = 5;

  pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (nr_points);
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 8192) / 8192.0f;
    const float radial_jitter = 0.16f * std::sin (t * 41.0f);
    const float radius = 1.00f + radial_jitter;
    (*cloud)[i].x = 0.25f + radius * std::cos (t * 19.0f);
    (*cloud)[i].y = -0.20f + radius * std::sin (t * 19.0f);
    (*cloud)[i].z = 0.5f * std::sin (t * 7.0f);
  }

  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  if (index_mode == "shuffled")
  {
    for (std::size_t i = 1; i < indices.size (); i += 4)
      std::swap (indices[i - 1], indices[i]);
  }

  Eigen::VectorXf coeffs (3);
  coeffs << 0.25f, -0.20f, 1.00f;
  constexpr double threshold = 0.08;

  SampleConsensusModelCircleBench model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices inliers;
  std::vector<double> distances;

  const Result public_select = runTimed ([&]() {
    model.selectWithinDistance (coeffs, threshold, inliers);
    return hashIndicesAndErrors (inliers, model.error_sqr_dists_);
  }, iterations, warmup);

  const Result full_rvv_select_error_tail = runTimed ([&]() {
    model.selectWithinDistanceFullRVVErrorTailCandidate (coeffs, threshold, inliers);
    return hashIndicesAndErrors (inliers, model.error_sqr_dists_);
  }, iterations, warmup);

  const Result public_count = runTimed ([&]() {
    return model.countWithinDistance (coeffs, threshold);
  }, iterations, warmup);

  const Result public_distances = runTimed ([&]() {
    model.getDistancesToModel (coeffs, distances);
    return hashDoubleVector (distances);
  }, iterations, warmup);

  const Result diagnostic_distances = runTimed ([&]() {
    model.getDistancesToModelScalarSqrtCandidate (coeffs, distances);
    return hashDoubleVector (distances);
  }, iterations, warmup);

  const Result full_rvv_distances = runTimed ([&]() {
    model.getDistancesToModelFullRVVCandidate (coeffs, distances);
    return hashDoubleVector (distances);
  }, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_circle direct indexed shell cloud (points="
            << nr_points << ", "
            << (index_mode == "identity" ? "identity indices" : "shuffled adjacent pairs")
            << ")\n";
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
            << (public_select.checksum ^ public_count.checksum ^
                public_distances.checksum ^ diagnostic_distances.checksum ^
                full_rvv_distances.checksum ^
                full_rvv_select_error_tail.checksum)
            << "\n";
  std::cout << "Checksum public selectWithinDistance : " << public_select.checksum << "\n";
  std::cout << "Checksum diagnostic select full-rvv error tail : "
            << full_rvv_select_error_tail.checksum << "\n";
  std::cout << "Checksum public countWithinDistance : " << public_count.checksum << "\n";
  std::cout << "Checksum public getDistancesToModel : " << public_distances.checksum << "\n";
  std::cout << "Checksum diagnostic candidate getDistancesToModel : "
            << diagnostic_distances.checksum << "\n";
  std::cout << "Checksum diagnostic full-rvv getDistancesToModel : "
            << full_rvv_distances.checksum << "\n";
  std::cout << std::fixed << std::setprecision (6);
  std::cout << "public selectWithinDistance : " << public_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic select full-rvv error tail : "
            << full_rvv_select_error_tail.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public countWithinDistance : " << public_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public getDistancesToModel : " << public_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate getDistancesToModel : "
            << diagnostic_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic full-rvv getDistancesToModel : "
            << full_rvv_distances.avg_ms_per_iter << " ms/iter\n";

  pcl::utils::ignore (inliers, distances);
  return 0;
}
