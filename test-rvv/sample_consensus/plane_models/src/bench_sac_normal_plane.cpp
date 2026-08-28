#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/sample_consensus/sac_model_normal_plane.h>
#include <pcl/common/utils.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <iomanip>
#include <string>
#include <type_traits>
#include <vector>

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

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeSourceCloud(const pcl::PointCloud<pcl::PointXYZ>& xyz,
                const pcl::PointCloud<pcl::Normal>& normals)
{
  typename pcl::PointCloud<PointT>::Ptr cloud(new pcl::PointCloud<PointT>);
  cloud->points.resize(xyz.size());
  cloud->width = xyz.width;
  cloud->height = xyz.height;
  cloud->is_dense = xyz.is_dense;

  for (std::size_t i = 0; i < xyz.size(); ++i)
  {
    auto& point = cloud->points[i];
    point.x = xyz.points[i].x;
    point.y = xyz.points[i].y;
    point.z = xyz.points[i].z;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
    {
      point.intensity = static_cast<float>((i % 17) + 1) * 0.25f;
    }
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZINormal>)
    {
      point.intensity = static_cast<float>((i % 17) + 1) * 0.25f;
      point.normal_x = normals.points[i].normal_x;
      point.normal_y = normals.points[i].normal_y;
      point.normal_z = normals.points[i].normal_z;
      point.curvature = normals.points[i].curvature;
    }
  }

  return cloud;
}

template <typename PointT>
void
runNormalPlaneBenchForSource(const std::string& source_label,
                             const pcl::PointCloud<pcl::PointXYZ>& xyz,
                             const pcl::PointCloud<pcl::Normal>::Ptr& normals,
                             const pcl::Indices& indices,
                             const Eigen::VectorXf& coeffs,
                             double threshold,
                             int iters,
                             bool suffixed_labels)
{
  auto cloud = makeSourceCloud<PointT>(xyz, *normals);
  SampleConsensusModelNormalPlaneBench<PointT, pcl::Normal> model (cloud, true);
  model.setInputNormals (normals);
  model.setIndices (indices);
  model.setNormalDistanceWeight (0.1);

  // 预分配误差与 inliers 缓冲区，避免把 buffer resize 成本混入 helper 热点计时。
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

  constexpr int w_item = 36;
  auto label_for = [&](const std::string& item) {
    if (!suffixed_labels)
      return item;
    return item + "_" + source_label + "_Normal";
  };
  auto print_line = [&](const std::string& item, const Benchmarker::Result& r) {
    std::cout << std::left << std::setw(w_item) << label_for(item) << " : "
              << std::right << std::fixed << std::setprecision(4)
              << r.avg_ms_per_iter << " ms/iter\n";
  };

  print_line("selectWithinDistance", sel);
  print_line("countWithinDistance", cnt);
  print_line("getDistancesToModel", dist);
}

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
    std::cerr << "Usage: " << argv[0] << " sac_plane_test.pcd [iters] [--representative-aos-sources]\n";
    return -1;
  }

  const char* pcd_path = argv[1];
  int iters = 50;
  bool representative_aos_sources = false;
  for (int i = 2; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "--representative-aos-sources") == 0 ||
        std::strcmp(argv[i], "--all-source-types") == 0)
    {
      representative_aos_sources = true;
      continue;
    }
    iters = std::max(1, std::atoi(argv[i]));
  }

  pcl::PCLPointCloud2 cloud_blob;
  if (pcl::io::loadPCDFile (pcd_path, cloud_blob) < 0)
  {
    std::cerr << "Failed to read test file: " << pcd_path << "\n";
    return -1;
  }

  pcl::PointCloud<pcl::PointXYZ> xyz;
  pcl::PointCloud<pcl::Normal>::Ptr normals (new pcl::PointCloud<pcl::Normal>);

  pcl::fromPCLPointCloud2 (cloud_blob, xyz);
  pcl::fromPCLPointCloud2 (cloud_blob, *normals);

  pcl::Indices indices (xyz.size ());
  for (std::size_t i = 0; i < indices.size (); ++i)
    indices[i] = static_cast<int>(i);

  Eigen::VectorXf coeffs (4);
  coeffs[0] = -0.8964f;
  coeffs[1] = -0.5868f;
  coeffs[2] = -1.208f;
  coeffs[3] = 1.0f;

  const double threshold = 0.05;

  constexpr int total_width = 85;

  print_bar('=', total_width);
  std::cout << " PCL SampleConsensus: NormalPlane Benchmark\n";
  print_bar('=', total_width);
  std::cout << "Dataset: " << pcd_path << " (" << xyz.size () << " points)\n";
  std::cout << "Iterations: " << iters << "\n";
  std::cout << "Build: " <<
#if defined(__RVV10__)
      "RVV"
#else
      "Std"
#endif
      << "\n";
  std::cout << "Source Types: "
            << (representative_aos_sources ? "representative AoS source set" : "PointXYZ + Normal")
            << "\n";
  std::cout << '\n';

  runNormalPlaneBenchForSource<pcl::PointXYZ>("PointXYZ", xyz, normals, indices, coeffs, threshold, iters, representative_aos_sources);
  if (representative_aos_sources)
  {
    runNormalPlaneBenchForSource<pcl::PointXYZI>("PointXYZI", xyz, normals, indices, coeffs, threshold, iters, true);
    runNormalPlaneBenchForSource<pcl::PointXYZINormal>("PointXYZINormal", xyz, normals, indices, coeffs, threshold, iters, true);
  }

  print_bar('=', total_width);
  return 0;
}
