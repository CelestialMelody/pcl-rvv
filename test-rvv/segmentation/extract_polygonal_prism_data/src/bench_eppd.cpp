#include "eppd.h"

#include <Eigen/Core>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/segmentation/extract_polygonal_prism_data.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class IndexMode {
  Dense,
  Indexed
};

enum class PathMode {
  Diagnostic,
  Production
};

enum class PolygonMode {
  Single,
  Nested
};

enum class PointTypeMode {
  PointXYZ,
  PointXYZI,
  PointXYZRGB,
  PointXYZRGBA,
  PointXYZINormal
};

struct BenchConfig {
  std::size_t cloud_size = 262144;
  int iterations = 8;
  int warmup_iterations = 2;
  float half_extent = 96.0f;
  IndexMode index_mode = IndexMode::Dense;
  PathMode path_mode = PathMode::Diagnostic;
  PolygonMode polygon_mode = PolygonMode::Single;
  PointTypeMode point_type_mode = PointTypeMode::PointXYZ;
};

IndexMode
parseIndexMode(const std::string& mode)
{
  if (mode == "dense") {
    return IndexMode::Dense;
  }
  if (mode == "indexed") {
    return IndexMode::Indexed;
  }
  throw std::invalid_argument("--indices must be dense or indexed");
}

PathMode
parsePathMode(const std::string& mode)
{
  if (mode == "diagnostic") {
    return PathMode::Diagnostic;
  }
  if (mode == "production") {
    return PathMode::Production;
  }
  throw std::invalid_argument("--path must be diagnostic or production");
}

PolygonMode
parsePolygonMode(const std::string& mode)
{
  if (mode == "single") {
    return PolygonMode::Single;
  }
  if (mode == "nested") {
    return PolygonMode::Nested;
  }
  throw std::invalid_argument("--polygons must be single or nested");
}

PointTypeMode
parsePointTypeMode(const std::string& mode)
{
  if (mode == "xyz") {
    return PointTypeMode::PointXYZ;
  }
  if (mode == "xyzi") {
    return PointTypeMode::PointXYZI;
  }
  if (mode == "xyzrgb") {
    return PointTypeMode::PointXYZRGB;
  }
  if (mode == "xyzrgba") {
    return PointTypeMode::PointXYZRGBA;
  }
  if (mode == "xyzinormal") {
    return PointTypeMode::PointXYZINormal;
  }
  throw std::invalid_argument("--point-type must be xyz, xyzi, xyzrgb, xyzrgba or xyzinormal");
}

const char*
indexModeName(IndexMode mode)
{
  return mode == IndexMode::Dense ? "dense" : "indexed";
}

const char*
pathModeName(PathMode mode)
{
  return mode == PathMode::Diagnostic ? "diagnostic" : "production";
}

const char*
polygonModeName(PolygonMode mode)
{
  return mode == PolygonMode::Single ? "single" : "nested";
}

const char*
pointTypeModeName(PointTypeMode mode)
{
  switch (mode) {
    case PointTypeMode::PointXYZ:
      return "xyz";
    case PointTypeMode::PointXYZI:
      return "xyzi";
    case PointTypeMode::PointXYZRGB:
      return "xyzrgb";
    case PointTypeMode::PointXYZRGBA:
      return "xyzrgba";
    default:
      return "xyzinormal";
  }
}

BenchConfig
parseArgs(int argc, char** argv)
{
  BenchConfig config;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string key = argv[i];
    if (key == "--size") {
      config.cloud_size = static_cast<std::size_t>(std::atoi(argv[i + 1]));
    }
    else if (key == "--iterations") {
      config.iterations = std::atoi(argv[i + 1]);
    }
    else if (key == "--warmup") {
      config.warmup_iterations = std::atoi(argv[i + 1]);
    }
    else if (key == "--half-extent") {
      config.half_extent = std::atof(argv[i + 1]);
    }
    else if (key == "--indices") {
      config.index_mode = parseIndexMode(argv[i + 1]);
    }
    else if (key == "--path") {
      config.path_mode = parsePathMode(argv[i + 1]);
    }
    else if (key == "--polygons") {
      config.polygon_mode = parsePolygonMode(argv[i + 1]);
    }
    else if (key == "--point-type") {
      config.point_type_mode = parsePointTypeMode(argv[i + 1]);
    }
  }
  if (config.path_mode != PathMode::Production &&
      config.point_type_mode != PointTypeMode::PointXYZ) {
    throw std::invalid_argument("--point-type other than xyz requires --path production");
  }
  return config;
}

