/*
 * sincos_range_smoke.cpp - RangeImageSpherical-shaped sinf/cosf 调用方形态冒烟测试。
 *
 * 本文件只用于实验：它镜像球面 RangeImage 调用方的 angle/range 形态，
 * 但不调用、不修改生产 RangeImage 代码。它只提供下游形态证据；数学专项
 * 验收仍由 sincos_test.cpp 负责。
 *
 * 阅读提示：
 *   1. build_range_image_spherical_samples 生成类似
 *      RangeImageSpherical::getAnglesFromImagePoint 的 angle_x/angle_y/range。
 *   2. calculate3DPoint_local_with_scratch_sincos 是一眼能看懂的 single-entry
 *      调用方形态入口：它镜像 local xyz 公式，并调用
 *      finite-domain sincos helper。
 *   3. calculate3DPoint_local_with_scratch_sincos_batch 是标量批量路径：
 *      先批量算 sin/cos，再算 xyz；sanity check（健全性检查）会抽样比较它和
 *      single-entry 入口完全一致。
 *   4. calculate3DPoint_local_with_scratch_sincos_rvv_batch 是 RVV 批量路径：
 *      通过 rvv_sincos_finite_domain 到达 lane-level helper
 *     （单个 RVV 向量寄存器级 helper）sincos_finite_domain_RVV_f32m2。
 *   5. update_reference_error 对比 std::sin/std::cos 参考链路的 local xyz；
 *      update_rvv_diff 对比 RVV 与标量同构链路。
 *   6. 本 smoke 只覆盖 local xyz shape，不调用 production RangeImage /
 *      RangeImageSpherical，不覆盖 to_world_system_，也不覆盖 base RangeImage 中
 *      angle_x / cos(angle_y) 的放大路径。
 */
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <vector>

#include "sincos_finite_domain.hpp"

