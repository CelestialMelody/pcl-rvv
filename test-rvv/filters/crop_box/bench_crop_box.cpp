#include <pcl/filters/crop_box.h>
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
makeCloud(std::size_t n, bool dense)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = dense;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 1200.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 1300.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 1400.0f;
    if (!dense && i % 113 == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    if (!dense && i % 157 == 0)
      cloud[i].z = std::numeric_limits<float>::infinity();
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
    sum ^= static_cast<std::uint64_t>(std::fabs(cloud[i].x + cloud[i].y + cloud[i].z) * 100000.0f) + (i << 1);
  return sum;
}

void
configureBox(pcl::CropBox<pcl::PointXYZ>& filter, bool negative)
{
  filter.setMin(Eigen::Vector4f(-0.65f, -0.55f, -0.45f, 1.0f));
  filter.setMax(Eigen::Vector4f(0.75f, 0.60f, 0.50f, 1.0f));
  filter.setNegative(negative);
}

void
benchIndices(const std::string& name,
             const pcl::PointCloud<pcl::PointXYZ>& cloud,
             bool negative,
             bool extract_removed)
{
  Benchmarker bench(name);
  pcl::CropBox<pcl::PointXYZ> filter(extract_removed);
  filter.setInputCloud(cloud.makeShared());
  configureBox(filter, negative);
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
benchCloudOut(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  Benchmarker bench(name);
  pcl::CropBox<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  configureBox(filter, false);
  pcl::PointCloud<pcl::PointXYZ> out;
  bench.run([&]() {
    filter.filter(out);
    bench.setChecksum(checksumCloud(out));
    doNotOptimize(out);
  });
}

void
benchSubsetFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  auto subset = pcl::make_shared<pcl::Indices>();
  subset->reserve(cloud.size() / 2);
  for (std::size_t i = 1; i < cloud.size(); i += 2)
    subset->push_back(static_cast<int>(i));

  Benchmarker bench(name);
  pcl::CropBox<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setIndices(subset);
  configureBox(filter, false);
  pcl::Indices indices;
  bench.run([&]() {
    filter.filter(indices);
    bench.setChecksum(checksumIndices(indices));
    doNotOptimize(indices);
  });
}

void
benchTransformFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  Benchmarker bench(name);
  pcl::CropBox<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  configureBox(filter, false);
  filter.setTranslation(Eigen::Vector3f(0.1f, -0.2f, 0.05f));
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
  std::cout << "PCL filters/crop_box benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; dense identity CropBox RVV cases and fallback cases\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud_64k = makeCloud(64 * 1024, true);
  const auto cloud_1m = makeCloud(1024 * 1024, true);
  const auto cloud_1m_invalid = makeCloud(1024 * 1024, false);

  benchIndices("crop_box indices identity 64K", cloud_64k, false, false);
  benchIndices("crop_box indices identity 1M", cloud_1m, false, false);
  benchIndices("crop_box indices negative 1M", cloud_1m, true, false);
  benchIndices("crop_box indices removed 1M", cloud_1m, false, true);
  benchCloudOut("crop_box cloud-out identity 1M", cloud_1m);
  benchSubsetFallback("crop_box explicit subset fallback 1M", cloud_1m);
  benchIndices("crop_box non-dense fallback 1M", cloud_1m_invalid, false, false);
  benchTransformFallback("crop_box translation fallback 1M", cloud_1m);

  printBanner('=');
  return 0;
}
