/*
 * sincos_test.cpp - finite-domain paired sinf/cosf 数学专项测试。
 *
 * 本文件用于验证周期函数数学流程，不是生产调用方接入测试。当前
 * lp-abs-hi-lo RVV 路径会调用 rvv_math.hpp 中的受限生产接入原型，但没有接
 * RangeImage / RangeImageSpherical 的生产路径。
 *
 * 阅读提示：
 *   1. run_scalar 驱动 scalar same-chain（标量同构链路），用于算法对拍，
 *      不是 libm 参考链路。
 *   2. run_std_sincos / run_float_libm_sincos 是参考和 benchmark（性能测试）
 *      baseline（基线）；它们可以调用 libm，RVV 快速路径不可以。
 *   3. run_rvv_scratch 是测试薄包装。lp-abs-hi-lo 会走
 *      rvv_sincos_finite_domain -> sincos_finite_domain_RVV_f32m2，也就是
 *      当前被审查的 lane-level helper（单个 RVV 向量寄存器级 helper）。
 *   4. dense（密集网格）和 adversarial（边界对抗点）检查近似误差；
 *      special（特殊值）和 signed-zero（带符号零）检查合同分类和 bit-level
 *      验收条件，失败会返回非 0。
 *   5. RVV 同构链路会先比较 RVV 输出与标量同构链路，包括 NaN/非有限分类；
 *      正确性验收通过后才考虑性能信号。
 *   6. --bench 在正确性验收通过后运行。板卡计时才是性能证据；
 *      QEMU timing（QEMU 计时）只看作结构信号。
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "sincos_finite_domain.hpp"

namespace {

namespace scratch = sincos_scratch;
using scratch::ApproxConfig;
using scratch::kConfigs;
using scratch::kPi;
using scratch::kPi2;
using scratch::kPi4;
using scratch::k3Pi4;
using scratch::in_contract_domain;

struct ErrorStats {
  double sin_max_abs = 0.0;
  double cos_max_abs = 0.0;
  double sin_mean_abs = 0.0;
  double cos_mean_abs = 0.0;
  float sin_worst_x = 0.0f;
  float cos_worst_x = 0.0f;
  std::uint64_t sin_max_ulp = 0;
  std::uint64_t cos_max_ulp = 0;
  float sin_ulp_worst_x = 0.0f;
  float cos_ulp_worst_x = 0.0f;
};

struct SpecialStats {
  int total = 0;
  int in_domain = 0;
  int domain_out = 0;
  int nan_in = 0;
  int inf_in = 0;
  int subnormal_in = 0;
  int signed_zero_in = 0;
  int both_nan_out = 0;
  int finite_out = 0;
};

struct DiffStats {
  float max_abs = 0.0f;
  int nan_mismatch = 0;
  int nonfinite_mismatch = 0;
};

struct BenchResult {
  const char* label;
  std::vector<double> samples_ms;
  double min_ms;
  double median_ms;
  double checksum;
};

inline float ref_sin_f32(float x);
inline float ref_cos_f32(float x);
std::vector<float> make_dense_grid(float lo, float hi, std::size_t n);

constexpr double kAbsErrorGate = 2.0e-7;
constexpr float kSameChainDiffGate = 0.0f;
volatile double gBenchSink = 0.0;

void
run_scalar(
    const std::vector<float>& xs,
    std::vector<float>& out_s,
    std::vector<float>& out_c,
    const ApproxConfig& config)
{
  // 作用：批量调用标量同构 helper；调用者：dense/adversarial/special/bench。
  // 类别：标量/RVV 同构链路，不是 libm 参考链路。
  scratch::scalar_sincos_finite_domain(xs, out_s, out_c, config);
}

// double-ref（double 精度 libm 后转回 float）参考链路。作用：提供误差统计
// 目标；调用者：数学专项测试和 benchmark。类别：参考链路，不要求和 RVV
// chain（RVV 操作链）bitwise 一致。
void
run_std_sincos(const std::vector<float>& xs, std::vector<float>& out_s, std::vector<float>& out_c)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i) {
    out_s[i] = ref_sin_f32(xs[i]);
    out_c[i] = ref_cos_f32(xs[i]);
  }
}

// float-libm（float 形态 libm 调用）性能基线。作用：让板卡加速比
// 能和普通 float sin/cos 调用形态比较；调用者：run_benchmark。类别：性能测试 /
// 参考链路。
void
run_float_libm_sincos(const std::vector<float>& xs, std::vector<float>& out_s, std::vector<float>& out_c)
{
  const std::size_t n = xs.size();
  for (std::size_t i = 0; i < n; ++i) {
    out_s[i] = std::sin(xs[i]);
    out_c[i] = std::cos(xs[i]);
  }
}

#if defined(__RVV10__)
// RVV 测试入口。作用：把测试候选分流到固定 lane-level helper 或 experimental
// path（实验路径）。调用者：正确性、special、signed-zero 和 benchmark。
// 类别：RVV 执行链路。lp-abs-hi-lo 走接近生产形态的受限原型；
// 其它候选留在 test-rvv，避免 ApproxConfig 污染未来 rvv_math.hpp 入口。
void
run_rvv_scratch(
    const std::vector<float>& xs,
    std::vector<float>& out_s,
    std::vector<float>& out_c,
    const ApproxConfig& config)
{
  if (config.coeffs == scratch::kLpAbsHiLoConfig.coeffs && config.use_hi_lo_reduction) {
    scratch::rvv_sincos_finite_domain(xs, out_s, out_c);
    return;
  }
  scratch::rvv_sincos_finite_domain_experimental(xs, out_s, out_c, config);
}
#endif

// ULP（相邻浮点数单位）诊断工具。作用：把 float 映射到可排序整数空间。
// 调用者：compute_error_stats。类别：数学专项测试诊断；过零点附近 ULP 不作为
// 第一版 gate。
std::uint32_t
float_bits(float x)
{
  std::uint32_t u = 0;
  std::memcpy(&u, &x, sizeof(u));
  return u;
}

std::uint64_t
ordered_float_bits(float x)
{
  const std::uint32_t u = float_bits(x);
  if ((u & 0x80000000u) != 0u)
    return static_cast<std::uint64_t>(0x80000000u - (u & 0x7fffffffu));
  return static_cast<std::uint64_t>(0x80000000u) + static_cast<std::uint64_t>(u);
}

std::uint64_t
ulp_diff(float a, float b)
{
  if (!std::isfinite(a) || !std::isfinite(b))
    return 0;
  const std::uint64_t oa = ordered_float_bits(a);
  const std::uint64_t ob = ordered_float_bits(b);
  return (oa > ob) ? (oa - ob) : (ob - oa);
}

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

double
checksum_outputs(const std::vector<float>& s, const std::vector<float>& c)
{
  // 作用：计时后做完整 checksum，防止 benchmark 结果被 DCE（死代码删除）吞掉。
  // 调用者：run_bench_kernel。类别：benchmark。
  double sum = 0.0;
  for (std::size_t i = 0; i < s.size(); ++i)
    sum += static_cast<double>(s[i]) * 0.25 + static_cast<double>(c[i]) * 0.75;
  return sum;
}

double
median_sample(std::vector<double> samples)
{
  std::sort(samples.begin(), samples.end());
  const std::size_t n = samples.size();
  if ((n & 1u) != 0u)
    return samples[n / 2];
  return 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
}

// benchmark 共享计时器。作用：重复执行候选，并输出 min/median/samples。
// timed loop（计时循环）内只保留轻量 volatile guard（防 DCE 哨兵），完整
// checksum 放到计时后，避免测到 reduction（归约）而不是 sincos 候选。
// 调用者：run_benchmark。类别：benchmark。
template <typename Fn>
BenchResult
run_bench_kernel(const char* label, Fn&& fn, int iters, int repeats)
{
  constexpr std::size_t n = 1u << 18;
  std::vector<float> xs = make_dense_grid(-kPi, kPi, n);
  std::vector<float> s(n);
  std::vector<float> c(n);
  std::vector<double> samples_ms;
  samples_ms.reserve(static_cast<std::size_t>(repeats));

  fn(xs, s, c);
  for (int repeat = 0; repeat < repeats; ++repeat) {
    volatile float guard = 0.0f;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; ++i) {
      fn(xs, s, c);
      const std::size_t idx =
          (static_cast<std::size_t>(i) * 9973u + static_cast<std::size_t>(repeat) * 101u) &
          (n - 1u);
      guard = s[idx] + c[idx];
    }
    const auto t1 = std::chrono::steady_clock::now();
    samples_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    gBenchSink += static_cast<double>(guard) * 1.0e-30;
  }
  const double checksum = checksum_outputs(s, c);
  const double min_ms = *std::min_element(samples_ms.begin(), samples_ms.end());
  const double median_ms = median_sample(samples_ms);
  return {label, samples_ms, min_ms, median_ms, checksum};
}

void
print_bench_result(const BenchResult& r, double baseline_ms)
{
  std::printf("%-30s min = %10.3f ms, median = %10.3f ms, speedup min/median vs double-ref = %7.3fx / %7.3fx, checksum = %.9e\n",
              r.label,
              r.min_ms,
              r.median_ms,
              baseline_ms / r.min_ms,
              baseline_ms / r.median_ms,
              r.checksum);
  std::printf("  samples ms:");
  for (double ms : r.samples_ms)
    std::printf(" %.3f", ms);
  std::printf("\n");
}

void
print_direct_speedup(const char* label, const BenchResult& numerator, const BenchResult& denominator)
{
  std::printf("[bench speedup] %s: min %.3fx, median %.3fx\n",
              label,
              denominator.min_ms / numerator.min_ms,
              denominator.median_ms / numerator.median_ms);
}

void
run_benchmark()
{
  // 作用：统一比较 double-ref、float-libm、标量同构链路和 RVV 执行链路。
  // 调用者：main --bench。类别：benchmark；只有板卡结果可作为性能证据。
  constexpr int kIters = 50;
  constexpr int kRepeats = 5;
  std::printf("\n=== microbench, n = %u, iters = %d, repeats = %d ===\n",
              1u << 18,
              kIters,
              kRepeats);
  std::printf("performance signal is meaningful on board; QEMU timing is correctness-only noise.\n");
  std::printf("timed loop uses a lightweight volatile guard; checksum is computed after each repeat.\n");

  const BenchResult std_ref = run_bench_kernel(
      "double-ref std::sin/cos",
      [](const std::vector<float>& xs, std::vector<float>& s, std::vector<float>& c) {
        run_std_sincos(xs, s, c);
      },
      kIters,
      kRepeats);
  print_bench_result(std_ref, std_ref.median_ms);

  const BenchResult float_libm = run_bench_kernel(
      "float-libm sin/cos",
      [](const std::vector<float>& xs, std::vector<float>& s, std::vector<float>& c) {
        run_float_libm_sincos(xs, s, c);
      },
      kIters,
      kRepeats);
  print_bench_result(float_libm, std_ref.median_ms);

  const BenchResult scalar_taylor = run_bench_kernel(
      "scalar taylor-hi-lo",
      [](const std::vector<float>& xs, std::vector<float>& s, std::vector<float>& c) {
        run_scalar(xs, s, c, kConfigs[1]);
      },
      kIters,
      kRepeats);
  print_bench_result(scalar_taylor, std_ref.median_ms);

  const BenchResult scalar_lp = run_bench_kernel(
      "scalar lp-abs-hi-lo",
      [](const std::vector<float>& xs, std::vector<float>& s, std::vector<float>& c) {
        run_scalar(xs, s, c, kConfigs[3]);
      },
      kIters,
      kRepeats);
  print_bench_result(scalar_lp, std_ref.median_ms);

#if defined(__RVV10__)
  const BenchResult rvv_taylor = run_bench_kernel(
      "RVV taylor-hi-lo",
      [](const std::vector<float>& xs, std::vector<float>& s, std::vector<float>& c) {
        run_rvv_scratch(xs, s, c, kConfigs[1]);
      },
      kIters,
      kRepeats);
  print_bench_result(rvv_taylor, std_ref.median_ms);

  const BenchResult rvv_lp = run_bench_kernel(
      "RVV lp-abs-hi-lo",
      [](const std::vector<float>& xs, std::vector<float>& s, std::vector<float>& c) {
        run_rvv_scratch(xs, s, c, kConfigs[3]);
      },
      kIters,
      kRepeats);
  print_bench_result(rvv_lp, std_ref.median_ms);
  print_direct_speedup("RVV taylor-hi-lo vs scalar taylor-hi-lo", rvv_taylor, scalar_taylor);
  print_direct_speedup("RVV lp-abs-hi-lo vs scalar lp-abs-hi-lo", rvv_lp, scalar_lp);
  print_direct_speedup("RVV lp-abs-hi-lo vs float-libm sin/cos", rvv_lp, float_libm);
#else
  std::printf("RVV benchmark disabled: build without __RVV10__\n");
#endif
  std::printf("bench guard sink = %.9e\n", gBenchSink);
}

// 误差验收统计。作用：对合同域内输入比较 double-ref libm；
// domain-out（域外）由 special/domain-out matrix（特殊值矩阵）单独测试。
// 调用者：run_candidate_case。类别：数学专项测试。
ErrorStats
compute_error_stats(
    const std::vector<float>& xs,
    const std::vector<float>& out_s,
    const std::vector<float>& out_c)
{
  ErrorStats st;
  std::size_t n_valid = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (!in_contract_domain(xs[i]))
      continue;
    const float ref_s = ref_sin_f32(xs[i]);
    const float ref_c = ref_cos_f32(xs[i]);
    const double sin_abs = std::fabs(static_cast<double>(out_s[i]) - static_cast<double>(ref_s));
    const double cos_abs = std::fabs(static_cast<double>(out_c[i]) - static_cast<double>(ref_c));
    st.sin_mean_abs += sin_abs;
    st.cos_mean_abs += cos_abs;
    if (sin_abs > st.sin_max_abs) {
      st.sin_max_abs = sin_abs;
      st.sin_worst_x = xs[i];
    }
    if (cos_abs > st.cos_max_abs) {
      st.cos_max_abs = cos_abs;
      st.cos_worst_x = xs[i];
    }
    const std::uint64_t sin_ulp = ulp_diff(out_s[i], ref_s);
    const std::uint64_t cos_ulp = ulp_diff(out_c[i], ref_c);
    if (sin_ulp > st.sin_max_ulp) {
      st.sin_max_ulp = sin_ulp;
      st.sin_ulp_worst_x = xs[i];
    }
    if (cos_ulp > st.cos_max_ulp) {
      st.cos_max_ulp = cos_ulp;
      st.cos_ulp_worst_x = xs[i];
    }
    ++n_valid;
  }
  if (n_valid != 0) {
    st.sin_mean_abs /= static_cast<double>(n_valid);
    st.cos_mean_abs /= static_cast<double>(n_valid);
  }
  return st;
}

DiffStats
same_chain_diff(const std::vector<float>& a, const std::vector<float>& b)
{
  // 作用：比较标量/RVV 同构输出。两边同为 NaN 视为一致，一边 NaN 一边非 NaN
  // 记为不一致；有限值才比较绝对差。调用者：run_candidate_case 和特殊值验收。
  // 类别：标量/RVV 同构链路验收。
  DiffStats st;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const bool a_nan = std::isnan(a[i]);
    const bool b_nan = std::isnan(b[i]);
    if (a_nan && b_nan)
      continue;
    if (a_nan != b_nan) {
      ++st.nan_mismatch;
      continue;
    }
    const bool a_finite = std::isfinite(a[i]);
    const bool b_finite = std::isfinite(b[i]);
    if (a_finite && b_finite) {
      st.max_abs = std::max(st.max_abs, std::fabs(a[i] - b[i]));
      continue;
    }
    if (float_bits(a[i]) != float_bits(b[i]))
      ++st.nonfinite_mismatch;
  }
  return st;
}

bool
same_chain_diff_ok(const DiffStats& st)
{
  return st.nan_mismatch == 0 && st.nonfinite_mismatch == 0 &&
         st.max_abs <= kSameChainDiffGate;
}

void
print_error_stats(const char* label, const ErrorStats& st, std::size_t n)
{
  std::printf("\n=== %s, n = %zu ===\n", label, n);
  std::printf("sin max abs = %.9e at x = %.9e, mean abs = %.9e, max ulp = %llu at x = %.9e\n",
              st.sin_max_abs,
              static_cast<double>(st.sin_worst_x),
              st.sin_mean_abs,
              static_cast<unsigned long long>(st.sin_max_ulp),
              static_cast<double>(st.sin_ulp_worst_x));
  std::printf("cos max abs = %.9e at x = %.9e, mean abs = %.9e, max ulp = %llu at x = %.9e\n",
              st.cos_max_abs,
              static_cast<double>(st.cos_worst_x),
              st.cos_mean_abs,
              static_cast<unsigned long long>(st.cos_max_ulp),
              static_cast<double>(st.cos_ulp_worst_x));
  std::printf("ULP is reported for diagnostics only; near-zero ULP is not a first-version gate.\n");
}

bool
check_abs_error_gate(const char* label, const ErrorStats& st)
{
  const bool ok = st.sin_max_abs <= kAbsErrorGate && st.cos_max_abs <= kAbsErrorGate;
  std::printf("[gate] %s abs error <= %.1e : %s\n",
              label,
              kAbsErrorGate,
              ok ? "PASS" : "FAIL");
  return ok;
}

// dense grid（密集网格）生成器。作用：覆盖完整合同域和 RangeImageSpherical
// 相关子域，捕获普通输入上的误差漂移。调用者：main。类别：数学专项测试。
std::vector<float>
make_dense_grid(float lo, float hi, std::size_t n)
{
  std::vector<float> xs(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(n - 1);
    xs[i] = static_cast<float>(static_cast<double>(lo) +
                               (static_cast<double>(hi) - static_cast<double>(lo)) * t);
  }
  return xs;
}

// adversarial points（边界对抗点）生成器。作用：覆盖 range reduction（范围约化）、
// sign/swap reconstruction（符号/交换重构）和过零 ULP 最脆弱的位置。
// 调用者：main。类别：数学专项测试。
void
append_nextafter_triplet(std::vector<float>& xs, float x)
{
  xs.push_back(std::nextafterf(x, -std::numeric_limits<float>::infinity()));
  xs.push_back(x);
  xs.push_back(std::nextafterf(x, std::numeric_limits<float>::infinity()));
}

std::vector<float>
make_adversarial_points()
{
  std::vector<float> xs;
  const float bases[] = {0.0f, -0.0f, kPi4, -kPi4, kPi2, -kPi2, k3Pi4, -k3Pi4, kPi, -kPi};
  for (float x : bases)
    append_nextafter_triplet(xs, x);
  return xs;
}

// special values matrix（特殊值矩阵）。作用：测试有限域合同而不是近似误差：
// NaN/Inf/large finite 应归为 domain-out NaN，subnormal（次正规数）在域内，
// signed zero 由位级 gate 单独检查。调用者：main。类别：数学专项测试。
std::vector<float>
make_special_points()
{
  const float den = std::numeric_limits<float>::denorm_min();
  const float inf = std::numeric_limits<float>::infinity();
  const float qnan = std::numeric_limits<float>::quiet_NaN();
  return {
      qnan,
      inf,
      -inf,
      0.0f,
      -0.0f,
      den,
      -den,
      std::numeric_limits<float>::max(),
      -std::numeric_limits<float>::max(),
      std::nextafterf(kPi, inf),
      std::nextafterf(-kPi, -inf),
      4.0f,
      -4.0f,
  };
}

SpecialStats
classify_special(const std::vector<float>& xs, const std::vector<float>& s, const std::vector<float>& c)
{
  SpecialStats st;
  st.total = static_cast<int>(xs.size());
  for (std::size_t i = 0; i < xs.size(); ++i) {
    const std::uint32_t bits = float_bits(xs[i]);
    const bool is_sub =
        ((bits & 0x7f800000u) == 0u) && ((bits & 0x007fffffu) != 0u);
    const bool is_zero = ((bits & 0x7fffffffu) == 0u);
    if (std::isnan(xs[i]))
      ++st.nan_in;
    if (std::isinf(xs[i]))
      ++st.inf_in;
    if (is_sub)
      ++st.subnormal_in;
    if (is_zero)
      ++st.signed_zero_in;
    if (in_contract_domain(xs[i]))
      ++st.in_domain;
    else
      ++st.domain_out;
    if (std::isnan(s[i]) && std::isnan(c[i]))
      ++st.both_nan_out;
    if (std::isfinite(s[i]) && std::isfinite(c[i]))
      ++st.finite_out;
  }
  return st;
}

void
print_special_stats(const SpecialStats& st)
{
  std::printf("\n=== special/domain-out classification ===\n");
  std::printf("total=%d in_domain=%d domain_out=%d nan_in=%d inf_in=%d subnormal_in=%d signed_zero_in=%d\n",
              st.total,
              st.in_domain,
              st.domain_out,
              st.nan_in,
              st.inf_in,
              st.subnormal_in,
              st.signed_zero_in);
  std::printf("both_nan_out=%d finite_out=%d\n", st.both_nan_out, st.finite_out);
}

// 位级带符号零验收。作用：确认 sin(-0) 保持 -0，
// cos(+0/-0) 均为 +1，防止看似无害但破坏合同的 merge。调用者：
// run_signed_zero_check。类别：数学专项测试验收。
void
print_bits(const char* label, float x)
{
  std::printf("%s = %.9e bits=0x%08x\n", label, static_cast<double>(x), float_bits(x));
}

bool
check_signed_zero_outputs(const char* label, const std::vector<float>& s, const std::vector<float>& c)
{
  const bool ok = float_bits(s[0]) == 0x00000000u &&
                  float_bits(s[1]) == 0x80000000u &&
                  float_bits(c[0]) == 0x3f800000u &&
                  float_bits(c[1]) == 0x3f800000u;
  std::printf("\n=== signed-zero check: %s ===\n", label);
  print_bits("sin(+0)", s[0]);
  print_bits("sin(-0)", s[1]);
  print_bits("cos(+0)", c[0]);
  print_bits("cos(-0)", c[1]);
  std::printf("[gate] signed zero contract: %s\n", ok ? "PASS" : "FAIL");
  return ok;
}

bool
run_signed_zero_check(const ApproxConfig& config)
{
  const std::vector<float> xs = {0.0f, -0.0f};
  std::vector<float> s(xs.size());
  std::vector<float> c(xs.size());
  run_scalar(xs, s, c, config);
  char label[128];
  std::snprintf(label, sizeof(label), "%s scalar same-chain", config.label);
  bool ok = check_signed_zero_outputs(label, s, c);

#if defined(__RVV10__)
  std::vector<float> rvv_s(xs.size());
  std::vector<float> rvv_c(xs.size());
  run_rvv_scratch(xs, rvv_s, rvv_c, config);
  std::snprintf(label, sizeof(label), "%s RVV scratch", config.label);
  ok = check_signed_zero_outputs(label, rvv_s, rvv_c) && ok;
#endif

  return ok;
}

// 单候选/单输入集验收。作用：把 double-ref 误差、绝对误差阈值和 RVV
// 同构链路一致性组合起来。调用者：main。类别：数学专项测试。
bool
run_candidate_case(const char* label, const std::vector<float>& xs, const ApproxConfig& config)
{
  std::vector<float> s(xs.size());
  std::vector<float> c(xs.size());
  run_scalar(xs, s, c, config);
  const ErrorStats st = compute_error_stats(xs, s, c);
  char full_label[160];
  std::snprintf(full_label, sizeof(full_label), "%s / %s", label, config.label);
  print_error_stats(full_label, st, xs.size());
  bool ok = check_abs_error_gate(full_label, st);

#if defined(__RVV10__)
  std::vector<float> rvv_s(xs.size());
  std::vector<float> rvv_c(xs.size());
  run_rvv_scratch(xs, rvv_s, rvv_c, config);
  const DiffStats sin_diff = same_chain_diff(rvv_s, s);
  const DiffStats cos_diff = same_chain_diff(rvv_c, c);
  const bool diff_ok = same_chain_diff_ok(sin_diff) && same_chain_diff_ok(cos_diff);
  std::printf("[%s RVV scratch vs scalar same-chain] sin max |diff| = %.9e, cos max |diff| = %.9e, "
              "nan mismatch = %d/%d, nonfinite mismatch = %d/%d\n",
              config.label,
              static_cast<double>(sin_diff.max_abs),
              static_cast<double>(cos_diff.max_abs),
              sin_diff.nan_mismatch,
              cos_diff.nan_mismatch,
              sin_diff.nonfinite_mismatch,
              cos_diff.nonfinite_mismatch);
  std::printf("[gate] RVV same-chain max |diff| <= %.1e and NaN classes match: %s\n",
              static_cast<double>(kSameChainDiffGate),
              diff_ok ? "PASS" : "FAIL");
  ok = diff_ok && ok;
#else
  std::printf("[RVV scratch vs scalar same-chain] disabled: build without __RVV10__\n");
#endif
  return ok;
}

bool
run_case(const char* label, const std::vector<float>& xs)
{
  bool ok = true;
  for (const ApproxConfig& config : kConfigs)
    ok = run_candidate_case(label, xs, config) && ok;
  return ok;
}

} // namespace

int
main(int argc, char** argv)
{
  bool run_bench = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--bench") == 0) {
      run_bench = true;
    }
    else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      return 2;
    }
  }

  std::printf("sincos scratch: finite-domain paired helper, mask segmentation, x in [-pi, pi]\n");
  std::printf("complete RVV candidate: lane-level sincos_finite_domain_RVV_f32m2, not reduced kernel\n");
  std::printf("not a strict libm replacement; domain-out lanes produce NaN in this scratch test\n");
  std::printf("scratch gates: abs error <= %.1e; RVV same-chain diff == %.1e; ULP is diagnostic only\n",
              kAbsErrorGate,
              static_cast<double>(kSameChainDiffGate));

  bool ok = true;
  ok = run_case("dense [-pi, pi]", make_dense_grid(-kPi, kPi, 200001)) && ok;
  ok = run_case("dense [-pi/2, pi/2]", make_dense_grid(-kPi2, kPi2, 100001)) && ok;
  ok = run_case("adversarial boundaries", make_adversarial_points()) && ok;

  const std::vector<float> special = make_special_points();
  for (const ApproxConfig& config : kConfigs) {
    std::printf("\n=== special/domain-out candidate: %s ===\n", config.label);
    std::vector<float> s(special.size());
    std::vector<float> c(special.size());
    run_scalar(special, s, c, config);
    print_special_stats(classify_special(special, s, c));
    ok = run_signed_zero_check(config) && ok;

#if defined(__RVV10__)
    std::vector<float> rvv_s(special.size());
    std::vector<float> rvv_c(special.size());
    run_rvv_scratch(special, rvv_s, rvv_c, config);
    const DiffStats special_sin_diff = same_chain_diff(rvv_s, s);
    const DiffStats special_cos_diff = same_chain_diff(rvv_c, c);
    const bool special_diff_ok =
        same_chain_diff_ok(special_sin_diff) && same_chain_diff_ok(special_cos_diff);
    std::printf("[%s special RVV vs scalar same-chain] sin max |diff| = %.9e, cos max |diff| = %.9e, "
                "nan mismatch = %d/%d, nonfinite mismatch = %d/%d\n",
                config.label,
                static_cast<double>(special_sin_diff.max_abs),
                static_cast<double>(special_cos_diff.max_abs),
                special_sin_diff.nan_mismatch,
                special_cos_diff.nan_mismatch,
                special_sin_diff.nonfinite_mismatch,
                special_cos_diff.nonfinite_mismatch);
    std::printf("[gate] special RVV same-chain max |diff| <= %.1e and NaN classes match: %s\n",
                static_cast<double>(kSameChainDiffGate),
                special_diff_ok ? "PASS" : "FAIL");
    ok = special_diff_ok && ok;
#endif
  }

  std::printf("\n=== scratch gate summary: %s ===\n", ok ? "PASS" : "FAIL");
  if (ok && run_bench)
    run_benchmark();
  return ok ? 0 : 1;
}
