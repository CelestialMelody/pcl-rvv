#!/usr/bin/env python3
"""
log1p 参数脚本（对齐 parms_expf.py / parms_atan2.py 的报告风格）

目标区间:
  u in [0, 1],  log(1+u) ~= P(u),  P(u)=c0 + u*(c1 + u*(... + c7*u))

方法:
  (1) baseline: common.hpp 当前系数
  (2) remez1: 第一算法风格交换实现（绝对误差）
  (3) remez2: LP 初值 + Powell 密栅 min-max（绝对误差）
  (4) lp: 离散 L∞ (HiGHS, 绝对误差)
  (5) sollya: fpminimax 连续 minimax 参考（absolute）

说明:
  log1p(0)=0，因此 relative 目标不适合作为主目标；此脚本以 absolute 为主。
  是否替换常数需同时看：
    - 核函数误差（u 域）
    - logf 真实链路 f32 仿真（mantissa reduction + Horner + e*ln2）
"""

from __future__ import annotations

import argparse
import math
import os
import shutil
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MATH_SCRIPT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "script"))
for path in (SCRIPT_DIR, MATH_SCRIPT_DIR):
    if path not in sys.path:
        sys.path.insert(0, path)

from lp_minimax import minimax_polynomial_lp  # noqa: E402
from sollya_utils import parse_sollya_horner_to_coeffs, run_sollya_file  # noqa: E402

try:
    import numpy as np

    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    from scipy.optimize import minimize

    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False

U_LO = 0.0
U_HI = 1.0
LOG_X_LO = -20.0
LOG_X_HI = 20.0

# 与 common.hpp 一致
K_LOG2_HI = 0.6931471824645996
K_LOG2_LO = -1.904654290582768e-09
K_SUBNORM_SCALE = 16777216.0

# baseline(common.hpp 当前系数)
HEADER_C = [
    1.8556080581e-07,
    9.9997340558e-01,
    -4.9937887289e-01,
    3.2776673211e-01,
    -2.2467442806e-01,
    1.3301016232e-01,
    -5.4002504554e-02,
    1.0452683404e-02,
]

# -----------------------------------------------------------------------------
# 五路系数快照（默认 degree=7, u in [0, 1]；与 logf_test.cpp 对齐）
# (1) baseline（common.hpp 当前系数）:
#     c0 = 1.8556080581e-07f;
#     c1 = 0.99997340558f;
#     c2 = -0.49937887289f;
#     c3 = 0.32776673211f;
#     c4 = -0.22467442806f;
#     c5 = 0.13301016232f;
#     c6 = -0.054002504554f;
#     c7 = 0.010452683404f;
# (2) remez1(abs):
#     c0 = 5.44528206223229e-08f;
#     c1 = 0.9999541289372607f;
#     c2 = -0.4991081652207077f;
#     c3 = 0.326430012819039f;
#     c4 = -0.2215213295201122f;
#     c5 = 0.1291661256269744f;
#     c6 = -0.05166960644611545f;
#     c7 = 0.009896014363606419f;
# (3) remez2(abs):
#     c0 = 2.011035705367378e-07f;
#     c1 = 0.9999737148907613f;
#     c2 = -0.4993804568247253f;
#     c3 = 0.3277496941750039f;
#     c4 = -0.2245729332813923f;
#     c5 = 0.1328137950599862f;
#     c6 = -0.05383971752877982f;
#     c7 = 0.01040308262004508f;
# (4) lp(abs):
#     c0 = 1.64871564074708e-07f;
#     c1 = 0.9999737145176905f;
#     c2 = -0.4993804568371159f;
#     c3 = 0.3277496941850039f;
#     c4 = -0.2245729332706192f;
#     c5 = 0.1328137950816015f;
#     c6 = -0.05383971750244008f;
#     c7 = 0.0104030826362085f;
# (5) Sollya fpminimax(deg7, absolute):
#     c0 = 1.922023964144791e-07f;
#     c1 = 0.999973122126788f;
#     c2 = -0.4993799892147339f;
#     c3 = 0.3277878623278651f;
#     c4 = -0.2247556918993918f;
#     c5 = 0.1331463211106413f;
#     c6 = -0.05410854392330709f;
#     c7 = 0.0104841000320837f;
# -----------------------------------------------------------------------------


def fmt16g(x: float) -> str:
    return format(float(x), ".16g")


def c_float_literal(x: float) -> str:
    return f"{fmt16g(float(x))}f;"