template <typename PointT>
pcl::PointCloud<PointT>
makeCloud(const BenchConfig& config)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(config.cloud_size);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(config.cloud_size);
  for (std::size_t i = 0; i < config.cloud_size; ++i) {
    const int x_code = static_cast<int>((i * 37) % 4099) - 2049;
    const int y_code = static_cast<int>((i * 53) % 4093) - 2046;
    cloud[i].x = static_cast<float>(x_code) * 0.0625f;
    cloud[i].y = static_cast<float>(y_code) * 0.0625f;
    cloud[i].z = (i % 7 == 0) ? 0.18f : 0.02f;
  }
  return cloud;
}

std::vector<int>
makeDenseIndices(std::size_t n)
{
  std::vector<int> indices(n);
  for (std::size_t i = 0; i < n; ++i) {
    indices[i] = static_cast<int>(i);
  }
  return indices;
}

std::vector<int>
makeIndexedIndices(std::size_t n)
{
  std::vector<int> indices(n);
  if (n == 0) {
    return indices;
  }
  if (n == 1) {
    indices[0] = 0;
    return indices;
  }

  const std::size_t step = (n % 2 == 0) ? (n - 1) : (n - 2);
  for (std::size_t i = 0; i < n; ++i) {
    indices[i] = static_cast<int>((i * step + 1) % n);
  }
  return indices;
}

std::vector<int>
makeIndices(const BenchConfig& config)
{
  if (config.index_mode == IndexMode::Indexed) {
    return makeIndexedIndices(config.cloud_size);
  }
  return makeDenseIndices(config.cloud_size);
}

template <typename PointT>
PointT
makePoint(float x, float y, float z)
{
  PointT point;
  point.x = x;
  point.y = y;
  point.z = z;
  return point;
}

template <typename PointT>
std::vector<std::vector<PointT>>
makeSquarePolygon(float half_extent)
{
  return {{
      makePoint<PointT>(-half_extent, -half_extent, 0.0f),
      makePoint<PointT>(half_extent, -half_extent, 0.0f),
      makePoint<PointT>(half_extent, half_extent, 0.0f),
      makePoint<PointT>(-half_extent, half_extent, 0.0f),
  }};
}

template <typename PointT>
std::vector<std::vector<PointT>>
makeNestedPolygons(float half_extent)
{
  const float inner_extent = half_extent * 0.25f;
  return {
      {
          makePoint<PointT>(-half_extent, -half_extent, 0.0f),
          makePoint<PointT>(half_extent, -half_extent, 0.0f),
          makePoint<PointT>(half_extent, half_extent, 0.0f),
          makePoint<PointT>(-half_extent, half_extent, 0.0f),
      },
      {
          makePoint<PointT>(-inner_extent, -inner_extent, 0.0f),
          makePoint<PointT>(inner_extent, -inner_extent, 0.0f),
          makePoint<PointT>(inner_extent, inner_extent, 0.0f),
          makePoint<PointT>(-inner_extent, inner_extent, 0.0f),
      },
  };
}

template <typename PointT>
std::vector<std::vector<PointT>>
makePolygons(const BenchConfig& config)
{
  if (config.polygon_mode == PolygonMode::Nested) {
    return makeNestedPolygons<PointT>(config.half_extent);
  }
  return makeSquarePolygon<PointT>(config.half_extent);
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeHull(const std::vector<std::vector<PointT>>& polygons)
{
  auto hull = typename pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
  for (const auto& polygon : polygons) {
    for (const auto& point : polygon) {
      hull->push_back(point);
    }
  }
  hull->width = static_cast<std::uint32_t>(hull->size());
  hull->height = 1;
  hull->is_dense = true;
  return hull;
}

template <typename PointT>
std::vector<pcl::Vertices>
makePolygonVertices(const std::vector<std::vector<PointT>>& polygons)
{
  std::vector<pcl::Vertices> vertices;
  vertices.reserve(polygons.size());
  std::uint32_t offset = 0;
  for (const auto& polygon : polygons) {
    pcl::Vertices polygon_vertices;
    polygon_vertices.vertices.reserve(polygon.size());
    for (std::size_t i = 0; i < polygon.size(); ++i) {
      polygon_vertices.vertices.push_back(offset + static_cast<std::uint32_t>(i));
    }
    vertices.push_back(polygon_vertices);
    offset += static_cast<std::uint32_t>(polygon.size());
  }
  return vertices;
}

std::vector<pcl::PointXYZ>
makeProjectedPoints(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const std::vector<int>& indices)
{
  std::vector<pcl::PointXYZ> projected;
  projected.reserve(indices.size());
  for (const int index : indices) {
    const auto& point = cloud[static_cast<std::size_t>(index)];
    projected.emplace_back(point.x, point.y, 0.0f);
  }
  return projected;
}

std::uint64_t
checksumIndices(const std::vector<int>& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int index : indices) {
    sum ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(index));
    sum *= 1099511628211ull;
  }
  return sum;
}

