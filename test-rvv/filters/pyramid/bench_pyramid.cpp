#include <pcl/filters/pyramid.h>
#include <pcl/filters/impl/pyramid.hpp>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 3) const
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

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePointXYZCloud(const int width, const int height, const bool dense)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = dense;
  cloud->points.resize(static_cast<std::size_t>(width * height));
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      auto& p = cloud->at(c, r);
      p.x = static_cast<float>((c * 17 + r * 3) % 1021) / 511.0f;
      p.y = static_cast<float>((c * 5 + r * 13) % 997) / 499.0f;
      p.z = static_cast<float>((c * 11 + r * 7) % 991) / 487.0f;
    }
  }
  if (!dense) {
    for (int r = 3; r < height; r += 23)
      for (int c = 5; c < width; c += 31)
        cloud->at(c, r).x = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr
makePointXYZICloud(const int width, const int height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(static_cast<std::size_t>(width * height));
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      auto& p = cloud->at(c, r);
      p.x = static_cast<float>((c * 3 + r * 5) % 1021) / 600.0f;
      p.y = static_cast<float>((c * 7 + r * 2) % 997) / 580.0f;
      p.z = static_cast<float>((c * 11 + r * 13) % 991) / 560.0f;
      p.intensity = static_cast<float>((c + r) % 31);
    }
  }
  return cloud;
}

template <typename PointT>
std::uint64_t
checksumClouds(const std::vector<typename pcl::PointCloud<PointT>::Ptr>& output)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const auto& cloud : output) {
    for (const auto& p : cloud->points) {
      const auto ix = static_cast<std::int64_t>(std::llround(p.x * 100000.0f));
      const auto iy = static_cast<std::int64_t>(std::llround(p.y * 100000.0f));
      const auto iz = static_cast<std::int64_t>(std::llround(p.z * 100000.0f));
      sum = (sum ^ static_cast<std::uint64_t>(ix)) * 1099511628211ull;
      sum = (sum ^ static_cast<std::uint64_t>(iy)) * 1099511628211ull;
      sum = (sum ^ static_cast<std::uint64_t>(iz)) * 1099511628211ull;
    }
  }
  return sum;
}

template <typename PointT>
void
benchPyramid(const std::string& name,
             const typename pcl::PointCloud<PointT>::ConstPtr& cloud,
             const int levels,
             const bool large)
{
  Benchmarker bench(name);
  std::vector<typename pcl::PointCloud<PointT>::Ptr> output;
  bench.run([&]() {
    pcl::filters::Pyramid<PointT> pyramid(levels);
    pyramid.setInputCloud(cloud);
    pyramid.setLargeSmoothingKernel(large);
    pyramid.setDistanceThreshold(std::numeric_limits<float>::infinity());
    pyramid.compute(output);
    bench.setChecksum(checksumClouds<PointT>(output));
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << " PCL filters/pyramid Benchmark (Pyramid<PointXYZ> dense organized RVV path)\n";
  printBanner('=');
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic organized clouds; PointXYZ dense small/large kernel RVV cases and fallback cases\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';

  const auto cloud_vga = makePointXYZCloud(640, 480, true);
  const auto cloud_hd = makePointXYZCloud(1280, 720, true);
  const auto cloud_nondense = makePointXYZCloud(640, 480, false);
  const auto cloud_xyzi = makePointXYZICloud(640, 480);

  benchPyramid<pcl::PointXYZ>("pyramid pointxyz dense 640x480 small-kernel", cloud_vga, 3, false);
  benchPyramid<pcl::PointXYZ>("pyramid pointxyz dense 640x480 large-kernel", cloud_vga, 3, true);
  benchPyramid<pcl::PointXYZ>("pyramid pointxyz dense 1280x720 small-kernel", cloud_hd, 3, false);
  benchPyramid<pcl::PointXYZ>("pyramid pointxyz non-dense fallback 640x480", cloud_nondense, 3, false);
  benchPyramid<pcl::PointXYZI>("pyramid pointxyzi fallback 640x480", cloud_xyzi, 3, false);

  return 0;
}
