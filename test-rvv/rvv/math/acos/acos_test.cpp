/*
 * acos_test.cpp — 在 x∈[0, x_hi] 上将多种 acos 逼近与 std::acos 对比（不改 common.hpp 亦可跑）。
 *
 *  (1) 标量 sqrt(1-x)*P 八常数（历史 PCL baseline）
 *  (2)–(10) 约化模型 acos(x)≈sqrt(1-x)*Q(1-x)，deg11/deg7/deg5 ×（remez1 / remez2 / LP）；标量侧为 float32 同构 Horner
 *  (11) __RVV10__：pcl::acos_RVV_f32m2（common.hpp 当前实现，deg5 remez2）
 *  (12) __RVV10__：约化模型 deg7 + remez2 的 RVV 实现
 *  (13) __RVV10__：约化模型 deg5 + remez2 的 RVV 实现
 *
 * 脚本：test-rvv/rvv/math/acos/script/parms_acos.py（默认三路 report）。
 *
 * 精度与 RVV/标量对照（下文「max |diff|」）
 * ----------------------------------------
 *  (1) PCL：保留旧八常数作为历史 baseline。
 *  (2)～(11) 约化多项式：标量路径按 RVV 思路使用 float32 Horner + sqrtf，常量由 double 系数 cast 到 float；
 *      RVV 约化内核全程 float32。若两端序列一致，RVV 与标量 max|diff| 通常显著收敛（接近 0 或低 ULP）。
 *
 * Build: make acos_test ARCH=riscv  或  g++ -std=c++17 -O3 -o acos_test acos_test.cpp -lm （+ RVV 时同 atan2_test）
 */
#include <algorithm>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../../script/term_table.hpp"

#if defined(__RVV10__)
#include <riscv_vector.h>
#include <pcl/common/common.h>
#include <pcl/common/impl/rvv_math.hpp>
#include <cstddef>
#endif

