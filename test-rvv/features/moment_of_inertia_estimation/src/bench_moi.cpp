/*
 * 本文件做什么：
 * 这个 bench（性能测试）同时覆盖 MomentOfInertiaEstimation 逐点 diagnostic
 * helper 和 public compute（公开 compute 入口）的同边界 Std/RVV 成本。
 * QEMU 只用于 correctness（正确性）和日志形状；性能结论必须来自板卡。
 */

#include "moi.h"

#include <Eigen/Geometry>

#include <pcl/features/moment_of_inertia_estimation.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <string>
#include <vector>

namespace moi = pcl::features::rvv_test::moi;

namespace {

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

struct Args {
  std::size_t points = 262144;
  int iterations = 30;
  int warmup = 4;
  std::string case_filter = "all";
};

struct BenchResult {
  double avg_us = 0.0;
  std::uint64_t checksum = 0;
};

Args
parseArgs(int argc, char** argv)
{
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "--points" && i + 1 < argc)
      args.points = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
    else if (key == "--iterations" && i + 1 < argc)
      args.iterations = std::atoi(argv[++i]);
    else if (key == "--warmup-iterations" && i + 1 < argc)
      args.warmup = std::atoi(argv[++i]);
    else if (key == "--case-filter" && i + 1 < argc)
      args.case_filter = argv[++i];
  }
  return args;
}

std::vector<pcl::PointXYZ>
makeCloud(const std::size_t count)
{
  std::vector<pcl::PointXYZ> cloud(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float f = static_cast<float>(i);
    cloud[i].x = 0.03125f * f - 13.0f;
    cloud[i].y = 0.125f * static_cast<float>((i * 11) % 67) - 4.0f;
    cloud[i].z = 2.0f + 0.0005f * f * f - 0.25f * static_cast<float>(i % 9);
  }
  return cloud;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeTypedPointCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  const std::vector<pcl::PointXYZ> points = makeCloud(count);
  cloud->points.resize(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    cloud->points[i].x = points[i].x;
    cloud->points[i].y = points[i].y;
    cloud->points[i].z = points[i].z;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      cloud->points[i].intensity = 0.25f + static_cast<float>(i % 23);
  }
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  cloud->height = 1;
  return cloud;
}

std::vector<std::uint32_t>
makeIndices(const std::size_t points)
{
  std::vector<std::uint32_t> indices;
  indices.reserve(points);
  for (std::size_t i = 0; i < points; ++i)
    indices.push_back(static_cast<std::uint32_t>((i * 37 + 11) % points));
  return indices;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePointCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const std::vector<pcl::PointXYZ> points = makeCloud(count);
  cloud->points.assign(points.begin(), points.end());
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  cloud->height = 1;
  return cloud;
}

pcl::IndicesPtr
makePclIndices(const std::size_t points)
{
  auto indices = pcl::make_shared<pcl::Indices>();
  indices->reserve(points);
  for (const std::uint32_t index : makeIndices(points))
    indices->push_back(static_cast<int>(index));
  return indices;
}

std::uint64_t
checksum(const moi::ReductionSummary& summary)
{
  std::uint64_t seed = 1469598103934665603ull;
  auto mix = [&seed](float value) {
    const auto q = static_cast<std::int64_t>(value * 100000.0f);
    seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  };
  for (float value : summary.mean)
    mix(value);
  for (float value : summary.aabb_min)
    mix(value);
  for (float value : summary.aabb_max)
    mix(value);
  for (float value : summary.covariance)
    mix(value);
  mix(summary.moment);
  for (float value : summary.obb_min)
    mix(value);
  for (float value : summary.obb_max)
    mix(value);
  return seed;
}

std::uint64_t
checksum(const moi::Covariance6& covariance)
{
  std::uint64_t seed = 1099511628211ull;
  for (const float value : covariance) {
    const auto q = static_cast<std::int64_t>(value * 100000.0f);
    seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  }
  return seed;
}

