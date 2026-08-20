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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
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

bool
caseSelectedOnly(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter")
      return std::string(argv[i + 1]) == requested;
  }
  return false;
}

inline std::uint64_t
mixChecksum(std::uint64_t checksum, const std::uint64_t value)
{
  checksum = (checksum ^ value) * 1099511628211ull;
  return checksum;
}

inline std::uint64_t
floatBitsForChecksum(const float value)
{
  const auto scaled = static_cast<std::int64_t>(std::llround(value * 1000000.0f));
  return static_cast<std::uint64_t>(scaled);
}

template <typename PointT>
inline std::uint64_t
mixXYZ(std::uint64_t checksum, const PointT& point)
{
  checksum = mixChecksum(checksum, floatBitsForChecksum(point.x));
  checksum = mixChecksum(checksum, floatBitsForChecksum(point.y));
  checksum = mixChecksum(checksum, floatBitsForChecksum(point.z));
  return checksum;
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
makeStridedIndices(const std::size_t row_count,
                   const std::size_t storage_count,
                   const std::size_t stride = 3,
                   const std::size_t offset = 5)
{
  pcl::Indices indices(row_count);
  for (std::size_t i = 0; i < row_count; ++i)
    indices[i] =
        static_cast<pcl::index_t>((i * stride + offset) % storage_count);
  return indices;
}

pcl::Indices
makeIdentityIndices(const std::size_t row_count)
{
  pcl::Indices indices(row_count);
  for (std::size_t i = 0; i < row_count; ++i)
    indices[i] = static_cast<pcl::index_t>(i);
  return indices;
}

pcl::Indices
makeReverseIndices(const std::size_t row_count)
{
  pcl::Indices indices(row_count);
  for (std::size_t i = 0; i < row_count; ++i)
    indices[i] = static_cast<pcl::index_t>(row_count - i - 1);
  return indices;
}

pcl::Indices
makeShuffledIndices(const std::size_t row_count,
                    const std::uint64_t multiplier,
                    const std::uint64_t increment)
{
  pcl::Indices indices(row_count);
  for (std::size_t i = 0; i < row_count; ++i) {
    const auto mixed =
        multiplier * static_cast<std::uint64_t>(i) + increment;
    indices[i] = static_cast<pcl::index_t>(mixed % row_count);
  }
  return indices;
}

pcl::Correspondences
makeStridedCorrespondences(const pcl::Indices& query_indices,
                           const pcl::Indices& match_indices)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(query_indices.size());
  for (std::size_t i = 0; i < query_indices.size(); ++i) {
    correspondences.emplace_back(
        static_cast<int>(query_indices[i]), static_cast<int>(match_indices[i]), 0.0f);
  }
  return correspondences;
}

template <typename PointT>
pcl::PointCloud<PointT>
makeTargetForIndexedPairs(const pcl::PointCloud<PointT>& source,
                          const pcl::Indices& source_indices,
                          const pcl::Indices& target_indices,
                          const Eigen::Matrix4f& transform)
{
  pcl::PointCloud<PointT> target = source;
  target.is_dense = source.is_dense;
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto source_index = static_cast<std::size_t>(source_indices[i]);
    const auto target_index = static_cast<std::size_t>(target_indices[i]);
    const Eigen::Vector4f p(source[source_index].x,
                            source[source_index].y,
                            source[source_index].z,
                            1.0f);
    const Eigen::Vector4f q = transform * p;
    target[target_index].x = q.x();
    target[target_index].y = q.y();
    target[target_index].z = source[source_index].z;
  }
  return target;
}

template <typename PointSource, typename PointTarget>
pcl::PointCloud<PointTarget>
makeTargetForIndexedPairsTo(const pcl::PointCloud<PointSource>& source,
                            const pcl::Indices& source_indices,
                            const pcl::Indices& target_indices,
                            const Eigen::Matrix4f& transform,
                            const std::size_t target_storage_count,
                            const float target_bias)
{
  if (source_indices.size() != target_indices.size())
    throw std::runtime_error("indexed generic bench received mismatched indices");

  auto target =
      support::makeXYZLikeCloud<PointTarget>(target_storage_count, target_bias);
  target.is_dense = source.is_dense;
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    const auto source_index = static_cast<std::size_t>(source_indices[i]);
    const auto target_index = static_cast<std::size_t>(target_indices[i]);
    const Eigen::Vector4f p(source[source_index].x,
                            source[source_index].y,
                            source[source_index].z,
                            1.0f);
    const Eigen::Vector4f q = transform * p;
    target[target_index].x = q.x();
    target[target_index].y = q.y();
    target[target_index].z = source[source_index].z;
  }
  return target;
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

template <typename PointSource, typename PointTarget>
void
runPublicSourceIndexedCase(const std::string& label,
                           const pcl::PointCloud<PointSource>& source,
                           const pcl::Indices& source_indices,
                           const pcl::PointCloud<PointTarget>& target,
                           const int iterations,
                           const int warmup_iterations)
{
  // Phase 091 production-public bench：只调用真实 source-indexed public overload。
  // test-only direct gather candidate 有单独 case-filter，不能混作生产证据。
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicSourceIndexed2D(source, source_indices, target));
  });
}

template <typename PointSource, typename PointTarget>
void
runPublicSourceIndexedGenericCase(const std::string& label,
                                  const pcl::PointCloud<PointSource>& source,
                                  const pcl::Indices& source_indices,
                                  const pcl::PointCloud<PointTarget>& target,
                                  const int iterations,
                                  const int warmup_iterations)
{
  // Phase 103 production-public bench：真实 source-indexed public overload，
  // 但点型范围是 source/target 分别 traits-gated 的代表性泛型组合。
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicSourceIndexed2D(source, source_indices, target));
  });
}

template <typename PointSource, typename PointTarget>
void
runGeneratedPublicSourceIndexedGenericCase(const std::string& label,
                                           const std::size_t row_count,
                                           const float source_bias,
                                           const float target_bias,
                                           const Eigen::Matrix4f& transform,
                                           const int iterations,
                                           const int warmup_iterations)
{
  const auto source =
      support::makeXYZLikeCloud<PointSource>(row_count * 2, source_bias);
  const auto source_indices =
      makeStridedIndices(row_count, source.size(), 3, 5);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<PointSource, PointTarget>(
          selected, transform, target_bias);
  runPublicSourceIndexedGenericCase(
      label, source, source_indices, target, iterations, warmup_iterations);
}

template <typename CloudT>
void
runPublicDualIndexedCase(const std::string& label,
                         const CloudT& source,
                         const pcl::Indices& source_indices,
                         const CloudT& target,
                         const pcl::Indices& target_indices,
                         const int iterations,
                         const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimatePublicDualIndexed2D(
        source, source_indices, target, target_indices));
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
        support::estimatePublicCorrespondence2D(source, target, correspondences));
  });
}

template <typename CloudT>
void
runSourceIndexedFamilyABMaterializeOrderedCase(const std::string& label,
                                               const CloudT& source,
                                               const pcl::Indices& source_indices,
                                               const CloudT& target,
                                               const int iterations,
                                               const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicSourceIndexedMaterializedOrdered2D(
            source, source_indices, target));
  });
}

template <typename CloudT>
void
runDualIndexedFamilyABMaterializeOrderedCase(const std::string& label,
                                             const CloudT& source,
                                             const pcl::Indices& source_indices,
                                             const CloudT& target,
                                             const pcl::Indices& target_indices,
                                             const int iterations,
                                             const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicDualIndexedMaterializedOrdered2D(
            source, source_indices, target, target_indices));
  });
}

template <typename CloudT>
void
runSourceIndexedFamilyABDirectCase(const std::string& label,
                                   const CloudT& source,
                                   const pcl::Indices& source_indices,
                                   const CloudT& target,
                                   const int iterations,
                                   const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicSourceIndexed2D(source, source_indices, target));
  });
}

