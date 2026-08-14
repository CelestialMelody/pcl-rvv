/*
 * 本文件做什么：
 * 这是 transformation_estimation_2D 的 bench smoke（性能测试烟测）入口。它输出
 * public ordered-cloud-pair 路径和 test-only fused candidate 的 label、耗时和
 * checksum（校验和），供 QEMU 日志形状检查和后续板卡 repeated bench 使用。
 *
 * 证据边界：
 * QEMU timing（QEMU 计时）只证明构建、路径和输出格式；真实性能结论必须来自 board /
 * target hardware repeated benchmark（板卡或目标硬件重复性能测试）。
 */

#include "te2d.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace support = pcl::registration::rvv_te2d_support;

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
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter")
      return std::string(argv[i + 1]) == requested || std::string(argv[i + 1]) == "all";
  }
  return true;
}

template <typename PointT>
pcl::PointCloud<PointT>
selectByIndices(const pcl::PointCloud<PointT>& source, const pcl::Indices& indices)
{
  pcl::PointCloud<PointT> selected;
  selected.width = static_cast<std::uint32_t>(indices.size());
  selected.height = 1;
  selected.is_dense = source.is_dense;
  selected.resize(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    selected[i] = source[static_cast<std::size_t>(indices[i])];
  return selected;
}

pcl::Indices
makeStridedIndices(const std::size_t row_count, const std::size_t storage_count)
{
  pcl::Indices indices(row_count);
  for (std::size_t i = 0; i < row_count; ++i)
    indices[i] = static_cast<pcl::index_t>((i * 2) % storage_count);
  return indices;
}

pcl::Correspondences
makeStridedCorrespondences(const std::size_t row_count, const std::size_t storage_count)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(row_count);
  for (std::size_t i = 0; i < row_count; ++i) {
    const int index = static_cast<int>((i * 2) % storage_count);
    correspondences.emplace_back(index, index, 0.0f);
  }
  return correspondences;
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

  std::cout << std::left << std::setw(64) << name << ": " << std::fixed
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
    return support::matrixChecksum(support::estimatePublic2D(source, target));
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
    const Eigen::Matrix4f matrix =
        support::estimateFused2DCandidate(source, target, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x9e3779b97f4a7c15ull : 0ull);
  });
}

void
runRowSourceCase(const std::string& label,
                 const int iterations,
                 const int warmup_iterations,
                 const std::function<std::uint64_t()>& fn)
{
  // 统一 row-source candidate 的计时外壳，便于 ASM 归属和 bench 边界审查。
  runCase(label, iterations, warmup_iterations, fn);
}

template <typename CloudT>
void
runSourceIndexedCase(const std::string& label,
                     const CloudT& source,
                     const pcl::Indices& source_indices,
                     const CloudT& target,
                     const int iterations,
                     const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix = support::estimateFused2DSourceIndexedCandidate(
        source, source_indices, target, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x9e3779b97f4a7c15ull : 0ull);
  });
}

template <typename CloudT>
void
runDualIndexedCase(const std::string& label,
                   const CloudT& source,
                   const pcl::Indices& source_indices,
                   const CloudT& target,
                   const pcl::Indices& target_indices,
                   const int iterations,
                   const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix = support::estimateFused2DDualIndexedCandidate(
        source, source_indices, target, target_indices, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x9e3779b97f4a7c15ull : 0ull);
  });
}

