/*
 * 本文件做什么：
 * 本文件复刻 MarchingCubesRBF::voxelizeData() 里的三个主要阶段：
 * 1. RBF matrix fill（RBF 矩阵填充）。
 * 2. Eigen fullPivLu solve（Eigen 求解器，保持标量库实现）。
 * 3. voxel grid evaluation（体素网格求值）。
 *
 * RVV（RISC-V Vector，可变向量扩展）candidate 只接管 matrix fill 和 voxel
 * evaluation 中的 cubic-distance kernel（r^3 距离核）。它不接管 Eigen solve，
 * 也不接管 MarchingCubes::createSurface() 的可变三角形输出。
 */

#pragma once

#include <Eigen/Dense>

#include <pcl/memory.h>
#include <pcl/point_types.h>
#include <pcl/surface/marching_cubes_rbf.h>
#include <pcl/surface/impl/marching_cubes_rbf.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::surface::rvv_marching_cubes_rbf_support {

using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>;
using Vector = Eigen::Matrix<double, Eigen::Dynamic, 1>;

struct RbfInput {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> z;
  std::vector<double> nx;
  std::vector<double> ny;
  std::vector<double> nz;
  double off_surface_epsilon = 0.02;
};

struct Centers {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> z;
};

struct GridSpec {
  int res_x = 20;
  int res_y = 20;
  int res_z = 20;
  Eigen::Array3d lower {-1.25, -1.25, -1.25};
  Eigen::Array3d size_voxel {0.125, 0.125, 0.125};
  double iso_level = 0.0;
};

struct ComponentTimes {
  double prepare_ms = 0.0;
  double matrix_ms = 0.0;
  double solve_ms = 0.0;
  double voxel_ms = 0.0;
  double active_scan_ms = 0.0;
};

struct RunStats {
  std::uint64_t checksum = 1469598103934665603ull;
  double max_matrix_abs_diff = 0.0;
  double max_grid_abs_diff = 0.0;
  std::size_t active_cells = 0;
  std::size_t grid_values = 0;
  ComponentTimes times;
};

