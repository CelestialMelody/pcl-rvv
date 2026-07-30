/*
 * 本文件做什么：
 * 这个 benchmark（性能测试）只度量 test-rvv diagnostic（测试专用诊断）入口，
 * 不代表 production（生产源码）已经接入。std 构建会走 scalar reference
 *（标量参考链路）；RVV 构建在规模、VLEN（向量寄存器位宽）和 offset（字节偏移）
 * gate（会导致回退的验收条件）满足时走 RVV staging（分阶段暂存）。
 *
 * 测量边界：
 * 输入 cloud、target、weights 和 correspondences 在计时前构造完成。每次迭代测量
 * candidate estimate（候选估计）本身：normal-equation（法方程）构造、Eigen solve
 *（Eigen 求解器）和 constructTransformationMatrix（构造变换矩阵）。全云 case
 *（full-cloud case，一一对应扫描）不包含权重生成；source-indexed（源索引路径）和
 * dual-indices（双索引路径）的 pcl::Indices 在计时前构造，但 candidate 内部的
 * valid-index scan（有效索引扫描）、uint32 staging（32 位索引暂存）和 byte-offset
 * prepare（字节偏移准备）属于每次 estimate，计入计时；其后的 index load、gather
 *（离散加载）和连续 weight load 也计入。对应关系 case（correspondences case，按
 * index_query/index_match 指定点对）包含 candidate 内部的 index/weight 展开，因为
 * 这是当前 gather 方案为了进入 RVV 必须付出的入口成本。
 */

#include "test_support_teptplw.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;

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
      point.z = 0.09f * x * x + 0.14f * x * y - 0.19f * y + 0.8f;
      point.normal_x = -0.18f * x - 0.14f * y;
      point.normal_y = -0.14f * x + 0.19f;
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

std::vector<float>
makeWeights(const std::size_t n)
{
  // 确定性权重序列覆盖 normal 缩放，但不引入 NaN/Inf；production 也不检查权重有限性。
  std::vector<float> weights;
  weights.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    weights.push_back(0.55f + 0.07f * static_cast<float>(i % 9));
  return weights;
}

pcl::Correspondences
makeCorrespondences(const std::size_t n)
{
  // 这个确定性子集（subset）专门给 gather bench（离散加载性能测试）使用。它不是最佳
  // 访问形态，而是让对应关系索引路径暴露 index/weight 展开和非连续读点字段的成本。
  pcl::Correspondences correspondences;
  correspondences.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.60f + 0.03f * (i % 7));
  for (std::size_t i = 3; i < n; i += 5)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.75f + 0.02f * (i % 5));
  return correspondences;
}

