#include <pcl/filters/voxel_grid_covariance.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kBenchmarkIterations = 8;
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

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 2) const
  {
    for (int i = 0; i < warmup; ++i)
      func();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(64) << name_ << ": " << std::fixed
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

  for (std::size_t i = 0; i < n; ++i)
  {
    const float cell_x = static_cast<float>(static_cast<int>(i % 251) - 125) * 0.031f;
    const float cell_y = static_cast<float>(static_cast<int>((i / 251) % 239) - 119) * 0.029f;
    const float cell_z = static_cast<float>(static_cast<int>((i / (251 * 7)) % 233) - 116) * 0.027f;
    const float jitter = static_cast<float>(static_cast<int>((i * 17) % 17) - 8) * 0.0007f;
    cloud[i].x = cell_x + jitter;
    cloud[i].y = cell_y - jitter * 0.5f;
    cloud[i].z = cell_z + jitter * 0.25f;
    if (!dense && i % 997 == 0)
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
    if (!dense && i % 1231 == 0)
      cloud[i].z = std::numeric_limits<float>::infinity();
  }
  return cloud;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::vector<std::uint64_t> values;
  values.reserve(cloud.size());
  for (const auto& p : cloud)
  {
    const auto x = static_cast<std::uint64_t>(std::llround((p.x + 16.0f) * 100000.0f));
    const auto y = static_cast<std::uint64_t>(std::llround((p.y + 16.0f) * 100000.0f));
    const auto z = static_cast<std::uint64_t>(std::llround((p.z + 16.0f) * 100000.0f));
    values.push_back((x << 42) ^ (y << 21) ^ z);
  }
  std::sort(values.begin(), values.end());

  std::uint64_t sum = cloud.size();
  for (const auto v : values)
    sum = (sum ^ v) * 1099511628211ull;
  return sum;
}

void
configure(pcl::VoxelGridCovariance<pcl::PointXYZ>& grid)
{
  grid.setLeafSize(0.045f, 0.044f, 0.043f);
  grid.setMinPointPerVoxel(3);
}

void
benchFilter(const std::string& name,
            const pcl::PointCloud<pcl::PointXYZ>& cloud,
            bool save_leaf_layout,
            bool searchable,
            bool distance_filter)
{
  Benchmarker bench(name);
  pcl::VoxelGridCovariance<pcl::PointXYZ> grid;
  configure(grid);
  grid.setSaveLeafLayout(save_leaf_layout);
  if (distance_filter)
  {
    grid.setFilterFieldName("z");
    grid.setFilterLimits(-1.75f, 1.75f);
  }
  grid.setInputCloud(cloud.makeShared());
  pcl::PointCloud<pcl::PointXYZ> out;
  bench.run([&]() {
    grid.filter(out, searchable);
    bench.setChecksum(checksumCloud(out));
    doNotOptimize(out);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/voxel_grid_covariance benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; dense leaf-index RVV cases plus distance/non-dense fallback cases\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud_64k = makeCloud(64 * 1024, true);
  const auto cloud_256k = makeCloud(256 * 1024, true);
  const auto cloud_256k_invalid = makeCloud(256 * 1024, false);

  benchFilter("vgcov dense leaf-index 64K", cloud_64k, false, false, false);
  benchFilter("vgcov dense leaf-index 256K", cloud_256k, false, false, false);
  benchFilter("vgcov dense save-layout 256K", cloud_256k, true, false, false);
  benchFilter("vgcov dense searchable 64K", cloud_64k, false, true, false);
  benchFilter("vgcov distance-field fallback 256K", cloud_256k, false, false, true);
  benchFilter("vgcov non-dense fallback 256K", cloud_256k_invalid, false, false, false);

  printBanner('=');
  return 0;
}
