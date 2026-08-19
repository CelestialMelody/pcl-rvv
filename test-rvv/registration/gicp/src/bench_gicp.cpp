/*
 * 本文件做什么：
 * 这是 gicp topic 的 QEMU smoke / board bench（性能测试）入口。输出格式
 * 保持 shared analyzer 可解析：Dataset、Iterations、每个 case 的
 * `name: X ms/iter`、checksum 和 Total Time。
 *
 * 证据边界：
 * QEMU 运行只证明 build、日志形状和 RVV path（RVV 执行链路）可命中；
 * 性能结论只来自板卡或目标硬件。当前 case 都是 pre-production diagnostic
 * （接入生产前诊断），不能直接写成 production-ready。
 */

#include "gicp.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/gicp.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
  int iterations{120};
  int warmup_iterations{10};
  std::string case_filter{"all"};
  std::size_t rows{65536};
  std::size_t points{32768};
  std::size_t k{20};
};

struct BenchResult {
  std::string name;
  double ms_per_iter{0.0};
  std::uint64_t checksum{1469598103934665603ull};
};

Options
parse_options(int argc, char** argv)
{
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto require_value = [&](const char* name) -> const char* {
      if (i + 1 >= argc)
        throw std::runtime_error(std::string("missing value for ") + name);
      return argv[++i];
    };
    if (arg == "--iterations")
      options.iterations = std::max(1, std::atoi(require_value("--iterations")));
    else if (arg == "--warmup-iterations")
      options.warmup_iterations =
          std::max(0, std::atoi(require_value("--warmup-iterations")));
    else if (arg == "--case-filter")
      options.case_filter = require_value("--case-filter");
    else if (arg == "--rows")
      options.rows = static_cast<std::size_t>(std::max(1, std::atoi(require_value("--rows"))));
    else if (arg == "--points")
      options.points =
          static_cast<std::size_t>(std::max(1, std::atoi(require_value("--points"))));
    else if (arg == "--k")
      options.k = static_cast<std::size_t>(std::max(1, std::atoi(require_value("--k"))));
    else if (arg == "--help") {
      std::cout << "Usage: bench_gicp [--iterations N] [--warmup-iterations N]\n"
                   "                  [--case-filter all|residual|covariance|production]\n"
                   "                  [--rows N] [--points N] [--k N]\n";
      std::exit(0);
    }
    else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }
  return options;
}

bool
matches_filter(const std::string& name, const std::string& filter)
{
  return filter == "all" || name.find(filter) != std::string::npos;
}

bool
matches_production_filter(const std::string& name, const std::string& filter)
{
  return filter != "all" && name.find(filter) != std::string::npos;
}

template <typename WarmupFn, typename RunFn>
BenchResult
run_case(const std::string& name,
         const Options& options,
         WarmupFn warmup,
         RunFn run_once)
{
  std::uint64_t checksum = 0;
  for (int i = 0; i < options.warmup_iterations; ++i)
    checksum ^= warmup(static_cast<std::size_t>(i));

  const auto start = Clock::now();
  for (int i = 0; i < options.iterations; ++i)
    checksum ^= run_once(static_cast<std::size_t>(i));
  const auto end = Clock::now();

  const auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
  return BenchResult{name, elapsed / static_cast<double>(options.iterations), checksum};
}

std::uint64_t
run_residual_case(const pcl::registration::rvv_gicp_support::ResidualInput& input,
                  const std::size_t salt)
{
  std::uint64_t checksum = pcl::registration::rvv_gicp_support::checksum_residual(
      pcl::registration::rvv_gicp_support::residual_accumulate_candidate(input));
  pcl::registration::rvv_gicp_support::mix_u64(checksum, static_cast<std::uint64_t>(salt));
  return checksum;
}

std::uint64_t
run_indexed_residual_case(
    const pcl::registration::rvv_gicp_support::IndexedResidualInput& input,
    const std::size_t salt)
{
  std::uint64_t checksum = pcl::registration::rvv_gicp_support::checksum_residual(
      pcl::registration::rvv_gicp_support::indexed_residual_accumulate_candidate(input));
  pcl::registration::rvv_gicp_support::mix_u64(checksum, static_cast<std::uint64_t>(salt));
  return checksum;
}

std::uint64_t
run_hessian_loop_case(const pcl::registration::rvv_gicp_support::ResidualInput& input,
                      const std::size_t salt)
{
  std::uint64_t checksum = pcl::registration::rvv_gicp_support::checksum_hessian_loop(
      pcl::registration::rvv_gicp_support::hessian_loop_accumulate_candidate(input));
  pcl::registration::rvv_gicp_support::mix_u64(checksum, static_cast<std::uint64_t>(salt));
  return checksum;
}

