#include <pcl/filters/plane_clipper3D.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
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

pcl::PointCloud<pcl::PointXYZ>
makeCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 1600.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 1700.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 1800.0f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>
makeCloudXYZI(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) / 100.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 110.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 120.0f;
    cloud[i].intensity = static_cast<float>(i % 17);
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

void
benchPointXYZ(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud, const Eigen::Vector4f& plane)
{
  Benchmarker bench(name);
  pcl::PlaneClipper3D<pcl::PointXYZ> clipper(plane);
  pcl::Indices clipped;
  bench.run([&]() {
    clipped.clear();
    clipper.clipPointCloud3D(cloud, clipped);
    bench.setChecksum(checksumIndices(clipped));
    doNotOptimize(clipped);
  });
}

void
benchSubsetFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud, const Eigen::Vector4f& plane)
{
  pcl::Indices subset;
  subset.reserve(cloud.size() / 2);
  for (std::size_t i = 1; i < cloud.size(); i += 2)
    subset.push_back(static_cast<int>(i));

  Benchmarker bench(name);
  pcl::PlaneClipper3D<pcl::PointXYZ> clipper(plane);
  pcl::Indices clipped;
  bench.run([&]() {
    clipped.clear();
    clipper.clipPointCloud3D(cloud, clipped, subset);
    bench.setChecksum(checksumIndices(clipped));
    doNotOptimize(clipped);
  });
}

void
benchPointXYZI(const std::string& name, const pcl::PointCloud<pcl::PointXYZI>& cloud, const Eigen::Vector4f& plane)
{
  Benchmarker bench(name);
  pcl::PlaneClipper3D<pcl::PointXYZI> clipper(plane);
  pcl::Indices clipped;
  bench.run([&]() {
    clipped.clear();
    clipper.clipPointCloud3D(cloud, clipped);
    bench.setChecksum(checksumIndices(clipped));
    doNotOptimize(clipped);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << " PCL filters/plane_clipper3D Benchmark (PlaneClipper3D<PointXYZ> full-cloud RVV path)\n";
  printBanner('=');
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ/PointXYZI clouds; full-cloud plane clipping RVV cases and subset fallback case\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';

  const auto cloud64k = makeCloud(64 * 1024);
  const auto cloud1m = makeCloud(1024 * 1024);
  const auto cloudXYZI = makeCloudXYZI(1024 * 1024);
  const Eigen::Vector4f balanced_plane(0.75f, -0.5f, 0.35f, -0.15f);
  const Eigen::Vector4f mostly_keep_plane(0.25f, 0.10f, -0.15f, 0.65f);

  benchPointXYZ("plane_clipper3D pointxyz balanced 64K", cloud64k, balanced_plane);
  benchPointXYZ("plane_clipper3D pointxyz balanced 1M", cloud1m, balanced_plane);
  benchPointXYZ("plane_clipper3D pointxyz mostly-keep 1M", cloud1m, mostly_keep_plane);
  benchSubsetFallback("plane_clipper3D subset fallback 1M", cloud1m, balanced_plane);
  benchPointXYZI("plane_clipper3D pointxyzi xyz-compatible 1M", cloudXYZI, balanced_plane);

  return 0;
}
