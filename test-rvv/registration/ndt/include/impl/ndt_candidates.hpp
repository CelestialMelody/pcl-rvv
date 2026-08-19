/*
 * 本文件做什么：
 * 提供 NDT derivative accumulation（导数累加）的 test-only RVV 诊断候选。
 * `__RVV10__` 打开时，候选跨样本批量计算 c_inv*x、x^T*c_inv*x、
 * gradient（梯度）和 hessian（海森矩阵）累加。Phase 020 额外加入
 * topic-local double `exp` RVV prototype（主题本地 double exp 原型）：
 * 它只服务本诊断候选，用来隔离 `std::exp` 是否是阶段 000 负向结果的主因。
 * 当前仓库的 `rvv_math.hpp` 已有 float `expf_RVV_f32m2` helper，但 NDT
 * 这段公式使用 double；本文件不把该原型声明成公共 helper。
 *
 * 证据边界：
 * 这里证明的是“如果 production 能安全 staged（分阶段暂存）出同构样本，
 * 导数累加数学核是否值得 RVV 化”。它不证明真实 NDT public entry 已经获得收益，
 * 也不覆盖 neighborhood search（邻域搜索）和 line search（线搜索）。
 */

#pragma once

#include "ndt_references.hpp"

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

#include <algorithm>
#include <cstdint>
#include <vector>

