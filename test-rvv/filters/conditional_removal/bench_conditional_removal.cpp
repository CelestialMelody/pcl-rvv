#include <pcl/filters/conditional_removal.h>
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
constexpr std::size_t kBenchmarkBannerWidth = 104;

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
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 1400.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 1500.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 1600.0f;
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
    cloud[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) / 95.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 105.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 115.0f;
    cloud[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

pcl::ConditionAnd<pcl::PointXYZ>::Ptr
singleFieldCondition(const std::string& field, pcl::ComparisonOps::CompareOp op, double value)
{
  pcl::ConditionAnd<pcl::PointXYZ>::Ptr condition(new pcl::ConditionAnd<pcl::PointXYZ>());
  condition->addComparison(pcl::FieldComparison<pcl::PointXYZ>::ConstPtr(
      new pcl::FieldComparison<pcl::PointXYZ>(field, op, value)));
  return condition;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const auto& p : cloud) {
    const auto xi = static_cast<std::uint32_t>(std::lround(p.x * 100000.0f));
    const auto yi = static_cast<std::uint32_t>(std::lround(p.y * 100000.0f));
    const auto zi = static_cast<std::uint32_t>(std::lround(p.z * 100000.0f));
    sum = (sum ^ xi) * 1099511628211ull;
    sum = (sum ^ yi) * 1099511628211ull;
    sum = (sum ^ zi) * 1099511628211ull;
  }
  return sum ^ static_cast<std::uint64_t>(cloud.size());
}

std::uint64_t
checksumRemoved(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int v : indices)
    sum = (sum ^ static_cast<std::uint32_t>(v)) * 1099511628211ull;
  return sum ^ static_cast<std::uint64_t>(indices.size());
}

void
benchSingleField(const std::string& name,
                 const pcl::PointCloud<pcl::PointXYZ>& cloud,
                 const std::string& field,
                 pcl::ComparisonOps::CompareOp op,
                 double value,
                 bool extract_removed)
{
  Benchmarker bench(name);
  pcl::ConditionalRemoval<pcl::PointXYZ> filter(extract_removed);
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(singleFieldCondition(field, op, value));
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    std::uint64_t checksum = checksumCloud(output);
    if (extract_removed)
      checksum ^= checksumRemoved(*filter.getRemovedIndices());
    bench.setChecksum(checksum);
    doNotOptimize(output);
  });
}

void
benchKeepOrganizedFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  Benchmarker bench(name);
  pcl::ConditionalRemoval<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(singleFieldCondition("z", pcl::ComparisonOps::GT, 0.05));
  filter.setKeepOrganized(true);
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(checksumCloud(output) ^ checksumRemoved(*filter.getRemovedIndices()));
    doNotOptimize(output);
  });
}

void
benchSubsetFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  pcl::IndicesPtr subset(new pcl::Indices);
  subset->reserve(cloud.size() / 2);
  for (std::size_t i = 1; i < cloud.size(); i += 2)
    subset->push_back(static_cast<int>(i));

  Benchmarker bench(name);
  pcl::ConditionalRemoval<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setIndices(subset);
  filter.setCondition(singleFieldCondition("z", pcl::ComparisonOps::GT, 0.05));
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(checksumCloud(output));
    doNotOptimize(output);
  });
}

void
benchXYZIIntensity(const std::string& name, const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  Benchmarker bench(name);
  pcl::ConditionAnd<pcl::PointXYZI>::Ptr condition(new pcl::ConditionAnd<pcl::PointXYZI>());
  condition->addComparison(pcl::FieldComparison<pcl::PointXYZI>::ConstPtr(
      new pcl::FieldComparison<pcl::PointXYZI>("intensity", pcl::ComparisonOps::GT, 4.0)));
  pcl::ConditionalRemoval<pcl::PointXYZI> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(condition);
  pcl::PointCloud<pcl::PointXYZI> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(output.size());
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << " PCL filters/conditional_removal Benchmark (single FLOAT32 FieldComparison RVV path)\n";
  printBanner('=');
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; full-cloud single FieldComparison<float> RVV cases and fallback cases\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';

  const auto cloud64k = makeCloud(64 * 1024);
  const auto cloud1m = makeCloud(1024 * 1024);
  const auto cloudXYZI = makeCloudXYZI(1024 * 1024);

  benchSingleField("conditional_removal single z>0.05 64K", cloud64k, "z", pcl::ComparisonOps::GT, 0.05, false);
  benchSingleField("conditional_removal single z>0.05 removed 1M", cloud1m, "z", pcl::ComparisonOps::GT, 0.05, true);
  benchSingleField("conditional_removal single y<=0.10 1M", cloud1m, "y", pcl::ComparisonOps::LE, 0.10, false);
  benchKeepOrganizedFallback("conditional_removal keep-organized fallback 1M", cloud1m);
  benchSubsetFallback("conditional_removal subset fallback 1M", cloud1m);
  benchXYZIIntensity("conditional_removal pointxyzi intensity>4 1M", cloudXYZI);

  return 0;
}
