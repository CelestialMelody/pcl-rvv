/*
 * atan2_test.cpp — 与 std::atan2 对比六种标量 atan 核系数来源 + RVV。
 *
 *  (1) 标量 mazzo.li 奇次核（系数同 pcl::atan2_RVV，common.hpp）
 *  (2) Remez 第一算法风格（parms_atan2.py：交换参考点 + 线性方程组；区间 [0,1]）
 *  (3) Remez 第二算法（parms_atan2.py：Powell 密栅 min max|err|，初值 LP）
 *  (4) 离散 LP（parms_atan2.py --method lp）
 *  (5) Sollya fpminimax 全次数 5（六常数 Horner；核为 P(t) 非 t*(a1+a3 t^2+…)）
 *  (6) Sollya fpminimax 全次数 11（十二常数 Horner；与 report 第 (6) 节同源）
 *  (7) __RVV10__: pcl::atan2_RVV_f32m2（系数同 (1)）
 *
 * 旧版「仅一种标量逼近 + 详细 printf」保留为同目录 atan2_test.bak.cpp。
 *
 * Build: g++ -std=c++17 -O3 -o atan2_test atan2_test.cpp -lm
 *        make atan2_test ARCH=riscv
 *
 * 表格：误差段为「竖线表格」；性能段首列宽度仅按 kernel 英文名计，避免被中文 Case 列撑宽（对齐思路同 analyze_bench_compare 定宽列）。
 * 表头量纲写作 max (rad)、mean (rad)。RVV 与标量 (1) 的 |diff| 单独一节，不插入前表。
 * 终端表格见 ../../script/term_table.hpp（math_test 命名空间），其它数学测试可复用。
 */
#include <algorithm>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../script/term_table.hpp"

#if defined(__RVV10__)
#include <riscv_vector.h>
#include <pcl/common/common.h>
#include <pcl/common/impl/rvv_math.hpp>
#include <cstddef>
#endif

