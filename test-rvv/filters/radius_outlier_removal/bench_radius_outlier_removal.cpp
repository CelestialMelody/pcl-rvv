#include "radius_outlier_removal_diag.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

using namespace pcl_rvv_filters_radius_outlier_removal;

constexpr int kIterations = 5;
constexpr std::size_t kBannerWidth = 100;

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

void
printBanner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void run(const std::function<void()>& fn, int iterations = kIterations, int warmup = 1) const
  {
    for (int i = 0; i < warmup; ++i)
      fn();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      fn();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(72) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t checksum) const { checksum_ = checksum; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

void
benchTail(const std::string& name,
          const pcl::Indices& indices,
          const std::vector<std::uint8_t>& to_keep,
          bool extract_removed,
          bool use_rvv)
{
  Benchmarker bench(name);
  ApplyFilterIndicesTailResult result;
  const ApplyFilterIndicesTailReplayContext ctx{&indices, &to_keep, extract_removed};
  bench.run([&]() {
    result = applyFilterIndicesTailAutoReplay(ctx, use_rvv);
    bench.setChecksum(checksumApplyFilterIndicesTailResult(result));
    doNotOptimize(result);
  });
}

void
benchFull(const std::string& name,
          const pcl::PointCloud<pcl::PointXYZ>& cloud,
          const pcl::Indices& indices,
          const std::vector<std::uint8_t>& to_keep,
          bool extract_removed,
          int simulated_search_rounds,
          bool use_rvv)
{
  Benchmarker bench(name);
  ApplyFilterIndicesDiagnosticResult result;
  bench.run([&]() {
    result = runApplyFilterIndicesDiagnostic(cloud, indices, to_keep, extract_removed, simulated_search_rounds, use_rvv);
    bench.setChecksum(result.checksum);
    doNotOptimize(result);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/radius_outlier_removal RVV diagnostic\n";
#if defined(__RVV10__) && defined(PCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV available, diagnostic macro disabled\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic to_keep masks and source indices; tail compress plus search-dominated full diagnostic\n";
  std::cout << "Iterations: " << kIterations << '\n';
  printBanner('-');

  const auto cloud64k = makePointCloud(65536);
  const auto cloud1m = makePointCloud(1048576);
  const auto identity64k = makeIndices(65536, false);
  const auto shuffled64k = makeIndices(65536, true);
  const auto identity1m = makeIndices(1048576, false);
  const auto shuffled1m = makeIndices(1048576, true);
  const auto mostly_keep64k = makeKeepMask(65536, 0);
  const auto half_keep64k = makeKeepMask(65536, 1);
  const auto mostly_remove64k = makeKeepMask(65536, 2);
  const auto mostly_keep1m = makeKeepMask(1048576, 0);
  const auto half_keep1m = makeKeepMask(1048576, 1);
  const auto mostly_remove1m = makeKeepMask(1048576, 2);

#if defined(__RVV10__) && defined(PCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC)
  constexpr bool kUseRVV = true;
#else
  constexpr bool kUseRVV = false;
#endif

  benchTail("radius_outlier_removal tail compress mostly-keep 64K",
            identity64k,
            mostly_keep64k,
            true,
            kUseRVV);
  benchTail("radius_outlier_removal tail compress half-keep shuffled 64K",
            shuffled64k,
            half_keep64k,
            true,
            kUseRVV);
  benchTail("radius_outlier_removal tail compress mostly-remove 64K",
            identity64k,
            mostly_remove64k,
            true,
            kUseRVV);
  benchTail("radius_outlier_removal tail compress mostly-keep 1M",
            identity1m,
            mostly_keep1m,
            true,
            kUseRVV);
  benchTail("radius_outlier_removal tail compress half-keep shuffled 1M",
            shuffled1m,
            half_keep1m,
            true,
            kUseRVV);
  benchTail("radius_outlier_removal tail compress no-removed 1M",
            identity1m,
            mostly_keep1m,
            false,
            kUseRVV);
  benchFull("radius_outlier_removal full diag light-search 64K",
            cloud64k,
            identity64k,
            mostly_keep64k,
            true,
            1,
            kUseRVV);
  benchFull("radius_outlier_removal full diag search-dominated 64K",
            cloud64k,
            identity64k,
            mostly_keep64k,
            true,
            8,
            kUseRVV);
  benchFull("radius_outlier_removal full diag search-dominated shuffled 64K",
            cloud64k,
            shuffled64k,
            half_keep64k,
            true,
            8,
            kUseRVV);
  benchFull("radius_outlier_removal full diag search-dominated 1M",
            cloud1m,
            identity1m,
            mostly_keep1m,
            true,
            8,
            kUseRVV);
  benchFull("radius_outlier_removal full diag search-dominated mostly-remove 1M",
            cloud1m,
            identity1m,
            mostly_remove1m,
            true,
            8,
            kUseRVV);

  printBanner('=');
  return 0;
}
