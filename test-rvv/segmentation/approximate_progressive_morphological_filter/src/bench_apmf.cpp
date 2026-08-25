#include "apmf.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct BenchConfig {
  std::size_t cloud_size = 262144;
  int grid_rows = 160;
  int grid_cols = 160;
  int half_size = 4;
  int iterations = 8;
  int warmup_iterations = 2;
};

BenchConfig
parseArgs(int argc, char** argv)
{
  BenchConfig config;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string key = argv[i];
    const int value = std::atoi(argv[i + 1]);
    if (key == "--size")
      config.cloud_size = static_cast<std::size_t>(value);
    else if (key == "--grid-rows")
      config.grid_rows = value;
    else if (key == "--grid-cols")
      config.grid_cols = value;
    else if (key == "--half")
      config.half_size = value;
    else if (key == "--iterations")
      config.iterations = value;
    else if (key == "--warmup")
      config.warmup_iterations = value;
  }
  return config;
}

pcl::PointCloud<pcl::PointXYZ>
makeCloud(const std::size_t n, const bool with_invalid)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 1021) - 510) * 0.125f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 37) % 1019) - 509) * 0.125f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 53) % 4093) - 2046) * 0.004f;
  }
  if (with_invalid) {
    for (std::size_t i = 31; i < n; i += 4096) {
      cloud[i].x = std::numeric_limits<float>::quiet_NaN();
      if (i + 17 < n)
        cloud[i + 17].z = std::numeric_limits<float>::infinity();
    }
  }
  return cloud;
}

Eigen::MatrixXf
makeGrid(const int rows, const int cols)
{
  Eigen::MatrixXf grid(rows, cols);
  grid.setConstant(std::numeric_limits<float>::quiet_NaN());
  for (int c = 0; c < cols; ++c) {
    for (int r = 0; r < rows; ++r) {
      if ((r * 7 + c * 11) % 19 != 0)
        grid(r, c) = static_cast<float>((r * 13 + c * 17) % 257) * 0.03125f;
    }
  }
  return grid;
}

pcl::Indices
makeGround(const std::size_t n)
{
  pcl::Indices ground;
  ground.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    ground.push_back(static_cast<int>(i));
  return ground;
}

double
checksumMatrix(const Eigen::MatrixXf& matrix)
{
  double sum = 0.0;
  for (int c = 0; c < matrix.cols(); ++c)
    for (int r = 0; r < matrix.rows(); ++r)
      if (!std::isnan(matrix(r, c)))
        sum += static_cast<double>(matrix(r, c)) * static_cast<double>(1 + r + c * matrix.rows());
  return sum;
}

std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int index : indices) {
    sum ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(index));
    sum *= 1099511628211ull;
  }
  return sum;
}

double
checksumProgressiveResult(const pcl_rvv_segmentation_apmf::ProgressiveFilterResult& result)
{
  return checksumMatrix(result.grid) + static_cast<double>(checksumIndices(result.ground)) * 1.0e-9;
}

template <typename Fn>
double
timeCase(const BenchConfig& config, const std::string& label, Fn&& fn)
{
  double last_checksum = 0.0;
  for (int i = 0; i < config.warmup_iterations; ++i)
    last_checksum += fn();

  const auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i)
    last_checksum += fn();
  const auto end = std::chrono::steady_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - begin).count();
  std::cout << label << " : " << (elapsed / static_cast<double>(config.iterations)) << " ms / iter\n";
  std::cout << "Checksum " << label << " : " << last_checksum << "\n";
  return last_checksum;
}

} // namespace

int
main(int argc, char** argv)
{
  const BenchConfig config = parseArgs(argc, argv);
  std::cout << "Dataset: synthetic APMF components, cloud=" << config.cloud_size
            << ", grid=" << config.grid_rows << "x" << config.grid_cols
            << ", half=" << config.half_size << "\n";
  std::cout << "Iterations: " << config.iterations << "\n";
  std::cout << "Warmup Iterations: " << config.warmup_iterations << "\n";

  const auto dense_cloud = makeCloud(config.cloud_size, false);
  const auto sparse_cloud = makeCloud(config.cloud_size, true);
  const auto shape = pcl_rvv_segmentation_apmf::computeGridShape(dense_cloud, 1.0f);
  const auto grid = makeGrid(config.grid_rows, config.grid_cols);
  const auto filtered = pcl_rvv_segmentation_apmf::computeGridZMinStd(dense_cloud, shape);
  const auto ground = makeGround(dense_cloud.size());
  const std::vector<int> full_half_sizes{1, config.half_size > 2 ? config.half_size / 2 : 2, config.half_size};
  const std::vector<float> full_height_thresholds{0.20f, 0.35f, 0.50f};

  volatile double sink = 0.0;

  sink += timeCase(config, "apmf grid z-min dense component", [&]() {
#if defined(__RVV10__)
    Eigen::MatrixXf out;
    if (pcl_rvv_segmentation_apmf::computeGridZMinRVV(dense_cloud, shape, out))
      return checksumMatrix(out);
#endif
    return checksumMatrix(pcl_rvv_segmentation_apmf::computeGridZMinStd(dense_cloud, shape));
  });

  sink += timeCase(config, "apmf grid z-min non-dense component", [&]() {
#if defined(__RVV10__)
    Eigen::MatrixXf out;
    if (pcl_rvv_segmentation_apmf::computeGridZMinRVV(sparse_cloud, shape, out))
      return checksumMatrix(out);
#endif
    return checksumMatrix(pcl_rvv_segmentation_apmf::computeGridZMinStd(sparse_cloud, shape));
  });

  sink += timeCase(config, "apmf window open component", [&]() {
#if defined(__RVV10__)
    Eigen::MatrixXf out;
    if (pcl_rvv_segmentation_apmf::morphologicalOpenRVV(grid, config.half_size, out))
      return checksumMatrix(out);
#endif
    return checksumMatrix(pcl_rvv_segmentation_apmf::morphologicalOpenStd(grid, config.half_size));
  });

  sink += timeCase(config, "apmf tail compress component", [&]() {
#if defined(__RVV10__)
    pcl::Indices out;
    if (pcl_rvv_segmentation_apmf::thresholdGroundRVV(
            dense_cloud, ground, shape, filtered, 0.35f, out))
      return static_cast<double>(checksumIndices(out));
#endif
    return static_cast<double>(
        checksumIndices(pcl_rvv_segmentation_apmf::thresholdGroundStd(
            dense_cloud, ground, shape, filtered, 0.35f)));
  });

  sink += timeCase(config, "apmf full pipeline diagnostic", [&]() {
#if defined(__RVV10__)
    pcl_rvv_segmentation_apmf::ProgressiveFilterResult out;
    if (pcl_rvv_segmentation_apmf::progressiveFilterRVV(
            sparse_cloud, shape, full_half_sizes, full_height_thresholds, out))
      return checksumProgressiveResult(out);
#endif
    return checksumProgressiveResult(pcl_rvv_segmentation_apmf::progressiveFilterStd(
        sparse_cloud, shape, full_half_sizes, full_height_thresholds));
  });

  return sink == 0.123 ? 1 : 0;
}