inline std::uint64_t
mixChecksum(std::uint64_t seed, const std::uint64_t value)
{
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

inline std::uint64_t
doubleBits(const double value)
{
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline std::uint64_t
quantizedDoubleBits(const double value)
{
  const auto quantized = static_cast<std::int64_t>(std::llround(value * 1.0e6));
  return static_cast<std::uint64_t>(quantized);
}

template <typename Fn>
double
measureOnce(Fn&& fn)
{
  const auto start = std::chrono::high_resolution_clock::now();
  fn();
  const auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(end - start).count();
}

inline double
cubicKernel(const double cx,
            const double cy,
            const double cz,
            const double px,
            const double py,
            const double pz)
{
  const double dx = px - cx;
  const double dy = py - cy;
  const double dz = pz - cz;
  const double r2 = dx * dx + dy * dy + dz * dz;
  return r2 * std::sqrt(r2);
}

inline RbfInput
makeInput(const int points)
{
  RbfInput input;
  input.x.reserve(points);
  input.y.reserve(points);
  input.z.reserve(points);
  input.nx.reserve(points);
  input.ny.reserve(points);
  input.nz.reserve(points);

  for (int i = 0; i < points; ++i) {
    const double t = 0.37 * static_cast<double>(i);
    const double u = 0.19 * static_cast<double>((i * 7) % 23);
    input.x.push_back(0.82 * std::sin(t) + 0.05 * std::cos(u));
    input.y.push_back(0.74 * std::cos(0.73 * t) + 0.03 * std::sin(u));
    input.z.push_back(0.58 * std::sin(0.41 * t + 0.2));

    Eigen::Vector3d n(input.x.back(), input.y.back(), input.z.back());
    if (n.norm() < 1.0e-9)
      n = Eigen::Vector3d(0.0, 0.0, 1.0);
    n.normalize();
    input.nx.push_back(n.x());
    input.ny.push_back(n.y());
    input.nz.push_back(n.z());
  }
  return input;
}

inline Centers
makeCenters(const RbfInput& input)
{
  const std::size_t n = input.x.size();
  Centers centers;
  centers.x.resize(2 * n);
  centers.y.resize(2 * n);
  centers.z.resize(2 * n);
  for (std::size_t i = 0; i < n; ++i) {
    centers.x[i] = input.x[i];
    centers.y[i] = input.y[i];
    centers.z[i] = input.z[i];
    centers.x[i + n] = input.x[i] + input.nx[i] * input.off_surface_epsilon;
    centers.y[i + n] = input.y[i] + input.ny[i] * input.off_surface_epsilon;
    centers.z[i + n] = input.z[i] + input.nz[i] * input.off_surface_epsilon;
  }
  return centers;
}

inline Vector
makeRhs(const std::size_t center_count, const double off_surface_epsilon)
{
  Vector rhs(center_count);
  const std::size_t n = center_count / 2;
  for (std::size_t row = 0; row < center_count; ++row)
    rhs(static_cast<Eigen::Index>(row), 0) = row >= n ? off_surface_epsilon : 0.0;
  return rhs;
}

inline void
fillMatrixSourceOrderScalar(const Centers& centers, Matrix& matrix)
{
  const std::size_t count = centers.x.size();
  matrix.resize(static_cast<Eigen::Index>(count), static_cast<Eigen::Index>(count));
  for (std::size_t row = 0; row < count; ++row) {
    for (std::size_t col = 0; col < count; ++col) {
      matrix(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) =
          cubicKernel(centers.x[col],
                      centers.y[col],
                      centers.z[col],
                      centers.x[row],
                      centers.y[row],
                      centers.z[row]);
    }
  }
}

inline void
fillMatrixColumnOrderScalar(const Centers& centers, Matrix& matrix)
{
  const std::size_t count = centers.x.size();
  matrix.resize(static_cast<Eigen::Index>(count), static_cast<Eigen::Index>(count));
  for (std::size_t col = 0; col < count; ++col) {
    const double cx = centers.x[col];
    const double cy = centers.y[col];
    const double cz = centers.z[col];
    for (std::size_t row = 0; row < count; ++row) {
      matrix(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) =
          cubicKernel(cx, cy, cz, centers.x[row], centers.y[row], centers.z[row]);
    }
  }
}

inline void
fillMatrixCandidate(const Centers& centers, Matrix& matrix)
{
#if defined(__RVV10__)
  const std::size_t count = centers.x.size();
  matrix.resize(static_cast<Eigen::Index>(count), static_cast<Eigen::Index>(count));
  for (std::size_t col = 0; col < count; ++col) {
    const double cx = centers.x[col];
    const double cy = centers.y[col];
    const double cz = centers.z[col];
    std::size_t row = 0;
    while (row < count) {
      const std::size_t vl = __riscv_vsetvl_e64m1(count - row);
      const vfloat64m1_t px = __riscv_vle64_v_f64m1(centers.x.data() + row, vl);
      const vfloat64m1_t py = __riscv_vle64_v_f64m1(centers.y.data() + row, vl);
      const vfloat64m1_t pz = __riscv_vle64_v_f64m1(centers.z.data() + row, vl);
      const vfloat64m1_t dx = __riscv_vfsub_vf_f64m1(px, cx, vl);
      const vfloat64m1_t dy = __riscv_vfsub_vf_f64m1(py, cy, vl);
      const vfloat64m1_t dz = __riscv_vfsub_vf_f64m1(pz, cz, vl);
      vfloat64m1_t r2 = __riscv_vfmul_vv_f64m1(dx, dx, vl);
      r2 = __riscv_vfmacc_vv_f64m1(r2, dy, dy, vl);
      r2 = __riscv_vfmacc_vv_f64m1(r2, dz, dz, vl);
      const vfloat64m1_t r = __riscv_vfsqrt_v_f64m1(r2, vl);
      const vfloat64m1_t out = __riscv_vfmul_vv_f64m1(r2, r, vl);
      __riscv_vse64_v_f64m1(&matrix(static_cast<Eigen::Index>(row),
                                    static_cast<Eigen::Index>(col)),
                            out,
                            vl);
      row += vl;
    }
  }
#else
  fillMatrixColumnOrderScalar(centers, matrix);
#endif
}

inline std::vector<double>
evaluateGridScalar(const Centers& centers, const Vector& weights, const GridSpec& spec)
{
  std::vector<double> grid(static_cast<std::size_t>(spec.res_x * spec.res_y * spec.res_z));
  for (int x = 0; x < spec.res_x; ++x) {
    for (int y = 0; y < spec.res_y; ++y) {
      for (int z = 0; z < spec.res_z; ++z) {
        const Eigen::Array3d p =
            spec.size_voxel * Eigen::Array3d(static_cast<double>(x),
                                             static_cast<double>(y),
                                             static_cast<double>(z)) +
            spec.lower;
        double f = 0.0;
        for (std::size_t i = 0; i < centers.x.size(); ++i)
          f += weights(static_cast<Eigen::Index>(i), 0) *
               cubicKernel(centers.x[i], centers.y[i], centers.z[i], p.x(), p.y(), p.z());
        grid[static_cast<std::size_t>(x * spec.res_y * spec.res_z + y * spec.res_z + z)] = f;
      }
    }
  }
  return grid;
}

inline std::vector<double>
evaluateGridCandidate(const Centers& centers, const Vector& weights, const GridSpec& spec)
{
#if defined(__RVV10__)
  std::vector<double> grid(static_cast<std::size_t>(spec.res_x * spec.res_y * spec.res_z));
  for (int x = 0; x < spec.res_x; ++x) {
    for (int y = 0; y < spec.res_y; ++y) {
      for (int z = 0; z < spec.res_z; ++z) {
        const Eigen::Array3d p =
            spec.size_voxel * Eigen::Array3d(static_cast<double>(x),
                                             static_cast<double>(y),
                                             static_cast<double>(z)) +
            spec.lower;
        double f = 0.0;
        std::size_t i = 0;
        while (i < centers.x.size()) {
          const std::size_t vl = __riscv_vsetvl_e64m1(centers.x.size() - i);
          const vfloat64m1_t cx = __riscv_vle64_v_f64m1(centers.x.data() + i, vl);
          const vfloat64m1_t cy = __riscv_vle64_v_f64m1(centers.y.data() + i, vl);
          const vfloat64m1_t cz = __riscv_vle64_v_f64m1(centers.z.data() + i, vl);
          const vfloat64m1_t w = __riscv_vle64_v_f64m1(weights.data() + i, vl);
          const vfloat64m1_t dx = __riscv_vfsub_vf_f64m1(cx, p.x(), vl);
          const vfloat64m1_t dy = __riscv_vfsub_vf_f64m1(cy, p.y(), vl);
          const vfloat64m1_t dz = __riscv_vfsub_vf_f64m1(cz, p.z(), vl);
          vfloat64m1_t r2 = __riscv_vfmul_vv_f64m1(dx, dx, vl);
          r2 = __riscv_vfmacc_vv_f64m1(r2, dy, dy, vl);
          r2 = __riscv_vfmacc_vv_f64m1(r2, dz, dz, vl);
          const vfloat64m1_t r = __riscv_vfsqrt_v_f64m1(r2, vl);
          const vfloat64m1_t weighted = __riscv_vfmul_vv_f64m1(w, __riscv_vfmul_vv_f64m1(r2, r, vl), vl);
          vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vl);
          const vfloat64m1_t reduced = __riscv_vfredusum_vs_f64m1_f64m1(weighted, zero, vl);
          f += __riscv_vfmv_f_s_f64m1_f64(reduced);
          i += vl;
        }
        grid[static_cast<std::size_t>(x * spec.res_y * spec.res_z + y * spec.res_z + z)] = f;
      }
    }
  }
  return grid;
#else
  return evaluateGridScalar(centers, weights, spec);
#endif
}

inline std::size_t
countActiveCells(const std::vector<double>& grid, const GridSpec& spec)
{
  std::size_t active = 0;
  const auto value_at = [&](const int x, const int y, const int z) {
    return grid[static_cast<std::size_t>(x * spec.res_y * spec.res_z + y * spec.res_z + z)];
  };
  for (int x = 1; x < spec.res_x - 1; ++x) {
    for (int y = 1; y < spec.res_y - 1; ++y) {
      for (int z = 1; z < spec.res_z - 1; ++z) {
        int cube_index = 0;
        const double leaf[8] = {value_at(x, y, z),
                                value_at(x + 1, y, z),
                                value_at(x + 1, y, z + 1),
                                value_at(x, y, z + 1),
                                value_at(x, y + 1, z),
                                value_at(x + 1, y + 1, z),
                                value_at(x + 1, y + 1, z + 1),
                                value_at(x, y + 1, z + 1)};
        for (int i = 0; i < 8; ++i)
          cube_index |= leaf[i] < spec.iso_level ? (1 << i) : 0;
        active += cube_index != 0 && cube_index != 255 ? 1u : 0u;
      }
    }
  }
  return active;
}

inline double
maxAbsDiff(const Matrix& a, const Matrix& b)
{
  double max_diff = 0.0;
  for (Eigen::Index col = 0; col < a.cols(); ++col)
    for (Eigen::Index row = 0; row < a.rows(); ++row)
      max_diff = std::max(max_diff, std::abs(a(row, col) - b(row, col)));
  return max_diff;
}

inline double
maxAbsDiff(const std::vector<double>& a, const std::vector<double>& b)
{
  double max_diff = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i)
    max_diff = std::max(max_diff, std::abs(a[i] - b[i]));
  return max_diff;
}

