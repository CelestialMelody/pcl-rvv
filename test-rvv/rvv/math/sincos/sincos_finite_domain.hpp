/*
 * sincos_finite_domain.hpp - 仅测试使用的有限输入域 sinf/cosf 实验 helper。
 *
 * 阅读提示：
 *   - 本文件只给 test-rvv 使用，不安装，也不新增 public API（公开 API）。
 *   - finite-domain fast approximation（有限输入域快速近似）：合同只覆盖
 *     finite x in [-pi, pi]，不是 strict libm replacement（严格 libm 替换）。
 *   - kernel（约化区间多项式核函数）只处理 r in [-pi/4, pi/4]，不能当成
 *     完整的 sin/cos helper。
 *   - scalar same-chain（标量同构链路）用于和 RVV 做逐步对拍，不是 libm
 *     参考链路。
 *   - lane-level helper（单个 RVV 向量寄存器级 helper）固定使用 lp-abs-hi-lo
 *     候选，并通过 pcl::sincos_finite_domain_RVV_f32m2 进入 rvv_math.hpp
 *     里的受限接入原型。
 *   - batch wrapper（批量包装层）只处理 std::vector 的 load/store 与
 *     strip-mining（分段处理），不属于未来 rvv_math.hpp 核心入口。
 *   - experimental path（实验候选路径）只用于 Taylor/LP 对比，不进入
 *     接近生产形态的 lane-level 入口。
 *
 * 验收条件：失败会返回非 0，具体见 sincos_test.cpp 和
 * sincos_range_smoke.cpp。
 */
#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <riscv_vector.h>
#endif