def poly_eval(coeffs, u: np.ndarray) -> np.ndarray:
    return np.polyval(np.array(list(coeffs)[::-1], dtype=np.float64), u)


def remez_log1p_first_algorithm(
    degree: int,
    a: float = U_LO,
    b: float = U_HI,
    max_iter: int = 60,
    n_fine: int = 5000,
):
    n = degree
    n_ref = n + 2
    k = np.arange(1, n_ref + 1)
    x_cheb = np.cos((2 * k - 1) * np.pi / (2 * n_ref))
    ref = 0.5 * (a + b) + 0.5 * (b - a) * x_cheb
    ref = np.sort(ref)
    max_err_abs = 0.0
    coeffs = np.zeros(n + 1, dtype=float)

    for _ in range(max_iter):
        v = np.vander(ref, n + 1, increasing=True)
        col_e = np.array([(-1) ** i for i in range(n_ref)], dtype=float)
        a_m = np.column_stack([v, col_e])
        rhs = np.log1p(ref)
        try:
            sol = np.linalg.solve(a_m, rhs)
        except np.linalg.LinAlgError:
            break
        coeffs = sol[: n + 1]

        u_fine = np.linspace(a, b, n_fine, dtype=np.float64)
        p_fine = np.polyval(coeffs[::-1], u_fine)
        err_fine = np.log1p(u_fine) - p_fine
        max_err_abs = float(np.max(np.abs(err_fine)))
        diff_err = np.diff(err_fine)
        sign_changes = np.diff(np.sign(diff_err)) != 0
        extrema_idx = np.where(sign_changes)[0] + 1
        if len(extrema_idx) < n_ref:
            extrema_idx = np.array([int(np.argmax(np.abs(err_fine)))], dtype=int)
        u_cand = np.concatenate([[a], u_fine[extrema_idx], [b]])
        err_cand = np.log1p(u_cand) - np.polyval(coeffs[::-1], u_cand)
        sort_idx = np.argsort(u_cand)
        u_cand = u_cand[sort_idx]
        err_cand = err_cand[sort_idx]
        ref_new = []
        j = 0
        for i in range(n_ref):
            need = (-1) ** i
            while j < len(u_cand) and np.sign(err_cand[j]) != need:
                j += 1
            if j < len(u_cand):
                ref_new.append(float(u_cand[j]))
                j += 1
        if len(ref_new) < n_ref:
            ref = np.linspace(a, b, n_ref, dtype=np.float64)
        else:
            ref_new = np.sort(np.unique(np.array(ref_new, dtype=np.float64)))
            if len(ref_new) < n_ref:
                ref = np.linspace(a, b, n_ref, dtype=np.float64)
            else:
                idx_sel = np.linspace(0, len(ref_new) - 1, n_ref, dtype=int)
                ref = ref_new[idx_sel]

    return coeffs.tolist(), float(max_err_abs)


def fit_lp_log1p_abs(degree: int, a: float, b: float, n_grid: int = 15000):
    coeffs, max_err = minimax_polynomial_lp(np.log1p, a, b, degree, n_grid=n_grid, e_lower=1e-300)
    return [float(v) for v in coeffs], float(max_err)


def fit_remez2_log1p_abs(
    degree: int,
    a: float,
    b: float,
    n_grid_lp_init: int,
    n_grid_objective: int,
    powell_maxiter: int,
):
    if not HAS_SCIPY:
        raise RuntimeError("remez2 需要 scipy.optimize.minimize")
    c0 = np.array(
        fit_lp_log1p_abs(degree, a, b, n_grid=max(3000, int(n_grid_lp_init)))[0],
        dtype=np.float64,
    )
    us = np.linspace(a, b, int(n_grid_objective), dtype=np.float64)
    ref = np.log1p(us)

    def obj(vec: np.ndarray) -> float:
        appr = poly_eval(vec, us)
        return float(np.max(np.abs(ref - appr)))

    res = minimize(obj, c0, method="Powell", options={"maxiter": int(powell_maxiter), "ftol": 1e-15})
    if not res.success:
        raise RuntimeError(f"Powell 未收敛: {res.message}")
    coeffs = [float(v) for v in res.x]
    appr = poly_eval(coeffs, us)
    return coeffs, float(np.max(np.abs(ref - appr)))


def f32(x: float) -> float:
    return struct.unpack("f", struct.pack("f", float(x)))[0]