inline std::uint64_t
checksumGrid(const std::vector<double>& grid)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const double value : grid)
    checksum = mixChecksum(checksum, quantizedDoubleBits(value));
  return checksum;
}

inline std::uint64_t
checksumGridSigns(const std::vector<double>& grid, const GridSpec& spec)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const double value : grid)
    checksum = mixChecksum(checksum, value < spec.iso_level ? 1u : 0u);
  return checksum;
}

inline RunStats
runFullCandidatePipeline(const int points, const int resolution)
{
  RunStats stats;
  GridSpec spec;
  spec.res_x = resolution;
  spec.res_y = resolution;
  spec.res_z = resolution;
  spec.size_voxel = Eigen::Array3d(2.5 / static_cast<double>(resolution),
                                   2.5 / static_cast<double>(resolution),
                                   2.5 / static_cast<double>(resolution));

  RbfInput input;
  Centers centers;
  Matrix matrix;
  Vector rhs;
  Vector weights;
  std::vector<double> grid;

  stats.times.prepare_ms = measureOnce([&]() {
    input = makeInput(points);
    centers = makeCenters(input);
    rhs = makeRhs(centers.x.size(), input.off_surface_epsilon);
  });
  stats.times.matrix_ms = measureOnce([&]() { fillMatrixCandidate(centers, matrix); });
  stats.times.solve_ms = measureOnce([&]() { weights = matrix.fullPivLu().solve(rhs); });
  stats.times.voxel_ms = measureOnce([&]() { grid = evaluateGridCandidate(centers, weights, spec); });
  stats.times.active_scan_ms = measureOnce([&]() { stats.active_cells = countActiveCells(grid, spec); });
  stats.grid_values = grid.size();
  stats.checksum = mixChecksum(checksumGridSigns(grid, spec), stats.active_cells);
  return stats;
}

