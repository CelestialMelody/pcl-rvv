/*
 * 本文件做什么：
 * 这个 bench（性能测试）直接计时 SampleConsensusModelLine 的公开
 * count/select/getDistances 入口和测试专用 RVV candidate（候选实现）。
 * Phase 040 后，公开入口行就是 production direct（真实生产入口直连）证据来源；
 * diagnostic candidate 行继续保留为历史候选对照。
 * 计时边界只包含入口调用本身，不包含点云、indices（索引）和模型系数构造。
 * QEMU 只允许作为 build / asm / log-shape（构建、反汇编、日志形状）证据；
 * 真实性能结论必须来自 board（板卡）或目标硬件。
 */

#include "impl/sac_model_line_diagnostic.hpp"

#include <pcl/common/utils.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using PointT = pcl::PointXYZ;

struct Result
{
  double avg_ms_per_iter = 0.0;
  std::size_t checksum = 0;
};

static std::size_t
hashDoubleVector (const std::vector<double>& values)
{
  // getDistancesToModel 的 RVV diagnostic 允许 `sqrt` 前的 float 公式产生
  // 1e-6 量级舍入差异；bench checksum（校验和）只确认 dense output
  // （连续输出）规模，逐项数值正确性由 gtest tolerance（误差容忍度）负责。
  return values.size ();
}

static std::size_t
hashIndicesAndErrors (const pcl::Indices& indices, const std::vector<double>& errors)
{
  std::size_t hash = indices.size ();
  for (const pcl::index_t index : indices)
  {
    hash ^= static_cast<std::size_t> (index) + 0x9e3779b97f4a7c15ULL +
            (hash << 6) + (hash >> 2);
  }
  return hash ^ (errors.size () << 1);
}

static Result
runTimed (const std::function<std::size_t()>& func, const int iterations, const int warmup)
{
  std::size_t checksum = 0;
  for (int i = 0; i < warmup; ++i)
    checksum ^= func () + static_cast<std::size_t> (i);

  const auto start = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < iterations; ++i)
    checksum ^= func () + static_cast<std::size_t> (i + 17);
  const auto end = std::chrono::high_resolution_clock::now ();

  return {std::chrono::duration<double, std::milli> (end - start).count () / iterations,
          checksum};
}

static Eigen::VectorXf
lineCoefficients ()
{
  Eigen::VectorXf coeffs (6);
  coeffs << 1.0f, -2.0f, 0.5f, 2.0f, 1.0f, -0.5f;
  return coeffs;
}

static pcl::PointCloud<PointT>::Ptr
makeBenchCloud (const std::size_t nr_points)
{
  pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (nr_points);

  const Eigen::Vector3f p0 (1.0f, -2.0f, 0.5f);
  Eigen::Vector3f dir (2.0f, 1.0f, -0.5f);
  dir.normalize ();
  Eigen::Vector3f n1 (1.0f, -2.0f, 0.0f);
  n1 -= n1.dot (dir) * dir;
  n1.normalize ();
  Eigen::Vector3f n2 = dir.cross (n1);
  n2.normalize ();

  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 8192) / 8192.0f;
    const float along_line = (static_cast<float> (i) - static_cast<float> (nr_points / 2)) * 0.001f;
    const float radial_a = 0.12f * std::sin (t * 37.0f);
    const float radial_b = 0.09f * std::cos (t * 29.0f);
    const Eigen::Vector3f pt = p0 + along_line * dir + radial_a * n1 + radial_b * n2;
    (*cloud)[i].x = pt.x ();
    (*cloud)[i].y = pt.y ();
    (*cloud)[i].z = pt.z ();
  }
  return cloud;
}

