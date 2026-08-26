/*
 * 本文件做什么：
 * 这里保存 ROPS（Rotational Projection Statistics，旋转投影统计）
 * component ablation（组件消融）的 test-only helper。Phase 000 只覆盖
 * central moments（中心矩）计算：Std helper（标量参考链路）复刻 production
 * 私有 helper 公式，RVV helper（RISC-V Vector，可变长度向量候选）在
 * `__RVV10__` 下用 VLA（vector-length agnostic，可变向量长度无关）分块规约
 * mean 和四个低阶矩。
 *
 * 证据边界：
 * 这些 helper 不修改 production（生产源码），不证明完整 RoPS descriptor
 * （描述子）路径已经适合 RVV。entropy（熵）仍使用标量 `std::log`，因为当前
 * phase 的目标是先隔离矩阵 moments 的可测边界。
 */

#pragma once

#include <Eigen/Core>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::rops {

inline void
rotateCloudStd(const pcl::PointXYZ& axis,
               const float angle,
               const pcl::PointCloud<pcl::PointXYZ>& cloud,
               pcl::PointCloud<pcl::PointXYZ>& rotated_cloud,
               Eigen::Vector3f& min,
               Eigen::Vector3f& max)
{
  Eigen::Matrix3f rotation_matrix;
  const float x = axis.x;
  const float y = axis.y;
  const float z = axis.z;
  const float rad = M_PI / 180.0f;
  const float cosine = std::cos(angle * rad);
  const float sine = std::sin(angle * rad);
  rotation_matrix << cosine + (1 - cosine) * x * x,
      (1 - cosine) * x * y - sine * z,
      (1 - cosine) * x * z + sine * y,
      (1 - cosine) * y * x + sine * z,
      cosine + (1 - cosine) * y * y,
      (1 - cosine) * y * z - sine * x,
      (1 - cosine) * z * x - sine * y,
      (1 - cosine) * z * y + sine * x,
      cosine + (1 - cosine) * z * z;

  const auto number_of_points = cloud.size();

  rotated_cloud.header = cloud.header;
  rotated_cloud.width = number_of_points;
  rotated_cloud.height = 1;
  rotated_cloud.is_dense = cloud.is_dense;
  rotated_cloud.clear();
  rotated_cloud.reserve(number_of_points);

  min(0) = std::numeric_limits<float>::max();
  min(1) = std::numeric_limits<float>::max();
  min(2) = std::numeric_limits<float>::max();
  max(0) = -std::numeric_limits<float>::max();
  max(1) = -std::numeric_limits<float>::max();
  max(2) = -std::numeric_limits<float>::max();

  for (const auto& pt : cloud.points) {
    Eigen::Vector3f point(pt.x, pt.y, pt.z);
    point = rotation_matrix * point;

    pcl::PointXYZ rotated_point;
    rotated_point.x = point(0);
    rotated_point.y = point(1);
    rotated_point.z = point(2);
    rotated_cloud.emplace_back(rotated_point);

    for (int i = 0; i < 3; ++i) {
      min(i) = std::min(min(i), point(i));
      max(i) = std::max(max(i), point(i));
    }
  }
}

inline void
getDistributionMatrixStd(const unsigned int projection,
                         const Eigen::Vector3f& min,
                         const Eigen::Vector3f& max,
                         const pcl::PointCloud<pcl::PointXYZ>& cloud,
                         Eigen::MatrixXf& matrix)
{
  matrix.setZero();
  const unsigned int coord[3][2] = {{0, 1}, {0, 2}, {1, 2}};
  const auto bins = static_cast<unsigned int>(matrix.rows());
  const float u_bin_length = (max(coord[projection][0]) - min(coord[projection][0])) / bins;
  const float v_bin_length = (max(coord[projection][1]) - min(coord[projection][1])) / bins;

  for (const auto& pt : cloud.points) {
    const float point[3] = {pt.x, pt.y, pt.z};
    const float u_length = point[coord[projection][0]] - min[coord[projection][0]];
    const float v_length = point[coord[projection][1]] - min[coord[projection][1]];

    auto row = static_cast<unsigned int>(u_length / u_bin_length);
    if (row == bins)
      --row;
    auto col = static_cast<unsigned int>(v_length / v_bin_length);
    if (col == bins)
      --col;
    matrix(row, col) += 1.0f;
  }

  matrix /= std::max<float>(1.0f, static_cast<float>(cloud.size()));
}