namespace sincos_scratch {

struct SinCosPair {
  float s;
  float c;
};

struct KernelCoeffs {
  const char* label;
  float sin_s0;
  float sin_s1;
  float sin_s2;
  float sin_s3;
  float cos_c0;
  float cos_c1;
  float cos_c2;
  float cos_c3;
};

struct ApproxConfig {
  const char* label;
  const KernelCoeffs* coeffs;
  bool use_hi_lo_reduction;
};

// float32 常量写成已量化的数值，便于和参数脚本、C++、RVV 反汇编对照。
// low part（低位补偿常量）只用于 hi/lo range reduction（高低位约化）。
inline constexpr float kPi = 3.1415927410125732f;
inline constexpr float kPi2 = 1.5707963705062866f;
inline constexpr float kPi4 = 0.7853981852531433f;
inline constexpr float k3Pi4 = 2.3561944961547852f;
inline constexpr float kPiLo = -8.742277657347586e-08f;
inline constexpr float kPi2Lo = -4.371138828673793e-08f;

inline constexpr KernelCoeffs kTaylorCoeffs = {
    "taylor",
    -1.666666716337204e-01f,
    8.333333767950535e-03f,
    -1.984126984840259e-04f,
    2.755731884462875e-06f,
    -5.000000000000000e-01f,
    4.166666790843010e-02f,
    -1.388888922519982e-03f,
    2.480158764228690e-05f,
};

inline constexpr KernelCoeffs kLpAbsCoeffs = {
    "lp-abs",
    -1.666666716337204e-01f,
    8.333330973982811e-03f,
    -1.983980037039146e-04f,
    2.721804321481613e-06f,
    -5.000000000000000e-01f,
    4.166663810610771e-02f,
    -1.388695789501071e-03f,
    2.439828858769033e-05f,
};

inline constexpr ApproxConfig kConfigs[] = {
    {"taylor", &kTaylorCoeffs, false},
    {"taylor-hi-lo", &kTaylorCoeffs, true},
    {"lp-abs", &kLpAbsCoeffs, false},
    {"lp-abs-hi-lo", &kLpAbsCoeffs, true},
};

inline constexpr ApproxConfig kLpAbsHiLoConfig = {"lp-abs-hi-lo", &kLpAbsCoeffs, true};

// 合同谓词：测试、smoke（冒烟测试）和 helper 共用。它故意只覆盖
// finite-domain（有限输入域），任意有限周期输入不在本 helper 合同内。
inline bool
in_contract_domain(float x)
{
  return std::isfinite(x) && x >= kPi * -1.0f && x <= kPi;
}

// sin kernel（约化区间多项式核函数）。调用方必须先保证 r in
// [-pi/4, pi/4]；它不是完整 sinf 近似。调用者：scalar_sincos_finite_domain
// 和实验 RVV 执行链路。类别：数学专项测试 / 标量同构链路。
inline float
sin_kernel_scalar(float r, const KernelCoeffs& coeffs)
{
  const float r2 = r * r;
  float p = coeffs.sin_s3;
  p = std::fmaf(r2, p, coeffs.sin_s2);
  p = std::fmaf(r2, p, coeffs.sin_s1);
  p = std::fmaf(r2, p, coeffs.sin_s0);
  return std::fmaf(r * r2, p, r);
}

// cos kernel（约化区间多项式核函数）。调用方必须先保证 r in
// [-pi/4, pi/4]；它不是完整 cosf 近似。调用者：scalar_sincos_finite_domain
// 和实验 RVV 执行链路。类别：数学专项测试 / 标量同构链路。
inline float
cos_kernel_scalar(float r, const KernelCoeffs& coeffs)
{
  const float r2 = r * r;
  float p = coeffs.cos_c3;
  p = std::fmaf(r2, p, coeffs.cos_c2);
  p = std::fmaf(r2, p, coeffs.cos_c1);
  p = std::fmaf(r2, p, coeffs.cos_c0);
  return std::fmaf(r2, p, 1.0f);
}

inline float
sub_pi(float x, bool use_hi_lo)
{
  if (!use_hi_lo)
    return x - kPi;
  const float r = x - kPi;
  return r - kPiLo;
}

// 标量 range-reduction（范围约化）小函数。它们镜像 RVV add/sub 顺序，
// 让同构链路对拍比较算法本身，而不是比较一个更精确的标量捷径。
inline float
add_pi(float x, bool use_hi_lo)
{
  if (!use_hi_lo)
    return x + kPi;
  const float r = x + kPi;
  return r + kPiLo;
}

inline float
sub_pi2(float x, bool use_hi_lo)
{
  if (!use_hi_lo)
    return x - kPi2;
  const float r = x - kPi2;
  return r - kPi2Lo;
}

inline float
add_pi2(float x, bool use_hi_lo)
{
  if (!use_hi_lo)
    return x + kPi2;
  const float r = x + kPi2;
  return r + kPi2Lo;
}

// 完整的标量同构链路 helper。作用：覆盖有限域合同、
// bounded mask range reduction（有限域 mask 分段约化）、hi/lo compensation
//（高低位常量补偿）、象限 sign/swap 重构、signed zero（带符号零）和
// domain-out NaN merge（域外 NaN 合并）。调用者：数学专项测试和调用方形态
// 冒烟测试。类别：标量/RVV 同构链路，不是 libm 参考链路。
inline SinCosPair
scalar_sincos_finite_domain(float x, const ApproxConfig& config)
{
  if (!in_contract_domain(x)) {
    const float qnan = std::numeric_limits<float>::quiet_NaN();
    return {qnan, qnan};
  }
  if (x == 0.0f)
    return {x, 1.0f};

  float r = x;
  enum class Mode {
    center,
    neg_sin_neg_cos,
    cos_neg_sin,
    neg_cos_sin,
  };
  Mode mode = Mode::center;

  if (x > k3Pi4) {
    r = sub_pi(x, config.use_hi_lo_reduction);
    mode = Mode::neg_sin_neg_cos;
  }
  else if (x > kPi4) {
    r = sub_pi2(x, config.use_hi_lo_reduction);
    mode = Mode::cos_neg_sin;
  }
  else if (x < -k3Pi4) {
    r = add_pi(x, config.use_hi_lo_reduction);
    mode = Mode::neg_sin_neg_cos;
  }
  else if (x < -kPi4) {
    r = add_pi2(x, config.use_hi_lo_reduction);
    mode = Mode::neg_cos_sin;
  }

  const float sr = sin_kernel_scalar(r, *config.coeffs);
  const float cr = cos_kernel_scalar(r, *config.coeffs);
  switch (mode) {
    case Mode::center:
      return {sr, cr};
    case Mode::neg_sin_neg_cos:
      return {-sr, -cr};
    case Mode::cos_neg_sin:
      return {cr, -sr};
    case Mode::neg_cos_sin:
      return {-cr, sr};
  }
  return {sr, cr};
}

inline float
scalar_sin_finite_domain(float x, const ApproxConfig& config)
{
  // 单函数包装。作用：让测试能以接近 sin(x) 的形式调用有限域 helper；
  // 调用者：需要单独 sin 的实验检查。类别：标量同构链路。
  return scalar_sincos_finite_domain(x, config).s;
}

inline float
scalar_cos_finite_domain(float x, const ApproxConfig& config)
{
  // 单函数包装。作用：让测试能以接近 cos(x) 的形式调用有限域 helper；
  // 调用者：需要单独 cos 的实验检查。类别：标量同构链路。
  return scalar_sincos_finite_domain(x, config).c;
}

inline void
scalar_sincos_finite_domain(
    const std::vector<float>& xs,
    std::vector<float>& out_s,
    std::vector<float>& out_c,
    const ApproxConfig& config)
{
  // 标量批量包装层。作用：只负责 std::vector 遍历，并调用
  // scalar_sincos_finite_domain；调用者：数学专项测试和调用方形态冒烟测试。
  // 类别：标量/RVV 同构链路。
  for (std::size_t i = 0; i < xs.size(); ++i) {
    const SinCosPair y = scalar_sincos_finite_domain(xs[i], config);
    out_s[i] = y.s;
    out_c[i] = y.c;
  }
}

#if defined(__RVV10__)
// RVV sin kernel（RVV 约化区间多项式核函数）。每个 lane（向量通道）都必须
// 已经是 r in [-pi/4, pi/4]；它不是完整 sinf_RVV helper。调用者：实验 RVV 执行链路。
inline vfloat32m2_t
sin_kernel_rvv(vfloat32m2_t r, const KernelCoeffs& coeffs, std::size_t vl)
{
  const vfloat32m2_t r2 = __riscv_vfmul_vv_f32m2(r, r, vl);
  vfloat32m2_t p = __riscv_vfmv_v_f_f32m2(coeffs.sin_s3, vl);
  p = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(coeffs.sin_s2, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(coeffs.sin_s1, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(coeffs.sin_s0, vl), r2, p, vl);
  const vfloat32m2_t r3 = __riscv_vfmul_vv_f32m2(r, r2, vl);
  return __riscv_vfmacc_vv_f32m2(r, r3, p, vl);
}

// RVV cos kernel（RVV 约化区间多项式核函数）。每个 lane（向量通道）都必须
// 已经是 r in [-pi/4, pi/4]；它不是完整 cosf_RVV helper。调用者：实验 RVV 执行链路。
inline vfloat32m2_t
cos_kernel_rvv(vfloat32m2_t r, const KernelCoeffs& coeffs, std::size_t vl)
{
  const vfloat32m2_t r2 = __riscv_vfmul_vv_f32m2(r, r, vl);
  vfloat32m2_t p = __riscv_vfmv_v_f_f32m2(coeffs.cos_c3, vl);
  p = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(coeffs.cos_c2, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(coeffs.cos_c1, vl), r2, p, vl);
  p = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(coeffs.cos_c0, vl), r2, p, vl);
  return __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(1.0f, vl), r2, p, vl);
}

