#include <pcl/filters/frustum_culling.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
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

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(std::size_t n, bool dense = true)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = dense;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const float x = static_cast<float>(static_cast<int>(i % 4099) - 1024) / 220.0f;
    const float y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 500.0f;
    const float z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 520.0f;
    (*cloud)[i] = pcl::PointXYZ(x, y, z);
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr
makeCloudXYZI(std::size_t n)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 257) - 64) / 60.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 90.0f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 95.0f;
    (*cloud)[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

pcl::IndicesPtr
makeSubset(std::size_t n)
{
  auto indices = std::make_shared<pcl::Indices>();
  indices->reserve(n / 2);
  for (std::size_t i = 1; i < n; i += 2)
    indices->push_back(static_cast<int>(i));
  return indices;
}

template <typename PointT>
pcl::FrustumCulling<PointT>
configuredFrustum(bool extract_removed_indices = false)
{
  pcl::FrustumCulling<PointT> fc(extract_removed_indices);
  fc.setVerticalFOV(76.0f);
  fc.setHorizontalFOV(68.0f);
  fc.setNearPlaneDistance(0.05f);
  fc.setFarPlaneDistance(6.0f);
  fc.setRegionOfInterest(0.52f, 0.48f, 0.72f, 0.66f);

  Eigen::Matrix4f camera_pose = Eigen::Matrix4f::Identity();
  camera_pose.block<3, 1>(0, 3) = Eigen::Vector3f(-1.25f, 0.15f, -0.20f);
  fc.setCameraPose(camera_pose);
  return fc;
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
benchPointXYZ(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud, bool negative = false, bool extract_removed = false)
{
  Benchmarker bench(name);
  auto fc = configuredFrustum<pcl::PointXYZ>(extract_removed);
  fc.setInputCloud(cloud);
  fc.setNegative(negative);
  pcl::Indices indices;
  bench.run([&]() {
    fc.filter(indices);
    std::uint64_t checksum = checksumIndices(indices);
    if (extract_removed)
      checksum ^= checksumIndices(*fc.getRemovedIndices());
    bench.setChecksum(checksum);
    doNotOptimize(indices);
  });
}

void
benchSubsetFallback(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud)
{
  Benchmarker bench(name);
  auto fc = configuredFrustum<pcl::PointXYZ>();
  fc.setInputCloud(cloud);
  fc.setIndices(makeSubset(cloud->size()));
  pcl::Indices indices;
  bench.run([&]() {
    fc.filter(indices);
    bench.setChecksum(checksumIndices(indices));
    doNotOptimize(indices);
  });
}

void
benchPointXYZI(const std::string& name, const pcl::PointCloud<pcl::PointXYZI>::Ptr& cloud)
{
  Benchmarker bench(name);
  auto fc = configuredFrustum<pcl::PointXYZI>();
  fc.setInputCloud(cloud);
  pcl::Indices indices;
  bench.run([&]() {
    fc.filter(indices);
    bench.setChecksum(checksumIndices(indices));
    doNotOptimize(indices);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << " PCL filters/frustum_culling Benchmark (FrustumCulling<PointXYZ> full-cloud RVV path)\n";
  printBanner('=');
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ/PointXYZI clouds; full-cloud six-plane frustum RVV cases and subset fallback cases\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';

  const auto cloud64k = makeCloud(64 * 1024);
  const auto cloud1m = makeCloud(1024 * 1024);
  const auto cloud1m_nondense = makeCloud(1024 * 1024, false);
  const auto cloudXYZI = makeCloudXYZI(1024 * 1024);

  benchPointXYZ("frustum_culling pointxyz full-cloud 64K", cloud64k);
  benchPointXYZ("frustum_culling pointxyz full-cloud 1M", cloud1m);
  benchPointXYZ("frustum_culling pointxyz negative removed 1M", cloud1m, true, true);
  benchPointXYZ("frustum_culling nondense fallback 1M", cloud1m_nondense);
  benchSubsetFallback("frustum_culling subset fallback 1M", cloud1m);
  benchPointXYZI("frustum_culling pointxyzi xyz-compatible 1M", cloudXYZI);

  return 0;
}
