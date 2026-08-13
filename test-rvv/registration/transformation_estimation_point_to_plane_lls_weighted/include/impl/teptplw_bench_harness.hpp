/*
 * 本文件做什么：
 * transformation_estimation_point_to_plane_lls_weighted bench 的 CLI、计时、trace
 * 和输出合同。它不定义具体 benchmark case，也不证明 production dispatch。
 */

#pragma once

#include "teptplw_candidates.hpp"

#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <Eigen/Core>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_bench {

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;

struct BenchResult {
  std::string name;
  double average_ms = 0.0;
  double total_ms = 0.0;
  double checksum = 0.0;
  std::vector<double> iteration_ms;
};

struct BenchOptions {
  std::vector<std::size_t> sizes = {65536u, 262144u};
  std::string case_filter;
  int iterations = 20;
  int warmup_iterations = 0;
  std::vector<std::string> generic_abc_order = {"baseline", "abc", "ilp"};
};

static int g_warmup_iterations = 0;

inline void
set_warmup_iterations(const int value)
{
  g_warmup_iterations = value;
}

inline std::vector<std::size_t>
parse_sizes(const std::string& value)
{
  std::vector<std::size_t> sizes;
  std::stringstream stream(value);
  std::string token;
  while (std::getline(stream, token, ',')) {
    if (!token.empty())
      sizes.push_back(static_cast<std::size_t>(std::stoul(token)));
  }
  return sizes.empty() ? std::vector<std::size_t>{65536u, 262144u} : sizes;
}

inline std::vector<std::string>
parse_generic_abc_order(const std::string& value)
{
  std::vector<std::string> order;
  std::stringstream stream(value);
  std::string token;
  while (std::getline(stream, token, ',')) {
    if (token.empty())
      continue;
    if (token == "baseline" || token == "abc" || token == "ilp") {
      order.push_back(token);
      continue;
    }
    throw std::invalid_argument(
        "Unknown token in --generic-abc-order: " + token +
        " (expected baseline,abc,ilp)");
  }
  if (order.empty())
    return {"baseline", "abc", "ilp"};
  return order;
}

inline BenchOptions
parse_options(const int argc, char** argv)
{
  BenchOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto read_value = [&](const std::string& prefix) -> std::string {
      if (arg.rfind(prefix + "=", 0) == 0)
        return arg.substr(prefix.size() + 1);
      if (arg == prefix && i + 1 < argc)
        return argv[++i];
      return {};
    };

    std::string value = read_value("--size");
    if (!value.empty()) {
      options.sizes = parse_sizes(value);
      continue;
    }
    value = read_value("--case-filter");
    if (!value.empty()) {
      options.case_filter = value;
      continue;
    }
    value = read_value("--iterations");
    if (!value.empty()) {
      const int parsed = std::stoi(value);
      if (parsed <= 0)
        throw std::invalid_argument("--iterations must be positive");
      options.iterations = parsed;
      continue;
    }
    value = read_value("--warmup-iterations");
    if (!value.empty()) {
      const int parsed = std::stoi(value);
      if (parsed < 0)
        throw std::invalid_argument("--warmup-iterations must be non-negative");
      options.warmup_iterations = parsed;
      continue;
    }
    value = read_value("--generic-abc-order");
    if (!value.empty()) {
      options.generic_abc_order = parse_generic_abc_order(value);
      continue;
    }
  }
  return options;
}

template <typename NormalEquationLike>
double
normal_equation_checksum(const NormalEquationLike& eq)
{
  // no-solve component bench 需要稳定 checksum，避免编译器把构造整段消掉。
  double checksum = static_cast<double>(eq.accepted_points) * 1e-6;
  for (int row = 0; row < 6; ++row) {
    for (int col = 0; col < 6; ++col)
      checksum += eq.ata(row, col) * (1.0 + row * 6 + col) * 1e-9;
    checksum += eq.atb(row) * (1.0 + row) * 1e-6;
  }
  return checksum;
}

inline double
solved_normal_equation_checksum(const diag::NormalEquation& eq)
{
  // full-estimate 与 component no-solve 对照时，full 路径也保留 normal-equation
  // sink，再额外 sink solve 后的 matrix，减少 checksum 口径差异带来的误读。
  const Eigen::Matrix4f matrix = diag::solve_normal_equation(eq);
  return normal_equation_checksum(eq) + diag::matrix_checksum(matrix);
}

template <typename Fn>
BenchResult
run_case(const std::string& name, const int iterations, Fn&& fn)
{
  volatile double warmup_checksum = 0.0;
  for (int i = 0; i < g_warmup_iterations; ++i)
    warmup_checksum += fn();
  double checksum = 0.0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto stop = std::chrono::steady_clock::now();
  const double total_ms =
      std::chrono::duration<double, std::milli>(stop - start).count();
  return BenchResult{name, total_ms / iterations, total_ms, checksum};
}