namespace {

using math_test::utf8_display_width;

constexpr math_test::Table3NumCols k_err_tbl{};
constexpr math_test::Perf2Cols k_perf_tbl{};

constexpr float k_x_hi = 0.999f;

void print_err_table_header(std::size_t n_pts, float x_hi, int w_item_disp)
{
  std::printf("=== acos 逼近 vs std::acos, n = %zu, x_hi = %.3f ===\n", n_pts, x_hi);
  math_test::print_rule_chars('=', k_err_tbl.line_columns(w_item_disp));
  math_test::print_pipe_table_3num_header_line(
      "Case / kernel", w_item_disp, "max (rad)", "deg", "mean (rad)", k_err_tbl);
  math_test::print_rule_chars('-', k_err_tbl.line_columns(w_item_disp));
}

void print_err_table_row(int w_item_disp, const char* label, double max_r, double deg, double mean_r)
{
  math_test::print_pipe_table_3num_row(
      w_item_disp,
      label,
      max_r,
      true,
      6,
      deg,
      false,
      4,
      mean_r,
      true,
      6,
      k_err_tbl);
}

int err_line_cols(int w_item_disp)
{
  return k_err_tbl.line_columns(w_item_disp);
}

void print_perf_table_header(std::size_t n_pts, int iters, int w_kern_disp)
{
  math_test::print_perf_pipe_header(
      n_pts, iters, w_kern_disp, "Kernel", "Time (ms)", "speedup", k_perf_tbl);
}

void print_perf_table_row(int w_kern_disp, const char* label, double ms, const char* note)
{
  math_test::print_perf_pipe_row(w_kern_disp, label, ms, 3, note, k_perf_tbl);
}

void print_perf_table_close(int w_kern_disp)
{
  math_test::print_perf_pipe_close(w_kern_disp, k_perf_tbl);
}

void print_rvv_tradeoff_summary(
    double ms_rvv_common,
    double err_rvv_common,
    double ms_rvv_d7,
    double err_rvv_d7,
    double ms_rvv_d5,
    double err_rvv_d5)
{
  const double spd_d7_vs_common = ms_rvv_common / ms_rvv_d7;
  const double spd_d5_vs_common = ms_rvv_common / ms_rvv_d5;

  std::printf("\n=== RVV Status vs Reduced Candidates ===\n");
  std::printf("  common.hpp: pcl::acos_RVV_f32m2  max err = %.6e rad, time = %.3f ms\n", err_rvv_common, ms_rvv_common);
  std::printf(
      "  candidate: RVV d7 remez2        max err = %.6e rad, speed = %.2fx of common.hpp\n",
      err_rvv_d7,
      spd_d7_vs_common);
  std::printf(
      "  candidate: RVV d5 remez2        max err = %.6e rad, speed = %.2fx of common.hpp\n",
      err_rvv_d5,
      spd_d5_vs_common);
  std::printf("  note: row (1) remains the historical PCL eight-constant scalar baseline.\n");
}

const float k_pi = 3.141592653589793f;

/* --- (1) PCL sqrt 八常数：与 common.hpp acos_SSE / acos_RVV 同系 --- */
inline float acos_pcl_scalar(float x)
{
  const float a0 = 1.59121552f;
  const float a1 = -0.15461442f;
  const float a2 = 0.05354897f;
  const float b0 = 0.89286965f;
  const float b1 = -0.89282669f;
  const float c0 = 0.06681017f;
  const float c1 = -0.09402311f;
  const float c2 = 0.02708663f;
  float inner = b0 + x * b1;
  if (inner < 0.f)
    inner = 0.f;
  float mul_term = a0 + x * (a1 + x * a2);
  float add_term = c0 + x * (c1 + x * c2);
  return mul_term * std::sqrt(inner) + add_term;
}

/* --- 约化模型：acos(x) ~= sqrt(1-x) * Q(1-x) ---
 *  标量路径改为 float32 Horner（与 RVV 同构）：按 float 运算链路执行，避免 double 累加导致的系统性 diff。
 */
template <typename C>
inline float eval_acos_reduced_poly(float x)
{
  float u = 1.0f - x;
  if (u < 0.0f)
    u = 0.0f;
  const float u2 = u * u;
  const float u3 = u2 * u;
  // 当前拟合里 q7=q8=q10=0，可按稀疏结构减少若干乘加
  float t = static_cast<float>(C::q9) + u2 * static_cast<float>(C::q11);
  float q = static_cast<float>(C::q6) + u3 * t;
  q = static_cast<float>(C::q5) + u * q;
  q = static_cast<float>(C::q4) + u * q;
  q = static_cast<float>(C::q3) + u * q;
  q = static_cast<float>(C::q2) + u * q;
  q = static_cast<float>(C::q1) + u * q;
  return std::sqrt(u) * (static_cast<float>(C::q0) + u * q);
}

template <typename C>
inline float eval_acos_reduced_poly_deg7(float x)
{
  float u = 1.0f - x;
  if (u < 0.0f)
    u = 0.0f;
  float q = static_cast<float>(C::q7);
  q = static_cast<float>(C::q6) + u * q;
  q = static_cast<float>(C::q5) + u * q;
  q = static_cast<float>(C::q4) + u * q;
  q = static_cast<float>(C::q3) + u * q;
  q = static_cast<float>(C::q2) + u * q;
  q = static_cast<float>(C::q1) + u * q;
  return std::sqrt(u) * (static_cast<float>(C::q0) + u * q);
}

template <typename C>
inline float eval_acos_reduced_poly_deg5(float x)
{
  float u = 1.0f - x;
  if (u < 0.0f)
    u = 0.0f;
  float q = static_cast<float>(C::q5);
  q = static_cast<float>(C::q4) + u * q;
  q = static_cast<float>(C::q3) + u * q;
  q = static_cast<float>(C::q2) + u * q;
  q = static_cast<float>(C::q1) + u * q;
  return std::sqrt(u) * (static_cast<float>(C::q0) + u * q);
}

template <typename C>
inline float eval_acos_reduced_poly_dense11(float x)
{
  float u = 1.0f - x;
  if (u < 0.0f)
    u = 0.0f;
  float q = static_cast<float>(C::q11);
  q = static_cast<float>(C::q10) + u * q;
  q = static_cast<float>(C::q9) + u * q;
  q = static_cast<float>(C::q8) + u * q;
  q = static_cast<float>(C::q7) + u * q;
  q = static_cast<float>(C::q6) + u * q;
  q = static_cast<float>(C::q5) + u * q;
  q = static_cast<float>(C::q4) + u * q;
  q = static_cast<float>(C::q3) + u * q;
  q = static_cast<float>(C::q2) + u * q;
  q = static_cast<float>(C::q1) + u * q;
  return std::sqrt(u) * (static_cast<float>(C::q0) + u * q);
}

/* --- (2) deg11 remez1：parms_acos.py --method remez1 --deg 11（稠密 q0..q11）--- */
struct AcosReducedRemez1Deg11 {
  static constexpr double q0 = 1.414213562193966;
  static constexpr double q1 = 0.1178511461608607;
  static constexpr double q2 = 0.02651605183620386;
  static constexpr double q3 = 0.007897888366046083;
  static constexpr double q4 = 0.002639581213701285;
  static constexpr double q5 = 0.001201858781216681;
  static constexpr double q6 = -0.0002584332125616682;
  static constexpr double q7 = 0.001434126856504966;
  static constexpr double q8 = -0.001622669688140837;
  static constexpr double q9 = 0.001450402024837244;
  static constexpr double q10 = -0.0006958168594569422;
  static constexpr double q11 = 0.0001686291165455616;
};

/* --- (3) deg11 remez2：parms_acos.py --method remez2 --deg 11 --- */
struct AcosReducedRemez2Deg11 {
  static constexpr double q0 = 1.414213564630177;
  static constexpr double q1 = 0.1178511294464069;
  static constexpr double q2 = 0.02651656650651231;
  static constexpr double q3 = 0.007890196791318107;
  static constexpr double q4 = 0.002703030232117461;
  static constexpr double q5 = 0.0009005233717898873;
  static constexpr double q6 = 0.0005798378741792223;
  static constexpr double q7 = 0.0;
  static constexpr double q8 = 0.0;
  static constexpr double q9 = 0.0001536018140430774;
  static constexpr double q10 = 0.0;
  static constexpr double q11 = -1.217035647128233e-05;
};

/* --- (4) deg11 LP：parms_acos.py --method lp --deg 11 --- */
struct AcosReducedLpDeg11 {
  static constexpr double q0 = 1.414213562373779;
  static constexpr double q1 = 0.1178511294564023;
  static constexpr double q2 = 0.02651656650651231;
  static constexpr double q3 = 0.007890196791318107;
  static constexpr double q4 = 0.002703030232117461;
  static constexpr double q5 = 0.0009005233717898873;
  static constexpr double q6 = 0.0005798378741792223;
  static constexpr double q7 = 0.0;
  static constexpr double q8 = 0.0;
  static constexpr double q9 = 0.0001536018140430774;
  static constexpr double q10 = 0.0;
  static constexpr double q11 = -1.217035647128233e-05;
};

template <typename C>
void run_reduced_poly_dense11(const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = eval_acos_reduced_poly_dense11<C>(xs[i]);
}

template <typename C>
void run_reduced_poly(const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = eval_acos_reduced_poly<C>(xs[i]);
}

/* --- (4) deg7 remez1：parms_acos.py --method remez1-deg7 --- */
struct AcosReducedRemez1Deg7 {
  static constexpr double q0 = 1.414213188912104;
  static constexpr double q1 = 0.1178641849866039;
  static constexpr double q2 = 0.02636458860921847;
  static constexpr double q3 = 0.008719535139112927;
  static constexpr double q4 = 0.0002768916562795434;
  static constexpr double q5 = 0.004891147985924504;
  static constexpr double q6 = -0.003017314299520197;
  static constexpr double q7 = 0.001484092403350398;
};

/* --- (5) deg7 remez2：parms_acos.py --method remez2-deg7 --- */
struct AcosReducedRemez2Deg7 {
  static constexpr double q0 = 1.414213558747199;
  static constexpr double q1 = 0.1178539448393304;
  static constexpr double q2 = 0.02644738228454647;
  static constexpr double q3 = 0.008406567572921202;
  static constexpr double q4 = 0.0009190414245998239;
  static constexpr double q5 = 0.004154037817069804;
  static constexpr double q6 = -0.002572261363003802;
  static constexpr double q7 = 0.001374063533135086;
};

/* --- (6) deg7 LP：parms_acos.py --method lp-deg7（用于标量对照） --- */
struct AcosReducedLpDeg7 {
  static constexpr double q0 = 1.41421354285702;
  static constexpr double q1 = 0.1178539448571087;
  static constexpr double q2 = 0.02644738228454647;
  static constexpr double q3 = 0.008406567572921202;
  static constexpr double q4 = 0.0009190414245998239;
  static constexpr double q5 = 0.004154037817069804;
  static constexpr double q6 = -0.002572261363003802;
  static constexpr double q7 = 0.001374063533135086;
};

/* --- (7) deg5 remez1：parms_acos.py --method remez1-deg5 --- */
struct AcosReducedRemez1Deg5 {
  static constexpr double q0 = 1.414196327328033;
  static constexpr double q1 = 0.1181715632132089;
  static constexpr double q2 = 0.02451971606187768;
  static constexpr double q3 = 0.01346301021415174;
  static constexpr double q4 = -0.004724656324727283;
  static constexpr double q5 = 0.005169831352385951;
};

/* --- (8) deg5 remez2：parms_acos.py --method remez2-deg5 --- */
struct AcosReducedRemez2Deg5 {
  static constexpr double q0 = 1.414212408248559;
  static constexpr double q1 = 0.117926522053977;
  static constexpr double q2 = 0.02571508511147162;
  static constexpr double q3 = 0.01095480727067022;
  static constexpr double q4 = -0.002360310714948563;
  static constexpr double q5 = 0.004346735271181379;
};

/* --- (9) deg5 LP：parms_acos.py --method lp-deg5（用于标量对照） --- */
struct AcosReducedLpDeg5 {
  static constexpr double q0 = 1.414212391465758;
  static constexpr double q1 = 0.1179265220794355;
  static constexpr double q2 = 0.02571508511147162;
  static constexpr double q3 = 0.01095480727067022;
  static constexpr double q4 = -0.002360310714948563;
  static constexpr double q5 = 0.004346735271181379;
};

template <typename C>
void run_reduced_poly_deg7(const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = eval_acos_reduced_poly_deg7<C>(xs[i]);
}

template <typename C>
void run_reduced_poly_deg5(const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = eval_acos_reduced_poly_deg5<C>(xs[i]);
}

struct AcosCase {
  const char* label;
  void (*run)(const float*, float*, std::size_t);
};

void run_scalar_pcl(const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = acos_pcl_scalar(xs[i]);
}

void run_std_acos(const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = static_cast<float>(std::acos(static_cast<double>(xs[i])));
}

#if defined(__RVV10__)
void run_rvv_acos(const float* xs, float* out, std::size_t n)
{
  std::size_t j = 0;
  while (j < n) {
    std::size_t vl = __riscv_vsetvl_e32m2(n - j);
    vfloat32m2_t vx = __riscv_vle32_v_f32m2(xs + j, vl);
    vfloat32m2_t vo = pcl::acos_RVV_f32m2(vx, vl);
    __riscv_vse32_v_f32m2(out + j, vo, vl);
    j += vl;
  }
}

/* 约化模型 RVV：系数由 double 常量 cast 为 float，乘加与 sqrt 均为 float32（无 double 累加）。 */
template <typename C>
inline vfloat32m2_t eval_reduced_deg7_rvv(vfloat32m2_t vx, std::size_t vl)
{
  vfloat32m2_t vone = __riscv_vfmv_v_f_f32m2(1.0f, vl);
  vfloat32m2_t vu = __riscv_vfsub_vv_f32m2(vone, vx, vl);
  vu = __riscv_vfmax_vf_f32m2(vu, 0.0f, vl);

  vfloat32m2_t q = __riscv_vfmv_v_f_f32m2(static_cast<float>(C::q7), vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q6), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q5), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q4), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q3), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q2), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q1), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q0), vl), vu, q, vl);
  vfloat32m2_t vsqrt_u = __riscv_vfsqrt_v_f32m2(vu, vl);
  return __riscv_vfmul_vv_f32m2(vsqrt_u, q, vl);
}

