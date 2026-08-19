/*
 * 本文件做什么：
 * 这是 ndt topic 的 QEMU smoke / board bench（性能测试）入口。输出格式
 * 保持 shared analyzer 可解析：Dataset、Iterations、每个 case 的
 * `name: X ms/iter`、checksum 和 Total Time。
 *
 * 证据边界：
 * QEMU 运行只证明 build、日志形状和 RVV path（RVV 执行链路）可命中；
 * 性能结论只来自板卡或目标硬件。当前 case 是 pre-production diagnostic
 *（接入生产前诊断），不能直接写成 production-ready。
 */

#include "ndt.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/ndt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
  int iterations{80};
  int warmup_iterations{8};
  std::string case_filter{"all"};
  std::size_t samples{65536};
  std::size_t public_points{4096};
  int public_max_iterations{8};
  double public_resolution{0.35};
  double public_step_size{0.05};
  std::string public_search_method{"direct7"};
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
    else if (arg == "--samples")
      options.samples =
          static_cast<std::size_t>(std::max(1, std::atoi(require_value("--samples"))));
    else if (arg == "--public-points")
      options.public_points =
          static_cast<std::size_t>(std::max(64, std::atoi(require_value("--public-points"))));
    else if (arg == "--public-max-iterations")
      options.public_max_iterations =
          std::max(1, std::atoi(require_value("--public-max-iterations")));
    else if (arg == "--public-resolution")
      options.public_resolution = std::atof(require_value("--public-resolution"));
    else if (arg == "--public-step-size")
      options.public_step_size = std::atof(require_value("--public-step-size"));
    else if (arg == "--public-search-method")
      options.public_search_method = require_value("--public-search-method");
    else if (arg == "--help") {
      std::cout << "Usage: bench_ndt [--iterations N] [--warmup-iterations N]\n"
                   "                 [--case-filter all|hessian|gradient|public]\n"
                   "                 [--samples N]\n"
                   "                 [--public-points N] [--public-max-iterations N]\n"
                   "                 [--public-resolution R] [--public-step-size S]\n"
                   "                 [--public-search-method radius|direct27|direct26|direct7|direct1]\n";
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
wants_derivative_cases(const std::string& filter)
{
  return filter == "all" || filter.find("hessian") != std::string::npos ||
         filter.find("gradient") != std::string::npos ||
         filter.find("derivative") != std::string::npos;
}

pcl::NeighborSearchMethod
parse_public_search_method(const std::string& value)
{
  if (value == "radius")
    return pcl::NeighborSearchMethod::RADIUS;
  if (value == "direct27")
    return pcl::NeighborSearchMethod::DIRECT27;
  if (value == "direct26")
    return pcl::NeighborSearchMethod::DIRECT26;
  if (value == "direct7")
    return pcl::NeighborSearchMethod::DIRECT7;
  if (value == "direct1")
    return pcl::NeighborSearchMethod::DIRECT1;
  throw std::runtime_error("unknown public search method: " + value);
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
run_derivative_case(const pcl::registration::rvv_ndt_support::DerivativeBatch& batch,
                    const std::size_t salt)
{
  (void)salt;
  return pcl::registration::rvv_ndt_support::checksum_result(
      pcl::registration::rvv_ndt_support::derivative_accumulate_candidate(batch));
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_public_target_cloud(const std::size_t requested_points, const float resolution)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->reserve(requested_points);

  const std::array<std::array<float, 3>, 8> offsets{{
      {-0.060f, -0.030f, -0.020f},
      {-0.040f, 0.050f, 0.035f},
      {0.025f, -0.055f, 0.045f},
      {0.055f, 0.040f, -0.050f},
      {-0.015f, 0.015f, 0.065f},
      {0.045f, -0.010f, -0.030f},
      {-0.055f, 0.035f, -0.055f},
      {0.015f, -0.045f, 0.015f},
  }};
  const std::size_t cells = std::max<std::size_t>(1, (requested_points + offsets.size() - 1) /
                                                        offsets.size());
  const std::size_t side = static_cast<std::size_t>(std::ceil(std::cbrt(static_cast<double>(cells))));
  const float spacing = resolution * 1.35f;

  for (std::size_t cell = 0; cell < cells && cloud->size() < requested_points; ++cell) {
    const std::size_t ix = cell % side;
    const std::size_t iy = (cell / side) % side;
    const std::size_t iz = cell / (side * side);
    const float base_x = spacing * static_cast<float>(ix);
    const float base_y = spacing * static_cast<float>(iy);
    const float base_z = spacing * static_cast<float>(iz);
    for (const auto& offset : offsets) {
      if (cloud->size() >= requested_points)
        break;
      pcl::PointXYZ point;
      point.x = base_x + offset[0];
      point.y = base_y + offset[1];
      point.z = base_z + offset[2];
      cloud->push_back(point);
    }
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
    shifted.x = point.x - 0.035f;
    shifted.y = point.y + 0.025f;
    shifted.z = point.z - 0.020f;
    cloud->push_back(shifted);
  }
  return cloud;
}

std::uint64_t
run_public_align_case(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source,
                      const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                      const Options& options,
                      const std::size_t salt)
{
  pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> reg;
  pcl::PointCloud<pcl::PointXYZ> output;
  reg.setNeighborhoodSearchMethod(parse_public_search_method(options.public_search_method));
  reg.setNumberOfThreads(1);
  reg.setMinPointPerVoxel(3);
  reg.setMaximumIterations(options.public_max_iterations);
  reg.setResolution(static_cast<float>(options.public_resolution));
  reg.setStepSize(options.public_step_size);
  reg.setTransformationEpsilon(1e-6);
  reg.setInputSource(source);
  reg.setInputTarget(target);

  reg.align(output);

  std::uint64_t checksum = 1469598103934665603ull;
  checksum = pcl::registration::rvv_ndt_support::mix_u64(
      checksum, static_cast<std::uint64_t>(reg.hasConverged()));
  checksum = pcl::registration::rvv_ndt_support::mix_double(checksum, reg.getFitnessScore());
  checksum = pcl::registration::rvv_ndt_support::mix_double(
      checksum, reg.getTransformationLikelihood());
  const auto transform = reg.getFinalTransformation();
  for (int row = 0; row < transform.rows(); ++row)
    for (int col = 0; col < transform.cols(); ++col)
      checksum = pcl::registration::rvv_ndt_support::mix_double(
          checksum, static_cast<double>(transform(row, col)));
  checksum = pcl::registration::rvv_ndt_support::mix_u64(checksum, output.size());
  checksum = pcl::registration::rvv_ndt_support::mix_u64(
      checksum, static_cast<std::uint64_t>(reg.getFinalNumIteration()));
  return pcl::registration::rvv_ndt_support::mix_u64(checksum, static_cast<std::uint64_t>(salt));
}

} // namespace

int
main(int argc, char** argv)
{
  try {
    const Options options = parse_options(argc, argv);
    std::vector<BenchResult> results;

    std::cout << "Dataset: deterministic NDT derivative diagnostic and public-entry profile\n";
    std::cout << "Iterations: " << options.iterations << "\n";
    std::cout << "Warmup Iterations: " << options.warmup_iterations << "\n";
    std::cout << "Samples: " << options.samples << "\n";
    std::cout << "Public Points: " << options.public_points << "\n";
    std::cout << "Public Max Iterations: " << options.public_max_iterations << "\n";
    std::cout << "Public Resolution: " << options.public_resolution << "\n";
    std::cout << "Public Search Method: " << options.public_search_method << "\n";
#ifdef __RVV10__
    std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#else
    std::cout << "Build: Std diagnostic (__RVV10__ disabled)\n";
#endif

    auto maybe_run = [&](const std::string& name, auto warmup, auto run_once) {
      if (matches_filter(name, options.case_filter))
        results.push_back(run_case(name, options, warmup, run_once));
    };

    if (wants_derivative_cases(options.case_filter)) {
      const auto hessian_batch =
          pcl::registration::rvv_ndt_support::make_derivative_batch(options.samples, true);
      const auto gradient_batch =
          pcl::registration::rvv_ndt_support::make_derivative_batch(options.samples, false);
      maybe_run("derivative-hessian-staged",
                [&](std::size_t i) { return run_derivative_case(hessian_batch, i); },
                [&](std::size_t i) { return run_derivative_case(hessian_batch, i); });
      maybe_run("derivative-gradient-staged",
                [&](std::size_t i) { return run_derivative_case(gradient_batch, i); },
                [&](std::size_t i) { return run_derivative_case(gradient_batch, i); });
    }
    if (matches_filter("production-public-align-pointxyz", options.case_filter)) {
      const auto target = make_public_target_cloud(
          options.public_points, static_cast<float>(options.public_resolution));
      const auto source = make_public_source_cloud(*target);
      maybe_run("production-public-align-pointxyz",
                [&](std::size_t i) { return run_public_align_case(source, target, options, i); },
                [&](std::size_t i) { return run_public_align_case(source, target, options, i); });
    }

    if (results.empty())
      throw std::runtime_error("case filter matched no cases: " + options.case_filter);

    double total = 0.0;
    std::uint64_t total_checksum = 1469598103934665603ull;
    for (const BenchResult& result : results) {
      total += result.ms_per_iter * static_cast<double>(options.iterations);
      total_checksum =
          pcl::registration::rvv_ndt_support::mix_u64(total_checksum, result.checksum);
      std::cout << result.name << ": " << result.ms_per_iter << " ms/iter\n";
      std::cout << "checksum: " << result.checksum << "\n";
    }

    std::cout << "Total Time: " << total << " ms, checksum: " << total_checksum << "\n";
  }
  catch (const std::exception& error) {
    std::cerr << "bench_ndt error: " << error.what() << "\n";
    return 2;
  }
  return 0;
}
