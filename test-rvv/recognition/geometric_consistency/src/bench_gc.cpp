/*
 * 本文件做什么：
 * 这是 geometric_consistency topic 的 bench 入口。Std build 运行 scalar
 * reference；RVV build 运行 pairwise-consistency candidate。它度量的是固定 consensus set
 * 上的 batch predicate，不包含 production `clusterCorrespondences()`、排序、RANSAC 或
 * transformation 输出。
 *
 * 证据边界：
 * bench 只证明局部谓词是否值得继续，不把结果写成 production direct。QEMU 只用于
 * 构建、正确性和日志形状；性能结论必须来自 board（板卡）或目标硬件。
 */

#include "gc.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace gc = pcl::test::geometric_consistency_rvv;

namespace
{

template <typename Fn>
double
timeKernel(Fn&& fn, const int iterations, const int warmup_iterations)
{
  for (int i = 0; i < warmup_iterations; ++i)
    fn();
  const auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    fn();
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count() /
         static_cast<double>(iterations);
}

std::vector<int>
makeConsensusIndices()
{
  return {3, 11, 17, 29, 31, 43, 47, 59, 61, 67, 71, 83, 89, 97, 101, 109};
}

void
addClusterGrowthNoise(gc::PackedCorrespondenceCloud& cloud)
{
  for (std::size_t index = 37; index < cloud.size(); index += 97)
    gc::injectSceneNoise(cloud, index, 0.18f, 0.0f, 0.0f);
}

void
runBatchCase(const std::size_t count,
             const int iterations,
             const int warmup_iterations,
             const float gc_size)
{
  auto cloud = gc::makeSyntheticPackedCloud(count);
  const auto consensus = makeConsensusIndices();
  for (const auto index : {90u, 101u, 127u, 139u})
    gc::injectSceneNoise(cloud, index, 0.16f, 0.0f, 0.0f);

  std::size_t candidate_count = 0;
  std::size_t vector_chunks = 0;
  const double micros = timeKernel(
      [&] {
        std::size_t local_count = 0;
        std::size_t local_chunks = 0;
        const auto path = gc::countConsistentCandidatesCandidate(
            cloud, consensus, gc_size, local_count, &local_chunks);
        candidate_count = local_count;
        vector_chunks = local_chunks;
        (void)path;
      },
      iterations,
      warmup_iterations);

  const auto checksum = gc::checksumCount(candidate_count);
  std::cout << "Dataset: pairwise distance consistency diagnostic; correspondences=" << count
            << "; consensus=" << consensus.size() << "\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << "pairwise_consistency_batch: " << std::fixed << std::setprecision(6) << micros
            << " ms/iter\n";
  std::cout << "  Total Time: " << (micros * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", consistent: " << candidate_count
            << ", vector_chunks: " << vector_chunks << "\n";
}

void
runGrowthCase(const std::size_t count,
              const int iterations,
              const int warmup_iterations,
              const float gc_size)
{
  auto cloud = gc::makeSyntheticPackedCloud(count);
  addClusterGrowthNoise(cloud);

  gc::ClusterGrowthResult result;
  const double micros = timeKernel(
      [&] {
        gc::ClusterGrowthResult local_result;
        const auto path = gc::clusterGrowthCandidate(cloud, gc_size, 3, local_result);
        result = std::move(local_result);
        (void)path;
      },
      iterations,
      warmup_iterations);

  const auto checksum = gc::checksumClusters(result);
  std::cout << "Dataset: cluster growth diagnostic; correspondences=" << count
            << "; gc_threshold=3\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << "cluster_growth: " << std::fixed << std::setprecision(6) << micros
            << " ms/iter\n";
  std::cout << "  Total Time: " << (micros * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", clusters: " << result.clusters.size()
            << ", accepted: " << result.accepted_correspondences
            << ", vector_chunks: " << result.vector_chunks << "\n";
}

} // namespace

int
main(int argc, char** argv)
{
  const std::size_t count = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 4096;
  const int iterations = argc > 2 ? std::atoi(argv[2]) : 200;
  const int warmup_iterations = argc > 3 ? std::atoi(argv[3]) : 5;
  const float gc_size = argc > 4 ? std::strtof(argv[4], nullptr) : 0.03f;
  const std::string mode = argc > 5 ? argv[5] : "pairwise";

  if (mode == "growth")
    runGrowthCase(count, iterations, warmup_iterations, gc_size);
  else
    runBatchCase(count, iterations, warmup_iterations, gc_size);
  return 0;
}