namespace {

namespace scratch = sincos_scratch;
using scratch::kPi;
using scratch::kPi2;
using scratch::kPi4;
using scratch::kLpAbsHiLoConfig;

struct SmokeStats {
  std::size_t samples = 0;
  std::size_t in_contract = 0;
  std::size_t domain_out = 0;
  std::size_t angle_x_in_contract = 0;
  std::size_t angle_y_in_contract = 0;
  float min_angle_x = std::numeric_limits<float>::infinity();
  float max_angle_x = -std::numeric_limits<float>::infinity();
  float min_angle_y = std::numeric_limits<float>::infinity();
  float max_angle_y = -std::numeric_limits<float>::infinity();
  double max_abs_x = 0.0;
  double max_abs_y = 0.0;
  double max_abs_z = 0.0;
  double max_euclidean = 0.0;
  double mean_euclidean = 0.0;
  float worst_angle_x = 0.0f;
  float worst_angle_y = 0.0f;
  float worst_range = 0.0f;
  double single_entry_max_diff = 0.0;
#if defined(__RVV10__)
  double rvv_max_sincos_diff = 0.0;
  double rvv_max_xyz_diff = 0.0;
  int rvv_nan_mismatch = 0;
  int rvv_nonfinite_mismatch = 0;
#endif
};

struct LocalPoint {
  double x;
  double y;
  double z;
};

constexpr double kMaxEuclideanGate = 1.0e-5;
constexpr double kRvvDiffGate = 0.0;
constexpr double kSingleEntryDiffGate = 0.0;

// 合同谓词。作用：复用数学 helper 的 finite-domain（有限输入域）合同，防止
// 调用方形态冒烟测试生成输入时，不能悄悄扩大 helper 的合同域。调用者：
// update_reference_error。类别：调用方形态冒烟测试验收条件。
inline bool
in_contract_domain(float x)
{
  return scratch::in_contract_domain(x);
}

// 标量同构批量驱动。作用：给调用方形态冒烟测试生成实验 sin/cos 输出；
// 调用者：标量批量路径。类别：标量/RVV 同构链路，不是 libm 参考链路，
// 也不是生产 RangeImage 代码。
void
run_scalar_scratch(const std::vector<float>& xs, std::vector<float>& out_s, std::vector<float>& out_c)
{
  scratch::scalar_sincos_finite_domain(xs, out_s, out_c, kLpAbsHiLoConfig);
}

#if defined(__RVV10__)
// RVV 批量驱动。作用：通过 rvv_sincos_finite_domain 到达 lane-level helper：
// rvv_sincos_finite_domain -> sincos_finite_domain_RVV_f32m2。调用者：RVV
// 调用方形态批量路径。类别：RVV 执行链路 / 标量-RVV 同构链路。
void
run_rvv_scratch(const std::vector<float>& xs, std::vector<float>& out_s, std::vector<float>& out_c)
{
  scratch::rvv_sincos_finite_domain(xs, out_s, out_c);
}
#endif

// libm 参考链路。作用：只用于统计 local xyz 下游误差；
// 不参与标量/RVV 同构链路比较。调用者：
// calculate3DPoint_local_with_reference_libm。
inline float
ref_sin_f32(float x)
{
  return static_cast<float>(std::sin(static_cast<double>(x)));
}

inline float
ref_cos_f32(float x)
{
  return static_cast<float>(std::cos(static_cast<double>(x)));
}

void
append_sample(std::vector<float>& angle_xs, std::vector<float>& angle_ys, std::vector<float>& ranges,
              float angle_x, float angle_y, float range)
{
  angle_xs.push_back(angle_x);
  angle_ys.push_back(angle_y);
  ranges.push_back(range);
}

// 调用方输入域样本生成器。作用：镜像球面
// image-to-angle（图像点到角度）公式，使用完整 360x180 度网格和手工边界点。
// 调用者：run_range_image_spherical_smoke。类别：调用方形态冒烟测试。
// 它只能证明 spherical shape 落在 [-pi, pi] / [-pi/2, pi/2]；不能证明 base
// RangeImage，因为 base RangeImage 的 angle_x 可能被 cos(angle_y) 除法放大。
void
build_range_image_spherical_samples(
    std::vector<float>& angle_xs, std::vector<float>& angle_ys, std::vector<float>& ranges)
{
  // 镜像 RangeImageSpherical::getAnglesFromImagePoint，但不调用生产代码。
  // 这只闭合 spherical 调用方形态输入域，不闭合 base RangeImage。
  constexpr int kWidth = 720;
  constexpr int kHeight = 360;
  constexpr int kOffsetX = 0;
  constexpr int kOffsetY = 0;
  constexpr float kAngularResolutionX = 2.0f * kPi / static_cast<float>(kWidth);
  constexpr float kAngularResolutionY = kPi / static_cast<float>(kHeight);
  const float range_values[] = {1.0f, 10.0f, 80.0f};

  angle_xs.reserve(static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight) * 3u + 256u);
  angle_ys.reserve(angle_xs.capacity());
  ranges.reserve(angle_xs.capacity());

  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const float angle_x =
          (static_cast<float>(x + kOffsetX) * kAngularResolutionX) - kPi;
      const float angle_y =
          (static_cast<float>(y + kOffsetY) * kAngularResolutionY) - kPi2;
      for (float range : range_values)
        append_sample(angle_xs, angle_ys, ranges, angle_x, angle_y, range);
    }
  }

  const float edge_angles_x[] = {
      -kPi, std::nextafterf(-kPi, 0.0f), -kPi2, -kPi4, 0.0f,
      kPi4, kPi2, std::nextafterf(kPi, 0.0f), kPi};
  const float edge_angles_y[] = {
      -kPi2, std::nextafterf(-kPi2, 0.0f), -kPi4, 0.0f,
      kPi4, std::nextafterf(kPi2, 0.0f), kPi2};
  for (float angle_y : edge_angles_y) {
    for (float angle_x : edge_angles_x) {
      for (float range : range_values)
        append_sample(angle_xs, angle_ys, ranges, angle_x, angle_y, range);
    }
  }
}

// local xyz 公式。作用：single-entry、标量批量、RVV 批量和 libm 参考链路
// 共用同一几何公式，让读者能看出不同路径只替换 sin/cos provider（提供者）。
// 类别：调用方形态冒烟测试。
inline void
compute_xyz(float sx, float cx, float sy, float cy, float range, double& x, double& y, double& z)
{
  x = static_cast<double>(range) * static_cast<double>(sx) * static_cast<double>(cy);
  y = static_cast<double>(range) * static_cast<double>(sy);
  z = static_cast<double>(range) * static_cast<double>(cx) * static_cast<double>(cy);
}

