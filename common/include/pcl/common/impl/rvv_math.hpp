/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *
 *  Copyright (c) 2026
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */


#pragma once

#if defined(__RVV10__)

#include <cmath>
#include <cstddef>
#include <limits>
#include <riscv_vector.h>

namespace pcl
{

inline vfloat32m2_t
acos_RVV_f32m2 (const vfloat32m2_t& x, const std::size_t vl)
{
  // acos(x) ~= sqrt(1 - x) * Q(1 - x), deg5 remez2 from parms_acos.py.
  const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
  vfloat32m2_t u = __riscv_vfsub_vv_f32m2 (one, x, vl);
  u = __riscv_vfmax_vf_f32m2 (u, 0.0f, vl);

  vfloat32m2_t q = __riscv_vfmv_v_f_f32m2 (0.004346735271181379f, vl);
  q = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (-0.002360310714948563f, vl), u, q, vl);
  q = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (0.01095480727067022f, vl), u, q, vl);
  q = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (0.02571508511147162f, vl), u, q, vl);
  q = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (0.117926522053977f, vl), u, q, vl);
  q = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (1.414212408248559f, vl), u, q, vl);

  const vfloat32m2_t sqrt_u = __riscv_vfsqrt_v_f32m2 (u, vl);
  return __riscv_vfmul_vv_f32m2 (sqrt_u, q, vl);
}

inline vfloat32m2_t
getAcuteAngle3DRVV_f32m2 (const vfloat32m2_t& x1, const vfloat32m2_t& y1, const vfloat32m2_t& z1,
                        const vfloat32m2_t& x2, const vfloat32m2_t& y2, const vfloat32m2_t& z2, const std::size_t vl)
{
  // dot = x1*x2 + y1*y2 + z1*z2
  const vfloat32m2_t dot = __riscv_vfmacc_vv_f32m2 (
      __riscv_vfmacc_vv_f32m2 (
          __riscv_vfmul_vv_f32m2 (x1, x2, vl),
          y1, y2, vl),
      z1, z2, vl);

  // Compute Absolute Value
  // Use vfsgnjx (Floating-point Sign Injection - XOR) with itself.
  const vfloat32m2_t dot_abs = __riscv_vfsgnjx_vv_f32m2 (dot, dot, vl);

  // Clamp to [0, 1]
  const vfloat32m2_t dot_clamped = __riscv_vfmin_vf_f32m2 (dot_abs, 1.0f, vl);

  return acos_RVV_f32m2 (dot_clamped, vl);
}

