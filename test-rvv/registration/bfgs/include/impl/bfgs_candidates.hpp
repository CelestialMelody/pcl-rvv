/*
 * 本文件做什么：
 * 保存 bfgs topic 的 test-only RVV diagnostic candidate（测试专用 RVV 诊断候选）。
 * RVV build 下，这些 helper 用 RVV intrinsic 处理连续 double 向量，再与
 * bfgs_references.hpp 的标量 reference 做 same-chain（同构链路）对拍。
 *
 * 证据边界：
 * 本文件不进入 production。即使本 helper 在板卡上有收益，也只能说明 BFGS
 * 局部向量状态更新值得继续诊断，不能替代 GICP caller 或 production direct 证据。
 */

#pragma once

#include "bfgs_references.hpp"

#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_bfgs_support {

inline double
dot_candidate(const std::vector<double>& a,
              const std::vector<double>& b,
              bool* used_rvv = nullptr)
{
  ensure_same_size(a, b, "dot_candidate size mismatch");

#ifdef __RVV10__
  if (!a.empty()) {
    if (used_rvv)
      *used_rvv = true;
    double result = 0.0;
    std::vector<double> chunk(__riscv_vsetvlmax_e64m1());
    std::size_t i = 0;
    while (i < a.size()) {
      const std::size_t vl = __riscv_vsetvl_e64m1(a.size() - i);
      const vfloat64m1_t va = __riscv_vle64_v_f64m1(a.data() + i, vl);
      const vfloat64m1_t vb = __riscv_vle64_v_f64m1(b.data() + i, vl);
      const vfloat64m1_t prod = __riscv_vfmul_vv_f64m1(va, vb, vl);
      __riscv_vse64_v_f64m1(chunk.data(), prod, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        result += chunk[lane];
      i += vl;
    }
    return result;
  }
#endif

  if (used_rvv)
    *used_rvv = false;
  return dot_std(a, b);
}

inline std::vector<double>
sub_candidate(const std::vector<double>& a,
              const std::vector<double>& b,
              bool* used_rvv = nullptr)
{
  ensure_same_size(a, b, "sub_candidate size mismatch");
  std::vector<double> out(a.size());

#ifdef __RVV10__
  if (!a.empty()) {
    if (used_rvv)
      *used_rvv = true;
    std::size_t i = 0;
    while (i < a.size()) {
      const std::size_t vl = __riscv_vsetvl_e64m1(a.size() - i);
      const vfloat64m1_t va = __riscv_vle64_v_f64m1(a.data() + i, vl);
      const vfloat64m1_t vb = __riscv_vle64_v_f64m1(b.data() + i, vl);
      const vfloat64m1_t diff = __riscv_vfsub_vv_f64m1(va, vb, vl);
      __riscv_vse64_v_f64m1(out.data() + i, diff, vl);
      i += vl;
    }
    return out;
  }
#endif

  if (used_rvv)
    *used_rvv = false;
  return sub_std(a, b);
}

inline std::vector<double>
scale_candidate(const std::vector<double>& a,
                const double scale,
                bool* used_rvv = nullptr)
{
  std::vector<double> out(a.size());

#ifdef __RVV10__
  if (!a.empty()) {
    if (used_rvv)
      *used_rvv = true;
    std::size_t i = 0;
    while (i < a.size()) {
      const std::size_t vl = __riscv_vsetvl_e64m1(a.size() - i);
      const vfloat64m1_t va = __riscv_vle64_v_f64m1(a.data() + i, vl);
      const vfloat64m1_t scaled = __riscv_vfmul_vf_f64m1(va, scale, vl);
      __riscv_vse64_v_f64m1(out.data() + i, scaled, vl);
      i += vl;
    }
    return out;
  }
#endif

  if (used_rvv)
    *used_rvv = false;
  return scale_std(a, scale);
}

inline std::vector<double>
linear3_candidate(const std::vector<double>& a,
                  const double a_scale,
                  const std::vector<double>& b,
                  const double b_scale,
                  const std::vector<double>& c,
                  const double c_scale,
                  bool* used_rvv = nullptr)
{
  ensure_same_size(a, b, "linear3_candidate a/b size mismatch");
  ensure_same_size(a, c, "linear3_candidate a/c size mismatch");
  std::vector<double> out(a.size());

#ifdef __RVV10__
  if (!a.empty()) {
    if (used_rvv)
      *used_rvv = true;
    std::size_t i = 0;
    while (i < a.size()) {
      const std::size_t vl = __riscv_vsetvl_e64m1(a.size() - i);
      const vfloat64m1_t va = __riscv_vle64_v_f64m1(a.data() + i, vl);
      const vfloat64m1_t vb = __riscv_vle64_v_f64m1(b.data() + i, vl);
      const vfloat64m1_t vc = __riscv_vle64_v_f64m1(c.data() + i, vl);
      const vfloat64m1_t as = __riscv_vfmul_vf_f64m1(va, a_scale, vl);
      const vfloat64m1_t bs = __riscv_vfmul_vf_f64m1(vb, b_scale, vl);
      const vfloat64m1_t cs = __riscv_vfmul_vf_f64m1(vc, c_scale, vl);
      const vfloat64m1_t sum_ab = __riscv_vfadd_vv_f64m1(as, bs, vl);
      const vfloat64m1_t sum = __riscv_vfadd_vv_f64m1(sum_ab, cs, vl);
      __riscv_vse64_v_f64m1(out.data() + i, sum, vl);
      i += vl;
    }
    return out;
  }
#endif

  if (used_rvv)
    *used_rvv = false;
  return linear3_std(a, a_scale, b, b_scale, c, c_scale);
}

inline double
norm_candidate(const std::vector<double>& a, bool* used_rvv = nullptr)
{
  return std::sqrt(dot_candidate(a, a, used_rvv));
}

inline MoveSlopeResult
move_to_and_slope_candidate(const DirectionInput& input, const double alpha)
{
  ensure_same_size(input.x0, input.p, "move_to_and_slope_candidate x0/p size mismatch");
  ensure_same_size(input.gradient, input.p, "move_to_and_slope_candidate gradient/p size mismatch");

  bool used = false;
  MoveSlopeResult result;
  result.x_alpha = linear3_candidate(input.x0,
                                     1.0,
                                     input.p,
                                     alpha,
                                     input.p,
                                     0.0,
                                     &used);
  result.slope = dot_candidate(input.gradient, input.p, &used);
  result.used_rvv = used;
  return result;
}

inline DirectionResult
direction_update_candidate(const DirectionInput& input)
{
  bool used = false;
  DirectionResult result;
  result.dx0 = sub_candidate(input.x, input.x0, &used);
  result.dg0 = sub_candidate(input.gradient, input.g0, &used);
  result.dxg = dot_candidate(result.dx0, input.gradient, &used);
  result.dgg = dot_candidate(result.dg0, input.gradient, &used);
  result.dxdg = dot_candidate(result.dx0, result.dg0, &used);
  result.dgnorm = norm_candidate(result.dg0, &used);

  if (result.dxdg != 0.0) {
    result.B = result.dxg / result.dxdg;
    result.A = -(1.0 + result.dgnorm * result.dgnorm / result.dxdg) * result.B +
               result.dgg / result.dxdg;
  }

  result.p = linear3_candidate(result.dx0,
                               -result.A,
                               input.gradient,
                               1.0,
                               result.dg0,
                               -result.B,
                               &used);
  result.g0norm = norm_candidate(input.gradient, &used);
  result.pnorm = norm_candidate(result.p, &used);

  if (result.pnorm != 0.0) {
    const double dir = (dot_candidate(result.p, input.gradient, &used) > 0.0) ? -1.0 : 1.0;
    result.p = scale_candidate(result.p, dir / result.pnorm, &used);
    result.pnorm = norm_candidate(result.p, &used);
    result.fp0 = dot_candidate(result.p, input.gradient, &used);
  }

  result.used_rvv = used;
  return result;
}

} // namespace pcl::registration::rvv_bfgs_support