template <typename PointT>
std::uint64_t
checksum(const pcl::MomentOfInertiaEstimation<PointT>& estimator)
{
  std::uint64_t seed = 1469598103934665603ull;
  auto mix = [&seed](float value) {
    const auto q = static_cast<std::int64_t>(value * 100000.0f);
    seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  };

  Eigen::Vector3f mass_center;
  PointT aabb_min;
  PointT aabb_max;
  std::vector<float> moments;
  std::vector<float> eccentricities;
  estimator.getMassCenter(mass_center);
  estimator.getAABB(aabb_min, aabb_max);
  estimator.getMomentOfInertia(moments);
  estimator.getEccentricity(eccentricities);

  mix(mass_center.x());
  mix(mass_center.y());
  mix(mass_center.z());
  mix(aabb_min.x);
  mix(aabb_min.y);
  mix(aabb_min.z);
  mix(aabb_max.x);
  mix(aabb_max.y);
  mix(aabb_max.z);
  for (float value : moments)
    mix(value);
  for (float value : eccentricities)
    mix(value);
  return seed;
}

const char*
pointTypeLabel(const std::string& case_filter)
{
  if (case_filter == "moi_public_compute_pointxyzi")
    return "PointXYZI";
  if (case_filter == "moi_public_compute_pointxyzrgb")
    return "PointXYZRGB";
  if (case_filter == "moi_public_compute_pointxyzrgba")
    return "PointXYZRGBA";
  if (case_filter == "moi_public_compute_pointxyzrgbnormal")
    return "PointXYZRGBNormal";
  return "PointXYZ";
}

moi::ReductionSummary
computeSummary(const std::vector<pcl::PointXYZ>& cloud,
               const std::vector<std::uint32_t>& indices,
               const Eigen::Vector3f& inertia_axis,
               const Eigen::Vector3f& major_axis,
               const Eigen::Vector3f& middle_axis,
               const Eigen::Vector3f& minor_axis)
{
  constexpr float point_mass = 1.0f / 1048576.0f;
#if defined(__RVV10__)
  return moi::computeReductionSummaryRVV(
      cloud.data(), indices.data(), indices.size(), inertia_axis, major_axis, middle_axis, minor_axis, point_mass);
#else
  return moi::computeReductionSummaryStd(
      cloud.data(), indices.data(), indices.size(), inertia_axis, major_axis, middle_axis, minor_axis, point_mass);
#endif
}

moi::Covariance6
computeProjectedCovariance(const std::vector<pcl::PointXYZ>& cloud,
                           const std::vector<std::uint32_t>& indices,
                           const Eigen::Vector3f& mean,
                           const Eigen::Vector3f& normal)
{
#if defined(__RVV10__)
  return moi::computeProjectedCovarianceRVV(cloud.data(), indices.data(), indices.size(), mean, normal);
#else
  return moi::computeProjectedCovarianceStd(cloud.data(), indices.data(), indices.size(), mean, normal);
#endif
}

BenchResult
measureReductions(const Args& args)
{
  const std::vector<pcl::PointXYZ> cloud = makeCloud(args.points);
  const std::vector<std::uint32_t> indices = makeIndices(args.points);
  const Eigen::Vector3f inertia_axis = Eigen::Vector3f(0.2f, 0.7f, -0.4f).normalized();
  const Eigen::Vector3f major_axis = Eigen::Vector3f(0.8f, 0.2f, 0.1f).normalized();
  const Eigen::Vector3f middle_axis = Eigen::Vector3f(-0.1f, 0.9f, 0.3f).normalized();
  const Eigen::Vector3f minor_axis = major_axis.cross(middle_axis).normalized();

  moi::ReductionSummary summary;
  for (int i = 0; i < args.warmup; ++i)
    summary = computeSummary(cloud, indices, inertia_axis, major_axis, middle_axis, minor_axis);

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < args.iterations; ++i)
    summary = computeSummary(cloud, indices, inertia_axis, major_axis, middle_axis, minor_axis);
  const auto end = std::chrono::high_resolution_clock::now();

  doNotOptimize(summary.count);
  const std::uint64_t fingerprint = checksum(summary);
  doNotOptimize(fingerprint);
  const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
  return {total_us / static_cast<double>(std::max(args.iterations, 1)), fingerprint};
}

BenchResult
measureProjectedCovariance(const Args& args)
{
  const std::vector<pcl::PointXYZ> cloud = makeCloud(args.points);
  const std::vector<std::uint32_t> indices = makeIndices(args.points);
  const Eigen::Vector3f mean(64.0f, 0.75f, 127.0f);
  const Eigen::Vector3f normal = Eigen::Vector3f(-0.35f, 0.4f, 0.84f).normalized();

  moi::Covariance6 covariance;
  for (int i = 0; i < args.warmup; ++i)
    covariance = computeProjectedCovariance(cloud, indices, mean, normal);

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < args.iterations; ++i)
    covariance = computeProjectedCovariance(cloud, indices, mean, normal);
  const auto end = std::chrono::high_resolution_clock::now();

  doNotOptimize(covariance[0]);
  const std::uint64_t fingerprint = checksum(covariance);
  doNotOptimize(fingerprint);
  const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
  return {total_us / static_cast<double>(std::max(args.iterations, 1)), fingerprint};
}