inline vfloat32m2_t
atan2_RVV_f32m2 (const vfloat32m2_t& y, const vfloat32m2_t& x, const std::size_t vl)
{
  // Polynomial coefficients (Hastings-style, odd powers), ~0.01 deg max error
  // parms form https://mazzo.li/posts/vectorized-atan2.html
  const float a1 = 0.99997726f;
  const float a3 = -0.33262347f;
  const float a5 = 0.19354346f;
  const float a7 = -0.11643287f;
  const float a9 = 0.05265332f;
  const float a11 = -0.01172120f;
  const float pi = 3.14159265358979323846f;
  const float pi_2 = 1.57079632679489661923f;
  const float tiny = 1e-20f;

  const vfloat32m2_t abs_x = __riscv_vfsgnjx_vv_f32m2 (x, x, vl);
  const vfloat32m2_t abs_y = __riscv_vfsgnjx_vv_f32m2 (y, y, vl);
  // swap when |y| > |x|; vmerge(op1, op2, mask) => mask ? op2 : op1 (match atan2.cpp)
  const vbool16_t swap_mask = __riscv_vmflt_vv_f32m2_b16 (abs_x, abs_y, vl);
  const vfloat32m2_t num = __riscv_vmerge_vvm_f32m2 (y, x, swap_mask, vl);
  const vfloat32m2_t den = __riscv_vmerge_vvm_f32m2 (x, y, swap_mask, vl);
  // Preserve sign of den when clamping (scalar: den = (den>=0)? tiny : -tiny)
  const vfloat32m2_t abs_den = __riscv_vfsgnjx_vv_f32m2 (den, den, vl);
  const vfloat32m2_t den_safe = __riscv_vfsgnj_vv_f32m2 (
      __riscv_vfmax_vf_f32m2 (abs_den, tiny, vl), den, vl);
  vfloat32m2_t atan_input = __riscv_vfdiv_vv_f32m2 (num, den_safe, vl);
  // Clamp to [-1,1] so polynomial stays valid (avoids overflow from float noise)
  atan_input = __riscv_vfmin_vf_f32m2 (__riscv_vfmax_vf_f32m2 (atan_input, -1.0f, vl), 1.0f, vl);

  const vfloat32m2_t x2 = __riscv_vfmul_vv_f32m2 (atan_input, atan_input, vl);
  vfloat32m2_t p = __riscv_vfmv_v_f_f32m2 (a11, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (a9, vl), x2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (a7, vl), x2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (a5, vl), x2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (a3, vl), x2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (a1, vl), x2, p, vl);
  vfloat32m2_t result = __riscv_vfmul_vv_f32m2 (atan_input, p, vl);

  // When swapped: adj = +pi/2 when atan_input>=0 (same sign x,y), else -pi/2
  const vbool16_t atan_ge_zero = __riscv_vmfge_vf_f32m2_b16 (atan_input, 0.0f, vl);
  const vfloat32m2_t pi_2_vec = __riscv_vfmv_v_f_f32m2 (pi_2, vl);
  const vfloat32m2_t neg_pi_2 = __riscv_vfmv_v_f_f32m2 (-pi_2, vl);
  const vfloat32m2_t adj = __riscv_vmerge_vvm_f32m2 (neg_pi_2, pi_2_vec, atan_ge_zero, vl);
  result = __riscv_vmerge_vvm_f32m2 (result, __riscv_vfsub_vv_f32m2 (adj, result, vl), swap_mask, vl);

  const vbool16_t x_lt_zero = __riscv_vmflt_vf_f32m2_b16 (x, 0.0f, vl);
  const vbool16_t y_ge_zero = __riscv_vmfge_vf_f32m2_b16 (y, 0.0f, vl);
  const vfloat32m2_t pi_vec = __riscv_vfmv_v_f_f32m2 (pi, vl);
  const vfloat32m2_t neg_pi = __riscv_vfmv_v_f_f32m2 (-pi, vl);
  const vfloat32m2_t add_val = __riscv_vmerge_vvm_f32m2 (neg_pi, pi_vec, y_ge_zero, vl);
  result = __riscv_vmerge_vvm_f32m2 (result, __riscv_vfadd_vv_f32m2 (result, add_val, vl), x_lt_zero, vl);

  return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------------------------------------
// sincos_finite_domain_RVV_f32m2:
//   受限接入原型（integration prototype，仅验证接入形态，不接 production caller）。
//
// 合同:
//   - finite-domain fast approximation（有限输入域快速近似），不是 strict libm
//     replacement（严格 libm 替换）。
//   - 只接受有限 float 且 x in [-pi, pi]。
//   - domain-out（域外）/ NaN / Inf lane 合并为 quiet NaN，不静默 clamp。
//   - subnormal（次正规数）在合同域内，走 fast path。
//   - signed zero（带符号零）保持 sin(+0)=+0、sin(-0)=-0、cos(+-0)=+1。
//
// 分层:
//   - kernel（约化区间多项式核函数）只处理 r in [-pi/4, pi/4]。
//   - lane-level helper（单个 RVV 向量寄存器级 helper）负责 domain mask、
//     bounded mask range reduction（有限域 mask 分段约化）、hi/lo compensation
//     （高低位常量补偿）、象限 sign/swap 重构、signed-zero merge 和 NaN merge。
//   - 本函数不负责 load/store，不负责 strip-mining，不是 public API。
//
// 系数来源:
//   - test-rvv/rvv/math/sincos/script/parms_sincos.py 的 lp-abs-hi-lo 候选。
//   - 进入 production 前仍需保留参数脚本、专项测试、反汇编和板卡证据。
// -----------------------------------------------------------------------------
namespace {
  const float kSincosPi = 3.1415927410125732f;
  const float kSincosPi2 = 1.5707963705062866f;
  const float kSincosPi4 = 0.7853981852531433f;
  const float kSincos3Pi4 = 2.3561944961547852f;
  const float kSincosPiLo = -8.742277657347586e-08f;
  const float kSincosPi2Lo = -4.371138828673793e-08f;

  const float kSincosSinS0 = -1.666666716337204e-01f;
  const float kSincosSinS1 = 8.333330973982811e-03f;
  const float kSincosSinS2 = -1.983980037039146e-04f;
  const float kSincosSinS3 = 2.721804321481613e-06f;
  const float kSincosCosC0 = -5.000000000000000e-01f;
  const float kSincosCosC1 = 4.166663810610771e-02f;
  const float kSincosCosC2 = -1.388695789501071e-03f;
  const float kSincosCosC3 = 2.439828858769033e-05f;
}

/** \brief sin kernel（约化区间多项式核函数）：只接受 r in [-pi/4, pi/4]，不能当完整 sinf 使用。 */
inline vfloat32m2_t
sincos_finite_domain_sin_kernel_RVV_f32m2 (const vfloat32m2_t& r, const std::size_t vl)
{
  const vfloat32m2_t r2 = __riscv_vfmul_vv_f32m2 (r, r, vl);
  vfloat32m2_t p = __riscv_vfmv_v_f_f32m2 (kSincosSinS3, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kSincosSinS2, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kSincosSinS1, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kSincosSinS0, vl), r2, p, vl);
  const vfloat32m2_t r3 = __riscv_vfmul_vv_f32m2 (r, r2, vl);
  return __riscv_vfmacc_vv_f32m2 (r, r3, p, vl);
}

