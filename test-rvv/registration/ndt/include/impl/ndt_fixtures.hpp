/*
 * 本文件做什么：
 * 构造 NDT derivative accumulation（导数累加）诊断样本。样本使用 SoA
 *（数组结构）布局，让 RVV candidate 可以跨 point-neighbor sample（点-邻域样本）
 * 批量处理；标量 reference 使用同一批数据，保证对拍只比较数学核本身。
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

namespace pcl::registration::rvv_ndt_support {

constexpr int kDim3 = 3;
constexpr int kTransformDof = 6;
constexpr int kHessianRows = 18;
constexpr int kHessianCols = 6;

struct DerivativeBatch {
  std::vector<double> x0;
  std::vector<double> x1;
  std::vector<double> x2;
  std::array<std::vector<double>, 9> c_inv;
  std::array<std::vector<double>, kDim3 * kTransformDof> jacobian;
  std::array<std::vector<double>, kHessianRows * kHessianCols> point_hessian;
  double gauss_d1{-0.6321205588285577};
  double gauss_d2{1.2};
  bool compute_hessian{true};

  std::size_t size() const { return x0.size(); }
};

struct DerivativeResult {
  double score{0.0};
  std::array<double, kTransformDof> gradient{};
  std::array<double, kTransformDof * kTransformDof> hessian{};
  bool used_rvv{false};
  std::size_t samples{0};
};

inline std::uint64_t
mix_u64(std::uint64_t seed, std::uint64_t value)
{
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

inline double
deterministic_value(const std::size_t i, const int salt, const double scale)
{
  const double t = static_cast<double>((i * 17 + static_cast<std::size_t>(salt * 31)) % 997);
  return std::sin(t * 0.013 + static_cast<double>(salt) * 0.07) * scale;
}

inline DerivativeBatch
make_derivative_batch(const std::size_t samples, const bool compute_hessian)
{
  DerivativeBatch batch;
  batch.compute_hessian = compute_hessian;
  batch.x0.resize(samples);
  batch.x1.resize(samples);
  batch.x2.resize(samples);
  for (auto& values : batch.c_inv)
    values.resize(samples);
  for (auto& values : batch.jacobian)
    values.resize(samples);
  for (auto& values : batch.point_hessian)
    values.resize(samples);

  for (std::size_t i = 0; i < samples; ++i) {
    batch.x0[i] = deterministic_value(i, 1, 0.45);
    batch.x1[i] = deterministic_value(i, 2, 0.40);
    batch.x2[i] = deterministic_value(i, 3, 0.35);

    const double c00 = 1.0 + 0.08 * std::fabs(deterministic_value(i, 4, 1.0));
    const double c11 = 1.1 + 0.07 * std::fabs(deterministic_value(i, 5, 1.0));
    const double c22 = 1.2 + 0.06 * std::fabs(deterministic_value(i, 6, 1.0));
    const double c01 = deterministic_value(i, 7, 0.015);
    const double c02 = deterministic_value(i, 8, 0.012);
    const double c12 = deterministic_value(i, 9, 0.010);
    batch.c_inv[0][i] = c00;
    batch.c_inv[1][i] = c01;
    batch.c_inv[2][i] = c02;
    batch.c_inv[3][i] = c01;
    batch.c_inv[4][i] = c11;
    batch.c_inv[5][i] = c12;
    batch.c_inv[6][i] = c02;
    batch.c_inv[7][i] = c12;
    batch.c_inv[8][i] = c22;

    for (int row = 0; row < kDim3; ++row) {
      for (int col = 0; col < kTransformDof; ++col) {
        const int index = row * kTransformDof + col;
        const double identity = row == col ? 1.0 : 0.0;
        const double angular =
            col >= 3 ? deterministic_value(i, 20 + row * 7 + col, 0.30) : 0.0;
        batch.jacobian[index][i] = identity + angular;
      }
    }

    for (int row = 0; row < kHessianRows; ++row) {
      for (int col = 0; col < kHessianCols; ++col) {
        const int index = row * kHessianCols + col;
        const bool angular_block = row >= 9 && col >= 3;
        batch.point_hessian[index][i] =
            angular_block ? deterministic_value(i, 100 + row * 11 + col, 0.08) : 0.0;
      }
    }
  }
  return batch;
}

inline std::uint64_t
mix_double(std::uint64_t seed, const double value)
{
  std::uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "double checksum assumes 64-bit double");
  std::memcpy(&bits, &value, sizeof(bits));
  return mix_u64(seed, bits);
}

inline std::uint64_t
checksum_result(const DerivativeResult& result)
{
  std::uint64_t checksum = mix_double(1469598103934665603ull, result.score);
  for (const double value : result.gradient)
    checksum = mix_double(checksum, value);
  for (const double value : result.hessian)
    checksum = mix_double(checksum, value);
  checksum = mix_u64(checksum, result.used_rvv ? 1ull : 0ull);
  checksum = mix_u64(checksum, static_cast<std::uint64_t>(result.samples));
  return checksum;
}

} // namespace pcl::registration::rvv_ndt_support