/* 约化 deg5 的 RVV：与 deg7 相同，全程 float32。 */
template <typename C>
inline vfloat32m2_t eval_reduced_deg5_rvv(vfloat32m2_t vx, std::size_t vl)
{
  vfloat32m2_t vone = __riscv_vfmv_v_f_f32m2(1.0f, vl);
  vfloat32m2_t vu = __riscv_vfsub_vv_f32m2(vone, vx, vl);
  vu = __riscv_vfmax_vf_f32m2(vu, 0.0f, vl);

  vfloat32m2_t q = __riscv_vfmv_v_f_f32m2(static_cast<float>(C::q5), vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q4), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q3), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q2), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q1), vl), vu, q, vl);
  q = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(static_cast<float>(C::q0), vl), vu, q, vl);
  vfloat32m2_t vsqrt_u = __riscv_vfsqrt_v_f32m2(vu, vl);
  return __riscv_vfmul_vv_f32m2(vsqrt_u, q, vl);
}

void run_rvv_reduced_deg7_remez2(const float* xs, float* out, std::size_t n)
{
  std::size_t j = 0;
  while (j < n) {
    std::size_t vl = __riscv_vsetvl_e32m2(n - j);
    vfloat32m2_t vx = __riscv_vle32_v_f32m2(xs + j, vl);
    vfloat32m2_t vy = eval_reduced_deg7_rvv<AcosReducedRemez2Deg7>(vx, vl);
    __riscv_vse32_v_f32m2(out + j, vy, vl);
    j += vl;
  }
}

