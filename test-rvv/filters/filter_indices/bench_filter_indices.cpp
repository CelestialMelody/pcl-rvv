#include <pcl/filters/filter.h>
#include <pcl/filters/filter_indices.h>
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
    std::cout << std::left << std::setw(56) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t checksum) { checksum_ = checksum; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

pcl::PointCloud<pcl::PointXYZ>
makeXYZCloud(std::size_t n, int invalid_period)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = invalid_period <= 0;
  cloud.points.resize(n);

  std::mt19937 rng(2026u + static_cast<std::uint32_t>(n + invalid_period));
  std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = dist(rng);
    cloud[i].y = dist(rng);
    cloud[i].z = dist(rng);
    if (invalid_period > 0 && i % static_cast<std::size_t>(invalid_period) == 0)
      cloud[i].z = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

pcl::PointCloud<pcl::PointNormal>
makeNormalCloud(std::size_t n, int invalid_period)
{
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = false;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(i % 1024);
    cloud[i].y = static_cast<float>((i + 1) % 1024);
    cloud[i].z = static_cast<float>((i + 2) % 1024);
    cloud[i].normal_x = 1.0f;
    cloud[i].normal_y = 0.0f;
    cloud[i].normal_z = 0.0f;
    cloud[i].curvature = 0.1f;
    if (invalid_period > 0 && i % static_cast<std::size_t>(invalid_period) == 0)
      cloud[i].normal_y = std::numeric_limits<float>::infinity();
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
    sum ^= static_cast<std::uint64_t>(std::fabs(cloud[i].x) * 1000.0f) + (i << 1);
  return sum;
}

void
benchIndicesOnly(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  Benchmarker bench(name);
  pcl::Indices indices;
  bench.run([&]() {
    pcl::removeNaNFromPointCloud(cloud, indices);
    bench.setChecksum(checksumIndices(indices));
    doNotOptimize(indices);
  });
}

void
benchCloudOut(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  Benchmarker bench(name);
  pcl::PointCloud<pcl::PointXYZ> out;
  pcl::Indices indices;
  bench.run([&]() {
    pcl::removeNaNFromPointCloud(cloud, out, indices);
    bench.setChecksum(checksumIndices(indices) ^ checksumCloud(out));
    doNotOptimize(out);
    doNotOptimize(indices);
  });
}

void
benchNormals(const std::string& name, const pcl::PointCloud<pcl::PointNormal>& cloud)
{
  Benchmarker bench(name);
  pcl::PointCloud<pcl::PointNormal> out;
  pcl::Indices indices;
  bench.run([&]() {
    pcl::removeNaNNormalsFromPointCloud(cloud, out, indices);
    bench.setChecksum(checksumIndices(indices) ^ checksumCloud(out));
    doNotOptimize(out);
    doNotOptimize(indices);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/filter_indices removeNaN benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ/PointNormal non-dense clouds; cases: 64K and 1M with sparse invalid entries\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto xyz_64k_sparse = makeXYZCloud(64 * 1024, 97);
  const auto xyz_1m_sparse = makeXYZCloud(1024 * 1024, 101);
  const auto xyz_1m_dense = makeXYZCloud(1024 * 1024, 0);
  const auto normal_1m_sparse = makeNormalCloud(1024 * 1024, 113);

  benchIndicesOnly("removeNaN indices-only sparse xyz 64K", xyz_64k_sparse);
  benchIndicesOnly("removeNaN indices-only sparse xyz 1M", xyz_1m_sparse);
  benchIndicesOnly("removeNaN indices-only dense xyz 1M", xyz_1m_dense);
  benchCloudOut("removeNaN cloud-out sparse xyz 1M", xyz_1m_sparse);
  benchNormals("removeNaNNormals cloud-out sparse normal 1M", normal_1m_sparse);

  printBanner('=');
  return 0;
}