namespace {

using math_test::utf8_display_width;

/** 与 ../../script/term_table.hpp 中默认一致的误差/性能列宽；其它测试可自定义 Table3NumCols / Perf2Cols。 */
constexpr math_test::Table3NumCols k_err_tbl{};
constexpr math_test::Perf2Cols k_perf_tbl{};

void print_err_table_header(std::size_t n_pts, int w_item_disp)
{
  std::printf("=== atan2 核 vs std::atan2, n = %zu ===\n", n_pts);
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

int err_table_line_columns(int w_item_disp)
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

const float k_pi = 3.141592653589793f;
const float k_pi_2 = 1.5707963267948966f;
const float tiny_f = 1e-20f;

// --- (1) 文章 ---
struct PolyMazzo {
  static constexpr float a1 = 0.99997726f;
  static constexpr float a3 = -0.33262347f;
  static constexpr float a5 = 0.19354346f;
  static constexpr float a7 = -0.11643287f;
  static constexpr float a9 = 0.05265332f;
  static constexpr float a11 = -0.01172120f;
};

// --- (2) remez1 / 交换点（parms_atan2.py remez_atan_odd_first，[0,1] 起点）---
struct PolyRemez1 {
  static constexpr float a1 = 0.9999772190799245f;
  static constexpr float a3 = -0.3326228278409405f;
  static constexpr float a5 = 0.1935403757741253f;
  static constexpr float a7 = -0.1164264811875351f;
  static constexpr float a9 = 0.05264735061895011f;
  static constexpr float a11 = -0.01171913540713792f;
};

// --- (3) remez2 / Powell（与 parms_atan2 默认 report 第 (3) 节一致）---
struct PolyRemez2 {
  static constexpr float a1 = 0.9999775690468202f;
  static constexpr float a3 = -0.3326270073651729f;
  static constexpr float a5 = 0.1935518578599886f;
  static constexpr float a7 = -0.1164322962576437f;
  static constexpr float a9 = 0.05263769152193183f;
  static constexpr float a11 = -0.01171128155647548f;
};

// --- (4) LP ---
struct PolyLp {
  static constexpr float a1 = 0.9999775803753757f;
  static constexpr float a3 = -0.3326270971752202f;
  static constexpr float a5 = 0.1935520249746597f;
  static constexpr float a7 = -0.1164321024444021f;
  static constexpr float a9 = 0.05263708887719762f;
  static constexpr float a11 = -0.01171095541590521f;
};

// --- (5) Sollya deg5 Horner: P(t)=c0+t*(c1+t*(...))，与奇次核不同 ---
struct PolySollyaDeg5 {
  static constexpr float c0 = 2.093957818039105e-05f;
  static constexpr float c1 = 0.9982532435543107f;
  static constexpr float c2 = 0.02366052995707943f;
  static constexpr float c3 = -0.4511472680586187f;
  static constexpr float c4 = 0.2641312491225646f;
  static constexpr float c5 = -0.04949959117788765f;
  static float eval_abs(float t_abs)
  {
    float t = t_abs;
    float p = c5;
    p = c4 + t * p;
    p = c3 + t * p;
    p = c2 + t * p;
    p = c1 + t * p;
    return c0 + t * p;
  }
  /** atan 为奇函数：在 [-1,1] 上用 sign(t)*P(|t|)，P 仅在 [0,1] 上与 Sollya 拟合一致。 */
  static float eval_atan_odd_extended(float t)
  {
    float a = std::fabs(t);
    float p = eval_abs(a);
    return (t >= 0.f) ? p : -p;
  }
};

// --- (6) Sollya deg11 Horner: P(t)=c0+t*(c1+t*(…))，与 deg5 同形；十二常数 ---
struct PolySollyaDeg11 {
  static constexpr float c0 = 1.692908826312289e-09f;
  static constexpr float c1 = 0.999999497998623f;
  static constexpr float c2 = 2.462210154373695e-05f;
  static constexpr float c3 = -0.33380530086035f;
  static constexpr float c4 = 0.004653922613137158f;
  static constexpr float c5 = 0.1731460010057108f;
  static constexpr float c6 = 0.09664877047164804f;
  static constexpr float c7 = -0.3642341265792391f;
  static constexpr float c8 = 0.3129036457771687f;
  static constexpr float c9 = -0.122116900202231f;
  static constexpr float c10 = 0.01736722164089919f;
  static constexpr float c11 = 0.0008108094305379294f;
  static float eval_abs(float t_abs)
  {
    float t = t_abs;
    float p = c11;
    p = c10 + t * p;
    p = c9 + t * p;
    p = c8 + t * p;
    p = c7 + t * p;
    p = c6 + t * p;
    p = c5 + t * p;
    p = c4 + t * p;
    p = c3 + t * p;
    p = c2 + t * p;
    p = c1 + t * p;
    return c0 + t * p;
  }
  static float eval_atan_odd_extended(float t)
  {
    float a = std::fabs(t);
    float p = eval_abs(a);
    return (t >= 0.f) ? p : -p;
  }
};

template <typename Coeffs>
float atan2_poly_odd(float y, float x)
{
  float abs_x = std::fabs(x);
  float abs_y = std::fabs(y);
  bool swap = abs_x < abs_y;
  float num = swap ? x : y;
  float den = swap ? y : x;
  if (std::fabs(den) < tiny_f)
    den = (den >= 0.f) ? tiny_f : -tiny_f;
  float atan_input = num / den;

  float x2 = atan_input * atan_input;
  float p = Coeffs::a11;
  p = Coeffs::a9 + x2 * p;
  p = Coeffs::a7 + x2 * p;
  p = Coeffs::a5 + x2 * p;
  p = Coeffs::a3 + x2 * p;
  p = Coeffs::a1 + x2 * p;
  float result = atan_input * p;

  if (swap)
    result = (atan_input >= 0.f ? k_pi_2 : -k_pi_2) - result;
  if (x < 0.f)
    result += (y >= 0.f ? k_pi : -k_pi);
  return result;
}

float atan2_poly_sollya5(float y, float x)
{
  float abs_x = std::fabs(x);
  float abs_y = std::fabs(y);
  bool swap = abs_x < abs_y;
  float num = swap ? x : y;
  float den = swap ? y : x;
  if (std::fabs(den) < tiny_f)
    den = (den >= 0.f) ? tiny_f : -tiny_f;
  float t = num / den;
  float result = PolySollyaDeg5::eval_atan_odd_extended(t);
  if (swap)
    result = (t >= 0.f ? k_pi_2 : -k_pi_2) - result;
  if (x < 0.f)
    result += (y >= 0.f ? k_pi : -k_pi);
  return result;
}

float atan2_poly_sollya11(float y, float x)
{
  float abs_x = std::fabs(x);
  float abs_y = std::fabs(y);
  bool swap = abs_x < abs_y;
  float num = swap ? x : y;
  float den = swap ? y : x;
  if (std::fabs(den) < tiny_f)
    den = (den >= 0.f) ? tiny_f : -tiny_f;
  float t = num / den;
  float result = PolySollyaDeg11::eval_atan_odd_extended(t);
  if (swap)
    result = (t >= 0.f ? k_pi_2 : -k_pi_2) - result;
  if (x < 0.f)
    result += (y >= 0.f ? k_pi : -k_pi);
  return result;
}

struct PolyCase {
  const char* label;
  float (*f)(float, float);
};

float run_mazzo(float y, float x) { return atan2_poly_odd<PolyMazzo>(y, x); }
float run_remez1(float y, float x) { return atan2_poly_odd<PolyRemez1>(y, x); }
float run_remez2(float y, float x) { return atan2_poly_odd<PolyRemez2>(y, x); }
float run_lp(float y, float x) { return atan2_poly_odd<PolyLp>(y, x); }
float run_sollya(float y, float x) { return atan2_poly_sollya5(y, x); }
float run_sollya11(float y, float x) { return atan2_poly_sollya11(y, x); }

const PolyCase kScalarCases[] = {
    {"(1) 标量 mazzo.li 奇次核（系数同 pcl::atan2_RVV）", run_mazzo},
    {"(2) Remez1 交换点（parms_atan2）", run_remez1},
    {"(3) Remez2 + Powell 密栅（parms_atan2）", run_remez2},
    {"(4) 离散 LP（parms_atan2）", run_lp},
    {"(5) Sollya fpminimax，全次项 Horner（6 系数）", run_sollya},
    {"(6) Sollya fpminimax，全次项 Horner（12 系数）", run_sollya11},
};

void run_batch(const float* ys, const float* xs, float* out, std::size_t n, float (*fn)(float, float))
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = fn(ys[i], xs[i]);
}

void run_std(const float* ys, const float* xs, float* out, std::size_t n)
{
  for (std::size_t i = 0; i < n; ++i)
    out[i] = std::atan2(ys[i], xs[i]);
}

// ---------------------------------------------------------------------------
// Performance loop vs -O3:
// If the outer loop repeatedly fills the same buffer from read-only inputs and
// nothing in the loop reads that buffer back, the compiler may legally fold
// iterations and execute only the last one (C++ as-if). That made the scalar
// `run_batch` loop look ~100× faster than reality after inlining.
//
// After each timed iteration, fold a few output samples into a volatile double
// so every iteration is observable; cost is negligible vs atan2 / poly work.
// ---------------------------------------------------------------------------
inline void bench_touch_output(const float* buf, std::size_t sz, volatile double* sink)
{
  if (sz == 0)
    return;
  *sink += static_cast<double>(buf[0]) + static_cast<double>(buf[sz - 1]);
  if (sz > 2)
    *sink += static_cast<double>(buf[sz / 2]);
}

#if defined(__RVV10__)
void run_rvv(const float* ys, const float* xs, float* out, std::size_t n)
{
  std::size_t j = 0;
  while (j < n) {
    std::size_t vl = __riscv_vsetvl_e32m2(n - j);
    vfloat32m2_t v_y = __riscv_vle32_v_f32m2(ys + j, vl);
    vfloat32m2_t v_x = __riscv_vle32_v_f32m2(xs + j, vl);
    vfloat32m2_t v_out = pcl::atan2_RVV_f32m2(v_y, v_x, vl);
    __riscv_vse32_v_f32m2(out + j, v_out, vl);
    j += vl;
  }
}
#endif

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

} // namespace

