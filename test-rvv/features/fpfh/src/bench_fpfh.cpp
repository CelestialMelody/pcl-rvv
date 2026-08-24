/*
 * fpfh.hpp RVV topic benchmark（性能测试）入口。
 *
 * 输出格式兼容 `test-rvv/script/analyze_bench_compare.py`。QEMU（仿真器）运行只
 * 允许作为 build/log-shape smoke（构建 / 日志形状小型验证）；性能结论只能来自
 * board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
 */

#include "fpfh.h"

#include <pcl/features/fpfh.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace fpfh_test = pcl::features::rvv_test::fpfh;

namespace
{
using Clock = std::chrono::steady_clock;
using Estimator = pcl::FPFHEstimation<fpfh_test::PointT, fpfh_test::PointT, pcl::FPFHSignature33>;

struct Options
{
  int side = 48;
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
  const auto cloud = fpfh_test::makeFeatureCloud(options.side);
  const int center = static_cast<int>(cloud->size() / 2);
  const pcl::Indices neighborhood = fpfh_test::makeWrappedNeighborhood(*cloud, center, options.k);

  std::cout << "Dataset: synthetic fpfh point-normal grid side=" << options.side
            << " points=" << cloud->size() << " k=" << options.k << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "component_spfh_signature"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      Estimator fpfh;
      Eigen::MatrixXf hist_f1(1, 11);
      Eigen::MatrixXf hist_f2(1, 11);
      Eigen::MatrixXf hist_f3(1, 11);
      for (int repeat = 0; repeat < 256; ++repeat)
      {
        hist_f1.setZero();
        hist_f2.setZero();
        hist_f3.setZero();
        fpfh.computePointSPFHSignature(*cloud, *cloud, center, 0, neighborhood, hist_f1, hist_f2, hist_f3);
        local_checksum += hist_f1.sum() + 3.0 * hist_f2.sum() + 7.0 * hist_f3.sum();
      }
      return local_checksum;
    }, checksum);
    printCase("component_spfh_signature", ms, checksum);
  }

  if (caseEnabled(options, "component_weighted_spfh_33"))
  {
    Eigen::MatrixXf hist_f1(options.k, 11);
    Eigen::MatrixXf hist_f2(options.k, 11);
    Eigen::MatrixXf hist_f3(options.k, 11);
    for (int row = 0; row < options.k; ++row)
    {
      for (int bin = 0; bin < 11; ++bin)
      {
        hist_f1(row, bin) = 0.25f + static_cast<float>((row + 1) * (bin + 2) % 17);
        hist_f2(row, bin) = 0.50f + static_cast<float>((row + 3) * (bin + 1) % 19);
        hist_f3(row, bin) = 0.75f + static_cast<float>((row + 5) * (bin + 4) % 23);
      }
    }
    pcl::Indices row_indices = fpfh_test::makeSequentialIndices(static_cast<std::size_t>(options.k));
    std::vector<float> dists(static_cast<std::size_t>(options.k));
    for (std::size_t i = 0; i < dists.size(); ++i)
      dists[i] = i == 0 ? 0.0f : 0.25f + 0.03125f * static_cast<float>(i);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      Estimator fpfh;
      Eigen::VectorXf histogram;
      for (int repeat = 0; repeat < 2048; ++repeat)
      {
        fpfh.weightPointSPFHSignature(hist_f1, hist_f2, hist_f3, row_indices, dists, histogram);
        local_checksum += fpfh_test::checksumHistogram(histogram);
      }
      return local_checksum;
    }, checksum);
    printCase("component_weighted_spfh_33", ms, checksum);
  }

  if (caseEnabled(options, "candidate_weighted_spfh_dense_rows"))
  {
    Eigen::MatrixXf hist_f1(options.k, 11);
    Eigen::MatrixXf hist_f2(options.k, 11);
    Eigen::MatrixXf hist_f3(options.k, 11);
    for (int row = 0; row < options.k; ++row)
    {
      for (int bin = 0; bin < 11; ++bin)
      {
        hist_f1(row, bin) = 0.125f + static_cast<float>((row + 2) * (bin + 5) % 29);
        hist_f2(row, bin) = 0.375f + static_cast<float>((row + 7) * (bin + 3) % 31);
        hist_f3(row, bin) = 0.625f + static_cast<float>((row + 11) * (bin + 9) % 37);
      }
    }
    pcl::Indices row_indices = fpfh_test::makeSequentialIndices(static_cast<std::size_t>(options.k));
    std::vector<float> dists(static_cast<std::size_t>(options.k));
    for (std::size_t i = 0; i < dists.size(); ++i)
      dists[i] = i == 0 ? 0.0f : 0.20f + 0.041f * static_cast<float>(i);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      Eigen::VectorXf histogram;
      for (int repeat = 0; repeat < 2048; ++repeat)
      {
        fpfh_test::weightPointSPFHDenseRowsRVV(hist_f1, hist_f2, hist_f3, row_indices, dists, histogram);
        local_checksum += fpfh_test::checksumHistogram(histogram);
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_weighted_spfh_dense_rows", ms, checksum);
  }

  if (caseEnabled(options, "public_fpfh_k"))
  {
    Estimator fpfh;
    fpfh.setInputCloud(cloud);
    fpfh.setInputNormals(cloud);
    fpfh.setSearchMethod(pcl::search::KdTree<fpfh_test::PointT>::Ptr(new pcl::search::KdTree<fpfh_test::PointT>));
    fpfh.setKSearch(options.k);
    pcl::PointCloud<pcl::FPFHSignature33> output;

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      fpfh.compute(output);
      double local_checksum = 0.0;
      for (const auto& descriptor : output)
      {
        for (int i = 0; i < 33; ++i)
          local_checksum += static_cast<double>(descriptor.histogram[i]) * static_cast<double>(i + 1);
      }
      return local_checksum;
    }, checksum);
    printCase("public_fpfh_k", ms, checksum);
  }

  return 0;
}
