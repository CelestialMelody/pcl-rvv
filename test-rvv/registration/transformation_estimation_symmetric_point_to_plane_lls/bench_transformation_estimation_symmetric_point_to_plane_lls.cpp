/*
 * 本文件做什么：
 * 这个 benchmark（性能测试）同时度量 production direct（真实生产入口分流）和
 * test-rvv diagnostic（测试专用诊断）入口。production direct case 调用公开 estimator；
 * diagnostic case 保留用于和上一轮证据对照。
 *
 * 测量边界：
 * 输入 cloud、target 和 correspondences 在计时前构造完成。每次迭代测量 candidate
 * estimate（候选估计）本身：normal-equation（法方程）构造、Eigen LDLT solve
 *（Eigen LDLT 求解器）和 constructTransformationMatrix（构造变换矩阵）。对应关系
 * case（correspondences case，按 index_query/index_match 指定点对）包含 candidate
 * 内部的 index 展开，因为这是当前 gather（离散加载）方案为了进入 RVV 必须付出的入口成本。
 */

#include "transformation_estimation_symmetric_point_to_plane_lls_diag.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_symmetric_point_to_plane_lls.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace diag = pcl::registration::rvv_te_symmetric_pt2plane_lls_diag;

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
      point.z = 0.07f * x * x + 0.12f * x * y - 0.18f * y + 0.75f;
      point.normal_x = -0.14f * x - 0.12f * y;
      point.normal_y = -0.12f * x + 0.18f;
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
  transform.row(0) << 0.9952f, 0.0867f, 0.0452f, 0.0800f;
  transform.row(1) << -0.0876f, 0.9960f, 0.0170f, -0.1300f;
  transform.row(2) << -0.0435f, -0.0208f, 0.9988f, 0.2200f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

void
flipSomeNormals(pcl::PointCloud<pcl::PointNormal>& cloud)
{
  // 固定翻转比例用于暴露 enforce_same_direction_normals（同向法线）分支成本。
  for (std::size_t i = 5; i < cloud.size(); i += 17) {
    cloud[i].normal_x = -cloud[i].normal_x;
    cloud[i].normal_y = -cloud[i].normal_y;
    cloud[i].normal_z = -cloud[i].normal_z;
  }
}

template <typename PointT>
pcl::PointCloud<PointT>
copyAsGenericNormalCloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // 这个转换只改变 AoS（结构数组）布局，用于度量 traits-gated 泛型 normal production
  // dispatch（分流逻辑）的真实公开入口成本。
  pcl::PointCloud<PointT> cloud;
  cloud.height = source.height;
  cloud.width = source.width;
  cloud.is_dense = source.is_dense;
  cloud.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    PointT point;
    point.x = source[i].x;
    point.y = source[i].y;
    point.z = source[i].z;
    point.normal_x = source[i].normal_x;
    point.normal_y = source[i].normal_y;
    point.normal_z = source[i].normal_z;
    point.intensity = static_cast<float>(i % 19) * 0.01f;
    cloud.push_back(point);
  }
  cloud.width = cloud.size();
  return cloud;
}

pcl::Correspondences
makeCorrespondences(const std::size_t n)
{
  // 这个确定性子集（subset）专门给 gather bench（离散加载性能测试）使用。它不是最佳
  // 访问形态，而是让对应关系索引路径暴露 index 展开和非连续读点字段的成本。
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0f);
  for (std::size_t i = 3; i < n; i += 5)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0f);
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
  // runCase 统一输出 avg、Total Time 和 checksum，供 QEMU 与板卡日志共用分析脚本。
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
  std::cout
      << "Dataset: synthetic PointNormal symmetric point-to-plane LLS diagnostic\n";
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
    flipSomeNormals(target);
    const auto source_xyzinormal =
        copyAsGenericNormalCloud<pcl::PointXYZINormal>(source);
    const auto target_xyzinormal =
        copyAsGenericNormalCloud<pcl::PointXYZINormal>(target);

    results.push_back(runCase(
        "symmetric lls production-direct full-cloud pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
              pcl::PointNormal,
              pcl::PointNormal>
              estimator;
          Eigen::Matrix4f matrix;
          estimator.estimateRigidTransformation(source, target, matrix);
          return diag::matrix_checksum(matrix);
        }));

    results.push_back(runCase(
        "symmetric lls production-direct full-cloud pointxyzinormal " +
            std::to_string(n),
        kIterations,
        [&]() {
          pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
              pcl::PointXYZINormal,
              pcl::PointXYZINormal>
              estimator;
          Eigen::Matrix4f matrix;
          estimator.estimateRigidTransformation(
              source_xyzinormal, target_xyzinormal, matrix);
          return diag::matrix_checksum(matrix);
        }));

    results.push_back(runCase(
        "symmetric lls full-cloud pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              diag::estimate_candidate_full(source, target, true, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Correspondences correspondences = makeCorrespondences(source.size());
    results.push_back(runCase(
        "symmetric lls correspondences pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_correspondences(
              source, target, correspondences, true, &stats);
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