inline void
computeCentralMomentsStd(const Eigen::MatrixXf& matrix, std::vector<float>& moments)
{
  float mean_i = 0.0f;
  float mean_j = 0.0f;
  const auto rows = static_cast<unsigned int>(matrix.rows());
  const auto cols = static_cast<unsigned int>(matrix.cols());

  for (unsigned int i = 0; i < rows; ++i)
    for (unsigned int j = 0; j < cols; ++j) {
      const float m = matrix(i, j);
      mean_i += static_cast<float>(i + 1) * m;
      mean_j += static_cast<float>(j + 1) * m;
    }

  float entropy = 0.0f;
  moments.assign(5, 0.0f);
  for (unsigned int i = 0; i < rows; ++i) {
    const float i_factor = static_cast<float>(i + 1) - mean_i;
    const float i_factor2 = i_factor * i_factor;
    for (unsigned int j = 0; j < cols; ++j) {
      const float j_factor = static_cast<float>(j + 1) - mean_j;
      const float j_factor2 = j_factor * j_factor;
      const float m = matrix(i, j);
      if (m > 0.0f)
        entropy -= m * std::log(m);
      moments[0] += i_factor * j_factor * m;
      moments[1] += i_factor2 * j_factor * m;
      moments[2] += i_factor * j_factor2 * m;
      moments[3] += i_factor2 * j_factor2 * m;
    }
  }
  moments[4] = entropy;
}

#ifdef __RVV10__
inline float
reduceSumF32m2(const vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t init = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  const vfloat32m1_t reduced = __riscv_vfredusum_vs_f32m2_f32m1(values, init, vl);
  return __riscv_vfmv_f_s_f32m1_f32(reduced);
}
#endif