void run_rvv_reduced_deg5_remez2(const float* xs, float* out, std::size_t n)
{
  std::size_t j = 0;
  while (j < n) {
    std::size_t vl = __riscv_vsetvl_e32m2(n - j);
    vfloat32m2_t vx = __riscv_vle32_v_f32m2(xs + j, vl);
    vfloat32m2_t vy = eval_reduced_deg5_rvv<AcosReducedRemez2Deg5>(vx, vl);
    __riscv_vse32_v_f32m2(out + j, vy, vl);
    j += vl;
  }
}

void print_rvv_scalar_diff_legend()
{
  std::printf(
      "\n"
      "（RVV 与标量 max|diff| 说明）(11) common.hpp 当前实现应与标量 (9) deg5 remez2 对齐。"
      "约化 (2)–(10) 标量改为 float32 同构 Horner；(12)(13) 亦为 float32 RVV，故与对应标量应更接近（常见为 0 或低 ULP），"
      "与主表相对 std::acos 的误差是不同指标。\n");
}
#endif

inline void bench_touch_output(const float* buf, std::size_t sz, volatile double* sink)
{
  if (sz == 0)
    return;
  *sink += static_cast<double>(buf[0]) + static_cast<double>(buf[sz - 1]);
  if (sz > 2)
    *sink += static_cast<double>(buf[sz / 2]);
}