pcl::PointCloud<pcl::PointXYZ>
copySourceAsXYZ(const pcl::PointCloud<pcl::PointNormal>& source)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.reserve(source.size());
  cloud.height = source.height;
  cloud.is_dense = source.is_dense;
  for (const auto& point : source) {
    pcl::PointXYZ copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

pcl::PointCloud<pcl::PointXYZINormal>
copyTargetAsXYZINormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  pcl::PointCloud<pcl::PointXYZINormal> cloud;
  cloud.reserve(target.size());
  cloud.height = target.height;
  cloud.is_dense = target.is_dense;
  for (const auto& point : target) {
    pcl::PointXYZINormal copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    copied.normal_x = point.normal_x;
    copied.normal_y = point.normal_y;
    copied.normal_z = point.normal_z;
    copied.intensity = 0.5f;
    copied.curvature = 0.0f;
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

pcl::Indices
makeSourceIndices(const std::size_t n)
{
  // source-indexed bench 只覆盖有效索引；乱序和重复 row 用来暴露单侧 gather 成本。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>((i * 37 + 11) % n));
  return indices;
}

pcl::Indices
makeTargetIndices(const std::size_t n)
{
  // target 侧使用独立分布，让 dual-indices 可以和 source-indexed 分开归因。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>((i * 19 + 5) % n));
  return indices;
}

struct BenchResult {
  std::string name;
  double average_ms = 0.0;
  double total_ms = 0.0;
  double checksum = 0.0;
};

struct BenchOptions {
  std::vector<std::size_t> sizes = {65536u, 262144u};
  std::string case_filter;
};

std::vector<std::size_t>
parseSizes(const std::string& value)
{
  std::vector<std::size_t> sizes;
  std::stringstream stream(value);
  std::string token;
  while (std::getline(stream, token, ',')) {
    if (!token.empty())
      sizes.push_back(static_cast<std::size_t>(std::stoul(token)));
  }
  return sizes.empty() ? std::vector<std::size_t>{65536u, 262144u} : sizes;
}

BenchOptions
parseOptions(const int argc, char** argv)
{
  BenchOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto read_value = [&](const std::string& prefix) -> std::string {
      if (arg.rfind(prefix + "=", 0) == 0)
        return arg.substr(prefix.size() + 1);
      if (arg == prefix && i + 1 < argc)
        return argv[++i];
      return {};
    };

    std::string value = read_value("--size");
    if (!value.empty()) {
      options.sizes = parseSizes(value);
      continue;
    }
    value = read_value("--case-filter");
    if (!value.empty()) {
      options.case_filter = value;
      continue;
    }
  }
  return options;
}

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
main(int argc, char** argv)
{
  constexpr int kIterations = 20;
  const BenchOptions options = parseOptions(argc, argv);
  std::cout << std::fixed << std::setprecision(6);
  std::cout
      << "Dataset: synthetic PointNormal weighted point-to-plane LLS diagnostic\n";
  std::cout << "Iterations: " << kIterations << "\n";
  if (!options.case_filter.empty())
    std::cout << "Case filter: " << options.case_filter << "\n";
#ifdef __RVV10__
  std::cout << "Build: rvv\n";
#else
  std::cout << "Build: std\n";
#endif

  std::vector<BenchResult> results;
  for (const auto n : options.sizes) {
    pcl::PointCloud<pcl::PointNormal> source = makeCloudWithAtLeast(n);
    pcl::PointCloud<pcl::PointNormal> target;
    pcl::transformPointCloudWithNormals(source, target, makeTransform());
    const std::vector<float> weights = makeWeights(source.size());

    if (options.case_filter == "production-dispatch") {
      auto run_public_full_cloud = [&](auto& estimator, const auto& src, const auto& tgt) {
        estimator.setCorrespondenceWeights(weights);
        Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
        estimator.estimateRigidTransformation(src, tgt, matrix);
        return diag::matrix_checksum(matrix);
      };

      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointNormal,
          pcl::PointNormal>
          pointnormal_estimator;
      results.push_back(runCase(
          "weighted lls production-dispatch full-cloud pointnormal " +
              std::to_string(n),
          kIterations,
          [&]() {
            return run_public_full_cloud(pointnormal_estimator, source, target);
          }));

      const auto source_xyz = copySourceAsXYZ(source);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointNormal>
          pointxyz_pointnormal_estimator;
      results.push_back(runCase(
          "weighted lls production-dispatch full-cloud pointxyz-to-pointnormal " +
              std::to_string(n),
          kIterations,
          [&]() {
            return run_public_full_cloud(
                pointxyz_pointnormal_estimator, source_xyz, target);
          }));

      const auto target_xyzinormal = copyTargetAsXYZINormal(target);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointXYZINormal>
          pointxyz_xyzinormal_estimator;
      results.push_back(runCase(
          "weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal " +
              std::to_string(n),
          kIterations,
          [&]() {
            return run_public_full_cloud(
                pointxyz_xyzinormal_estimator, source_xyz, target_xyzinormal);
          }));
      continue;
    }

    results.push_back(runCase(
        "weighted lls full-cloud pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              diag::estimate_candidate_full(source, target, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    results.push_back(runCase(
        "weighted lls full-cloud block-reduction pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_full_block_reduction(
              source, target, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Indices source_indices = makeSourceIndices(source.size());
    results.push_back(runCase(
        "weighted lls source-indices pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_source_indices(
              source, source_indices, target, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Indices target_indices = makeTargetIndices(source.size());
    results.push_back(runCase(
        "weighted lls dual-indices pointnormal " + std::to_string(n),
        kIterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_dual_indices(
              source, source_indices, target, target_indices, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Correspondences correspondences = makeCorrespondences(source.size());
    results.push_back(runCase(
        "weighted lls correspondences pointnormal " + std::to_string(n),
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
