/*
 * 本文件做什么：
 * 这个 benchmark（性能测试）只度量 test-rvv diagnostic（测试专用诊断）入口，
 * 不代表 production（生产源码）已经接入。std 构建会自然走 scalar reference
 *（标量参考链路）；RVV 构建在规模、VLEN（向量寄存器位宽）和 offset（字节偏移）
 * gate（可失败验收条件）满足时走 RVV staging（暂存阶段）。
 *
 * 输出合同：
 * 每个 case 输出 avg ms/iter、Total Time 和 checksum（校验和），供 QEMU correctness
 *（QEMU 正确性验证，不代表真实性能）与 board evidence（板卡证据）共用解析脚本。
 */

#include "transformation_estimation_point_to_plane_lls_diag.hpp"

#include <pcl/common/transforms.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace diag = pcl::registration::rvv_te_pt2plane_lls_diag;

namespace {

pcl::PointCloud<pcl::PointNormal>
makeCloudWithAtLeast(const std::size_t target_size)
{
  // bench 输入固定为解析曲面，避免随机数让 std/RVV checksum（校验和）不可复现。
  const int radius = static_cast<int>(std::ceil(std::sqrt(target_size) / 2.0));
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
  for (int ix = -radius; ix <= radius; ++ix) {
    for (int iy = -radius; iy <= radius; ++iy) {
      const float x = static_cast<float>(ix) * 0.025f;
      const float y = static_cast<float>(iy) * 0.025f;
      pcl::PointNormal point;
      point.x = x;
      point.y = y;
      point.z = 0.08f * x * x + 0.16f * x * y - 0.22f * y + 0.7f;
      point.normal_x = -0.16f * x - 0.16f * y;
      point.normal_y = -0.16f * x + 0.22f;
      point.normal_z = 1.0f;
      const float norm = std::sqrt(point.normal_x * point.normal_x +
                                   point.normal_y * point.normal_y +
                                   point.normal_z * point.normal_z);
      point.normal_x /= norm;
      point.normal_y /= norm;
      point.normal_z /= norm;
      cloud.push_back(point);
      if (cloud.size() == target_size) {
        cloud.width = cloud.size();
        return cloud;
      }
    }
  }
  cloud.width = cloud.size();
  return cloud;
}

Eigen::Matrix4f
makeTransform()
{
  // 与专项测试保持同一温和刚体变换，减少 bench 与 correctness case 的解释差异。
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform.row(0) << 0.9938f, 0.0988f, 0.0517f, 0.1000f;
  transform.row(1) << -0.0997f, 0.9949f, 0.0149f, -0.2000f;
  transform.row(2) << -0.0500f, -0.0200f, 0.9986f, 0.3000f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

pcl::Correspondences
makeCorrespondences(const std::size_t n)
{
  // 这个 subset 专门给 gather bench 使用；它不是性能最佳情况，而是验证非连续访问成本。
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  for (std::size_t i = 3; i < n; i += 5)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  return correspondences;
}

struct BenchResult {
  std::string name;
  double average_ms = 0.0;
  double total_ms = 0.0;
  double checksum = 0.0;
};

template <typename Fn>
BenchResult
runCase(const std::string& name, const int iterations, Fn&& fn)
{
  // runCase 统一输出合同需要的 avg、Total Time 和 checksum，供 QEMU 与板卡日志共用。
  double checksum = 0.0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto stop = std::chrono::steady_clock::now();
  const double total_ms =
      std::chrono::duration<double, std::milli>(stop - start).count();
  return BenchResult{name, total_ms / iterations, total_ms, checksum};
}

} // namespace

int
main()
{
  constexpr int kIterations = 20;
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "Dataset: synthetic PointNormal point-to-plane LLS normal-equation diagnostic\n";
  std::cout << "Iterations: " << kIterations << "\n";
#ifdef __RVV10__
  std::cout << "Build: rvv\n";
#else
  std::cout << "Build: std\n";
#endif

  std::vector<BenchResult> results;
  for (const auto n : {65536u, 262144u}) {
    pcl::PointCloud<pcl::PointNormal> source = makeCloudWithAtLeast(n);
    pcl::PointCloud<pcl::PointNormal> target;
    pcl::transformPointCloudWithNormals(source, target, makeTransform());

    results.push_back(runCase(
        "lls normal-equation full-cloud pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              diag::estimate_candidate_full(source, target, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Correspondences correspondences = makeCorrespondences(source.size());
    results.push_back(runCase(
        "lls normal-equation correspondences pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_correspondences(
              source, target, correspondences, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));
  }

  double checksum = 0.0;
  double total = 0.0;
  for (const auto& result : results) {
    checksum += result.checksum;
    total += result.total_ms;
    std::cout << result.name << ": " << result.average_ms << " ms/iter\n";
    std::cout << "  Total Time: " << result.total_ms << " ms\n";
    std::cout << "  Checksum: " << result.checksum << "\n";
  }
  std::cout << "Total Time: " << total << " ms\n";
  std::cout << "Checksum: " << checksum << "\n";
  return 0;
}
