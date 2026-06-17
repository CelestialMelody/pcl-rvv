#include "transformation_validation_euclidean_diag.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace diag = pcl::registration::transformation_validation_euclidean_diag;

namespace {

constexpr int kIterations = 20;
constexpr std::size_t kBannerWidth = 104;

void
printBanner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
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

  void run(const std::function<void()>& func, int iterations = kIterations, int warmup = 3) const
  {
    for (int i = 0; i < warmup; ++i)
      func();

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto stop = std::chrono::steady_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(stop - start).count();
    std::cout << std::left << std::setw(72) << name_ << ": " << std::fixed
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
makeCloud(std::size_t n)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 200.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 220.0f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 240.0f;
  }
  return cloud;
}

diag::Matrix4f
makeTransform()
{
  diag::Matrix4f t = diag::Matrix4f::Identity();
  t(0, 0) = 0.9848077f;
  t(0, 1) = -0.1736482f;
  t(1, 0) = 0.1736482f;
  t(1, 1) = 0.9848077f;
  t(0, 3) = 0.075f;
  t(1, 3) = -0.045f;
  t(2, 3) = 0.030f;
  return t;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t i = 0; i < cloud.size(); i += 17) {
    const auto& p = cloud[i];
    const auto bucket = static_cast<std::int64_t>(
        std::llround((p.x * 3.0f + p.y * 5.0f + p.z * 7.0f) * 100000.0f));
    hash ^= static_cast<std::uint64_t>(bucket);
    hash *= 1099511628211ull;
  }
  return hash ^ cloud.size();
}

std::uint64_t
checksumScore(double score)
{
  return static_cast<std::uint64_t>(std::llround(score * 1000000000.0));
}

void
benchTransform(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>& source)
{
  const auto transform = makeTransform();
  pcl::PointCloud<pcl::PointXYZ> transformed;
  Benchmarker bench(name);
  bench.run([&]() {
    diag::transformPointXYZCandidate(source, transformed, transform);
    bench.setChecksum(checksumCloud(transformed));
    doNotOptimize(transformed);
  });
}

void
benchFullValidation(const std::string& name, const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source)
{
  const auto transform = makeTransform();
  auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  diag::transformPointXYZStd(*source, *target, transform);
  Benchmarker bench(name);
  bench.run([&]() {
    const double score = diag::validateTransformationCandidate(source, target, transform, 1.0);
    bench.setChecksum(checksumScore(score));
    doNotOptimize(score);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL registration/transformation_validation_euclidean diagnostic benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ clouds; transform staging microbench and full KdTree validation diagnostic\n";
  std::cout << "Iterations: " << kIterations << '\n';
  printBanner('-');

  const auto source_64k = makeCloud(64 * 1024);
  const auto source_256k = makeCloud(256 * 1024);

  benchTransform("tve transform-staging pointxyz 64K", *source_64k);
  benchTransform("tve transform-staging pointxyz 256K", *source_256k);
  benchFullValidation("tve full-validation pointxyz 64K", source_64k);
  benchFullValidation("tve full-validation pointxyz 256K", source_256k);

  printBanner('=');
  return 0;
}
