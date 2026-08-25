#pragma once

/*
 * 本文件做什么：
 * 这里把 ApproximateProgressiveMorphologicalFilter::extract 中的三个热点片段拆成
 * test-only（仅测试使用）的 component helper：点云到 grid z-min、window open
 *（窗口开运算）和 height-threshold tail compress（高度阈值尾段压缩）。标量 helper
 * 是 same-chain reference（同构参考链路），RVV helper 只证明局部候选语义和指令形态，
 * 不能替代真实 production dispatch（生产分流）证据。
 */

#include <pcl/common/point_tests.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_segmentation_apmf {

struct GridShape {
  Eigen::Vector4f global_min;
  Eigen::Vector4f global_max;
  float cell_size = 1.0f;
  int rows = 0;
  int cols = 0;
};

struct ProgressiveFilterResult {
  Eigen::MatrixXf grid;
  pcl::Indices ground;
};

inline bool
isFiniteXYZ(const pcl::PointXYZ& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline GridShape
computeGridShape(const pcl::PointCloud<pcl::PointXYZ>& cloud, const float cell_size)
{
  GridShape shape;
  shape.cell_size = cell_size;
  shape.global_min.setConstant(std::numeric_limits<float>::max());
  shape.global_max.setConstant(std::numeric_limits<float>::lowest());
  shape.global_min[3] = shape.global_max[3] = 0.0f;

  for (const auto& point : cloud) {
    if (!cloud.is_dense && !isFiniteXYZ(point))
      continue;
    shape.global_min[0] = std::min(shape.global_min[0], point.x);
    shape.global_min[1] = std::min(shape.global_min[1], point.y);
    shape.global_min[2] = std::min(shape.global_min[2], point.z);
    shape.global_max[0] = std::max(shape.global_max[0], point.x);
    shape.global_max[1] = std::max(shape.global_max[1], point.y);
    shape.global_max[2] = std::max(shape.global_max[2], point.z);
  }

  const float xextent = shape.global_max.x() - shape.global_min.x();
  const float yextent = shape.global_max.y() - shape.global_min.y();
  shape.rows = static_cast<int>(std::floor(yextent / cell_size) + 1);
  shape.cols = static_cast<int>(std::floor(xextent / cell_size) + 1);
  return shape;
}

inline int
cellRow(const pcl::PointXYZ& point, const GridShape& shape)
{
  return static_cast<int>(std::floor((point.y - shape.global_min.y()) / shape.cell_size));
}

inline int
cellCol(const pcl::PointXYZ& point, const GridShape& shape)
{
  return static_cast<int>(std::floor((point.x - shape.global_min.x()) / shape.cell_size));
}

inline Eigen::MatrixXf
makeNanGrid(const GridShape& shape)
{
  Eigen::MatrixXf grid(shape.rows, shape.cols);
  grid.setConstant(std::numeric_limits<float>::quiet_NaN());
  return grid;
}

inline void
updateCellMin(Eigen::MatrixXf& grid, const int row, const int col, const float z)
{
  if (row < 0 || row >= grid.rows() || col < 0 || col >= grid.cols())
    return;
  if (std::isnan(grid(row, col)) || z < grid(row, col))
    grid(row, col) = z;
}

inline Eigen::MatrixXf
computeGridZMinStd(const pcl::PointCloud<pcl::PointXYZ>& cloud, const GridShape& shape)
{
  Eigen::MatrixXf grid = makeNanGrid(shape);
  for (const auto& point : cloud) {
    if (!cloud.is_dense && !isFiniteXYZ(point))
      continue;
    updateCellMin(grid, cellRow(point, shape), cellCol(point, shape), point.z);
  }
  return grid;
}

inline float
windowMinStd(const Eigen::MatrixXf& input,
             const int row,
             const int col,
             const int half_size)
{
  const int rows = static_cast<int>(input.rows());
  const int cols = static_cast<int>(input.cols());
  const int rs = std::max(0, row - half_size);
  const int re = std::min(rows - 1, row + half_size);
  const int cs = std::max(0, col - half_size);
  const int ce = std::min(cols - 1, col + half_size);

  float min_coeff = std::numeric_limits<float>::max();
  for (int c = cs; c <= ce; ++c) {
    for (int r = rs; r <= re; ++r) {
      const float value = input(r, c);
      if (!std::isnan(value) && value < min_coeff)
        min_coeff = value;
    }
  }
  return min_coeff == std::numeric_limits<float>::max()
             ? std::numeric_limits<float>::quiet_NaN()
             : min_coeff;
}

inline float
windowMaxStd(const Eigen::MatrixXf& input,
             const int row,
             const int col,
             const int half_size)
{
  const int rows = static_cast<int>(input.rows());
  const int cols = static_cast<int>(input.cols());
  const int rs = std::max(0, row - half_size);
  const int re = std::min(rows - 1, row + half_size);
  const int cs = std::max(0, col - half_size);
  const int ce = std::min(cols - 1, col + half_size);

  float max_coeff = -std::numeric_limits<float>::max();
  for (int c = cs; c <= ce; ++c) {
    for (int r = rs; r <= re; ++r) {
      const float value = input(r, c);
      if (!std::isnan(value) && value > max_coeff)
        max_coeff = value;
    }
  }
  return max_coeff == -std::numeric_limits<float>::max()
             ? std::numeric_limits<float>::quiet_NaN()
             : max_coeff;
}

inline Eigen::MatrixXf
erodeMinStd(const Eigen::MatrixXf& input, const int half_size)
{
  Eigen::MatrixXf out(input.rows(), input.cols());
  out.setConstant(std::numeric_limits<float>::quiet_NaN());
  for (int row = 0; row < input.rows(); ++row)
    for (int col = 0; col < input.cols(); ++col)
      out(row, col) = windowMinStd(input, row, col, half_size);
  return out;
}

inline Eigen::MatrixXf
dilateMaxStd(const Eigen::MatrixXf& input, const int half_size)
{
  Eigen::MatrixXf out(input.rows(), input.cols());
  out.setConstant(std::numeric_limits<float>::quiet_NaN());
  for (int row = 0; row < input.rows(); ++row)
    for (int col = 0; col < input.cols(); ++col)
      out(row, col) = windowMaxStd(input, row, col, half_size);
  return out;
}

inline Eigen::MatrixXf
morphologicalOpenStd(const Eigen::MatrixXf& input, const int half_size)
{
  return dilateMaxStd(erodeMinStd(input, half_size), half_size);
}

inline pcl::Indices
thresholdGroundStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                   const pcl::Indices& ground,
                   const GridShape& shape,
                   const Eigen::MatrixXf& filtered,
                   const float height_threshold)
{
  pcl::Indices out;
  out.reserve(ground.size());
  for (const int index : ground) {
    const auto& point = cloud[static_cast<std::size_t>(index)];
    const int row = cellRow(point, shape);
    const int col = cellCol(point, shape);
    if (row < 0 || row >= filtered.rows() || col < 0 || col >= filtered.cols())
      continue;
    const float diff = point.z - filtered(row, col);
    if (diff < height_threshold)
      out.push_back(index);
  }
  return out;
}

