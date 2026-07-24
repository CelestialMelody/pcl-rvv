#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/sample_consensus/sac_model_normal_plane.h>
#include <pcl/common/utils.h>

#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <functional>

using PointT = pcl::PointXYZ;
using PointNT = pcl::Normal;
using ModelT = pcl::SampleConsensusModelNormalPlane<PointT, PointNT>;

// Proxy to expose protected RVV APIs, same style as test file.
template <typename PointT_, typename PointNT_>
class SampleConsensusModelNormalPlaneBench
  : public pcl::SampleConsensusModelNormalPlane<PointT_, PointNT_>
{
  using Base = pcl::SampleConsensusModelNormalPlane<PointT_, PointNT_>;

public:
  using Base::Base;

  using Base::selectWithinDistanceStandard;
  using Base::countWithinDistanceStandard;
  using Base::getDistancesToModelStandard;
  using Base::error_sqr_dists_;
#if defined (__RVV10__)
  using Base::selectWithinDistanceRVV;
  using Base::countWithinDistanceRVV;
  using Base::getDistancesToModelRVV;
#endif
};

class Benchmarker {
public:
  struct Result {
    double avg_ms_per_iter{};
    double total_ms{};
  };

  Result run(const std::function<void()>& func, int iterations = 20, int warmup = 3) {
    for (int i = 0; i < warmup; ++i) func();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) func();
    auto end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg_ms = total_ms / iterations;
    return {.avg_ms_per_iter = avg_ms, .total_ms = total_ms};
  }
};

static void
print_bar(char ch, int width = 85)
{
  for (int i = 0; i < width; ++i)
    std::cout << ch;
  std::cout << "\n";
}

int
main (int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " sac_plane_test.pcd [iters]\n";
    return -1;
  }

  const char* pcd_path = argv[1];
  int iters = 50;
  if (argc >= 3)
    iters = std::max(1, std::atoi(argv[2]));

  pcl::PCLPointCloud2 cloud_blob;
  if (pcl::io::loadPCDFile (pcd_path, cloud_blob) < 0)
  {
    std::cerr << "Failed to read test file: " << pcd_path << "\n";
    return -1;
  }

  pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  pcl::PointCloud<PointNT>::Ptr normals (new pcl::PointCloud<PointNT>);

  pcl::fromPCLPointCloud2 (cloud_blob, *cloud);
  pcl::fromPCLPointCloud2 (cloud_blob, *normals);

  pcl::Indices indices (cloud->size ());
  for (std::size_t i = 0; i < indices.size (); ++i)
    indices[i] = static_cast<int>(i);

  Eigen::VectorXf coeffs (4);
  coeffs[0] = -0.8964f;
  coeffs[1] = -0.5868f;
  coeffs[2] = -1.208f;
  coeffs[3] = 1.0f;

  SampleConsensusModelNormalPlaneBench<PointT, PointNT> model (cloud, true);
  model.setInputNormals (normals);
  model.setIndices (indices);
  model.setNormalDistanceWeight (0.1);

  const double threshold = 0.05;

  // 预分配误差与 inliers 缓冲区，避免 RVV 写越界（与测试用例保持一致）
  model.error_sqr_dists_.assign(indices.size(), 0.0);
  pcl::Indices inliers_buffer(indices.size());
  std::vector<double> distances_buffer(indices.size());

  auto run_case = [&](const std::function<void()>& func) {
    Benchmarker bench;
    return bench.run(func, iters);
  };

  auto run_select = [&]() {
#if defined(__RVV10__)
    return run_case([&](){
      model.error_sqr_dists_.assign(indices.size(), 0.0);
      inliers_buffer.assign(indices.size(), 0);
      const std::size_t nr = model.selectWithinDistanceRVV(coeffs, threshold, inliers_buffer);
      pcl::utils::ignore(nr);
    });
#else
    return run_case([&](){
      model.error_sqr_dists_.assign(indices.size(), 0.0);
      inliers_buffer.assign(indices.size(), 0);
      const std::size_t nr = model.selectWithinDistanceStandard(coeffs, threshold, inliers_buffer, 0, 0);
      pcl::utils::ignore(nr);
    });
#endif
  };

  auto run_count = [&]() {
#if defined(__RVV10__)
    return run_case([&](){
      const std::size_t nr = model.countWithinDistanceRVV(coeffs, threshold, 0);
      pcl::utils::ignore(nr);
    });
#else
    return run_case([&](){
      const std::size_t nr = model.countWithinDistanceStandard(coeffs, threshold, 0);
      pcl::utils::ignore(nr);
    });
#endif
  };

  auto run_dist = [&]() {
#if defined(__RVV10__)
    return run_case([&](){
      model.getDistancesToModelRVV(coeffs, distances_buffer);
    });
#else
    return run_case([&](){
      model.getDistancesToModelStandard(coeffs, distances_buffer, 0);
    });
#endif
  };

  const auto sel = run_select();
  const auto cnt = run_count();
  const auto dist = run_dist();

  constexpr int w_item = 24;
  constexpr int total_width = 85;

  print_bar('=', total_width);
  std::cout << " PCL SampleConsensus: NormalPlane Benchmark\n";
  print_bar('=', total_width);
  std::cout << "Dataset: " << pcd_path << " (" << cloud->size () << " points)\n";
  std::cout << "Iterations: " << iters << "\n";
  std::cout << "Build: " <<
#if defined(__RVV10__)
      "RVV"
#else
      "Std"
#endif
      << "\n";
  std::cout << '\n';

  auto print_line = [&](const std::string& item, const Benchmarker::Result& r) {
    std::cout << std::left << std::setw(w_item) << item << " : "
              << std::right << std::fixed << std::setprecision(4)
              << r.avg_ms_per_iter << " ms/iter\n";
  };

  print_line("selectWithinDistance", sel);
  print_line("countWithinDistance", cnt);
  print_line("getDistancesToModel", dist);

  print_bar('=', total_width);
  return 0;
}