template <typename PointT>
inline typename pcl::PointCloud<PointT>::Ptr
makeNormalPointCloud(const RbfInput& input)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->points.resize(input.x.size());
  cloud->width = static_cast<std::uint32_t>(input.x.size());
  cloud->height = 1;
  cloud->is_dense = true;

  for (std::size_t i = 0; i < input.x.size(); ++i) {
    PointT& point = cloud->points[i];
    point.x = static_cast<float>(input.x[i]);
    point.y = static_cast<float>(input.y[i]);
    point.z = static_cast<float>(input.z[i]);
    point.normal_x = static_cast<float>(input.nx[i]);
    point.normal_y = static_cast<float>(input.ny[i]);
    point.normal_z = static_cast<float>(input.nz[i]);
    if constexpr (std::is_same_v<PointT, pcl::PointXYZINormal>)
      point.intensity = 1.0f;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBNormal>) {
      point.r = static_cast<std::uint8_t>((31 * i) % 255);
      point.g = static_cast<std::uint8_t>((47 * i) % 255);
      point.b = static_cast<std::uint8_t>((61 * i) % 255);
      point.a = 255;
      point.curvature = 0.01f * static_cast<float>((i % 7) + 1);
    }
  }
  return cloud;
}

inline GridSpec
makeProductionGridSpec(const int resolution)
{
  GridSpec spec;
  spec.res_x = resolution;
  spec.res_y = resolution;
  spec.res_z = resolution;
  spec.size_voxel = Eigen::Array3d(2.5 / static_cast<double>(resolution),
                                   2.5 / static_cast<double>(resolution),
                                   2.5 / static_cast<double>(resolution));
  return spec;
}