def max_abs_f32_horner(coeffs, u_lo: float = U_LO, u_hi: float = U_HI, n: int = 100000):
    u = np.linspace(float(u_lo), float(u_hi), n, dtype=np.float32)
    c = [f32(v) for v in coeffs]
    m = 0.0
    for ui in u:
        uu = f32(float(ui))
        t = f32(math.log1p(float(uu)))
        p = c[-1]
        for j in range(len(c) - 2, -1, -1):
            p = f32(f32(c[j]) + f32(uu * p))
        m = max(m, abs(t - p))
    return float(m)


def eval_logf_scalar_chain(coeffs, x: float) -> float:
    if math.isnan(x):
        return x
    if x < 0.0:
        return float("nan")
    if x == 0.0:
        return float("-inf")
    if math.isinf(x):
        return float("inf")
    ix = struct.unpack("I", struct.pack("f", f32(x)))[0]
    if (ix & 0x7F800000) == 0x7F800000 and (ix & 0x7FFFFF) != 0:
        return float("nan")

    e8_0 = (ix >> 23) & 255
    m0 = ix & 0x7FFFFF
    xf = f32(x)
    if e8_0 == 0 and m0 != 0:
        xf = f32(xf * f32(K_SUBNORM_SCALE))

    ix = struct.unpack("I", struct.pack("f", xf))[0]
    e8 = (ix >> 23) & 255
    e = int(e8) - 127
    if e8_0 == 0 and m0 != 0:
        e -= 24

    mi = (ix & 0x7FFFFF) | 0x3F800000
    m = struct.unpack("f", struct.pack("I", mi))[0]
    u = f32(m - f32(1.0))

    c = [f32(v) for v in coeffs]
    poly = c[-1]
    for j in range(len(c) - 2, 0, -1):
        poly = f32(c[j] + f32(u * poly))
    log1p = f32(c[0] + f32(u * poly))
    fe = f32(float(e))
    r = f32(f32(fe * f32(K_LOG2_HI)) + f32(fe * f32(K_LOG2_LO)))
    return f32(r + log1p)


def max_rel_logf_chain(
    coeffs,
    n: int = 200000,
    log_x_lo: float = LOG_X_LO,
    log_x_hi: float = LOG_X_HI,
):
    ts = np.linspace(float(log_x_lo), float(log_x_hi), int(n), dtype=np.float64)
    xs = np.exp(ts)
    m = 0.0
    for x in xs:
        ref = math.log(float(x))
        appr = eval_logf_scalar_chain(coeffs, float(x))
        if ref != 0.0 and math.isfinite(ref) and math.isfinite(appr):
            m = max(m, abs(appr - ref) / abs(ref))
    return float(m)


def print_coeffs(coeffs, prefix: str = "c"):
    for i, c in enumerate(coeffs):
        print(f"{prefix}{i} = {c_float_literal(c)}")


def sollya_log1p_fpminimax_script(degree: int, u_lo: float, u_hi: float, relative: bool = False) -> str:
    nd = degree + 1
    ds = ",".join(["D"] * nd)
    mode = "relative" if relative else "absolute"
    return (
        "display=decimal!;\n"
        f"I = [{fmt16g(u_lo)};{fmt16g(u_hi)}];\n"
        f"p = fpminimax(log(1+x), {degree}, [|{ds}|], I, {mode});\n"
        "p;\n"
        "quit;\n"
    )


def run_and_print_sollya(deg: int, u_lo: float, u_hi: float, sollya_bin: str):
    print(f"# Sollya settings: degree={deg}, mode=absolute, I=[{fmt16g(u_lo)}, {fmt16g(u_hi)}]")
    if not shutil.which(sollya_bin):
        print(f"# 未找到 {sollya_bin}，仅打印脚本；如需参数请安装后执行 --run-sollya")
        print(sollya_log1p_fpminimax_script(deg, u_lo, u_hi, relative=False).rstrip())
        return

    rc, out, err = run_sollya_file(sollya_log1p_fpminimax_script(deg, u_lo, u_hi, relative=False), sollya_bin=sollya_bin)
    lines = (out or "").strip().splitlines()
    horner_line = lines[-1] if lines else ""
    print(f"# sollya exit code: {rc}")
    if horner_line:
        try:
            coeffs = parse_sollya_horner_to_coeffs(horner_line)
            print("# Horner 链常数（可直接贴 C++）:")
            print_coeffs(coeffs, prefix="c")
            uu = np.linspace(u_lo, u_hi, 200000, dtype=np.float64)
            ref = np.log1p(uu)
            appr = poly_eval(coeffs, uu)
            chk_abs = float(np.max(np.abs(ref - appr)))
            print(f"# max|log1p-P| (200k float64): {fmt16g(chk_abs)}")
            print(f"# max|log1p-P| (f32 Horner, 100k): {fmt16g(max_abs_f32_horner(coeffs, u_lo, u_hi, n=100000))}")
            print(f"# max relative error (logf f32 chain, 200k log-x in [-20,20]): {fmt16g(max_rel_logf_chain(coeffs, n=200000))}")
        except (ValueError, TypeError) as ex:
            print(f"# 解析失败: {ex}")
            print("# stdout tail:", horner_line[:240])
    else:
        print("# 无 Horner 输出")
    if (err or "").strip():
        print("# stderr:", err.strip()[:300])