int main()
{
  static const char* k_try_locales[] = { "", "C.UTF-8", "zh_CN.UTF-8", "en_US.UTF-8" };
  for (const char* loc : k_try_locales) {
    if (std::setlocale(LC_ALL, loc))
      break;
  }

  const std::size_t n = 256 * 256;
  float* ys = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* xs = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* ref = static_cast<float*>(std::malloc(n * sizeof(float)));
  float* out = static_cast<float*>(std::malloc(n * sizeof(float)));
#if defined(__RVV10__)
  float* out_rvv = static_cast<float*>(std::malloc(n * sizeof(float)));
#endif

  if (!ys || !xs || !ref || !out) {
    std::fprintf(stderr, "malloc failed\n");
    return 1;
  }
#if defined(__RVV10__)
  if (!out_rvv) {
    std::fprintf(stderr, "malloc failed (rvv)\n");
    return 1;
  }
#endif

  std::size_t idx = 0;
  for (int iy = 0; iy < 256; ++iy) {
    float y = (iy == 0) ? -1.f : ((iy == 255) ? 1.f : (-1.f + 2.f * iy / 255.f));
    for (int ix = 0; ix < 256; ++ix) {
      float x = (ix == 0) ? -1.f : ((ix == 255) ? 1.f : (-1.f + 2.f * ix / 255.f));
      ys[idx] = y;
      xs[idx] = x;
      ref[idx] = std::atan2(y, x);
      ++idx;
    }
  }

  /* 仅 (1)–(7) 误差表：|diff| 不计入标签列宽（单独小节） */
  int w_item_disp = 24;
  for (const PolyCase& pc : kScalarCases)
    w_item_disp = std::max(w_item_disp, utf8_display_width(pc.label));
#if defined(__RVV10__)
  w_item_disp = std::max(
      w_item_disp,
      utf8_display_width("(7) pcl::atan2_RVV_f32m2（系数同 (1)）"));
#endif
  w_item_disp += 1;

  print_err_table_header(n, w_item_disp);
  for (const PolyCase& pc : kScalarCases) {
    run_batch(ys, xs, out, n, pc.f);
    float max_r, max_d;
    double mean_r;
    compute_errors(ref, out, n, max_r, max_d, mean_r);
    print_err_table_row(
        w_item_disp,
        pc.label,
        static_cast<double>(max_r),
        static_cast<double>(max_d),
        mean_r);
  }

#if defined(__RVV10__)
  run_rvv(ys, xs, out_rvv, n);
  {
    float max_r, max_d;
    double mean_r;
    compute_errors(ref, out_rvv, n, max_r, max_d, mean_r);
    print_err_table_row(
        w_item_disp,
        "(7) pcl::atan2_RVV_f32m2（系数同 (1)）",
        static_cast<double>(max_r),
        static_cast<double>(max_d),
        mean_r);
  }
  math_test::print_rule_chars('=', err_table_line_columns(w_item_disp));

  {
    run_batch(ys, xs, out, n, run_mazzo);
    float max_diff = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      float d = std::fabs(out_rvv[i] - out[i]);
      if (d > max_diff)
        max_diff = d;
    }
    const int col0 = 6;
    const char* diff_lbl = "max |diff| :";
    std::printf("\n[pcl::atan2_RVV vs 标量 (1)]\n");
    std::printf("  %-*s  %*.*e\n", col0, diff_lbl, col0, 6, static_cast<double>(max_diff));
  }
