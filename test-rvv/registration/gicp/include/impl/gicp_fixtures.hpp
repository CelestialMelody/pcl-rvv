/*
 * 本文件负责 gicp topic 的确定性输入样本。样本是 test-only（仅测试使用）
 * 的密集数组，复刻 gicp.hpp 中已经完成 correspondence / KNN 之后的
 * 局部数学输入形状。
 *
 * 证据边界：
 * 这里不测 KdTree search（最近邻搜索）、真实 PointT traits（点类型字段特征）
 * 或 Eigen solver（求解器）。这些输入只让 residual / covariance 组件在
 * QEMU correctness（QEMU 正确性）和板卡 benchmark（性能测试）中可复现。
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pcl::registration::rvv_gicp_support {

struct ResidualInput {
  std::size_t rows{0};
  std::vector<double> src_x;
  std::vector<double> src_y;
  std::vector<double> src_z;
  std::vector<double> tgt_x;
  std::vector<double> tgt_y;
  std::vector<double> tgt_z;
  std::vector<double> m00;
  std::vector<double> m01;
  std::vector<double> m02;
  std::vector<double> m10;
  std::vector<double> m11;
  std::vector<double> m12;
  std::vector<double> m20;
  std::vector<double> m21;
  std::vector<double> m22;
  std::array<double, 12> transform{};
  std::array<double, 12> base_transform{};
};

struct ResidualResult {
  double f{0.0};
  std::array<double, 3> translation_gradient{};
  std::array<double, 9> dcost_drt{};
  bool used_rvv{false};
};

struct HessianLoopResult {
  std::array<double, 3> translation_gradient{};
  std::array<double, 9> translation_hessian{};
  std::array<double, 9> dcost_drt{};
  std::array<double, 27> dcost_drt_b{};
  std::array<double, 54> hessian_rot_tmp{};
  bool used_rvv{false};
};

struct IndexedResidualInput {
  std::size_t rows{0};
  std::vector<std::uint64_t> src_idx;
  std::vector<std::uint64_t> tgt_idx;
  std::vector<double> src_x;
  std::vector<double> src_y;
  std::vector<double> src_z;
  std::vector<double> tgt_x;
  std::vector<double> tgt_y;
  std::vector<double> tgt_z;
  std::vector<double> m00;
  std::vector<double> m01;
  std::vector<double> m02;
  std::vector<double> m10;
  std::vector<double> m11;
  std::vector<double> m12;
  std::vector<double> m20;
  std::vector<double> m21;
  std::vector<double> m22;
  std::array<double, 12> transform{};
  std::array<double, 12> base_transform{};
};

struct CovarianceInput {
  std::size_t points{0};
  std::size_t k{0};
  std::vector<double> dx;
  std::vector<double> dy;
  std::vector<double> dz;
};

struct CovarianceResult {
  std::vector<double> cov00;
  std::vector<double> cov10;
  std::vector<double> cov11;
  std::vector<double> cov20;
  std::vector<double> cov21;
  std::vector<double> cov22;
  bool used_rvv{false};
};

inline double
wave_value(const std::size_t i, const double scale, const double phase)
{
  const double x = static_cast<double>((i * 37u + 11u) % 997u) * 0.013 + phase;
  return scale * (std::sin(x) + 0.25 * std::cos(0.37 * x));
}

inline ResidualInput
make_residual_input(const std::size_t rows)
{
  ResidualInput input;
  input.rows = rows;
  input.src_x.resize(rows);
  input.src_y.resize(rows);
  input.src_z.resize(rows);
  input.tgt_x.resize(rows);
  input.tgt_y.resize(rows);
  input.tgt_z.resize(rows);
  input.m00.resize(rows);
  input.m01.resize(rows);
  input.m02.resize(rows);
  input.m10.resize(rows);
  input.m11.resize(rows);
  input.m12.resize(rows);
  input.m20.resize(rows);
  input.m21.resize(rows);
  input.m22.resize(rows);

  input.transform = {0.9987, -0.0312, 0.0391, 0.35,
                     0.0330, 0.9981, -0.0460, -0.28,
                     -0.0376, 0.0472, 0.9980, 0.17};
  input.base_transform = {0.9991, -0.0180, 0.0370, 0.11,
                          0.0195, 0.9990, -0.0410, -0.07,
                          -0.0362, 0.0417, 0.9984, 0.05};

  for (std::size_t i = 0; i < rows; ++i) {
    input.src_x[i] = wave_value(i, 4.0, 0.1) + 0.003 * static_cast<double>(i % 5u);
    input.src_y[i] = wave_value(i, 3.0, 1.7) - 0.002 * static_cast<double>(i % 7u);
    input.src_z[i] = wave_value(i, 2.0, 2.9) + 0.001 * static_cast<double>(i % 11u);

    input.tgt_x[i] = 0.97 * input.src_x[i] - 0.04 * input.src_y[i] + 0.02;
    input.tgt_y[i] = 0.03 * input.src_x[i] + 1.02 * input.src_y[i] - 0.04;
    input.tgt_z[i] = 1.01 * input.src_z[i] + 0.05 * input.src_x[i] + 0.01;

    const double a = 1.0 + 0.01 * static_cast<double>(i % 13u);
    const double b = 0.02 * std::sin(static_cast<double>(i) * 0.17);
    const double c = 0.015 * std::cos(static_cast<double>(i) * 0.11);
    const double d = 0.012 * std::sin(static_cast<double>(i) * 0.07);
    input.m00[i] = a + 0.20;
    input.m01[i] = b;
    input.m02[i] = c;
    input.m10[i] = b;
    input.m11[i] = a + 0.35;
    input.m12[i] = d;
    input.m20[i] = c;
    input.m21[i] = d;
    input.m22[i] = a + 0.50;
  }
  return input;
}

inline IndexedResidualInput
make_indexed_residual_input(const std::size_t rows)
{
  IndexedResidualInput input;
  input.rows = rows;
  input.src_idx.resize(rows);
  input.tgt_idx.resize(rows);

  const std::size_t source_size = rows + 257u;
  const std::size_t target_size = rows + 131u;
  input.src_x.resize(source_size);
  input.src_y.resize(source_size);
  input.src_z.resize(source_size);
  input.tgt_x.resize(target_size);
  input.tgt_y.resize(target_size);
  input.tgt_z.resize(target_size);
  input.m00.resize(source_size);
  input.m01.resize(source_size);
  input.m02.resize(source_size);
  input.m10.resize(source_size);
  input.m11.resize(source_size);
  input.m12.resize(source_size);
  input.m20.resize(source_size);
  input.m21.resize(source_size);
  input.m22.resize(source_size);

  input.transform = {0.9987, -0.0312, 0.0391, 0.35,
                     0.0330, 0.9981, -0.0460, -0.28,
                     -0.0376, 0.0472, 0.9980, 0.17};
  input.base_transform = {0.9991, -0.0180, 0.0370, 0.11,
                          0.0195, 0.9990, -0.0410, -0.07,
                          -0.0362, 0.0417, 0.9984, 0.05};

  for (std::size_t i = 0; i < source_size; ++i) {
    input.src_x[i] = wave_value(i, 4.0, 0.1) + 0.003 * static_cast<double>(i % 5u);
    input.src_y[i] = wave_value(i, 3.0, 1.7) - 0.002 * static_cast<double>(i % 7u);
    input.src_z[i] = wave_value(i, 2.0, 2.9) + 0.001 * static_cast<double>(i % 11u);
    const double a = 1.0 + 0.01 * static_cast<double>(i % 13u);
    const double b = 0.02 * std::sin(static_cast<double>(i) * 0.17);
    const double c = 0.015 * std::cos(static_cast<double>(i) * 0.11);
    const double d = 0.012 * std::sin(static_cast<double>(i) * 0.07);
    input.m00[i] = a + 0.20;
    input.m01[i] = b;
    input.m02[i] = c;
    input.m10[i] = b;
    input.m11[i] = a + 0.35;
    input.m12[i] = d;
    input.m20[i] = c;
    input.m21[i] = d;
    input.m22[i] = a + 0.50;
  }

  for (std::size_t i = 0; i < target_size; ++i) {
    const double sx = wave_value(i * 3u + 5u, 3.8, 0.4);
    const double sy = wave_value(i * 5u + 7u, 2.7, 1.3);
    const double sz = wave_value(i * 7u + 11u, 1.9, 2.4);
    input.tgt_x[i] = 0.97 * sx - 0.04 * sy + 0.02;
    input.tgt_y[i] = 0.03 * sx + 1.02 * sy - 0.04;
    input.tgt_z[i] = 1.01 * sz + 0.05 * sx + 0.01;
  }

  for (std::size_t i = 0; i < rows; ++i) {
    input.src_idx[i] = static_cast<std::uint64_t>((i * 37u + 17u) % source_size);
    input.tgt_idx[i] = static_cast<std::uint64_t>((i * 53u + 23u) % target_size);
  }
  return input;
}

inline CovarianceInput
make_covariance_input(const std::size_t points, const std::size_t k)
{
  CovarianceInput input;
  input.points = points;
  input.k = k;
  input.dx.resize(points * k);
  input.dy.resize(points * k);
  input.dz.resize(points * k);
  for (std::size_t p = 0; p < points; ++p) {
    for (std::size_t j = 0; j < k; ++j) {
      const std::size_t idx = p * k + j;
      const double center = static_cast<double>((p * 17u + j * 31u) % 541u);
      input.dx[idx] = 0.08 * std::sin(0.021 * center) + 0.0003 * static_cast<double>(j);
      input.dy[idx] = 0.07 * std::cos(0.019 * center) - 0.0002 * static_cast<double>(j);
      input.dz[idx] = 0.05 * std::sin(0.023 * center + 0.4);
    }
  }
  return input;
}

inline void
mix_u64(std::uint64_t& hash, const std::uint64_t value)
{
  hash ^= value;
  hash *= 1099511628211ull;
}

inline void
mix_double(std::uint64_t& hash, const double value)
{
  union {
    double d;
    std::uint64_t u;
  } bits{value};
  mix_u64(hash, bits.u);
}

inline std::uint64_t
checksum_residual(const ResidualResult& result)
{
  std::uint64_t hash = 1469598103934665603ull;
  mix_double(hash, result.f);
  for (double value : result.translation_gradient)
    mix_double(hash, value);
  for (double value : result.dcost_drt)
    mix_double(hash, value);
  return hash;
}

inline std::uint64_t
checksum_hessian_loop(const HessianLoopResult& result)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (double value : result.translation_gradient)
    mix_double(hash, value);
  for (double value : result.translation_hessian)
    mix_double(hash, value);
  for (double value : result.dcost_drt)
    mix_double(hash, value);
  for (double value : result.dcost_drt_b)
    mix_double(hash, value);
  for (double value : result.hessian_rot_tmp)
    mix_double(hash, value);
  return hash;
}

inline std::uint64_t
checksum_covariance(const CovarianceResult& result)
{
  std::uint64_t hash = 1469598103934665603ull;
  mix_u64(hash, static_cast<std::uint64_t>(result.cov00.size()));
  const std::size_t n = result.cov00.size();
  for (std::size_t i = 0; i < n; ++i) {
    mix_double(hash, result.cov00[i]);
    mix_double(hash, result.cov10[i]);
    mix_double(hash, result.cov11[i]);
    mix_double(hash, result.cov20[i]);
    mix_double(hash, result.cov21[i]);
    mix_double(hash, result.cov22[i]);
  }
  return hash;
}

} // namespace pcl::registration::rvv_gicp_support
