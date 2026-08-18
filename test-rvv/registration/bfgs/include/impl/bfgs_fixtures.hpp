/*
 * 本文件做什么：
 * 保存 bfgs topic 的 deterministic fixtures（确定性输入样本）和一个小型
 * quadratic functor（二次函数回调）。它们让 correctness test 可以覆盖
 * BFGS 的 Vector6d caller shape（GICP 使用的 6 维状态）和更大的诊断维度。
 */

#pragma once

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <vector>

#include <pcl/registration/bfgs.h>

namespace pcl::registration::rvv_bfgs_support {

using Vector6d = Eigen::Matrix<double, 6, 1>;

struct DirectionInput {
  std::vector<double> x0;
  std::vector<double> x;
  std::vector<double> g0;
  std::vector<double> gradient;
  std::vector<double> p;
};

struct DirectionResult {
  std::vector<double> dx0;
  std::vector<double> dg0;
  std::vector<double> p;
  double dxg{0.0};
  double dgg{0.0};
  double dxdg{0.0};
  double dgnorm{0.0};
  double A{0.0};
  double B{0.0};
  double g0norm{0.0};
  double pnorm{0.0};
  double fp0{0.0};
  bool used_rvv{false};
};

struct MoveSlopeResult {
  std::vector<double> x_alpha;
  double slope{0.0};
  bool used_rvv{false};
};

inline DirectionInput
make_direction_input(const std::size_t n)
{
  DirectionInput input;
  input.x0.resize(n);
  input.x.resize(n);
  input.g0.resize(n);
  input.gradient.resize(n);
  input.p.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const double k = static_cast<double>(i + 1);
    input.x0[i] = 0.13 * k - 0.02 * static_cast<double>(i % 3);
    input.gradient[i] = 0.35 + 0.017 * k + 0.003 * static_cast<double>(i % 5);
    input.g0[i] = input.gradient[i] - (0.05 + 0.004 * static_cast<double>((i + 2) % 7));
    input.x[i] = input.x0[i] + 0.21 * input.gradient[i] + 0.011 * k;
    input.p[i] = -0.2 + 0.015 * k - 0.004 * static_cast<double>(i % 4);
  }

  return input;
}

inline DirectionInput
make_zero_dxdg_input()
{
  auto input = make_direction_input(6);
  for (std::size_t i = 0; i < input.x0.size(); ++i) {
    input.x0[i] = 0.0;
    input.x[i] = 0.0;
    input.g0[i] = 0.1 + static_cast<double>(i) * 0.02;
    input.gradient[i] = input.g0[i] + 0.03 + static_cast<double>(i) * 0.01;
    input.p[i] = -0.4 + 0.07 * static_cast<double>(i);
  }

  input.x[0] = 1.0;
  input.gradient[0] = input.g0[0];
  return input;
}

inline Vector6d
make_vector6_initial()
{
  Vector6d x;
  x << 1.4, -0.8, 0.6, -0.25, 0.35, -0.15;
  return x;
}

inline Vector6d
make_vector6_center()
{
  Vector6d center;
  center << 0.25, -0.15, 0.4, 0.05, -0.2, 0.1;
  return center;
}

// 这个 functor 用固定二次函数保护 BFGS public API 的最小可运行语义。
// 它不模拟 GICP 的真实 cost function，因此不能作为 caller 热点证据。
struct QuadraticFunctor : public ::BFGSDummyFunctor<double, 6> {
  using Base = ::BFGSDummyFunctor<double, 6>;
  using VectorType = typename Base::VectorType;

  explicit QuadraticFunctor(const VectorType& center_in = make_vector6_center())
  : Base()
  , center(center_in)
  {}

  double
  operator()(const VectorType& x) override
  {
    ++f_calls;
    const VectorType delta = x - center;
    return 0.5 * delta.squaredNorm();
  }

  void
  df(const VectorType& x, VectorType& df_out) override
  {
    ++df_calls;
    df_out = x - center;
  }

  void
  fdf(const VectorType& x, double& f_out, VectorType& df_out) override
  {
    ++fdf_calls;
    const VectorType delta = x - center;
    df_out = delta;
    f_out = 0.5 * delta.squaredNorm();
  }

  BFGSSpace::Status
  checkGradient(const VectorType& g) override
  {
    return (g.norm() < 1e-8) ? BFGSSpace::Success : BFGSSpace::Running;
  }

  VectorType center;
  int f_calls{0};
  int df_calls{0};
  int fdf_calls{0};
};

} // namespace pcl::registration::rvv_bfgs_support