namespace pcl::registration::rvv_ndt_support {

#ifdef __RVV10__
namespace detail {

// 这个 helper 是 finite-domain fast approximation（有限输入域快速近似），
// 不是 strict libm replacement（严格 libm 替换）。它只覆盖 NDT Gaussian
// 权重里的 x = -gauss_d2*q/2 诊断域，并把域外输入夹到 [-32, 0]。
inline vfloat64m1_t
exp_finite_domain_RVV_f64m1(const vfloat64m1_t& x, const std::size_t vl)
{
  constexpr double kLog2Inv = 1.44269504088896340735992468100189214;
  constexpr double kLog2Hi = 0.693147180559945286226763982995180413;
  constexpr double kLog2Lo = 2.31904681384629955841777185769994890e-17;
  constexpr double kXMin = -32.0;
  constexpr double kXMax = 0.0;

  vfloat64m1_t vx =
      __riscv_vfmin_vf_f64m1(__riscv_vfmax_vf_f64m1(x, kXMin, vl), kXMax, vl);

  vfloat64m1_t flt_n = __riscv_vfmul_vf_f64m1(vx, kLog2Inv, vl);
  vint64m1_t n = __riscv_vfcvt_x_f_v_i64m1(flt_n, vl);
  flt_n = __riscv_vfcvt_f_x_v_f64m1(n, vl);

  vfloat64m1_t r = __riscv_vfnmsub_vf_f64m1(flt_n, kLog2Hi, vx, vl);
  r = __riscv_vfnmsub_vf_f64m1(flt_n, kLog2Lo, r, vl);

  // Horner 多项式使用 exp(r) 的 13 阶 Taylor 系数。这里 r 被约化到
  // [-ln2/2, ln2/2]，double 精度下足够支撑 NDT caller 现有误差门槛；
  // 若后续要进入 common helper，仍必须重新走数学专项系数和特殊值审查。
  vfloat64m1_t poly = __riscv_vfmv_v_f_f64m1(1.60590438368216145994e-10, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(2.08767569878680989792e-9, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(2.50521083854417187751e-8, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(2.75573192239858906526e-7, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(2.75573192239858925110e-6, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(2.48015873015873015660e-5, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(1.98412698412698412535e-4, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(1.38888888888888894189e-3, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(8.33333333333333321769e-3, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(4.16666666666666643537e-2, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(1.66666666666666657415e-1, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(5.00000000000000000000e-1, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(1.00000000000000000000e+0, vl), r, poly, vl);
  const vfloat64m1_t exp_r = __riscv_vfmacc_vv_f64m1(
      __riscv_vfmv_v_f_f64m1(1.00000000000000000000e+0, vl), r, poly, vl);

  const vint64m1_t exp_offset = __riscv_vadd_vx_i64m1(n, 1023, vl);
  const vuint64m1_t exp_bits =
      __riscv_vsll_vx_u64m1(__riscv_vreinterpret_v_i64m1_u64m1(exp_offset), 52, vl);
  const vfloat64m1_t two_n = __riscv_vreinterpret_v_u64m1_f64m1(exp_bits);
  return __riscv_vfmul_vv_f64m1(exp_r, two_n, vl);
}

} // namespace detail
#endif

inline DerivativeResult
derivative_accumulate_candidate(const DerivativeBatch& batch)
{
#ifndef __RVV10__
  return derivative_accumulate_std(batch);
#else
  DerivativeResult result;
  const std::size_t n = batch.size();
  result.samples = n;
  if (n == 0)
    return result;

  std::vector<double> scaled_weight(n, 0.0);
  std::vector<double> xdot_cov_j(kTransformDof * n, 0.0);

  for (std::size_t offset = 0; offset < n;) {
    const std::size_t vl = __riscv_vsetvl_e64m1(n - offset);
    const vfloat64m1_t vx0 = __riscv_vle64_v_f64m1(batch.x0.data() + offset, vl);
    const vfloat64m1_t vx1 = __riscv_vle64_v_f64m1(batch.x1.data() + offset, vl);
    const vfloat64m1_t vx2 = __riscv_vle64_v_f64m1(batch.x2.data() + offset, vl);

    vfloat64m1_t cx0 =
        __riscv_vfmul_vv_f64m1(__riscv_vle64_v_f64m1(batch.c_inv[0].data() + offset, vl),
                               vx0,
                               vl);
    cx0 = __riscv_vfmacc_vv_f64m1(
        cx0, __riscv_vle64_v_f64m1(batch.c_inv[1].data() + offset, vl), vx1, vl);
    cx0 = __riscv_vfmacc_vv_f64m1(
        cx0, __riscv_vle64_v_f64m1(batch.c_inv[2].data() + offset, vl), vx2, vl);

    vfloat64m1_t cx1 =
        __riscv_vfmul_vv_f64m1(__riscv_vle64_v_f64m1(batch.c_inv[3].data() + offset, vl),
                               vx0,
                               vl);
    cx1 = __riscv_vfmacc_vv_f64m1(
        cx1, __riscv_vle64_v_f64m1(batch.c_inv[4].data() + offset, vl), vx1, vl);
    cx1 = __riscv_vfmacc_vv_f64m1(
        cx1, __riscv_vle64_v_f64m1(batch.c_inv[5].data() + offset, vl), vx2, vl);

    vfloat64m1_t cx2 =
        __riscv_vfmul_vv_f64m1(__riscv_vle64_v_f64m1(batch.c_inv[6].data() + offset, vl),
                               vx0,
                               vl);
    cx2 = __riscv_vfmacc_vv_f64m1(
        cx2, __riscv_vle64_v_f64m1(batch.c_inv[7].data() + offset, vl), vx1, vl);
    cx2 = __riscv_vfmacc_vv_f64m1(
        cx2, __riscv_vle64_v_f64m1(batch.c_inv[8].data() + offset, vl), vx2, vl);

    vfloat64m1_t q = __riscv_vfmul_vv_f64m1(vx0, cx0, vl);
    q = __riscv_vfmacc_vv_f64m1(q, vx1, cx1, vl);
    q = __riscv_vfmacc_vv_f64m1(q, vx2, cx2, vl);

    const vfloat64m1_t exp_arg = __riscv_vfmul_vf_f64m1(q, -batch.gauss_d2 / 2.0, vl);
    const vfloat64m1_t exp_values = detail::exp_finite_domain_RVV_f64m1(exp_arg, vl);

    std::vector<double> exp_buf(vl);
    __riscv_vse64_v_f64m1(exp_buf.data(), exp_values, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t sample = offset + lane;
      const double exp_value = exp_buf[lane];
      double e_x_cov_x = batch.gauss_d2 * exp_value;
      if (e_x_cov_x <= 1.0 && e_x_cov_x >= 0.0 && !std::isnan(e_x_cov_x)) {
        result.score += -batch.gauss_d1 * exp_value;
        scaled_weight[sample] = e_x_cov_x * batch.gauss_d1;
      }
    }
    offset += vl;
  }

  auto reduce_sum = [](vfloat64m1_t value, std::size_t vl) {
    const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vl);
    const vfloat64m1_t sum = __riscv_vfredusum_vs_f64m1_f64m1(value, zero, vl);
    return __riscv_vfmv_f_s_f64m1_f64(sum);
  };

  for (int col = 0; col < kTransformDof; ++col) {
    double grad_sum = 0.0;
    for (std::size_t offset = 0; offset < n;) {
      const std::size_t vl = __riscv_vsetvl_e64m1(n - offset);
      const vfloat64m1_t vx0 = __riscv_vle64_v_f64m1(batch.x0.data() + offset, vl);
      const vfloat64m1_t vx1 = __riscv_vle64_v_f64m1(batch.x1.data() + offset, vl);
      const vfloat64m1_t vx2 = __riscv_vle64_v_f64m1(batch.x2.data() + offset, vl);
      const vfloat64m1_t j0 =
          __riscv_vle64_v_f64m1(batch.jacobian[jac_index(0, col)].data() + offset, vl);
      const vfloat64m1_t j1 =
          __riscv_vle64_v_f64m1(batch.jacobian[jac_index(1, col)].data() + offset, vl);
      const vfloat64m1_t j2 =
          __riscv_vle64_v_f64m1(batch.jacobian[jac_index(2, col)].data() + offset, vl);

      vfloat64m1_t cov0 = __riscv_vfmul_vv_f64m1(
          __riscv_vle64_v_f64m1(batch.c_inv[0].data() + offset, vl), j0, vl);
      cov0 = __riscv_vfmacc_vv_f64m1(
          cov0, __riscv_vle64_v_f64m1(batch.c_inv[1].data() + offset, vl), j1, vl);
      cov0 = __riscv_vfmacc_vv_f64m1(
          cov0, __riscv_vle64_v_f64m1(batch.c_inv[2].data() + offset, vl), j2, vl);

      vfloat64m1_t cov1 = __riscv_vfmul_vv_f64m1(
          __riscv_vle64_v_f64m1(batch.c_inv[3].data() + offset, vl), j0, vl);
      cov1 = __riscv_vfmacc_vv_f64m1(
          cov1, __riscv_vle64_v_f64m1(batch.c_inv[4].data() + offset, vl), j1, vl);
      cov1 = __riscv_vfmacc_vv_f64m1(
          cov1, __riscv_vle64_v_f64m1(batch.c_inv[5].data() + offset, vl), j2, vl);

      vfloat64m1_t cov2 = __riscv_vfmul_vv_f64m1(
          __riscv_vle64_v_f64m1(batch.c_inv[6].data() + offset, vl), j0, vl);
      cov2 = __riscv_vfmacc_vv_f64m1(
          cov2, __riscv_vle64_v_f64m1(batch.c_inv[7].data() + offset, vl), j1, vl);
      cov2 = __riscv_vfmacc_vv_f64m1(
          cov2, __riscv_vle64_v_f64m1(batch.c_inv[8].data() + offset, vl), j2, vl);

      vfloat64m1_t xdot = __riscv_vfmul_vv_f64m1(vx0, cov0, vl);
      xdot = __riscv_vfmacc_vv_f64m1(xdot, vx1, cov1, vl);
      xdot = __riscv_vfmacc_vv_f64m1(xdot, vx2, cov2, vl);
      __riscv_vse64_v_f64m1(xdot_cov_j.data() + static_cast<std::size_t>(col) * n + offset,
                            xdot,
                            vl);

      const vfloat64m1_t weight =
          __riscv_vle64_v_f64m1(scaled_weight.data() + offset, vl);
      grad_sum += reduce_sum(__riscv_vfmul_vv_f64m1(xdot, weight, vl), vl);
      offset += vl;
    }
    result.gradient[col] = grad_sum;
  }

  if (batch.compute_hessian) {
    for (int i = 0; i < kTransformDof; ++i) {
      for (int j = 0; j < kTransformDof; ++j) {
        double hessian_sum = 0.0;
        for (std::size_t offset = 0; offset < n;) {
          const std::size_t vl = __riscv_vsetvl_e64m1(n - offset);
          const vfloat64m1_t vx0 = __riscv_vle64_v_f64m1(batch.x0.data() + offset, vl);
          const vfloat64m1_t vx1 = __riscv_vle64_v_f64m1(batch.x1.data() + offset, vl);
          const vfloat64m1_t vx2 = __riscv_vle64_v_f64m1(batch.x2.data() + offset, vl);
          const vfloat64m1_t weight =
              __riscv_vle64_v_f64m1(scaled_weight.data() + offset, vl);
          const vfloat64m1_t xdot_i =
              __riscv_vle64_v_f64m1(xdot_cov_j.data() + static_cast<std::size_t>(i) * n +
                                        offset,
                                    vl);
          const vfloat64m1_t xdot_j =
              __riscv_vle64_v_f64m1(xdot_cov_j.data() + static_cast<std::size_t>(j) * n +
                                        offset,
                                    vl);

          const vfloat64m1_t h0 = __riscv_vle64_v_f64m1(
              batch.point_hessian[hess_index(3 * i + 0, j)].data() + offset, vl);
          const vfloat64m1_t h1 = __riscv_vle64_v_f64m1(
              batch.point_hessian[hess_index(3 * i + 1, j)].data() + offset, vl);
          const vfloat64m1_t h2 = __riscv_vle64_v_f64m1(
              batch.point_hessian[hess_index(3 * i + 2, j)].data() + offset, vl);

          vfloat64m1_t ch0 = __riscv_vfmul_vv_f64m1(
              __riscv_vle64_v_f64m1(batch.c_inv[0].data() + offset, vl), h0, vl);
          ch0 = __riscv_vfmacc_vv_f64m1(
              ch0, __riscv_vle64_v_f64m1(batch.c_inv[1].data() + offset, vl), h1, vl);
          ch0 = __riscv_vfmacc_vv_f64m1(
              ch0, __riscv_vle64_v_f64m1(batch.c_inv[2].data() + offset, vl), h2, vl);
          vfloat64m1_t ch1 = __riscv_vfmul_vv_f64m1(
              __riscv_vle64_v_f64m1(batch.c_inv[3].data() + offset, vl), h0, vl);
          ch1 = __riscv_vfmacc_vv_f64m1(
              ch1, __riscv_vle64_v_f64m1(batch.c_inv[4].data() + offset, vl), h1, vl);
          ch1 = __riscv_vfmacc_vv_f64m1(
              ch1, __riscv_vle64_v_f64m1(batch.c_inv[5].data() + offset, vl), h2, vl);
          vfloat64m1_t ch2 = __riscv_vfmul_vv_f64m1(
              __riscv_vle64_v_f64m1(batch.c_inv[6].data() + offset, vl), h0, vl);
          ch2 = __riscv_vfmacc_vv_f64m1(
              ch2, __riscv_vle64_v_f64m1(batch.c_inv[7].data() + offset, vl), h1, vl);
          ch2 = __riscv_vfmacc_vv_f64m1(
              ch2, __riscv_vle64_v_f64m1(batch.c_inv[8].data() + offset, vl), h2, vl);

          vfloat64m1_t xdot_c_hess = __riscv_vfmul_vv_f64m1(vx0, ch0, vl);
          xdot_c_hess = __riscv_vfmacc_vv_f64m1(xdot_c_hess, vx1, ch1, vl);
          xdot_c_hess = __riscv_vfmacc_vv_f64m1(xdot_c_hess, vx2, ch2, vl);

          double cov_i0 = 0.0;
          double cov_i1 = 0.0;
          double cov_i2 = 0.0;
          vfloat64m1_t covi0;
          vfloat64m1_t covi1;
          vfloat64m1_t covi2;
          {
            const vfloat64m1_t ji0 = __riscv_vle64_v_f64m1(
                batch.jacobian[jac_index(0, i)].data() + offset, vl);
            const vfloat64m1_t ji1 = __riscv_vle64_v_f64m1(
                batch.jacobian[jac_index(1, i)].data() + offset, vl);
            const vfloat64m1_t ji2 = __riscv_vle64_v_f64m1(
                batch.jacobian[jac_index(2, i)].data() + offset, vl);
            covi0 = __riscv_vfmul_vv_f64m1(
                __riscv_vle64_v_f64m1(batch.c_inv[0].data() + offset, vl), ji0, vl);
            covi0 = __riscv_vfmacc_vv_f64m1(
                covi0, __riscv_vle64_v_f64m1(batch.c_inv[1].data() + offset, vl), ji1, vl);
            covi0 = __riscv_vfmacc_vv_f64m1(
                covi0, __riscv_vle64_v_f64m1(batch.c_inv[2].data() + offset, vl), ji2, vl);
            covi1 = __riscv_vfmul_vv_f64m1(
                __riscv_vle64_v_f64m1(batch.c_inv[3].data() + offset, vl), ji0, vl);
            covi1 = __riscv_vfmacc_vv_f64m1(
                covi1, __riscv_vle64_v_f64m1(batch.c_inv[4].data() + offset, vl), ji1, vl);
            covi1 = __riscv_vfmacc_vv_f64m1(
                covi1, __riscv_vle64_v_f64m1(batch.c_inv[5].data() + offset, vl), ji2, vl);
            covi2 = __riscv_vfmul_vv_f64m1(
                __riscv_vle64_v_f64m1(batch.c_inv[6].data() + offset, vl), ji0, vl);
            covi2 = __riscv_vfmacc_vv_f64m1(
                covi2, __riscv_vle64_v_f64m1(batch.c_inv[7].data() + offset, vl), ji1, vl);
            covi2 = __riscv_vfmacc_vv_f64m1(
                covi2, __riscv_vle64_v_f64m1(batch.c_inv[8].data() + offset, vl), ji2, vl);
            (void)cov_i0;
            (void)cov_i1;
            (void)cov_i2;
          }

          const vfloat64m1_t jj0 = __riscv_vle64_v_f64m1(
              batch.jacobian[jac_index(0, j)].data() + offset, vl);
          const vfloat64m1_t jj1 = __riscv_vle64_v_f64m1(
              batch.jacobian[jac_index(1, j)].data() + offset, vl);
          const vfloat64m1_t jj2 = __riscv_vle64_v_f64m1(
              batch.jacobian[jac_index(2, j)].data() + offset, vl);
          vfloat64m1_t jacj_dot_covi = __riscv_vfmul_vv_f64m1(jj0, covi0, vl);
          jacj_dot_covi = __riscv_vfmacc_vv_f64m1(jacj_dot_covi, jj1, covi1, vl);
          jacj_dot_covi = __riscv_vfmacc_vv_f64m1(jacj_dot_covi, jj2, covi2, vl);

          vfloat64m1_t term =
              __riscv_vfmul_vf_f64m1(__riscv_vfmul_vv_f64m1(xdot_i, xdot_j, vl),
                                     -batch.gauss_d2,
                                     vl);
          term = __riscv_vfadd_vv_f64m1(term, xdot_c_hess, vl);
          term = __riscv_vfadd_vv_f64m1(term, jacj_dot_covi, vl);
          hessian_sum += reduce_sum(__riscv_vfmul_vv_f64m1(term, weight, vl), vl);
          offset += vl;
        }
        result.hessian[i * kTransformDof + j] = hessian_sum;
      }
    }
  }
  result.used_rvv = true;
  return result;
#endif
}

} // namespace pcl::registration::rvv_ndt_support
