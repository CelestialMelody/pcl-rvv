/*
 * 本文件做什么：
 * 这个 bench（性能测试）把 MomentInvariantsEstimation 的 RVV 证据分成
 * diagnostic（诊断）和 production direct（真实生产路径）两层。helper-only
 * case 只计中心矩累加，public-search-shaped case 观察 searchForNeighbors
 * （邻域搜索）是否稀释局部收益，production case 直接调用真实 computeFeature。
 *
 * 证据边界：
 * `mi_production_compute_feature*` case 是 production-public（生产公开入口）
 * 证据；helper-only 和 public-search-shaped case 仍只作为历史诊断证据。
 */

#include "moment_invariants.h"

#include <pcl/features/moment_invariants.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace mi = pcl::features::rvv_test::moment_invariants;

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

pcl::Indices
makeIndices(const std::size_t points)
{
  pcl::Indices indices;
  indices.reserve(points);
  for (std::size_t i = 0; i < points; ++i)
    indices.push_back(static_cast<int>((i * 37 + 11) % points));
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
  cloud->is_dense = true;
  return cloud;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeTypedPointCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float f = static_cast<float>(i);
    cloud->points[i].x = 0.03125f * f - 13.0f;
    cloud->points[i].y = 0.125f * static_cast<float>((i * 11) % 67) - 4.0f;
    cloud->points[i].z = 2.0f + 0.0005f * f * f - 0.25f * static_cast<float>(i % 9);
  }
  cloud->width = static_cast<std::uint32_t>(cloud->points.size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

std::uint64_t
checksum(const mi::MomentSummary& summary)
{
  std::uint64_t seed = 1469598103934665603ull;
  auto mix = [&seed](const float value) {
    const auto q = static_cast<std::int64_t>(value * 1000.0f);
    seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  };
  mix(summary.mu200);
  mix(summary.mu020);
  mix(summary.mu002);
  mix(summary.mu110);
  mix(summary.mu101);
  mix(summary.mu011);
  mix(summary.j1);
  mix(summary.j2);
  mix(summary.j3);
  return seed;
}

std::uint64_t
checksum(const pcl::PointCloud<pcl::MomentInvariants>& moments)
{
  std::uint64_t seed = 1099511628211ull;
  for (const auto& point : moments) {
    const auto q1 = static_cast<std::int64_t>(point.j1 * 1000.0f);
    const auto q2 = static_cast<std::int64_t>(point.j2 * 1000.0f);
    const auto q3 = static_cast<std::int64_t>(point.j3 * 1000.0f);
    seed ^= static_cast<std::uint64_t>(q1 + 31 * q2 + 131 * q3) + 0x9e3779b97f4a7c15ull +
            (seed << 6) + (seed >> 2);
  }
  return seed;
}

template <typename F>
BenchResult
timeCase(const int warmup, const int iterations, F&& fn)
{
  std::uint64_t last_checksum = 1469598103934665603ull;
  auto mix_checksum = [&last_checksum](const std::uint64_t value) {
    last_checksum ^= value + 0x9e3779b97f4a7c15ull + (last_checksum << 6) + (last_checksum >> 2);
  };
  for (int i = 0; i < warmup; ++i)
    mix_checksum(fn());

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    mix_checksum(fn());
  const auto end = std::chrono::steady_clock::now();
  doNotOptimize(last_checksum);

  const double total_us =
      std::chrono::duration<double, std::micro>(end - start).count();
  return {total_us / static_cast<double>(iterations), last_checksum};
}

bool
caseEnabled(const Args& args, const std::string& name)
{
  return args.case_filter == "all" || args.case_filter == name;
}

void
printResult(const std::string& name, const std::size_t points, const BenchResult& result)
{
  std::cout << name << ",points=" << points << " : " << std::fixed << std::setprecision(4)
            << result.avg_us << " us / iter\n";
  std::cout << "Checksum: " << result.checksum << "\n";
}

mi::MomentSummary
computeCandidateIndexed(const pcl::PointXYZ* points, const pcl::Indices& indices)
{
#ifdef __RVV10__
  return mi::computeMomentSummaryRVV(points, indices);
#else
  return mi::computeMomentSummaryStd(points, indices);
#endif
}

mi::MomentSummary
computeCandidateFullCloud(const pcl::PointXYZ* points, const std::size_t count)
{
#ifdef __RVV10__
  return mi::computeMomentSummaryRVV(points, count);
#else
  return mi::computeMomentSummaryStd(points, count);
#endif
}

std::uint64_t
runPublicSearchShape(const std::size_t points)
{
  const auto cloud = makePointCloud(points);
  auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>(false);
  tree->setInputCloud(cloud);
  auto indices = pcl::make_shared<pcl::Indices>();
  indices->reserve(points);
  for (std::size_t i = 0; i < points; i += 4)
    indices->push_back(static_cast<int>(i));

  pcl::Indices nn_indices(32);
  std::vector<float> nn_dists(32);
  std::uint64_t seed = 1099511628211ull;
  for (const int query_index : *indices) {
    const int found = tree->nearestKSearch(query_index, 32, nn_indices, nn_dists);
    if (found == 0)
      continue;
    nn_indices.resize(static_cast<std::size_t>(found));
    const mi::MomentSummary summary = computeCandidateIndexed(cloud->points.data(), nn_indices);
    seed ^= checksum(summary) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    nn_indices.resize(32);
  }
  return seed;
}

pcl::IndicesPtr
makeQueryIndices(const std::size_t points)
{
  auto indices = pcl::make_shared<pcl::Indices>();
  indices->reserve(points / 4 + 1);
  for (std::size_t i = 0; i < points; i += 4)
    indices->push_back(static_cast<int>(i));
  return indices;
}

std::uint64_t
runProductionComputeFeature(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& cloud,
                            const pcl::IndicesPtr& indices,
                            const pcl::search::KdTree<pcl::PointXYZ>::Ptr& tree,
                            pcl::PointCloud<pcl::MomentInvariants>& output)
{
  pcl::MomentInvariantsEstimation<pcl::PointXYZ, pcl::MomentInvariants> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);
  estimator.setSearchMethod(tree);
  estimator.setKSearch(32);
  estimator.compute(output);
  return checksum(output);
}

