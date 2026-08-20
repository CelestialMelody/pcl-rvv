/*
 * 本文件做什么：
 * 这是 marching_cubes 的 helper-level 和 production-direct bench（性能测试）入口。
 * Std build 运行标量 reference path，RVV build 在 __RVV10__ 下运行 RVV candidate
 * 或 production active-cell prepass。
 *
 * 证据边界：
 * 本 bench 只测 createSurface 风格的 grid cell -> triangle emission，不包含 Hoppe
 * nearestKSearch、RBF solver 或真实 production dispatch。QEMU 只用于构建和日志形状；
 * 性能结论必须来自板卡或目标硬件。
 */

#include "marching_cubes.h"

#include <pcl/rvv_point_traits.h>
#include <pcl/surface/marching_cubes.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace mc = pcl::surface::rvv_marching_cubes_support;

namespace {

constexpr int kDefaultIterations = 12;
constexpr int kDefaultWarmupIterations = 2;

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

int
parseIntArg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter") {
      const std::string selected(argv[i + 1]);
      return selected == requested || selected == "all";
    }
  }
  return true;
}

template <typename PointT>
class SyntheticMarchingCubes final : public pcl::MarchingCubes<PointT> {
public:
  SyntheticMarchingCubes(const mc::GridSpec& spec, std::vector<float> grid)
  : pcl::MarchingCubes<PointT>(0.0f, spec.iso_level), grid_values_(std::move(grid))
  {
    this->setGridResolution(spec.res_x, spec.res_y, spec.res_z);
    auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
    cloud->resize(2);
    (*cloud)[0].x = spec.lower[0];
    (*cloud)[0].y = spec.lower[1];
    (*cloud)[0].z = spec.lower[2];
    const Eigen::Array3f upper =
        spec.lower + spec.size_voxel * Eigen::Array3f(static_cast<float>(spec.res_x),
                                                      static_cast<float>(spec.res_y),
                                                      static_cast<float>(spec.res_z));
    (*cloud)[1].x = upper[0];
    (*cloud)[1].y = upper[1];
    (*cloud)[1].z = upper[2];
    this->setInputCloud(cloud);
  }

private:
  void
  voxelizeData() override
  {
    this->grid_ = grid_values_;
  }

  std::vector<float> grid_values_;
};

template <typename PointT>
std::uint64_t
updatePointChecksum(const PointT& point, std::uint64_t checksum)
{
  checksum = mc::mixChecksum(checksum, mc::floatBits(point.x));
  checksum = mc::mixChecksum(checksum, mc::floatBits(point.y));
  checksum = mc::mixChecksum(checksum, mc::floatBits(point.z));
  return checksum;
}

template <typename PointT>
mc::SurfaceStats
runProductionCaseT(const mc::GridSpec& spec,
                   const std::vector<float>& grid)
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value);
  SyntheticMarchingCubes<PointT> production(spec, grid);
  pcl::PointCloud<PointT> points;
  std::vector<pcl::Vertices> polygons;
  production.reconstruct(points, polygons);

  mc::SurfaceStats stats;
  stats.points = points.size();
  stats.triangles = polygons.size();
  stats.active_cells = polygons.size();
  for (const auto& point : points)
    updatePointChecksum(point, stats.checksum);
  stats.checksum = mc::mixChecksum(stats.checksum, stats.points);
  stats.checksum = mc::mixChecksum(stats.checksum, stats.triangles);
  stats.checksum = mc::mixChecksum(stats.checksum, stats.active_cells);
  return stats;
}

