/*
 * logf_test.cpp — 多路 logf 参数对比（标量 + RVV）
 *
 * 比较对象：
 *   (1) 本文件内标量 float Horner；系数与 common.hpp 中 logf_RVV 所用 log1p 常数一致（库内无单独标量 logf API）
 *   (2) parms_log1p.py remez1（绝对误差，第一算法风格）
 *   (3) parms_log1p.py remez2（绝对误差）
 *   (4) parms_log1p.py lp（绝对误差）
 *   (5) parms_log1p.py sollya(fpminimax, absolute)
 *
 * RVV:
 *   - (6) 直接调用 pcl::logf_RVV_f32m2
 *   - (7)–(10) 同一 RVV 约化与重构路径，仅替换 log1p Horner 系数
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "../../script/term_table.hpp"

#if defined(__RVV10__)
#include <cstddef>
#include <pcl/common/common.h>
#include <pcl/common/impl/common.hpp>
#include <riscv_vector.h>
#endif

namespace {

using math_test::utf8_display_width;

constexpr math_test::Perf2Cols k_perf_tbl{};
static constexpr int k_err_num_w = 16;

static constexpr float kLog2Hi = 0.6931471824645996f;
static constexpr float kLog2Lo = -1.904654290582768e-09f;
static constexpr float kSubnormSc = 16777216.0f;
static constexpr float kMaxAbsRefThreshold = 1e10f;
static constexpr float kLogXMin = -20.0f;
static constexpr float kLogXMax = 20.0f;

struct LogfBaselineCoeff {
  static constexpr float c0 = 1.8556080581e-07f;
  static constexpr float c1 = 9.9997340558e-01f;
  static constexpr float c2 = -4.9937887289e-01f;
  static constexpr float c3 = 3.2776673211e-01f;
  static constexpr float c4 = -2.2467442806e-01f;
  static constexpr float c5 = 1.3301016232e-01f;
  static constexpr float c6 = -5.4002504554e-02f;
  static constexpr float c7 = 1.0452683404e-02f;
};

struct LogfRemez1Coeff {
  static constexpr float c0 = 5.44528206223229e-08f;
  static constexpr float c1 = 0.9999541289372607f;
  static constexpr float c2 = -0.4991081652207077f;
  static constexpr float c3 = 0.326430012819039f;
  static constexpr float c4 = -0.2215213295201122f;
  static constexpr float c5 = 0.1291661256269744f;
  static constexpr float c6 = -0.05166960644611545f;
  static constexpr float c7 = 0.009896014363606419f;
};

struct LogfRemez2Coeff {
  static constexpr float c0 = 2.011035705367378e-07f;
  static constexpr float c1 = 0.9999737148907613f;
  static constexpr float c2 = -0.4993804568247253f;
  static constexpr float c3 = 0.3277496941750039f;
  static constexpr float c4 = -0.2245729332813923f;
  static constexpr float c5 = 0.1328137950599862f;
  static constexpr float c6 = -0.05383971752877982f;
  static constexpr float c7 = 0.01040308262004508f;
};

struct LogfLpCoeff {
  static constexpr float c0 = 1.64871564074708e-07f;
  static constexpr float c1 = 0.9999737145176905f;
  static constexpr float c2 = -0.4993804568371159f;
  static constexpr float c3 = 0.3277496941850039f;
  static constexpr float c4 = -0.2245729332706192f;
  static constexpr float c5 = 0.1328137950816015f;
  static constexpr float c6 = -0.05383971750244008f;
  static constexpr float c7 = 0.0104030826362085f;
};

struct LogfSollyaCoeff {
  static constexpr float c0 = 1.922023964144791e-07f;
  static constexpr float c1 = 0.999973122126788f;
  static constexpr float c2 = -0.4993799892147339f;
  static constexpr float c3 = 0.3277878623278651f;
  static constexpr float c4 = -0.2247556918993918f;
  static constexpr float c5 = 0.1331463211106413f;
  static constexpr float c6 = -0.05410854392330709f;
  static constexpr float c7 = 0.0104841000320837f;
};

template <typename C>
inline float
logf_poly_scalar(float x)
{
  if (std::isnan(x))
    return x;
  if (x < 0.0f)
    return std::nanf("");
  if (x == 0.0f)
    return -std::numeric_limits<float>::infinity();
  if (x == std::numeric_limits<float>::infinity())
    return x;

  std::uint32_t ix = 0;
  static_assert(sizeof(x) == sizeof(ix), "");
  std::memcpy(&ix, &x, sizeof(ix));
  if ((ix & 0x7f800000u) == 0x7f800000u && (ix & 0x7fffffu) != 0u)
    return std::nanf("");

  const std::uint32_t e8_0 = (ix >> 23) & 255u;
  const std::uint32_t m0 = ix & 0x7fffffu;
  if (e8_0 == 0u && m0 != 0u)
    x *= kSubnormSc;
  std::memcpy(&ix, &x, sizeof(ix));
  const std::uint32_t e8 = (ix >> 23) & 255u;
  int e = static_cast<int>(e8) - 127;
  if (e8_0 == 0u && m0 != 0u)
    e -= 24;
  const std::uint32_t mi = (ix & 0x7fffffu) | 0x3f800000u;
  float m;
  std::memcpy(&m, &mi, sizeof(m));
  const float u = m - 1.0f;
  float poly = C::c7;
  poly = std::fmaf(u, poly, C::c6);
  poly = std::fmaf(u, poly, C::c5);
  poly = std::fmaf(u, poly, C::c4);
  poly = std::fmaf(u, poly, C::c3);
  poly = std::fmaf(u, poly, C::c2);
  poly = std::fmaf(u, poly, C::c1);
  const float log1p = std::fmaf(u, poly, C::c0);
  const float fe = static_cast<float>(e);
  float r = std::fmaf(fe, kLog2Hi, 0.0f);
  r = std::fmaf(fe, kLog2Lo, r);
  return r + log1p;
}

template <typename C>
inline float
logf_poly_scalar_fast(float x)
{
  // fast-path 前提：x 为正、有限、normal；用于性能测量，不替代完整语义路径
  std::uint32_t ix = 0;
  std::memcpy(&ix, &x, sizeof(ix));
  const std::uint32_t e8 = (ix >> 23) & 255u;
  int e = static_cast<int>(e8) - 127;
  const std::uint32_t mi = (ix & 0x7fffffu) | 0x3f800000u;
  float m = 0.0f;
  std::memcpy(&m, &mi, sizeof(m));
  const float u = m - 1.0f;
  float poly = C::c7;
  poly = std::fmaf(u, poly, C::c6);
  poly = std::fmaf(u, poly, C::c5);
  poly = std::fmaf(u, poly, C::c4);
  poly = std::fmaf(u, poly, C::c3);
  poly = std::fmaf(u, poly, C::c2);
  poly = std::fmaf(u, poly, C::c1);
  const float log1p = std::fmaf(u, poly, C::c0);
  const float fe = static_cast<float>(e);
  float r = std::fmaf(fe, kLog2Hi, 0.0f);
  r = std::fmaf(fe, kLog2Lo, r);
  return r + log1p;
}

template <typename C>
void
run_scalar_poly(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i)
    out[i] = logf_poly_scalar<C>(xs[i]);
}

template <typename C>
void
run_scalar_poly_fast(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i)
    out[i] = logf_poly_scalar_fast<C>(xs[i]);
}

void
run_std_logf(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i)
    out[i] = std::logf(xs[i]);
}

#if defined(__RVV10__)
void
run_rvv_common(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  std::size_t j = 0;
  while (j < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - j);
    const vfloat32m2_t vx = __riscv_vle32_v_f32m2(xs.data() + j, vl);
    const vfloat32m2_t vy = pcl::logf_RVV_f32m2(vx, vl);
    __riscv_vse32_v_f32m2(out.data() + j, vy, vl);
    j += vl;
  }
}

template <typename C>
inline vfloat32m2_t
eval_logf_rvv_poly(vfloat32m2_t x, std::size_t vl)
{
  const vfloat32m2_t v_one = __riscv_vfmv_v_f_f32m2(1.0f, vl);
  const vuint32m2_t ixu = __riscv_vreinterpret_v_f32m2_u32m2(x);
  const vbool16_t m_pos = __riscv_vmfgt_vf_f32m2_b16(x, 0.0f, vl);
  const vuint32m2_t absi = __riscv_vand_vx_u32m2(ixu, 0x7fffffffu, vl);
  const vbool16_t m_nzero = __riscv_vmsne_vx_u32m2_b16(absi, 0, vl);
  const vuint32m2_t e8 = __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(ixu, 23, vl), 255, vl);
  const vbool16_t m_finite = __riscv_vmsne_vx_u32m2_b16(e8, 255, vl);
  vbool16_t m_ok = __riscv_vmand_mm_b16(m_pos, m_nzero, vl);
  m_ok = __riscv_vmand_mm_b16(m_ok, m_finite, vl);
  const vfloat32m2_t x_use = __riscv_vmerge_vvm_f32m2(v_one, x, m_ok, vl);

  // __core_logf_RVV_f32m2 with templated coefficients
  const vuint32m2_t ix0 = __riscv_vreinterpret_v_f32m2_u32m2(x_use);
  const vuint32m2_t e8_0 = __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(ix0, 23, vl), 255, vl);
  const vuint32m2_t mbits0 = __riscv_vand_vx_u32m2(ix0, 0x7fffffu, vl);
  const vbool16_t is_sub = __riscv_vmand_mm_b16(
      __riscv_vmseq_vx_u32m2_b16(e8_0, 0, vl),
      __riscv_vmsne_vx_u32m2_b16(mbits0, 0, vl), vl);
  const vfloat32m2_t x_scaled = __riscv_vfmul_vf_f32m2(x_use, kSubnormSc, vl);
  vfloat32m2_t x_norm = __riscv_vmerge_vvm_f32m2(x_use, x_scaled, is_sub, vl);
  const vuint32m2_t ix = __riscv_vreinterpret_v_f32m2_u32m2(x_norm);
  const vuint32m2_t e8n = __riscv_vand_vx_u32m2(__riscv_vsrl_vx_u32m2(ix, 23, vl), 255, vl);
  const vint32m2_t e_bias = __riscv_vreinterpret_v_u32m2_i32m2(e8n);
  vint32m2_t e = __riscv_vsub_vx_i32m2(e_bias, 127, vl);
  const vint32m2_t e_sub = __riscv_vsub_vx_i32m2(e, 24, vl);
  e = __riscv_vmerge_vvm_i32m2(e, e_sub, is_sub, vl);

  const vuint32m2_t mi = __riscv_vor_vx_u32m2(__riscv_vand_vx_u32m2(ix, 0x7fffffu, vl), 0x3f800000, vl);
  const vfloat32m2_t m = __riscv_vreinterpret_v_u32m2_f32m2(mi);
  vfloat32m2_t u = __riscv_vfsub_vf_f32m2(m, 1.0f, vl);
  vfloat32m2_t poly = __riscv_vfmv_v_f_f32m2(C::c7, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c6, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c5, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c4, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c3, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c2, vl), u, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c1, vl), u, poly, vl);
  const vfloat32m2_t log1p =
      __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c0, vl), u, poly, vl);
  vfloat32m2_t fe = __riscv_vfcvt_f_x_v_f32m2(e, vl);
  vfloat32m2_t r = __riscv_vfmv_v_f_f32m2(0.0f, vl);
  r = __riscv_vfmacc_vf_f32m2(r, kLog2Hi, fe, vl);
  r = __riscv_vfmacc_vf_f32m2(r, kLog2Lo, fe, vl);
  vfloat32m2_t y = __riscv_vfadd_vv_f32m2(r, log1p, vl);

  const vbool16_t m_zero = __riscv_vmseq_vx_u32m2_b16(absi, 0, vl);
  y = __riscv_vmerge_vvm_f32m2(
      y, __riscv_vfmv_v_f_f32m2(-std::numeric_limits<float>::infinity(), vl), m_zero, vl);
  const vbool16_t m_neg = __riscv_vmflt_vf_f32m2_b16(x, 0.0f, vl);
  y = __riscv_vmerge_vvm_f32m2(y, __riscv_vfmv_v_f_f32m2(std::nanf(""), vl), m_neg, vl);
  const vfloat32m2_t v_inf = __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t m_pinf = __riscv_vmfeq_vv_f32m2_b16(x, v_inf, vl);
  y = __riscv_vmerge_vvm_f32m2(y, v_inf, m_pinf, vl);
  const vbool16_t m_nan = __riscv_vmfne_vv_f32m2_b16(x, x, vl);
  y = __riscv_vmerge_vvm_f32m2(y, x, m_nan, vl);
  return y;
}

template <typename C>
void
run_rvv_poly(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  std::size_t j = 0;
  while (j < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - j);
    const vfloat32m2_t vx = __riscv_vle32_v_f32m2(xs.data() + j, vl);
    const vfloat32m2_t vy = eval_logf_rvv_poly<C>(vx, vl);
    __riscv_vse32_v_f32m2(out.data() + j, vy, vl);
    j += vl;
  }
}
#endif

void
compute_errors(const std::vector<float>& ref,
               const std::vector<float>& approx,
               float& max_abs_err,
               float& max_rel_err,
               double& mean_abs_err,
               float max_abs_ref = kMaxAbsRefThreshold)
{
  const std::size_t n = ref.size();
  max_abs_err = 0.0f;
  max_rel_err = 0.0f;
  mean_abs_err = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const float r = ref[i];
    const float a = approx[i];
    if (std::isnan(r) || std::isnan(a) || std::isinf(r) || std::isinf(a))
      continue;
    const float abs_err = std::fabs(a - r);
    if (std::fabs(r) <= max_abs_ref && abs_err > max_abs_err)
      max_abs_err = abs_err;
    mean_abs_err += static_cast<double>(abs_err);
    if (r != 0.0f) {
      const float rel = abs_err / std::fabs(r);
      if (rel > max_rel_err)
        max_rel_err = rel;
    }
  }
  mean_abs_err /= static_cast<double>(n);
}

template <typename Runner>
double
time_ms(Runner&& fn)
{
  const auto t0 = std::chrono::high_resolution_clock::now();
  fn();
  const auto t1 = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

float
max_diff(const std::vector<float>& a, const std::vector<float>& b)
{
  float m = 0.0f;
  const std::size_t n = a.size();
  for (std::size_t i = 0; i < n; ++i) {
    const float d = std::fabs(a[i] - b[i]);
    if (d > m)
      m = d;
  }
  return m;
}

int
err_table_line_columns(int w_lab)
{
  const int ws[2] = { k_err_num_w, k_err_num_w };
  return math_test::pipe_grid_line_columns(w_lab, ws, 2);
}

void
print_err_table_top(int w_lab)
{
  math_test::print_rule_chars('=', err_table_line_columns(w_lab));
  const char* hdrs[2] = { "max rel", "mean abs" };
  const int ws[2] = { k_err_num_w, k_err_num_w };
  math_test::print_pipe_grid_header_line("Case / kernel", w_lab, hdrs, ws, 2);
  math_test::print_rule_chars('-', err_table_line_columns(w_lab));
}

void
print_err_table_row(int w_lab, const char* label, float max_rel, double mean_abs)
{
  char b1[64], b2[64];
  math_test::fmt_fixed_e(b1, k_err_num_w, 6, static_cast<double>(max_rel));
  math_test::fmt_fixed_e(b2, k_err_num_w, 6, mean_abs);
  const char* cells[2] = { b1, b2 };
  const int ws[2] = { k_err_num_w, k_err_num_w };
  math_test::print_pipe_grid_row(w_lab, label, cells, ws, 2);
}

void
print_err_table_close(int w_lab)
{
  math_test::print_rule_chars('=', err_table_line_columns(w_lab));
}

void
print_err_table_rvv_disabled(int w_lab)
{
  char b1[64], b2[64];
  math_test::fmt_fixed_right_bytes(b1, k_err_num_w, "");
  math_test::fmt_fixed_right_bytes(b2, k_err_num_w, "");
  const char* cells[2] = { b1, b2 };
  const int ws[2] = { k_err_num_w, k_err_num_w };
  math_test::print_pipe_grid_row(
      w_lab, "(RVV path disabled: compile without __RVV10__)", cells, ws, 2);
}

void
print_perf_table_header(std::size_t n_pts, int iters, int w_kern_disp)
{
  math_test::print_perf_pipe_header(
      n_pts, iters, w_kern_disp, "Kernel", "Time (ms)", "speedup", k_perf_tbl);
}

void
print_perf_table_row(int w_kern_disp, const char* label, double ms, const char* note)
{
  math_test::print_perf_pipe_row(w_kern_disp, label, ms, 3, note, k_perf_tbl);
}

void
print_perf_table_close(int w_kern_disp)
{
  math_test::print_perf_pipe_close(w_kern_disp, k_perf_tbl);
}

} // namespace

int
main()
{
  const std::size_t n = 10000;
  const int iters = 100;
  std::vector<float> xs(n), ref(n), out_std(n);
  std::vector<float> out_s_base(n), out_s_base_fast(n), out_s_r1(n), out_s_r2(n), out_s_lp(n), out_s_sol(n);
#if defined(__RVV10__)
  std::vector<float> out_v_base(n), out_v_r1(n), out_v_r2(n), out_v_lp(n), out_v_sol(n);
#endif

  for (std::size_t i = 0; i < n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n - 1);
    const float le = kLogXMin + (kLogXMax - kLogXMin) * t;
    const float x = std::expf(le);
    xs[i] = x;
    ref[i] = std::logf(x);
  }

  run_scalar_poly<LogfBaselineCoeff>(xs, out_s_base);
  run_scalar_poly<LogfRemez1Coeff>(xs, out_s_r1);
  run_scalar_poly<LogfRemez2Coeff>(xs, out_s_r2);
  run_scalar_poly<LogfLpCoeff>(xs, out_s_lp);
  run_scalar_poly<LogfSollyaCoeff>(xs, out_s_sol);

  float max_abs = 0.0f, max_rel = 0.0f;
  double mean_abs = 0.0;

  int w_err = 10;
  w_err = std::max(w_err, utf8_display_width("(1) 标量 float（同 pcl::logf_RVV）"));
  w_err = std::max(w_err, utf8_display_width("(2) 标量 remez1 abs"));
  w_err = std::max(w_err, utf8_display_width("(3) 标量 remez2 abs"));
  w_err = std::max(w_err, utf8_display_width("(4) 标量 lp abs"));
  w_err = std::max(w_err, utf8_display_width("(5) 标量 Sollya abs"));
#if defined(__RVV10__)
  w_err = std::max(w_err, utf8_display_width("(6) pcl::logf_RVV_f32m2（同(1)）"));
  w_err = std::max(w_err, utf8_display_width("(7) RVV remez1 abs"));
  w_err = std::max(w_err, utf8_display_width("(8) RVV remez2 abs"));
  w_err = std::max(w_err, utf8_display_width("(9) RVV lp abs"));
  w_err = std::max(w_err, utf8_display_width("(10) RVV Sollya abs"));
#else
  w_err = std::max(
      w_err, utf8_display_width("(RVV path disabled: compile without __RVV10__)"));
#endif
  w_err += 2;

  std::printf("=== logf approximation vs std::logf (n = %zu) ===\n", n);
  print_err_table_top(w_err);
  compute_errors(ref, out_s_base, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(1) 标量 float（同 pcl::logf_RVV）", max_rel, mean_abs);
  compute_errors(ref, out_s_r1, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(2) 标量 remez1 abs", max_rel, mean_abs);
  compute_errors(ref, out_s_r2, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(3) 标量 remez2 abs", max_rel, mean_abs);
  compute_errors(ref, out_s_lp, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(4) 标量 lp abs", max_rel, mean_abs);
  compute_errors(ref, out_s_sol, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(5) 标量 Sollya abs", max_rel, mean_abs);

#if defined(__RVV10__)
  run_rvv_common(xs, out_v_base);
  run_rvv_poly<LogfRemez1Coeff>(xs, out_v_r1);
  run_rvv_poly<LogfRemez2Coeff>(xs, out_v_r2);
  run_rvv_poly<LogfLpCoeff>(xs, out_v_lp);
  run_rvv_poly<LogfSollyaCoeff>(xs, out_v_sol);

  compute_errors(ref, out_v_base, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(6) pcl::logf_RVV_f32m2（同(1)）", max_rel, mean_abs);
  compute_errors(ref, out_v_r1, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(7) RVV remez1 abs", max_rel, mean_abs);
  compute_errors(ref, out_v_r2, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(8) RVV remez2 abs", max_rel, mean_abs);
  compute_errors(ref, out_v_lp, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(9) RVV lp abs", max_rel, mean_abs);
  compute_errors(ref, out_v_sol, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(10) RVV Sollya abs", max_rel, mean_abs);

#else
  print_err_table_rvv_disabled(w_err);
#endif
  print_err_table_close(w_err);
#if defined(__RVV10__)
  std::printf("[pcl::logf_RVV vs 标量 (1)] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_base, out_s_base)));
  std::printf("[RVV-r1   vs scalar-r1  ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_r1, out_s_r1)));
  std::printf("[RVV-r2   vs scalar-r2  ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_r2, out_s_r2)));
  std::printf("[RVV-lp   vs scalar-lp  ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_lp, out_s_lp)));
  std::printf("[RVV-sol  vs scalar-sol ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_sol, out_s_sol)));
#endif

  run_std_logf(xs, out_std);
  run_scalar_poly<LogfBaselineCoeff>(xs, out_s_base);
  run_scalar_poly_fast<LogfBaselineCoeff>(xs, out_s_base_fast);
  run_scalar_poly<LogfRemez1Coeff>(xs, out_s_r1);
  run_scalar_poly<LogfRemez2Coeff>(xs, out_s_r2);
  run_scalar_poly<LogfLpCoeff>(xs, out_s_lp);
  run_scalar_poly<LogfSollyaCoeff>(xs, out_s_sol);
#if defined(__RVV10__)
  run_rvv_common(xs, out_v_base);
  run_rvv_poly<LogfRemez1Coeff>(xs, out_v_r1);
  run_rvv_poly<LogfRemez2Coeff>(xs, out_v_r2);
  run_rvv_poly<LogfLpCoeff>(xs, out_v_lp);
  run_rvv_poly<LogfSollyaCoeff>(xs, out_v_sol);
#endif

  const double t_std = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_std_logf(xs, out_std);
  });
  const double t_s_base = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<LogfBaselineCoeff>(xs, out_s_base);
  });
  const double t_s_base_fast = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly_fast<LogfBaselineCoeff>(xs, out_s_base_fast);
  });
  const double t_s_r1 = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<LogfRemez1Coeff>(xs, out_s_r1);
  });
  const double t_s_r2 = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<LogfRemez2Coeff>(xs, out_s_r2);
  });
  const double t_s_lp = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<LogfLpCoeff>(xs, out_s_lp);
  });
  const double t_s_sol = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<LogfSollyaCoeff>(xs, out_s_sol);
  });

  int w_perf = 10;
  w_perf = std::max(w_perf, utf8_display_width("标量（同 logf_RVV）"));
  w_perf = std::max(w_perf, utf8_display_width("标量 fast（正normal）"));
  w_perf = std::max(w_perf, utf8_display_width("标量 remez1 abs"));
  w_perf = std::max(w_perf, utf8_display_width("标量 remez2 abs"));
  w_perf = std::max(w_perf, utf8_display_width("标量 lp abs"));
  w_perf = std::max(w_perf, utf8_display_width("标量 Sollya"));
#if defined(__RVV10__)
  w_perf = std::max(w_perf, utf8_display_width("pcl::logf_RVV_f32m2"));
  w_perf = std::max(w_perf, utf8_display_width("RVV remez1 abs"));
  w_perf = std::max(w_perf, utf8_display_width("RVV remez2 abs"));
  w_perf = std::max(w_perf, utf8_display_width("RVV lp abs"));
  w_perf = std::max(w_perf, utf8_display_width("RVV Sollya"));
#endif
  w_perf += 2;

  char note_buf[64];
  print_perf_table_header(n, iters, w_perf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_base);
  print_perf_table_row(w_perf, "标量（同 logf_RVV）", t_s_base, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_base_fast);
  print_perf_table_row(w_perf, "标量 fast（正normal）", t_s_base_fast, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_r1);
  print_perf_table_row(w_perf, "标量 remez1 abs", t_s_r1, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_r2);
  print_perf_table_row(w_perf, "标量 remez2 abs", t_s_r2, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_lp);
  print_perf_table_row(w_perf, "标量 lp abs", t_s_lp, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_sol);
  print_perf_table_row(w_perf, "标量 Sollya", t_s_sol, note_buf);

#if defined(__RVV10__)
  const double t_v_base = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_common(xs, out_v_base);
  });
  const double t_v_r1 = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<LogfRemez1Coeff>(xs, out_v_r1);
  });
  const double t_v_r2 = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<LogfRemez2Coeff>(xs, out_v_r2);
  });
  const double t_v_lp = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<LogfLpCoeff>(xs, out_v_lp);
  });
  const double t_v_sol = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<LogfSollyaCoeff>(xs, out_v_sol);
  });
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_base);
  print_perf_table_row(w_perf, "pcl::logf_RVV_f32m2", t_v_base, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_r1);
  print_perf_table_row(w_perf, "RVV remez1 abs", t_v_r1, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_r2);
  print_perf_table_row(w_perf, "RVV remez2 abs", t_v_r2, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_lp);
  print_perf_table_row(w_perf, "RVV lp abs", t_v_lp, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_sol);
  print_perf_table_row(w_perf, "RVV Sollya", t_v_sol, note_buf);
#endif
  print_perf_table_close(w_perf);

  return 0;
}
