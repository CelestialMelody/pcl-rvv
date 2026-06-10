#include "extract_indices_diag.hpp"

#include <pcl/filters/extract_indices.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

constexpr int kBenchmarkIterations = 30;
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

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 5) const
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

pcl::PointCloud<pcl::PointXYZ>
makeCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.0031f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.0029f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.0027f;
  }
  return cloud;
}

pcl::Indices
makeSelected(std::size_t n, int stride, int offset = 0)
{
  pcl::Indices selected;
  selected.reserve(n / static_cast<std::size_t>(stride) + 1);
  for (std::size_t i = static_cast<std::size_t>(offset); i < n; i += static_cast<std::size_t>(stride))
    selected.push_back(static_cast<int>(i));
  return selected;
}

void
benchComplement(const std::string& name, std::size_t input_size, const pcl::Indices& selected)
{
  Benchmarker bench(name);
  pcl::Indices out;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_EXTRACT_INDICES_RVV_DIAGNOSTIC)
    if (!pcl_rvv_filters_extract_indices::complementBitmapRVV(input_size, selected, out))
      out = pcl_rvv_filters_extract_indices::complementSetDifferenceStd(input_size, selected);
#else
    out = pcl_rvv_filters_extract_indices::complementSetDifferenceStd(input_size, selected);
#endif
    bench.setChecksum(pcl_rvv_filters_extract_indices::checksumIndices(out));
    doNotOptimize(out);
  });
}

void
benchKeepOrganized(const std::string& name,
                   const pcl::PointCloud<pcl::PointXYZ>& cloud,
                   const pcl::Indices& selected,
                   bool negative,
                   float user_value)
{
  Benchmarker bench(name);
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_EXTRACT_INDICES_RVV_DIAGNOSTIC)
    if (!pcl_rvv_filters_extract_indices::keepOrganizedPointXYZRVV(
            cloud, selected, negative, user_value, output))
      output = pcl_rvv_filters_extract_indices::keepOrganizedPointXYZStd(
          cloud, selected, negative, user_value);
#else
    output =
        pcl_rvv_filters_extract_indices::keepOrganizedPointXYZStd(cloud, selected, negative, user_value);
#endif
    bench.setChecksum(pcl_rvv_filters_extract_indices::checksumCloudXYZ(output));
    doNotOptimize(output);
  });
}

void
benchProduction(const std::string& name,
                const pcl::PointCloud<pcl::PointXYZ>& cloud,
                const pcl::Indices& selected,
                bool negative,
                bool keep_organized)
{
  Benchmarker bench(name);
  auto cloud_ptr = cloud.makeShared();
  auto selected_ptr = pcl::make_shared<pcl::Indices>(selected);
  pcl::ExtractIndices<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud_ptr);
  filter.setIndices(selected_ptr);
  filter.setNegative(negative);
  filter.setKeepOrganized(keep_organized);
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(pcl_rvv_filters_extract_indices::checksumCloudXYZ(output));
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/extract_indices RVV diagnostic\n";
#if defined(__RVV10__) && defined(PCL_EXTRACT_INDICES_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV available, diagnostic macro disabled\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; ExtractIndices complement bitmap, keep_organized write, and unchanged production filter\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud64k = makeCloud(64 * 1024);
  const auto cloud1m = makeCloud(1024 * 1024);
  const auto sparse64k = makeSelected(cloud64k.size(), 8, 1);
  const auto sparse1m = makeSelected(cloud1m.size(), 8, 1);
  const auto half1m = makeSelected(cloud1m.size(), 2, 0);
  const auto dense1m = makeSelected(cloud1m.size(), 8, 0);

  benchComplement("extract indices complement diag sparse 64K", cloud64k.size(), sparse64k);
  benchComplement("extract indices complement diag sparse 1M", cloud1m.size(), sparse1m);
  benchComplement("extract indices complement diag half 1M", cloud1m.size(), half1m);
  benchKeepOrganized("extract indices keep organized positive diag 64K",
                     cloud64k,
                     sparse64k,
                     false,
                     std::numeric_limits<float>::quiet_NaN());
  benchKeepOrganized("extract indices keep organized positive diag 1M",
                     cloud1m,
                     sparse1m,
                     false,
                     std::numeric_limits<float>::quiet_NaN());
  benchKeepOrganized("extract indices keep organized negative diag 1M", cloud1m, dense1m, true, 42.0f);
  benchProduction("extract indices production unchanged negative 64K", cloud64k, sparse64k, true, false);
  benchProduction("extract indices production unchanged keep organized 64K", cloud64k, sparse64k, false, true);

  printBanner('=');
  return 0;
}
