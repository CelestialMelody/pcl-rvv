/*
 * 本文件负责 gicp topic 的标量 reference（参考链路）。reference 复刻
 * gicp.hpp 中两个 KNN / correspondence 之后的局部组件：
 * residual / Mahalanobis functor 累加，以及 covariance post-KNN 协方差构造。
 *
 * 证据边界：
 * reference 不运行 GICP public entry（公开入口），不运行 KdTree，也不运行
 * Eigen SVD / Newton solver。它只给 test-only candidate 提供同构对拍基线。
 */

#pragma once

#include "gicp_fixtures.hpp"

#include <cstddef>

namespace pcl::registration::rvv_gicp_support {

inline double
apply3x4(const std::array<double, 12>& t,
         const int row,
         const double x,
         const double y,
         const double z)
{
  const std::size_t r = static_cast<std::size_t>(row) * 4u;
  return t[r] * x + t[r + 1u] * y + t[r + 2u] * z + t[r + 3u];
}

inline ResidualResult
residual_accumulate_std(const ResidualInput& input)
{
  ResidualResult result;
  const std::size_t n = input.rows;
  if (n == 0)
    return result;

  double f = 0.0;
  double gx = 0.0;
  double gy = 0.0;
  double gz = 0.0;
  double dcost[9] = {};

  for (std::size_t i = 0; i < n; ++i) {
    const double sx = input.src_x[i];
    const double sy = input.src_y[i];
    const double sz = input.src_z[i];

    const double tx = apply3x4(input.transform, 0, sx, sy, sz);
    const double ty = apply3x4(input.transform, 1, sx, sy, sz);
    const double tz = apply3x4(input.transform, 2, sx, sy, sz);
    const double dx = tx - input.tgt_x[i];
    const double dy = ty - input.tgt_y[i];
    const double dz = tz - input.tgt_z[i];

    const double mdx = input.m00[i] * dx + input.m01[i] * dy + input.m02[i] * dz;
    const double mdy = input.m10[i] * dx + input.m11[i] * dy + input.m12[i] * dz;
    const double mdz = input.m20[i] * dx + input.m21[i] * dy + input.m22[i] * dz;

    f += dx * mdx + dy * mdy + dz * mdz;
    gx += mdx;
    gy += mdy;
    gz += mdz;

    const double bx = apply3x4(input.base_transform, 0, sx, sy, sz);
    const double by = apply3x4(input.base_transform, 1, sx, sy, sz);
    const double bz = apply3x4(input.base_transform, 2, sx, sy, sz);
    dcost[0] += bx * mdx;
    dcost[1] += bx * mdy;
    dcost[2] += bx * mdz;
    dcost[3] += by * mdx;
    dcost[4] += by * mdy;
    dcost[5] += by * mdz;
    dcost[6] += bz * mdx;
    dcost[7] += bz * mdy;
    dcost[8] += bz * mdz;
  }

  const double inv_n = 1.0 / static_cast<double>(n);
  result.f = f * inv_n;
  result.translation_gradient = {2.0 * gx * inv_n, 2.0 * gy * inv_n, 2.0 * gz * inv_n};
  for (std::size_t i = 0; i < 9u; ++i)
    result.dcost_drt[i] = 2.0 * dcost[i] * inv_n;
  return result;
}

inline ResidualResult
indexed_residual_accumulate_std(const IndexedResidualInput& input)
{
  ResidualResult result;
  const std::size_t n = input.rows;
  if (n == 0)
    return result;

  double f = 0.0;
  double gx = 0.0;
  double gy = 0.0;
  double gz = 0.0;
  double dcost[9] = {};

  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t src_idx = static_cast<std::size_t>(input.src_idx[i]);
    const std::size_t tgt_idx = static_cast<std::size_t>(input.tgt_idx[i]);
    const double sx = input.src_x[src_idx];
    const double sy = input.src_y[src_idx];
    const double sz = input.src_z[src_idx];

    const double tx = apply3x4(input.transform, 0, sx, sy, sz);
    const double ty = apply3x4(input.transform, 1, sx, sy, sz);
    const double tz = apply3x4(input.transform, 2, sx, sy, sz);
    const double dx = tx - input.tgt_x[tgt_idx];
    const double dy = ty - input.tgt_y[tgt_idx];
    const double dz = tz - input.tgt_z[tgt_idx];

    const double mdx =
        input.m00[src_idx] * dx + input.m01[src_idx] * dy + input.m02[src_idx] * dz;
    const double mdy =
        input.m10[src_idx] * dx + input.m11[src_idx] * dy + input.m12[src_idx] * dz;
    const double mdz =
        input.m20[src_idx] * dx + input.m21[src_idx] * dy + input.m22[src_idx] * dz;

    f += dx * mdx + dy * mdy + dz * mdz;
    gx += mdx;
    gy += mdy;
    gz += mdz;

    const double bx = apply3x4(input.base_transform, 0, sx, sy, sz);
    const double by = apply3x4(input.base_transform, 1, sx, sy, sz);
    const double bz = apply3x4(input.base_transform, 2, sx, sy, sz);
    dcost[0] += bx * mdx;
    dcost[1] += bx * mdy;
    dcost[2] += bx * mdz;
    dcost[3] += by * mdx;
    dcost[4] += by * mdy;
    dcost[5] += by * mdz;
    dcost[6] += bz * mdx;
    dcost[7] += bz * mdy;
    dcost[8] += bz * mdz;
  }