int
main (int argc, char** argv)
{
  const std::size_t nr_points =
      (argc >= 2) ? std::max<std::size_t> (1, std::strtoull (argv[1], nullptr, 10)) : 65536;
  const int iterations = (argc >= 3) ? std::max (1, std::atoi (argv[2])) : 200;
  const std::string index_mode = (argc >= 4) ? argv[3] : "shuffled";
  if (index_mode != "shuffled" && index_mode != "identity")
  {
    std::cerr << "Usage: " << argv[0] << " [points] [iterations] [identity|shuffled]\n";
    return 2;
  }
  constexpr int warmup = 5;

  auto cloud = makeBenchCloud (nr_points);

  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  if (index_mode == "shuffled")
  {
    for (std::size_t i = 1; i < indices.size (); i += 4)
      std::swap (indices[i - 1], indices[i]);
  }

  const Eigen::VectorXf coeffs = lineCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<PointT> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  pcl::Indices inliers;
  std::vector<double> distances;

  const Result public_count = runTimed ([&]() {
    return model.countWithinDistance (coeffs, threshold);
  }, iterations, warmup);

  const Result candidate_count = runTimed ([&]() {
    return model.countWithinDistanceCandidate (coeffs, threshold);
  }, iterations, warmup);

  const Result public_select = runTimed ([&]() {
    model.selectWithinDistance (coeffs, threshold, inliers);
    return hashIndicesAndErrors (inliers, model.error_sqr_dists_);
  }, iterations, warmup);

  const Result candidate_select = runTimed ([&]() {
    model.selectWithinDistanceCandidate (coeffs, threshold, inliers);
    return hashIndicesAndErrors (inliers, model.error_sqr_dists_);
  }, iterations, warmup);

  const Result public_get_distances = runTimed ([&]() {
    model.getDistancesToModel (coeffs, distances);
    return hashDoubleVector (distances);
  }, iterations, warmup);

  const Result candidate_get_distances = runTimed ([&]() {
    model.getDistancesToModelCandidate (coeffs, distances);
    return hashDoubleVector (distances);
  }, iterations, warmup);

  const Result candidate_get_distances_vfsqrt = runTimed ([&]() {
    model.getDistancesToModelVFSqrtCandidate (coeffs, distances);
    return hashDoubleVector (distances);
  }, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_line direct indexed line-distance cloud (points="
            << nr_points << ", "
            << (index_mode == "identity" ? "identity indices" : "shuffled adjacent pairs")
            << ")\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup << "\n";
  std::cout << "Build: "
#if defined (__RVV10__)
            << "RVV"
#else
            << "Std"
#endif
            << "\n";
  std::cout << "Checksum: "
            << (public_count.checksum ^ candidate_count.checksum ^
                public_select.checksum ^ candidate_select.checksum ^
                public_get_distances.checksum ^ candidate_get_distances.checksum ^
                candidate_get_distances_vfsqrt.checksum)
            << "\n";
  std::cout << std::fixed << std::setprecision (6);
  std::cout << "public countWithinDistance : " << public_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate countWithinDistance : "
            << candidate_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public selectWithinDistance : " << public_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate selectWithinDistance : "
            << candidate_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public getDistancesToModel : "
            << public_get_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate getDistancesToModel : "
            << candidate_get_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "diagnostic candidate getDistancesToModel vfsqrt : "
            << candidate_get_distances_vfsqrt.avg_ms_per_iter << " ms/iter\n";
  std::cout << "Checksum public countWithinDistance : " << public_count.checksum << "\n";
  std::cout << "Checksum diagnostic candidate countWithinDistance : "
            << candidate_count.checksum << "\n";
  std::cout << "Checksum public selectWithinDistance : " << public_select.checksum << "\n";
  std::cout << "Checksum diagnostic candidate selectWithinDistance : "
            << candidate_select.checksum << "\n";
  std::cout << "Checksum public getDistancesToModel : "
            << public_get_distances.checksum << "\n";
  std::cout << "Checksum diagnostic candidate getDistancesToModel : "
            << candidate_get_distances.checksum << "\n";
  std::cout << "Checksum diagnostic candidate getDistancesToModel vfsqrt : "
            << candidate_get_distances_vfsqrt.checksum << "\n";

  pcl::utils::ignore (model, inliers, distances);
  return 0;
}
