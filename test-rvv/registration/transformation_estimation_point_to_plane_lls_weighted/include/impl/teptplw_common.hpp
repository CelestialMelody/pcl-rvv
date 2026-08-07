/*
 * 本文件做什么：
 * weighted point-to-plane LLS RVV diagnostic 的公共类型、标量行公式和求解边界。
 * 它只服务 test-rvv，不修改 production。
 */

#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag {

using Matrix4f = Eigen::Matrix4f;
using Matrix6d = Eigen::Matrix<double, 6, 6>;
using Vector6d = Eigen::Matrix<double, 6, 1>;

struct AccumulationStats {
  std::size_t input_points = 0;
  std::size_t accepted_points = 0;
  bool used_rvv = false;
};

struct NormalEquation {
  Matrix6d ata = Matrix6d::Zero();
  Vector6d atb = Vector6d::Zero();
  std::size_t accepted_points = 0;
};

template <typename PointSource, typename PointTarget>
inline bool
finite_point_and_normal(const PointSource& source, const PointTarget& target)
{
  return std::isfinite(source.x) && std::isfinite(source.y) &&
         std::isfinite(source.z) && std::isfinite(target.x) &&
         std::isfinite(target.y) && std::isfinite(target.z) &&
         std::isfinite(target.normal_x) && std::isfinite(target.normal_y) &&
         std::isfinite(target.normal_z);
}

// 这个函数是 weighted helper 的权威标量行构造：先把 weight 乘到 target normal，
// 再用加权后的 nx/ny/nz 生成 a/b/c/d 和 normal-equation 贡献。
inline void
accumulate_weighted_row(const float sx,
                        const float sy,
                        const float sz,
                        const float dx,
                        const float dy,
                        const float dz,
                        const float normal_x,
                        const float normal_y,
                        const float normal_z,
                        const float weight,
                        NormalEquation& eq)
{
  const float nx = normal_x * weight;
  const float ny = normal_y * weight;
  const float nz = normal_z * weight;

  const double a = nz * sy - ny * sz;
  const double b = nx * sz - nz * sx;
  const double c = ny * sx - nx * sy;

  eq.ata.coeffRef(0) += a * a;
  eq.ata.coeffRef(1) += a * b;
  eq.ata.coeffRef(2) += a * c;
  eq.ata.coeffRef(3) += a * nx;
  eq.ata.coeffRef(4) += a * ny;
  eq.ata.coeffRef(5) += a * nz;
  eq.ata.coeffRef(7) += b * b;
  eq.ata.coeffRef(8) += b * c;
  eq.ata.coeffRef(9) += b * nx;
  eq.ata.coeffRef(10) += b * ny;
  eq.ata.coeffRef(11) += b * nz;
  eq.ata.coeffRef(14) += c * c;
  eq.ata.coeffRef(15) += c * nx;
  eq.ata.coeffRef(16) += c * ny;
  eq.ata.coeffRef(17) += c * nz;
  eq.ata.coeffRef(21) += nx * nx;
  eq.ata.coeffRef(22) += nx * ny;
  eq.ata.coeffRef(23) += nx * nz;
  eq.ata.coeffRef(28) += ny * ny;
  eq.ata.coeffRef(29) += ny * nz;
  eq.ata.coeffRef(35) += nz * nz;

  const double d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz;
  eq.atb.coeffRef(0) += a * d;
  eq.atb.coeffRef(1) += b * d;
  eq.atb.coeffRef(2) += c * d;
  eq.atb.coeffRef(3) += nx * d;
  eq.atb.coeffRef(4) += ny * d;
  eq.atb.coeffRef(5) += nz * d;
  ++eq.accepted_points;
}

inline void
complete_symmetric_upper(NormalEquation& eq)
{
  eq.ata.coeffRef(6) = eq.ata.coeff(1);
  eq.ata.coeffRef(12) = eq.ata.coeff(2);
  eq.ata.coeffRef(13) = eq.ata.coeff(8);
  eq.ata.coeffRef(18) = eq.ata.coeff(3);
  eq.ata.coeffRef(19) = eq.ata.coeff(9);
  eq.ata.coeffRef(20) = eq.ata.coeff(15);
  eq.ata.coeffRef(24) = eq.ata.coeff(4);
  eq.ata.coeffRef(25) = eq.ata.coeff(10);
  eq.ata.coeffRef(26) = eq.ata.coeff(16);
  eq.ata.coeffRef(27) = eq.ata.coeff(22);
  eq.ata.coeffRef(30) = eq.ata.coeff(5);
  eq.ata.coeffRef(31) = eq.ata.coeff(11);
  eq.ata.coeffRef(32) = eq.ata.coeff(17);
  eq.ata.coeffRef(33) = eq.ata.coeff(23);
  eq.ata.coeffRef(34) = eq.ata.coeff(29);
}

inline Matrix4f
construct_transformation_matrix(const Vector6d& x)
{
  const double alpha = x(0);
  const double beta = x(1);
  const double gamma = x(2);
  Matrix4f transformation = Matrix4f::Zero();
  transformation(0, 0) = static_cast<float>(std::cos(gamma) * std::cos(beta));
  transformation(0, 1) = static_cast<float>(
      -std::sin(gamma) * std::cos(alpha) +
      std::cos(gamma) * std::sin(beta) * std::sin(alpha));
  transformation(0, 2) = static_cast<float>(
      std::sin(gamma) * std::sin(alpha) +
      std::cos(gamma) * std::sin(beta) * std::cos(alpha));
  transformation(1, 0) = static_cast<float>(std::sin(gamma) * std::cos(beta));
  transformation(1, 1) = static_cast<float>(
      std::cos(gamma) * std::cos(alpha) +
      std::sin(gamma) * std::sin(beta) * std::sin(alpha));
  transformation(1, 2) = static_cast<float>(
      -std::cos(gamma) * std::sin(alpha) +
      std::sin(gamma) * std::sin(beta) * std::cos(alpha));
  transformation(2, 0) = static_cast<float>(-std::sin(beta));
  transformation(2, 1) = static_cast<float>(std::cos(beta) * std::sin(alpha));
  transformation(2, 2) = static_cast<float>(std::cos(beta) * std::cos(alpha));
  transformation(0, 3) = static_cast<float>(x(3));
  transformation(1, 3) = static_cast<float>(x(4));
  transformation(2, 3) = static_cast<float>(x(5));
  transformation(3, 3) = 1.0f;
  return transformation;
}

inline Matrix4f
solve_normal_equation(NormalEquation eq)
{
  complete_symmetric_upper(eq);
  const Vector6d x = static_cast<Vector6d>(eq.ata.inverse() * eq.atb);
  return construct_transformation_matrix(x);
}

inline double
matrix_checksum(const Matrix4f& matrix)
{
  double checksum = 0.0;
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      checksum += static_cast<double>(matrix(row, col)) * (1.0 + row * 4 + col);
  return checksum;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_diag