  const double inv_n = 1.0 / static_cast<double>(n);
  result.f = f * inv_n;
  result.translation_gradient = {2.0 * gx * inv_n, 2.0 * gy * inv_n, 2.0 * gz * inv_n};
  for (std::size_t i = 0; i < 9u; ++i)
    result.dcost_drt[i] = 2.0 * dcost[i] * inv_n;
  return result;
}

inline HessianLoopResult
hessian_loop_accumulate_std(const ResidualInput& input)
{
  HessianLoopResult result;
  const std::size_t n = input.rows;
  if (n == 0)
    return result;

  double gx = 0.0, gy = 0.0, gz = 0.0;
  double htt[9] = {};
  double dcost[9] = {};
  double dcost_b[27] = {};
  double hrot[54] = {};

  for (std::size_t i = 0; i < n; ++i) {
    const double sx = input.src_x[i];
    const double sy = input.src_y[i];
    const double sz = input.src_z[i];
    const double tx = apply3x4(input.transform, 0, sx, sy, sz);
    const double ty = apply3x4(input.transform, 1, sx, sy, sz);
    const double tz = apply3x4(input.transform, 2, sx, sy, sz);
    const double dx = tx - input.tgt_x[i];
    const double dy = ty - input.tgt_y[i];
    const double dz = tz - input.tgt_z[i];

    const double m[9] = {input.m00[i],
                         input.m01[i],
                         input.m02[i],
                         input.m10[i],
                         input.m11[i],
                         input.m12[i],
                         input.m20[i],
                         input.m21[i],
                         input.m22[i]};
    const double mdx = m[0] * dx + m[1] * dy + m[2] * dz;
    const double mdy = m[3] * dx + m[4] * dy + m[5] * dz;
    const double mdz = m[6] * dx + m[7] * dy + m[8] * dz;

    gx += mdx;
    gy += mdy;
    gz += mdz;
    for (std::size_t k = 0; k < 9u; ++k)
      htt[k] += m[k];

    const double bx = apply3x4(input.base_transform, 0, sx, sy, sz);
    const double by = apply3x4(input.base_transform, 1, sx, sy, sz);
    const double bz = apply3x4(input.base_transform, 2, sx, sy, sz);
    const double base[3] = {bx, by, bz};
    const double md[3] = {mdx, mdy, mdz};
    for (std::size_t row = 0; row < 3u; ++row)
      for (std::size_t col = 0; col < 3u; ++col)
        dcost[row * 3u + col] += base[row] * md[col];

    for (std::size_t axis = 0; axis < 3u; ++axis)
      for (std::size_t k = 0; k < 9u; ++k)
        dcost_b[axis * 9u + k] += base[axis] * m[k];

    const double m_col_major[9] = {m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]};
    const double base_products[6] = {bx * bx, bx * by, bx * bz, by * by, by * bz, bz * bz};
    for (std::size_t row = 0; row < 9u; ++row)
      for (std::size_t col = 0; col < 6u; ++col)
        hrot[row * 6u + col] += m_col_major[row] * base_products[col];
  }

  const double scale = 2.0 / static_cast<double>(n);
  result.translation_gradient = {gx * scale, gy * scale, gz * scale};
  for (std::size_t i = 0; i < 9u; ++i) {
    result.translation_hessian[i] = htt[i] * scale;
    result.dcost_drt[i] = dcost[i] * scale;
  }
  for (std::size_t i = 0; i < 27u; ++i)
    result.dcost_drt_b[i] = dcost_b[i] * scale;
  for (std::size_t i = 0; i < 54u; ++i)
    result.hessian_rot_tmp[i] = hrot[i] * scale;
  return result;
}

inline CovarianceResult
covariance_post_knn_std(const CovarianceInput& input)
{
  CovarianceResult result;
  result.cov00.resize(input.points);
  result.cov10.resize(input.points);
  result.cov11.resize(input.points);
  result.cov20.resize(input.points);
  result.cov21.resize(input.points);
  result.cov22.resize(input.points);
  if (input.k == 0)
    return result;

  const double inv_k = 1.0 / static_cast<double>(input.k);
  for (std::size_t p = 0; p < input.points; ++p) {
    const std::size_t offset = p * input.k;
    double sx = 0.0, sy = 0.0, sz = 0.0;
    double sxx = 0.0, syx = 0.0, syy = 0.0, szx = 0.0, szy = 0.0, szz = 0.0;
    for (std::size_t j = 0; j < input.k; ++j) {
      const double dx = input.dx[offset + j];
      const double dy = input.dy[offset + j];
      const double dz = input.dz[offset + j];
      sx += dx;
      sy += dy;
      sz += dz;
      sxx += dx * dx;
      syx += dy * dx;
      syy += dy * dy;
      szx += dz * dx;
      szy += dz * dy;
      szz += dz * dz;
    }
    const double mx = sx * inv_k;
    const double my = sy * inv_k;
    const double mz = sz * inv_k;
    result.cov00[p] = sxx * inv_k - mx * mx;
    result.cov10[p] = syx * inv_k - my * mx;
    result.cov11[p] = syy * inv_k - my * my;
    result.cov20[p] = szx * inv_k - mz * mx;
    result.cov21[p] = szy * inv_k - mz * my;
    result.cov22[p] = szz * inv_k - mz * mz;
  }
  return result;
}

} // namespace pcl::registration::rvv_gicp_support
