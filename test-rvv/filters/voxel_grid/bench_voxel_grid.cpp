#include <pcl/PCLPointCloud2.h>
#include <pcl/common/common.h>
#include <pcl/filters/voxel_grid.h>

#include <Eigen/Core>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kBenchmarkBannerWidth = 96;

void
printBanner(char ch, std::size_t width = kBenchmarkBannerWidth)
{
  std::cout << std::string(width, ch) << '\n';
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

  void run(const std::function<void()>& func, int iterations = 50, int warmup = 5) const
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
  }

private:
  std::string name_;
};

pcl::PCLPointField
makeFloatField(const std::string& name, std::uint32_t offset)
{
  pcl::PCLPointField field;
  field.name = name;
  field.offset = offset;
  field.datatype = pcl::PCLPointField::FLOAT32;
  field.count = 1;
  return field;
}

void
writeFloat(std::vector<std::uint8_t>& data, std::size_t base, std::uint32_t offset, float value)
{
  std::memcpy(data.data() + base + offset, &value, sizeof(value));
}

pcl::PCLPointCloud2::Ptr
makeCloud(std::size_t n, std::uint32_t point_step, std::uint32_t x_off, std::uint32_t y_off, std::uint32_t z_off)
{
  auto cloud = pcl::make_shared<pcl::PCLPointCloud2>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->is_bigendian = false;
  cloud->point_step = point_step;
  cloud->row_step = point_step * cloud->width;
  cloud->fields = {makeFloatField("x", x_off), makeFloatField("y", y_off), makeFloatField("z", z_off)};
  cloud->data.assign(n * point_step, std::uint8_t{0});

  std::mt19937 rng(2025u + static_cast<std::uint32_t>(n + point_step));
  std::uniform_real_distribution<float> dist(-250.0f, 250.0f);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t base = i * point_step;
    writeFloat(cloud->data, base, x_off, dist(rng) + static_cast<float>(i % 17));
    writeFloat(cloud->data, base, y_off, dist(rng) - static_cast<float>(i % 19));
    writeFloat(cloud->data, base, z_off, dist(rng) + static_cast<float>(i % 23));
  }
  return cloud;
}

pcl::PCLPointCloud2::Ptr
makeCloudWithDistance(std::size_t n)
{
  auto cloud = makeCloud(n, 20, 0, 4, 8);
  cloud->fields.push_back(makeFloatField("distance", 12));
  for (std::size_t i = 0; i < n; ++i) {
    const float distance = static_cast<float>(i % 101) * 0.01f;
    writeFloat(cloud->data, i * cloud->point_step, 12, distance);
  }
  return cloud;
}

void
runGetMinMax(const pcl::PCLPointCloud2ConstPtr& cloud)
{
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D<float>(cloud, 0, 1, 2, min_pt, max_pt);
  doNotOptimize(min_pt);
  doNotOptimize(max_pt);
}

pcl::Indices
makeStrideIndices(std::size_t n)
{
  pcl::Indices indices;
  indices.reserve(n / 2);
  for (std::size_t i = 1; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  return indices;
}

void
runGetMinMaxIndices(const pcl::PCLPointCloud2ConstPtr& cloud, const pcl::Indices& indices)
{
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D<float>(cloud, indices, 0, 1, 2, min_pt, max_pt);
  doNotOptimize(min_pt);
  doNotOptimize(max_pt);
}

void
runGetMinMaxDistance(const pcl::PCLPointCloud2ConstPtr& cloud, bool limit_negative)
{
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D<float, float>(cloud,
                                 0,
                                 1,
                                 2,
                                 "distance",
                                 0.25f,
                                 0.75f,
                                 min_pt,
                                 max_pt,
                                 limit_negative);
  doNotOptimize(min_pt);
  doNotOptimize(max_pt);
}

constexpr int kBenchmarkIterations = 30;

void
benchCase(const std::string& name,
          std::size_t n,
          std::uint32_t point_step,
          std::uint32_t x_off,
          std::uint32_t y_off,
          std::uint32_t z_off)
{
  const auto cloud = makeCloud(n, point_step, x_off, y_off, z_off);
  Benchmarker(name).run([&]() { runGetMinMax(cloud); }, kBenchmarkIterations);
}

void
benchIndicesCase(const std::string& name,
                 std::size_t n,
                 std::uint32_t point_step,
                 std::uint32_t x_off,
                 std::uint32_t y_off,
                 std::uint32_t z_off)
{
  const auto cloud = makeCloud(n, point_step, x_off, y_off, z_off);
  const auto indices = makeStrideIndices(n);
  Benchmarker(name).run([&]() { runGetMinMaxIndices(cloud, indices); }, kBenchmarkIterations);
}

void
benchDistanceCase(const std::string& name, std::size_t n, bool limit_negative)
{
  const auto cloud = makeCloudWithDistance(n);
  Benchmarker(name).run([&]() { runGetMinMaxDistance(cloud, limit_negative); }, kBenchmarkIterations);
}

} // namespace

int
main()
{
  printBanner('=');
#if defined(__RVV10__)
  std::cout << "PCL filters/voxel_grid getMinMax3D benchmark [RVV enabled]\n";
#else
  std::cout << "PCL filters/voxel_grid getMinMax3D benchmark [std]\n";
#endif
  std::cout << "Dataset: synthetic dense PCLPointCloud2 float xyz; cases: 64K packed, 1M packed, 1M padded, 1M indexed, 1M distance\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  benchCase("getMinMax3D dense packed xyz 64K", 64 * 1024, 16, 0, 4, 8);
  benchCase("getMinMax3D dense packed xyz 1M", 1024 * 1024, 16, 0, 4, 8);
  benchCase("getMinMax3D dense padded xyz 1M", 1024 * 1024, 32, 4, 16, 24);
  benchIndicesCase("getMinMax3D dense indexed xyz 1M", 1024 * 1024, 16, 0, 4, 8);
  benchDistanceCase("getMinMax3D dense distance xyz 1M", 1024 * 1024, false);
  benchDistanceCase("getMinMax3D dense distance negative xyz 1M", 1024 * 1024, true);

  printBanner('=');
  return 0;
}