template <typename PointT>
std::uint64_t
runProductionComputeFeatureTyped(const typename pcl::PointCloud<PointT>::ConstPtr& cloud,
                                 const pcl::IndicesPtr& indices,
                                 const typename pcl::search::KdTree<PointT>::Ptr& tree,
                                 pcl::PointCloud<pcl::MomentInvariants>& output)
{
  pcl::MomentInvariantsEstimation<PointT, pcl::MomentInvariants> estimator;
  estimator.setInputCloud(cloud);
  estimator.setIndices(indices);
  estimator.setSearchMethod(tree);
  estimator.setKSearch(32);
  estimator.compute(output);
  return checksum(output);
}

template <typename PointT>
void
runProductionTypedCase(const Args& args, const std::string& case_name)
{
  if (!caseEnabled(args, case_name))
    return;

  const std::size_t public_points = std::min<std::size_t>(args.points, 4096);
  const auto public_cloud = makeTypedPointCloud<PointT>(public_points);
  const pcl::IndicesPtr query_indices = makeQueryIndices(public_points);
  auto tree = pcl::make_shared<pcl::search::KdTree<PointT>>(false);
  tree->setInputCloud(public_cloud);
  pcl::PointCloud<pcl::MomentInvariants> output;
  const BenchResult result = timeCase(args.warmup, args.iterations, [&]() {
    return runProductionComputeFeatureTyped<PointT>(public_cloud, query_indices, tree, output);
  });
  printResult(case_name, public_points, result);
}

} // namespace

int
main(int argc, char** argv)
{
  const Args args = parseArgs(argc, argv);
  const std::vector<pcl::PointXYZ> cloud = makeCloud(args.points);
  const pcl::Indices indices = makeIndices(args.points);

  std::cout << "PCL Moment Invariants RVV diagnostic bench\n";
  std::cout << "Workload: synthetic PointXYZ moment_invariants; points=" << args.points
            << "; case_filter=" << args.case_filter << "\n";
  std::cout << "Iterations: " << args.iterations << "\n";
  std::cout << "Warmup Iterations: " << args.warmup << "\n";

  if (caseEnabled(args, "mi_accumulation_indexed")) {
    const BenchResult result = timeCase(args.warmup, args.iterations, [&]() {
      return checksum(computeCandidateIndexed(cloud.data(), indices));
    });
    printResult("mi_accumulation_indexed", args.points, result);
  }

  if (caseEnabled(args, "mi_accumulation_full_cloud")) {
    const BenchResult result = timeCase(args.warmup, args.iterations, [&]() {
      return checksum(computeCandidateFullCloud(cloud.data(), cloud.size()));
    });
    printResult("mi_accumulation_full_cloud", args.points, result);
  }

  if (caseEnabled(args, "mi_public_search_shape")) {
    const std::size_t public_points = std::min<std::size_t>(args.points, 4096);
    const BenchResult result = timeCase(args.warmup, args.iterations, [&]() {
      return runPublicSearchShape(public_points);
    });
    printResult("mi_public_search_shape", public_points, result);
  }

  if (caseEnabled(args, "mi_production_compute_feature")) {
    const std::size_t public_points = std::min<std::size_t>(args.points, 4096);
    const auto public_cloud = makePointCloud(public_points);
    const pcl::IndicesPtr query_indices = makeQueryIndices(public_points);
    auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>(false);
    tree->setInputCloud(public_cloud);
    pcl::PointCloud<pcl::MomentInvariants> output;
    const BenchResult result = timeCase(args.warmup, args.iterations, [&]() {
      return runProductionComputeFeature(public_cloud, query_indices, tree, output);
    });
    printResult("mi_production_compute_feature", public_points, result);
  }

  runProductionTypedCase<pcl::PointXYZI>(args, "mi_production_compute_feature_pointxyzi");
  runProductionTypedCase<pcl::PointXYZRGB>(args, "mi_production_compute_feature_pointxyzrgb");
  runProductionTypedCase<pcl::PointXYZRGBA>(args, "mi_production_compute_feature_pointxyzrgba");

  return 0;
}