def print_report(deg: int, u_lo: float, u_hi: float, grid: int, powell_grid: int, iterations: int, sollya_bin: str):
    print(f"=== log1p 参数五路 report, interval [{fmt16g(u_lo)}, {fmt16g(u_hi)}], degree={deg} ===")
    uu = np.linspace(u_lo, u_hi, 200000, dtype=np.float64)
    ref = np.log1p(uu)

    print("=== (1) common.hpp baseline（当前头文件）===")
    b = HEADER_C[: deg + 1]
    e64 = float(np.max(np.abs(ref - poly_eval(b, uu))))
    print(f"# max|log1p-P| (200k float64): {fmt16g(e64)}")
    print(f"# max|log1p-P| (f32 Horner, 100k): {fmt16g(max_abs_f32_horner(b, u_lo, u_hi, n=100000))}")
    print(f"# max relative error (logf f32 chain, 200k log-x in [-20,20]): {fmt16g(max_rel_logf_chain(b, n=200000))}")
    print_coeffs(b, prefix="c")
    print()

    print("=== (2) remez1（第一算法风格交换实现，absolute）===")
    c1, e1 = remez_log1p_first_algorithm(deg, a=u_lo, b=u_hi, max_iter=iterations)
    print(f"# method est (max abs on refine grid): {fmt16g(e1)}")
    print(f"# max|log1p-P| (f32 Horner, 100k): {fmt16g(max_abs_f32_horner(c1, u_lo, u_hi, n=100000))}")
    print(f"# max relative error (logf f32 chain, 200k log-x in [-20,20]): {fmt16g(max_rel_logf_chain(c1, n=200000))}")
    print_coeffs(c1, prefix="c")
    print()

    print("=== (3) remez2（LP 初值 + Powell 密栅 minmax，absolute）===")
    c2, e2 = fit_remez2_log1p_abs(
        deg,
        u_lo,
        u_hi,
        n_grid_lp_init=max(grid, 8000),
        n_grid_objective=powell_grid,
        powell_maxiter=iterations,
    )
    print(f"# max abs error on Powell grid: {fmt16g(e2)}")
    print(f"# max|log1p-P| (f32 Horner, 100k): {fmt16g(max_abs_f32_horner(c2, u_lo, u_hi, n=100000))}")
    print(f"# max relative error (logf f32 chain, 200k log-x in [-20,20]): {fmt16g(max_rel_logf_chain(c2, n=200000))}")
    print_coeffs(c2, prefix="c")
    print()

    print("=== (4) lp（离散 L∞, HiGHS，absolute）===")
    c3, e3 = fit_lp_log1p_abs(deg, u_lo, u_hi, n_grid=grid)
    print(f"# max abs error on LP grid: {fmt16g(e3)}")
    print(f"# max|log1p-P| (f32 Horner, 100k): {fmt16g(max_abs_f32_horner(c3, u_lo, u_hi, n=100000))}")
    print(f"# max relative error (logf f32 chain, 200k log-x in [-20,20]): {fmt16g(max_rel_logf_chain(c3, n=200000))}")
    print_coeffs(c3, prefix="c")
    print()

    print("=== (5) Sollya fpminimax（absolute）===")
    print("# 注：log1p(0)=0，relative 目标在端点数值不稳，因此此处用 absolute。")
    run_and_print_sollya(deg, u_lo, u_hi, sollya_bin)


