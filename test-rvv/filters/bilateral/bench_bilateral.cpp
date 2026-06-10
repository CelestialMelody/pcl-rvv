#include "bilateral_diag.hpp"

#include <pcl/filters/bilateral.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

namespace {

constexpr int kBenchmarkIterations = 3;
constexpr std::size_t kBenchmarkBannerWidth = 100;

void
printBanner(char ch)
{
  std::cout << std::string(kBenchmarkBannerWidth, ch) << '\n';
}

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

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 3) const
  {
    for (int i = 0; i < warmup; ++i)
      func();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(66) << name_ << ": " << std::fixed
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
printErrorStats(const pcl_rvv_filters_bilateral::IntensityErrorStats& stats)
{
  std::cout << "  Error: max_abs=" << std::scientific << std::setprecision(6) << stats.max_abs
            << ", max_rel=" << stats.max_rel
            << ", rmse=" << stats.rmse
            << ", p95_abs=" << stats.p95_abs
            << ", p99_abs=" << stats.p99_abs << std::fixed << std::setprecision(4) << '\n';
}

void
benchWeightStage(const std::string& name,
                 const pcl::PointCloud<pcl::PointXYZI>& cloud,
                 const pcl::Indices& neighbors,
                 const std::vector<float>& squared_distances)
{
  Benchmarker bench(name);
  std::vector<pcl_rvv_filters_bilateral::NeighborFeature> features;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
    if (!pcl_rvv_filters_bilateral::stageNeighborFeaturesRVV(
            cloud, 0, neighbors, squared_distances, features))
      features = pcl_rvv_filters_bilateral::stageNeighborFeaturesStd(
          cloud, 0, neighbors, squared_distances);
#else
    features = pcl_rvv_filters_bilateral::stageNeighborFeaturesStd(
        cloud, 0, neighbors, squared_distances);
#endif
    bench.setChecksum(static_cast<std::uint64_t>(
        pcl_rvv_filters_bilateral::accumulateWeightFromFeatures(features, 0.09, 18.0) * 1000000.0));
    doNotOptimize(features);
  });
}

void
benchFullDiagnostic(const std::string& name,
                    const pcl::PointCloud<pcl::PointXYZI>& cloud,
                    const pcl::Indices& indices,
                    double sigma_s,
                    double sigma_r)
{
  Benchmarker bench(name);
  pcl::PointCloud<pcl::PointXYZI> output;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
    output = pcl_rvv_filters_bilateral::filterRVV(cloud, indices, sigma_s, sigma_r);
#else
    output = pcl_rvv_filters_bilateral::filterStd(cloud, indices, sigma_s, sigma_r);
#endif
    bench.setChecksum(pcl_rvv_filters_bilateral::checksumCloudIntensity(output));
    doNotOptimize(output);
  });
}

void
benchFullExpDiagnostic(const std::string& name,
                       const pcl::PointCloud<pcl::PointXYZI>& cloud,
                       const pcl::Indices& indices,
                       double sigma_s,
                       double sigma_r)
{
  Benchmarker bench(name);
  const auto expected = pcl_rvv_filters_bilateral::filterStd(cloud, indices, sigma_s, sigma_r);
  pcl::PointCloud<pcl::PointXYZI> output;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
    output = pcl_rvv_filters_bilateral::filterExpRVV(cloud, indices, sigma_s, sigma_r);
#else
    output = pcl_rvv_filters_bilateral::filterStd(cloud, indices, sigma_s, sigma_r);
#endif
    bench.setChecksum(pcl_rvv_filters_bilateral::checksumCloudIntensity(output));
    doNotOptimize(output);
  });
  printErrorStats(pcl_rvv_filters_bilateral::compareCloudIntensity(expected, output));
}

void
benchProductionFilter(const std::string& name,
                      const pcl::PointCloud<pcl::PointXYZI>& cloud,
                      double sigma_s,
                      double sigma_r)
{
  Benchmarker bench(name);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const auto expected = pcl_rvv_filters_bilateral::filterStd(cloud, indices, sigma_s, sigma_r);
  pcl::PointCloud<pcl::PointXYZI> output;
  bench.run([&]() {
    output = pcl_rvv_filters_bilateral::filterProduction(cloud, sigma_s, sigma_r);
    bench.setChecksum(pcl_rvv_filters_bilateral::checksumCloudIntensity(output));
    doNotOptimize(output);
  });
  printErrorStats(pcl_rvv_filters_bilateral::compareCloudIntensity(expected, output));
}