BenchResult
measurePublicCompute(const Args& args)
{
  const auto cloud = makePointCloud(args.points);
  const auto indices = makePclIndices(args.points);
  pcl::MomentOfInertiaEstimation<pcl::PointXYZ> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);
  estimator.setAngleStep(45.0f);

  for (int i = 0; i < args.warmup; ++i)
    estimator.compute();

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < args.iterations; ++i)
    estimator.compute();
  const auto end = std::chrono::high_resolution_clock::now();

  const std::uint64_t fingerprint = checksum(estimator);
  doNotOptimize(fingerprint);
  const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
  return {total_us / static_cast<double>(std::max(args.iterations, 1)), fingerprint};
}

template <typename PointT>
BenchResult
measurePublicComputeTyped(const Args& args)
{
  const auto cloud = makeTypedPointCloud<PointT>(args.points);
  const auto indices = makePclIndices(args.points);
  pcl::MomentOfInertiaEstimation<PointT> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);
  estimator.setAngleStep(45.0f);

  for (int i = 0; i < args.warmup; ++i)
    estimator.compute();

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < args.iterations; ++i)
    estimator.compute();
  const auto end = std::chrono::high_resolution_clock::now();

  std::vector<float> moments;
  std::vector<float> eccentricities;
  estimator.getMomentOfInertia(moments);
  estimator.getEccentricity(eccentricities);
  std::uint64_t fingerprint = checksum(estimator);
  fingerprint ^= static_cast<std::uint64_t>(moments.size()) << 16;
  fingerprint ^= static_cast<std::uint64_t>(eccentricities.size()) << 32;
  doNotOptimize(fingerprint);
  const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
  return {total_us / static_cast<double>(std::max(args.iterations, 1)), fingerprint};
}

bool
matchesCase(const std::string& filter, const std::string& label)
{
  return filter == "all" || filter == label;
}

void
runCase(const Args& args, const std::string& label, BenchResult (*measure)(const Args&))
{
  const BenchResult result = measure(args);
  std::cout << std::left << std::setw(56)
            << label + ",points=" + std::to_string(args.points)
            << ": " << std::fixed << std::setprecision(4) << result.avg_us << " us/iter\n";
  std::cout << " Checksum: " << result.checksum << "\n";
}

} // namespace

int
main(int argc, char** argv)
{
  const Args args = parseArgs(argc, argv);
  std::cout << "============================================================\n";
  std::cout << " PCL moment_of_inertia_estimation diagnostic Benchmark\n";
  std::cout << " Iterations: " << args.iterations << "\n";
  std::cout << " Warmup Iterations: " << args.warmup << "\n";
  std::cout << " Dataset: indexed " << pointTypeLabel(args.case_filter) << " points=" << args.points
            << " iterations=" << args.iterations << " unit=us/iter\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ defined\n";
#else
  std::cout << " build: __RVV10__ NOT defined\n";
#endif
  std::cout << "============================================================\n";

  if (matchesCase(args.case_filter, "moi_reductions"))
    runCase(args, "moi_reductions", measureReductions);
  if (matchesCase(args.case_filter, "moi_projected_covariance"))
    runCase(args, "moi_projected_covariance", measureProjectedCovariance);
  if (args.case_filter == "moi_public_compute")
    runCase(args, "moi_public_compute", measurePublicCompute);
  if (args.case_filter == "moi_public_compute_pointxyzi")
    runCase(args, "moi_public_compute_pointxyzi", measurePublicComputeTyped<pcl::PointXYZI>);
  if (args.case_filter == "moi_public_compute_pointxyzrgb")
    runCase(args, "moi_public_compute_pointxyzrgb", measurePublicComputeTyped<pcl::PointXYZRGB>);
  if (args.case_filter == "moi_public_compute_pointxyzrgba")
    runCase(args, "moi_public_compute_pointxyzrgba", measurePublicComputeTyped<pcl::PointXYZRGBA>);
  if (args.case_filter == "moi_public_compute_pointxyzrgbnormal")
    runCase(args, "moi_public_compute_pointxyzrgbnormal", measurePublicComputeTyped<pcl::PointXYZRGBNormal>);
  return 0;
}