template <typename Fn>
BenchResult
run_case_trace(const std::string& name, const int iterations, Fn&& fn)
{
  volatile double warmup_checksum = 0.0;
  for (int i = 0; i < g_warmup_iterations; ++i)
    warmup_checksum += fn();
  double checksum = 0.0;
  double total_ms = 0.0;
  std::vector<double> iteration_ms;
  iteration_ms.reserve(static_cast<std::size_t>(iterations));
  for (int i = 0; i < iterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    checksum += fn();
    const auto stop = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(stop - start).count();
    iteration_ms.push_back(elapsed_ms);
    total_ms += elapsed_ms;
  }
  return BenchResult{name, total_ms / iterations, total_ms, checksum, iteration_ms};
}

inline double
median_value(std::vector<double> values)
{
  std::sort(values.begin(), values.end());
  const std::size_t mid = values.size() / 2;
  if (values.size() % 2 != 0)
    return values[mid];
  return (values[mid - 1] + values[mid]) * 0.5;
}

template <typename PointSource, typename PointTarget>
double
run_public_full_cloud_weighted(
    pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<PointSource,
                                                                        PointTarget>&
        estimator,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights)
{
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, matrix);
  return diag::matrix_checksum(matrix);
}

template <typename PointSource, typename PointTarget>
double
run_public_source_indices_weighted(
    pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<PointSource,
                                                                        PointTarget>&
        estimator,
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights)
{
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, source_indices, target, matrix);
  return diag::matrix_checksum(matrix);
}

inline double
solved_production_normal_equation_checksum(
    const pcl::registration::detail::PointToPlaneLLSWeightedNormalEquation& eq)
{
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  pcl::registration::detail::solvePointToPlaneLLSWeightedNormalEquation(eq, matrix);
  return normal_equation_checksum(eq) + diag::matrix_checksum(matrix);
}

template <typename PointSource, typename PointTarget>
pcl::registration::detail::PointToPlaneLLSWeightedNormalEquation
build_production_source_indices_staged_or_std(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    pcl::registration::detail::PointToPlaneLLSWeightedFullCloudStats* stats)
{
#if defined(__RVV10__)
  pcl::registration::detail::PointToPlaneLLSWeightedNormalEquation eq;
  if (pcl::registration::detail::buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(
          source, source_indices, target, weights, eq, stats))
    return eq;
#endif
  return pcl::registration::detail::buildPointToPlaneLLSWeightedSourceIndicesStd(
      source, source_indices, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
pcl::registration::detail::PointToPlaneLLSWeightedNormalEquation
build_production_source_indices_block_fused_or_std(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights,
    pcl::registration::detail::PointToPlaneLLSWeightedFullCloudStats* stats)
{
#if defined(__RVV10__)
  pcl::registration::detail::PointToPlaneLLSWeightedNormalEquation eq;
  if (pcl::registration::detail::
          buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(
              source, source_indices, target, weights, eq, stats))
    return eq;
#endif
  return pcl::registration::detail::buildPointToPlaneLLSWeightedSourceIndicesStd(
      source, source_indices, target, weights, stats);
}

template <typename PointSource, typename PointTarget>
double
run_production_source_indices_staged_no_solve(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights)
{
  pcl::registration::detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto eq = build_production_source_indices_staged_or_std(
      source, source_indices, target, weights, &stats);
  return normal_equation_checksum(eq) +
         static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
}

template <typename PointSource, typename PointTarget>
double
run_production_source_indices_block_fused_no_solve(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights)
{
  pcl::registration::detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto eq = build_production_source_indices_block_fused_or_std(
      source, source_indices, target, weights, &stats);
  return normal_equation_checksum(eq) +
         static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
}

template <typename PointSource, typename PointTarget>
double
run_production_source_indices_staged_full(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights)
{
  pcl::registration::detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto eq = build_production_source_indices_staged_or_std(
      source, source_indices, target, weights, &stats);
  return solved_production_normal_equation_checksum(eq) +
         static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
}

template <typename PointSource, typename PointTarget>
double
run_production_source_indices_block_fused_full(
    const pcl::PointCloud<PointSource>& source,
    const pcl::Indices& source_indices,
    const pcl::PointCloud<PointTarget>& target,
    const std::vector<float>& weights)
{
  pcl::registration::detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto eq = build_production_source_indices_block_fused_or_std(
      source, source_indices, target, weights, &stats);
  return solved_production_normal_equation_checksum(eq) +
         static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
}

inline void
print_results(const std::vector<BenchResult>& results)
{
  double checksum = 0.0;
  double total = 0.0;
  for (const auto& result : results) {
    checksum += result.checksum;
    total += result.total_ms;
    std::cout << result.name << ": " << result.average_ms << " ms/iter\n";
    std::cout << "  Total Time: " << result.total_ms << " ms\n";
    std::cout << "  Checksum: " << result.checksum << "\n";
    if (!result.iteration_ms.empty()) {
      std::cout << "  Iteration Times:";
      for (const double elapsed_ms : result.iteration_ms)
        std::cout << " " << elapsed_ms;
      std::cout << " ms\n";

      std::vector<double> sorted = result.iteration_ms;
      std::sort(sorted.begin(), sorted.end());
      std::cout << "  Iteration Min/Median/Max: " << sorted.front() << " / "
                << median_value(result.iteration_ms) << " / " << sorted.back()
                << " ms\n";
    }
  }
  std::cout << "Total Time: " << total << " ms\n";
  std::cout << "Checksum: " << checksum << "\n";
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_bench
