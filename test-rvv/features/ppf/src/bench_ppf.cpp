/*
 * ppf.hpp RVV topic benchmark（性能测试）入口。
 *
 * 输出格式兼容 `test-rvv/script/analyze_bench_compare.py`。QEMU（仿真器）运行只
 * 允许作为 build/log-shape smoke（构建 / 日志形状小型验证）；性能结论只能来自
 * board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
 */

#include "ppf.h"

#include <pcl/features/ppf.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace ppf_test = pcl::features::rvv_test::ppf;

namespace
{
using Clock = std::chrono::steady_clock;

struct Options
{
  int side = 28;
  int index_count = 64;
  int repeat = 8;
  int iterations = 8;
  int warmup = 2;
  std::string case_filter = "all";
};

bool
hasArgValue(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
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
  hasArgValue(argc, argv, "--side", options.side);
  hasArgValue(argc, argv, "--index-count", options.index_count);
  hasArgValue(argc, argv, "--repeat", options.repeat);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.side = std::max(options.side, 4);
  options.index_count = std::max(options.index_count, 1);
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

pcl::Indices
makePrefixIndices(const std::size_t cloud_size, const int requested_count)
{
  pcl::Indices indices = ppf_test::makeSequentialIndices(cloud_size);
  indices.resize(std::min(indices.size(), static_cast<std::size_t>(requested_count)));
  return indices;
}

template <typename Fn>
double
timeCase(const int warmup, const int iterations, Fn&& fn, double& checksum)
{
  for (int i = 0; i < warmup; ++i)
    checksum += fn();

  const auto t0 = Clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / static_cast<double>(iterations);
}

void
printCase(const std::string& name, const double ms_per_iter, const double checksum)
{
  std::cout << name << ": " << ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << checksum << "\n";
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(options.side);
  const pcl::Indices indices = makePrefixIndices(cloud->size(), options.index_count);

  std::cout << "Dataset: synthetic ppf grid side=" << options.side
            << " points=" << cloud->size() << " indices=" << indices.size()
            << " repeat=" << options.repeat << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "component_ppf_reference"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::PPFSignature> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        ppf_test::computePPFReference(*cloud, *normals, indices, output);
        local_checksum += ppf_test::checksumPPF(output);
      }
      return local_checksum;
    }, checksum);
    printCase("component_ppf_reference", ms, checksum);
  }

  if (caseEnabled(options, "candidate_ppf_pair_feature_batch_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::PPFSignature> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        ppf_test::computePPFPairFeatureBatchRVV(*cloud, *normals, indices, output);
        local_checksum += ppf_test::checksumPPF(output);
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_ppf_pair_feature_batch_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_ppf_alpha_m_batch_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::PPFSignature> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        ppf_test::computePPFAlphaMBatchRVV(*cloud, *normals, indices, output);
        local_checksum += ppf_test::checksumPPF(output);
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_ppf_alpha_m_batch_rvv", ms, checksum);
  }

  if (caseEnabled(options, "public_ppf_compute"))
  {
    pcl::PPFEstimation<pcl::PointXYZ, pcl::Normal, pcl::PPFSignature> estimator;
    estimator.setInputCloud(cloud);
    estimator.setInputNormals(normals);
    estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::PPFSignature> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        estimator.compute(output);
        local_checksum += ppf_test::checksumPPF(output);
      }
      return local_checksum;
    }, checksum);
    printCase("public_ppf_compute", ms, checksum);
  }

  if (caseEnabled(options, "public_ppf_compute_pointxyzi_normal"))
  {
    const auto [xyzi_cloud, xyzi_normals] = ppf_test::makeXYZIAndNormalClouds(options.side);
    pcl::PPFEstimation<pcl::PointXYZI, pcl::Normal, pcl::PPFSignature> estimator;
    estimator.setInputCloud(xyzi_cloud);
    estimator.setInputNormals(xyzi_normals);
    estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::PPFSignature> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        estimator.compute(output);
        local_checksum += ppf_test::checksumPPF(output);
      }
      return local_checksum;
    }, checksum);
    printCase("public_ppf_compute_pointxyzi_normal", ms, checksum);
  }

  if (caseEnabled(options, "public_ppf_compute_pointxyz_pointnormal"))
  {
    const auto [xyz_cloud, point_normals] = ppf_test::makeXYZAndPointNormalClouds(options.side);
    pcl::PPFEstimation<pcl::PointXYZ, pcl::PointNormal, pcl::PPFSignature> estimator;
    estimator.setInputCloud(xyz_cloud);
    estimator.setInputNormals(point_normals);
    estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::PPFSignature> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        estimator.compute(output);
        local_checksum += ppf_test::checksumPPF(output);
      }
      return local_checksum;
    }, checksum);
    printCase("public_ppf_compute_pointxyz_pointnormal", ms, checksum);
  }

  return 0;
}