inline LocalPoint
compute_xyz(float sx, float cx, float sy, float cy, float range)
{
  LocalPoint p{0.0, 0.0, 0.0};
  compute_xyz(sx, cx, sy, cy, range, p.x, p.y, p.z);
  return p;
}

inline double
point_distance(const LocalPoint& a, const LocalPoint& b)
{
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  const double dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// 接近生产调用形态的标量冒烟入口。
// 作用：镜像 RangeImageSpherical::calculate3DPoint 的 local xyz 数学，只把
// std::sin/std::cos 替换为实验用的有限域 helper。调用者：single-entry
// 健全性检查。类别：调用方形态冒烟测试；不调用 RangeImage 生产代码，也不覆盖
// to_world_system_ 变换。
inline LocalPoint
calculate3DPoint_local_with_scratch_sincos(float angle_x, float angle_y, float range)
{
  const scratch::SinCosPair x = scratch::scalar_sincos_finite_domain(angle_x, kLpAbsHiLoConfig);
  const scratch::SinCosPair y = scratch::scalar_sincos_finite_domain(angle_y, kLpAbsHiLoConfig);
  return compute_xyz(x.s, x.c, y.s, y.c, range);
}

inline LocalPoint
calculate3DPoint_local_with_reference_libm(float angle_x, float angle_y, float range)
{
  // 作用：生成 std::sin/std::cos local xyz 参考值；调用者：update_reference_error。
  // 类别：参考链路。
  return compute_xyz(
      ref_sin_f32(angle_x),
      ref_cos_f32(angle_x),
      ref_sin_f32(angle_y),
      ref_cos_f32(angle_y),
      range);
}

// 标量批量路径。作用：先批量计算 angle_x/angle_y 的 sin/cos，
// 再用这些输出计算 xyz，避免每点重复调用 helper。调用者：
// run_range_image_spherical_smoke。类别：调用方形态冒烟测试 / 标量同构链路。
void
calculate3DPoint_local_with_scratch_sincos_batch(const std::vector<float>& angle_xs,
                                                 const std::vector<float>& angle_ys,
                                                 const std::vector<float>& ranges,
                                                 std::vector<float>& sx,
                                                 std::vector<float>& cx,
                                                 std::vector<float>& sy,
                                                 std::vector<float>& cy,
                                                 std::vector<LocalPoint>& points)
{
  run_scalar_scratch(angle_xs, sx, cx);
  run_scalar_scratch(angle_ys, sy, cy);
  for (std::size_t i = 0; i < ranges.size(); ++i)
    points[i] = compute_xyz(sx[i], cx[i], sy[i], cy[i], ranges[i]);
}

#if defined(__RVV10__)
// RVV 批量路径。作用：只把 sin/cos 数组的计算换成
// rvv_sincos_finite_domain，xyz 仍由同一个 compute_xyz 公式完成。调用者：
// run_range_image_spherical_smoke。类别：调用方形态冒烟测试 / RVV 执行链路。
void
calculate3DPoint_local_with_scratch_sincos_rvv_batch(const std::vector<float>& angle_xs,
                                                     const std::vector<float>& angle_ys,
                                                     const std::vector<float>& ranges,
                                                     std::vector<float>& sx,
                                                     std::vector<float>& cx,
                                                     std::vector<float>& sy,
                                                     std::vector<float>& cy,
                                                     std::vector<LocalPoint>& points)
{
  run_rvv_scratch(angle_xs, sx, cx);
  run_rvv_scratch(angle_ys, sy, cy);
  for (std::size_t i = 0; i < ranges.size(); ++i)
    points[i] = compute_xyz(sx[i], cx[i], sy[i], cy[i], ranges[i]);
}
#endif

// 可读性健全性检查。作用：抽样确认一眼能懂的 single-entry 调用方形态入口
// 与标量批量输出完全一致。调用者：
// run_range_image_spherical_smoke。类别：调用方形态冒烟测试验收条件。
bool
check_single_entry_matches_batch(const std::vector<float>& angle_xs,
                                 const std::vector<float>& angle_ys,
                                 const std::vector<float>& ranges,
                                 const std::vector<LocalPoint>& batch_points,
                                 SmokeStats& st)
{
  const std::size_t n = ranges.size();
  const std::size_t indices[] = {
      0u,
      1u,
      n / 4u,
      n / 2u,
      (n * 3u) / 4u,
      n - 2u,
      n - 1u,
  };
  for (std::size_t idx : indices) {
    const LocalPoint single =
        calculate3DPoint_local_with_scratch_sincos(angle_xs[idx], angle_ys[idx], ranges[idx]);
    st.single_entry_max_diff =
        std::max(st.single_entry_max_diff, point_distance(single, batch_points[idx]));
  }
  return st.single_entry_max_diff <= kSingleEntryDiffGate;
}

// 下游参考误差统计。作用：比较标量实验
// local xyz 与 std::sin/std::cos local xyz，同时检查 angle_x/angle_y 输入域。
// 调用者：run_range_image_spherical_smoke。类别：调用方形态冒烟测试；数学正确性
// 仍由 sincos_test.cpp 负责。
void
update_reference_error(const std::vector<float>& angle_xs,
                       const std::vector<float>& angle_ys,
                       const std::vector<float>& ranges,
                       const std::vector<LocalPoint>& scratch_points,
                       SmokeStats& st)
{
  st.samples = angle_xs.size();
  for (std::size_t i = 0; i < st.samples; ++i) {
    const float angle_x = angle_xs[i];
    const float angle_y = angle_ys[i];
    const float range = ranges[i];
    const bool angle_x_ok = in_contract_domain(angle_x);
    const bool angle_y_ok = std::isfinite(angle_y) && angle_y >= -kPi2 && angle_y <= kPi2;
    const bool in_domain = angle_x_ok && angle_y_ok;
    st.angle_x_in_contract += angle_x_ok ? 1u : 0u;
    st.angle_y_in_contract += angle_y_ok ? 1u : 0u;
    st.in_contract += in_domain ? 1u : 0u;
    st.domain_out += in_domain ? 0u : 1u;
    st.min_angle_x = std::min(st.min_angle_x, angle_x);
    st.max_angle_x = std::max(st.max_angle_x, angle_x);
    st.min_angle_y = std::min(st.min_angle_y, angle_y);
    st.max_angle_y = std::max(st.max_angle_y, angle_y);
    if (!in_domain)
      continue;

    const LocalPoint& approx = scratch_points[i];
    const LocalPoint ref = calculate3DPoint_local_with_reference_libm(angle_x, angle_y, range);
    const double dx = approx.x - ref.x;
    const double dy = approx.y - ref.y;
    const double dz = approx.z - ref.z;
    const double euclidean = point_distance(approx, ref);
    st.max_abs_x = std::max(st.max_abs_x, std::fabs(dx));
    st.max_abs_y = std::max(st.max_abs_y, std::fabs(dy));
    st.max_abs_z = std::max(st.max_abs_z, std::fabs(dz));
    st.mean_euclidean += euclidean;
    if (euclidean > st.max_euclidean) {
      st.max_euclidean = euclidean;
      st.worst_angle_x = angle_x;
      st.worst_angle_y = angle_y;
      st.worst_range = range;
    }
  }

  if (st.in_contract != 0)
    st.mean_euclidean /= static_cast<double>(st.in_contract);
}

#if defined(__RVV10__)
// RVV/标量同构冒烟。作用：检查原始 sin/cos lane（向量通道）和由其计算出的
// local xyz 都与标量同构链路完全一致。调用者：run_range_image_spherical_smoke。
// 类别：调用方形态冒烟测试 / RVV 验收条件。
void
update_rvv_diff(const std::vector<float>& scalar_sx,
                const std::vector<float>& scalar_cx,
                const std::vector<float>& scalar_sy,
                const std::vector<float>& scalar_cy,
                const std::vector<LocalPoint>& scalar_points,
                const std::vector<float>& rvv_sx,
                const std::vector<float>& rvv_cx,
                const std::vector<float>& rvv_sy,
                const std::vector<float>& rvv_cy,
                const std::vector<LocalPoint>& rvv_points,
                SmokeStats& st)
{
  for (std::size_t i = 0; i < scalar_points.size(); ++i) {
    const float scalar_vals[] = {scalar_sx[i], scalar_cx[i], scalar_sy[i], scalar_cy[i]};
    const float rvv_vals[] = {rvv_sx[i], rvv_cx[i], rvv_sy[i], rvv_cy[i]};
    for (int j = 0; j < 4; ++j) {
      const bool scalar_nan = std::isnan(scalar_vals[j]);
      const bool rvv_nan = std::isnan(rvv_vals[j]);
      if (scalar_nan || rvv_nan) {
        st.rvv_nan_mismatch += (scalar_nan != rvv_nan) ? 1 : 0;
        continue;
      }
      if (!std::isfinite(scalar_vals[j]) || !std::isfinite(rvv_vals[j])) {
        st.rvv_nonfinite_mismatch += (scalar_vals[j] != rvv_vals[j]) ? 1 : 0;
        continue;
      }
      st.rvv_max_sincos_diff = std::max(
          st.rvv_max_sincos_diff,
          static_cast<double>(std::fabs(scalar_vals[j] - rvv_vals[j])));
    }

    st.rvv_max_xyz_diff =
        std::max(st.rvv_max_xyz_diff, point_distance(rvv_points[i], scalar_points[i]));
  }
}
#endif

// 顶层冒烟验收。作用：验证 spherical 调用方形态 local xyz 的输入域、下游
// xyz 误差、single-entry/batch 一致性，以及 RVV 构建下的标量/RVV 同构链路
// 一致性。调用者：main。类别：调用方形态冒烟测试验收条件。
bool
run_range_image_spherical_smoke()
{
  std::vector<float> angle_xs;
  std::vector<float> angle_ys;
  std::vector<float> ranges;
  build_range_image_spherical_samples(angle_xs, angle_ys, ranges);

  // 标量实验链路通过接近生产调用形态的 local calculate3DPoint 入口，
  // 统计它相对 std::sin/std::cos 参考值的下游误差。
  std::vector<float> scalar_sx(angle_xs.size());
  std::vector<float> scalar_cx(angle_xs.size());
  std::vector<float> scalar_sy(angle_ys.size());
  std::vector<float> scalar_cy(angle_ys.size());
  std::vector<LocalPoint> scalar_points(ranges.size());
  calculate3DPoint_local_with_scratch_sincos_batch(
      angle_xs, angle_ys, ranges, scalar_sx, scalar_cx, scalar_sy, scalar_cy, scalar_points);

  SmokeStats st;
  const bool single_entry_ok =
      check_single_entry_matches_batch(angle_xs, angle_ys, ranges, scalar_points, st);
  update_reference_error(angle_xs, angle_ys, ranges, scalar_points, st);

#if defined(__RVV10__)
  // RVV 实验链路使用同一个调用方形态入口，再和标量同构链路比较输出。
  std::vector<float> rvv_sx(angle_xs.size());
  std::vector<float> rvv_cx(angle_xs.size());
  std::vector<float> rvv_sy(angle_ys.size());
  std::vector<float> rvv_cy(angle_ys.size());
  std::vector<LocalPoint> rvv_points(ranges.size());
  calculate3DPoint_local_with_scratch_sincos_rvv_batch(
      angle_xs, angle_ys, ranges, rvv_sx, rvv_cx, rvv_sy, rvv_cy, rvv_points);
  update_rvv_diff(
      scalar_sx, scalar_cx, scalar_sy, scalar_cy, scalar_points, rvv_sx, rvv_cx, rvv_sy, rvv_cy, rvv_points, st);
#endif

  const bool angle_x_ok = st.angle_x_in_contract == st.samples;
  const bool angle_y_ok = st.angle_y_in_contract == st.samples;
  const bool domain_ok = st.domain_out == 0 && st.in_contract == st.samples;
  const bool error_ok = st.max_euclidean <= kMaxEuclideanGate;
#if defined(__RVV10__)
  const bool rvv_ok = st.rvv_nan_mismatch == 0 && st.rvv_nonfinite_mismatch == 0 &&
                      st.rvv_max_sincos_diff <= kRvvDiffGate &&
                      st.rvv_max_xyz_diff <= kRvvDiffGate;
#else
  const bool rvv_ok = true;
#endif
  const bool ok = angle_x_ok && angle_y_ok && domain_ok && single_entry_ok && error_ok && rvv_ok;

  std::printf("\n=== RangeImageSpherical-shaped smoke: local calculate3DPoint form ===\n");
  std::printf("source formula mirrored from RangeImageSpherical: angle_x=(image_x+offset_x)*res_x-pi, angle_y=(image_y+offset_y)*res_y-pi/2\n");
  std::printf("scratch config: lp-abs-hi-lo, full grid width=720 height=360 offsets=0/0\n");
  std::printf("samples=%zu angle_x_in_contract=%zu angle_y_in_contract=%zu in_contract=%zu domain_out=%zu\n",
              st.samples,
              st.angle_x_in_contract,
              st.angle_y_in_contract,
              st.in_contract,
              st.domain_out);
  std::printf("angle_x range=[%.9e, %.9e], angle_y range=[%.9e, %.9e]\n",
              static_cast<double>(st.min_angle_x),
              static_cast<double>(st.max_angle_x),
              static_cast<double>(st.min_angle_y),
              static_cast<double>(st.max_angle_y));
  std::printf("xyz max abs: x=%.9e y=%.9e z=%.9e; euclidean max=%.9e mean=%.9e at angle_x=%.9e angle_y=%.9e range=%.9e\n",
              st.max_abs_x,
              st.max_abs_y,
              st.max_abs_z,
              st.max_euclidean,
              st.mean_euclidean,
              static_cast<double>(st.worst_angle_x),
              static_cast<double>(st.worst_angle_y),
              static_cast<double>(st.worst_range));
  std::printf("[gate] angle_x in [-pi, pi]: %s\n", angle_x_ok ? "PASS" : "FAIL");
  std::printf("[gate] angle_y in [-pi/2, pi/2]: %s\n", angle_y_ok ? "PASS" : "FAIL");
  std::printf("[gate] domain_out == 0 and in_contract == samples: %s\n", domain_ok ? "PASS" : "FAIL");
  std::printf("[gate] single-entry calculate3DPoint local smoke matches scalar batch, max diff = %.9e <= %.1e: %s\n",
              st.single_entry_max_diff,
              kSingleEntryDiffGate,
              single_entry_ok ? "PASS" : "FAIL");
  std::printf("[gate] euclidean max <= %.1e scratch caller smoke threshold: %s\n",
              kMaxEuclideanGate,
              error_ok ? "PASS" : "FAIL");
#if defined(__RVV10__)
  std::printf("[RVV vs scalar same-chain] sin/cos max |diff| = %.9e, xyz max euclidean diff = %.9e, nan mismatch = %d, nonfinite mismatch = %d\n",
              st.rvv_max_sincos_diff,
              st.rvv_max_xyz_diff,
              st.rvv_nan_mismatch,
              st.rvv_nonfinite_mismatch);
  std::printf("[gate] RVV caller-shaped path matches scalar same-chain exactly: %s\n", rvv_ok ? "PASS" : "FAIL");
#else
  std::printf("[RVV vs scalar same-chain] disabled: build without __RVV10__\n");
#endif
  std::printf("caller smoke is downstream evidence only; it does not replace parameter scripts, dense/adversarial math tests, or scalar/RVV chain checks.\n");
  std::printf("production gate note: base RangeImage angle_x divides by cos(angle_y), so this smoke only closes the spherical caller shape, not every RangeImage caller.\n");
  std::printf("\n=== caller smoke gate summary: %s ===\n", ok ? "PASS" : "FAIL");
  return ok;
}

} // namespace

int
main(int argc, char** argv)
{
  if (argc != 1) {
    std::fprintf(stderr, "usage: %s\n", argv[0]);
    return 2;
  }

  std::printf("sincos range smoke: RangeImageSpherical caller shape, not a production replacement\n");
  return run_range_image_spherical_smoke() ? 0 : 1;
}