template <typename CloudT>
void
runDualIndexedFamilyABDirectCase(const std::string& label,
                                 const CloudT& source,
                                 const pcl::Indices& source_indices,
                                 const CloudT& target,
                                 const pcl::Indices& target_indices,
                                 const int iterations,
                                 const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicDualIndexed2D(
            source, source_indices, target, target_indices));
  });
}

template <typename CloudT>
void
runCorrespondenceFamilyABMaterializeOrderedCase(
    const std::string& label,
    const CloudT& source,
    const CloudT& target,
    const pcl::Correspondences& correspondences,
    const int iterations,
    const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicCorrespondenceMaterializedOrdered2D(
            source, target, correspondences));
  });
}

template <typename CloudT>
void
runCorrespondenceFamilyABDirectCase(const std::string& label,
                                    const CloudT& source,
                                    const CloudT& target,
                                    const pcl::Correspondences& correspondences,
                                    const int iterations,
                                    const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicCorrespondence2D(source, target, correspondences));
  });
}

template <typename CloudT>
void
runCorrespondenceStagedDualIndexedCase(const std::string& label,
                                       const CloudT& source,
                                       const CloudT& target,
                                       const pcl::Correspondences& correspondences,
                                       const int iterations,
                                       const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(
        support::estimatePublicCorrespondenceStagedDualIndexed2D(
            source, target, correspondences));
  });
}

template <typename CloudT>
void
runCorrespondenceLocalityOrderProfileCase(
    const std::string& profile,
    const std::string& size_label,
    const CloudT& source,
    const CloudT& target,
    const pcl::Correspondences& correspondences,
    const int iterations,
    const int warmup_iterations)
{
  const std::string prefix =
      " 2D correspondence-pair " + profile + " " + size_label;
  runCorrespondenceFamilyABDirectCase(
      "direct correspondence public" + prefix,
      source,
      target,
      correspondences,
      iterations,
      warmup_iterations);
  runCorrespondenceStagedDualIndexedCase(
      "staged-dual-indexed public" + prefix,
      source,
      target,
      correspondences,
      iterations,
      warmup_iterations);
  runCorrespondenceFamilyABMaterializeOrderedCase(
      "materialize-ordered public" + prefix,
      source,
      target,
      correspondences,
      iterations,
      warmup_iterations);
}

std::uint64_t
checksumCorrespondenceStructScan(const pcl::Correspondences& correspondences)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, static_cast<std::uint64_t>(correspondences.size()));
  for (const auto& correspondence : correspondences) {
    checksum = mixChecksum(checksum,
                           static_cast<std::uint64_t>(
                               static_cast<std::int64_t>(correspondence.index_query)));
    checksum = mixChecksum(checksum,
                           static_cast<std::uint64_t>(
                               static_cast<std::int64_t>(correspondence.index_match)));
    checksum = mixChecksum(checksum, floatBitsForChecksum(correspondence.distance));
  }
  return checksum;
}

template <typename PointSource, typename PointTarget>
std::uint64_t
checksumExtractedCorrespondenceIndices(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  pcl::Indices source_indices;
  pcl::Indices target_indices;
  if (!support::extractCorrespondenceIndices(
          source, target, correspondences, source_indices, target_indices))
    return 0;
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, static_cast<std::uint64_t>(source_indices.size()));
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    checksum = mixChecksum(
        checksum, static_cast<std::uint64_t>(static_cast<std::int64_t>(source_indices[i])));
    checksum = mixChecksum(
        checksum, static_cast<std::uint64_t>(static_cast<std::int64_t>(target_indices[i])));
  }
  return checksum;
}

template <typename PointSource, typename PointTarget>
std::uint64_t
checksumCorrespondenceXYZGatherNoSolve(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, static_cast<std::uint64_t>(correspondences.size()));
  for (const auto& correspondence : correspondences) {
    const auto query = static_cast<std::size_t>(correspondence.index_query);
    const auto match = static_cast<std::size_t>(correspondence.index_match);
    checksum = mixXYZ(checksum, source[query]);
    checksum = mixXYZ(checksum, target[match]);
  }
  return checksum;
}

template <typename PointSource, typename PointTarget>
std::uint64_t
checksumMaterializedCorrespondenceRowsNoSolve(
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences)
{
  pcl::PointCloud<PointSource> materialized_source;
  pcl::PointCloud<PointTarget> materialized_target;
  if (!support::materializeCorrespondencePair(
          source, target, correspondences, materialized_source, materialized_target))
    return 0;
  std::uint64_t checksum = 1469598103934665603ull;
  checksum = mixChecksum(checksum, static_cast<std::uint64_t>(materialized_source.size()));
  for (std::size_t i = 0; i < materialized_source.size(); ++i) {
    checksum = mixXYZ(checksum, materialized_source[i]);
    checksum = mixXYZ(checksum, materialized_target[i]);
  }
  return checksum;
}

template <typename PointSource, typename PointTarget>
void
runCorrespondenceComponentAblationCase(
    const std::string& profile,
    const std::string& size_label,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Correspondences& correspondences,
    const int iterations,
    const int warmup_iterations)
{
  const std::string suffix = " 2D correspondence-pair " + profile + " " + size_label;
  pcl::PointCloud<PointSource> prematerialized_source;
  pcl::PointCloud<PointTarget> prematerialized_target;
  if (!support::materializeCorrespondencePair(
          source, target, correspondences, prematerialized_source, prematerialized_target))
    throw std::runtime_error("Phase 097 component ablation received invalid correspondences");

  runCase("component scan-correspondence-structs" + suffix,
          iterations,
          warmup_iterations,
          [&]() { return checksumCorrespondenceStructScan(correspondences); });
  runCase("component extract-indices" + suffix, iterations, warmup_iterations, [&]() {
    return checksumExtractedCorrespondenceIndices(source, target, correspondences);
  });
  runCase("component gather-xyz-no-solve" + suffix,
          iterations,
          warmup_iterations,
          [&]() {
            return checksumCorrespondenceXYZGatherNoSolve(source, target, correspondences);
          });
  runCase("component materialize-rows-no-solve" + suffix,
          iterations,
          warmup_iterations,
          [&]() {
            return checksumMaterializedCorrespondenceRowsNoSolve(
                source, target, correspondences);
          });
  runCase("prematerialized ordered public full" + suffix,
          iterations,
          warmup_iterations,
          [&]() {
            return support::matrixChecksum(
                support::estimatePublic2D(prematerialized_source, prematerialized_target));
          });
  runCorrespondenceFamilyABDirectCase("direct correspondence public full" + suffix,
                                      source,
                                      target,
                                      correspondences,
                                      iterations,
                                      warmup_iterations);
  runCorrespondenceStagedDualIndexedCase("staged-dual-indexed public full" + suffix,
                                         source,
                                         target,
                                         correspondences,
                                         iterations,
                                         warmup_iterations);
  runCorrespondenceFamilyABMaterializeOrderedCase(
      "materialize-ordered public full" + suffix,
      source,
      target,
      correspondences,
      iterations,
      warmup_iterations);
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

template <typename PointSource, typename PointTarget>
void
runSourceIndexedDirectGatherCase(const std::string& label,
                                 const pcl::PointCloud<PointSource>& source,
                                 const pcl::Indices& source_indices,
                                 const pcl::PointCloud<PointTarget>& target,
                                 const int iterations,
                                 const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DSourceIndexedDirectGatherCandidate(
            source, source_indices, target, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x517cc1b727220a95ull : 0ull);
  });
}