inline void
rotateCloudRVV(const pcl::PointXYZ& axis,
               const float angle,
               const pcl::PointCloud<pcl::PointXYZ>& cloud,
               pcl::PointCloud<pcl::PointXYZ>& rotated_cloud,
               Eigen::Vector3f& min,
               Eigen::Vector3f& max)
{
#ifndef __RVV10__
  rotateCloudStd(axis, angle, cloud, rotated_cloud, min, max);
#else
  const float x = axis.x;
  const float y = axis.y;
  const float z = axis.z;
  const float rad = M_PI / 180.0f;
  const float cosine = std::cos(angle * rad);
  const float sine = std::sin(angle * rad);
  const float one_minus_cosine = 1.0f - cosine;
  const float r00 = cosine + one_minus_cosine * x * x;
  const float r01 = one_minus_cosine * x * y - sine * z;
  const float r02 = one_minus_cosine * x * z + sine * y;
  const float r10 = one_minus_cosine * y * x + sine * z;
  const float r11 = cosine + one_minus_cosine * y * y;
  const float r12 = one_minus_cosine * y * z - sine * x;
  const float r20 = one_minus_cosine * z * x - sine * y;
  const float r21 = one_minus_cosine * z * y + sine * x;
  const float r22 = cosine + one_minus_cosine * z * z;
  const std::size_t number_of_points = cloud.size();

  rotated_cloud.header = cloud.header;
  rotated_cloud.width = static_cast<std::uint32_t>(number_of_points);
  rotated_cloud.height = 1;
  rotated_cloud.clear();
  rotated_cloud.resize(number_of_points);

  const std::size_t vlmax = __riscv_vsetvl_e32m2(static_cast<std::size_t>(-1));
  const float init_min = std::numeric_limits<float>::max();
  const float init_max = -std::numeric_limits<float>::max();
  vfloat32m2_t v_min_x = __riscv_vfmv_v_f_f32m2(init_min, vlmax);
  vfloat32m2_t v_min_y = __riscv_vfmv_v_f_f32m2(init_min, vlmax);
  vfloat32m2_t v_min_z = __riscv_vfmv_v_f_f32m2(init_min, vlmax);
  vfloat32m2_t v_max_x = __riscv_vfmv_v_f_f32m2(init_max, vlmax);
  vfloat32m2_t v_max_y = __riscv_vfmv_v_f_f32m2(init_max, vlmax);
  vfloat32m2_t v_max_z = __riscv_vfmv_v_f_f32m2(init_max, vlmax);

  for (std::size_t i = 0; i < number_of_points;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(number_of_points - i);
    const auto* input = cloud.points.data() + i;
    auto* output = rotated_cloud.points.data() + i;
    const auto point_stride = static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ));
    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(&input->x, point_stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(&input->y, point_stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(&input->z, point_stride, vl);

    vfloat32m2_t rx = __riscv_vfmul_vf_f32m2(vx, r00, vl);
    rx = __riscv_vfmacc_vf_f32m2(rx, r01, vy, vl);
    rx = __riscv_vfmacc_vf_f32m2(rx, r02, vz, vl);
    vfloat32m2_t ry = __riscv_vfmul_vf_f32m2(vx, r10, vl);
    ry = __riscv_vfmacc_vf_f32m2(ry, r11, vy, vl);
    ry = __riscv_vfmacc_vf_f32m2(ry, r12, vz, vl);
    vfloat32m2_t rz = __riscv_vfmul_vf_f32m2(vx, r20, vl);
    rz = __riscv_vfmacc_vf_f32m2(rz, r21, vy, vl);
    rz = __riscv_vfmacc_vf_f32m2(rz, r22, vz, vl);

    __riscv_vsse32_v_f32m2(&output->x, point_stride, rx, vl);
    __riscv_vsse32_v_f32m2(&output->y, point_stride, ry, vl);
    __riscv_vsse32_v_f32m2(&output->z, point_stride, rz, vl);

    v_min_x = __riscv_vfmin_vv_f32m2_tu(v_min_x, v_min_x, rx, vl);
    v_min_y = __riscv_vfmin_vv_f32m2_tu(v_min_y, v_min_y, ry, vl);
    v_min_z = __riscv_vfmin_vv_f32m2_tu(v_min_z, v_min_z, rz, vl);
    v_max_x = __riscv_vfmax_vv_f32m2_tu(v_max_x, v_max_x, rx, vl);
    v_max_y = __riscv_vfmax_vv_f32m2_tu(v_max_y, v_max_y, ry, vl);
    v_max_z = __riscv_vfmax_vv_f32m2_tu(v_max_z, v_max_z, rz, vl);

    i += vl;
  }

  const vfloat32m1_t min_seed = __riscv_vfmv_s_f_f32m1(init_min, 1);
  const vfloat32m1_t max_seed = __riscv_vfmv_s_f_f32m1(init_max, 1);
  min(0) = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1(v_min_x, min_seed, vlmax));
  min(1) = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1(v_min_y, min_seed, vlmax));
  min(2) = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmin_vs_f32m2_f32m1(v_min_z, min_seed, vlmax));
  max(0) = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1(v_max_x, max_seed, vlmax));
  max(1) = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1(v_max_y, max_seed, vlmax));
  max(2) = __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredmax_vs_f32m2_f32m1(v_max_z, max_seed, vlmax));
#endif
}

