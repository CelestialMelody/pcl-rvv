/*
 * 本文件做什么：
 * 这是 transformation_estimation_dual_quaternion 的 QEMU bench smoke（QEMU
 * 性能测试烟测）和后续 board bench（板卡性能测试）入口。它比较 test-only
 * candidate（测试专用候选）与标量 reference 的日志形状和 checksum（校验和）。
 * `component-ablation` 过滤器会额外拆出 C1/C2 accumulation-only（只测累加前端）
 * 和 solve-only（只测 Eigen 求解后段），用来解释局部 helper 收益为什么没有转成
 * production public entry（生产公开入口）收益。
 *
 * 证据边界：
 * QEMU timing（QEMU 计时）只证明构建、路径和输出格式；真实性能结论必须来自 board /
 * target hardware repeated benchmark（板卡或目标硬件重复性能测试）。
 */

#include "tedq.h"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace support = pcl::registration::rvv_tedq_support;

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
    return support::matrixChecksum(support::estimatePublicDualQuaternion(source, target));
  });
}

template <typename CloudT>
void
runCandidateCase(const std::string& label,
                 const CloudT& source,
                 const CloudT& target,
                 const int iterations,
                 const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionCandidate(source, target, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runAccumulationOnlyCase(const std::string& label,
                        const CloudT& source,
                        const CloudT& target,
                        const int iterations,
                        const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const support::DualQuaternionAccumulation acc =
        support::accumulateDualQuaternionCandidate(source, target, &stats);
    return support::accumulationChecksum(acc);
  });
}

template <typename CloudT>
void
runSolveOnlyCase(const std::string& label,
                 const CloudT& source,
                 const CloudT& target,
                 const int iterations,
                 const int warmup_iterations)
{
  const support::DualQuaternionAccumulation acc =
      support::accumulateDualQuaternionStd(source, target);
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::finishDualQuaternionEstimate(acc));
  });
}

template <typename CloudT>
void
runSourceIndexedPublicCase(const std::string& label,
                           const CloudT& source,
                           const pcl::Indices& indices,
                           const CloudT& target,
                           const int iterations,
                           const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicSourceIndexedDualQuaternion(source, indices, target));
  });
}