inline pcl::Indices
initialGroundStd(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  pcl::Indices ground;
  ground.reserve(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    if (cloud.is_dense || isFiniteXYZ(cloud[i]))
      ground.push_back(static_cast<int>(i));
  }
  return ground;
}

inline ProgressiveFilterResult
progressiveFilterStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                     const GridShape& shape,
                     const std::vector<int>& half_sizes,
                     const std::vector<float>& height_thresholds)
{
  ProgressiveFilterResult result;
  result.grid = computeGridZMinStd(cloud, shape);
  result.ground = initialGroundStd(cloud);

  const std::size_t iterations = std::min(half_sizes.size(), height_thresholds.size());
  for (std::size_t i = 0; i < iterations; ++i) {
    Eigen::MatrixXf filtered = morphologicalOpenStd(result.grid, half_sizes[i]);
    result.ground =
        thresholdGroundStd(cloud, result.ground, shape, filtered, height_thresholds[i]);
    result.grid.swap(filtered);
  }

  return result;
}

#if defined(__RVV10__)

inline vint32m2_t
floorF32ToI32NoFrm(const vfloat32m2_t values, const std::size_t vl)
{
  const vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2(values, vl);
  const vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2(trunc, vl);
  const vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16(values, trunc_f, vl);
  const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
  const vint32m2_t adjust = __riscv_vmerge_vxm_i32m2(zero, 1, negative_fraction, vl);
  return __riscv_vsub_vv_i32m2(trunc, adjust, vl);
}

inline vbool16_t
finiteMask(const vfloat32m2_t values, const std::size_t vl)
{
  const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16(values, values, vl);
  const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2(values, vl);
  const vfloat32m2_t inf_v = __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16(abs_v, inf_v, vl);
  return __riscv_vmand_mm_b16(eq_self, not_inf, vl);
}

