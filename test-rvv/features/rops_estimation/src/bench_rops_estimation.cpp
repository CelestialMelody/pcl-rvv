/*
 * 本文件做什么：
 * 这个 bench（性能测试）只测 RoPS（Rotational Projection Statistics，旋转投影统计）
 * distribution matrix（分布矩阵）和 rotateCloud + AABB（轴对齐包围盒）
 * 两个 component diagnostic（组件诊断）case：前者把 rotated local cloud
 * 按 XY / XZ / YZ 三个 projection（投影）分到二维 bins（分箱）中；后者把
 * synthetic local cloud 旋转后写回 rotated cloud，并规约三轴 min/max。
 * combined case（组合 case）把 rotateCloud + AABB 和 distribution matrix 串到
 * 同一个计时边界中，观察 component 正向能否在更接近 production 的链路里保留。
 *
 * 证据边界：
 * Std build（标量构建）中 candidate helper 自动回退到 scalar helper；RVV build
 *（RISC-V Vector 构建）才执行候选路径。distribution matrix timing 包含 row/col
 * staging 和标量 scatter，不包含 LRF、rotateCloud、mesh local surface 或完整
 * descriptor normalization；rotateCloud timing 只覆盖 AoS stride load/store、
 * 3x3 rotation 和 min/max reduction，不包含 production 的 LRF / projection / descriptor
 * normalization。combined case 仍不包含 LRF、central moments 或完整 descriptor
 * normalization，只能作为 production-shaped diagnostic 证据。production-detail
 * case（生产私有 helper 直连证据）通过真实 `ROPSEstimation` private helper
 * 进入本阶段 production dispatch（生产分流逻辑），用于区分“测试 helper 正向”
 * 和“生产源码分流正向”。
 */

#include "rops_estimation.h"

#include <Eigen/Core>

#define private public
#include <pcl/features/rops_estimation.h>
#undef private

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

namespace rops = pcl::features::rvv_test::rops;

namespace {
using Clock = std::chrono::steady_clock;

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

struct Options {
  std::size_t points = 65536;
  unsigned int bins = 5;
  int repeat = 16;
  int iterations = 30;
  int warmup = 4;
  std::string case_filter = "all";
};

bool
hasArgValue(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::size_t& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
      value = static_cast<std::size_t>(std::strtoull(argv[i + 1], nullptr, 10));
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
      value = argv[i + 1];
      return true;
    }
  }
  return false;
}

Options
parseOptions(const int argc, char** argv)
{
  Options options;
  int bins = static_cast<int>(options.bins);
  hasArgValue(argc, argv, "--points", options.points);
  hasArgValue(argc, argv, "--bins", bins);
  hasArgValue(argc, argv, "--repeat", options.repeat);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup-iterations", options.warmup);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.points = std::max<std::size_t>(options.points, 1);
  options.bins = static_cast<unsigned int>(std::max(bins, 2));
  options.repeat = std::max(options.repeat, 1);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  return options.case_filter == "all" || options.case_filter == name;
}