inline void
computeCentralMomentsRVV(const Eigen::MatrixXf& matrix, std::vector<float>& moments)
{
#ifndef __RVV10__
  computeCentralMomentsStd(matrix, moments);
#else
  const auto rows = static_cast<unsigned int>(matrix.rows());
  const auto cols = static_cast<unsigned int>(matrix.cols());
  const std::size_t count = static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
  std::vector<float> row_coord(count);
  std::vector<float> col_coord(count);
  std::vector<float> mass(count);

  std::size_t offset = 0;
  for (unsigned int i = 0; i < rows; ++i) {
    for (unsigned int j = 0; j < cols; ++j) {
      row_coord[offset] = static_cast<float>(i + 1);
      col_coord[offset] = static_cast<float>(j + 1);
      mass[offset] = matrix(i, j);
      ++offset;
    }
  }

  float mean_i = 0.0f;
  float mean_j = 0.0f;
  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const vfloat32m2_t v_row = __riscv_vle32_v_f32m2(row_coord.data() + i, vl);
    const vfloat32m2_t v_col = __riscv_vle32_v_f32m2(col_coord.data() + i, vl);
    const vfloat32m2_t v_mass = __riscv_vle32_v_f32m2(mass.data() + i, vl);
    mean_i += reduceSumF32m2(__riscv_vfmul_vv_f32m2(v_row, v_mass, vl), vl);
    mean_j += reduceSumF32m2(__riscv_vfmul_vv_f32m2(v_col, v_mass, vl), vl);
    i += vl;
  }

  float moment_11 = 0.0f;
  float moment_21 = 0.0f;
  float moment_12 = 0.0f;
  float moment_22 = 0.0f;
  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const vfloat32m2_t v_row = __riscv_vle32_v_f32m2(row_coord.data() + i, vl);
    const vfloat32m2_t v_col = __riscv_vle32_v_f32m2(col_coord.data() + i, vl);
    const vfloat32m2_t v_mass = __riscv_vle32_v_f32m2(mass.data() + i, vl);
    const vfloat32m2_t v_if = __riscv_vfsub_vf_f32m2(v_row, mean_i, vl);
    const vfloat32m2_t v_jf = __riscv_vfsub_vf_f32m2(v_col, mean_j, vl);
    const vfloat32m2_t v_if2 = __riscv_vfmul_vv_f32m2(v_if, v_if, vl);
    const vfloat32m2_t v_jf2 = __riscv_vfmul_vv_f32m2(v_jf, v_jf, vl);
    const vfloat32m2_t v_ij = __riscv_vfmul_vv_f32m2(v_if, v_jf, vl);
    moment_11 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(v_ij, v_mass, vl), vl);
    moment_21 += reduceSumF32m2(
        __riscv_vfmul_vv_f32m2(__riscv_vfmul_vv_f32m2(v_if2, v_jf, vl), v_mass, vl), vl);
    moment_12 += reduceSumF32m2(
        __riscv_vfmul_vv_f32m2(__riscv_vfmul_vv_f32m2(v_if, v_jf2, vl), v_mass, vl), vl);
    moment_22 += reduceSumF32m2(__riscv_vfmul_vv_f32m2(
                                    __riscv_vfmul_vv_f32m2(v_if2, v_jf2, vl), v_mass, vl),
                                vl);
    i += vl;
  }

  float entropy = 0.0f;
  for (float m : mass)
    if (m > 0.0f)
      entropy -= m * std::log(m);

  moments = {moment_11, moment_21, moment_12, moment_22, entropy};
#endif
}

