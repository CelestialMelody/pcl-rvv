/*
 * expf_test.cpp — 多路 expf 参数对比（标量 + RVV）
 *
 * 比较对象：
 *   (1) 本文件内标量 float Horner；系数与 common.hpp 中 expf_RVV 所用 remez1-rel 常数一致（库内无单独标量 expf API）
 *   (2) parms_expf.py remez1（绝对误差，第一算法风格）
 *   (3) parms_expf.py remez1-rel（相对误差；当前与 (1)/common.hpp 同系数，用于候选复核）
 *   (4) parms_expf.py remez2-rel（相对误差）
 *   (5) parms_expf.py lp-rel（相对误差）
 *   (6) parms_expf.py sollya(fpminimax, relative)
 *
 * RVV:
 *   - (7) 直接调用 pcl::expf_RVV_f32m2
 *   - (8)–(12) 同一 RVV 约化与重构路径，仅替换 Horner 系数
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
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

static constexpr float kLog2Inv = 1.442695040888963f;
static constexpr float kLog2Hi = 0.6931471824645996f;
static constexpr float kLog2Lo = -1.904654290582768e-09f;
static constexpr float kTwoToMinus127 = 5.877471754111438e-39f;
static constexpr float kXMax = 88.0f;
static constexpr float kXMin = -88.0f;
static constexpr float kMaxAbsRefThreshold = 1e10f;

struct ExpfBaselineCoeff {
  static constexpr float c0 = 0.9999999999876557f;
  static constexpr float c1 = 1.000000000027863f;
  static constexpr float c2 = 0.5000000053614374f;
  static constexpr float c3 = 0.16666666439294f;
  static constexpr float c4 = 0.04166635362288752f;
  static constexpr float c5 = 0.008333359419394903f;
  static constexpr float c6 = 0.001394106053653905f;
  static constexpr float c7 = 0.0001986611354469939f;
};

struct ExpfRemez1Coeff {
  static constexpr float c0 = 0.9999999999875628f;
  static constexpr float c1 = 0.9999999999949485f;
  static constexpr float c2 = 0.5000000053852762f;
  static constexpr float c3 = 0.1666666676245945f;
  static constexpr float c4 = 0.04166635288798774f;
  static constexpr float c5 = 0.008333290857447563f;
  static constexpr float c6 = 0.001394110951809275f;
  static constexpr float c7 = 0.0001990342095739361f;
};

struct ExpfRemez1RelCoeff {
  static constexpr float c0 = 0.9999999999876557f;
  static constexpr float c1 = 1.000000000027863f;
  static constexpr float c2 = 0.5000000053614374f;
  static constexpr float c3 = 0.16666666439294f;
  static constexpr float c4 = 0.04166635362288752f;
  static constexpr float c5 = 0.008333359419394903f;
  static constexpr float c6 = 0.001394106053653905f;
  static constexpr float c7 = 0.0001986611354469939f;
};

struct ExpfRemez2RelCoeff {
  static constexpr float c0 = 1.000000000728035f;
  static constexpr float c1 = 1.000000050005224f;
  static constexpr float c2 = 0.5000000729575707f;
  static constexpr float c3 = 0.1666638095880147f;
  static constexpr float c4 = 0.04166471714273724f;
  static constexpr float c5 = 0.008377074610962506f;
  static constexpr float c6 = 0.001401267315781624f;
  static constexpr float c7 = -6.450792317356219e-07f;
};

struct ExpfLpRelCoeff {
  static constexpr float c0 = 1.000000000814267f;
  static constexpr float c1 = 1.000000050904498f;
  static constexpr float c2 = 0.5000000751242663f;
  static constexpr float c3 = 0.1666638177995045f;
  static constexpr float c4 = 0.04166474213130857f;
  static constexpr float c5 = 0.008377186523046082f;
  static constexpr float c6 = 0.001401616120109518f;
  static constexpr float c7 = 0.0f;
};

struct ExpfSollyaCoeff {
  static constexpr float c0 = 0.999999999961682f;
  static constexpr float c1 = 1.000000000243097f;
  static constexpr float c2 = 0.5000000104536195f;
  static constexpr float c3 = 0.1666666512613725f;
  static constexpr float c4 = 0.04166622542551786f;
  static constexpr float c5 = 0.008333561090240499f;
  static constexpr float c6 = 0.001394818332705201f;
  static constexpr float c7 = 0.0001977517158175297f;
};

template <typename C>
inline float
expf_poly_scalar(float x)
{
  if (std::isnan(x))
    return std::numeric_limits<float>::quiet_NaN();
  if (std::isinf(x))
    return (x > 0.f) ? std::numeric_limits<float>::infinity() : 0.f;

  x = std::fmax(std::fmin(x, kXMax), kXMin);
  float flt_n = x * kLog2Inv;
  // 与 RVV vfcvt_x_f 目标语义对齐：默认舍入模式下取“就近”整数
  const int32_t n = static_cast<int32_t>(std::nearbyintf(flt_n));
  flt_n = static_cast<float>(n);
  const float r = x - flt_n * kLog2Hi - flt_n * kLog2Lo;

  float poly = C::c7;
  poly = C::c6 + r * poly;
  poly = C::c5 + r * poly;
  poly = C::c4 + r * poly;
  poly = C::c3 + r * poly;
  poly = C::c2 + r * poly;
  poly = C::c1 + r * poly;
  const float exp_r = C::c0 + r * poly;
  // 与 RVV 路径同构：位级构造 2^n（含 n=-127 的最小正规边界处理）
  int32_t exp_off = n + 127;
  exp_off = std::max(0, std::min(255, exp_off));
  std::uint32_t bits = static_cast<std::uint32_t>(exp_off) << 23;
  float two_n_normal = 0.0f;
  std::memcpy(&two_n_normal, &bits, sizeof(two_n_normal));
  const float two_n = (n == -127) ? kTwoToMinus127 : two_n_normal;
  return exp_r * two_n;
}

template <typename C>
void
run_scalar_poly(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i)
    out[i] = expf_poly_scalar<C>(xs[i]);
}

void
run_std_expf(const std::vector<float>& xs, std::vector<float>& out)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i)
    out[i] = std::expf(xs[i]);
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
    const vfloat32m2_t vy = pcl::expf_RVV_f32m2(vx, vl);
    __riscv_vse32_v_f32m2(out.data() + j, vy, vl);
    j += vl;
  }
}

template <typename C>
inline vfloat32m2_t
eval_expf_rvv_poly(vfloat32m2_t vx, std::size_t vl)
{
  vx = __riscv_vfmin_vf_f32m2(__riscv_vfmax_vf_f32m2(vx, kXMin, vl), kXMax, vl);

  vfloat32m2_t flt_n = __riscv_vfmul_vf_f32m2(vx, kLog2Inv, vl);
  vint32m2_t n_i = __riscv_vfcvt_x_f_v_i32m2(flt_n, vl);
  flt_n = __riscv_vfcvt_f_x_v_f32m2(n_i, vl);

  vfloat32m2_t r = __riscv_vfnmsub_vf_f32m2(flt_n, kLog2Hi, vx, vl);
  r = __riscv_vfnmsub_vf_f32m2(flt_n, kLog2Lo, r, vl);

  vfloat32m2_t poly = __riscv_vfmv_v_f_f32m2(C::c7, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c6, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c5, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c4, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c3, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c2, vl), r, poly, vl);
  poly = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c1, vl), r, poly, vl);
  vfloat32m2_t exp_r = __riscv_vfmacc_vv_f32m2(__riscv_vfmv_v_f_f32m2(C::c0, vl), r, poly, vl);

  vint32m2_t exp_offset = __riscv_vadd_vx_i32m2(n_i, 127, vl);
  exp_offset = __riscv_vmax_vx_i32m2(exp_offset, 0, vl);
  exp_offset = __riscv_vmin_vx_i32m2(exp_offset, 255, vl);
  vuint32m2_t res_bits =
      __riscv_vsll_vx_u32m2(__riscv_vreinterpret_v_i32m2_u32m2(exp_offset), 23, vl);
  vfloat32m2_t two_n_normal = __riscv_vreinterpret_v_u32m2_f32m2(res_bits);
  const vbool16_t is_n_neg127 = __riscv_vmseq_vx_i32m2_b16(n_i, -127, vl);
  vfloat32m2_t two_n_sub = __riscv_vfmv_v_f_f32m2(kTwoToMinus127, vl);
  vfloat32m2_t two_n = __riscv_vmerge_vvm_f32m2(two_n_normal, two_n_sub, is_n_neg127, vl);
  return __riscv_vfmul_vv_f32m2(exp_r, two_n, vl);
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
    const vfloat32m2_t vy = eval_expf_rvv_poly<C>(vx, vl);
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
  std::vector<float> out_s_base(n), out_s_r1(n), out_s_r1r(n), out_s_r2r(n), out_s_lpr(n), out_s_sol(n);
#if defined(__RVV10__)
  std::vector<float> out_v_base(n), out_v_r1(n), out_v_r1r(n), out_v_r2r(n), out_v_lpr(n), out_v_sol(n);
#endif

  for (std::size_t i = 0; i < n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n - 1);
    const float x = kXMin + (kXMax - kXMin) * t;
    xs[i] = x;
    ref[i] = std::expf(x);
  }

  run_scalar_poly<ExpfBaselineCoeff>(xs, out_s_base);
  run_scalar_poly<ExpfRemez1Coeff>(xs, out_s_r1);
  run_scalar_poly<ExpfRemez1RelCoeff>(xs, out_s_r1r);
  run_scalar_poly<ExpfRemez2RelCoeff>(xs, out_s_r2r);
  run_scalar_poly<ExpfLpRelCoeff>(xs, out_s_lpr);
  run_scalar_poly<ExpfSollyaCoeff>(xs, out_s_sol);

  float max_abs = 0.0f, max_rel = 0.0f;
  double mean_abs = 0.0;

  int w_err = 10;
  w_err = std::max(w_err, utf8_display_width("(1) 标量 remez1-rel（同 common.hpp）"));
  w_err = std::max(w_err, utf8_display_width("(2) 标量 remez1 abs"));
  w_err = std::max(w_err, utf8_display_width("(3) 标量 remez1-rel（候选复核，同(1)）"));
  w_err = std::max(w_err, utf8_display_width("(4) 标量 remez2-rel"));
  w_err = std::max(w_err, utf8_display_width("(5) 标量 lp-rel"));
  w_err = std::max(w_err, utf8_display_width("(6) 标量 Sollya rel"));
#if defined(__RVV10__)
  w_err = std::max(w_err, utf8_display_width("(7) pcl::expf_RVV_f32m2（remez1-rel，同(1)）"));
  w_err = std::max(w_err, utf8_display_width("(8) RVV remez1 abs"));
  w_err = std::max(w_err, utf8_display_width("(9) RVV remez1-rel（候选复核，同(7)）"));
  w_err = std::max(w_err, utf8_display_width("(10) RVV remez2-rel"));
  w_err = std::max(w_err, utf8_display_width("(11) RVV lp-rel"));
  w_err = std::max(w_err, utf8_display_width("(12) RVV Sollya rel"));
#else
  w_err = std::max(
      w_err, utf8_display_width("(RVV path disabled: compile without __RVV10__)"));
#endif
  w_err += 2;

  std::printf("=== expf approximation vs std::expf (n = %zu) ===\n", n);
  print_err_table_top(w_err);
  compute_errors(ref, out_s_base, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(1) 标量 remez1-rel（同 common.hpp）", max_rel, mean_abs);
  compute_errors(ref, out_s_r1, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(2) 标量 remez1 abs", max_rel, mean_abs);
  compute_errors(ref, out_s_r1r, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(3) 标量 remez1-rel（候选复核，同(1)）", max_rel, mean_abs);
  compute_errors(ref, out_s_r2r, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(4) 标量 remez2-rel", max_rel, mean_abs);
  compute_errors(ref, out_s_lpr, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(5) 标量 lp-rel", max_rel, mean_abs);
  compute_errors(ref, out_s_sol, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(6) 标量 Sollya rel", max_rel, mean_abs);

#if defined(__RVV10__)
  run_rvv_common(xs, out_v_base);
  run_rvv_poly<ExpfRemez1Coeff>(xs, out_v_r1);
  run_rvv_poly<ExpfRemez1RelCoeff>(xs, out_v_r1r);
  run_rvv_poly<ExpfRemez2RelCoeff>(xs, out_v_r2r);
  run_rvv_poly<ExpfLpRelCoeff>(xs, out_v_lpr);
  run_rvv_poly<ExpfSollyaCoeff>(xs, out_v_sol);

  compute_errors(ref, out_v_base, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(7) pcl::expf_RVV_f32m2（remez1-rel，同(1)）", max_rel, mean_abs);
  compute_errors(ref, out_v_r1, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(8) RVV remez1 abs", max_rel, mean_abs);
  compute_errors(ref, out_v_r1r, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(9) RVV remez1-rel（候选复核，同(7)）", max_rel, mean_abs);
  compute_errors(ref, out_v_r2r, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(10) RVV remez2-rel", max_rel, mean_abs);
  compute_errors(ref, out_v_lpr, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(11) RVV lp-rel", max_rel, mean_abs);
  compute_errors(ref, out_v_sol, max_abs, max_rel, mean_abs);
  print_err_table_row(w_err, "(12) RVV Sollya rel", max_rel, mean_abs);

#else
  print_err_table_rvv_disabled(w_err);
#endif
  print_err_table_close(w_err);
#if defined(__RVV10__)
  std::printf("[pcl::expf_RVV vs 标量 (1)] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_base, out_s_base)));
  std::printf("[RVV-r1   vs scalar-r1  ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_r1, out_s_r1)));
  std::printf("[RVV-r1r  vs scalar-r1r ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_r1r, out_s_r1r)));
  std::printf("[RVV-r2r  vs scalar-r2r ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_r2r, out_s_r2r)));
  std::printf("[RVV-lpr  vs scalar-lpr ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_lpr, out_s_lpr)));
  std::printf("[RVV-sol  vs scalar-sol ] max |diff| : %.6e\n", static_cast<double>(max_diff(out_v_sol, out_s_sol)));
#endif

  run_std_expf(xs, out_std);
  run_scalar_poly<ExpfBaselineCoeff>(xs, out_s_base);
  run_scalar_poly<ExpfRemez1Coeff>(xs, out_s_r1);
  run_scalar_poly<ExpfRemez1RelCoeff>(xs, out_s_r1r);
  run_scalar_poly<ExpfRemez2RelCoeff>(xs, out_s_r2r);
  run_scalar_poly<ExpfLpRelCoeff>(xs, out_s_lpr);
  run_scalar_poly<ExpfSollyaCoeff>(xs, out_s_sol);
#if defined(__RVV10__)
  run_rvv_common(xs, out_v_base);
  run_rvv_poly<ExpfRemez1Coeff>(xs, out_v_r1);
  run_rvv_poly<ExpfRemez1RelCoeff>(xs, out_v_r1r);
  run_rvv_poly<ExpfRemez2RelCoeff>(xs, out_v_r2r);
  run_rvv_poly<ExpfLpRelCoeff>(xs, out_v_lpr);
  run_rvv_poly<ExpfSollyaCoeff>(xs, out_v_sol);
#endif

  const double t_std = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_std_expf(xs, out_std);
  });
  const double t_s_base = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<ExpfBaselineCoeff>(xs, out_s_base);
  });
  const double t_s_r1 = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<ExpfRemez1Coeff>(xs, out_s_r1);
  });
  const double t_s_r1r = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<ExpfRemez1RelCoeff>(xs, out_s_r1r);
  });
  const double t_s_r2r = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<ExpfRemez2RelCoeff>(xs, out_s_r2r);
  });
  const double t_s_lpr = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<ExpfLpRelCoeff>(xs, out_s_lpr);
  });
  const double t_s_sol = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_scalar_poly<ExpfSollyaCoeff>(xs, out_s_sol);
  });

  int w_perf = 10;
  w_perf = std::max(w_perf, utf8_display_width("标量 remez1-rel（同 common.hpp）"));
  w_perf = std::max(w_perf, utf8_display_width("标量 remez1 abs"));
  w_perf = std::max(w_perf, utf8_display_width("标量 remez1-rel"));
  w_perf = std::max(w_perf, utf8_display_width("标量 remez2-rel"));
  w_perf = std::max(w_perf, utf8_display_width("标量 lp-rel"));
  w_perf = std::max(w_perf, utf8_display_width("标量 Sollya"));
#if defined(__RVV10__)
  w_perf = std::max(w_perf, utf8_display_width("pcl::expf_RVV_f32m2（remez1-rel）"));
  w_perf = std::max(w_perf, utf8_display_width("RVV remez1 abs"));
  w_perf = std::max(w_perf, utf8_display_width("RVV remez1-rel"));
  w_perf = std::max(w_perf, utf8_display_width("RVV remez2-rel"));
  w_perf = std::max(w_perf, utf8_display_width("RVV lp-rel"));
  w_perf = std::max(w_perf, utf8_display_width("RVV Sollya"));
#endif
  w_perf += 2;

  char note_buf[64];
  print_perf_table_header(n, iters, w_perf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_base);
  print_perf_table_row(w_perf, "标量 remez1-rel（同 common.hpp）", t_s_base, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_r1);
  print_perf_table_row(w_perf, "标量 remez1 abs", t_s_r1, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_r1r);
  print_perf_table_row(w_perf, "标量 remez1-rel", t_s_r1r, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_r2r);
  print_perf_table_row(w_perf, "标量 remez2-rel", t_s_r2r, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_lpr);
  print_perf_table_row(w_perf, "标量 lp-rel", t_s_lpr, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_s_sol);
  print_perf_table_row(w_perf, "标量 Sollya", t_s_sol, note_buf);

#if defined(__RVV10__)
  const double t_v_base = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_common(xs, out_v_base);
  });
  const double t_v_r1 = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<ExpfRemez1Coeff>(xs, out_v_r1);
  });
  const double t_v_r1r = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<ExpfRemez1RelCoeff>(xs, out_v_r1r);
  });
  const double t_v_r2r = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<ExpfRemez2RelCoeff>(xs, out_v_r2r);
  });
  const double t_v_lpr = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<ExpfLpRelCoeff>(xs, out_v_lpr);
  });
  const double t_v_sol = time_ms([&]() {
    for (int k = 0; k < iters; ++k)
      run_rvv_poly<ExpfSollyaCoeff>(xs, out_v_sol);
  });
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_base);
  print_perf_table_row(w_perf, "pcl::expf_RVV_f32m2（remez1-rel）", t_v_base, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_r1);
  print_perf_table_row(w_perf, "RVV remez1 abs", t_v_r1, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_r1r);
  print_perf_table_row(w_perf, "RVV remez1-rel", t_v_r1r, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_r2r);
  print_perf_table_row(w_perf, "RVV remez2-rel", t_v_r2r, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_lpr);
  print_perf_table_row(w_perf, "RVV lp-rel", t_v_lpr, note_buf);
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", t_std / t_v_sol);
  print_perf_table_row(w_perf, "RVV Sollya", t_v_sol, note_buf);
#endif
  print_perf_table_close(w_perf);

  return 0;
}
