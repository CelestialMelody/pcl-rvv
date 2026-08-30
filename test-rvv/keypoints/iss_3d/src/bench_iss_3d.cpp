/*
 * 本文件是 ISS 3D scatter matrix（散布矩阵）component ablation（组件消融）
 * benchmark 入口。它只计时测试专用 helper，不包含 searchForNeighbors（邻域搜索）、
 * Eigen EVD（特征值分解）、NMS（非极大值抑制）或 output push（输出写入）。
 */

#include "iss_3d.h"

#include <pcl/keypoints/iss_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <Eigen/Core>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace iss = pcl::keypoints::rvv_test::iss_3d;

namespace
{
using Clock = std::chrono::steady_clock;

struct Options
{
  int iterations = 20;
  int warmup = 3;
  int points = 8192;
  int neighbors = 256;
  std::string mode = "diagnostic";
  std::string case_filter = "all";
};

bool
readIntArg(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
readStringArg(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
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
  readIntArg(argc, argv, "--iterations", options.iterations);
  readIntArg(argc, argv, "--warmup", options.warmup);
  readIntArg(argc, argv, "--points", options.points);
  readIntArg(argc, argv, "--neighbors", options.neighbors);
  readStringArg(argc, argv, "--mode", options.mode);
  readStringArg(argc, argv, "--case-filter", options.case_filter);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  options.points = std::max(options.points, 32);
  options.neighbors = std::max(options.neighbors, 1);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  if (options.case_filter == "all")
    return true;
  std::size_t start = 0;
  while (start <= options.case_filter.size())
  {
    const std::size_t comma = options.case_filter.find(',', start);
    if (options.case_filter.substr(start, comma - start) == name)
      return true;
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return false;
}

std::vector<pcl::PointXYZ>
makeCloud(const std::size_t count)
{
  std::vector<pcl::PointXYZ> points(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    points[i].x = static_cast<float>((static_cast<int>(i % 41) - 20) * 0.019f);
    points[i].y = static_cast<float>((static_cast<int>((i * 13) % 47) - 23) * 0.017f);
    points[i].z = static_cast<float>(1.0f + static_cast<int>((i * 29) % 53) * 0.011f);
  }
  return points;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePublicCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(count);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(count);

  const int side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(count))));
  for (std::size_t i = 0; i < count; ++i)
  {
    const int x = static_cast<int>(i % side);
    const int y = static_cast<int>((i / side) % side);
    const int z = static_cast<int>(i / (side * side));
    pcl::PointXYZ& point = cloud->points[i];
    point.x = static_cast<float>(x) * 0.01f;
    point.y = static_cast<float>(y) * 0.01f;
    point.z = static_cast<float>(z) * 0.01f +
              static_cast<float>((x * 11 + y * 7 + z * 5) % 13) * 0.0005f;
  }
  return cloud;
}

std::vector<int>
makeNeighborList(const std::size_t count, const std::size_t point_count, const bool contiguous)
{
  std::vector<int> indices(count);
  if (contiguous)
  {
    for (std::size_t i = 0; i < count; ++i)
      indices[i] = static_cast<int>((i + 17) % point_count);
    return indices;
  }
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<int>((i * 97 + 31) % point_count);
  return indices;
}