inline bool
computeGridZMinRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                   const GridShape& shape,
                   Eigen::MatrixXf& out)
{
  const std::size_t n = cloud.size();
  if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    return false;

  out = makeNanGrid(shape);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.data());
  const auto max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<int> rows(max_vl);
  std::vector<int> cols(max_vl);
  std::vector<float> zs(max_vl);

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(pcl::PointXYZ),
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(base + i * sizeof(pcl::PointXYZ),
                                                                   vl,
                                                                   vx,
                                                                   vy,
                                                                   vz);

    const vfloat32m2_t shifted_x = __riscv_vfsub_vf_f32m2(vx, shape.global_min.x(), vl);
    const vfloat32m2_t shifted_y = __riscv_vfsub_vf_f32m2(vy, shape.global_min.y(), vl);
    const vfloat32m2_t scaled_x = __riscv_vfdiv_vf_f32m2(shifted_x, shape.cell_size, vl);
    const vfloat32m2_t scaled_y = __riscv_vfdiv_vf_f32m2(shifted_y, shape.cell_size, vl);
    vint32m2_t v_cols = floorF32ToI32NoFrm(scaled_x, vl);
    vint32m2_t v_rows = floorF32ToI32NoFrm(scaled_y, vl);

    vbool16_t keep = __riscv_vmset_m_b16(vl);
    if (!cloud.is_dense) {
      keep = __riscv_vmand_mm_b16(__riscv_vmand_mm_b16(finiteMask(vx, vl), finiteMask(vy, vl), vl),
                                  finiteMask(vz, vl),
                                  vl);
    }

    const vint32m2_t rows_kept = __riscv_vcompress_vm_i32m2(v_rows, keep, vl);
    const vint32m2_t cols_kept = __riscv_vcompress_vm_i32m2(v_cols, keep, vl);
    const vfloat32m2_t zs_kept = __riscv_vcompress_vm_f32m2(vz, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);
    __riscv_vse32_v_i32m2(rows.data(), rows_kept, keep_count);
    __riscv_vse32_v_i32m2(cols.data(), cols_kept, keep_count);
    __riscv_vse32_v_f32m2(zs.data(), zs_kept, keep_count);

    for (std::size_t lane = 0; lane < keep_count; ++lane)
      updateCellMin(out, rows[lane], cols[lane], zs[lane]);

    i += vl;
  }
  return true;
}

inline float
windowMinRVV(const Eigen::MatrixXf& input,
             const int row,
             const int col,
             const int half_size,
             std::vector<float>& scratch)
{
  const int rows = static_cast<int>(input.rows());
  const int cols = static_cast<int>(input.cols());
  const int rs = std::max(0, row - half_size);
  const int re = std::min(rows - 1, row + half_size);
  const int cs = std::max(0, col - half_size);
  const int ce = std::min(cols - 1, col + half_size);

  float min_coeff = std::numeric_limits<float>::max();
  for (int c = cs; c <= ce; ++c) {
    int r = rs;
    while (r <= re) {
      const std::size_t vl = __riscv_vsetvl_e32m2(static_cast<std::size_t>(re - r + 1));
      const vfloat32m2_t values = __riscv_vle32_v_f32m2(&input(r, c), vl);
      __riscv_vse32_v_f32m2(scratch.data(), values, vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        const float value = scratch[lane];
        if (!std::isnan(value) && value < min_coeff)
          min_coeff = value;
      }
      r += static_cast<int>(vl);
    }
  }
  return min_coeff == std::numeric_limits<float>::max()
             ? std::numeric_limits<float>::quiet_NaN()
             : min_coeff;
}

inline float
windowMaxRVV(const Eigen::MatrixXf& input,
             const int row,
             const int col,
             const int half_size,
             std::vector<float>& scratch)
{
  const int rows = static_cast<int>(input.rows());
  const int cols = static_cast<int>(input.cols());
  const int rs = std::max(0, row - half_size);
  const int re = std::min(rows - 1, row + half_size);
  const int cs = std::max(0, col - half_size);
  const int ce = std::min(cols - 1, col + half_size);

  float max_coeff = -std::numeric_limits<float>::max();
  for (int c = cs; c <= ce; ++c) {
    int r = rs;
    while (r <= re) {
      const std::size_t vl = __riscv_vsetvl_e32m2(static_cast<std::size_t>(re - r + 1));
      const vfloat32m2_t values = __riscv_vle32_v_f32m2(&input(r, c), vl);
      __riscv_vse32_v_f32m2(scratch.data(), values, vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        const float value = scratch[lane];
        if (!std::isnan(value) && value > max_coeff)
          max_coeff = value;
      }
      r += static_cast<int>(vl);
    }
  }
  return max_coeff == -std::numeric_limits<float>::max()
             ? std::numeric_limits<float>::quiet_NaN()
             : max_coeff;
}