template <typename PointT>
std::vector<int>
runProductionSegment(const typename pcl::PointCloud<PointT>::Ptr& cloud,
                     const std::vector<int>& indices,
                     const std::vector<std::vector<PointT>>& polygons)
{
  pcl::ExtractPolygonalPrismData<PointT> prism;
  prism.setInputCloud(cloud);
  prism.setIndices(std::make_shared<std::vector<int>>(indices));
  prism.setInputPlanarHull(makeHull<PointT>(polygons));
  if (polygons.size() > 1) {
    prism.setPolygons(makePolygonVertices<PointT>(polygons));
  }
  prism.setHeightLimits(-20.0, 20.0);
  prism.setViewPoint(0.0f, 0.0f, 512.0f);

  pcl::PointIndices output;
  prism.segment(output);
  return output.indices;
}

template <typename Fn>
std::uint64_t
timeCase(const BenchConfig& config, const std::string& label, Fn&& fn)
{
  std::uint64_t checksum = 0;
  const auto mix_checksum = [](std::uint64_t seed, std::uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
  };
  for (int i = 0; i < config.warmup_iterations; ++i) {
    checksum = mix_checksum(checksum, checksumIndices(fn()));
  }

  const auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i) {
    checksum = mix_checksum(checksum, checksumIndices(fn()));
  }
  const auto end = std::chrono::steady_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - begin).count();

  std::cout << label << " : " << std::fixed << std::setprecision(6)
            << (elapsed / static_cast<double>(config.iterations)) << " ms / iter\n";
  std::cout << "Checksum " << label << " : " << checksum << "\n";
  return checksum;
}

template <typename PointT>
int
runProductionBench(const BenchConfig& config, const std::string& label)
{
  auto cloud = typename pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>(makeCloud<PointT>(config)));
  const auto indices = makeIndices(config);
  const auto polygons = makePolygons<PointT>(config);

  volatile std::uint64_t sink = 0;
  sink ^= timeCase(config, label, [&]() {
    return runProductionSegment<PointT>(cloud, indices, polygons);
  });

  return sink == 0x1234u ? 1 : 0;
}

} // namespace

int
main(int argc, char** argv)
{
  const BenchConfig config = parseArgs(argc, argv);
  std::cout << "Dataset: synthetic EPPD full-scan " << polygonModeName(config.polygon_mode)
            << " polygon, cloud="
            << config.cloud_size << ", half_extent=" << config.half_extent
            << ", indices=" << indexModeName(config.index_mode)
            << ", path=" << pathModeName(config.path_mode)
            << ", polygons=" << polygonModeName(config.polygon_mode)
            << ", point_type=" << pointTypeModeName(config.point_type_mode) << "\n";
  std::cout << "Iterations: " << config.iterations << "\n";
  std::cout << "Warmup Iterations: " << config.warmup_iterations << "\n";

  std::string label =
      std::string("eppd ") + polygonModeName(config.polygon_mode) + " polygon full-scan " +
      pathModeName(config.path_mode) + " indices=" + indexModeName(config.index_mode);
  if (config.point_type_mode != PointTypeMode::PointXYZ) {
    label += std::string(" point-type=") + pointTypeModeName(config.point_type_mode);
  }

  if (config.path_mode == PathMode::Production) {
    switch (config.point_type_mode) {
      case PointTypeMode::PointXYZ:
        return runProductionBench<pcl::PointXYZ>(config, label);
      case PointTypeMode::PointXYZI:
        return runProductionBench<pcl::PointXYZI>(config, label);
      case PointTypeMode::PointXYZRGB:
        return runProductionBench<pcl::PointXYZRGB>(config, label);
      case PointTypeMode::PointXYZRGBA:
        return runProductionBench<pcl::PointXYZRGBA>(config, label);
      case PointTypeMode::PointXYZINormal:
        return runProductionBench<pcl::PointXYZINormal>(config, label);
    }
  }

  auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>(makeCloud<pcl::PointXYZ>(config)));
  const auto indices = makeIndices(config);
  const auto polygons = makePolygons<pcl::PointXYZ>(config);
  const auto projected_points = makeProjectedPoints(*cloud, indices);
  const Eigen::Vector4f model_coefficients(0.25f, -0.125f, 0.0f, 0.0f);

  volatile std::uint64_t sink = 0;
  sink ^= timeCase(config, label, [&]() {
#if defined(__RVV10__)
    return eppd::segmentPolygonalPrismRvvFullScanCandidate(
        cloud->points,
        indices,
        polygons,
        projected_points,
        model_coefficients,
        -20.0f,
        20.0f,
        0,
        1);
#else
    return eppd::segmentPolygonalPrismFullScanReference(cloud->points,
                                                        indices,
                                                        polygons,
                                                        projected_points,
                                                        model_coefficients,
                                                        -20.0f,
                                                        20.0f,
                                                        0,
                                                        1);
#endif
  });

  return sink == 0x1234u ? 1 : 0;
}
