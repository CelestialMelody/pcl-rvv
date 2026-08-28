/*
 * 本文件做什么：
 * 这个 bench（性能测试）直接计时 SampleConsensusModelStick 的公开 count/select/getDistances
 * 入口。Phase 080 后这些行是 production direct（真实生产路径）证据；计时边界只包含入口调用本身，不包含
 * 点云、indices（索引）和模型系数构造。QEMU 只允许作为 build / asm / log-shape
 * （构建、反汇编、日志形状）证据；真实性能结论必须来自 board（板卡）或目标硬件。
 */

#define SAC_MODEL_STICK_DONT_WARN_DEPRECATED

#include "impl/sac_model_stick_diagnostic.hpp"

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
#include <vector>

using PointT = pcl::PointXYZ;

struct Result
{
  double avg_ms_per_iter = 0.0;
  std::size_t checksum = 0;
};

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

static std::size_t
mixChecksum (std::size_t checksum, const std::size_t value)
{
  checksum ^= value + 0x9e3779b97f4a7c15ULL + (checksum << 6) + (checksum >> 2);
  return checksum;
}

static std::size_t
selectChecksum (const pcl::Indices& inliers)
{
  std::size_t checksum = inliers.size ();
  if (!inliers.empty ())
  {
    checksum = mixChecksum (checksum, static_cast<std::size_t> (inliers.front ()));
    checksum = mixChecksum (checksum, static_cast<std::size_t> (inliers[inliers.size () / 2]));
    checksum = mixChecksum (checksum, static_cast<std::size_t> (inliers.back ()));
  }
  return checksum;
}

static std::size_t
distanceChecksum (const std::vector<double>& distances)
{
  std::size_t checksum = distances.size ();
  if (!distances.empty ())
  {
    checksum = mixChecksum (checksum, static_cast<std::size_t> (distances.front () * 1000000.0));
    checksum = mixChecksum (
        checksum,
        static_cast<std::size_t> (distances[distances.size () / 2] * 1000000.0));
    checksum = mixChecksum (checksum, static_cast<std::size_t> (distances.back () * 1000000.0));
  }
  return checksum;
}

static Eigen::VectorXf
stickCoefficients ()
{
  Eigen::VectorXf coeffs (7);
  coeffs << 1.0f, -2.0f, 0.5f, 3.0f, -0.5f, 1.5f, 0.10f;
  return coeffs;
}

static pcl::PointCloud<PointT>::Ptr
makeBenchCloud (const std::size_t nr_points)
{
  pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (nr_points);

  const Eigen::Vector3f p0 (1.0f, -2.0f, 0.5f);
  const Eigen::Vector3f p1 (3.0f, -0.5f, 1.5f);
  Eigen::Vector3f dir = p1 - p0;
  dir.normalize ();
  Eigen::Vector3f n1 (0.0f, 1.0f, -1.0f);
  n1 -= n1.dot (dir) * dir;
  n1.normalize ();
  Eigen::Vector3f n2 = dir.cross (n1);
  n2.normalize ();

  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 8192) / 8192.0f;
    const float along_line = (static_cast<float> (i) - static_cast<float> (nr_points / 2)) * 0.001f;
    const float radial =
        (i % 9 < 4) ? 0.085f * std::abs (std::sin (t * 37.0f)) :
        (i % 9 < 7) ? 0.11f + 0.07f * std::abs (std::cos (t * 29.0f)) :
                      0.24f + 0.08f * std::abs (std::sin (t * 13.0f));
    const float side_mix = (i % 2 == 0) ? 0.25f : -0.15f;
    const Eigen::Vector3f pt = p0 + along_line * dir + radial * (n1 + side_mix * n2).normalized ();
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
  constexpr int warmup = 5;

  auto cloud = makeBenchCloud (nr_points);

  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);

  const Eigen::VectorXf coeffs = stickCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<PointT> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setRadiusLimits (0.0, threshold);

  const Result public_count = runTimed ([&]() {
    return model.countWithinDistance (coeffs, threshold);
  }, iterations, warmup);

  pcl::Indices public_select_inliers;
  const Result public_select = runTimed ([&]() {
    model.selectWithinDistance (coeffs, threshold, public_select_inliers);
    return selectChecksum (public_select_inliers);
  }, iterations, warmup);

  std::vector<double> public_distances;
  const Result public_get_distances = runTimed ([&]() {
    model.getDistancesToModel (coeffs, public_distances);
    return distanceChecksum (public_distances);
  }, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_stick direct indexed dual-count cloud (points="
            << nr_points << ", shuffled adjacent pairs)\n";
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
            << (public_count.checksum ^ public_select.checksum ^ public_get_distances.checksum)
            << "\n";
  std::cout << std::fixed << std::setprecision (6);
  std::cout << "public countWithinDistance : " << public_count.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public selectWithinDistance : " << public_select.avg_ms_per_iter << " ms/iter\n";
  std::cout << "public getDistancesToModel : "
            << public_get_distances.avg_ms_per_iter << " ms/iter\n";

  pcl::utils::ignore (model);
  return 0;
}