template <typename PointSource, typename PointTarget>
void
runSourceIndexedGenericCase(const std::string& label,
                            const pcl::PointCloud<PointSource>& source,
                            const pcl::Indices& source_indices,
                            const pcl::PointCloud<PointTarget>& target,
                            const int iterations,
                            const int warmup_iterations)
{
  // Phase 099：source-indexed 泛型点型使用独立 lambda 边界，避免把
  // ordered-cloud-pair generic evidence 误读成 indexed row source 证据。
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DSourceIndexedDirectGatherCandidate(
            source, source_indices, target, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x4b6c06ad4be2a1f9ull : 0ull);
  });
}

template <typename PointSource, typename PointTarget>
void
runGeneratedSourceIndexedGenericCase(const std::string& label,
                                     const std::size_t row_count,
                                     const float source_bias,
                                     const float target_bias,
                                     const Eigen::Matrix4f& transform,
                                     const int iterations,
                                     const int warmup_iterations)
{
  const auto source =
      support::makeXYZLikeCloud<PointSource>(row_count * 2, source_bias);
  const auto source_indices =
      makeStridedIndices(row_count, source.size(), 3, 5);
  const auto selected = selectByIndices(source, source_indices);
  const auto target =
      support::transformCloud2DTo<PointSource, PointTarget>(
          selected, transform, target_bias);
  runSourceIndexedGenericCase(
      label, source, source_indices, target, iterations, warmup_iterations);
}

template <typename CloudT>
void
runDualIndexedDirectGatherCase(const std::string& label,
                               const CloudT& source,
                               const pcl::Indices& source_indices,
                               const CloudT& target,
                               const pcl::Indices& target_indices,
                               const int iterations,
                               const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DDualIndexedDirectGatherCandidate(
            source, source_indices, target, target_indices, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x6d2b79f5aa4d27c3ull : 0ull);
  });
}

template <typename PointSource, typename PointTarget>
void
runDualIndexedGenericCase(const std::string& label,
                          const pcl::PointCloud<PointSource>& source,
                          const pcl::Indices& source_indices,
                          const pcl::PointCloud<PointTarget>& target,
                          const pcl::Indices& target_indices,
                          const int iterations,
                          const int warmup_iterations)
{
  // Phase 100：dual-indexed 泛型点型使用专用 lambda 边界，分别审计
  // source / target 的 indexed gather，不能复用 ordered 或 source-indexed 结论。
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DDualIndexedDirectGatherCandidate(
            source, source_indices, target, target_indices, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x7c2b9f4d53a168e5ull : 0ull);
  });
}

template <typename PointSource, typename PointTarget>
void
runGeneratedDualIndexedGenericCase(const std::string& label,
                                   const std::size_t row_count,
                                   const float source_bias,
                                   const float target_bias,
                                   const Eigen::Matrix4f& transform,
                                   const int iterations,
                                   const int warmup_iterations)
{
  const auto source =
      support::makeXYZLikeCloud<PointSource>(row_count * 2, source_bias);
  const auto source_indices =
      makeStridedIndices(row_count, source.size(), 3, 5);
  const auto target_indices =
      makeStridedIndices(row_count, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairsTo<PointSource, PointTarget>(
      source, source_indices, target_indices, transform, source.size(), target_bias);
  runDualIndexedGenericCase(label,
                            source,
                            source_indices,
                            target,
                            target_indices,
                            iterations,
                            warmup_iterations);
}

template <typename PointSource, typename PointTarget>
void
runCorrespondenceGenericCase(const std::string& label,
                             const pcl::PointCloud<PointSource>& source,
                             const pcl::PointCloud<PointTarget>& target,
                             const pcl::Correspondences& correspondences,
                             const int iterations,
                             const int warmup_iterations)
{
  // Phase 101：correspondence 泛型点型使用专用 lambda 边界，分别审计
  // query/source 和 match/target 的 indexed gather，不能继承其它 row source 结论。
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DCorrespondenceDirectGatherCandidate(
            source, target, correspondences, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x5fd1a8e32c6b4971ull : 0ull);
  });
}

template <typename PointSource, typename PointTarget>
void
runGeneratedCorrespondenceGenericCase(const std::string& label,
                                      const std::size_t row_count,
                                      const float source_bias,
                                      const float target_bias,
                                      const Eigen::Matrix4f& transform,
                                      const int iterations,
                                      const int warmup_iterations)
{
  const auto source =
      support::makeXYZLikeCloud<PointSource>(row_count * 2, source_bias);
  const auto query_indices =
      makeStridedIndices(row_count, source.size(), 3, 5);
  const auto match_indices =
      makeStridedIndices(row_count, source.size(), 5, 7);
  const auto target = makeTargetForIndexedPairsTo<PointSource, PointTarget>(
      source, query_indices, match_indices, transform, source.size(), target_bias);
  const auto correspondences =
      makeStridedCorrespondences(query_indices, match_indices);
  runCorrespondenceGenericCase(
      label, source, target, correspondences, iterations, warmup_iterations);
}

template <typename CloudT>
void
runCorrespondenceDirectGatherCase(const std::string& label,
                                  const CloudT& source,
                                  const CloudT& target,
                                  const pcl::Correspondences& correspondences,
                                  const int iterations,
                                  const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DCorrespondenceDirectGatherCandidate(
            source, target, correspondences, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x94d049bb133111ebull : 0ull);
  });
}

template <typename CloudT>
void
runCorrespondenceChunkedXYZStagingCase(const std::string& label,
                                       const CloudT& source,
                                       const CloudT& target,
                                       const pcl::Correspondences& correspondences,
                                       const int iterations,
                                       const int warmup_iterations)
{
  runRowSourceCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DCorrespondenceChunkedXYZStagingCandidate(
            source, target, correspondences, &stats);
    return support::matrixChecksum(matrix);
  });
}

template <typename CloudT>
void
runCorrespondenceChunkedXYZStagingProfileCase(
    const std::string& size_label,
    const CloudT& source,
    const CloudT& target,
    const pcl::Correspondences& correspondences,
    const int iterations,
    const int warmup_iterations)
{
  const std::string suffix = " 2D correspondence-pair " + size_label;
  runCorrespondenceFamilyABDirectCase("direct correspondence public" + suffix,
                                      source,
                                      target,
                                      correspondences,
                                      iterations,
                                      warmup_iterations);
  runCorrespondenceChunkedXYZStagingCase("chunked-xyz-staging" + suffix,
                                         source,
                                         target,
                                         correspondences,
                                         iterations,
                                         warmup_iterations);
  runCorrespondenceFamilyABMaterializeOrderedCase(
      "materialize-ordered public" + suffix,
      source,
      target,
      correspondences,
      iterations,
      warmup_iterations);
  runCorrespondenceStagedDualIndexedCase("staged-dual-indexed public" + suffix,
                                         source,
                                         target,
                                         correspondences,
                                         iterations,
                                         warmup_iterations);
}

template <typename PointSource, typename PointTarget>
void
runGenericCase(const std::string& label,
               const pcl::PointCloud<PointSource>& source,
               const pcl::PointCloud<PointTarget>& target,
               const int iterations,
               const int warmup_iterations)
{
  // 泛型点型 bench 使用独立 lambda 边界，便于 asm attribution 与
  // ordered-cloud-pair 的旧 candidate 证据分开阅读。
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats stats;
    const Eigen::Matrix4f matrix =
        support::estimateFused2DCandidate(source, target, &stats);
    return support::matrixChecksum(matrix) ^
           static_cast<std::uint64_t>(stats.used_rvv ? 0x9e3779b97f4a7c15ull : 0ull);
  });
}

