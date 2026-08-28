/*
 * 本文件做什么：
 * Phase 000 benchmark（性能测试）比较 public count/select 和 test-only
 * projection candidate（仅测试使用的投影候选）。输出保留每个 case 的
 * ms/iter、inlier count 和 checksum（校验和），供后续 board（板卡）
 * repeated summary 与 Evidence Doctor（证据体检）解析。
 *
 * QEMU（仿真器）运行本文件只能证明二进制可运行和日志形状，不作为性能结论。
 */

#include "sac_model_circle3d.h"

#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

struct Result
{
  double avg_ms_per_iter = 0.0;
  double checksum = 0.0;
  std::size_t count = 0;
};

template <typename Func>
static Result
runTimed (Func&& func, std::size_t iterations, std::size_t warmup_iterations);

static Eigen::VectorXf
circle3dCoefficients ()
{
  Eigen::VectorXf coeffs (7);
  coeffs << 0.25f, -0.10f, 0.30f, 1.20f, 0.20f, -0.30f, 0.9327379f;
  return coeffs;
}

static pcl::PointXYZ
makeCircle3DPoint (const Eigen::Vector3d& center,
                   const Eigen::Vector3d& normal,
                   double radius,
                   double angle,
                   double radial_offset,
                   double normal_offset)
{
  Eigen::Vector3d seed (1.0, 0.0, 0.0);
  if (std::abs (normal.dot (seed)) > 0.95)
    seed = Eigen::Vector3d (0.0, 1.0, 0.0);

  const Eigen::Vector3d axis_u = (seed - normal.dot (seed) * normal).normalized ();
  const Eigen::Vector3d axis_v = normal.cross (axis_u).normalized ();
  const Eigen::Vector3d in_plane =
      (radius + radial_offset) * (std::cos (angle) * axis_u + std::sin (angle) * axis_v);
  const Eigen::Vector3d p = center + in_plane + normal_offset * normal;

  pcl::PointXYZ point;
  point.x = static_cast<float> (p.x ());
  point.y = static_cast<float> (p.y ());
  point.z = static_cast<float> (p.z ());
  return point;
}

template <typename PointT>
static void
fillBenchExtraFields (PointT& point, std::size_t i)
{
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
  else
    (void)point;
}

template <typename PointT>
static typename pcl::PointCloud<PointT>::Ptr
makeBenchCloud (std::size_t size)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>> ();
  const Eigen::VectorXf coeffs = circle3dCoefficients ();
  const Eigen::Vector3d center (coeffs[0], coeffs[1], coeffs[2]);
  const Eigen::Vector3d normal =
      Eigen::Vector3d (coeffs[4], coeffs[5], coeffs[6]).normalized ();
  cloud->resize (size);
  for (std::size_t i = 0; i < size; ++i)
  {
    const double angle = static_cast<double> ((i * 37) % 6283) / 1000.0;
    const double radial_offset = (static_cast<int> (i % 17) - 8) * 0.018;
    const double normal_offset = (static_cast<int> ((i * 3) % 11) - 5) * 0.012;
    const pcl::PointXYZ xyz =
        makeCircle3DPoint (center, normal, 1.20, angle, radial_offset, normal_offset);
    (*cloud)[i].x = xyz.x;
    (*cloud)[i].y = xyz.y;
    (*cloud)[i].z = xyz.z;
    fillBenchExtraFields ((*cloud)[i], i);
  }
  return cloud;
}

static pcl::Indices
makeShuffledIndices (std::size_t size)
{
  pcl::Indices indices (size);
  for (std::size_t i = 0; i < size; ++i)
    indices[i] = static_cast<pcl::index_t> ((i * 1103515245u + 12345u) % size);
  return indices;
}