std::uint64_t
run_covariance_case(const pcl::registration::rvv_gicp_support::CovarianceInput& input,
                    const std::size_t salt)
{
  std::uint64_t checksum = pcl::registration::rvv_gicp_support::checksum_covariance(
      pcl::registration::rvv_gicp_support::covariance_post_knn_candidate(input));
  pcl::registration::rvv_gicp_support::mix_u64(checksum, static_cast<std::uint64_t>(salt));
  return checksum;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_public_target_cloud(const std::size_t requested_points)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->reserve(requested_points);
  for (std::size_t i = 0; i < requested_points; ++i) {
    const std::size_t x = i % 32u;
    const std::size_t y = (i / 32u) % 32u;
    const std::size_t z = i / (32u * 32u);
    pcl::PointXYZ point;
    point.x = 0.030f * static_cast<float>(x) +
              0.002f * std::sin(static_cast<float>(y + z));
    point.y = 0.028f * static_cast<float>(y) +
              0.002f * std::cos(static_cast<float>(x + z));
    point.z = 0.026f * static_cast<float>(z) +
              0.001f * std::sin(static_cast<float>(x + y));
    cloud->push_back(point);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_public_source_cloud(const pcl::PointCloud<pcl::PointXYZ>& target)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->reserve(target.size());
  for (const auto& point : target) {
    pcl::PointXYZ shifted;
    shifted.x = point.x - 0.020f;
    shifted.y = point.y + 0.015f;
    shifted.z = point.z - 0.010f;
    cloud->push_back(shifted);
  }
  return cloud;
}

std::uint64_t
run_production_public_align_case(
    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source,
    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
    const std::size_t salt)
{
  pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> reg;
  pcl::PointCloud<pcl::PointXYZ> output;
  reg.setInputSource(source);
  reg.setInputTarget(target);
  reg.setMaximumIterations(10);
  reg.setMaximumOptimizerIterations(6);
  reg.setTransformationEpsilon(1e-7);
  reg.setCorrespondenceRandomness(20);
  reg.setNumberOfThreads(1);

  reg.align(output);

  std::uint64_t checksum = 1469598103934665603ull;
  pcl::registration::rvv_gicp_support::mix_u64(
      checksum, static_cast<std::uint64_t>(reg.hasConverged()));
  pcl::registration::rvv_gicp_support::mix_double(checksum, reg.getFitnessScore());
  const auto transform = reg.getFinalTransformation();
  for (int row = 0; row < transform.rows(); ++row)
    for (int col = 0; col < transform.cols(); ++col)
      pcl::registration::rvv_gicp_support::mix_double(
          checksum, static_cast<double>(transform(row, col)));
  pcl::registration::rvv_gicp_support::mix_u64(checksum, output.size());
  pcl::registration::rvv_gicp_support::mix_u64(checksum, static_cast<std::uint64_t>(salt));
  return checksum;
}

} // namespace

int
main(int argc, char** argv)
{
  try {
    const Options options = parse_options(argc, argv);
    const auto residual_input =
        pcl::registration::rvv_gicp_support::make_residual_input(options.rows);
    const auto indexed_residual_input =
        pcl::registration::rvv_gicp_support::make_indexed_residual_input(options.rows);
    const auto covariance_input =
        pcl::registration::rvv_gicp_support::make_covariance_input(options.points, options.k);
    const auto production_target = make_public_target_cloud(std::max<std::size_t>(64, options.points));
    const auto production_source = make_public_source_cloud(*production_target);
    std::vector<BenchResult> results;

    std::cout << "Dataset: deterministic GICP diagnostic and production-public probe\n";
    std::cout << "Iterations: " << options.iterations << "\n";
    std::cout << "Warmup Iterations: " << options.warmup_iterations << "\n";
    std::cout << "Rows: " << options.rows << "\n";
    std::cout << "Points: " << options.points << "\n";
    std::cout << "K: " << options.k << "\n";
#ifdef __RVV10__
    std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#else
    std::cout << "Build: Std diagnostic (__RVV10__ disabled)\n";
#endif

    auto maybe_run = [&](const std::string& name, auto warmup, auto run_once) {
      if (matches_filter(name, options.case_filter))
        results.push_back(run_case(name, options, warmup, run_once));
    };

    maybe_run("residual-mahalanobis-dense",
              [&](std::size_t i) { return run_residual_case(residual_input, i); },
              [&](std::size_t i) { return run_residual_case(residual_input, i); });
    maybe_run("residual-indexed-gather",
              [&](std::size_t i) { return run_indexed_residual_case(indexed_residual_input, i); },
              [&](std::size_t i) { return run_indexed_residual_case(indexed_residual_input, i); });
    maybe_run("dfddf-loop-dense",
              [&](std::size_t i) { return run_hessian_loop_case(residual_input, i); },
              [&](std::size_t i) { return run_hessian_loop_case(residual_input, i); });
    maybe_run("covariance-post-knn-default-k",
              [&](std::size_t i) { return run_covariance_case(covariance_input, i); },
              [&](std::size_t i) { return run_covariance_case(covariance_input, i); });
    if (matches_production_filter("production-public-align-pointxyz", options.case_filter)) {
      results.push_back(run_case(
          "production-public-align-pointxyz",
          options,
          [&](std::size_t i) {
            return run_production_public_align_case(production_source, production_target, i);
          },
          [&](std::size_t i) {
            return run_production_public_align_case(production_source, production_target, i);
          }));
    }

    if (results.empty())
      throw std::runtime_error("case filter matched no cases: " + options.case_filter);

    double total = 0.0;
    std::uint64_t total_checksum = 1469598103934665603ull;
    for (const BenchResult& result : results) {
      total += result.ms_per_iter * static_cast<double>(options.iterations);
      pcl::registration::rvv_gicp_support::mix_u64(total_checksum, result.checksum);
      std::cout << result.name << ": " << result.ms_per_iter << " ms/iter\n";
      std::cout << "checksum: " << result.checksum << "\n";
    }

    std::cout << "Total Time: " << total << " ms, checksum: " << total_checksum << "\n";
  }
  catch (const std::exception& error) {
    std::cerr << "bench_gicp error: " << error.what() << "\n";
    return 2;
  }
  return 0;
}