inline bool
erodeMinRVV(const Eigen::MatrixXf& input, const int half_size, Eigen::MatrixXf& out)
{
  if (input.size() == 0 || half_size < 0)
    return false;
  out.resize(input.rows(), input.cols());
  out.setConstant(std::numeric_limits<float>::quiet_NaN());
  std::vector<float> scratch(__riscv_vsetvlmax_e32m2());
  for (int row = 0; row < input.rows(); ++row)
    for (int col = 0; col < input.cols(); ++col)
      out(row, col) = windowMinRVV(input, row, col, half_size, scratch);
  return true;
}

inline bool
dilateMaxRVV(const Eigen::MatrixXf& input, const int half_size, Eigen::MatrixXf& out)
{
  if (input.size() == 0 || half_size < 0)
    return false;
  out.resize(input.rows(), input.cols());
  out.setConstant(std::numeric_limits<float>::quiet_NaN());
  std::vector<float> scratch(__riscv_vsetvlmax_e32m2());
  for (int row = 0; row < input.rows(); ++row)
    for (int col = 0; col < input.cols(); ++col)
      out(row, col) = windowMaxRVV(input, row, col, half_size, scratch);
  return true;
}

inline bool
morphologicalOpenRVV(const Eigen::MatrixXf& input, const int half_size, Eigen::MatrixXf& out)
{
  Eigen::MatrixXf eroded;
  if (!erodeMinRVV(input, half_size, eroded))
    return false;
  return dilateMaxRVV(eroded, half_size, out);
}

inline bool
thresholdGroundRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                   const pcl::Indices& ground,
                   const GridShape& shape,
                   const Eigen::MatrixXf& filtered,
                   const float height_threshold,
                   pcl::Indices& out)
{
  const std::size_t n = ground.size();
  if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
      cloud.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    return false;

  out.clear();
  out.reserve(n);
  const auto* base = reinterpret_cast<const std::uint8_t*>(cloud.data());
  const auto max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<int> rows(max_vl);
  std::vector<int> cols(max_vl);
  std::vector<float> zs(max_vl);
  std::vector<std::uint32_t> source(max_vl);

  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const vint32m2_t v_indices = __riscv_vle32_v_i32m2(ground.data() + i, vl);
    const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
    const vuint32m2_t offsets = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_indices_u, vl);
    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::indexed_load3_f32m2<pcl::PointXYZ,
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(base, offsets, vl, vx, vy, vz);

    const vfloat32m2_t shifted_x = __riscv_vfsub_vf_f32m2(vx, shape.global_min.x(), vl);
    const vfloat32m2_t shifted_y = __riscv_vfsub_vf_f32m2(vy, shape.global_min.y(), vl);
    const vint32m2_t v_cols = floorF32ToI32NoFrm(__riscv_vfdiv_vf_f32m2(shifted_x, shape.cell_size, vl), vl);
    const vint32m2_t v_rows = floorF32ToI32NoFrm(__riscv_vfdiv_vf_f32m2(shifted_y, shape.cell_size, vl), vl);

    __riscv_vse32_v_i32m2(rows.data(), v_rows, vl);
    __riscv_vse32_v_i32m2(cols.data(), v_cols, vl);
    __riscv_vse32_v_f32m2(zs.data(), vz, vl);
    __riscv_vse32_v_u32m2(source.data(), v_indices_u, vl);

    // filtered grid lookup remains scalar in this diagnostic. That preserves
    // the production order and isolates the first RVV question to coordinate
    // conversion, indexed point load, and threshold predicate cost.
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const int row = rows[lane];
      const int col = cols[lane];
      if (row < 0 || row >= filtered.rows() || col < 0 || col >= filtered.cols())
        continue;
      const float diff = zs[lane] - filtered(row, col);
      if (diff < height_threshold)
        out.push_back(static_cast<int>(source[lane]));
    }

    i += vl;
  }
  return true;
}

inline bool
progressiveFilterRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                     const GridShape& shape,
                     const std::vector<int>& half_sizes,
                     const std::vector<float>& height_thresholds,
                     ProgressiveFilterResult& result)
{
  if (half_sizes.size() != height_thresholds.size())
    return false;
  if (!computeGridZMinRVV(cloud, shape, result.grid))
    return false;

  result.ground = initialGroundStd(cloud);
  for (std::size_t i = 0; i < half_sizes.size(); ++i) {
    Eigen::MatrixXf filtered;
    if (!morphologicalOpenRVV(result.grid, half_sizes[i], filtered))
      return false;

    pcl::Indices next_ground;
    if (!thresholdGroundRVV(
            cloud, result.ground, shape, filtered, height_thresholds[i], next_ground)) {
      next_ground =
          thresholdGroundStd(cloud, result.ground, shape, filtered, height_thresholds[i]);
    }
    result.grid.swap(filtered);
    result.ground.swap(next_ground);
  }

  return true;
}

#endif // defined(__RVV10__)

} // namespace pcl_rvv_segmentation_apmf