inline void
getDistributionMatrixRVV(const unsigned int projection,
                         const Eigen::Vector3f& min,
                         const Eigen::Vector3f& max,
                         const pcl::PointCloud<pcl::PointXYZ>& cloud,
                         Eigen::MatrixXf& matrix)
{
#ifndef __RVV10__
  getDistributionMatrixStd(projection, min, max, cloud, matrix);
#else
  matrix.setZero();
  const unsigned int coord[3][2] = {{0, 1}, {0, 2}, {1, 2}};
  const auto bins = static_cast<unsigned int>(matrix.rows());
  const float u_min = min[coord[projection][0]];
  const float v_min = min[coord[projection][1]];
  const float inv_u_bin_length = static_cast<float>(bins) / (max(coord[projection][0]) - u_min);
  const float inv_v_bin_length = static_cast<float>(bins) / (max(coord[projection][1]) - v_min);
  const std::size_t count = cloud.size();
  std::vector<std::uint32_t> rows(count);
  std::vector<std::uint32_t> cols(count);

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    vfloat32m2_t v_u;
    vfloat32m2_t v_v;
    if (projection == 0) {
      v_u = __riscv_vlse32_v_f32m2(&cloud.points[i].x, sizeof(pcl::PointXYZ), vl);
      v_v = __riscv_vlse32_v_f32m2(&cloud.points[i].y, sizeof(pcl::PointXYZ), vl);
    }
    else if (projection == 1) {
      v_u = __riscv_vlse32_v_f32m2(&cloud.points[i].x, sizeof(pcl::PointXYZ), vl);
      v_v = __riscv_vlse32_v_f32m2(&cloud.points[i].z, sizeof(pcl::PointXYZ), vl);
    }
    else {
      v_u = __riscv_vlse32_v_f32m2(&cloud.points[i].y, sizeof(pcl::PointXYZ), vl);
      v_v = __riscv_vlse32_v_f32m2(&cloud.points[i].z, sizeof(pcl::PointXYZ), vl);
    }

    v_u = __riscv_vfmul_vf_f32m2(__riscv_vfsub_vf_f32m2(v_u, u_min, vl), inv_u_bin_length, vl);
    v_v = __riscv_vfmul_vf_f32m2(__riscv_vfsub_vf_f32m2(v_v, v_min, vl), inv_v_bin_length, vl);
    vuint32m2_t v_row = __riscv_vfcvt_rtz_xu_f_v_u32m2(v_u, vl);
    vuint32m2_t v_col = __riscv_vfcvt_rtz_xu_f_v_u32m2(v_v, vl);
    const vuint32m2_t v_last_bin = __riscv_vmv_v_x_u32m2(bins - 1, vl);
    const vuint32m2_t v_bins = __riscv_vmv_v_x_u32m2(bins, vl);
    v_row = __riscv_vminu_vv_u32m2(v_row, v_last_bin, vl);
    v_col = __riscv_vminu_vv_u32m2(v_col, v_last_bin, vl);
    v_row = __riscv_vmerge_vvm_u32m2(v_row, v_last_bin, __riscv_vmsgeu_vv_u32m2_b16(v_row, v_bins, vl), vl);
    v_col = __riscv_vmerge_vvm_u32m2(v_col, v_last_bin, __riscv_vmsgeu_vv_u32m2_b16(v_col, v_bins, vl), vl);
    __riscv_vse32_v_u32m2(rows.data() + i, v_row, vl);
    __riscv_vse32_v_u32m2(cols.data() + i, v_col, vl);
    i += vl;
  }

  for (std::size_t i = 0; i < count; ++i)
    matrix(rows[i], cols[i]) += 1.0f;
  matrix /= std::max<float>(1.0f, static_cast<float>(count));
#endif
}

inline void
rotateCloudAndDistributionMatricesRVV(const pcl::PointXYZ& axis,
                                      const float angle,
                                      const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                      const unsigned int bins,
                                      pcl::PointCloud<pcl::PointXYZ>& rotated_cloud,
                                      Eigen::Vector3f& min,
                                      Eigen::Vector3f& max,
                                      std::array<Eigen::MatrixXf, 3>& matrices)
{
  rotateCloudRVV(axis, angle, cloud, rotated_cloud, min, max);
  for (unsigned int projection = 0; projection < matrices.size(); ++projection) {
    matrices[projection].resize(bins, bins);
    getDistributionMatrixRVV(projection, min, max, rotated_cloud, matrices[projection]);
  }
}

} // namespace pcl::features::rvv_test::rops
