/*
 * pfh.hpp RVV topic benchmark（性能测试）入口。
 *
 * 输出格式兼容 `test-rvv/script/analyze_bench_compare.py`。QEMU（仿真器）运行只
 * 允许作为 build/log-shape smoke（构建 / 日志形状小型验证）；性能结论只能来自
 * board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
 */

#include "pfh.h"

#include <pcl/features/pfh.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace pfh_test = pcl::features::rvv_test::pfh;

namespace
{
using Clock = std::chrono::steady_clock;
using Estimator = pcl::PFHEstimation<pfh_test::PointT, pfh_test::PointT, pcl::PFHSignature125>;
using XYZNormalEstimator = pcl::PFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::PFHSignature125>;

struct Options
{
  int side = 32;
  int k = 32;
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
  hasArgValue(argc, argv, "--k", options.k);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.side = std::max(options.side, 8);
  options.k = std::max(options.k, 4);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  return options.case_filter == "all" || options.case_filter == name;
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
  const auto cloud = pfh_test::makeFeatureCloud(options.side);
  const auto [xyz_cloud, normal_cloud] = pfh_test::makeXYZAndNormalClouds(options.side);
  const int center = static_cast<int>(cloud->size() / 2);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, center, options.k);
  const pcl::Indices xyz_neighborhood = pfh_test::makeWrappedNeighborhood(*xyz_cloud, center, options.k);

  std::cout << "Dataset: synthetic pfh point-normal grid side=" << options.side
            << " points=" << cloud->size() << " k=" << options.k << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "component_pfh_signature"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      Estimator pfh;
      Eigen::VectorXf histogram(125);
      for (int repeat = 0; repeat < 128; ++repeat)
      {
        pfh.computePointPFHSignature(*cloud, *cloud, neighborhood, 5, histogram);
        local_checksum += pfh_test::checksumHistogram(histogram);
      }
      return local_checksum;
    }, checksum);
    printCase("component_pfh_signature", ms, checksum);
  }

  if (caseEnabled(options, "candidate_pfh_pair_batch_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      Eigen::VectorXf histogram;
      for (int repeat = 0; repeat < 128; ++repeat)
      {
        pfh_test::computePointPFHSignaturePairBatchRVV(*cloud, neighborhood, 5, histogram);
        local_checksum += pfh_test::checksumHistogram(histogram);
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_pfh_pair_batch_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_pfh_direct_aos_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      Eigen::VectorXf histogram;
      for (int repeat = 0; repeat < 128; ++repeat)
      {
        pfh_test::computePointPFHSignatureDirectAoSRVV(*cloud, neighborhood, 5, histogram);
        local_checksum += pfh_test::checksumHistogram(histogram);
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_pfh_direct_aos_rvv", ms, checksum);
  }

  if (caseEnabled(options, "component_pfh_signature_pointxyz_normal"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      XYZNormalEstimator pfh;
      Eigen::VectorXf histogram(125);
      for (int repeat = 0; repeat < 128; ++repeat)
      {
        pfh.computePointPFHSignature(*xyz_cloud, *normal_cloud, xyz_neighborhood, 5, histogram);
        local_checksum += pfh_test::checksumHistogram(histogram);
      }
      return local_checksum;
    }, checksum);
    printCase("component_pfh_signature_pointxyz_normal", ms, checksum);
  }

  if (caseEnabled(options, "public_pfh_k"))
  {
    Estimator pfh;
    pfh.setInputCloud(cloud);
    pfh.setInputNormals(cloud);
    pfh.setSearchMethod(pcl::search::KdTree<pfh_test::PointT>::Ptr(new pcl::search::KdTree<pfh_test::PointT>));
    pfh.setKSearch(options.k);
    pcl::PointCloud<pcl::PFHSignature125> output;

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      pfh.compute(output);
      double local_checksum = 0.0;
      for (const auto& descriptor : output)
      {
        for (int i = 0; i < 125; ++i)
          local_checksum += static_cast<double>(descriptor.histogram[i]) * static_cast<double>(i + 1);
      }
      return local_checksum;
    }, checksum);
    printCase("public_pfh_k", ms, checksum);
  }

  if (caseEnabled(options, "public_pfh_pointxyz_normal_k"))
  {
    XYZNormalEstimator pfh;
    pfh.setInputCloud(xyz_cloud);
    pfh.setInputNormals(normal_cloud);
    pfh.setSearchMethod(pcl::search::KdTree<pcl::PointXYZ>::Ptr(new pcl::search::KdTree<pcl::PointXYZ>));
    pfh.setKSearch(options.k);
    pcl::PointCloud<pcl::PFHSignature125> output;

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      pfh.compute(output);
      double local_checksum = 0.0;
      for (const auto& descriptor : output)
      {
        for (int i = 0; i < 125; ++i)
          local_checksum += static_cast<double>(descriptor.histogram[i]) * static_cast<double>(i + 1);
      }
      return local_checksum;
    }, checksum);
    printCase("public_pfh_pointxyz_normal_k", ms, checksum);
  }

  return 0;
}