template <typename PointSource, typename PointTarget>
void
runPublicGenericCase(const std::string& label,
                     const pcl::PointCloud<PointSource>& source,
                     const pcl::PointCloud<PointTarget>& target,
                     const int iterations,
                     const int warmup_iterations)
{
  // 生产泛型 bench 只调用真实 public ordered-cloud-pair overload；它与
  // test-only `runGenericCase` 保持独立的 lambda 边界，便于 PI4 的 asm 归属。
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimatePublic2D(source, target));
  });
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const bool generic_mode = caseEnabled(argc, argv, "generic-xyz-point-types");
  const bool generic_public_mode =
      caseEnabled(argc, argv, "generic-xyz-point-types-public");
  const bool source_indexed_generic_mode =
      caseEnabled(argc, argv, "source-indexed-generic-xyz-point-types");
  const bool source_indexed_generic_public_mode =
      caseEnabled(argc, argv, "source-indexed-generic-xyz-point-types-public") ||
      caseEnabled(argc, argv, "source-indexed-generic-xyz-point-types-public-variance");
  const bool source_indexed_generic_pointnormal_256k_public_mode =
      caseSelectedOnly(argc, argv, "source-indexed-generic-pointnormal-256k-public");
  const bool source_indexed_pointxyzi_public_mode =
      caseSelectedOnly(argc, argv, "source-indexed-pointxyzi-public");
  const bool source_indexed_public_mode =
      caseEnabled(argc, argv, "source-indexed-public");
  const bool source_indexed_family_ab_mode =
      caseEnabled(argc, argv, "source-indexed-family-ab");
  const bool dual_indexed_family_ab_mode =
      caseEnabled(argc, argv, "dual-indexed-family-ab");
  const bool dual_indexed_generic_mode =
      caseEnabled(argc, argv, "dual-indexed-generic-xyz-point-types");
  const bool correspondence_generic_mode =
      caseEnabled(argc, argv, "correspondence-generic-xyz-point-types");
  const bool correspondence_public_mode =
      caseEnabled(argc, argv, "correspondence-public");
  const bool correspondence_family_ab_mode =
      caseEnabled(argc, argv, "correspondence-family-ab");
  const bool correspondence_staging_profile_mode =
      caseEnabled(argc, argv, "correspondence-staging-profile");
  const bool correspondence_locality_order_profile_mode =
      caseEnabled(argc, argv, "correspondence-locality-order-profile");
  const bool correspondence_component_ablation_mode =
      caseEnabled(argc, argv, "correspondence-component-ablation");
  const bool correspondence_chunked_xyz_staging_mode =
      caseEnabled(argc, argv, "correspondence-chunked-xyz-staging");
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
  const auto row_small_indices = makeStridedIndices(4096, row_small.size(), 3, 5);
  const auto row_medium_indices =
      makeStridedIndices(65536, row_medium.size(), 3, 5);
  const auto row_large_indices =
      makeStridedIndices(262144, row_large.size(), 3, 5);
  const auto row_small_target_indices =
      makeStridedIndices(4096, row_small.size(), 5, 7);
  const auto row_medium_target_indices =
      makeStridedIndices(65536, row_medium.size(), 5, 7);
  const auto row_large_target_indices =
      makeStridedIndices(262144, row_large.size(), 5, 7);
  const auto row_small_correspondences =
      makeStridedCorrespondences(row_small_indices, row_small_target_indices);
  const auto row_medium_correspondences =
      makeStridedCorrespondences(row_medium_indices, row_medium_target_indices);
  const auto row_large_correspondences =
      makeStridedCorrespondences(row_large_indices, row_large_target_indices);
  const auto identity_small_indices = makeIdentityIndices(4096);
  const auto identity_medium_indices = makeIdentityIndices(65536);
  const auto identity_large_indices = makeIdentityIndices(262144);
  const auto reverse_small_indices = makeReverseIndices(4096);
  const auto reverse_medium_indices = makeReverseIndices(65536);
  const auto reverse_large_indices = makeReverseIndices(262144);
  const auto shuffled_small_query =
      makeShuffledIndices(4096, 1103515245ull, 12345ull);
  const auto shuffled_medium_query =
      makeShuffledIndices(65536, 1103515245ull, 12345ull);
  const auto shuffled_large_query =
      makeShuffledIndices(262144, 1103515245ull, 12345ull);
  const auto shuffled_small_match =
      makeShuffledIndices(4096, 1664525ull, 1013904223ull);
  const auto shuffled_medium_match =
      makeShuffledIndices(65536, 1664525ull, 1013904223ull);
  const auto shuffled_large_match =
      makeShuffledIndices(262144, 1664525ull, 1013904223ull);
  const auto row_small_target_selected = selectByIndices(row_small_target, row_small_indices);
  const auto row_medium_target_selected =
      selectByIndices(row_medium_target, row_medium_indices);
  const auto row_large_target_selected =
      selectByIndices(row_large_target, row_large_indices);
  const auto row_small_indexed_target =
      makeTargetForIndexedPairs(row_small, row_small_indices, row_small_target_indices, transform);
  const auto row_medium_indexed_target =
      makeTargetForIndexedPairs(
          row_medium, row_medium_indices, row_medium_target_indices, transform);
  const auto row_large_indexed_target =
      makeTargetForIndexedPairs(row_large, row_large_indices, row_large_target_indices, transform);
  printBanner('=');
  std::cout << "PCL registration/transformation_estimation_2D RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  const bool row_source_mode = caseEnabled(argc, argv, "row-source-fused");
  const bool row_source_direct_mode = caseEnabled(argc, argv, "row-source-direct-gather");
  const bool public_mode = caseEnabled(argc, argv, "ordered-cloud-pair-public");
  const char* dataset_label = source_indexed_pointxyzi_public_mode
                                  ? "synthetic dense PointXYZI source-indexed exact public probe"
                              : source_indexed_generic_pointnormal_256k_public_mode
                                  ? "synthetic dense PointNormal source-indexed generic public 256K long-tail"
                                  : generic_public_mode
                                  ? "synthetic dense PointXYZ-like generic public ordered-cloud pairs"
                                  : generic_mode
                                  ? "synthetic dense PointXYZ-like generic ordered-cloud pairs"
                                  : source_indexed_generic_public_mode
                                  ? (caseEnabled(argc,
                                                 argv,
                                                 "source-indexed-generic-xyz-point-types-public-variance")
                                         ? "synthetic dense PointXYZ-like generic public source-indexed-cloud pairs; variance rerun"
                                         : "synthetic dense PointXYZ-like generic public source-indexed-cloud pairs")
                                  : source_indexed_generic_mode
                                  ? "synthetic dense PointXYZ-like generic source-indexed-cloud pairs"
                                  : dual_indexed_generic_mode
                                  ? "synthetic dense PointXYZ-like generic dual-indexed-cloud pairs"
                                  : correspondence_generic_mode
                                  ? "synthetic dense PointXYZ-like generic correspondence pairs"
                                  : source_indexed_public_mode
                                  ? "synthetic dense PointXYZ public source-indexed-cloud pairs"
                                  : source_indexed_family_ab_mode
                                  ? "synthetic dense PointXYZ source-indexed family A/B pairs"
                                  : dual_indexed_family_ab_mode
                                  ? "synthetic dense PointXYZ dual-indexed family A/B pairs"
                                  : correspondence_public_mode
                                  ? "synthetic dense PointXYZ public correspondence pairs"
                                  : correspondence_family_ab_mode
                                  ? "synthetic dense PointXYZ correspondence family A/B pairs"
                                  : correspondence_staging_profile_mode
                                  ? "synthetic dense PointXYZ correspondence staging profile pairs"
                                  : correspondence_locality_order_profile_mode
                                  ? "synthetic dense PointXYZ correspondence locality/order profile pairs"
                                  : correspondence_component_ablation_mode
                                  ? "synthetic dense PointXYZ correspondence component ablation pairs"
                                  : correspondence_chunked_xyz_staging_mode
                                  ? "synthetic dense PointXYZ correspondence chunked xyz staging pairs"
                                  : row_source_direct_mode
                                  ? "synthetic dense PointXYZ row-source pairs; direct gather"
                                  : row_source_mode
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

  if (caseEnabled(argc, argv, "source-indexed-public")) {
    runPublicSourceIndexedCase("public 2D source-indexed-cloud-pair 4K",
                               row_small,
                               row_small_indices,
                               row_small_target_selected,
                               iterations,
                               warmup_iterations);
    runPublicSourceIndexedCase("public 2D source-indexed-cloud-pair 64K",
                               row_medium,
                               row_medium_indices,
                               row_medium_target_selected,
                               iterations,
                               warmup_iterations);
    runPublicSourceIndexedCase("public 2D source-indexed-cloud-pair 256K",
                               row_large,
                               row_large_indices,
                               row_large_target_selected,
                               iterations,
                               warmup_iterations);
  }

  if (source_indexed_generic_mode) {
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "source-indexed generic 2D PointXYZ->PointXYZ 4K",
        4096,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "source-indexed generic 2D PointXYZ->PointXYZ 64K",
        65536,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "source-indexed generic 2D PointXYZ->PointXYZ 256K",
        262144,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "source-indexed generic 2D PointXYZI->PointXYZI 4K",
        4096,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "source-indexed generic 2D PointXYZI->PointXYZI 64K",
        65536,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "source-indexed generic 2D PointXYZI->PointXYZI 256K",
        262144,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "source-indexed generic 2D PointNormal->PointNormal 4K",
        4096,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "source-indexed generic 2D PointNormal->PointNormal 64K",
        65536,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "source-indexed generic 2D PointNormal->PointNormal 256K",
        262144,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedSourceIndexedGenericCase<pcl::PointXYZINormal,
                                         pcl::PointXYZINormal>(
        "source-indexed generic 2D PointXYZINormal->PointXYZINormal 4K",
        4096,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZINormal,
                                         pcl::PointXYZINormal>(
        "source-indexed generic 2D PointXYZINormal->PointXYZINormal 64K",
        65536,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZINormal,
                                         pcl::PointXYZINormal>(
        "source-indexed generic 2D PointXYZINormal->PointXYZINormal 256K",
        262144,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZ>(
        "source-indexed generic 2D PointXYZI->PointXYZ 64K",
        65536,
        23.0f,
        -23.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZI>(
        "source-indexed generic 2D PointXYZ->PointXYZI 64K",
        65536,
        29.0f,
        -29.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointNormal,
                                         pcl::PointXYZINormal>(
        "source-indexed generic 2D PointNormal->PointXYZINormal 64K",
        65536,
        31.0f,
        -31.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedSourceIndexedGenericCase<pcl::PointXYZINormal,
                                         pcl::PointNormal>(
        "source-indexed generic 2D PointXYZINormal->PointNormal 64K",
        65536,
        37.0f,
        -37.0f,
        transform,
        iterations,
        warmup_iterations);
  }

  if (source_indexed_generic_public_mode) {
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "public source-indexed generic 2D PointXYZ->PointXYZ 4K",
        4096,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "public source-indexed generic 2D PointXYZ->PointXYZ 64K",
        65536,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "public source-indexed generic 2D PointXYZ->PointXYZ 256K",
        262144,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZI->PointXYZI 4K",
        4096,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZI->PointXYZI 64K",
        65536,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZI->PointXYZI 256K",
        262144,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedPublicSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "public source-indexed generic 2D PointNormal->PointNormal 4K",
        4096,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "public source-indexed generic 2D PointNormal->PointNormal 64K",
        65536,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "public source-indexed generic 2D PointNormal->PointNormal 256K",
        262144,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZINormal,
                                               pcl::PointXYZINormal>(
        "public source-indexed generic 2D PointXYZINormal->PointXYZINormal 4K",
        4096,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZINormal,
                                               pcl::PointXYZINormal>(
        "public source-indexed generic 2D PointXYZINormal->PointXYZINormal 64K",
        65536,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZINormal,
                                               pcl::PointXYZINormal>(
        "public source-indexed generic 2D PointXYZINormal->PointXYZINormal 256K",
        262144,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZ>(
        "public source-indexed generic 2D PointXYZI->PointXYZ 64K",
        65536,
        23.0f,
        -23.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZ->PointXYZI 64K",
        65536,
        29.0f,
        -29.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointNormal,
                                               pcl::PointXYZINormal>(
        "public source-indexed generic 2D PointNormal->PointXYZINormal 64K",
        65536,
        31.0f,
        -31.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZINormal,
                                               pcl::PointNormal>(
        "public source-indexed generic 2D PointXYZINormal->PointNormal 64K",
        65536,
        37.0f,
        -37.0f,
        transform,
        iterations,
        warmup_iterations);
  }

  if (source_indexed_generic_pointnormal_256k_public_mode) {
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "public source-indexed generic 2D PointNormal->PointNormal 256K",
        262144,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
  }

  if (source_indexed_pointxyzi_public_mode) {
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZI->PointXYZI 4K",
        4096,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZI->PointXYZI 64K",
        65536,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedPublicSourceIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "public source-indexed generic 2D PointXYZI->PointXYZI 256K",
        262144,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
  }

  if (dual_indexed_generic_mode) {
    runGeneratedDualIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "dual-indexed generic 2D PointXYZ->PointXYZ 4K",
        4096,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "dual-indexed generic 2D PointXYZ->PointXYZ 64K",
        65536,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "dual-indexed generic 2D PointXYZ->PointXYZ 256K",
        262144,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedDualIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "dual-indexed generic 2D PointXYZI->PointXYZI 4K",
        4096,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "dual-indexed generic 2D PointXYZI->PointXYZI 64K",
        65536,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "dual-indexed generic 2D PointXYZI->PointXYZI 256K",
        262144,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedDualIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "dual-indexed generic 2D PointNormal->PointNormal 4K",
        4096,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "dual-indexed generic 2D PointNormal->PointNormal 64K",
        65536,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "dual-indexed generic 2D PointNormal->PointNormal 256K",
        262144,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedDualIndexedGenericCase<pcl::PointXYZINormal,
                                       pcl::PointXYZINormal>(
        "dual-indexed generic 2D PointXYZINormal->PointXYZINormal 4K",
        4096,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZINormal,
                                       pcl::PointXYZINormal>(
        "dual-indexed generic 2D PointXYZINormal->PointXYZINormal 64K",
        65536,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZINormal,
                                       pcl::PointXYZINormal>(
        "dual-indexed generic 2D PointXYZINormal->PointXYZINormal 256K",
        262144,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedDualIndexedGenericCase<pcl::PointXYZI, pcl::PointXYZ>(
        "dual-indexed generic 2D PointXYZI->PointXYZ 64K",
        65536,
        23.0f,
        -23.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZ, pcl::PointXYZI>(
        "dual-indexed generic 2D PointXYZ->PointXYZI 64K",
        65536,
        29.0f,
        -29.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointNormal,
                                       pcl::PointXYZINormal>(
        "dual-indexed generic 2D PointNormal->PointXYZINormal 64K",
        65536,
        31.0f,
        -31.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedDualIndexedGenericCase<pcl::PointXYZINormal,
                                       pcl::PointNormal>(
        "dual-indexed generic 2D PointXYZINormal->PointNormal 64K",
        65536,
        37.0f,
        -37.0f,
        transform,
        iterations,
        warmup_iterations);
  }

  if (correspondence_generic_mode) {
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "correspondence generic 2D PointXYZ->PointXYZ 4K",
        4096,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "correspondence generic 2D PointXYZ->PointXYZ 64K",
        65536,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZ, pcl::PointXYZ>(
        "correspondence generic 2D PointXYZ->PointXYZ 256K",
        262144,
        11.0f,
        -11.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedCorrespondenceGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "correspondence generic 2D PointXYZI->PointXYZI 4K",
        4096,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "correspondence generic 2D PointXYZI->PointXYZI 64K",
        65536,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZI, pcl::PointXYZI>(
        "correspondence generic 2D PointXYZI->PointXYZI 256K",
        262144,
        13.0f,
        -13.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedCorrespondenceGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "correspondence generic 2D PointNormal->PointNormal 4K",
        4096,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "correspondence generic 2D PointNormal->PointNormal 64K",
        65536,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointNormal, pcl::PointNormal>(
        "correspondence generic 2D PointNormal->PointNormal 256K",
        262144,
        17.0f,
        -17.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedCorrespondenceGenericCase<pcl::PointXYZINormal,
                                          pcl::PointXYZINormal>(
        "correspondence generic 2D PointXYZINormal->PointXYZINormal 4K",
        4096,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZINormal,
                                          pcl::PointXYZINormal>(
        "correspondence generic 2D PointXYZINormal->PointXYZINormal 64K",
        65536,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZINormal,
                                          pcl::PointXYZINormal>(
        "correspondence generic 2D PointXYZINormal->PointXYZINormal 256K",
        262144,
        19.0f,
        -19.0f,
        transform,
        iterations,
        warmup_iterations);

    runGeneratedCorrespondenceGenericCase<pcl::PointXYZI, pcl::PointXYZ>(
        "correspondence generic 2D PointXYZI->PointXYZ 64K",
        65536,
        23.0f,
        -23.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZ, pcl::PointXYZI>(
        "correspondence generic 2D PointXYZ->PointXYZI 64K",
        65536,
        29.0f,
        -29.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointNormal,
                                          pcl::PointXYZINormal>(
        "correspondence generic 2D PointNormal->PointXYZINormal 64K",
        65536,
        31.0f,
        -31.0f,
        transform,
        iterations,
        warmup_iterations);
    runGeneratedCorrespondenceGenericCase<pcl::PointXYZINormal,
                                          pcl::PointNormal>(
        "correspondence generic 2D PointXYZINormal->PointNormal 64K",
        65536,
        37.0f,
        -37.0f,
        transform,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-public")) {
    runPublicCorrespondenceCase("public 2D correspondence-pair 4K",
                                row_small,
                                row_small_indexed_target,
                                row_small_correspondences,
                                iterations,
                                warmup_iterations);
    runPublicCorrespondenceCase("public 2D correspondence-pair 64K",
                                row_medium,
                                row_medium_indexed_target,
                                row_medium_correspondences,
                                iterations,
                                warmup_iterations);
    runPublicCorrespondenceCase("public 2D correspondence-pair 256K",
                                row_large,
                                row_large_indexed_target,
                                row_large_correspondences,
                                iterations,
                                warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-family-ab")) {
    runCorrespondenceFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D correspondence-pair 4K",
        row_small,
        row_small_indexed_target,
        row_small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceFamilyABDirectCase(
        "direct correspondence public 2D correspondence-pair 4K",
        row_small,
        row_small_indexed_target,
        row_small_correspondences,
        iterations,
        warmup_iterations);

    runCorrespondenceFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D correspondence-pair 64K",
        row_medium,
        row_medium_indexed_target,
        row_medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceFamilyABDirectCase(
        "direct correspondence public 2D correspondence-pair 64K",
        row_medium,
        row_medium_indexed_target,
        row_medium_correspondences,
        iterations,
        warmup_iterations);

    runCorrespondenceFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D correspondence-pair 256K",
        row_large,
        row_large_indexed_target,
        row_large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceFamilyABDirectCase(
        "direct correspondence public 2D correspondence-pair 256K",
        row_large,
        row_large_indexed_target,
        row_large_correspondences,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-staging-profile")) {
    runCorrespondenceFamilyABDirectCase(
        "direct correspondence public 2D correspondence-pair 4K",
        row_small,
        row_small_indexed_target,
        row_small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceStagedDualIndexedCase(
        "staged-dual-indexed public 2D correspondence-pair 4K",
        row_small,
        row_small_indexed_target,
        row_small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D correspondence-pair 4K",
        row_small,
        row_small_indexed_target,
        row_small_correspondences,
        iterations,
        warmup_iterations);

    runCorrespondenceFamilyABDirectCase(
        "direct correspondence public 2D correspondence-pair 64K",
        row_medium,
        row_medium_indexed_target,
        row_medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceStagedDualIndexedCase(
        "staged-dual-indexed public 2D correspondence-pair 64K",
        row_medium,
        row_medium_indexed_target,
        row_medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D correspondence-pair 64K",
        row_medium,
        row_medium_indexed_target,
        row_medium_correspondences,
        iterations,
        warmup_iterations);

    runCorrespondenceFamilyABDirectCase(
        "direct correspondence public 2D correspondence-pair 256K",
        row_large,
        row_large_indexed_target,
        row_large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceStagedDualIndexedCase(
        "staged-dual-indexed public 2D correspondence-pair 256K",
        row_large,
        row_large_indexed_target,
        row_large_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D correspondence-pair 256K",
        row_large,
        row_large_indexed_target,
        row_large_correspondences,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-locality-order-profile")) {
    const auto run_profile = [&](const std::string& profile,
                                 const std::string& size_label,
                                 const auto& source,
                                 const pcl::Indices& query_indices,
                                 const pcl::Indices& match_indices) {
      const auto target = makeTargetForIndexedPairs(
          source, query_indices, match_indices, transform);
      const auto correspondences =
          makeStridedCorrespondences(query_indices, match_indices);
      runCorrespondenceLocalityOrderProfileCase(
          profile,
          size_label,
          source,
          target,
          correspondences,
          iterations,
          warmup_iterations);
    };

    run_profile("identity", "4K", row_small, identity_small_indices, identity_small_indices);
    run_profile("strided", "4K", row_small, row_small_indices, row_small_target_indices);
    run_profile("reverse", "4K", row_small, reverse_small_indices, reverse_small_indices);
    run_profile("shuffled", "4K", row_small, shuffled_small_query, shuffled_small_match);

    run_profile(
        "identity", "64K", row_medium, identity_medium_indices, identity_medium_indices);
    run_profile("strided", "64K", row_medium, row_medium_indices, row_medium_target_indices);
    run_profile("reverse", "64K", row_medium, reverse_medium_indices, reverse_medium_indices);
    run_profile("shuffled", "64K", row_medium, shuffled_medium_query, shuffled_medium_match);

    run_profile(
        "identity", "256K", row_large, identity_large_indices, identity_large_indices);
    run_profile("strided", "256K", row_large, row_large_indices, row_large_target_indices);
    run_profile("reverse", "256K", row_large, reverse_large_indices, reverse_large_indices);
    run_profile("shuffled", "256K", row_large, shuffled_large_query, shuffled_large_match);
  }

  if (caseEnabled(argc, argv, "correspondence-component-ablation")) {
    runCorrespondenceComponentAblationCase("strided",
                                           "4K",
                                           row_small,
                                           row_small_indexed_target,
                                           row_small_correspondences,
                                           iterations,
                                           warmup_iterations);
    runCorrespondenceComponentAblationCase("strided",
                                           "64K",
                                           row_medium,
                                           row_medium_indexed_target,
                                           row_medium_correspondences,
                                           iterations,
                                           warmup_iterations);
    runCorrespondenceComponentAblationCase("strided",
                                           "256K",
                                           row_large,
                                           row_large_indexed_target,
                                           row_large_correspondences,
                                           iterations,
                                           warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-chunked-xyz-staging")) {
    runCorrespondenceChunkedXYZStagingProfileCase("4K",
                                                  row_small,
                                                  row_small_indexed_target,
                                                  row_small_correspondences,
                                                  iterations,
                                                  warmup_iterations);
    runCorrespondenceChunkedXYZStagingProfileCase("64K",
                                                  row_medium,
                                                  row_medium_indexed_target,
                                                  row_medium_correspondences,
                                                  iterations,
                                                  warmup_iterations);
    runCorrespondenceChunkedXYZStagingProfileCase("256K",
                                                  row_large,
                                                  row_large_indexed_target,
                                                  row_large_correspondences,
                                                  iterations,
                                                  warmup_iterations);
  }

  if (caseEnabled(argc, argv, "source-indexed-family-ab")) {
    runSourceIndexedFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D source-indexed-cloud-pair 4K",
        row_small,
        row_small_indices,
        row_small_target_selected,
        iterations,
        warmup_iterations);
    runSourceIndexedFamilyABDirectCase(
        "direct source-indexed public 2D source-indexed-cloud-pair 4K",
        row_small,
        row_small_indices,
        row_small_target_selected,
        iterations,
        warmup_iterations);

    runSourceIndexedFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D source-indexed-cloud-pair 64K",
        row_medium,
        row_medium_indices,
        row_medium_target_selected,
        iterations,
        warmup_iterations);
    runSourceIndexedFamilyABDirectCase(
        "direct source-indexed public 2D source-indexed-cloud-pair 64K",
        row_medium,
        row_medium_indices,
        row_medium_target_selected,
        iterations,
        warmup_iterations);

    runSourceIndexedFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D source-indexed-cloud-pair 256K",
        row_large,
        row_large_indices,
        row_large_target_selected,
        iterations,
        warmup_iterations);
    runSourceIndexedFamilyABDirectCase(
        "direct source-indexed public 2D source-indexed-cloud-pair 256K",
        row_large,
        row_large_indices,
        row_large_target_selected,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "dual-indexed-family-ab")) {
    runDualIndexedFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D dual-indexed-cloud-pair 4K",
        row_small,
        row_small_indices,
        row_small_indexed_target,
        row_small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedFamilyABDirectCase(
        "direct dual-indexed public 2D dual-indexed-cloud-pair 4K",
        row_small,
        row_small_indices,
        row_small_indexed_target,
        row_small_target_indices,
        iterations,
        warmup_iterations);

    runDualIndexedFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D dual-indexed-cloud-pair 64K",
        row_medium,
        row_medium_indices,
        row_medium_indexed_target,
        row_medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedFamilyABDirectCase(
        "direct dual-indexed public 2D dual-indexed-cloud-pair 64K",
        row_medium,
        row_medium_indices,
        row_medium_indexed_target,
        row_medium_target_indices,
        iterations,
        warmup_iterations);

    runDualIndexedFamilyABMaterializeOrderedCase(
        "materialize-ordered public 2D dual-indexed-cloud-pair 256K",
        row_large,
        row_large_indices,
        row_large_indexed_target,
        row_large_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedFamilyABDirectCase(
        "direct dual-indexed public 2D dual-indexed-cloud-pair 256K",
        row_large,
        row_large_indices,
        row_large_indexed_target,
        row_large_target_indices,
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
                       row_small_indexed_target,
                       row_small_target_indices,
                       iterations,
                       warmup_iterations);
    runDualIndexedCase("fused 2D correlation dual-indexed-cloud-pair 64K",
                       row_medium,
                       row_medium_indices,
                       row_medium_indexed_target,
                       row_medium_target_indices,
                       iterations,
                       warmup_iterations);
    runDualIndexedCase("fused 2D correlation dual-indexed-cloud-pair 256K",
                       row_large,
                       row_large_indices,
                       row_large_indexed_target,
                       row_large_target_indices,
                       iterations,
                       warmup_iterations);

    runCorrespondenceCase("fused 2D correlation correspondence-pair 4K",
                          row_small,
                          row_small_indexed_target,
                          row_small_correspondences,
                          iterations,
                          warmup_iterations);
    runCorrespondenceCase("fused 2D correlation correspondence-pair 64K",
                          row_medium,
                          row_medium_indexed_target,
                          row_medium_correspondences,
                          iterations,
                          warmup_iterations);
    runCorrespondenceCase("fused 2D correlation correspondence-pair 256K",
                          row_large,
                          row_large_indexed_target,
                          row_large_correspondences,
                          iterations,
                          warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-direct-gather")) {
    runSourceIndexedDirectGatherCase(
        "direct-gather 2D correlation source-indexed-cloud-pair 4K",
        row_small,
        row_small_indices,
        row_small_target_selected,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectGatherCase(
        "direct-gather 2D correlation source-indexed-cloud-pair 64K",
        row_medium,
        row_medium_indices,
        row_medium_target_selected,
        iterations,
        warmup_iterations);
    runSourceIndexedDirectGatherCase(
        "direct-gather 2D correlation source-indexed-cloud-pair 256K",
        row_large,
        row_large_indices,
        row_large_target_selected,
        iterations,
        warmup_iterations);

    runDualIndexedDirectGatherCase(
        "direct-gather 2D correlation dual-indexed-cloud-pair 4K",
        row_small,
        row_small_indices,
        row_small_indexed_target,
        row_small_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectGatherCase(
        "direct-gather 2D correlation dual-indexed-cloud-pair 64K",
        row_medium,
        row_medium_indices,
        row_medium_indexed_target,
        row_medium_target_indices,
        iterations,
        warmup_iterations);
    runDualIndexedDirectGatherCase(
        "direct-gather 2D correlation dual-indexed-cloud-pair 256K",
        row_large,
        row_large_indices,
        row_large_indexed_target,
        row_large_target_indices,
        iterations,
        warmup_iterations);

    runCorrespondenceDirectGatherCase(
        "direct-gather 2D correlation correspondence-pair 4K",
        row_small,
        row_small_indexed_target,
        row_small_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectGatherCase(
        "direct-gather 2D correlation correspondence-pair 64K",
        row_medium,
        row_medium_indexed_target,
        row_medium_correspondences,
        iterations,
        warmup_iterations);
    runCorrespondenceDirectGatherCase(
        "direct-gather 2D correlation correspondence-pair 256K",
        row_large,
        row_large_indexed_target,
        row_large_correspondences,
        iterations,
        warmup_iterations);
  }

  if (generic_mode || generic_public_mode) {
    // 泛型点型输入仅在专用 case-filter 下构造，避免影响旧 public / row-source
    // bench 的内存占用、输入构造和证据边界。
    const auto generic_xyz = support::makeXYZLikeCloud<pcl::PointXYZ>(4096, 11.0f);
    const auto generic_xyz_large =
        support::makeXYZLikeCloud<pcl::PointXYZ>(65536, 11.0f);
    const auto generic_xyz_huge =
        support::makeXYZLikeCloud<pcl::PointXYZ>(262144, 11.0f);
    const auto generic_xyzi = support::makeXYZLikeCloud<pcl::PointXYZI>(4096, 13.0f);
    const auto generic_xyzi_large =
        support::makeXYZLikeCloud<pcl::PointXYZI>(65536, 13.0f);
    const auto generic_xyzi_huge =
        support::makeXYZLikeCloud<pcl::PointXYZI>(262144, 13.0f);
    const auto generic_normal =
        support::makeXYZLikeCloud<pcl::PointNormal>(4096, 17.0f);
    const auto generic_normal_large =
        support::makeXYZLikeCloud<pcl::PointNormal>(65536, 17.0f);
    const auto generic_normal_huge =
        support::makeXYZLikeCloud<pcl::PointNormal>(262144, 17.0f);
    const auto generic_xyzinormal =
        support::makeXYZLikeCloud<pcl::PointXYZINormal>(4096, 19.0f);
    const auto generic_xyzinormal_large =
        support::makeXYZLikeCloud<pcl::PointXYZINormal>(65536, 19.0f);
    const auto generic_xyzinormal_huge =
        support::makeXYZLikeCloud<pcl::PointXYZINormal>(262144, 19.0f);
    const auto generic_xyz_target =
        support::transformCloud2DTo<pcl::PointXYZ, pcl::PointXYZ>(
            generic_xyz, transform, -11.0f);
    const auto generic_xyz_large_target =
        support::transformCloud2DTo<pcl::PointXYZ, pcl::PointXYZ>(
            generic_xyz_large, transform, -11.0f);
    const auto generic_xyz_huge_target =
        support::transformCloud2DTo<pcl::PointXYZ, pcl::PointXYZ>(
            generic_xyz_huge, transform, -11.0f);
    const auto generic_xyzi_target =
        support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZI>(
            generic_xyzi, transform, -13.0f);
    const auto generic_xyzi_large_target =
        support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZI>(
            generic_xyzi_large, transform, -13.0f);
    const auto generic_xyzi_huge_target =
        support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZI>(
            generic_xyzi_huge, transform, -13.0f);
    const auto generic_normal_target =
        support::transformCloud2DTo<pcl::PointNormal, pcl::PointNormal>(
            generic_normal, transform, -17.0f);
    const auto generic_normal_large_target =
        support::transformCloud2DTo<pcl::PointNormal, pcl::PointNormal>(
            generic_normal_large, transform, -17.0f);
    const auto generic_normal_huge_target =
        support::transformCloud2DTo<pcl::PointNormal, pcl::PointNormal>(
            generic_normal_huge, transform, -17.0f);
    const auto generic_xyzinormal_target =
        support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointXYZINormal>(
            generic_xyzinormal, transform, -19.0f);
    const auto generic_xyzinormal_large_target =
        support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointXYZINormal>(
            generic_xyzinormal_large, transform, -19.0f);
    const auto generic_xyzinormal_huge_target =
        support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointXYZINormal>(
            generic_xyzinormal_huge, transform, -19.0f);
    const auto generic_xyzi_to_xyz_target =
        support::transformCloud2DTo<pcl::PointXYZI, pcl::PointXYZ>(
            generic_xyzi_large, transform, -8.0f);
    const auto generic_xyz_to_xyzi_target =
        support::transformCloud2DTo<pcl::PointXYZ, pcl::PointXYZI>(
            generic_xyz_large, transform, -6.0f);
    const auto generic_normal_to_xyzinormal_target =
        support::transformCloud2DTo<pcl::PointNormal, pcl::PointXYZINormal>(
            generic_normal_large, transform, -4.0f);
    const auto generic_xyzinormal_to_normal_target =
        support::transformCloud2DTo<pcl::PointXYZINormal, pcl::PointNormal>(
            generic_xyzinormal_large, transform, -2.0f);

    if (generic_mode) {
      runGenericCase("generic 2D PointXYZ->PointXYZ 4K",
                   generic_xyz,
                   generic_xyz_target,
                   iterations,
                   warmup_iterations);
    runGenericCase("generic 2D PointXYZ->PointXYZ 64K",
                       generic_xyz_large,
                       generic_xyz_large_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZ->PointXYZ 256K",
                       generic_xyz_huge,
                       generic_xyz_huge_target,
                       iterations,
                       warmup_iterations);

    runGenericCase("generic 2D PointXYZI->PointXYZI 4K",
                       generic_xyzi,
                       generic_xyzi_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZI->PointXYZI 64K",
                       generic_xyzi_large,
                       generic_xyzi_large_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZI->PointXYZI 256K",
                       generic_xyzi_huge,
                       generic_xyzi_huge_target,
                       iterations,
                       warmup_iterations);

    runGenericCase("generic 2D PointNormal->PointNormal 4K",
                       generic_normal,
                       generic_normal_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointNormal->PointNormal 64K",
                       generic_normal_large,
                       generic_normal_large_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointNormal->PointNormal 256K",
                       generic_normal_huge,
                       generic_normal_huge_target,
                       iterations,
                       warmup_iterations);

    runGenericCase("generic 2D PointXYZINormal->PointXYZINormal 4K",
                       generic_xyzinormal,
                       generic_xyzinormal_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZINormal->PointXYZINormal 64K",
                       generic_xyzinormal_large,
                       generic_xyzinormal_large_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZINormal->PointXYZINormal 256K",
                       generic_xyzinormal_huge,
                       generic_xyzinormal_huge_target,
                       iterations,
                       warmup_iterations);

    runGenericCase("generic 2D PointXYZI->PointXYZ 64K",
                       generic_xyzi_large,
                       generic_xyzi_to_xyz_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZ->PointXYZI 64K",
                       generic_xyz_large,
                       generic_xyz_to_xyzi_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointNormal->PointXYZINormal 64K",
                       generic_normal_large,
                       generic_normal_to_xyzinormal_target,
                       iterations,
                       warmup_iterations);
    runGenericCase("generic 2D PointXYZINormal->PointNormal 64K",
                   generic_xyzinormal_large,
                   generic_xyzinormal_to_normal_target,
                   iterations,
                   warmup_iterations);
    }

    if (generic_public_mode) {
      runPublicGenericCase("public generic 2D PointXYZ->PointXYZ 4K",
                           generic_xyz,
                           generic_xyz_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZ->PointXYZ 64K",
                           generic_xyz_large,
                           generic_xyz_large_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZ->PointXYZ 256K",
                           generic_xyz_huge,
                           generic_xyz_huge_target,
                           iterations,
                           warmup_iterations);

      runPublicGenericCase("public generic 2D PointXYZI->PointXYZI 4K",
                           generic_xyzi,
                           generic_xyzi_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZI->PointXYZI 64K",
                           generic_xyzi_large,
                           generic_xyzi_large_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZI->PointXYZI 256K",
                           generic_xyzi_huge,
                           generic_xyzi_huge_target,
                           iterations,
                           warmup_iterations);

      runPublicGenericCase("public generic 2D PointNormal->PointNormal 4K",
                           generic_normal,
                           generic_normal_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointNormal->PointNormal 64K",
                           generic_normal_large,
                           generic_normal_large_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointNormal->PointNormal 256K",
                           generic_normal_huge,
                           generic_normal_huge_target,
                           iterations,
                           warmup_iterations);

      runPublicGenericCase("public generic 2D PointXYZINormal->PointXYZINormal 4K",
                           generic_xyzinormal,
                           generic_xyzinormal_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZINormal->PointXYZINormal 64K",
                           generic_xyzinormal_large,
                           generic_xyzinormal_large_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZINormal->PointXYZINormal 256K",
                           generic_xyzinormal_huge,
                           generic_xyzinormal_huge_target,
                           iterations,
                           warmup_iterations);

      runPublicGenericCase("public generic 2D PointXYZI->PointXYZ 64K",
                           generic_xyzi_large,
                           generic_xyzi_to_xyz_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZ->PointXYZI 64K",
                           generic_xyz_large,
                           generic_xyz_to_xyzi_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointNormal->PointXYZINormal 64K",
                           generic_normal_large,
                           generic_normal_to_xyzinormal_target,
                           iterations,
                           warmup_iterations);
      runPublicGenericCase("public generic 2D PointXYZINormal->PointNormal 64K",
                           generic_xyzinormal_large,
                           generic_xyzinormal_to_normal_target,
                           iterations,
                           warmup_iterations);
    }
  }

  printBanner('=');
  return 0;
}