template <typename PointT>
class ProductionProbeRBF final : public pcl::MarchingCubesRBF<PointT> {
public:
  using Base = pcl::MarchingCubesRBF<PointT>;

  explicit ProductionProbeRBF(const float off_surface_epsilon)
  : Base(off_surface_epsilon, 0.0f, 0.0f)
  {}

  void
  configure(const typename pcl::PointCloud<PointT>::ConstPtr& cloud, const GridSpec& spec)
  {
    this->input_ = cloud;
    this->res_x_ = spec.res_x;
    this->res_y_ = spec.res_y;
    this->res_z_ = spec.res_z;
    this->lower_boundary_ = spec.lower.cast<float>();
    this->size_voxel_ = spec.size_voxel.cast<float>();
    this->grid_.assign(static_cast<std::size_t>(spec.res_x * spec.res_y * spec.res_z), NAN);
  }

  const std::vector<float>&
  grid() const
  {
    return this->grid_;
  }
};

inline std::vector<double>
toDoubleGrid(const std::vector<float>& grid)
{
  std::vector<double> result;
  result.reserve(grid.size());
  for (const float value : grid)
    result.push_back(static_cast<double>(value));
  return result;
}

template <typename PointT>
inline std::vector<float>
runProductionVoxelizeGrid(const int points, const int resolution)
{
  RbfInput input = makeInput(points);
  const GridSpec spec = makeProductionGridSpec(resolution);
  auto cloud = makeNormalPointCloud<PointT>(input);

  ProductionProbeRBF<PointT> probe(static_cast<float>(input.off_surface_epsilon));
  probe.configure(cloud, spec);
  probe.voxelizeData();
  return probe.grid();
}

template <typename PointT>
inline RunStats
runProductionVoxelizePointNormalPipeline(const int points, const int resolution)
{
  RunStats stats;
  RbfInput input = makeInput(points);
  const GridSpec spec = makeProductionGridSpec(resolution);
  auto cloud = makeNormalPointCloud<PointT>(input);
  std::vector<float> grid;

  stats.times.voxel_ms = measureOnce([&]() {
    ProductionProbeRBF<PointT> probe(static_cast<float>(input.off_surface_epsilon));
    probe.configure(cloud, spec);
    probe.voxelizeData();
    grid = probe.grid();
  });

  const auto double_grid = toDoubleGrid(grid);
  stats.active_cells = countActiveCells(double_grid, spec);
  stats.grid_values = grid.size();
  stats.checksum = mixChecksum(checksumGridSigns(double_grid, spec), stats.active_cells);
  return stats;
}

} // namespace pcl::surface::rvv_marching_cubes_rbf_support
