/*
 * 本文件做什么：
 * 这是 transformation_estimation_svd 的 QEMU bench smoke（仿真器性能测试烟测）和后续
 * board bench（板卡性能测试）入口。它比较当前 public Umeyama 路径与 test-only fused
 * candidate 的日志形状和 checksum（校验和）。Phase 030 额外提供 source-indexed-cloud-pair
 * （源索引点云对）case-filter，用于独立判断 indexed gather 成本。
 *
 * 证据边界：
 * QEMU timing（QEMU 计时）只证明构建、路径和输出格式；真实性能结论必须来自 board /
 * target hardware repeated benchmark（板卡或目标硬件重复性能测试）。
 */

#include "tesvd.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace support = pcl::registration::rvv_tesvd_support;

namespace {

constexpr int kDefaultIterations = 20;
constexpr int kDefaultWarmupIterations = 3;
constexpr std::size_t kBannerWidth = 96;

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
printBanner(const char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

int
parseIntArg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc,
            char** argv,
            const std::string& requested,
            const std::string& legacy = "")
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter")
      return std::string(argv[i + 1]) == requested ||
             (!legacy.empty() && std::string(argv[i + 1]) == legacy) ||
             std::string(argv[i + 1]) == "all";
  }
  return true;
}

void
runCase(const std::string& name,
        const int iterations,
        const int warmup_iterations,
        const std::function<std::uint64_t()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    const std::uint64_t value = fn();
    checksum = (checksum ^ (value + static_cast<std::uint64_t>(i + 1))) *
               1099511628211ull;
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    const std::uint64_t value = fn();
    checksum = (checksum ^ (value + static_cast<std::uint64_t>(warmup_iterations + i + 1))) *
               1099511628211ull;
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(60) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}

template <typename CloudT>
void
runPublicCase(const std::string& label,
              const CloudT& source,
              const CloudT& target,
              const int iterations,
              const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimatePublicUmeyama(source, target));
  });
}

template <typename CloudT>
void
runPublicSourceIndexedCase(const std::string& label,
                           const CloudT& source,
                           const pcl::Indices& indices,
                           const CloudT& target,
                           const int iterations,
                           const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicSourceIndexedUmeyama(source, indices, target));
  });
}

template <typename CloudT>
void
runFusedCase(const std::string& label,
             const CloudT& source,
             const CloudT& target,
             const int iterations,
             const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix = support::estimateFusedCandidate(source, target, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runFusedSourceIndexedCase(const std::string& label,
                          const CloudT& source,
                          const pcl::Indices& indices,
                          const CloudT& target,
                          const int iterations,
                          const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFusedSourceIndexedCandidate(source, indices, target, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runPublicDualIndicesCase(const std::string& label,
                         const CloudT& source,
                         const pcl::Indices& source_indices,
                         const CloudT& target,
                         const pcl::Indices& target_indices,
                         const int iterations,
                         const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimatePublicDualIndicesUmeyama(
        source, source_indices, target, target_indices));
  });
}

template <typename CloudT>
void
runFusedDualIndicesCase(const std::string& label,
                        const CloudT& source,
                        const pcl::Indices& source_indices,
                        const CloudT& target,
                        const pcl::Indices& target_indices,
                        const int iterations,
                        const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix = support::estimateFusedDualIndicesCandidate(
        source, source_indices, target, target_indices, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runPublicCorrespondenceCase(const std::string& label,
                            const CloudT& source,
                            const CloudT& target,
                            const pcl::Correspondences& correspondences,
                            const int iterations,
                            const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicCorrespondencesUmeyama(source, target, correspondences));
  });
}

