/*
 * 本文件做什么：
 * 这是 bfgs topic 的 QEMU smoke / future board bench（性能测试）入口。
 * 输出格式故意保持简单：每个 case 打印 `name: X ms/iter`，下一行打印
 * `checksum: ...`，最后打印 Total Time。topic-local manifest wrapper 会解析
 * 这些行，再交给 Evidence Doctor（证据体检）检查证据边界。
 *
 * 证据边界：
 * QEMU 运行只证明 build、日志形状和 RVV path（RVV 执行链路）可命中；
 * 不能作为真实性能结论。当前 case 也仍是 test-only diagnostic，不代表
 * production `BFGS` 模板已经接入 RVV。
 */

#include "bfgs.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using pcl::registration::rvv_bfgs_support::DirectionResult;
using pcl::registration::rvv_bfgs_support::MoveSlopeResult;

struct Options {
  int iterations{200};
  int warmup_iterations{10};
  std::string case_filter{"all"};
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
    else if (arg == "--help") {
      std::cout << "Usage: bench_bfgs [--iterations N] [--warmup-iterations N]\n"
                   "                  [--case-filter all|direction-update|move-to-slope|gicp-shaped-vector6]\n";
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

void
hash_u64(std::uint64_t& hash, const std::uint64_t value)
{
  hash ^= value;
  hash *= 1099511628211ull;
}

void
hash_double(std::uint64_t& hash, const double value)
{
  std::uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "double hashing expects 64-bit double");
  std::memcpy(&bits, &value, sizeof(bits));
  hash_u64(hash, bits);
}

void
hash_vector(std::uint64_t& hash, const std::vector<double>& values)
{
  hash_u64(hash, static_cast<std::uint64_t>(values.size()));
  for (const double value : values)
    hash_double(hash, value);
}

std::uint64_t
checksum_direction(const DirectionResult& result)
{
  std::uint64_t hash = 1469598103934665603ull;
  hash_vector(hash, result.dx0);
  hash_vector(hash, result.dg0);
  hash_vector(hash, result.p);
  hash_double(hash, result.dxg);
  hash_double(hash, result.dgg);
  hash_double(hash, result.dxdg);
  hash_double(hash, result.dgnorm);
  hash_double(hash, result.A);
  hash_double(hash, result.B);
  hash_double(hash, result.g0norm);
  hash_double(hash, result.pnorm);
  hash_double(hash, result.fp0);
  return hash;
}

std::uint64_t
checksum_move_slope(const MoveSlopeResult& result)
{
  std::uint64_t hash = 1469598103934665603ull;
  hash_vector(hash, result.x_alpha);
  hash_double(hash, result.slope);
  return hash;
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
run_direction_update(const std::size_t n, const std::size_t salt)
{
  using namespace pcl::registration::rvv_bfgs_support;
  DirectionInput input = make_direction_input(n);
  if (!input.gradient.empty())
    input.gradient[salt % input.gradient.size()] += 1e-9 * static_cast<double>(salt + 1);
  return checksum_direction(direction_update_candidate(input));
}

std::uint64_t
run_move_to_slope(const std::size_t n, const std::size_t salt)
{
  using namespace pcl::registration::rvv_bfgs_support;
  const DirectionInput input = make_direction_input(n);
  const double alpha = 0.2 + 0.001 * static_cast<double>(salt % 17);
  return checksum_move_slope(move_to_and_slope_candidate(input, alpha));
}

std::uint64_t
run_gicp_shaped_vector6(const std::size_t salt)
{
  using namespace pcl::registration::rvv_bfgs_support;
  QuadraticFunctor functor(make_vector6_center());
  BFGS<QuadraticFunctor> optimizer(functor);
  auto x = make_vector6_initial();
  x[0] += 1e-5 * static_cast<double>(salt % 11);
  optimizer.minimizeInit(x);
  const BFGSSpace::Status status = optimizer.minimizeOneStep(x);

  std::uint64_t hash = 1469598103934665603ull;
  for (Eigen::DenseIndex i = 0; i < x.size(); ++i)
    hash_double(hash, x[i]);
  hash_double(hash, optimizer.f);
  hash_u64(hash, static_cast<std::uint64_t>(status));
  hash_u64(hash, static_cast<std::uint64_t>(functor.fdf_calls));
  return hash;
}

} // namespace

int
main(int argc, char** argv)
{
  try {
    const Options options = parse_options(argc, argv);
    std::vector<BenchResult> results;

    std::cout << "Dataset: deterministic BFGS diagnostic vectors\n";
    std::cout << "Iterations: " << options.iterations << "\n";
    std::cout << "Warmup Iterations: " << options.warmup_iterations << "\n";
#ifdef __RVV10__
    std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#else
    std::cout << "Build: Std diagnostic (__RVV10__ disabled)\n";
#endif

    auto maybe_run = [&](const std::string& name, auto warmup, auto run_once) {
      if (matches_filter(name, options.case_filter))
        results.push_back(run_case(name, options, warmup, run_once));
    };

    maybe_run("direction-update-vector6",
              [](std::size_t i) { return run_direction_update(6, i); },
              [](std::size_t i) { return run_direction_update(6, i); });
    maybe_run("direction-update-vector128",
              [](std::size_t i) { return run_direction_update(128, i); },
              [](std::size_t i) { return run_direction_update(128, i); });
    maybe_run("move-to-slope-vector128",
              [](std::size_t i) { return run_move_to_slope(128, i); },
              [](std::size_t i) { return run_move_to_slope(128, i); });
    maybe_run("gicp-shaped-vector6",
              [](std::size_t i) { return run_gicp_shaped_vector6(i); },
              [](std::size_t i) { return run_gicp_shaped_vector6(i); });

    if (results.empty())
      throw std::runtime_error("case filter matched no cases: " + options.case_filter);

    double total = 0.0;
    std::uint64_t total_checksum = 1469598103934665603ull;
    for (const BenchResult& result : results) {
      total += result.ms_per_iter * static_cast<double>(options.iterations);
      hash_u64(total_checksum, result.checksum);
      std::cout << result.name << ": " << result.ms_per_iter << " ms/iter\n";
      std::cout << "checksum: " << result.checksum << "\n";
    }

    std::cout << "Total Time: " << total << " ms, checksum: " << total_checksum << "\n";
  }
  catch (const std::exception& error) {
    std::cerr << "bench_bfgs error: " << error.what() << "\n";
    return 2;
  }
  return 0;
}