#else
  math_test::print_rule_chars('=', err_table_line_columns(w_item_disp));
#endif

  const int iters = 100;

  float* buf_std = static_cast<float*>(std::malloc(n * sizeof(float)));
  if (!buf_std) {
    std::free(ys);
    std::free(xs);
    std::free(ref);
    std::free(out);
#if defined(__RVV10__)
    std::free(out_rvv);
#endif
    return 1;
  }

  volatile double bench_sink = 0.0;

  run_std(ys, xs, buf_std, n);
  run_batch(ys, xs, out, n, run_mazzo);
#if defined(__RVV10__)
  run_rvv(ys, xs, out_rvv, n);
#endif

  auto t0 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_std(ys, xs, buf_std, n);
    bench_touch_output(buf_std, n, &bench_sink);
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_batch(ys, xs, out, n, run_mazzo);
    bench_touch_output(out, n, &bench_sink);
  }
  auto t2 = std::chrono::high_resolution_clock::now();

  /* 性能段首列只按英文 kernel 名定宽，避免与中文 Case 列同宽撑表（风格近 analyze_bench_compare） */
  int w_perf_lab = 10;
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("scalar mazzo"));
#if defined(__RVV10__)
  w_perf_lab = std::max(w_perf_lab, utf8_display_width("pcl::atan2_RVV"));
#endif
  w_perf_lab += 2;

  print_perf_table_header(n, iters, w_perf_lab);

  double ms_std = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double ms_scal = std::chrono::duration<double, std::milli>(t2 - t1).count();
  char note_buf[64];
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_scal);
  print_perf_table_row(w_perf_lab, "scalar mazzo", ms_scal, note_buf);
#if defined(__RVV10__)
  auto t3 = std::chrono::high_resolution_clock::now();
  for (int t = 0; t < iters; ++t) {
    run_rvv(ys, xs, out_rvv, n);
    bench_touch_output(out_rvv, n, &bench_sink);
  }
  auto t4 = std::chrono::high_resolution_clock::now();
  double ms_r = std::chrono::duration<double, std::milli>(t4 - t3).count();
  std::snprintf(note_buf, sizeof(note_buf), "%.2fx vs std", ms_std / ms_r);
  print_perf_table_row(w_perf_lab, "pcl::atan2_RVV", ms_r, note_buf);
  print_perf_table_close(w_perf_lab);
  std::free(out_rvv);
#else
  print_perf_table_close(w_perf_lab);
#endif

  (void)bench_sink;

  std::free(buf_std);
  std::free(ys);
  std::free(xs);
  std::free(ref);
  std::free(out);
  return 0;
}