template <typename CloudT>
void
runFusedCorrespondenceCase(const std::string& label,
                           const CloudT& source,
                           const CloudT& target,
                           const pcl::Correspondences& correspondences,
                           const int iterations,
                           const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix = support::estimateFusedCorrespondencesCandidate(
        source, target, correspondences, &stats);
    return support::matrixChecksum(matrix);
  });
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const auto transform = support::makeRigidTransform();
  const auto small = support::makePointXYZCloud(4096);
  const auto medium = support::makePointXYZCloud(65536);
  const auto large = support::makePointXYZCloud(262144);
  const auto small_target = support::transformCloudXYZ(small, transform);
  const auto medium_target = support::transformCloudXYZ(medium, transform);
  const auto large_target = support::transformCloudXYZ(large, transform);
  const auto small_indexed_source = support::makePointXYZCloud(8193);
  const auto medium_indexed_source = support::makePointXYZCloud(131073);
  const auto large_indexed_source = support::makePointXYZCloud(524289);
  const auto small_indices =
      support::makeSourceIndices(small_indexed_source.size(), 4096);
  const auto medium_indices =
      support::makeSourceIndices(medium_indexed_source.size(), 65536);
  const auto large_indices =
      support::makeSourceIndices(large_indexed_source.size(), 262144);
  const auto small_indexed_target = support::transformCloudXYZBySourceIndices(
      small_indexed_source, small_indices, transform);
  const auto medium_indexed_target = support::transformCloudXYZBySourceIndices(
      medium_indexed_source, medium_indices, transform);
  const auto large_indexed_target = support::transformCloudXYZBySourceIndices(
      large_indexed_source, large_indices, transform);
  const auto dual_small_source_indices =
      support::makeSourceIndices(small_indexed_source.size(), 4096);
  const auto dual_medium_source_indices =
      support::makeSourceIndices(medium_indexed_source.size(), 65536);
  const auto dual_large_source_indices =
      support::makeSourceIndices(large_indexed_source.size(), 262144);
  const auto dual_small_target_indices =
      support::makeTargetIndices(small_indexed_source.size(), 4096);
  const auto dual_medium_target_indices =
      support::makeTargetIndices(medium_indexed_source.size(), 65536);
  const auto dual_large_target_indices =
      support::makeTargetIndices(large_indexed_source.size(), 262144);
  const auto dual_small_target = support::transformCloudXYZByIndexedPairs(
      small_indexed_source, dual_small_source_indices, dual_small_target_indices, transform);
  const auto dual_medium_target = support::transformCloudXYZByIndexedPairs(
      medium_indexed_source, dual_medium_source_indices, dual_medium_target_indices, transform);
  const auto dual_large_target = support::transformCloudXYZByIndexedPairs(
      large_indexed_source, dual_large_source_indices, dual_large_target_indices, transform);
  const auto dual_small_correspondences =
      support::makeCorrespondences(dual_small_source_indices, dual_small_target_indices);
  const auto dual_medium_correspondences =
      support::makeCorrespondences(dual_medium_source_indices, dual_medium_target_indices);
  const auto dual_large_correspondences =
      support::makeCorrespondences(dual_large_source_indices, dual_large_target_indices);

  printBanner('=');
  std::cout << "PCL registration/transformation_estimation_svd RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic dense PointXYZ ordered-cloud-pair pairs; SVD no-scale rigid transform\n";
  std::cout << "Indexed Dataset: synthetic dense PointXYZ source-indexed-cloud-pair pairs\n";
  std::cout << "Dual-Indexed Dataset: synthetic dense PointXYZ dual-indices-cloud-pair pairs\n";
  std::cout << "Correspondence Dataset: synthetic dense PointXYZ correspondence-pair pairs\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  printBanner('-');

  if (caseEnabled(argc, argv, "public-umeyama")) {
    runPublicCase("public Umeyama ordered-cloud-pair 4K",
                  small,
                  small_target,
                  iterations,
                  warmup_iterations);
    runPublicCase("public Umeyama ordered-cloud-pair 64K",
                  medium,
                  medium_target,
                  iterations,
                  warmup_iterations);
    runPublicCase("public Umeyama ordered-cloud-pair 256K",
                  large,
                  large_target,
                  iterations,
                  warmup_iterations);
  }

  if (caseEnabled(argc, argv, "ordered-cloud-pair", "fused-full-cloud")) {
    runFusedCase("fused accum candidate ordered-cloud-pair 4K",
                 small,
                 small_target,
                 iterations,
                 warmup_iterations);
    runFusedCase("fused accum candidate ordered-cloud-pair 64K",
                 medium,
                 medium_target,
                 iterations,
                 warmup_iterations);
    runFusedCase("fused accum candidate ordered-cloud-pair 256K",
                 large,
                 large_target,
                 iterations,
                 warmup_iterations);
  }

  if (caseEnabled(argc, argv, "source-indexed-cloud-pair", "source-indexed")) {
    runPublicSourceIndexedCase("public Umeyama source-indexed-cloud-pair 4K",
                               small_indexed_source,
                               small_indices,
                               small_indexed_target,
                               iterations,
                               warmup_iterations);
    runPublicSourceIndexedCase("public Umeyama source-indexed-cloud-pair 64K",
                               medium_indexed_source,
                               medium_indices,
                               medium_indexed_target,
                               iterations,
                               warmup_iterations);
    runPublicSourceIndexedCase("public Umeyama source-indexed-cloud-pair 256K",
                               large_indexed_source,
                               large_indices,
                               large_indexed_target,
                               iterations,
                               warmup_iterations);
    runFusedSourceIndexedCase("fused accum candidate source-indexed-cloud-pair 4K",
                              small_indexed_source,
                              small_indices,
                              small_indexed_target,
                              iterations,
                              warmup_iterations);
    runFusedSourceIndexedCase("fused accum candidate source-indexed-cloud-pair 64K",
                              medium_indexed_source,
                              medium_indices,
                              medium_indexed_target,
                              iterations,
                              warmup_iterations);
    runFusedSourceIndexedCase("fused accum candidate source-indexed-cloud-pair 256K",
                              large_indexed_source,
                              large_indices,
                              large_indexed_target,
                              iterations,
                              warmup_iterations);
  }

  if (caseEnabled(argc, argv, "dual-indices-cloud-pair", "dual-indices")) {
    runPublicDualIndicesCase("public Umeyama dual-indices-cloud-pair 4K",
                             small_indexed_source,
                             dual_small_source_indices,
                             dual_small_target,
                             dual_small_target_indices,
                             iterations,
                             warmup_iterations);
    runPublicDualIndicesCase("public Umeyama dual-indices-cloud-pair 64K",
                             medium_indexed_source,
                             dual_medium_source_indices,
                             dual_medium_target,
                             dual_medium_target_indices,
                             iterations,
                             warmup_iterations);
    runPublicDualIndicesCase("public Umeyama dual-indices-cloud-pair 256K",
                             large_indexed_source,
                             dual_large_source_indices,
                             dual_large_target,
                             dual_large_target_indices,
                             iterations,
                             warmup_iterations);
    runFusedDualIndicesCase("fused accum candidate dual-indices-cloud-pair 4K",
                            small_indexed_source,
                            dual_small_source_indices,
                            dual_small_target,
                            dual_small_target_indices,
                            iterations,
                            warmup_iterations);
    runFusedDualIndicesCase("fused accum candidate dual-indices-cloud-pair 64K",
                            medium_indexed_source,
                            dual_medium_source_indices,
                            dual_medium_target,
                            dual_medium_target_indices,
                            iterations,
                            warmup_iterations);
    runFusedDualIndicesCase("fused accum candidate dual-indices-cloud-pair 256K",
                            large_indexed_source,
                            dual_large_source_indices,
                            dual_large_target,
                            dual_large_target_indices,
                            iterations,
                            warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-pair", "correspondences")) {
    runPublicCorrespondenceCase("public Umeyama correspondence-pair 4K",
                                small_indexed_source,
                                dual_small_target,
                                dual_small_correspondences,
                                iterations,
                                warmup_iterations);
    runPublicCorrespondenceCase("public Umeyama correspondence-pair 64K",
                                medium_indexed_source,
                                dual_medium_target,
                                dual_medium_correspondences,
                                iterations,
                                warmup_iterations);
    runPublicCorrespondenceCase("public Umeyama correspondence-pair 256K",
                                large_indexed_source,
                                dual_large_target,
                                dual_large_correspondences,
                                iterations,
                                warmup_iterations);
    runFusedCorrespondenceCase("fused accum candidate correspondence-pair 4K",
                               small_indexed_source,
                               dual_small_target,
                               dual_small_correspondences,
                               iterations,
                               warmup_iterations);
    runFusedCorrespondenceCase("fused accum candidate correspondence-pair 64K",
                               medium_indexed_source,
                               dual_medium_target,
                               dual_medium_correspondences,
                               iterations,
                               warmup_iterations);
    runFusedCorrespondenceCase("fused accum candidate correspondence-pair 256K",
                               large_indexed_source,
                               dual_large_target,
                               dual_large_correspondences,
                               iterations,
                               warmup_iterations);
  }

  printBanner('=');
  return 0;
}