// 实验用的 lane-level implementation（单个 RVV 向量寄存器级实现）。作用：保留
// Taylor/LP 多候选对比；调用者：rvv_sincos_finite_domain_experimental。
// 类别：数学专项测试 / 标量-RVV 同构链路。固定 lp-abs-hi-lo 受限接入原型
// 不走这里，而是调用 rvv_math.hpp 中的 pcl::sincos_finite_domain_RVV_f32m2。
inline void
sincos_finite_domain_RVV_f32m2_impl(const vfloat32m2_t& x,
                                    vfloat32m2_t& s,
                                    vfloat32m2_t& c,
                                    std::size_t vl,
                                    const KernelCoeffs& coeffs,
                                    bool use_hi_lo_reduction)
{
  // 输入域 mask：NaN 比较为 false，Inf 被边界比较排除。
  // 最后只合并合法通道，避免域外通道悄悄穿过多项式。
  const vbool16_t ge_lo = __riscv_vmfge_vf_f32m2_b16(x, -kPi, vl);
  const vbool16_t le_hi = __riscv_vmfle_vf_f32m2_b16(x, kPi, vl);
  const vbool16_t valid = __riscv_vmand_mm_b16(ge_lo, le_hi, vl);

  // bounded mask range reduction（有限域 mask 分段约化）。不使用
  // nearest(x*2/pi)、vfcvt_x_f 或 FRM/FCSR（浮点舍入模式/状态寄存器）。
  const vbool16_t hi = __riscv_vmfgt_vf_f32m2_b16(x, k3Pi4, vl);
  const vbool16_t mid = __riscv_vmand_mm_b16(
      __riscv_vmfgt_vf_f32m2_b16(x, kPi4, vl),
      __riscv_vmfle_vf_f32m2_b16(x, k3Pi4, vl),
      vl);
  const vbool16_t neg_hi = __riscv_vmflt_vf_f32m2_b16(x, -k3Pi4, vl);
  const vbool16_t neg_mid = __riscv_vmand_mm_b16(
      __riscv_vmfge_vf_f32m2_b16(x, -k3Pi4, vl),
      __riscv_vmflt_vf_f32m2_b16(x, -kPi4, vl),
      vl);
  const vbool16_t outer = __riscv_vmor_mm_b16(hi, neg_hi, vl);

  // 先算所有候选余项，再用 mask 选择。hi/lo add/sub 会在反汇编中出现，
  // 是 lp-abs-hi-lo 候选的关键路径。
  vfloat32m2_t r = x;
  vfloat32m2_t r_hi = __riscv_vfsub_vf_f32m2(x, kPi, vl);
  vfloat32m2_t r_mid = __riscv_vfsub_vf_f32m2(x, kPi2, vl);
  vfloat32m2_t r_neg_hi = __riscv_vfadd_vf_f32m2(x, kPi, vl);
  vfloat32m2_t r_neg_mid = __riscv_vfadd_vf_f32m2(x, kPi2, vl);
  if (use_hi_lo_reduction) {
    r_hi = __riscv_vfsub_vf_f32m2(r_hi, kPiLo, vl);
    r_mid = __riscv_vfsub_vf_f32m2(r_mid, kPi2Lo, vl);
    r_neg_hi = __riscv_vfadd_vf_f32m2(r_neg_hi, kPiLo, vl);
    r_neg_mid = __riscv_vfadd_vf_f32m2(r_neg_mid, kPi2Lo, vl);
  }
  r = __riscv_vmerge_vvm_f32m2(r, r_hi, hi, vl);
  r = __riscv_vmerge_vvm_f32m2(r, r_mid, mid, vl);
  r = __riscv_vmerge_vvm_f32m2(r, r_neg_hi, neg_hi, vl);
  r = __riscv_vmerge_vvm_f32m2(r, r_neg_mid, neg_mid, vl);

  const vfloat32m2_t sr = sin_kernel_rvv(r, coeffs, vl);
  const vfloat32m2_t cr = cos_kernel_rvv(r, coeffs, vl);
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
  const vfloat32m2_t neg_sr = __riscv_vfsub_vv_f32m2(zero, sr, vl);
  const vfloat32m2_t neg_cr = __riscv_vfsub_vv_f32m2(zero, cr, vl);

  vfloat32m2_t ys = sr;
  vfloat32m2_t yc = cr;
  // 象限重构使用 mask sign/swap，不涉及整数象限或 C/C++ 负数 % 语义。
  ys = __riscv_vmerge_vvm_f32m2(ys, neg_sr, outer, vl);
  yc = __riscv_vmerge_vvm_f32m2(yc, neg_cr, outer, vl);
  ys = __riscv_vmerge_vvm_f32m2(ys, cr, mid, vl);
  yc = __riscv_vmerge_vvm_f32m2(yc, neg_sr, mid, vl);
  ys = __riscv_vmerge_vvm_f32m2(ys, neg_cr, neg_mid, vl);
  yc = __riscv_vmerge_vvm_f32m2(yc, sr, neg_mid, vl);

  const vbool16_t zero_mask = __riscv_vmfeq_vf_f32m2_b16(x, 0.0f, vl);
  const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2(1.0f, vl);
  // 合并原始 x 到 sin lane，保留 sin(-0) 的符号位。
  ys = __riscv_vmerge_vvm_f32m2(ys, x, zero_mask, vl);
  yc = __riscv_vmerge_vvm_f32m2(yc, one, zero_mask, vl);

  const vfloat32m2_t qnan =
      __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::quiet_NaN(), vl);
  s = __riscv_vmerge_vvm_f32m2(qnan, ys, valid, vl);
  c = __riscv_vmerge_vvm_f32m2(qnan, yc, valid, vl);
}

