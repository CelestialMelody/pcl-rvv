#include "grid_minimum_diag.hpp"

#include <pcl/filters/grid_minimum.h>
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
    std::cout << std::left << std::setw(62) << name_ << ": " << std::fixed
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

pcl::Indices
makeIndices(std::size_t n, bool subset)
{
  pcl::Indices indices;
  indices.reserve(subset ? n / 2 : n);
  for (std::size_t i = subset ? 1 : 0; i < n; i += subset ? 2 : 1)
    indices.push_back(static_cast<int>(i));
  return indices;
}

void
benchCellId(const std::string& name,
            const pcl::PointCloud<pcl::PointXYZ>& cloud,
            const pcl::Indices& indices,
            float inverse_resolution)
{
  Benchmarker bench(name);
  Eigen::Vector4f min_p;
  Eigen::Vector4f max_p;
  pcl_rvv_filters_grid_minimum::computeBoundsStd(cloud, indices, min_p, max_p);
  int min_b0 = 0;
  int min_b1 = 0;
  int div_x = 0;
  pcl_rvv_filters_grid_minimum::computeGridShape(
      min_p, max_p, inverse_resolution, min_b0, min_b1, div_x);
  std::vector<pcl_rvv_filters_grid_minimum::GridCell> cells;

  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_GRID_MINIMUM_RVV_DIAGNOSTIC)
    if (!pcl_rvv_filters_grid_minimum::computeGridCellsRVV(
            cloud, indices, inverse_resolution, min_b0, min_b1, div_x, cells))
      cells = pcl_rvv_filters_grid_minimum::computeGridCellsStd(
          cloud, indices, inverse_resolution, min_b0, min_b1, div_x);
#else
    cells = pcl_rvv_filters_grid_minimum::computeGridCellsStd(
        cloud, indices, inverse_resolution, min_b0, min_b1, div_x);
#endif
    bench.setChecksum(pcl_rvv_filters_grid_minimum::checksumGridCells(cells));
    doNotOptimize(cells);
  });
}

void
benchFullDiagnostic(const std::string& name,
                    const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const pcl::Indices& indices,
                    float inverse_resolution)
{
  Benchmarker bench(name);
  pcl::Indices out;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_GRID_MINIMUM_RVV_DIAGNOSTIC)
    if (!pcl_rvv_filters_grid_minimum::gridMinimumPointXYZRVV(
            cloud, indices, inverse_resolution, out))
      out = pcl_rvv_filters_grid_minimum::gridMinimumPointXYZStd(
          cloud, indices, inverse_resolution);
#else
    out = pcl_rvv_filters_grid_minimum::gridMinimumPointXYZStd(
        cloud, indices, inverse_resolution);
#endif
    bench.setChecksum(pcl_rvv_filters_grid_minimum::checksumIndices(out));
    doNotOptimize(out);
  });
}

void
benchProductionFilter(const std::string& name,
                      const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      float resolution)
{
  Benchmarker bench(name);
  pcl::GridMinimum<pcl::PointXYZ> filter(resolution);
  filter.setInputCloud(cloud.makeShared());
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    filter.filter(output);
    bench.setChecksum(static_cast<std::uint64_t>(output.size()));
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/grid_minimum RVV diagnostic\n";
#if defined(__RVV10__) && defined(PCL_GRID_MINIMUM_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV available, diagnostic macro disabled\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; GridMinimum cell-id diagnostic, full diagnostic, and unchanged production filter\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud64k = makeCloud(64 * 1024, false);
  const auto cloud1m = makeCloud(1024 * 1024, false);
  const auto cloud1m_invalid = makeCloud(1024 * 1024, true);
  const auto indices64k = makeIndices(cloud64k.size(), false);
  const auto indices1m = makeIndices(cloud1m.size(), false);
  const auto subset1m = makeIndices(cloud1m.size(), true);
  const auto indices1m_invalid = makeIndices(cloud1m_invalid.size(), false);

  benchCellId("grid minimum cell-id diag 64K", cloud64k, indices64k, 8.0f);
  benchCellId("grid minimum cell-id diag 1M", cloud1m, indices1m, 8.0f);
  benchCellId("grid minimum subset cell-id diag 1M", cloud1m, subset1m, 8.0f);
  benchCellId("grid minimum finite cell-id diag 1M", cloud1m_invalid, indices1m_invalid, 8.0f);
  benchFullDiagnostic("grid minimum full diag 64K", cloud64k, indices64k, 8.0f);
  benchFullDiagnostic("grid minimum full diag 1M", cloud1m, indices1m, 8.0f);
  benchFullDiagnostic("grid minimum finite full diag 1M", cloud1m_invalid, indices1m_invalid, 8.0f);
  benchProductionFilter("grid minimum production unchanged 64K", cloud64k, 0.125f);

  printBanner('=');
  return 0;
}
