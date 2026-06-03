#include <pcl/filters/fast_bilateral_omp.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct CaseConfig {
  std::string name;
  std::uint32_t width;
  std::uint32_t height;
  float sigma_s;
  float sigma_r;
  bool inject_non_finite;
  unsigned int threads;
};

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(const CaseConfig& cfg)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = cfg.width;
  cloud->height = cfg.height;
  cloud->is_dense = !cfg.inject_non_finite;
  cloud->points.resize(static_cast<std::size_t>(cfg.width) * cfg.height);

  for (std::uint32_t y = 0; y < cfg.height; ++y) {
    for (std::uint32_t x = 0; x < cfg.width; ++x) {
      const std::size_t idx = static_cast<std::size_t>(y) * cfg.width + x;
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      const float ripple = 0.025f * std::sin(fx * 0.07f) + 0.035f * std::cos(fy * 0.05f);
      (*cloud)[idx].x = fx * 0.01f;
      (*cloud)[idx].y = fy * 0.01f;
      (*cloud)[idx].z = 0.75f + 0.0015f * fx + 0.0025f * fy + ripple;
    }
  }

  if (cfg.inject_non_finite && cloud->size() > 1024) {
    for (std::size_t i = 37; i < cloud->size(); i += 997)
      (*cloud)[i].z = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t i = 211; i < cloud->size(); i += 1601)
      (*cloud)[i].z = std::numeric_limits<float>::infinity();
    for (std::size_t i = 503; i < cloud->size(); i += 2039)
      (*cloud)[i].z = -std::numeric_limits<float>::infinity();
  }

  return cloud;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto& point : cloud) {
    const int bucket = std::isfinite(point.z) ? static_cast<int>(std::lround(point.z * 100000.0f)) : 0;
    hash ^= static_cast<std::uint64_t>(bucket + 0x9e3779b9);
    hash *= 1099511628211ull;
  }
  return hash;
}

double
runCase(const CaseConfig& cfg, int iterations, std::uint64_t& checksum)
{
  const auto cloud = makeCloud(cfg);
  pcl::PointCloud<pcl::PointXYZ> output;
  checksum = 0;

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    pcl::FastBilateralFilterOMP<pcl::PointXYZ> filter(cfg.threads);
    filter.setInputCloud(cloud);
    filter.setSigmaS(cfg.sigma_s);
    filter.setSigmaR(cfg.sigma_r);
    filter.filter(output);
    checksum ^= checksumCloud(output) + static_cast<std::uint64_t>(i);
  }
  auto stop = std::chrono::steady_clock::now();

  return std::chrono::duration<double, std::milli>(stop - start).count();
}

} // namespace

int
main()
{
  const int iterations = 8;
  const std::vector<CaseConfig> cases = {
      {"fast_bilateral_omp organized 64x48 finite t2", 64, 48, 4.0f, 0.05f, false, 2},
      {"fast_bilateral_omp organized 160x120 finite t2", 160, 120, 5.0f, 0.05f, false, 2},
      {"fast_bilateral_omp organized 160x120 nonfinite t2", 160, 120, 5.0f, 0.05f, true, 2},
      {"fast_bilateral_omp organized 320x240 finite t2", 320, 240, 6.0f, 0.05f, false, 2},
      {"fast_bilateral_omp organized 320x240 nonfinite t2", 320, 240, 6.0f, 0.05f, true, 2},
      {"fast_bilateral_omp small fallback 7x5 t1", 7, 5, 3.0f, 0.05f, true, 1},
  };

  std::cout << "==== PCL filters/fast_bilateral_omp RVV bench ====\n";
  std::cout << "Dataset: synthetic organized PointXYZ depth images; OMP filter with finite and non-finite z cases\n";
  std::cout << "Iterations: " << iterations << "\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV\n";
#else
  std::cout << "Build: Std\n";
#endif

  for (const auto& cfg : cases) {
    std::uint64_t checksum = 0;
    const double total_ms = runCase(cfg, iterations, checksum);
    const double avg_ms = total_ms / static_cast<double>(iterations);
    std::cout << cfg.name << " : " << std::fixed << std::setprecision(6) << avg_ms << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(6) << total_ms
              << " ms, checksum: " << checksum
              << ", size: " << cfg.width << "x" << cfg.height
              << ", sigma_s: " << cfg.sigma_s
              << ", sigma_r: " << cfg.sigma_r
              << ", nonfinite: " << (cfg.inject_non_finite ? "yes" : "no")
              << ", threads: " << cfg.threads << "\n";
  }

  return 0;
}