void
runCase(const std::string& label,
        const int iterations,
        const int warmup_iterations,
        const std::function<mc::SurfaceStats()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  mc::SurfaceStats last;
  for (int i = 0; i < warmup_iterations; ++i) {
    last = fn();
    checksum = mc::mixChecksum(checksum, last.checksum + static_cast<std::uint64_t>(i));
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    last = fn();
    checksum = mc::mixChecksum(checksum, last.checksum + static_cast<std::uint64_t>(i + 17));
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(56) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", points: " << last.points
            << ", active_cells: " << last.active_cells << '\n';
  doNotOptimize(checksum);
}

void
runGridCase(const int argc,
            char** argv,
            const std::string& label,
            const int resolution,
            const mc::GridKind kind,
            const bool use_prepass,
            const int iterations,
            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto path = [&spec, &grid, use_prepass]() {
#if defined(__RVV10__)
    return use_prepass ? mc::runPrepassCandidate(spec, grid) : mc::runCandidate(spec, grid);
#else
    (void)use_prepass;
    return mc::runReference(spec, grid);
#endif
  };
  runCase(label, iterations, warmup_iterations, path);
}

void
runProductionCase(const int argc,
                  char** argv,
                  const std::string& label,
                  const int resolution,
                  const mc::GridKind kind,
                  const int iterations,
                  const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto path = [&spec, &grid]() {
    return runProductionCaseT<pcl::PointNormal>(spec, grid);
  };
  runCase(label, iterations, warmup_iterations, path);
}

template <typename PointT>
void
runGenericProductionCase(const int argc,
                         char** argv,
                         const std::string& label,
                         const int resolution,
                         const mc::GridKind kind,
                         const int iterations,
                         const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto path = [&spec, &grid]() {
    return runProductionCaseT<PointT>(spec, grid);
  };
  runCase(label, iterations, warmup_iterations, path);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: synthetic signed-distance grids for marching_cubes createSurface diagnostic\n";
  std::cout << "Image Size: 96 x 96\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build: RVV marching_cubes candidates (__RVV10__)\n";
#else
  std::cout << "Build: scalar marching_cubes reference\n";
#endif

  runGridCase(argc,
              argv,
              "mc_edge_sphere_64",
              64,
              mc::GridKind::sphere,
              false,
              iterations,
              warmup_iterations);
  runGridCase(argc,
              argv,
              "mc_edge_wave_72",
              72,
              mc::GridKind::wave,
              false,
              iterations,
              warmup_iterations);
  runGridCase(argc,
              argv,
              "mc_edge_sparse_sphere_80",
              80,
              mc::GridKind::sparse_sphere,
              false,
              iterations,
              warmup_iterations);

  runGridCase(argc,
              argv,
              "mc_prepass_sphere_64",
              64,
              mc::GridKind::sphere,
              true,
              iterations,
              warmup_iterations);
  runGridCase(argc,
              argv,
              "mc_prepass_wave_72",
              72,
              mc::GridKind::wave,
              true,
              iterations,
              warmup_iterations);
  runGridCase(argc,
              argv,
              "mc_prepass_sparse_sphere_80",
              80,
              mc::GridKind::sparse_sphere,
              true,
              iterations,
              warmup_iterations);

  runProductionCase(argc,
                    argv,
                    "mc_prod_sphere_64",
                    64,
                    mc::GridKind::sphere,
                    iterations,
                    warmup_iterations);
  runProductionCase(argc,
                    argv,
                    "mc_prod_wave_72",
                    72,
                    mc::GridKind::wave,
                    iterations,
                    warmup_iterations);
  runProductionCase(argc,
                    argv,
                    "mc_prod_sparse_sphere_80",
                    80,
                    mc::GridKind::sparse_sphere,
                    iterations,
                    warmup_iterations);

  runGenericProductionCase<pcl::PointXYZ>(argc,
                                          argv,
                                          "mc_prod_xyz_64",
                                          64,
                                          mc::GridKind::sphere,
                                          iterations,
                                          warmup_iterations);
  runGenericProductionCase<pcl::PointXYZI>(argc,
                                           argv,
                                           "mc_prod_xyzi_64",
                                           64,
                                           mc::GridKind::sphere,
                                           iterations,
                                           warmup_iterations);
  runGenericProductionCase<pcl::PointXYZRGB>(argc,
                                             argv,
                                             "mc_prod_xyzrgb_64",
                                             64,
                                             mc::GridKind::sphere,
                                             iterations,
                                             warmup_iterations);
  runGenericProductionCase<pcl::PointXYZRGBA>(argc,
                                              argv,
                                              "mc_prod_xyzrgba_64",
                                              64,
                                              mc::GridKind::sphere,
                                              iterations,
                                              warmup_iterations);

  return 0;
}
