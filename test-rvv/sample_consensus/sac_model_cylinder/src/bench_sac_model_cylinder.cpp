/*
 * 本文件做什么：
 * 这个 bench（性能测试）计时 SampleConsensusModelCylinder 的公开
 * count/select 入口和测试专用 candidate（候选实现）。生产接入后，public
 * 行代表真实公开入口的 Std/RVV 对比；diagnostic candidate 行保留为
 * 历史 cross-check（交叉检查），不替代 production direct（真实生产路径证据）。
 *
 * 计时边界只包含入口调用本身，不包含点云、法线、indices（索引）和模型
 * 系数构造。QEMU（仿真器）只用于 build / asm / log-shape（构建、
 * 反汇编、日志形状）证据；真实性能结论必须来自 board（板卡）或目标硬件。
 */

#include "sac_model_cylinder.h"

#include <pcl/point_types.h>

#include <algorithm>
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

struct Result
{
  double avg_ms_per_iter = 0.0;
  std::size_t checksum = 0;
};

struct CaseResults
{
  Result count;
  Result select;
  Result get_distances;
};

static Eigen::VectorXf
cylinderCoefficients ()
{
  Eigen::VectorXf coeffs (7);
  coeffs << 0.20f, -0.30f, 0.10f, 0.0f, 0.0f, 1.0f, 1.0f;
  return coeffs;
}

static std::size_t
hashIndicesAndErrors (const pcl::Indices& indices, const std::vector<double>& errors)
{
  // bench checksum（校验和）只确认 select 的输出规模和索引顺序。误差值的
  // 逐项数值一致性由 gtest（单元测试框架）的 tolerance（误差容忍度）负责，
  // 避免把 RVV float 近似造成的 1e-5 量级差异误判为性能证据损坏。
  std::size_t hash = indices.size () ^ (errors.size () << 1);
  for (const pcl::index_t index : indices)
  {
    hash ^= static_cast<std::size_t> (index) + 0x9e3779b97f4a7c15ULL +
            (hash << 6) + (hash >> 2);
  }
  return hash;
}

static std::size_t
hashDistances (const std::vector<double>& distances)
{
  // getDistances 的输出是 dense vector（连续距离数组）。这里的 bench
  // checksum（校验和）只保护输出规模；逐项数值一致性由 gtest 的
  // bench-shaped correctness（按性能样本形态构造的正确性测试）负责。
  return distances.size ();
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

template <typename PointT>
static void
setXYZFields (PointT& point, const Eigen::Vector3f& value)
{
  point.x = value.x ();
  point.y = value.y ();
  point.z = value.z ();
}

template <typename PointNT>
static void
setNormalFields (PointNT& normal_point, const Eigen::Vector3f& value)
{
  normal_point.normal_x = value.x ();
  normal_point.normal_y = value.y ();
  normal_point.normal_z = value.z ();
}

template <typename PointT, typename PointNT>
static void
appendCylinderPoint (pcl::PointCloud<PointT>& cloud,
                     pcl::PointCloud<PointNT>& normals,
                     const float angle,
                     const float z,
                     const float radial_delta,
                     const float normal_angle)
{
  const Eigen::Vector3f axis_point (0.20f, -0.30f, 0.10f);
  const float radius = 1.0f + radial_delta;
  const Eigen::Vector3f radial (std::cos (angle), std::sin (angle), 0.0f);
  const Eigen::Vector3f point =
      axis_point + Eigen::Vector3f (0.0f, 0.0f, z) + radius * radial;

  PointT pt;
  setXYZFields (pt, point);
  cloud.push_back (pt);

  const float c = std::cos (normal_angle);
  const float s = std::sin (normal_angle);
  Eigen::Vector3f normal (c * radial.x () - s * radial.y (),
                          s * radial.x () + c * radial.y (),
                          0.0f);
  normal.normalize ();

  PointNT nt;
  setNormalFields (nt, normal);
  nt.curvature = 0.0f;
  normals.push_back (nt);
}

template <typename PointT, typename PointNT>
static void
makeBenchInputs (const std::size_t nr_points,
                 typename pcl::PointCloud<PointT>::Ptr& cloud,
                 typename pcl::PointCloud<PointNT>::Ptr& normals)
{
  cloud = pcl::make_shared<pcl::PointCloud<PointT>> ();
  normals = pcl::make_shared<pcl::PointCloud<PointNT>> ();
  cloud->reserve (nr_points);
  normals->reserve (nr_points);

  for (std::size_t i = 0; i < nr_points; ++i)
  {
    const float phase = static_cast<float> (i % 8192) / 8192.0f;
    const float angle = phase * 6.28318530718f + static_cast<float> (i % 7) * 0.013f;
    const float z = (static_cast<float> (i) - static_cast<float> (nr_points / 2)) * 0.0007f;

    // 让样本落在明显通过或明显失败的区域，减少 near-threshold（近阈值）
    // 点导致的 branch / 数值噪声。当前 correctness 测试另行覆盖阈值语义。
    const int bucket = static_cast<int> (i % 8);
    const float radial_delta =
        (bucket <= 2) ? (0.010f * static_cast<float> (bucket)) :
        (bucket <= 4) ? (0.075f + 0.010f * static_cast<float> (bucket - 3)) :
                        (0.210f + 0.015f * static_cast<float> (bucket - 5));
    const float normal_angle =
        (bucket <= 2) ? (0.010f * static_cast<float> (bucket)) :
        (bucket <= 4) ? (0.040f + 0.010f * static_cast<float> (bucket - 3)) :
                        (0.180f + 0.020f * static_cast<float> (bucket - 5));
    appendCylinderPoint (*cloud, *normals, angle, z, radial_delta, normal_angle);
  }
}

static pcl::Indices
makeIndices (const std::size_t nr_points, const std::string& index_mode)
{
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  if (index_mode == "shuffled")
  {
    for (std::size_t i = 1; i < indices.size (); i += 4)
      std::swap (indices[i - 1], indices[i]);
  }
  return indices;
}

template <typename PointT, typename PointNT>
static CaseResults
runPublicCase (const std::size_t nr_points,
               const std::string& index_mode,
               const int iterations,
               const int warmup)
{
  using ModelT = pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<PointT, PointNT>;

  typename pcl::PointCloud<PointT>::Ptr cloud;
  typename pcl::PointCloud<PointNT>::Ptr normals;
  makeBenchInputs<PointT, PointNT> (nr_points, cloud, normals);
  const pcl::Indices indices = makeIndices (nr_points, index_mode);
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.12;

  ModelT model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.25);

  pcl::Indices inliers;
  std::vector<double> distances;
  CaseResults results;
  results.count = runTimed ([&]() {
    return model.countWithinDistance (coeffs, threshold);
  }, iterations, warmup);
  results.select = runTimed ([&]() {
    model.selectWithinDistance (coeffs, threshold, inliers);
    return hashIndicesAndErrors (inliers, model.error_sqr_dists_);
  }, iterations, warmup);
  results.get_distances = runTimed ([&]() {
    model.getDistancesToModel (coeffs, distances);
    return hashDistances (distances);
  }, iterations, warmup);
  return results;
}