void
benchErrorSweep(const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const std::pair<double, double> params[] = {
      {0.06, 8.0},
      {0.09, 18.0},
      {0.14, 32.0},
  };

  for (const auto& param : params) {
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
    const auto actual = pcl_rvv_filters_bilateral::filterExpRVV(cloud, indices, param.first, param.second);
#else
    const auto actual = pcl_rvv_filters_bilateral::filterStd(cloud, indices, param.first, param.second);
#endif
    const auto expected = pcl_rvv_filters_bilateral::filterStd(cloud, indices, param.first, param.second);
    const auto stats = pcl_rvv_filters_bilateral::compareCloudIntensity(expected, actual);
    std::cout << "ErrorCase: high-contrast full exp"
              << " sigma_s=" << std::fixed << std::setprecision(3) << param.first
              << " sigma_r=" << std::setprecision(3) << param.second;
    std::cout << std::setprecision(4) << '\n';
    printErrorStats(stats);
  }
}

void
benchProductionErrorSweep(const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const std::pair<double, double> params[] = {
      {0.06, 8.0},
      {0.09, 18.0},
      {0.14, 32.0},
  };

  for (const auto& param : params) {
    const auto expected = pcl_rvv_filters_bilateral::filterStd(cloud, indices, param.first, param.second);
    const auto actual = pcl_rvv_filters_bilateral::filterProduction(cloud, param.first, param.second);
    const auto stats = pcl_rvv_filters_bilateral::compareCloudIntensity(expected, actual);
    std::cout << "ProductionErrorCase: high-contrast"
              << " sigma_s=" << std::fixed << std::setprecision(3) << param.first
              << " sigma_r=" << std::setprecision(3) << param.second;
    std::cout << std::setprecision(4) << '\n';
    printErrorStats(stats);
  }
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/bilateral RVV diagnostic\n";
#if defined(__RVV10__) && defined(PCL_BILATERAL_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV available, diagnostic macro disabled\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZI grid clouds; BilateralFilter radiusSearch full diagnostic and neighbor weight staging\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud256 = pcl_rvv_filters_bilateral::makeCloud(16, 16, false);
  const auto cloud1k = pcl_rvv_filters_bilateral::makeCloud(32, 32, false);
  const auto cloud1k_invalid = pcl_rvv_filters_bilateral::makeCloud(32, 32, true);
  const auto high_contrast = pcl_rvv_filters_bilateral::makeHighContrastCloud(40, 32);
  const auto indices256 = pcl_rvv_filters_bilateral::makeIndices(cloud256.size(), false);
  const auto indices1k = pcl_rvv_filters_bilateral::makeIndices(cloud1k.size(), false);
  const auto subset1k = pcl_rvv_filters_bilateral::makeIndices(cloud1k.size(), true);
  const auto indices1k_invalid = pcl_rvv_filters_bilateral::makeIndices(cloud1k_invalid.size(), false);

  pcl::Indices neighbors;
  std::vector<float> squared_distances;
  for (int i = 0; i < 512; ++i) {
    neighbors.push_back(i);
    squared_distances.push_back(static_cast<float>((i % 97) + 1) * 0.00015f);
  }

  benchWeightStage("bilateral neighbor weight-stage diag 512", cloud256, neighbors, squared_distances);
  benchFullDiagnostic("bilateral full diag 256 radiusSearch", cloud256, indices256, 0.09, 18.0);
  benchFullDiagnostic("bilateral full diag 1K radiusSearch", cloud1k, indices1k, 0.09, 18.0);
  benchFullDiagnostic("bilateral subset full diag 1K radiusSearch", cloud1k, subset1k, 0.09, 18.0);
  benchFullDiagnostic("bilateral finite full diag 1K radiusSearch", cloud1k_invalid, indices1k_invalid, 0.09, 18.0);
  benchFullExpDiagnostic("bilateral full exp diag 256 radiusSearch", cloud256, indices256, 0.09, 18.0);
  benchFullExpDiagnostic("bilateral full exp diag 1K radiusSearch", cloud1k, indices1k, 0.09, 18.0);
  benchFullExpDiagnostic("bilateral subset full exp diag 1K radiusSearch", cloud1k, subset1k, 0.09, 18.0);
  benchErrorSweep(high_contrast);
  benchProductionErrorSweep(high_contrast);
  benchProductionFilter("bilateral production filter 256", cloud256, 0.09, 18.0);
  benchProductionFilter("bilateral production filter 1K", cloud1k, 0.09, 18.0);

  printBanner('=');
  return 0;
}