void compute_errors(
    const float* ref, const float* approx, std::size_t n,
    float& max_abs_rad, float& max_abs_deg, double& mean_abs_rad)
{
  max_abs_rad = 0.f;
  mean_abs_rad = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    float e = std::fabs(approx[i] - ref[i]);
    if (e > max_abs_rad)
      max_abs_rad = e;
    mean_abs_rad += static_cast<double>(e);
  }
  mean_abs_rad /= static_cast<double>(n);
  const float rad2deg = 180.f / k_pi;
  max_abs_deg = max_abs_rad * rad2deg;
}

const AcosCase k_cases[] = {
    {"(1) 标量 sqrt(1-x)*P 八常数（历史 PCL baseline）", run_scalar_pcl},
    {"(2) 约化 deg11 + remez1（sqrt(1-x)*Q，稠密 float32 Horner）",
     run_reduced_poly_dense11<AcosReducedRemez1Deg11>},
    {"(3) 约化 deg11 + remez2（sqrt(1-x)*Q，float32 Horner）", run_reduced_poly<AcosReducedRemez2Deg11>},
    {"(4) 约化 deg11 + 离散 LP（sqrt(1-x)*Q）", run_reduced_poly<AcosReducedLpDeg11>},
    {"(5) 约化 deg7 + remez1（sqrt(1-x)*Q）", run_reduced_poly_deg7<AcosReducedRemez1Deg7>},
    {"(6) 约化 deg7 + remez2（sqrt(1-x)*Q）", run_reduced_poly_deg7<AcosReducedRemez2Deg7>},
    {"(7) 约化 deg7 + 离散 LP（sqrt(1-x)*Q）", run_reduced_poly_deg7<AcosReducedLpDeg7>},
    {"(8) 约化 deg5 + remez1（sqrt(1-x)*Q）", run_reduced_poly_deg5<AcosReducedRemez1Deg5>},
    {"(9) 约化 deg5 + remez2（sqrt(1-x)*Q）", run_reduced_poly_deg5<AcosReducedRemez2Deg5>},
    {"(10) 约化 deg5 + 离散 LP（sqrt(1-x)*Q）", run_reduced_poly_deg5<AcosReducedLpDeg5>},
};

} // namespace