static void
printPublicCase (const std::string& prefix, const CaseResults& results)
{
  std::cout << prefix << " countWithinDistance : "
            << results.count.avg_ms_per_iter << " ms/iter\n";
  std::cout << prefix << " selectWithinDistance : "
            << results.select.avg_ms_per_iter << " ms/iter\n";
  std::cout << prefix << " getDistancesToModel : "
            << results.get_distances.avg_ms_per_iter << " ms/iter\n";
  std::cout << "Checksum " << prefix << " countWithinDistance : "
            << results.count.checksum << "\n";
  std::cout << "Checksum " << prefix << " selectWithinDistance : "
            << results.select.checksum << "\n";
  std::cout << "Checksum " << prefix << " getDistancesToModel : "
            << results.get_distances.checksum << "\n";
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

  using PointT = pcl::PointXYZ;
  using NormalT = pcl::Normal;
  using ModelT = pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<PointT, NormalT>;

  pcl::PointCloud<PointT>::Ptr cloud;
  pcl::PointCloud<NormalT>::Ptr normals;
  makeBenchInputs<PointT, NormalT> (nr_points, cloud, normals);
  const pcl::Indices indices = makeIndices (nr_points, index_mode);
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.12;

  ModelT model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.25);

  pcl::Indices inliers;
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

  std::vector<double> distances;
  const Result public_get_distances = runTimed ([&]() {
    model.getDistancesToModel (coeffs, distances);
    return hashDistances (distances);
  }, iterations, warmup);

  const CaseResults xyzi_normal =
      runPublicCase<pcl::PointXYZI, pcl::Normal> (nr_points, index_mode, iterations, warmup);
  const CaseResults xyzrgb_normal =
      runPublicCase<pcl::PointXYZRGB, pcl::Normal> (nr_points, index_mode, iterations, warmup);
  const CaseResults xyz_pointnormal =
      runPublicCase<pcl::PointXYZ, pcl::PointNormal> (nr_points, index_mode, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_cylinder direct indexed cylinder-normal cloud (points="
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
                public_get_distances.checksum)
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
  printPublicCase ("public PointXYZI+Normal", xyzi_normal);
  printPublicCase ("public PointXYZRGB+Normal", xyzrgb_normal);
  printPublicCase ("public PointXYZ+PointNormal", xyz_pointnormal);
  std::cout << "Checksum public countWithinDistance : " << public_count.checksum << "\n";
  std::cout << "Checksum diagnostic candidate countWithinDistance : "
            << candidate_count.checksum << "\n";
  std::cout << "Checksum public selectWithinDistance : " << public_select.checksum << "\n";
  std::cout << "Checksum diagnostic candidate selectWithinDistance : "
            << candidate_select.checksum << "\n";
  std::cout << "Checksum public getDistancesToModel : "
            << public_get_distances.checksum << "\n";

  return 0;
}
