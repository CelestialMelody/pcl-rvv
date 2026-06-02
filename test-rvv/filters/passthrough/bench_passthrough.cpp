#include <pcl/filters/passthrough.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>

namespace {

constexpr int kBenchmarkIterations = 30;
constexpr std::size_t kBenchmarkBannerWidth = 96;

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

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 5) const
  {
    for (int i = 0; i < warmup; ++i)
      func();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(58) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t checksum) const { checksum_ = checksum; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

pcl::PointCloud<pcl::PointXYZI>
makeCloud(std::size_t n, int invalid_period)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = invalid_period <= 0;
  cloud.points.resize(n);

  std::mt19937 rng(2026u + static_cast<std::uint32_t>(n));
  std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(i % 4096);
    cloud[i].y = static_cast<float>((i + 17) % 4096);
    cloud[i].z = static_cast<float>((i + 29) % 4096);
    cloud[i].intensity = dist(rng);
    if (invalid_period > 0 && i % static_cast<std::size_t>(invalid_period) == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    if (invalid_period > 0 && i % static_cast<std::size_t>(invalid_period + 37) == 0)
      cloud[i].intensity = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int v : indices)
    sum = (sum ^ static_cast<std::uint32_t>(v)) * 1099511628211ull;
  return sum;
}

template <typename PointT>
std::uint64_t
checksumCloud(const pcl::PointCloud<PointT>& cloud)
{
  std::uint64_t sum = cloud.size();
  for (std::size_t i = 0; i < cloud.size(); i += 257)
    sum ^= static_cast<std::uint64_t>(std::fabs(cloud[i].intensity) * 100000.0f) + (i << 1);
  return sum;
}

void
benchIndices(const std::string& name,
             const pcl::PointCloud<pcl::PointXYZI>& cloud,
             bool negative,
             bool extract_removed)
{
  Benchmarker bench(name);
  pcl::PassThrough<pcl::PointXYZI> filter(extract_removed);
  filter.setInputCloud(cloud.makeShared());
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.5f, 0.75f);
  filter.setNegative(negative);
  pcl::Indices indices;
  bench.run([&]() {
    filter.filter(indices);
    std::uint64_t checksum = checksumIndices(indices);
    if (extract_removed)
      checksum ^= checksumIndices(*filter.getRemovedIndices());
    bench.setChecksum(checksum);
    doNotOptimize(indices);
  });
}

void
benchCloudOut(const std::string& name, const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  Benchmarker bench(name);
  pcl::PassThrough<pcl::PointXYZI> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.5f, 0.75f);
  pcl::PointCloud<pcl::PointXYZI> out;
  bench.run([&]() {
    filter.filter(out);
    bench.setChecksum(checksumCloud(out));
    doNotOptimize(out);
  });
}

void
benchSubsetFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  auto subset = pcl::make_shared<pcl::Indices>();
  subset->reserve(cloud.size() / 2);
  for (std::size_t i = 1; i < cloud.size(); i += 2)
    subset->push_back(static_cast<int>(i));

  Benchmarker bench(name);
  pcl::PassThrough<pcl::PointXYZI> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setIndices(subset);
  filter.setFilterFieldName("intensity");
  filter.setFilterLimits(-0.5f, 0.75f);
  pcl::Indices indices;
  bench.run([&]() {
    filter.filter(indices);
    bench.setChecksum(checksumIndices(indices));
    doNotOptimize(indices);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/passthrough benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZI clouds; identity indices RVV cases and explicit subset fallback\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud_64k = makeCloud(64 * 1024, 0);
  const auto cloud_1m = makeCloud(1024 * 1024, 0);
  const auto cloud_1m_invalid = makeCloud(1024 * 1024, 113);

  benchIndices("passthrough indices range 64K", cloud_64k, false, false);
  benchIndices("passthrough indices range 1M", cloud_1m, false, false);
  benchIndices("passthrough indices negative 1M", cloud_1m, true, false);
  benchIndices("passthrough indices removed 1M invalid", cloud_1m_invalid, false, true);
  benchCloudOut("passthrough cloud-out range 1M", cloud_1m);
  benchSubsetFallback("passthrough explicit subset fallback 1M", cloud_1m);

  printBanner('=');
  return 0;
}