int main()
{
  static const char* k_try_locales[] = { "", "C.UTF-8", "zh_CN.UTF-8", "en_US.UTF-8" };
  for (const char* loc : k_try_locales) {
    if (std::setlocale(LC_ALL, loc))
      break;
  }

  const std::size_t n = 256u * 256u;
  float* xs = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* ref = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* out = static_cast<float*>(std::malloc(n * sizeof(float)));
#if defined(__RVV10__)
  float* out_rvv_pcl = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* out_rvv_d7 = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* out_rvv_d5 = static_cast<float*>(std::malloc(n * sizeof(float)));
#endif

  if (!xs || !ref || !out) {
    std::fprintf(stderr, "malloc failed\n");
    return 1;
  }
#if defined(__RVV10__)
  if (!out_rvv_pcl || !out_rvv_d7 || !out_rvv_d5) {
    std::fprintf(stderr, "malloc failed (rvv)\n");
    return 1;
  }
#endif

  for (std::size_t i = 0; i < n; ++i)
    xs[i] = (static_cast<float>(i) / static_cast<float>(n - 1)) * k_x_hi;

  run_std_acos(xs, ref, n);

  int w_item_disp = 24;
  for (const auto& c : k_cases)
    w_item_disp = std::max(w_item_disp, utf8_display_width(c.label));
#if defined(__RVV10__)
  w_item_disp = std::max(
      w_item_disp,
      utf8_display_width("(11) pcl::acos_RVV_f32m2（common.hpp：deg5 remez2）"));
  w_item_disp = std::max(
      w_item_disp,
      utf8_display_width("(12) RVV 约化 deg7 remez2（sqrt(1-x)*Q）"));
  w_item_disp = std::max(
      w_item_disp,
      utf8_display_width("(13) RVV 约化 deg5 remez2（sqrt(1-x)*Q）"));
#endif
  w_item_disp += 1;

  print_err_table_header(n, k_x_hi, w_item_disp);

  for (const auto& c : k_cases) {
    c.run(xs, out, n);
    float max_r, max_d;
    double mean_r;
    compute_errors(ref, out, n, max_r, max_d, mean_r);
    print_err_table_row(
        w_item_disp,
        c.label,
        static_cast<double>(max_r),
        static_cast<double>(max_d),
        mean_r);
  }

#if defined(__RVV10__)
  double err_rvv_pcl_max = 0.0;
  double err_rvv_d7_max = 0.0;
  double err_rvv_d5_max = 0.0;
  run_rvv_acos(xs, out_rvv_pcl, n);
  {
    float max_r, max_d;
    double mean_r;
    compute_errors(ref, out_rvv_pcl, n, max_r, max_d, mean_r);
    err_rvv_pcl_max = static_cast<double>(max_r);
    print_err_table_row(
        w_item_disp,
        "(11) pcl::acos_RVV_f32m2（common.hpp：deg5 remez2）",
        static_cast<double>(max_r),
        static_cast<double>(max_d),
        mean_r);
  }
  run_rvv_reduced_deg7_remez2(xs, out_rvv_d7, n);
  {
    float max_r, max_d;
    double mean_r;
    compute_errors(ref, out_rvv_d7, n, max_r, max_d, mean_r);
    err_rvv_d7_max = static_cast<double>(max_r);
    print_err_table_row(
        w_item_disp,
        "(12) RVV 约化 deg7 remez2（sqrt(1-x)*Q）",
        static_cast<double>(max_r),
        static_cast<double>(max_d),
        mean_r);
  }
  run_rvv_reduced_deg5_remez2(xs, out_rvv_d5, n);
  {
    float max_r, max_d;
    double mean_r;
    compute_errors(ref, out_rvv_d5, n, max_r, max_d, mean_r);
    err_rvv_d5_max = static_cast<double>(max_r);
    print_err_table_row(
        w_item_disp,
        "(13) RVV 约化 deg5 remez2（sqrt(1-x)*Q）",
        static_cast<double>(max_r),
        static_cast<double>(max_d),
        mean_r);
  }
  math_test::print_rule_chars('=', err_line_cols(w_item_disp));

  print_rvv_scalar_diff_legend();
  {
    run_reduced_poly_deg5<AcosReducedRemez2Deg5>(xs, out, n);
    float max_diff = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      float d = std::fabs(out_rvv_pcl[i] - out[i]);
      if (d > max_diff)
        max_diff = d;
    }
    const int col0 = 6;
    const char* diff_lbl = "max |diff| :";
    std::printf("\n[pcl::acos_RVV vs 标量 (9) deg5 remez2]\n");
    std::printf("  %-*s  %*.*e\n", col0, diff_lbl, col0, 6, static_cast<double>(max_diff));
  }
  {
    run_reduced_poly_deg7<AcosReducedRemez2Deg7>(xs, out, n);
    float max_diff = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      float d = std::fabs(out_rvv_d7[i] - out[i]);
      if (d > max_diff)
        max_diff = d;
    }
    const int col0 = 6;
    const char* diff_lbl = "max |diff| :";
    std::printf("[RVV-deg7 vs 标量 (6) deg7 remez2]\n");
    std::printf("  %-*s  %*.*e\n", col0, diff_lbl, col0, 6, static_cast<double>(max_diff));
  }
  {
    run_reduced_poly_deg5<AcosReducedRemez2Deg5>(xs, out, n);
    float max_diff = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      float d = std::fabs(out_rvv_d5[i] - out[i]);
      if (d > max_diff)
        max_diff = d;
    }
    const int col0 = 6;
    const char* diff_lbl = "max |diff| :";
    std::printf("[RVV-deg5 vs 标量 (9) deg5 remez2]\n");
    std::printf("  %-*s  %*.*e\n", col0, diff_lbl, col0, 6, static_cast<double>(max_diff));
  }