def main():
    ap = argparse.ArgumentParser(description="log1p polynomial coefficients on [0,1]")
    ap.add_argument("--degree", type=int, default=7, help="多项式次数")
    ap.add_argument(
        "--method",
        choices=[
            "report",
            "baseline",
            "remez1",
            "remez2",
            "lp",
            "sollya-script",
            "run-sollya",
        ],
        default="report",
        help="report(默认) / baseline / remez1 / remez2 / lp / sollya-script / run-sollya",
    )
    ap.add_argument("--iterations", type=int, default=80, help="remez1/remez2 最大迭代")
    ap.add_argument("--grid", type=int, default=15000, help="LP 栅格点数（lp/remez2 初值）")
    ap.add_argument("--powell-grid", type=int, default=200001, help="remez2 目标栅格点数")
    ap.add_argument("--u-lo", type=float, default=U_LO, help="拟合区间下界（默认 0）")
    ap.add_argument("--u-hi", type=float, default=U_HI, help="拟合区间上界（默认 1）")
    ap.add_argument("--sollya-bin", type=str, default="sollya", help="sollya 可执行文件")
    ap.add_argument("--output-header", type=str, default="")
    args = ap.parse_args()

    if not HAS_NUMPY:
        raise SystemExit("需要 numpy")
    if args.degree < 0:
        raise SystemExit("--degree must be >= 0")
    u_lo = float(args.u_lo)
    u_hi = float(args.u_hi)
    if not (u_lo < u_hi):
        raise SystemExit("require u_lo < u_hi")

    if args.method == "report":
        if not HAS_SCIPY:
            raise SystemExit("report 需要 scipy")
        print_report(
            args.degree,
            u_lo,
            u_hi,
            int(args.grid),
            int(args.powell_grid),
            int(args.iterations),
            args.sollya_bin,
        )
        return

    if args.method == "sollya-script":
        print(sollya_log1p_fpminimax_script(args.degree, u_lo, u_hi, relative=False), end="")
        return

    if args.method == "run-sollya":
        run_and_print_sollya(args.degree, u_lo, u_hi, args.sollya_bin)
        return

    method = args.method
    if method == "baseline":
        coeffs = HEADER_C[: args.degree + 1]
        title = "baseline(common.hpp)"
        est = float(np.max(np.abs(np.log1p(np.linspace(u_lo, u_hi, 10000, dtype=np.float64)) - poly_eval(coeffs, np.linspace(u_lo, u_hi, 10000, dtype=np.float64)))))
    elif method == "remez1":
        coeffs, est = remez_log1p_first_algorithm(args.degree, a=u_lo, b=u_hi, max_iter=args.iterations)
        title = "remez1(abs-error, first algorithm style)"
    else:
        if not HAS_SCIPY:
            raise SystemExit("remez2 / lp 需要 scipy")
        if method == "lp":
            coeffs, est = fit_lp_log1p_abs(args.degree, u_lo, u_hi, n_grid=args.grid)
            title = "lp(abs-error)"
        else:
            coeffs, est = fit_remez2_log1p_abs(
                args.degree,
                u_lo,
                u_hi,
                n_grid_lp_init=max(args.grid, 8000),
                n_grid_objective=int(args.powell_grid),
                powell_maxiter=int(args.iterations),
            )
            title = "remez2(abs-error, second algorithm proxy)"

    uu = np.linspace(u_lo, u_hi, 200000, dtype=np.float64)
    chk_abs = float(np.max(np.abs(np.log1p(uu) - poly_eval(coeffs, uu))))
    print(f"{title}, degree={args.degree}")
    print(f"max |log1p(u)-P(u)| (200k float64): {fmt16g(chk_abs)}  (method est: {fmt16g(est)})")
    print(f"max |log1p(u)-P(u)| (f32 Horner, 100k): {fmt16g(max_abs_f32_horner(coeffs, u_lo, u_hi, n=100000))}")
    print(f"max relative error (logf f32 chain, 200k log-x in [-20,20]): {fmt16g(max_rel_logf_chain(coeffs, n=200000))}")
    print_coeffs(coeffs, prefix="c")

    if args.output_header:
        with open(args.output_header, "w", encoding="utf-8") as f:
            f.write("/* Generated by parms_log1p.py */\n")
            for i, c in enumerate(coeffs):
                f.write(f"static const float kLogfLog1pC{i} = {fmt16g(c)}f;\n")
        print(f"Wrote {args.output_header}")


if __name__ == "__main__":
    main()