// 固定候选的 lane-level helper（单个 RVV 向量寄存器级 helper）。作用：test-rvv
// 的 lp-abs-hi-lo 主路径通过 rvv_math.hpp 受限接入原型。调用者：
// rvv_sincos_finite_domain。类别：RVV 执行链路 / 接入原型验证。
inline void
sincos_finite_domain_RVV_f32m2(const vfloat32m2_t& x,
                               vfloat32m2_t& s,
                               vfloat32m2_t& c,
                               std::size_t vl)
{
  pcl::sincos_finite_domain_RVV_f32m2(x, s, c, vl);
}

// experimental lane-level path（实验候选路径）。作用：保留 Taylor/LP 切换；
// 调用者：rvv_sincos_finite_domain_experimental。类别：数学专项测试，不代表
// 接近生产形态的 helper 签名。
inline void
sincos_finite_domain_RVV_f32m2_experimental(const vfloat32m2_t& x,
                                            vfloat32m2_t& s,
                                            vfloat32m2_t& c,
                                            std::size_t vl,
                                            const ApproxConfig& config)
{
  sincos_finite_domain_RVV_f32m2_impl(x, s, c, vl, *config.coeffs, config.use_hi_lo_reduction);
}

// 批量包装层。作用：为 test-rvv 数组输入做 strip-mining
//（分段处理）和 load/store，然后调用固定 lane-level helper。调用者：
// sincos_test.cpp 与 sincos_range_smoke.cpp。类别：RVV 执行链路。
inline void
rvv_sincos_finite_domain(
    const std::vector<float>& xs,
    std::vector<float>& out_s,
    std::vector<float>& out_c)
{
  std::size_t j = 0;
  while (j < xs.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(xs.size() - j);
    const vfloat32m2_t x = __riscv_vle32_v_f32m2(xs.data() + j, vl);
    vfloat32m2_t s;
    vfloat32m2_t c;
    sincos_finite_domain_RVV_f32m2(x, s, c, vl);
    __riscv_vse32_v_f32m2(out_s.data() + j, s, vl);
    __riscv_vse32_v_f32m2(out_c.data() + j, c, vl);
    j += vl;
  }
}