template <typename PointT>
pcl::PointCloud<PointT>
makeCloudAs(const std::size_t points)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(points);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(points);
  for (std::size_t i = 0; i < points; ++i) {
    const float fi = static_cast<float>(i);
    cloud.points[i].x = -1.0f + 2.0f * static_cast<float>((i * 17) % 1024) / 1023.0f;
    cloud.points[i].y = 0.5f + 2.25f * static_cast<float>((i * 31 + 7) % 2048) / 2047.0f;
    cloud.points[i].z = -0.75f + 4.0f * static_cast<float>((i * 47 + 13) % 4096) / 4095.0f;
    cloud.points[i].x += 0.00003f * std::sin(fi * 0.013f);
    cloud.points[i].y += 0.00002f * std::cos(fi * 0.017f);
  }
  if (!cloud.points.empty()) {
    cloud.points.front().x = -1.0f;
    cloud.points.front().y = 0.5f;
    cloud.points.front().z = -0.75f;
    cloud.points.back().x = 1.0f;
    cloud.points.back().y = 2.75f;
    cloud.points.back().z = 3.25f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
makeCloud(const std::size_t points)
{
  return makeCloudAs<pcl::PointXYZ>(points);
}

std::array<pcl::PointXYZ, 4>
makeRotateAxes()
{
  return {
      pcl::PointXYZ{1.0f, 0.0f, 0.0f},
      pcl::PointXYZ{0.0f, 1.0f, 0.0f},
      pcl::PointXYZ{0.0f, 0.0f, 1.0f},
      pcl::PointXYZ{0.57735026f, 0.57735026f, 0.57735026f}};
}

std::array<float, 4>
makeRotateAngles()
{
  return {22.5f, 45.0f, 67.5f, 33.0f};
}

std::uint64_t
checksumMatrix(const Eigen::MatrixXf& matrix)
{
  std::uint64_t seed = 1469598103934665603ull;
  for (int row = 0; row < matrix.rows(); ++row) {
    for (int col = 0; col < matrix.cols(); ++col) {
      const auto q = static_cast<std::int64_t>(matrix(row, col) * 1000000.0f);
      seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }
  }
  return seed;
}

std::int64_t
quantizeRotateValue(const float value)
{
  return static_cast<std::int64_t>(std::lround(value * 1000.0f));
}

template <typename PointT>
std::uint64_t
checksumRotateCloudXYZ(const pcl::PointCloud<PointT>& rotated_cloud,
                       const Eigen::Vector3f& min,
                       const Eigen::Vector3f& max)
{
  std::uint64_t seed = 1469598103934665603ull;
  double sum_x = 0.0;
  double sum_y = 0.0;
  double sum_z = 0.0;
  double sum_abs_x = 0.0;
  double sum_abs_y = 0.0;
  double sum_abs_z = 0.0;
  for (const auto& pt : rotated_cloud.points) {
    sum_x += pt.x;
    sum_y += pt.y;
    sum_z += pt.z;
    sum_abs_x += std::abs(pt.x);
    sum_abs_y += std::abs(pt.y);
    sum_abs_z += std::abs(pt.z);
  }
  const std::array<std::int64_t, 7> aggregate = {
      static_cast<std::int64_t>(rotated_cloud.size()),
      quantizeRotateValue(static_cast<float>(sum_x)),
      quantizeRotateValue(static_cast<float>(sum_y)),
      quantizeRotateValue(static_cast<float>(sum_z)),
      quantizeRotateValue(static_cast<float>(sum_abs_x)),
      quantizeRotateValue(static_cast<float>(sum_abs_y)),
      quantizeRotateValue(static_cast<float>(sum_abs_z))};
  for (const auto value : aggregate)
    seed ^= static_cast<std::uint64_t>(value) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);

  for (int axis = 0; axis < 3; ++axis) {
    const auto qmin = quantizeRotateValue(min(axis));
    const auto qmax = quantizeRotateValue(max(axis));
    seed ^= static_cast<std::uint64_t>(qmin) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qmax) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  }
  return seed;
}

std::uint64_t
checksumRotateCloud(const pcl::PointCloud<pcl::PointXYZ>& rotated_cloud,
                    const Eigen::Vector3f& min,
                    const Eigen::Vector3f& max)
{
  return checksumRotateCloudXYZ(rotated_cloud, min, max);
}

std::uint64_t
runDistributionMatrixCase(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const Eigen::Vector3f& min,
                          const Eigen::Vector3f& max,
                          const unsigned int bins,
                          const int repeat)
{
  std::uint64_t checksum = 1099511628211ull;
  Eigen::MatrixXf matrix(bins, bins);
  for (int r = 0; r < repeat; ++r) {
    for (unsigned int projection = 0; projection < 3; ++projection) {
      rops::getDistributionMatrixRVV(projection, min, max, cloud, matrix);
      checksum ^= checksumMatrix(matrix) + 0x9e3779b97f4a7c15ull + (checksum << 6) + (checksum >> 2);
    }
  }
  doNotOptimize(checksum);
  return checksum;
}

