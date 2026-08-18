/*
 * 本文件做什么：
 * 保存 bfgs topic 的标量 reference（参考链路）。这些 helper 复刻
 * bfgs.h 中 direction update（方向更新）、moveTo 和 slope 的数学语义，
 * 供 RVV diagnostic candidate 做同构对拍。
 */

#pragma once

#include "bfgs_fixtures.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace pcl::registration::rvv_bfgs_support {

inline void
ensure_same_size(const std::vector<double>& a,
                 const std::vector<double>& b,
                 const char* context)
{
  if (a.size() != b.size())
    throw std::runtime_error(context);
}

inline double
dot_std(const std::vector<double>& a, const std::vector<double>& b)
{
  ensure_same_size(a, b, "dot_std size mismatch");
  double result = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i)
    result += a[i] * b[i];
  return result;
}

inline double
norm_std(const std::vector<double>& a)
{
  return std::sqrt(dot_std(a, a));
}

inline std::vector<double>
sub_std(const std::vector<double>& a, const std::vector<double>& b)
{
  ensure_same_size(a, b, "sub_std size mismatch");
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i)
    out[i] = a[i] - b[i];
  return out;
}

inline std::vector<double>
scale_std(const std::vector<double>& a, const double scale)
{
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i)
    out[i] = a[i] * scale;
  return out;
}

inline std::vector<double>
linear3_std(const std::vector<double>& a,
            const double a_scale,
            const std::vector<double>& b,
            const double b_scale,
            const std::vector<double>& c,
            const double c_scale)
{
  ensure_same_size(a, b, "linear3_std a/b size mismatch");
  ensure_same_size(a, c, "linear3_std a/c size mismatch");
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i)
    out[i] = a_scale * a[i] + b_scale * b[i] + c_scale * c[i];
  return out;
}

inline MoveSlopeResult
move_to_and_slope_std(const DirectionInput& input, const double alpha)
{
  ensure_same_size(input.x0, input.p, "move_to_and_slope_std x0/p size mismatch");
  ensure_same_size(input.gradient, input.p, "move_to_and_slope_std gradient/p size mismatch");
  MoveSlopeResult result;
  result.x_alpha.resize(input.x0.size());
  for (std::size_t i = 0; i < input.x0.size(); ++i)
    result.x_alpha[i] = input.x0[i] + alpha * input.p[i];
  result.slope = dot_std(input.gradient, input.p);
  return result;
}

inline DirectionResult
direction_update_std(const DirectionInput& input)
{
  ensure_same_size(input.x0, input.x, "direction_update_std x0/x size mismatch");
  ensure_same_size(input.g0, input.gradient, "direction_update_std g0/gradient size mismatch");
  ensure_same_size(input.x, input.gradient, "direction_update_std x/gradient size mismatch");

  DirectionResult result;
  result.dx0 = sub_std(input.x, input.x0);
  result.dg0 = sub_std(input.gradient, input.g0);
  result.dxg = dot_std(result.dx0, input.gradient);
  result.dgg = dot_std(result.dg0, input.gradient);
  result.dxdg = dot_std(result.dx0, result.dg0);
  result.dgnorm = norm_std(result.dg0);

  if (result.dxdg != 0.0) {
    result.B = result.dxg / result.dxdg;
    result.A = -(1.0 + result.dgnorm * result.dgnorm / result.dxdg) * result.B +
               result.dgg / result.dxdg;
  }

  result.p = linear3_std(result.dx0, -result.A, input.gradient, 1.0, result.dg0, -result.B);
  result.g0norm = norm_std(input.gradient);
  result.pnorm = norm_std(result.p);

  if (result.pnorm != 0.0) {
    const double dir = (dot_std(result.p, input.gradient) > 0.0) ? -1.0 : 1.0;
    result.p = scale_std(result.p, dir / result.pnorm);
    result.pnorm = norm_std(result.p);
    result.fp0 = dot_std(result.p, input.gradient);
  }

  return result;
}

} // namespace pcl::registration::rvv_bfgs_support