template <typename PointT>
static int
runBenchForPointType (const std::string& point_type,
                      std::size_t size,
                      std::size_t iterations,
                      std::size_t warmup_iterations)
{
  using Circle3DModelAccess =
      pcl_rvv_test::sac_model_circle3d::SampleConsensusModelCircle3DAccess<PointT>;
  constexpr double threshold = 0.12;
  const Eigen::VectorXf coeffs = circle3dCoefficients ();
  auto cloud = makeBenchCloud<PointT> (size);
  pcl::Indices indices = makeShuffledIndices (size);
  Circle3DModelAccess model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::cout << "case: circle3d projection count/select direct-indexed " << point_type << "\n";
  std::cout << "size: " << size << "\n";
  std::cout << "iterations: " << iterations << "\n";
  std::cout << "warmup_iterations: " << warmup_iterations << "\n";
  std::cout << "point_type: " << point_type << "\n";

  const Result public_count = runTimed (
      [&]() {
        Result r;
        r.count = model.countWithinDistance (coeffs, threshold);
        r.checksum = static_cast<double> (r.count);
        return r;
      },
      iterations,
      warmup_iterations);

  const Result candidate_count = runTimed (
      [&]() {
        Result r;
        r.count = model.countWithinDistanceProjectionCandidate (coeffs, threshold);
        r.checksum = static_cast<double> (r.count);
        return r;
      },
      iterations,
      warmup_iterations);

  const Result public_select = runTimed (
      [&]() {
        pcl::Indices inliers;
        model.selectWithinDistance (coeffs, threshold, inliers);
        Result r;
        r.count = inliers.size ();
        for (std::size_t i = 0; i < inliers.size (); ++i)
          r.checksum += static_cast<double> (inliers[i]) * 0.25 + model.error_sqr_dists_[i];
        return r;
      },
      iterations,
      warmup_iterations);

  const Result candidate_select = runTimed (
      [&]() {
        pcl::Indices inliers;
        model.selectWithinDistanceProjectionCandidate (coeffs, threshold, inliers);
        Result r;
        r.count = inliers.size ();
        for (std::size_t i = 0; i < inliers.size (); ++i)
          r.checksum += static_cast<double> (inliers[i]) * 0.25 + model.error_sqr_dists_[i];
        return r;
      },
      iterations,
      warmup_iterations);

  std::cout << "public countWithinDistance : " << public_count.avg_ms_per_iter
            << " ms/iter\n";
  std::cout << "candidate count projection : " << candidate_count.avg_ms_per_iter
            << " ms/iter\n";
  std::cout << "public selectWithinDistance : " << public_select.avg_ms_per_iter
            << " ms/iter\n";
  std::cout << "candidate select projection : " << candidate_select.avg_ms_per_iter
            << " ms/iter\n";
  std::cout << "Count public : " << public_count.count << "\n";
  std::cout << "Count candidate : " << candidate_count.count << "\n";
  std::cout << "Checksum public count : " << public_count.checksum << "\n";
  std::cout << "Checksum candidate count : " << candidate_count.checksum << "\n";
  std::cout << "Checksum public select : " << public_select.checksum << "\n";
  std::cout << "Checksum candidate select : " << candidate_select.checksum << "\n";
  return 0;
}

template <typename Func>
static Result
runTimed (Func&& func, std::size_t iterations, std::size_t warmup_iterations)
{
  Result result;
  for (std::size_t iter = 0; iter < warmup_iterations; ++iter)
    result = func ();

  const auto start = std::chrono::steady_clock::now ();
  for (std::size_t iter = 0; iter < iterations; ++iter)
    result = func ();
  const auto end = std::chrono::steady_clock::now ();
  result.avg_ms_per_iter =
      std::chrono::duration<double, std::milli> (end - start).count () /
      static_cast<double> (iterations);
  return result;
}

int
main (int argc, char** argv)
{
  const std::size_t size = argc > 1 ? static_cast<std::size_t> (std::stoul (argv[1])) : 65536;
  const std::size_t iterations = argc > 2 ? static_cast<std::size_t> (std::stoul (argv[2])) : 200;
  const std::size_t warmup_iterations =
      argc > 3 ? static_cast<std::size_t> (std::stoul (argv[3])) : 20;
  const std::string point_type = argc > 4 ? argv[4] : "PointXYZ";

  if (point_type == "PointXYZ")
    return runBenchForPointType<pcl::PointXYZ> (
        point_type, size, iterations, warmup_iterations);
  if (point_type == "PointXYZI")
    return runBenchForPointType<pcl::PointXYZI> (
        point_type, size, iterations, warmup_iterations);
  if (point_type == "PointXYZRGB")
    return runBenchForPointType<pcl::PointXYZRGB> (
        point_type, size, iterations, warmup_iterations);
  if (point_type == "PointXYZRGBA")
    return runBenchForPointType<pcl::PointXYZRGBA> (
        point_type, size, iterations, warmup_iterations);

  std::cerr << "Unsupported point type: " << point_type
            << " (expected PointXYZ, PointXYZI, PointXYZRGB or PointXYZRGBA)\n";
  return 2;
}