#else
  math_test::print_rule_chars('=', err_line_cols(w_item_disp));
#endif

  const int iters = 100;
  float* buf_std = static_cast<float*>(std::malloc(n * sizeof(float)));
  if (!buf_std) {
    std::free(xs);
    std::free(ref);
    std::free(out);
#if defined(__RVV10__)
    std::free(out_rvv_pcl);
    std::free(out_rvv_d7);
    std::free(out_rvv_d5);
#endif
    return 1;
  }

  volatile double bench_sink = 0.0;

  run_std_acos(xs, buf_std, n);
  run_scalar_pcl(xs, out, n);
  run_reduced_poly_dense11<AcosReducedRemez1Deg11>(xs, out, n);
  run_reduced_poly_deg7<AcosReducedRemez1Deg7>(xs, out, n);
  run_reduced_poly_deg5<AcosReducedRemez1Deg5>(xs, out, n);
  run_reduced_poly<AcosReducedRemez2Deg11>(xs, out, n);
  run_reduced_poly<AcosReducedLpDeg11>(xs, out, n);
  run_reduced_poly_deg7<AcosReducedRemez2Deg7>(xs, out, n);
  run_reduced_poly_deg7<AcosReducedLpDeg7>(xs, out, n);
  run_reduced_poly_deg5<AcosReducedRemez2Deg5>(xs, out, n);
  run_reduced_poly_deg5<AcosReducedLpDeg5>(xs, out, n);
#if defined(__RVV10__)
  run_rvv_acos(xs, out_rvv_pcl, n);
  run_rvv_reduced_deg7_remez2(xs, out_rvv_d7, n);
  run_rvv_reduced_deg5_remez2(xs, out_rvv_d5, n);
