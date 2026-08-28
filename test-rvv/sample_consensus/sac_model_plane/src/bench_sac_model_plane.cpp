/*
 * 本文件做什么：
 * 这个 bench（性能测试）直接调用 SampleConsensusModelPlane 的三个公开距离入口，
 * 用同一份合成点云比较 Std 构建和 RVV 构建。计时边界只包含入口调用本身，
 * 不包含点云、indices（索引）和模型系数构造。QEMU 只允许把本文件作为 build
 * 或日志形状 smoke（小型验证）使用；真实性能结论必须来自板卡或目标硬件。
 */

#include <pcl/point_types.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/common/utils.h>

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

template <typename PointT_>
class SampleConsensusModelPlaneBench
  : public pcl::SampleConsensusModelPlane<PointT_>
{
  using Base = pcl::SampleConsensusModelPlane<PointT_>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::getDistancesToModel;
  using Base::selectWithinDistance;
};

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

int
main (int argc, char** argv)
{
  const std::size_t nr_points = (argc >= 2) ? std::max<std::size_t> (1, std::strtoull (argv[1], nullptr, 10)) : 65536;
  const int iterations = (argc >= 3) ? std::max (1, std::atoi (argv[2])) : 200;
  const std::string index_mode = (argc >= 4) ? argv[3] : "shuffled";
  constexpr int warmup = 5;

  pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (nr_points);
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 4096) / 4096.0f;
    (*cloud)[i].x = std::sin (t * 17.0f);
    (*cloud)[i].y = std::cos (t * 11.0f);
    (*cloud)[i].z = 0.12f * std::sin (t * 31.0f);
  }

  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  if (index_mode == "shuffled")
  {
    for (std::size_t i = 1; i < indices.size (); i += 4)
      std::swap (indices[i - 1], indices[i]);
  }
  else if (index_mode != "identity")
  {
    std::cerr << "Unknown index mode: " << index_mode << "\n";
    return 2;
  }

  Eigen::VectorXf coeffs (4);
  coeffs << 0.0f, 0.0f, 1.0f, 0.0f;
  constexpr double threshold = 0.05;

  SampleConsensusModelPlaneBench<PointT> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices inliers;
  std::vector<double> distances;

  const Result select_result = runTimed ([&]() {
    model.selectWithinDistance (coeffs, threshold, inliers);
    return inliers.size ();
  }, iterations, warmup);

  const Result count_result = runTimed ([&]() {
    return model.countWithinDistance (coeffs, threshold);
  }, iterations, warmup);

  const Result distances_result = runTimed ([&]() {
    model.getDistancesToModel (coeffs, distances);
    return distances.size ();
  }, iterations, warmup);

  std::cout << "Dataset: synthetic sac_model_plane direct indexed cloud (points="
            << nr_points << ", " << index_mode << " indices)\n";
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
            << (select_result.checksum ^ count_result.checksum ^ distances_result.checksum)
            << "\n";
  std::cout << std::fixed << std::setprecision (6);
  std::cout << "selectWithinDistance : " << select_result.avg_ms_per_iter << " ms/iter\n";
  std::cout << "countWithinDistance : " << count_result.avg_ms_per_iter << " ms/iter\n";
  std::cout << "getDistancesToModel : " << distances_result.avg_ms_per_iter << " ms/iter\n";

  pcl::utils::ignore (inliers, distances);
  return 0;
}
