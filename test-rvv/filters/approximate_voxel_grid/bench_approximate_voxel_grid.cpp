#include "approximate_voxel_grid_diag.hpp"

#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
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
    std::cout << std::left << std::setw(68) << name_ << ": " << std::fixed
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
makeCloud(std::size_t n, bool with_invalid)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.0031f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.0029f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.0027f;
  }
  if (with_invalid) {
    for (std::size_t i = 3; i < n; i += 997)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t i = 67; i < n; i += 1231)
      cloud[i].y = std::numeric_limits<float>::infinity();
    for (std::size_t i = 129; i < n; i += 1879)
      cloud[i].z = -std::numeric_limits<float>::infinity();
  }
  return cloud;
}

void
benchLeafHash(const std::string& name,
              const pcl::PointCloud<pcl::PointXYZ>& cloud,
              const Eigen::Array3f& inverse_leaf_size)
{
  Benchmarker bench(name);
  std::vector<pcl_rvv_filters_approximate_voxel_grid::LeafHash> leaves;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_APPROXIMATE_VOXEL_GRID_RVV_DIAGNOSTIC)
    const bool used_rvv = pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesRVV(
        cloud, inverse_leaf_size, 512, leaves);
    if (!used_rvv)
      leaves = pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesStd(
          cloud, inverse_leaf_size, 512);
#else
    leaves =
        pcl_rvv_filters_approximate_voxel_grid::computeLeafHashesStd(cloud, inverse_leaf_size, 512);
#endif
    bench.setChecksum(pcl_rvv_filters_approximate_voxel_grid::checksumLeafHashes(leaves));
    doNotOptimize(leaves);
  });
}

void
benchFullDiagnostic(const std::string& name,
                    const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const Eigen::Array3f& inverse_leaf_size)
{
  Benchmarker bench(name);
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_APPROXIMATE_VOXEL_GRID_RVV_DIAGNOSTIC)
    const bool used_rvv = pcl_rvv_filters_approximate_voxel_grid::approximateVoxelGridPointXYZRVV(
        cloud, inverse_leaf_size, 512, output);
    if (!used_rvv)
      output = pcl_rvv_filters_approximate_voxel_grid::approximateVoxelGridPointXYZStd(
          cloud, inverse_leaf_size, 512);
#else
    output = pcl_rvv_filters_approximate_voxel_grid::approximateVoxelGridPointXYZStd(
        cloud, inverse_leaf_size, 512);
#endif
    bench.setChecksum(pcl_rvv_filters_approximate_voxel_grid::checksumCloud(output));
    doNotOptimize(output);
  });
}

void
benchProductionFilter(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  Benchmarker bench(name);
  pcl::ApproximateVoxelGrid<pcl::PointXYZ> filter;
  filter.setLeafSize(0.05f, 0.06f, 0.07f);
  filter.setInputCloud(cloud.makeShared());
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(pcl_rvv_filters_approximate_voxel_grid::checksumCloud(output));
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/approximate_voxel_grid RVV production + diagnostic\n";
#if defined(__RVV10__) && defined(PCL_APPROXIMATE_VOXEL_GRID_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV production + diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV production only (__RVV10__ enabled, diagnostic macro disabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; leaf-id/hash diagnostic, full diagnostic, and production filter RVV path\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const Eigen::Array3f inverse_leaf(4.0f, 3.0f, 5.0f);
  const auto cloud64k = makeCloud(64 * 1024, false);
  const auto cloud1m = makeCloud(1024 * 1024, false);
  const auto cloud1m_invalid = makeCloud(1024 * 1024, true);

  benchLeafHash("approx voxel leaf-hash diag 64K", cloud64k, inverse_leaf);
  benchLeafHash("approx voxel leaf-hash diag 1M", cloud1m, inverse_leaf);
  benchLeafHash("approx voxel finite leaf-hash diag 1M", cloud1m_invalid, inverse_leaf);
  benchFullDiagnostic("approx voxel full diag 64K", cloud64k, inverse_leaf);
  benchFullDiagnostic("approx voxel full diag 1M", cloud1m, inverse_leaf);
  benchFullDiagnostic("approx voxel finite full diag 1M", cloud1m_invalid, inverse_leaf);
  benchProductionFilter("approx voxel production pointxyz 64K", cloud64k);

  printBanner('=');
  return 0;
}