/** \brief cos kernel（约化区间多项式核函数）：只接受 r in [-pi/4, pi/4]，不能当完整 cosf 使用。 */
inline vfloat32m2_t
sincos_finite_domain_cos_kernel_RVV_f32m2 (const vfloat32m2_t& r, const std::size_t vl)
{
  const vfloat32m2_t r2 = __riscv_vfmul_vv_f32m2 (r, r, vl);
  vfloat32m2_t p = __riscv_vfmv_v_f_f32m2 (kSincosCosC3, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kSincosCosC2, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kSincosCosC1, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kSincosCosC0, vl), r2, p, vl);
  return __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (1.0f, vl), r2, p, vl);
}

/** \brief 有限输入域 paired sin/cos lane-level helper（单个 RVV 向量寄存器级 helper）。 */
inline void
sincos_finite_domain_RVV_f32m2 (const vfloat32m2_t& x, vfloat32m2_t& s, vfloat32m2_t& c, const std::size_t vl)
{
  const vbool16_t ge_lo = __riscv_vmfge_vf_f32m2_b16 (x, -kSincosPi, vl);
  const vbool16_t le_hi = __riscv_vmfle_vf_f32m2_b16 (x, kSincosPi, vl);
  const vbool16_t valid = __riscv_vmand_mm_b16 (ge_lo, le_hi, vl);

  const vbool16_t hi = __riscv_vmfgt_vf_f32m2_b16 (x, kSincos3Pi4, vl);
  const vbool16_t mid = __riscv_vmand_mm_b16 (
      __riscv_vmfgt_vf_f32m2_b16 (x, kSincosPi4, vl),
      __riscv_vmfle_vf_f32m2_b16 (x, kSincos3Pi4, vl), vl);
  const vbool16_t neg_hi = __riscv_vmflt_vf_f32m2_b16 (x, -kSincos3Pi4, vl);
  const vbool16_t neg_mid = __riscv_vmand_mm_b16 (
      __riscv_vmfge_vf_f32m2_b16 (x, -kSincos3Pi4, vl),
      __riscv_vmflt_vf_f32m2_b16 (x, -kSincosPi4, vl), vl);
  const vbool16_t outer = __riscv_vmor_mm_b16 (hi, neg_hi, vl);

  vfloat32m2_t r = x;
  vfloat32m2_t r_hi = __riscv_vfsub_vf_f32m2 (x, kSincosPi, vl);
  vfloat32m2_t r_mid = __riscv_vfsub_vf_f32m2 (x, kSincosPi2, vl);
  vfloat32m2_t r_neg_hi = __riscv_vfadd_vf_f32m2 (x, kSincosPi, vl);
  vfloat32m2_t r_neg_mid = __riscv_vfadd_vf_f32m2 (x, kSincosPi2, vl);
  r_hi = __riscv_vfsub_vf_f32m2 (r_hi, kSincosPiLo, vl);
  r_mid = __riscv_vfsub_vf_f32m2 (r_mid, kSincosPi2Lo, vl);
  r_neg_hi = __riscv_vfadd_vf_f32m2 (r_neg_hi, kSincosPiLo, vl);
  r_neg_mid = __riscv_vfadd_vf_f32m2 (r_neg_mid, kSincosPi2Lo, vl);

  r = __riscv_vmerge_vvm_f32m2 (r, r_hi, hi, vl);
  r = __riscv_vmerge_vvm_f32m2 (r, r_mid, mid, vl);
  r = __riscv_vmerge_vvm_f32m2 (r, r_neg_hi, neg_hi, vl);
  r = __riscv_vmerge_vvm_f32m2 (r, r_neg_mid, neg_mid, vl);

  const vfloat32m2_t sr = sincos_finite_domain_sin_kernel_RVV_f32m2 (r, vl);
  const vfloat32m2_t cr = sincos_finite_domain_cos_kernel_RVV_f32m2 (r, vl);
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2 (0.0f, vl);
  const vfloat32m2_t neg_sr = __riscv_vfsub_vv_f32m2 (zero, sr, vl);
  const vfloat32m2_t neg_cr = __riscv_vfsub_vv_f32m2 (zero, cr, vl);

  vfloat32m2_t ys = sr;
  vfloat32m2_t yc = cr;
  ys = __riscv_vmerge_vvm_f32m2 (ys, neg_sr, outer, vl);
  yc = __riscv_vmerge_vvm_f32m2 (yc, neg_cr, outer, vl);
  ys = __riscv_vmerge_vvm_f32m2 (ys, cr, mid, vl);
  yc = __riscv_vmerge_vvm_f32m2 (yc, neg_sr, mid, vl);
  ys = __riscv_vmerge_vvm_f32m2 (ys, neg_cr, neg_mid, vl);
  yc = __riscv_vmerge_vvm_f32m2 (yc, sr, neg_mid, vl);

  const vbool16_t zero_mask = __riscv_vmfeq_vf_f32m2_b16 (x, 0.0f, vl);
  const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
  ys = __riscv_vmerge_vvm_f32m2 (ys, x, zero_mask, vl);
  yc = __riscv_vmerge_vvm_f32m2 (yc, one, zero_mask, vl);

  const vfloat32m2_t qnan = __riscv_vfmv_v_f_f32m2 (std::numeric_limits<float>::quiet_NaN (), vl);
  s = __riscv_vmerge_vvm_f32m2 (qnan, ys, valid, vl);
  c = __riscv_vmerge_vvm_f32m2 (qnan, yc, valid, vl);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------------------------------------
// expf_RVV_f32m2: vectorized float exp using reduction → approximation → reconstruction
// -----------------------------------------------------------------------------
// 约化: x = n*ln2 + r, r ∈ [-ln2/2, ln2/2]. 逼近: exp(r) ≈ P(r) (Remez degree 7).
// 重构: exp(x) = 2^n * P(r). 2^n 用 IEEE754 指数位构造.
// 误差: run_expf_test 专项网格上相对 std::expf 最大相对误差约 2.3e-7.
// 说明: 本函数仍是有限输入域 fast approximation，不完整模拟 std::expf 特殊值语义.
// 系数见 test-rvv/common/common/script/parms_expf.py
//
// 2^n 计算说明:
//   - 约化后 n = round(x/ln2) 是整数，范围约 [-127, 128]（对应 x ∈ [-88, 88]）
//   - 2^n = 2^(n + 127) << 23，n ∈ [-127, 128]
//   - 2^-127 对应 IEEE754 非规格化数：exp=0, mantissa=2^22
// -----------------------------------------------------------------------------
namespace {
  const float kExpfLog2Inv  = 1.4426950408889634f;     // 1 / ln(2)
  const float kExpfLog2Hi   = 0.6931471824645996f;     // ln(2) 高精度部分
  const float kExpfLog2Lo   = -1.904654290582768e-09f; // ln(2) 低精度部分（补偿）
  const float kExpfXMax     = 88.0f;   // 输入上限（exp(88) ≈ 1.6e38，接近 float 上限）
  const float kExpfXMin     = -88.0f;  // 输入下限（exp(-88) ≈ 6e-39，接近 float 下界）
  // 2^-127 对应 IEEE754 非规格化数：exp=0, mantissa=2^22
  const float kExpfTwoToMinus127 = 5.877471754111438e-39f;
  // remez1-rel polynomial for exp(r) on [-ln(2)/2, ln(2)/2], degree 7.
  const float kExpfRemezC0  = 0.9999999999876557f;
  const float kExpfRemezC1  = 1.000000000027863f;
  const float kExpfRemezC2  = 0.5000000053614374f;
  const float kExpfRemezC3  = 0.16666666439294f;
  const float kExpfRemezC4  = 0.04166635362288752f;
  const float kExpfRemezC5  = 0.008333359419394903f;
  const float kExpfRemezC6  = 0.001394106053653905f;
  const float kExpfRemezC7  = 0.0001986611354469939f;
}

inline vfloat32m2_t
expf_RVV_f32m2 (const vfloat32m2_t& x, const std::size_t vl)
{
  // Clamp to avoid overflow/underflow
  vfloat32m2_t vx = __riscv_vfmin_vf_f32m2 (__riscv_vfmax_vf_f32m2 (x, kExpfXMin, vl), kExpfXMax, vl);
  // n = round(x / ln2), flt_n = (float)n
  vfloat32m2_t flt_n = __riscv_vfmul_vf_f32m2 (vx, kExpfLog2Inv, vl);
  vint32m2_t n = __riscv_vfcvt_x_f_v_i32m2 (flt_n, vl);
  flt_n = __riscv_vfcvt_f_x_v_f32m2 (n, vl);
  // r = x - n*log2_hi - n*log2_lo
  vfloat32m2_t r = __riscv_vfnmsub_vf_f32m2 (flt_n, kExpfLog2Hi, vx, vl);
  r = __riscv_vfnmsub_vf_f32m2 (flt_n, kExpfLog2Lo, r, vl);
  // Horner: poly = c7; poly = c6 + r*poly; ... ; exp_r = c0 + r*poly
  vfloat32m2_t poly = __riscv_vfmv_v_f_f32m2 (kExpfRemezC7, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC6, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC5, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC4, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC3, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC2, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC1, vl), r, poly, vl);
  vfloat32m2_t exp_r = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kExpfRemezC0, vl), r, poly, vl);
  // 位构造 2^n：normal 情况用 exp_offset<<23，n==-127 时补非规格化 2^-127。
  vint32m2_t exp_offset = __riscv_vadd_vx_i32m2 (n, 127, vl);
  exp_offset = __riscv_vmax_vx_i32m2 (exp_offset, 0, vl);
  exp_offset = __riscv_vmin_vx_i32m2 (exp_offset, 255, vl);
  vuint32m2_t res_bits =
      __riscv_vsll_vx_u32m2 (__riscv_vreinterpret_v_i32m2_u32m2 (exp_offset), 23, vl);
  vfloat32m2_t two_n_normal = __riscv_vreinterpret_v_u32m2_f32m2 (res_bits);
  const vbool16_t is_n_neg127 = __riscv_vmseq_vx_i32m2_b16 (n, -127, vl);
  vfloat32m2_t two_n_sub = __riscv_vfmv_v_f_f32m2 (kExpfTwoToMinus127, vl);
  vfloat32m2_t two_n = __riscv_vmerge_vvm_f32m2 (two_n_normal, two_n_sub, is_n_neg127, vl);
  return __riscv_vfmul_vv_f32m2 (exp_r, two_n, vl);  // exp(x) = 2^n * exp(r)
}