template <typename CloudT>
void
runCorrespondenceCase(const std::string& label,
                      const CloudT& source,
                      const CloudT& target,
                      const pcl::Correspondences& correspondences,
                      const int iterations,
                      const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix = support::estimateFused2DCorrespondenceCandidate(
        source, target, correspondences, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x9e3779b97f4a7c15ull : 0ull);
  });
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const auto transform = support::makeRigid2DTransform();
  const auto small = support::makePointXYZCloud(4096);
  const auto medium = support::makePointXYZCloud(65536);
  const auto large = support::makePointXYZCloud(262144);
  const auto small_target = support::transformCloud2D(small, transform);
  const auto medium_target = support::transformCloud2D(medium, transform);
  const auto large_target = support::transformCloud2D(large, transform);
  const auto row_small = support::makePointXYZCloud(8192);
  const auto row_medium = support::makePointXYZCloud(131072);
  const auto row_large = support::makePointXYZCloud(524288);
  const auto row_small_target = support::transformCloud2D(row_small, transform);
  const auto row_medium_target = support::transformCloud2D(row_medium, transform);
  const auto row_large_target = support::transformCloud2D(row_large, transform);
  const auto row_small_indices = makeStridedIndices(4096, row_small.size());
  const auto row_medium_indices = makeStridedIndices(65536, row_medium.size());
  const auto row_large_indices = makeStridedIndices(262144, row_large.size());
  const auto row_small_correspondences =
      makeStridedCorrespondences(4096, row_small.size());
  const auto row_medium_correspondences =
      makeStridedCorrespondences(65536, row_medium.size());
  const auto row_large_correspondences =
      makeStridedCorrespondences(262144, row_large.size());
  const auto row_small_target_selected = selectByIndices(row_small_target, row_small_indices);
  const auto row_medium_target_selected =
      selectByIndices(row_medium_target, row_medium_indices);
  const auto row_large_target_selected =
      selectByIndices(row_large_target, row_large_indices);

  printBanner('=');
  std::cout << "PCL registration/transformation_estimation_2D RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  const bool row_source_mode = caseEnabled(argc, argv, "row-source-fused");
  const bool public_mode = caseEnabled(argc, argv, "ordered-cloud-pair-public");
  const char* dataset_label = row_source_mode
                                  ? "synthetic dense PointXYZ row-source pairs; materialize-to-ordered"
                                  : (public_mode
                                         ? "synthetic dense PointXYZ ordered-cloud-pair public"
                                         : "synthetic dense PointXYZ ordered-cloud-pair fused");
  std::cout << "Dataset: " << dataset_label << "; 2D rigid transform\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  printBanner('-');

  if (caseEnabled(argc, argv, "ordered-cloud-pair-public")) {
    runPublicCase("public 2D ordered-cloud-pair 4K",
                  small,
                  small_target,
                  iterations,
                  warmup_iterations);
    runPublicCase("public 2D ordered-cloud-pair 64K",
                  medium,
                  medium_target,
                  iterations,
                  warmup_iterations);
    runPublicCase("public 2D ordered-cloud-pair 256K",
                  large,
                  large_target,
                  iterations,
                  warmup_iterations);
  }

  if (caseEnabled(argc, argv, "ordered-cloud-pair-fused")) {
    runFusedCase("fused 2D correlation ordered-cloud-pair 4K",
                 small,
                 small_target,
                 iterations,
                 warmup_iterations);
    runFusedCase("fused 2D correlation ordered-cloud-pair 64K",
                 medium,
                 medium_target,
                 iterations,
                 warmup_iterations);
    runFusedCase("fused 2D correlation ordered-cloud-pair 256K",
                 large,
                 large_target,
                 iterations,
                 warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-fused")) {
    runSourceIndexedCase("fused 2D correlation source-indexed-cloud-pair 4K",
                         row_small,
                         row_small_indices,
                         row_small_target_selected,
                         iterations,
                         warmup_iterations);
    runSourceIndexedCase("fused 2D correlation source-indexed-cloud-pair 64K",
                         row_medium,
                         row_medium_indices,
                         row_medium_target_selected,
                         iterations,
                         warmup_iterations);
    runSourceIndexedCase("fused 2D correlation source-indexed-cloud-pair 256K",
                         row_large,
                         row_large_indices,
                         row_large_target_selected,
                         iterations,
                         warmup_iterations);

    runDualIndexedCase("fused 2D correlation dual-indexed-cloud-pair 4K",
                       row_small,
                       row_small_indices,
                       row_small_target,
                       row_small_indices,
                       iterations,
                       warmup_iterations);
    runDualIndexedCase("fused 2D correlation dual-indexed-cloud-pair 64K",
                       row_medium,
                       row_medium_indices,
                       row_medium_target,
                       row_medium_indices,
                       iterations,
                       warmup_iterations);
    runDualIndexedCase("fused 2D correlation dual-indexed-cloud-pair 256K",
                       row_large,
                       row_large_indices,
                       row_large_target,
                       row_large_indices,
                       iterations,
                       warmup_iterations);

    runCorrespondenceCase("fused 2D correlation correspondence-pair 4K",
                          row_small,
                          row_small_target,
                          row_small_correspondences,
                          iterations,
                          warmup_iterations);
    runCorrespondenceCase("fused 2D correlation correspondence-pair 64K",
                          row_medium,
                          row_medium_target,
                          row_medium_correspondences,
                          iterations,
                          warmup_iterations);
    runCorrespondenceCase("fused 2D correlation correspondence-pair 256K",
                          row_large,
                          row_large_target,
                          row_large_correspondences,
                          iterations,
                          warmup_iterations);
  }

  printBanner('=');
  return 0;
}