std::uint64_t
checksumMatrix(const Eigen::Matrix3d& matrix)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (int row = 0; row < 3; ++row)
  {
    for (int col = 0; col < 3; ++col)
    {
      const auto quantized = static_cast<std::int64_t>(std::llround(matrix(row, col) * 100000.0));
      std::uint64_t bits = 0;
      std::memcpy(&bits, &quantized, sizeof(bits));
      hash ^= bits;
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

std::uint64_t
checksumOutput(const pcl::PointCloud<pcl::PointXYZ>& output, const pcl::PointIndices& indices)
{
  std::uint64_t hash = 1469598103934665603ull;
  hash ^= static_cast<std::uint64_t>(output.size());
  hash *= 1099511628211ull;
  for (std::size_t i = 0; i < output.size(); ++i)
  {
    const auto qx = static_cast<std::int64_t>(std::llround(static_cast<double>(output[i].x) * 100000.0));
    const auto qy = static_cast<std::int64_t>(std::llround(static_cast<double>(output[i].y) * 100000.0));
    const auto qz = static_cast<std::int64_t>(std::llround(static_cast<double>(output[i].z) * 100000.0));
    for (const auto value : {qx, qy, qz})
    {
      std::uint64_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      hash ^= bits;
      hash *= 1099511628211ull;
    }
    const auto index = static_cast<std::uint64_t>(indices.indices[i]);
    hash ^= index + 0x9e3779b97f4a7c15ull + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ull;
  }
  return hash;
}

template <typename Fn>
double
timeCase(const Options& options, Fn&& fn, std::uint64_t& checksum)
{
  for (int i = 0; i < options.warmup; ++i)
    checksum ^= fn();
  const auto begin = Clock::now();
  for (int i = 0; i < options.iterations; ++i)
    checksum ^= fn();
  const auto end = Clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count() /
         static_cast<double>(options.iterations);
}

void
runScatterCase(const Options& options, const std::string& label, const bool contiguous, const int neighbor_count)
{
  if (!caseEnabled(options, label))
    return;

  const auto points = makeCloud(static_cast<std::size_t>(options.points));
  const auto neighbors = makeNeighborList(static_cast<std::size_t>(neighbor_count), points.size(), contiguous);
  Eigen::Matrix3d scatter = Eigen::Matrix3d::Zero();
  std::uint64_t checksum = 0;
  int current_index = 3;

  const double ms = timeCase(options, [&] {
    current_index = (current_index + 37) % options.points;
    iss::computeScatterMatrixCandidate(points.data(),
                                       current_index,
                                       neighbors.data(),
                                       neighbors.size(),
                                       scatter);
    return checksumMatrix(scatter);
  }, checksum);

  std::cout << "Dataset: synthetic ISS 3D scatter component points=" << options.points
            << " neighbors=" << neighbor_count
            << " contiguous=" << (contiguous ? "true" : "false") << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";
  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms / iter\n";
  std::cout << label << " checksum: " << checksum << "\n";
}

void
runPublicCase(const Options& options, const std::string& label)
{
  if (!caseEnabled(options, label))
    return;

  auto cloud = makePublicCloud(static_cast<std::size_t>(options.points));
  auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>();
  tree->setInputCloud(cloud);
  std::uint64_t checksum = 0;

  const double ms = timeCase(options, [&] {
    pcl::ISSKeypoint3D<pcl::PointXYZ, pcl::PointXYZ> detector;
    detector.setSearchMethod(tree);
    detector.setInputCloud(cloud);
    detector.setSalientRadius(0.035);
    detector.setNonMaxRadius(0.025);
    detector.setThreshold21(0.975);
    detector.setThreshold32(0.975);
    detector.setMinNeighbors(5);
    detector.setNumberOfThreads(1);
    pcl::PointCloud<pcl::PointXYZ> output;
    detector.compute(output);
    return checksumOutput(output, *detector.getKeypointsIndices());
  }, checksum);

  std::cout << "Dataset: synthetic ISS 3D public compute points=" << options.points
            << " salient_radius=0.035 non_max_radius=0.025\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";
  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms / iter\n";
  std::cout << label << " checksum: " << checksum << "\n";
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  if (options.mode == "public")
  {
    runPublicCase(options, "public_iss_3d_grid_4096");
    return 0;
  }

  runScatterCase(options, "scatter_indexed_256", false, options.neighbors);
  runScatterCase(options, "scatter_indexed_tail_73", false, 73);
  runScatterCase(options, "scatter_contiguous_256", true, options.neighbors);
  return 0;
}
