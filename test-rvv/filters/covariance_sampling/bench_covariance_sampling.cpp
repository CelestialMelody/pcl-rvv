#include "covariance_sampling_diag.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

constexpr int kIterations = 5;
constexpr std::size_t kBannerWidth = 100;

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

void
printBanner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void run(const std::function<void()>& fn, int iterations = kIterations, int warmup = 1) const
  {
    for (int i = 0; i < warmup; ++i)
      fn();
    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      fn();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
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

void
benchFull(const std::string& name,
          const pcl::PointCloud<pcl::PointXYZ>& cloud,
          const pcl::PointCloud<pcl::Normal>& normals,
          const pcl::Indices& indices,
          std::size_t num_samples,
          bool use_rvv_fragments)
{
  Benchmarker bench(name);
  pcl_rvv_filters_covariance_sampling::SamplingResult result;
  bench.run([&]() {
    result = pcl_rvv_filters_covariance_sampling::runDiagnostic(cloud, normals, indices, num_samples, use_rvv_fragments);
    bench.setChecksum(result.checksum);
    doNotOptimize(result);
  });
}

void
benchScaledOnly(const std::string& name,
                const pcl::PointCloud<pcl::PointXYZ>& cloud,
                const pcl::Indices& indices,
                bool use_rvv)
{
  Benchmarker bench(name);
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> scaled;
  Eigen::Vector3f centroid;
  double average_norm = 0.0;
  bench.run([&]() {
    if (use_rvv && !pcl_rvv_filters_covariance_sampling::computeScaledPointsRVV(
                       cloud, indices, scaled, centroid, average_norm))
      pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(cloud, indices, scaled, centroid, average_norm);
    else if (!use_rvv)
      pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(cloud, indices, scaled, centroid, average_norm);
    std::uint64_t checksum = pcl_rvv_filters_covariance_sampling::checksumVector3f(scaled);
    pcl_rvv_filters_covariance_sampling::mixDouble(checksum, average_norm);
    bench.setChecksum(checksum);
    doNotOptimize(scaled);
  });
}

void
benchVectorBuildOnly(const std::string& name,
                     const pcl::PointCloud<pcl::PointXYZ>& cloud,
                     const pcl::PointCloud<pcl::Normal>& normals,
                     const pcl::Indices& indices,
                     bool use_rvv)
{
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> scaled;
  Eigen::Vector3f centroid;
  double average_norm = 0.0;
  pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(cloud, indices, scaled, centroid, average_norm);

  Benchmarker bench(name);
  std::vector<pcl_rvv_filters_covariance_sampling::Vector6d,
              Eigen::aligned_allocator<pcl_rvv_filters_covariance_sampling::Vector6d>> vectors;
  bench.run([&]() {
    if (use_rvv && !pcl_rvv_filters_covariance_sampling::buildVectorsRVV(scaled, normals, indices, vectors))
      pcl_rvv_filters_covariance_sampling::buildVectorsStd(scaled, normals, indices, vectors);
    else if (!use_rvv)
      pcl_rvv_filters_covariance_sampling::buildVectorsStd(scaled, normals, indices, vectors);
    bench.setChecksum(pcl_rvv_filters_covariance_sampling::checksumVector6d(vectors));
    doNotOptimize(vectors);
  });
}

void
benchCovarianceSolverOnly(const std::string& name,
                          const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const pcl::PointCloud<pcl::Normal>& normals,
                          const pcl::Indices& indices)
{
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> scaled;
  Eigen::Vector3f centroid;
  double average_norm = 0.0;
  pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(cloud, indices, scaled, centroid, average_norm);
  std::vector<pcl_rvv_filters_covariance_sampling::Vector6d,
              Eigen::aligned_allocator<pcl_rvv_filters_covariance_sampling::Vector6d>> vectors;
  pcl_rvv_filters_covariance_sampling::buildVectorsStd(scaled, normals, indices, vectors);

  Benchmarker bench(name);
  bench.run([&]() {
    const auto covariance = pcl_rvv_filters_covariance_sampling::computeCovarianceFromVectors(vectors);
    const double condition = pcl_rvv_filters_covariance_sampling::conditionNumber(covariance);
    std::uint64_t checksum = 1469598103934665603ull;
    pcl_rvv_filters_covariance_sampling::mixDouble(checksum, condition);
    bench.setChecksum(checksum);
    doNotOptimize(covariance);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL filters/covariance_sampling RVV diagnostic\n";
#if defined(__RVV10__) && defined(PCL_COVARIANCE_SAMPLING_RVV_DIAGNOSTIC)
  std::cout << "Build: RVV diagnostic (__RVV10__ enabled)\n";
#elif defined(__RVV10__)
  std::cout << "Build: RVV available, diagnostic macro disabled\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ + Normal clouds; identity and shuffled indices; phase breakdown plus full diagnostic\n";
  std::cout << "Iterations: " << kIterations << '\n';
  printBanner('-');

  const auto cloud4k = pcl_rvv_filters_covariance_sampling::makePointCloud(4096);
  const auto normals4k = pcl_rvv_filters_covariance_sampling::makeNormalCloud(4096);
  const auto identity4k = pcl_rvv_filters_covariance_sampling::makeIndices(4096, false);
  const auto shuffled4k = pcl_rvv_filters_covariance_sampling::makeIndices(4096, true);
  const auto cloud16k = pcl_rvv_filters_covariance_sampling::makePointCloud(16384);
  const auto normals16k = pcl_rvv_filters_covariance_sampling::makeNormalCloud(16384);
  const auto identity16k = pcl_rvv_filters_covariance_sampling::makeIndices(16384, false);
  const auto shuffled16k = pcl_rvv_filters_covariance_sampling::makeIndices(16384, true);

#if defined(__RVV10__) && defined(PCL_COVARIANCE_SAMPLING_RVV_DIAGNOSTIC)
  constexpr bool kUseRVV = true;
#else
  constexpr bool kUseRVV = false;
#endif

  benchScaledOnly("covariance_sampling scaled-points identity 4K", cloud4k, identity4k, kUseRVV);
  benchScaledOnly("covariance_sampling scaled-points shuffled 4K", cloud4k, shuffled4k, kUseRVV);
  benchVectorBuildOnly("covariance_sampling 6D-vector identity 4K", cloud4k, normals4k, identity4k, kUseRVV);
  benchVectorBuildOnly("covariance_sampling 6D-vector shuffled 4K", cloud4k, normals4k, shuffled4k, kUseRVV);
  benchCovarianceSolverOnly("covariance_sampling covariance+solver identity 4K", cloud4k, normals4k, identity4k);
  benchFull("covariance_sampling full apply identity 4K sample 512", cloud4k, normals4k, identity4k, 512, kUseRVV);
  benchFull("covariance_sampling full apply shuffled 4K sample 512", cloud4k, normals4k, shuffled4k, 512, kUseRVV);
  benchScaledOnly("covariance_sampling scaled-points identity 16K", cloud16k, identity16k, kUseRVV);
  benchVectorBuildOnly("covariance_sampling 6D-vector identity 16K", cloud16k, normals16k, identity16k, kUseRVV);
  benchCovarianceSolverOnly("covariance_sampling covariance+solver identity 16K", cloud16k, normals16k, identity16k);
  benchFull("covariance_sampling full apply identity 16K sample 1024", cloud16k, normals16k, identity16k, 1024, kUseRVV);
  benchFull("covariance_sampling full apply shuffled 16K sample 1024", cloud16k, normals16k, shuffled16k, 1024, kUseRVV);

  printBanner('=');
  return 0;
}
