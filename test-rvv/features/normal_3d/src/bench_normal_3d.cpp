/*
 * normal_3d.hpp RVV topic benchmark.
 *
 * 输出格式兼容 test-rvv/script/analyze_bench_compare.py：Dataset、Iterations
 * 和 "<case>: <time> ms / iter"。性能结论只能来自板卡 repeated run；
 * QEMU 运行仅可作为 build / log-shape smoke（日志形状小型验证）。
 */

#include <pcl/features/normal_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using Cloud = pcl::PointCloud<pcl::PointXYZ>;

struct Options {
  int side = 96;
  int k = 32;
  int iterations = 20;
  int warmup = 3;
  std::string case_filter = "all";
};

bool
hasArgValue(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
      value = argv[i + 1];
      return true;
    }
  }
  return false;
}

Options
parseOptions(const int argc, char** argv)
{
  Options options;
  hasArgValue(argc, argv, "--side", options.side);
  hasArgValue(argc, argv, "--k", options.k);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.side = std::max(options.side, 8);
  options.k = std::max(options.k, 3);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  return options;
}

Cloud::Ptr
makePlaneCloud(const int side)
{
  auto cloud = Cloud::Ptr(new Cloud);
  cloud->reserve(static_cast<std::size_t>(side * side));
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      const float fx = static_cast<float>(x) * 0.025f;
      const float fy = static_cast<float>(y) * 0.025f;
      const float wave = 0.01f * std::sin(0.17f * fx) * std::cos(0.11f * fy);
      const float fz = 0.35f * fx - 0.18f * fy + 1.0f + wave;
      cloud->push_back(pcl::PointXYZ(fx, fy, fz));
    }
  }
  cloud->width = static_cast<std::uint32_t>(cloud->size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

pcl::Indices
makeContiguousNeighborhood(const Cloud& cloud, const int k)
{
  pcl::Indices indices(static_cast<std::size_t>(k));
  const int offset = static_cast<int>(cloud.size() / 3);
  for (int i = 0; i < k; ++i)
    indices[static_cast<std::size_t>(i)] = static_cast<pcl::index_t>((offset + i) % static_cast<int>(cloud.size()));
  return indices;
}

template <typename Fn>
double
timeCase(const int warmup, const int iterations, Fn&& fn, double& checksum)
{
  for (int i = 0; i < warmup; ++i)
    checksum += fn();

  const auto t0 = Clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / static_cast<double>(iterations);
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  return options.case_filter == "all" || options.case_filter == name;
}

void
printCase(const std::string& name, const double ms_per_iter, const double checksum)
{
  std::cout << name << ": " << ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << checksum << "\n";
}

} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  const auto cloud = makePlaneCloud(options.side);
  const pcl::Indices local_indices = makeContiguousNeighborhood(*cloud, options.k);

  std::cout << "Dataset: synthetic normal_3d plane grid side=" << options.side
            << " points=" << cloud->size() << " k=" << options.k << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "component_compute_point_normal_indexed")) {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      for (int repeat = 0; repeat < 512; ++repeat) {
        Eigen::Vector4f plane_parameters;
        float curvature = std::numeric_limits<float>::quiet_NaN();
        pcl::computePointNormal(*cloud, local_indices, plane_parameters, curvature);
        local_checksum += plane_parameters[0] + plane_parameters[1] * 3.0 + plane_parameters[2] * 7.0 + curvature;
      }
      return local_checksum;
    }, checksum);
    printCase("component_compute_point_normal_indexed", ms, checksum);
  }

  if (caseEnabled(options, "public_normal_estimation_k")) {
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimation;
    normal_estimation.setInputCloud(cloud);
    normal_estimation.setSearchMethod(pcl::search::KdTree<pcl::PointXYZ>::Ptr(new pcl::search::KdTree<pcl::PointXYZ>));
    normal_estimation.setKSearch(options.k);
    pcl::PointCloud<pcl::Normal> normals;

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      normal_estimation.compute(normals);
      double local_checksum = 0.0;
      for (const auto& normal : normals)
        local_checksum += normal.normal_x + normal.normal_y * 3.0 + normal.normal_z * 7.0 + normal.curvature;
      return local_checksum;
    }, checksum);
    printCase("public_normal_estimation_k", ms, checksum);
  }

  return 0;
}