std::uint64_t
runRotateCloudCase(const pcl::PointCloud<pcl::PointXYZ>& cloud, const int repeat)
{
  const auto axes = makeRotateAxes();
  const auto angles = makeRotateAngles();
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < repeat; ++r) {
    for (std::size_t i = 0; i < axes.size(); ++i) {
      pcl::PointCloud<pcl::PointXYZ> rotated_cloud;
      Eigen::Vector3f min;
      Eigen::Vector3f max;
      rops::rotateCloudRVV(axes[i], angles[i], cloud, rotated_cloud, min, max);
      checksum ^= checksumRotateCloud(rotated_cloud, min, max) + 0x9e3779b97f4a7c15ull +
                  (checksum << 6) + (checksum >> 2);
    }
  }
  doNotOptimize(checksum);
  return checksum;
}

std::uint64_t
runRotateDistributionPipelineCase(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                  const unsigned int bins,
                                  const int repeat)
{
  const auto axes = makeRotateAxes();
  const auto angles = makeRotateAngles();
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < repeat; ++r) {
    for (std::size_t i = 0; i < axes.size(); ++i) {
      pcl::PointCloud<pcl::PointXYZ> rotated_cloud;
      Eigen::Vector3f min;
      Eigen::Vector3f max;
      std::array<Eigen::MatrixXf, 3> matrices;
      rops::rotateCloudAndDistributionMatricesRVV(
          axes[i], angles[i], cloud, bins, rotated_cloud, min, max, matrices);

      checksum ^= checksumRotateCloud(rotated_cloud, min, max) + 0x9e3779b97f4a7c15ull +
                  (checksum << 6) + (checksum >> 2);
      for (const auto& matrix : matrices)
        checksum ^= checksumMatrix(matrix) + 0x9e3779b97f4a7c15ull + (checksum << 6) +
                    (checksum >> 2);
    }
  }
  doNotOptimize(checksum);
  return checksum;
}

template <typename PointT>
std::uint64_t
runProductionRotateDistributionPipelineCaseT(const pcl::PointCloud<PointT>& cloud,
                                             const unsigned int bins,
                                             const int repeat)
{
  const auto axes = makeRotateAxes();
  const auto angles = makeRotateAngles();
  pcl::ROPSEstimation<PointT, pcl::Histogram<135>> estimator;
  estimator.setNumberOfPartitionBins(bins);
  std::uint64_t checksum = 1469598103934665603ull;
  for (int r = 0; r < repeat; ++r) {
    for (std::size_t i = 0; i < axes.size(); ++i) {
      PointT axis;
      axis.x = axes[i].x;
      axis.y = axes[i].y;
      axis.z = axes[i].z;
      pcl::PointCloud<PointT> rotated_cloud;
      Eigen::Vector3f min;
      Eigen::Vector3f max;
      estimator.rotateCloud(axis, angles[i], cloud, rotated_cloud, min, max);

      checksum ^= checksumRotateCloudXYZ(rotated_cloud, min, max) + 0x9e3779b97f4a7c15ull +
                  (checksum << 6) + (checksum >> 2);
      for (unsigned int projection = 0; projection < 3; ++projection) {
        Eigen::MatrixXf matrix(bins, bins);
        estimator.getDistributionMatrix(projection, min, max, rotated_cloud, matrix);
        checksum ^= checksumMatrix(matrix) + 0x9e3779b97f4a7c15ull + (checksum << 6) +
                    (checksum >> 2);
      }
    }
  }
  doNotOptimize(checksum);
  return checksum;
}

std::uint64_t
runProductionRotateDistributionPipelineCase(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                            const unsigned int bins,
                                            const int repeat)
{
  return runProductionRotateDistributionPipelineCaseT(cloud, bins, repeat);
}

template <typename Fn>
double
timeCase(const int warmup, const int iterations, Fn&& fn, std::uint64_t& checksum)
{
  for (int i = 0; i < warmup; ++i)
    checksum ^= fn() + 0x9e3779b97f4a7c15ull + (checksum << 6) + (checksum >> 2);

  const auto start = Clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum ^= fn() + 0x9e3779b97f4a7c15ull + (checksum << 6) + (checksum >> 2);
  const auto end = Clock::now();
  doNotOptimize(checksum);
  return std::chrono::duration<double, std::micro>(end - start).count() /
         static_cast<double>(iterations);
}

