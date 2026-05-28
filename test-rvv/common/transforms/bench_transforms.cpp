/*
 * PCL transforms.hpp RVV baseline / comparison benchmark.
 */

#include <pcl/common/transforms.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <utility>

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

  void run(const std::function<void()>& func, int iterations = 20, int warmup = 3) const
  {
    for (int i = 0; i < warmup; ++i)
      func();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    const double avg_ms = total_ms / iterations;
    std::cout << std::left << std::setw(56) << name_ << ": " << std::fixed
              << std::setprecision(4) << avg_ms << " ms/iter\n";
  }

private:
  std::string name_;
};

Eigen::Matrix4f
makeTransform()
{
  Eigen::Affine3f t = Eigen::Affine3f::Identity();
  t.translation() = Eigen::Vector3f(0.25f, -1.5f, 3.0f);
  t.linear() =
      (Eigen::AngleAxisf(0.31f, Eigen::Vector3f::UnitX()) *
       Eigen::AngleAxisf(-0.17f, Eigen::Vector3f::UnitY()) *
       Eigen::AngleAxisf(0.09f, Eigen::Vector3f::UnitZ()))
          .toRotationMatrix();
  return t.matrix();
}

void
fillXYZ(pcl::PointCloud<pcl::PointXYZ>& cloud, std::size_t n, std::uint32_t seed)
{
  cloud.clear();
  cloud.resize(n);
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> u(-20.0f, 20.0f);
  for (auto& p : cloud) {
    p.x = u(rng);
    p.y = u(rng);
    p.z = u(rng);
  }
}

void
fillXYZRGBNormal(pcl::PointCloud<pcl::PointXYZRGBNormal>& cloud,
                 std::size_t n,
                 std::uint32_t seed)
{
  cloud.clear();
  cloud.resize(n);
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> u(-20.0f, 20.0f);
  std::uniform_real_distribution<float> c(0.0f, 255.0f);
  for (auto& p : cloud) {
    p.x = u(rng);
    p.y = u(rng);
    p.z = u(rng);
    Eigen::Vector3f normal(u(rng), u(rng), u(rng));
    normal.normalize();
    p.normal_x = normal.x();
    p.normal_y = normal.y();
    p.normal_z = normal.z();
    p.r = static_cast<std::uint8_t>(c(rng));
    p.g = static_cast<std::uint8_t>(c(rng));
    p.b = static_cast<std::uint8_t>(c(rng));
  }
}

} // namespace

int
main(int argc, char** argv)
{
  std::size_t points = 1'000'000;
  int iterations = 20;

  if (argc >= 2)
    points = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
  if (argc >= 3)
    iterations = std::atoi(argv[2]);

  pcl::PointCloud<pcl::PointXYZ> xyz;
  pcl::PointCloud<pcl::PointXYZ> xyz_out;
  pcl::PointCloud<pcl::PointXYZRGBNormal> xyzn;
  pcl::PointCloud<pcl::PointXYZRGBNormal> xyzn_out;
  fillXYZ(xyz, points, 42);
  fillXYZRGBNormal(xyzn, points, 84);
  const Eigen::Matrix4f transform = makeTransform();

  printBanner('=');
  std::cout << " PCL transforms.hpp Benchmark\n";
  std::cout << " points: " << points << "\n";
  std::cout << " Dataset: PointXYZ/PointXYZRGBNormal dense cloud, points=" << points << "\n";
  std::cout << " iterations: " << iterations << "\n";
#if defined(__RVV10__)
  std::cout << " mode: RVV (__RVV10__ enabled)\n";
#else
  std::cout << " mode: Standard (__RVV10__ disabled)\n";
#endif
  printBanner('-');

  {
    Benchmarker b("transformPointCloud PointXYZ dense copy=true");
    b.run([&]() {
      pcl::transformPointCloud(xyz, xyz_out, transform, true);
      doNotOptimize(xyz_out[points / 2].x);
    }, iterations);
  }

  {
    Benchmarker b("transformPointCloud PointXYZ dense copy=false");
    b.run([&]() {
      pcl::transformPointCloud(xyz, xyz_out, transform, false);
      doNotOptimize(xyz_out[points / 2].y);
    }, iterations);
  }

  {
    pcl::PointCloud<pcl::PointXYZ> in_place = xyz;
    Benchmarker b("transformPointCloud PointXYZ dense in-place");
    b.run([&]() {
      in_place = xyz;
      pcl::transformPointCloud(in_place, in_place, transform, true);
      doNotOptimize(in_place[points / 2].z);
    }, std::max(1, iterations / 2));
  }

  {
    Benchmarker b("transformPointCloudWithNormals dense copy=true");
    b.run([&]() {
      pcl::transformPointCloudWithNormals(xyzn, xyzn_out, transform, true);
      doNotOptimize(xyzn_out[points / 2].normal_x);
    }, iterations);
  }

  {
    Benchmarker b("transformPointCloudWithNormals dense copy=false");
    b.run([&]() {
      pcl::transformPointCloudWithNormals(xyzn, xyzn_out, transform, false);
      doNotOptimize(xyzn_out[points / 2].normal_y);
    }, iterations);
  }

  printBanner('=');
  return 0;
}