//////////////////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------------------------------------
// logf_RVV_f32m2: log(x) via 尾数 约化到 [1,2) + Remez log(1+u), u = m-1. 与 expf 同源的 ln(2) 分解.
// 系数见 test-rvv/common/common/script/parms_log1p.py
// -----------------------------------------------------------------------------
namespace {
  // ln(2) 拆分与 expf 中 kExpfLog2Hi/Lo 一致
  const float kLogfLog2Hi   = 0.6931471824645996f;
  const float kLogfLog2Lo   = -1.904654290582768e-09f;
  const float kLogfSubnormScale = 16777216.f; // 0x1p+24f，正次正规数乘此值变为正规数
  // 离散 L∞ (LP on grid) / parms_log1p.py --method lp, degree 7; float Horner 后 max |err| 优于此前往返约 1/2.5
  const float kLogfLog1pC0  = 1.8556080581e-07f;
  const float kLogfLog1pC1  = 9.9997340558e-01f;
  const float kLogfLog1pC2  = -4.9937887289e-01f;
  const float kLogfLog1pC3  = 3.2776673211e-01f;
  const float kLogfLog1pC4  = -2.2467442806e-01f;
  const float kLogfLog1pC5  = 1.3301016232e-01f;
  const float kLogfLog1pC6  = -5.4002504554e-02f;
  const float kLogfLog1pC7  = 1.0452683404e-02f;

/** 尾数约化后的核心 \c log 计算（不含 \c +0 / 负 / Inf / NaN 合并）；调用方用 \c 1.0f 填无效通道（见 \ref logf_RVV_f32m2 ）。 */
inline vfloat32m2_t
__core_logf_RVV_f32m2 (vfloat32m2_t x, const std::size_t vl)
{
  // 次正规：x * 2^24 正规化，指数补偿 -24
  const vuint32m2_t ix0 = __riscv_vreinterpret_v_f32m2_u32m2 (x);
  const vuint32m2_t e8_0 = __riscv_vand_vx_u32m2 (
      __riscv_vsrl_vx_u32m2 (ix0, 23, vl), 255, vl);
  const vuint32m2_t mbits0 = __riscv_vand_vx_u32m2 (ix0, 0x7fffffu, vl);
  const vbool16_t is_sub = __riscv_vmand_mm_b16 (
      __riscv_vmseq_vx_u32m2_b16 (e8_0, 0, vl),
      __riscv_vmsne_vx_u32m2_b16 (mbits0, 0, vl), vl);
  const vfloat32m2_t x_scaled = __riscv_vfmul_vf_f32m2 (x, kLogfSubnormScale, vl);
  x = __riscv_vmerge_vvm_f32m2 (x, x_scaled, is_sub, vl);
  const vuint32m2_t ix = __riscv_vreinterpret_v_f32m2_u32m2 (x);
  const vuint32m2_t e8 = __riscv_vand_vx_u32m2 (
      __riscv_vsrl_vx_u32m2 (ix, 23, vl), 255, vl);
  const vint32m2_t e_bias = __riscv_vreinterpret_v_u32m2_i32m2 (e8);
  vint32m2_t e = __riscv_vsub_vx_i32m2 (e_bias, 127, vl);
  const vint32m2_t e_sub = __riscv_vsub_vx_i32m2 (e, 24, vl);
  e = __riscv_vmerge_vvm_i32m2 (e, e_sub, is_sub, vl);
  // m = 尾数|隐式前导 1.0，u = m-1
  const vuint32m2_t mi = __riscv_vor_vx_u32m2 (
      __riscv_vand_vx_u32m2 (ix, 0x7fffffu, vl), 0x3f800000, vl);
  const vfloat32m2_t m = __riscv_vreinterpret_v_u32m2_f32m2 (mi);
  vfloat32m2_t u = __riscv_vfsub_vf_f32m2 (m, 1.0f, vl);
  // Horner: log(1+u)
  vfloat32m2_t poly = __riscv_vfmv_v_f_f32m2 (kLogfLog1pC7, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kLogfLog1pC6, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kLogfLog1pC5, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kLogfLog1pC4, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kLogfLog1pC3, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kLogfLog1pC2, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2 (__riscv_vfmv_v_f_f32m2 (kLogfLog1pC1, vl), u, poly, vl);
  const vfloat32m2_t log1p = __riscv_vfmacc_vv_f32m2 (
      __riscv_vfmv_v_f_f32m2 (kLogfLog1pC0, vl), u, poly, vl);
  vfloat32m2_t fe = __riscv_vfcvt_f_x_v_f32m2 (e, vl);
  vfloat32m2_t r = __riscv_vfmv_v_f_f32m2 (0.f, vl);
  r = __riscv_vfmacc_vf_f32m2 (r, kLogfLog2Hi, fe, vl);
  r = __riscv_vfmacc_vf_f32m2 (r, kLogfLog2Lo, fe, vl);
  return __riscv_vfadd_vv_f32m2 (r, log1p, vl);
}
}

inline vfloat32m2_t
logf_RVV_f32m2 (const vfloat32m2_t& x, const std::size_t vl)
{
  const vfloat32m2_t v_one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
  const vuint32m2_t ixu = __riscv_vreinterpret_v_f32m2_u32m2 (x);
  const vbool16_t m_pos = __riscv_vmfgt_vf_f32m2_b16 (x, 0.0f, vl);
  const vuint32m2_t absi = __riscv_vand_vx_u32m2 (ixu, 0x7fffffffu, vl);
  const vbool16_t m_nzero = __riscv_vmsne_vx_u32m2_b16 (absi, 0, vl);
  const vuint32m2_t e8 = __riscv_vand_vx_u32m2 (
      __riscv_vsrl_vx_u32m2 (ixu, 23, vl), 255, vl);
  const vbool16_t m_finite = __riscv_vmsne_vx_u32m2_b16 (e8, 255, vl);
  vbool16_t m_ok = __riscv_vmand_mm_b16 (m_pos, m_nzero, vl);
  m_ok = __riscv_vmand_mm_b16 (m_ok, m_finite, vl);
  const vfloat32m2_t x_use = __riscv_vmerge_vvm_f32m2 (v_one, x, m_ok, vl);
  vfloat32m2_t y = __core_logf_RVV_f32m2 (x_use, vl);
  // log(+0) = -inf
  const vbool16_t m_zero = __riscv_vmseq_vx_u32m2_b16 (absi, 0, vl);
  y = __riscv_vmerge_vvm_f32m2 (y, __riscv_vfmv_v_f_f32m2 (-std::numeric_limits<float>::infinity (), vl), m_zero, vl);
  // log(负) = qNaN
  vbool16_t m_neg = __riscv_vmflt_vf_f32m2_b16 (x, 0.0f, vl);
  y = __riscv_vmerge_vvm_f32m2 (y, __riscv_vfmv_v_f_f32m2 (std::nanf (""), vl), m_neg, vl);
  // log(+inf) = +inf
  const vfloat32m2_t v_inf = __riscv_vfmv_v_f_f32m2 (std::numeric_limits<float>::infinity (), vl);
  const vbool16_t m_pinf = __riscv_vmfeq_vv_f32m2_b16 (x, v_inf, vl);
  y = __riscv_vmerge_vvm_f32m2 (y, v_inf, m_pinf, vl);
  // 保留 NaN
  const vbool16_t m_nan = __riscv_vmfne_vv_f32m2_b16 (x, x, vl);
  y = __riscv_vmerge_vvm_f32m2 (y, x, m_nan, vl);
  return y;
}

} // namespace pcl

#endif // defined(__RVV10__)