// 实验批量包装层。作用：只给 Taylor/LP 候选
// 对比使用，并把 ApproxConfig 留在 test-rvv 里。类别：数学专项测试。
inline void
rvv_sincos_finite_domain_experimental(
    const std::vector<float>& xs,
    std::vector<float>& out_s,
    std::vector<float>& out_c,
    const ApproxConfig& config)
{
  std::size_t j = 0;
  while (j < xs.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(xs.size() - j);
    const vfloat32m2_t x = __riscv_vle32_v_f32m2(xs.data() + j, vl);
    vfloat32m2_t s;
    vfloat32m2_t c;
    sincos_finite_domain_RVV_f32m2_experimental(x, s, c, vl, config);
    __riscv_vse32_v_f32m2(out_s.data() + j, s, vl);
    __riscv_vse32_v_f32m2(out_c.data() + j, c, vl);
    j += vl;
  }
}

inline void
rvv_sin_finite_domain(
    const std::vector<float>& xs,
    std::vector<float>& out_s)
{
  // RVV 单函数包装。作用：以接近 sin(xs) 的测试形态复用 paired helper；
  // 调用者：未来若需要单函数实验检查可复用。类别：RVV 执行链路。
  std::vector<float> out_c(xs.size());
  rvv_sincos_finite_domain(xs, out_s, out_c);
}

inline void
rvv_cos_finite_domain(
    const std::vector<float>& xs,
    std::vector<float>& out_c)
{
  // RVV 单函数包装。作用：以接近 cos(xs) 的测试形态复用 paired helper；
  // 调用者：未来若需要单函数实验检查可复用。类别：RVV 执行链路。
  std::vector<float> out_s(xs.size());
  rvv_sincos_finite_domain(xs, out_s, out_c);
}
#endif

} // namespace sincos_scratch
