#include "omps.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
  std::size_t size = 262144;
  int iterations = 8;
  int warmup = 2;
  std::string case_filter = "all";
};

Options
parseArgs(int argc, char** argv)
{
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&]() -> const char* {
      return (i + 1 < argc) ? argv[++i] : "";
    };
    if (arg == "--size")
      options.size = static_cast<std::size_t>(std::strtoull(next(), nullptr, 10));
    else if (arg == "--iterations")
      options.iterations = std::atoi(next());
    else if (arg == "--warmup")
      options.warmup = std::atoi(next());
    else if (arg == "--case-filter")
      options.case_filter = next();
  }
  return options;
}

pcl::PointCloud<pcl::PointXYZ>
makeCloud(const std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>((i * 17) % 4099) - 2048) * 0.003f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 29) % 4093) - 2046) * 0.004f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 43) % 4091) - 2045) * 0.002f + 6.0f;
  }
  return cloud;
}

pcl::PointCloud<pcl::Normal>
makeNormals(const std::size_t n)
{
  pcl::PointCloud<pcl::Normal> normals;
  normals.width = static_cast<std::uint32_t>(n);
  normals.height = 1;
  normals.is_dense = true;
  normals.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    normals[i].normal_x = 0.15f + static_cast<float>(i % 7) * 0.01f;
    normals[i].normal_y = -0.20f + static_cast<float>(i % 5) * 0.02f;
    normals[i].normal_z = 0.95f;
  }
  return normals;
}

std::vector<int>
makeBoundaryIndices(const std::size_t n)
{
  std::vector<int> indices;
  indices.reserve(n / 2);
  if (n == 0)
    return indices;
  const std::size_t step = (n % 2 == 0) ? (n - 1) : (n - 2);
  const std::size_t count = n / 2;
  for (std::size_t i = 0; i < count; ++i)
    indices.push_back(static_cast<int>((i * step + 3) % n));
  return indices;
}

std::vector<pcl_rvv_segmentation_omps::RegionBoundaryInput>
makeRegionBoundaries(const std::size_t n)
{
  std::vector<pcl_rvv_segmentation_omps::RegionBoundaryInput> regions;
  constexpr std::size_t region_count = 16;
  const std::size_t boundary_per_region = n / 64;
  regions.reserve(region_count);
  for (std::size_t region = 0; region < region_count; ++region) {
    pcl_rvv_segmentation_omps::RegionBoundaryInput input;
    input.model = Eigen::Vector4f(0.0f, 0.0f, 1.0f, -2.0f - static_cast<float>(region % 5));
    input.centroid = Eigen::Vector3f(0.0f, 0.0f, -input.model[3]);
    input.inlier_count = static_cast<unsigned int>(boundary_per_region * 3 + region);
    input.boundary_indices.reserve(boundary_per_region);
    for (std::size_t i = 0; i < boundary_per_region; ++i) {
      const std::size_t index = (region * 4099 + i * 37 + 11) % n;
      input.boundary_indices.push_back(static_cast<int>(index));
    }
    regions.push_back(input);
  }
  return regions;
}

template <typename Func>
double
timeCase(const int warmup, const int iterations, Func&& func)
{
  for (int i = 0; i < warmup; ++i)
    func();
  const auto start = Clock::now();
  for (int i = 0; i < iterations; ++i)
    func();
  const auto end = Clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count() /
         static_cast<double>(iterations);
}

bool
caseEnabled(const std::string& filter, const std::string& name)
{
  return filter == "all" || filter == name;
}

} // namespace

int
main(int argc, char** argv)
{
  const auto options = parseArgs(argc, argv);
  const auto cloud = makeCloud(options.size);
  const auto normals = makeNormals(options.size);
  const auto boundary_indices = makeBoundaryIndices(options.size);
  const auto region_boundaries = makeRegionBoundaries(options.size);
  const Eigen::Vector4f normal(0.0f, 0.0f, 1.0f, -2.0f);
  const Eigen::Vector3f centroid(0.0f, 0.0f, 2.0f);
  const Eigen::Vector3f viewpoint(0.0f, 0.0f, 0.0f);
  double checksum = 0.0;

  std::cout << "Dataset: organized_multi_plane_segmentation component ablation; size="
            << options.size << "; boundary=" << boundary_indices.size()
            << "; case_filter=" << options.case_filter << '\n';
  std::cout << "Iterations: " << options.iterations << '\n';
  std::cout << "Warmup Iterations: " << options.warmup << '\n';

  if (caseEnabled(options.case_filter, "plane_d_dot")) {
    std::vector<float> plane_d;
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary =
          pcl_rvv_segmentation_omps::computePlaneDValuesRVV(cloud, normals, &plane_d);
#else
      const auto summary =
          pcl_rvv_segmentation_omps::computePlaneDValuesStd(cloud, normals, &plane_d);
#endif
      checksum += summary.checksum;
    });
    std::cout << "plane_d_dot: " << avg_ms << " ms / iter\n";
  }

  if (caseEnabled(options.case_filter, "boundary_gather")) {
    pcl::PointCloud<pcl::PointXYZ> boundary_cloud;
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_omps::gatherBoundaryCloudRVV(
          cloud, boundary_indices, &boundary_cloud);
#else
      const auto summary = pcl_rvv_segmentation_omps::gatherBoundaryCloudStd(
          cloud, boundary_indices, &boundary_cloud);
#endif
      checksum += summary.checksum;
    });
    std::cout << "boundary_gather: " << avg_ms << " ms / iter\n";
  }

  if (caseEnabled(options.case_filter, "projection")) {
    pcl::PointCloud<pcl::PointXYZ> projected_cloud;
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_omps::projectBoundaryFromViewpointRVV(
          cloud, normal, centroid, viewpoint, &projected_cloud);
#else
      const auto summary = pcl_rvv_segmentation_omps::projectBoundaryFromViewpointStd(
          cloud, normal, centroid, viewpoint, &projected_cloud);
#endif
      checksum += summary.checksum;
    });
    std::cout << "projection: " << avg_ms << " ms / iter\n";
  }

  if (caseEnabled(options.case_filter, "region_gather_only")) {
    std::vector<pcl::PointCloud<pcl::PointXYZ>> output_boundaries;
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_omps::assembleRegionBoundariesRVV(
          cloud, region_boundaries, false, &output_boundaries);
#else
      const auto summary = pcl_rvv_segmentation_omps::assembleRegionBoundariesStd(
          cloud, region_boundaries, false, &output_boundaries);
#endif
      checksum += summary.checksum;
    });
    std::cout << "region_gather_only: " << avg_ms << " ms / iter\n";
  }

  if (caseEnabled(options.case_filter, "region_projected")) {
    std::vector<pcl::PointCloud<pcl::PointXYZ>> output_boundaries;
    const double avg_ms = timeCase(options.warmup, options.iterations, [&]() {
#if defined(__RVV10__)
      const auto summary = pcl_rvv_segmentation_omps::assembleRegionBoundariesRVV(
          cloud, region_boundaries, true, &output_boundaries);
#else
      const auto summary = pcl_rvv_segmentation_omps::assembleRegionBoundariesStd(
          cloud, region_boundaries, true, &output_boundaries);
#endif
      checksum += summary.checksum;
    });
    std::cout << "region_projected: " << avg_ms << " ms / iter\n";
  }

  std::cout << "Checksum: " << static_cast<std::uint64_t>(checksum * 1000.0) << '\n';
  return 0;
}
