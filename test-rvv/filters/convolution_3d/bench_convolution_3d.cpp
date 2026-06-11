#include "convolution_3d_diag.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

constexpr int kBenchmarkIterations = 5;
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

  void run(const std::function<void()>& func, int iterations = kBenchmarkIterations, int warmup = 2) const
  {
    for (int i = 0; i < warmup; ++i)
      func();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << std::left << std::setw(70) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t checksum) const { checksum_ = checksum; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

void
printErrorStats(const pcl_rvv_filters_convolution_3d::ErrorStats& stats)
{
  std::cout << "  Error: max_abs=" << std::scientific << std::setprecision(6) << stats.max_abs
            << ", max_rel=" << stats.max_rel
            << ", rmse=" << stats.rmse << std::fixed << std::setprecision(4) << '\n';
}

void
benchRadiusSearchOnly(const std::string& name,
                      const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      const pcl::Indices& query_indices,
                      double radius)
{
  Benchmarker bench(name);
  pcl::search::KdTree<pcl::PointXYZ> tree;
  tree.setInputCloud(cloud.makeShared());
  pcl::Indices nn_indices;
  std::vector<float> nn_distances;
  bench.run([&]() {
    std::uint64_t checksum = 1469598103934665603ull;
    for (const int id : query_indices) {
      if (id < 0 || static_cast<std::size_t>(id) >= cloud.size() ||
          !pcl_rvv_filters_convolution_3d::isFinitePoint(cloud[static_cast<std::size_t>(id)]))
        continue;
      tree.radiusSearch(cloud[static_cast<std::size_t>(id)], radius, nn_indices, nn_distances);
      checksum ^= static_cast<std::uint64_t>(nn_indices.size());
      checksum *= 1099511628211ull;
    }
    bench.setChecksum(checksum);
    doNotOptimize(nn_indices);
    doNotOptimize(nn_distances);
  });
}

void
benchKernelOnly(const std::string& name,
                const pcl::PointCloud<pcl::PointXYZ>& cloud,
                const pcl::Indices& neighbors,
                const std::vector<float>& distances,
                float sigma_sqr,
                float threshold)
{
  Benchmarker bench(name);
  pcl_rvv_filters_convolution_3d::KernelResult result;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
    if (!pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZRVV(
            cloud, neighbors, distances, sigma_sqr, threshold, result))
      result = pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZStd(
          cloud, neighbors, distances, sigma_sqr, threshold);
#else
    result = pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZStd(
        cloud, neighbors, distances, sigma_sqr, threshold);
#endif
    bench.setChecksum(pcl_rvv_filters_convolution_3d::checksumKernelResult(result));
    doNotOptimize(result);
  });
}

void
benchThresholdOnly(const std::string& name,
                   const std::vector<float>& distances,
                   float threshold)
{
  Benchmarker bench(name);
  bench.run([&]() {
    std::uint64_t count = 0;
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
    std::size_t offset = 0;
    while (offset < distances.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(distances.size() - offset);
      const vfloat32m2_t v_dist = __riscv_vle32_v_f32m2(distances.data() + offset, vl);
      const vbool16_t mask = __riscv_vmfle_vf_f32m2_b16(v_dist, threshold, vl);
      count += __riscv_vcpop_m_b16(mask, vl);
      offset += vl;
    }
#else
    for (const float distance : distances) {
      if (distance <= threshold)
        ++count;
    }
#endif
    bench.setChecksum(count);
    doNotOptimize(count);
  });
}

void
benchGatherFiniteOnly(const std::string& name,
                      const pcl::PointCloud<pcl::PointXYZ>& cloud,
                      const pcl::Indices& neighbors)
{
  Benchmarker bench(name);
  bench.run([&]() {
    std::uint64_t checksum = 1469598103934665603ull;
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
    const auto* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
    const auto* raw_indices = reinterpret_cast<const std::uint32_t*>(neighbors.data());
    std::size_t offset = 0;
    while (offset < neighbors.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(neighbors.size() - offset);
      const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
      const vuint32m2_t v_point_offsets =
          pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_ids, vl);
      const vfloat32m2_t vx =
          pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, x)>(
              base_u8, v_point_offsets, vl);
      const vfloat32m2_t vy =
          pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, y)>(
              base_u8, v_point_offsets, vl);
      const vfloat32m2_t vz =
          pcl::rvv_load::gather_load_f32m2<pcl::PointXYZ, offsetof(pcl::PointXYZ, z)>(
              base_u8, v_point_offsets, vl);
      const vbool16_t finite =
          __riscv_vmand_mm_b16(__riscv_vmfeq_vv_f32m2_b16(vx, vx, vl),
                               __riscv_vmand_mm_b16(__riscv_vmfeq_vv_f32m2_b16(vy, vy, vl),
                                                    __riscv_vmfeq_vv_f32m2_b16(vz, vz, vl),
                                                    vl),
                               vl);
      checksum ^= __riscv_vcpop_m_b16(finite, vl);
      checksum *= 1099511628211ull;
      offset += vl;
    }
#else
    for (const int id : neighbors) {
      if (id >= 0 && static_cast<std::size_t>(id) < cloud.size() &&
          pcl_rvv_filters_convolution_3d::isFinitePoint(cloud[static_cast<std::size_t>(id)]))
        ++checksum;
      checksum *= 1099511628211ull;
    }
#endif
    bench.setChecksum(checksum);
    doNotOptimize(checksum);
  });
}

