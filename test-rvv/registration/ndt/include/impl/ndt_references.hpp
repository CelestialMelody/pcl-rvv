/*
 * 本文件做什么：
 * 标量 reference（参考链路）复刻 ndt.hpp 中 updateDerivatives 的核心公式，
 * 但输入已经由 fixture 暂存成 SoA。它不调用 production protected helper，
 * 目的是把“数学核是否适合跨样本 RVV 批处理”和 NDT 对象状态、邻域搜索
 * 分开审查。
 */

#pragma once

#include "ndt_fixtures.hpp"

#include <cmath>

namespace pcl::registration::rvv_ndt_support {

inline int
jac_index(const int row, const int col)
{
  return row * kTransformDof + col;
}

inline int
hess_index(const int block_row, const int col)
{
  return block_row * kHessianCols + col;
}

inline int
mat3_index(const int row, const int col)
{
  return row * kDim3 + col;
}

inline bool
sample_weight(const DerivativeBatch& batch,
              const std::size_t sample,
              double& score_inc,
              double& scaled_weight)
{
  const double x0 = batch.x0[sample];
  const double x1 = batch.x1[sample];
  const double x2 = batch.x2[sample];
  const double cx0 = batch.c_inv[mat3_index(0, 0)][sample] * x0 +
                     batch.c_inv[mat3_index(0, 1)][sample] * x1 +
                     batch.c_inv[mat3_index(0, 2)][sample] * x2;
  const double cx1 = batch.c_inv[mat3_index(1, 0)][sample] * x0 +
                     batch.c_inv[mat3_index(1, 1)][sample] * x1 +
                     batch.c_inv[mat3_index(1, 2)][sample] * x2;
  const double cx2 = batch.c_inv[mat3_index(2, 0)][sample] * x0 +
                     batch.c_inv[mat3_index(2, 1)][sample] * x1 +
                     batch.c_inv[mat3_index(2, 2)][sample] * x2;
  const double x_cov_x = x0 * cx0 + x1 * cx1 + x2 * cx2;
  const double exp_value = std::exp(-batch.gauss_d2 * x_cov_x / 2.0);
  score_inc = -batch.gauss_d1 * exp_value;

  double e_x_cov_x = batch.gauss_d2 * exp_value;
  if (e_x_cov_x > 1.0 || e_x_cov_x < 0.0 || std::isnan(e_x_cov_x))
    return false;
  scaled_weight = e_x_cov_x * batch.gauss_d1;
  return true;
}

inline void
sample_cov_jacobian(const DerivativeBatch& batch,
                    const std::size_t sample,
                    const int col,
                    double& cov0,
                    double& cov1,
                    double& cov2)
{
  const double j0 = batch.jacobian[jac_index(0, col)][sample];
  const double j1 = batch.jacobian[jac_index(1, col)][sample];
  const double j2 = batch.jacobian[jac_index(2, col)][sample];
  cov0 = batch.c_inv[mat3_index(0, 0)][sample] * j0 +
         batch.c_inv[mat3_index(0, 1)][sample] * j1 +
         batch.c_inv[mat3_index(0, 2)][sample] * j2;
  cov1 = batch.c_inv[mat3_index(1, 0)][sample] * j0 +
         batch.c_inv[mat3_index(1, 1)][sample] * j1 +
         batch.c_inv[mat3_index(1, 2)][sample] * j2;
  cov2 = batch.c_inv[mat3_index(2, 0)][sample] * j0 +
         batch.c_inv[mat3_index(2, 1)][sample] * j1 +
         batch.c_inv[mat3_index(2, 2)][sample] * j2;
}

inline DerivativeResult
derivative_accumulate_std(const DerivativeBatch& batch)
{
  DerivativeResult result;
  result.samples = batch.size();
  for (std::size_t sample = 0; sample < batch.size(); ++sample) {
    double score_inc = 0.0;
    double scaled_weight = 0.0;
    if (!sample_weight(batch, sample, score_inc, scaled_weight))
      continue;
    result.score += score_inc;

    std::array<double, kTransformDof> xdot_cov_j{};
    std::array<std::array<double, kDim3>, kTransformDof> cov_j{};
    for (int i = 0; i < kTransformDof; ++i) {
      sample_cov_jacobian(batch, sample, i, cov_j[i][0], cov_j[i][1], cov_j[i][2]);
      xdot_cov_j[i] = batch.x0[sample] * cov_j[i][0] +
                      batch.x1[sample] * cov_j[i][1] +
                      batch.x2[sample] * cov_j[i][2];
    }

    for (int i = 0; i < kTransformDof; ++i) {
      result.gradient[i] += xdot_cov_j[i] * scaled_weight;

      if (batch.compute_hessian) {
        for (int j = 0; j < kTransformDof; ++j) {
          const double h0 = batch.point_hessian[hess_index(3 * i + 0, j)][sample];
          const double h1 = batch.point_hessian[hess_index(3 * i + 1, j)][sample];
          const double h2 = batch.point_hessian[hess_index(3 * i + 2, j)][sample];
          const double xdot_c_hess =
              batch.x0[sample] *
                  (batch.c_inv[mat3_index(0, 0)][sample] * h0 +
                   batch.c_inv[mat3_index(0, 1)][sample] * h1 +
                   batch.c_inv[mat3_index(0, 2)][sample] * h2) +
              batch.x1[sample] *
                  (batch.c_inv[mat3_index(1, 0)][sample] * h0 +
                   batch.c_inv[mat3_index(1, 1)][sample] * h1 +
                   batch.c_inv[mat3_index(1, 2)][sample] * h2) +
              batch.x2[sample] *
                  (batch.c_inv[mat3_index(2, 0)][sample] * h0 +
                   batch.c_inv[mat3_index(2, 1)][sample] * h1 +
                   batch.c_inv[mat3_index(2, 2)][sample] * h2);
          const double jacj_dot_covi =
              batch.jacobian[jac_index(0, j)][sample] * cov_j[i][0] +
              batch.jacobian[jac_index(1, j)][sample] * cov_j[i][1] +
              batch.jacobian[jac_index(2, j)][sample] * cov_j[i][2];
          result.hessian[i * kTransformDof + j] +=
              scaled_weight *
              (-batch.gauss_d2 * xdot_cov_j[i] * xdot_cov_j[j] + xdot_c_hess +
               jacj_dot_covi);
        }
      }
    }
  }
  return result;
}

} // namespace pcl::registration::rvv_ndt_support