#endif

  auto t0 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_std_acos(xs, buf_std, n);
    bench_touch_output(buf_std, n, &bench_sink);
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_scalar_pcl(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t2 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_dense11<AcosReducedRemez1Deg11>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t3 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_deg7<AcosReducedRemez1Deg7>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t4 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_deg5<AcosReducedRemez1Deg5>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t5 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly<AcosReducedRemez2Deg11>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t6 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly<AcosReducedLpDeg11>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t7 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_deg7<AcosReducedRemez2Deg7>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t8 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_deg7<AcosReducedLpDeg7>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t9 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_deg5<AcosReducedRemez2Deg5>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t10 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_reduced_poly_deg5<AcosReducedLpDeg5>(xs, out, n);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t11 = std::chrono::high_resolution_clock::now();

  int w_perf_lab = 10;
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar PCL"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d11 remez1"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d7 remez1"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d5 remez1"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d11 remez2"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d11 LP"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d7 remez2"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d7 LP"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d5 remez2"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar reduced d5 LP"));
#if defined(__RVV10__)
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("pcl::acos_RVV_f32m2"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("RVV reduced d7 remez2"));
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("RVV reduced d5 remez2"));
#endif
  w_perf_lab += 2;

  print_perf_table_header(n, iters, w_perf_lab);

  double ms_std = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double ms_scal = std::chrono::duration<double, std::milli>(t2 - t1).count();
  double ms_r1_d11 = std::chrono::duration<double, std::milli>(t3 - t2).count();
  double ms_r1_d7 = std::chrono::duration<double, std::milli>(t4 - t3).count();
  double ms_r1_d5 = std::chrono::duration<double, std::milli>(t5 - t4).count();
  double ms_remez2_d11 = std::chrono::duration<double, std::milli>(t6 - t5).count();
  double ms_lp_d11 = std::chrono::duration<double, std::milli>(t7 - t6).count();
  double ms_remez2_d7 = std::chrono::duration<double, std::milli>(t8 - t7).count();
  double ms_lp_d7 = std::chrono::duration<double, std::milli>(t9 - t8).count();
  double ms_remez2_d5 = std::chrono::duration<double, std::milli>(t10 - t9).count();
  double ms_lp_d5 = std::chrono::duration<double, std::milli>(t11 - t10).count();
  char note_buf[64];
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_scal);
  print_perf_table_row(w_perf_lab, "scalar PCL", ms_scal, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r1_d11);
  print_perf_table_row(w_perf_lab, "scalar reduced d11 remez1", ms_r1_d11, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r1_d7);
  print_perf_table_row(w_perf_lab, "scalar reduced d7 remez1", ms_r1_d7, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r1_d5);
  print_perf_table_row(w_perf_lab, "scalar reduced d5 remez1", ms_r1_d5, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_remez2_d11);
  print_perf_table_row(w_perf_lab, "scalar reduced d11 remez2", ms_remez2_d11, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_lp_d11);
  print_perf_table_row(w_perf_lab, "scalar reduced d11 LP", ms_lp_d11, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_remez2_d7);
  print_perf_table_row(w_perf_lab, "scalar reduced d7 remez2", ms_remez2_d7, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_lp_d7);
  print_perf_table_row(w_perf_lab, "scalar reduced d7 LP", ms_lp_d7, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_remez2_d5);
  print_perf_table_row(w_perf_lab, "scalar reduced d5 remez2", ms_remez2_d5, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_lp_d5);
  print_perf_table_row(w_perf_lab, "scalar reduced d5 LP", ms_lp_d5, note_buf);
#if defined(__RVV10__)
  auto t12 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_rvv_acos(xs, out_rvv_pcl, n);
    bench_touch_output(out_rvv_pcl, n, &bench_sink);
  }
  auto t13 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_rvv_reduced_deg7_remez2(xs, out_rvv_d7, n);
    bench_touch_output(out_rvv_d7, n, &bench_sink);
  }
  auto t14 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_rvv_reduced_deg5_remez2(xs, out_rvv_d5, n);
    bench_touch_output(out_rvv_d5, n, &bench_sink);
  }
  auto t15 = std::chrono::high_resolution_clock::now();
  double ms_r_common = std::chrono::duration<double, std::milli>(t13 - t12).count();
  double ms_r_d7 = std::chrono::duration<double, std::milli>(t14 - t13).count();
  double ms_r_d5 = std::chrono::duration<double, std::milli>(t15 - t14).count();
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r_common);
  print_perf_table_row(w_perf_lab, "pcl::acos_RVV_f32m2", ms_r_common, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r_d7);
  print_perf_table_row(w_perf_lab, "RVV reduced d7 remez2", ms_r_d7, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r_d5);
  print_perf_table_row(w_perf_lab, "RVV reduced d5 remez2", ms_r_d5, note_buf);
  print_perf_table_close(w_perf_lab);
  print_rvv_tradeoff_summary(
      ms_r_common, err_rvv_pcl_max, ms_r_d7, err_rvv_d7_max, ms_r_d5, err_rvv_d5_max);
  std::free(out_rvv_pcl);
  std::free(out_rvv_d7);
  std::free(out_rvv_d5);
#else
  print_perf_table_close(w_perf_lab);
#endif

  (void)bench_sink;

  std::free(buf_std);
  std::free(xs);
  std::free(ref);
  std::free(out);
  return 0;
}