template <typename CloudT>
void
runSourceIndexedCandidateCase(const std::string& label,
                              const CloudT& source,
                              const pcl::Indices& indices,
                              const CloudT& target,
                              const int iterations,
                              const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionSourceIndexedCandidate(
            source, indices, target, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runSourceIndexedDirectCandidateCase(const std::string& label,
                                    const CloudT& source,
                                    const pcl::Indices& indices,
                                    const CloudT& target,
                                    const int iterations,
                                    const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionSourceIndexedDirectCandidate(
            source, indices, target, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runDualIndexedPublicCase(const std::string& label,
                         const CloudT& source,
                         const pcl::Indices& source_indices,
                         const CloudT& target,
                         const pcl::Indices& target_indices,
                         const int iterations,
                         const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimatePublicDualIndexedDualQuaternion(
        source, source_indices, target, target_indices));
  });
}

template <typename CloudT>
void
runDualIndexedCandidateCase(const std::string& label,
                            const CloudT& source,
                            const pcl::Indices& source_indices,
                            const CloudT& target,
                            const pcl::Indices& target_indices,
                            const int iterations,
                            const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionDualIndexedCandidate(
            source, source_indices, target, target_indices, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runDualIndexedDirectCandidateCase(const std::string& label,
                                  const CloudT& source,
                                  const pcl::Indices& source_indices,
                                  const CloudT& target,
                                  const pcl::Indices& target_indices,
                                  const int iterations,
                                  const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionDualIndexedDirectCandidate(
            source, source_indices, target, target_indices, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runCorrespondencePublicCase(const std::string& label,
                            const CloudT& source,
                            const CloudT& target,
                            const pcl::Correspondences& correspondences,
                            const int iterations,
                            const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicCorrespondenceDualQuaternion(
            source, target, correspondences));
  });
}

template <typename CloudT>
void
runCorrespondenceCandidateCase(const std::string& label,
                               const CloudT& source,
                               const CloudT& target,
                               const pcl::Correspondences& correspondences,
                               const int iterations,
                               const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionCorrespondenceCandidate(
            source, target, correspondences, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runCorrespondenceDirectCandidateCase(const std::string& label,
                                     const CloudT& source,
                                     const CloudT& target,
                                     const pcl::Correspondences& correspondences,
                                     const int iterations,
                                     const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionCorrespondenceDirectCandidate(
            source, target, correspondences, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runCorrespondenceDirectIndexStreamCase(const std::string& label,
                                       const CloudT& source,
                                       const CloudT& target,
                                       const pcl::Correspondences& correspondences,
                                       const int iterations,
                                       const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionCorrespondenceDirectIndexStreamCandidate(
            source, target, correspondences, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runCorrespondenceSegmentIndexStreamCase(const std::string& label,
                                        const CloudT& source,
                                        const CloudT& target,
                                        const pcl::Correspondences& correspondences,
                                        const int iterations,
                                        const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateDualQuaternionCorrespondenceSegmentIndexStreamCandidate(
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
  const auto small_source_indices =
      support::makeSourceIndices(small_indexed_source.size(), 4096);
  const auto medium_source_indices =
      support::makeSourceIndices(medium_indexed_source.size(), 65536);
  const auto large_source_indices =
      support::makeSourceIndices(large_indexed_source.size(), 262144);
  const auto small_target_indices =
      support::makeTargetIndices(small_indexed_source.size(), 4096);
  const auto medium_target_indices =
      support::makeTargetIndices(medium_indexed_source.size(), 65536);
  const auto large_target_indices =
      support::makeTargetIndices(large_indexed_source.size(), 262144);
  const auto small_source_indexed_target = support::transformCloudXYZBySourceIndices(
      small_indexed_source, small_source_indices, transform);
  const auto medium_source_indexed_target = support::transformCloudXYZBySourceIndices(
      medium_indexed_source, medium_source_indices, transform);
  const auto large_source_indexed_target = support::transformCloudXYZBySourceIndices(
      large_indexed_source, large_source_indices, transform);
  const auto small_dual_target = support::transformCloudXYZByPairedIndices(
      small_indexed_source,
      small_source_indices,
      small_target_indices,
      small_indexed_source.size(),
      transform);
  const auto medium_dual_target = support::transformCloudXYZByPairedIndices(
      medium_indexed_source,
      medium_source_indices,
      medium_target_indices,
      medium_indexed_source.size(),
      transform);
  const auto large_dual_target = support::transformCloudXYZByPairedIndices(
      large_indexed_source,
      large_source_indices,
      large_target_indices,
      large_indexed_source.size(),
      transform);
  const auto small_correspondence_locality_target =
      support::transformCloudXYZ(small_indexed_source, transform);
  const auto medium_correspondence_locality_target =
      support::transformCloudXYZ(medium_indexed_source, transform);
  const auto large_correspondence_locality_target =
      support::transformCloudXYZ(large_indexed_source, transform);
  const auto small_correspondences = support::makeCorrespondences(
      small_indexed_source.size(), small_dual_target.size(), 4096);
  const auto medium_correspondences = support::makeCorrespondences(
      medium_indexed_source.size(), medium_dual_target.size(), 65536);
  const auto large_correspondences = support::makeCorrespondences(
      large_indexed_source.size(), large_dual_target.size(), 262144);
  const auto small_correspondences_contiguous =
      support::makeCorrespondencesForPattern(
          small_indexed_source.size(),
          small_correspondence_locality_target.size(),
          4096,
          support::CorrespondenceIndexPattern::contiguous);
  const auto small_correspondences_local_window =
      support::makeCorrespondencesForPattern(
          small_indexed_source.size(),
          small_correspondence_locality_target.size(),
          4096,
          support::CorrespondenceIndexPattern::local_window);
  const auto small_correspondences_strided =
      support::makeCorrespondencesForPattern(
          small_indexed_source.size(),
          small_correspondence_locality_target.size(),
          4096,
          support::CorrespondenceIndexPattern::strided);
  const auto medium_correspondences_contiguous =
      support::makeCorrespondencesForPattern(
          medium_indexed_source.size(),
          medium_correspondence_locality_target.size(),
          65536,
          support::CorrespondenceIndexPattern::contiguous);
  const auto medium_correspondences_local_window =
      support::makeCorrespondencesForPattern(
          medium_indexed_source.size(),
          medium_correspondence_locality_target.size(),
          65536,
          support::CorrespondenceIndexPattern::local_window);
  const auto medium_correspondences_strided =
      support::makeCorrespondencesForPattern(
          medium_indexed_source.size(),
          medium_correspondence_locality_target.size(),
          65536,
          support::CorrespondenceIndexPattern::strided);
  const auto large_correspondences_contiguous =
      support::makeCorrespondencesForPattern(
          large_indexed_source.size(),
          large_correspondence_locality_target.size(),
          262144,
          support::CorrespondenceIndexPattern::contiguous);
  const auto large_correspondences_local_window =
      support::makeCorrespondencesForPattern(
          large_indexed_source.size(),
          large_correspondence_locality_target.size(),
          262144,
          support::CorrespondenceIndexPattern::local_window);
  const auto large_correspondences_strided =
      support::makeCorrespondencesForPattern(
          large_indexed_source.size(),
          large_correspondence_locality_target.size(),
          262144,
          support::CorrespondenceIndexPattern::strided);
  const auto small_xyzi = support::makePointXYZICloud(8193);
  const auto medium_xyzi = support::makePointXYZICloud(131073);
  const auto large_xyzi = support::makePointXYZICloud(524289);
  const auto small_xyzi_target = support::transformCloudXYZ(small_xyzi, transform);
  const auto medium_xyzi_target = support::transformCloudXYZ(medium_xyzi, transform);
  const auto large_xyzi_target = support::transformCloudXYZ(large_xyzi, transform);
  const auto small_xyzi_source_indexed_target =
      support::transformCloudXYZBySourceIndices(small_xyzi, small_source_indices, transform);
  const auto medium_xyzi_source_indexed_target = support::transformCloudXYZBySourceIndices(
      medium_xyzi, medium_source_indices, transform);
  const auto large_xyzi_source_indexed_target =
      support::transformCloudXYZBySourceIndices(large_xyzi, large_source_indices, transform);
  const auto small_xyzi_dual_target = support::transformCloudXYZByPairedIndices(
      small_xyzi, small_source_indices, small_target_indices, small_xyzi.size(), transform);
  const auto medium_xyzi_dual_target = support::transformCloudXYZByPairedIndices(
      medium_xyzi, medium_source_indices, medium_target_indices, medium_xyzi.size(), transform);
  const auto large_xyzi_dual_target = support::transformCloudXYZByPairedIndices(
      large_xyzi, large_source_indices, large_target_indices, large_xyzi.size(), transform);
  const auto small_rgb = support::makePointXYZRGBCloud(8193);
  const auto medium_rgb = support::makePointXYZRGBCloud(131073);
  const auto large_rgb = support::makePointXYZRGBCloud(524289);
  const auto small_rgb_target = support::transformCloudXYZ(small_rgb, transform);
  const auto medium_rgb_target = support::transformCloudXYZ(medium_rgb, transform);
  const auto large_rgb_target = support::transformCloudXYZ(large_rgb, transform);
  const auto small_rgb_source_indexed_target =
      support::transformCloudXYZBySourceIndices(small_rgb, small_source_indices, transform);
  const auto medium_rgb_source_indexed_target = support::transformCloudXYZBySourceIndices(
      medium_rgb, medium_source_indices, transform);
  const auto large_rgb_source_indexed_target =
      support::transformCloudXYZBySourceIndices(large_rgb, large_source_indices, transform);
  const auto small_rgb_dual_target = support::transformCloudXYZByPairedIndices(
      small_rgb, small_source_indices, small_target_indices, small_rgb.size(), transform);
  const auto medium_rgb_dual_target = support::transformCloudXYZByPairedIndices(
      medium_rgb, medium_source_indices, medium_target_indices, medium_rgb.size(), transform);
  const auto large_rgb_dual_target = support::transformCloudXYZByPairedIndices(
      large_rgb, large_source_indices, large_target_indices, large_rgb.size(), transform);
  const auto small_point_type_correspondences = support::makeCorrespondencesForPattern(
      small_xyzi.size(),
      small_xyzi_target.size(),
      4096,
      support::CorrespondenceIndexPattern::strided);
  const auto medium_point_type_correspondences = support::makeCorrespondencesForPattern(
      medium_xyzi.size(),
      medium_xyzi_target.size(),
      65536,
      support::CorrespondenceIndexPattern::strided);
  const auto large_point_type_correspondences = support::makeCorrespondencesForPattern(
      large_xyzi.size(),
      large_xyzi_target.size(),
      262144,
      support::CorrespondenceIndexPattern::strided);

  printBanner('=');
  std::cout << "PCL registration/transformation_estimation_dual_quaternion RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic dense xyz AoS clouds; "
               "dual-quaternion C1/C2 accumulation, component ablation, and Eigen 4x4 solve\n";
  std::cout << "Row-source Dataset: ordered-cloud-pair, source-indexed-cloud-pair, "
               "dual-indexed-cloud-pair, and correspondence-pair diagnostics\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  printBanner('-');

  const bool run_component_ablation = caseEnabled(argc, argv, "component-ablation");
  const bool run_row_source_expansion =
      caseEnabled(argc, argv, "row-source-expansion");
  const bool run_production_public_row_sources =
      caseEnabled(argc, argv, "production-public-row-sources");
  const bool run_production_public_retained_row_sources =
      caseEnabled(argc, argv, "production-public-retained-row-sources");
  const bool run_any_production_public_row_sources =
      run_production_public_row_sources || run_production_public_retained_row_sources;
  const bool run_row_source_family_comparison =
      caseEnabled(argc, argv, "row-source-family-comparison");
  const bool run_indexed_direct_gather_family_comparison =
      caseEnabled(argc, argv, "indexed-direct-gather-family-comparison");
  const bool run_indexed_direct_gather_point_type_layout =
      caseEnabled(argc, argv, "indexed-direct-gather-point-type-layout");
  const bool run_correspondence_direct_index_stream =
      caseEnabled(argc, argv, "correspondence-direct-index-stream-comparison");
  const bool run_correspondence_segment_stream =
      caseEnabled(argc, argv, "correspondence-segment-load-comparison");
  const bool run_correspondence_index_locality =
      caseEnabled(argc, argv, "correspondence-index-locality-ablation");
  const bool run_correspondence_point_type_layout =
      caseEnabled(argc, argv, "correspondence-point-type-layout");

  if (caseEnabled(argc, argv, "public-dual-quaternion") || run_component_ablation ||
      run_any_production_public_row_sources) {
    runPublicCase("public dual quaternion ordered-cloud-pair 4K",
                  small,
                  small_target,
                  iterations,
                  warmup_iterations);
    runPublicCase("public dual quaternion ordered-cloud-pair 64K",
                  medium,
                  medium_target,
                  iterations,
                  warmup_iterations);
    runPublicCase("public dual quaternion ordered-cloud-pair 256K",
                  large,
                  large_target,
                  iterations,
                  warmup_iterations);
  }

  if (caseEnabled(argc, argv, "ordered-cloud-pair") || run_component_ablation) {
    runCandidateCase("rvv accum candidate ordered-cloud-pair 4K",
                     small,
                     small_target,
                     iterations,
                     warmup_iterations);
    runCandidateCase("rvv accum candidate ordered-cloud-pair 64K",
                     medium,
                     medium_target,
                     iterations,
                     warmup_iterations);
    runCandidateCase("rvv accum candidate ordered-cloud-pair 256K",
                     large,
                     large_target,
                     iterations,
                     warmup_iterations);
  }

  if (run_component_ablation) {
    runAccumulationOnlyCase("component accum only ordered-cloud-pair 4K",
                            small,
                            small_target,
                            iterations,
                            warmup_iterations);
    runAccumulationOnlyCase("component accum only ordered-cloud-pair 64K",
                            medium,
                            medium_target,
                            iterations,
                            warmup_iterations);
    runAccumulationOnlyCase("component accum only ordered-cloud-pair 256K",
                            large,
                            large_target,
                            iterations,
                            warmup_iterations);
    runSolveOnlyCase("component solve only ordered-cloud-pair 4K",
                     small,
                     small_target,
                     iterations,
                     warmup_iterations);
    runSolveOnlyCase("component solve only ordered-cloud-pair 64K",
                     medium,
                     medium_target,
                     iterations,
                     warmup_iterations);
    runSolveOnlyCase("component solve only ordered-cloud-pair 256K",
                     large,
                     large_target,
                     iterations,
                     warmup_iterations);
  }

  if (run_row_source_expansion || run_any_production_public_row_sources) {
    runSourceIndexedPublicCase("public dual quaternion source-indexed-cloud-pair 4K",
                               small_indexed_source,
                               small_source_indices,
                               small_source_indexed_target,
                               iterations,
                               warmup_iterations);
    runSourceIndexedPublicCase("public dual quaternion source-indexed-cloud-pair 64K",
                               medium_indexed_source,
                               medium_source_indices,
                               medium_source_indexed_target,
                               iterations,
                               warmup_iterations);
    runSourceIndexedPublicCase("public dual quaternion source-indexed-cloud-pair 256K",
                               large_indexed_source,
                               large_source_indices,
                               large_source_indexed_target,
                               iterations,
                               warmup_iterations);
  }

  if (run_row_source_expansion) {
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_source_indexed_target,
        iterations,
        warmup_iterations);
  }

  if (run_row_source_expansion || run_any_production_public_row_sources) {
    runDualIndexedPublicCase("public dual quaternion dual-indexed-cloud-pair 4K",
                             small_indexed_source,
                             small_source_indices,
                             small_dual_target,
                             small_target_indices,
                             iterations,
                             warmup_iterations);
    runDualIndexedPublicCase("public dual quaternion dual-indexed-cloud-pair 64K",
                             medium_indexed_source,
                             medium_source_indices,
                             medium_dual_target,
                             medium_target_indices,
                             iterations,
                             warmup_iterations);
    runDualIndexedPublicCase("public dual quaternion dual-indexed-cloud-pair 256K",
                             large_indexed_source,
                             large_source_indices,
                             large_dual_target,
                             large_target_indices,
                             iterations,
                             warmup_iterations);
  }

  if (run_row_source_expansion) {
    runDualIndexedCandidateCase(
        "staged accum candidate dual-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_dual_target,
        small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedCandidateCase(
        "staged accum candidate dual-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_dual_target,
        medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedCandidateCase(
        "staged accum candidate dual-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_dual_target,
        large_target_indices,
        iterations,
        warmup_iterations);
  }

  if (run_row_source_expansion || run_production_public_row_sources) {
    runCorrespondencePublicCase("public dual quaternion correspondence-pair 4K",
                                small_indexed_source,
                                small_dual_target,
                                small_correspondences,
                                iterations,
                                warmup_iterations);
    runCorrespondencePublicCase("public dual quaternion correspondence-pair 64K",
                                medium_indexed_source,
                                medium_dual_target,
                                medium_correspondences,
                                iterations,
                                warmup_iterations);
    runCorrespondencePublicCase("public dual quaternion correspondence-pair 256K",
                                large_indexed_source,
                                large_dual_target,
                                large_correspondences,
                                iterations,
                                warmup_iterations);
  }

  if (run_row_source_expansion) {
    runCorrespondenceCandidateCase("staged accum candidate correspondence-pair 4K",
                                  small_indexed_source,
                                  small_dual_target,
                                  small_correspondences,
                                  iterations,
                                  warmup_iterations);
    runCorrespondenceCandidateCase("staged accum candidate correspondence-pair 64K",
                                  medium_indexed_source,
                                  medium_dual_target,
                                  medium_correspondences,
                                  iterations,
                                  warmup_iterations);
    runCorrespondenceCandidateCase("staged accum candidate correspondence-pair 256K",
                                  large_indexed_source,
                                  large_dual_target,
                                  large_correspondences,
                                  iterations,
                                  warmup_iterations);
  }

  if (run_row_source_family_comparison) {
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_source_indexed_target,
        iterations,
        warmup_iterations);
  }

  if (run_indexed_direct_gather_family_comparison) {
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedCandidateCase(
        "staged accum candidate source-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_source_indexed_target,
        iterations,
        warmup_iterations);

    runDualIndexedCandidateCase(
        "staged accum candidate dual-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_dual_target,
        small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair 4K",
        small_indexed_source,
        small_source_indices,
        small_dual_target,
        small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedCandidateCase(
        "staged accum candidate dual-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_dual_target,
        medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair 64K",
        medium_indexed_source,
        medium_source_indices,
        medium_dual_target,
        medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedCandidateCase(
        "staged accum candidate dual-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_dual_target,
        large_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair 256K",
        large_indexed_source,
        large_source_indices,
        large_dual_target,
        large_target_indices,
        iterations,
        warmup_iterations);

    runCorrespondenceCandidateCase(
        "staged accum candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectCandidateCase(
        "direct gather candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceCandidateCase(
        "staged accum candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectCandidateCase(
        "direct gather candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceCandidateCase(
        "staged accum candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectCandidateCase(
        "direct gather candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
  }

  if (run_indexed_direct_gather_point_type_layout) {
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair PointXYZI 4K",
        small_xyzi,
        small_source_indices,
        small_xyzi_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair PointXYZRGB 4K",
        small_rgb,
        small_source_indices,
        small_rgb_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair PointXYZI 64K",
        medium_xyzi,
        medium_source_indices,
        medium_xyzi_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair PointXYZRGB 64K",
        medium_rgb,
        medium_source_indices,
        medium_rgb_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair PointXYZI 256K",
        large_xyzi,
        large_source_indices,
        large_xyzi_source_indexed_target,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectCandidateCase(
        "direct gather candidate source-indexed-cloud-pair PointXYZRGB 256K",
        large_rgb,
        large_source_indices,
        large_rgb_source_indexed_target,
        iterations,
        warmup_iterations);

    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair PointXYZI 4K",
        small_xyzi,
        small_source_indices,
        small_xyzi_dual_target,
        small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair PointXYZRGB 4K",
        small_rgb,
        small_source_indices,
        small_rgb_dual_target,
        small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair PointXYZI 64K",
        medium_xyzi,
        medium_source_indices,
        medium_xyzi_dual_target,
        medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair PointXYZRGB 64K",
        medium_rgb,
        medium_source_indices,
        medium_rgb_dual_target,
        medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair PointXYZI 256K",
        large_xyzi,
        large_source_indices,
        large_xyzi_dual_target,
        large_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectCandidateCase(
        "direct gather candidate dual-indexed-cloud-pair PointXYZRGB 256K",
        large_rgb,
        large_source_indices,
        large_rgb_dual_target,
        large_target_indices,
        iterations,
        warmup_iterations);
  }

  if (run_correspondence_direct_index_stream) {
    runCorrespondenceCandidateCase(
        "staged accum candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectCandidateCase(
        "direct gather candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceCandidateCase(
        "staged accum candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectCandidateCase(
        "direct gather candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceCandidateCase(
        "staged accum candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectCandidateCase(
        "direct gather candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
  }

  if (run_correspondence_segment_stream) {
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceSegmentIndexStreamCase(
        "direct segment stream candidate correspondence-pair 4K",
        small_indexed_source,
        small_dual_target,
        small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceSegmentIndexStreamCase(
        "direct segment stream candidate correspondence-pair 64K",
        medium_indexed_source,
        medium_dual_target,
        medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceSegmentIndexStreamCase(
        "direct segment stream candidate correspondence-pair 256K",
        large_indexed_source,
        large_dual_target,
        large_correspondences,
        iterations,
        warmup_iterations);
  }

  if (run_correspondence_index_locality) {
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair contiguous 4K",
        small_indexed_source,
        small_correspondence_locality_target,
        small_correspondences_contiguous,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair local-window 4K",
        small_indexed_source,
        small_correspondence_locality_target,
        small_correspondences_local_window,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair strided 4K",
        small_indexed_source,
        small_correspondence_locality_target,
        small_correspondences_strided,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair contiguous 64K",
        medium_indexed_source,
        medium_correspondence_locality_target,
        medium_correspondences_contiguous,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair local-window 64K",
        medium_indexed_source,
        medium_correspondence_locality_target,
        medium_correspondences_local_window,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair strided 64K",
        medium_indexed_source,
        medium_correspondence_locality_target,
        medium_correspondences_strided,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair contiguous 256K",
        large_indexed_source,
        large_correspondence_locality_target,
        large_correspondences_contiguous,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair local-window 256K",
        large_indexed_source,
        large_correspondence_locality_target,
        large_correspondences_local_window,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair strided 256K",
        large_indexed_source,
        large_correspondence_locality_target,
        large_correspondences_strided,
        iterations,
        warmup_iterations);
  }

  if (run_correspondence_point_type_layout) {
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair PointXYZI strided 4K",
        small_xyzi,
        small_xyzi_target,
        small_point_type_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair PointXYZRGB strided 4K",
        small_rgb,
        small_rgb_target,
        small_point_type_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair PointXYZI strided 64K",
        medium_xyzi,
        medium_xyzi_target,
        medium_point_type_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair PointXYZRGB strided 64K",
        medium_rgb,
        medium_rgb_target,
        medium_point_type_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair PointXYZI strided 256K",
        large_xyzi,
        large_xyzi_target,
        large_point_type_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectIndexStreamCase(
        "direct index stream candidate correspondence-pair PointXYZRGB strided 256K",
        large_rgb,
        large_rgb_target,
        large_point_type_correspondences,
        iterations,
        warmup_iterations);
  }

  printBanner('=');
  return 0;
}