void
printResult(const std::string& name, const Options& options, const double us_per_iter, const std::uint64_t checksum)
{
  std::cout << name << ",points=" << options.points << ",bins=" << options.bins << " : "
            << std::fixed << std::setprecision(4) << us_per_iter << " us / iter\n";
  std::cout << "Checksum: " << checksum << "\n";
}

void
printRotateResult(const std::string& name,
                  const std::size_t points,
                  const int repeat,
                  const double us_per_iter,
                  const std::uint64_t checksum)
{
  std::cout << name << ",points=" << points << ",repeat=" << repeat << " : "
            << std::fixed << std::setprecision(4) << us_per_iter << " us / iter\n";
  std::cout << "Checksum: " << checksum << "\n";
}

} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeCloud(options.points);
  const Eigen::Vector3f min(-1.0f, 0.5f, -0.75f);
  const Eigen::Vector3f max(1.0f, 2.75f, 3.25f);

  std::cout << "PCL ROPS RVV diagnostic bench\n";
  std::cout << "Workload: synthetic PointXYZ rops component diagnostics; points=" << options.points
            << "; bins=" << options.bins << "; repeat=" << options.repeat
            << "; projections=3; case_filter=" << options.case_filter << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "rops_distribution_matrix_binning")) {
    std::uint64_t checksum = 1469598103934665603ull;
    const double us = timeCase(options.warmup, options.iterations, [&]() {
      return runDistributionMatrixCase(cloud, min, max, options.bins, options.repeat);
    }, checksum);
    printResult("rops_distribution_matrix_binning", options, us, checksum);
  }

  if (caseEnabled(options, "rops_rotate_cloud_aabb")) {
    std::uint64_t checksum = 1469598103934665603ull;
    const double us = timeCase(options.warmup, options.iterations, [&]() {
      return runRotateCloudCase(cloud, options.repeat);
    }, checksum);
    printRotateResult("rops_rotate_cloud_aabb", options.points, options.repeat, us, checksum);
  }

  if (caseEnabled(options, "rops_rotate_distribution_pipeline")) {
    std::uint64_t checksum = 1469598103934665603ull;
    const double us = timeCase(options.warmup, options.iterations, [&]() {
      return runRotateDistributionPipelineCase(cloud, options.bins, options.repeat);
    }, checksum);
    printRotateResult("rops_rotate_distribution_pipeline", options.points, options.repeat, us, checksum);
  }

  if (caseEnabled(options, "rops_production_rotate_distribution_pipeline")) {
    std::uint64_t checksum = 1469598103934665603ull;
    const double us = timeCase(options.warmup, options.iterations, [&]() {
      return runProductionRotateDistributionPipelineCase(cloud, options.bins, options.repeat);
    }, checksum);
    printRotateResult(
        "rops_production_rotate_distribution_pipeline", options.points, options.repeat, us, checksum);
  }

  if (caseEnabled(options, "rops_production_pointxyzi_rotate_distribution_pipeline")) {
    const pcl::PointCloud<pcl::PointXYZI> typed_cloud = makeCloudAs<pcl::PointXYZI>(options.points);
    std::uint64_t checksum = 1469598103934665603ull;
    const double us = timeCase(options.warmup, options.iterations, [&]() {
      return runProductionRotateDistributionPipelineCaseT(typed_cloud, options.bins, options.repeat);
    }, checksum);
    printRotateResult(
        "rops_production_pointxyzi_rotate_distribution_pipeline", options.points, options.repeat, us, checksum);
  }

  if (caseEnabled(options, "rops_production_pointnormal_rotate_distribution_pipeline")) {
    const pcl::PointCloud<pcl::PointNormal> typed_cloud = makeCloudAs<pcl::PointNormal>(options.points);
    std::uint64_t checksum = 1469598103934665603ull;
    const double us = timeCase(options.warmup, options.iterations, [&]() {
      return runProductionRotateDistributionPipelineCaseT(typed_cloud, options.bins, options.repeat);
    }, checksum);
    printRotateResult(
        "rops_production_pointnormal_rotate_distribution_pipeline", options.points, options.repeat, us, checksum);
  }

  return 0;
}