void
benchWeightOnly(const std::string& name,
                const std::vector<float>& distances,
                float sigma_sqr)
{
  Benchmarker bench(name);
  const float scale = -0.5f / sigma_sqr;
  bench.run([&]() {
    float sum = 0.0f;
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
    std::size_t offset = 0;
    while (offset < distances.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(distances.size() - offset);
      const vfloat32m2_t v_dist = __riscv_vle32_v_f32m2(distances.data() + offset, vl);
      const vfloat32m2_t v_weight =
          pcl::expf_RVV_f32m2(__riscv_vfmul_vf_f32m2(v_dist, scale, vl), vl);
      alignas(64) float weights[64];
      __riscv_vse32_v_f32m2(weights, v_weight, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        sum += weights[lane];
      offset += vl;
    }
#else
    for (const float distance : distances)
      sum += std::exp(scale * distance);
#endif
    bench.setChecksum(static_cast<std::uint64_t>(sum * 1000000.0f));
    doNotOptimize(sum);
  });
}

void
benchFullDiagnostic(const std::string& name,
                    const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const pcl::Indices& query_indices,
                    double radius,
                    float sigma_sqr,
                    float threshold)
{
  Benchmarker bench(name);
  const auto expected =
      pcl_rvv_filters_convolution_3d::convolveDiagnosticStd(cloud, query_indices, radius, sigma_sqr, threshold);
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
    output = pcl_rvv_filters_convolution_3d::convolveDiagnosticRVV(
        cloud, query_indices, radius, sigma_sqr, threshold);
#else
    output = pcl_rvv_filters_convolution_3d::convolveDiagnosticStd(
        cloud, query_indices, radius, sigma_sqr, threshold);
#endif
    bench.setChecksum(pcl_rvv_filters_convolution_3d::checksumCloud(output));
    doNotOptimize(output);
  });
  printErrorStats(pcl_rvv_filters_convolution_3d::compareCloud(expected, output));
}

void
benchProductionUnchanged(const std::string& name,
                         const pcl::PointCloud<pcl::PointXYZ>& cloud,
                         double radius,
                         float sigma)
{
  Benchmarker bench(name);
  pcl::PointCloud<pcl::PointXYZ> output;
  bench.run([&]() {
    output = pcl_rvv_filters_convolution_3d::convolveProductionUnchanged(cloud, radius, sigma);
    bench.setChecksum(pcl_rvv_filters_convolution_3d::checksumCloud(output));
    doNotOptimize(output);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/convolution_3d RVV diagnostic\n";
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV available, diagnostic macro disabled\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ 3D grids; radiusSearch, GaussianKernel kernel-only, full diagnostic, unchanged production entry\n";
  std::cout << "Iterations: " << kBenchmarkIterations << '\n';
  printBanner('-');

  const auto cloud8k = pcl_rvv_filters_convolution_3d::makeCloud(20, 20, 20, false);
  const auto cloud32k = pcl_rvv_filters_convolution_3d::makeCloud(32, 32, 32, false);
  const auto cloud32k_invalid = pcl_rvv_filters_convolution_3d::makeCloud(32, 32, 32, true);
  const auto query8k = pcl_rvv_filters_convolution_3d::makeIndices(cloud8k.size(), false);
  const auto query32k = pcl_rvv_filters_convolution_3d::makeIndices(cloud32k.size(), false);
  const auto subset32k = pcl_rvv_filters_convolution_3d::makeIndices(cloud32k.size(), true);
  const auto query32k_invalid = pcl_rvv_filters_convolution_3d::makeIndices(cloud32k_invalid.size(), false);

  pcl::Indices neighbors;
  std::vector<float> distances;
  neighbors.reserve(1024);
  distances.reserve(1024);
  for (int i = 0; i < 1024; ++i) {
    neighbors.push_back(i % static_cast<int>(cloud8k.size()));
    distances.push_back(static_cast<float>((i % 127) + 1) * 0.00008f);
  }

  const double radius = 0.075;
  const float sigma = 0.045f;
  const float sigma_sqr = sigma * sigma;
  const float threshold = 6.0f * 6.0f * sigma_sqr;

  benchRadiusSearchOnly("convolution_3d radiusSearch only 8K", cloud8k, query8k, radius);
  benchRadiusSearchOnly("convolution_3d radiusSearch only 32K", cloud32k, query32k, radius);
  benchThresholdOnly("convolution_3d kernel distance-threshold 1024", distances, threshold);
  benchGatherFiniteOnly("convolution_3d kernel gather-finite 1024", cloud8k, neighbors);
  benchWeightOnly("convolution_3d kernel exp-weight 1024", distances, sigma_sqr);
  benchKernelOnly("convolution_3d kernel-only Gaussian 1024", cloud8k, neighbors, distances, sigma_sqr, threshold);
  benchFullDiagnostic("convolution_3d full diag 8K", cloud8k, query8k, radius, sigma_sqr, threshold);
  benchFullDiagnostic("convolution_3d full diag 32K", cloud32k, query32k, radius, sigma_sqr, threshold);
  benchFullDiagnostic("convolution_3d subset full diag 32K", cloud32k, subset32k, radius, sigma_sqr, threshold);
  benchFullDiagnostic("convolution_3d finite full diag 32K", cloud32k_invalid, query32k_invalid, radius, sigma_sqr, threshold);
  benchProductionUnchanged("convolution_3d production unchanged 8K", cloud8k, radius, sigma);

  printBanner('=');
  return 0;
}
