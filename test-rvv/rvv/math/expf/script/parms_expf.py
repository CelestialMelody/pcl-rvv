#!/usr/bin/env python3
"""
expf 参数脚本（对齐 parms_atan2.py 的输出风格）

默认目标区间:
  r in [-ln(2)/2, ln(2)/2]（与 expf 的 round 约化一致）
  exp(r) ~= P(r),  P(r)=c0 + r*(c1 + r*(... + c7*r))

方法:
  (1) baseline: common.hpp 当前系数（remez1-rel）
  (2) remez1: 第一算法风格交换实现（绝对误差）
  (3) remez1-rel: 第一算法风格交换实现（相对误差）
  (4) remez2-rel: LP 初值 + Powell 密栅 min-max（相对误差）
  (5) lp-rel: 离散 L∞ (HiGHS, 相对误差)
  (6) sollya: fpminimax 连续 minimax 参考（relative）

说明:
  是否替换 common.hpp 常数需看“真实链路 f32 仿真”（含 reduction+Horner+重构）
  与板卡端到端，不只看 float64 多项式误差。
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

from sollya_utils import parse_sollya_horner_to_coeffs, run_sollya_file  # noqa: E402

try:
    import numpy as np

    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    from scipy.optimize import linprog, minimize

    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False

LN2 = math.log(2.0)
R_LO_DEFAULT = -0.5 * LN2
R_HI_DEFAULT = 0.5 * LN2
X_LO_CHAIN_DEFAULT = -88.0
X_HI_CHAIN_DEFAULT = 88.0
K_LOG2_INV = 1.442695040888963
K_LOG2_HI = 0.6931471824645996
K_LOG2_LO = -1.904654290582768e-09

# -----------------------------------------------------------------------------
# 六路系数快照（默认 degree=7, r in [-ln2/2, ln2/2]；与 expf_test.cpp 对齐）
# (1) baseline（common.hpp 当前系数，remez1-rel）:
#     c0 = 0.9999999999876557f;
#     c1 = 1.000000000027863f;
#     c2 = 0.5000000053614374f;
#     c3 = 0.16666666439294f;
#     c4 = 0.04166635362288752f;
#     c5 = 0.008333359419394903f;
#     c6 = 0.001394106053653905f;
#     c7 = 0.0001986611354469939f;
# (2) remez1:
#     c0 = 0.9999999999875628f;
#     c1 = 0.9999999999949485f;
#     c2 = 0.5000000053852762f;
#     c3 = 0.1666666676245945f;
#     c4 = 0.04166635288798774f;
#     c5 = 0.008333290857447563f;
#     c6 = 0.001394110951809275f;
#     c7 = 0.0001990342095739361f;
# (3) remez1-rel:
#     c0 = 0.9999999999876557f;
#     c1 = 1.000000000027863f;
#     c2 = 0.5000000053614374f;
#     c3 = 0.16666666439294f;
#     c4 = 0.04166635362288752f;
#     c5 = 0.008333359419394903f;
#     c6 = 0.001394106053653905f;
#     c7 = 0.0001986611354469939f;
# (4) remez2-rel:
#     c0 = 1.000000000728035f;
#     c1 = 1.000000050005224f;
#     c2 = 0.5000000729575707f;
#     c3 = 0.1666638095880147f;
#     c4 = 0.04166471714273724f;
#     c5 = 0.008377074610962506f;
#     c6 = 0.001401267315781624f;
#     c7 = -6.450792317356219e-07f;
# (5) lp-rel:
#     c0 = 1.000000000814267f;
#     c1 = 1.000000050904498f;
#     c2 = 0.5000000751242663f;
#     c3 = 0.1666638177995045f;
#     c4 = 0.04166474213130857f;
#     c5 = 0.008377186523046082f;
#     c6 = 0.001401616120109518f;
#     c7 = 0f;
# (6) Sollya fpminimax(deg7, relative):
#     c0 = 0.999999999961682f;
#     c1 = 1.000000000243097f;
#     c2 = 0.5000000104536195f;
#     c3 = 0.1666666512613725f;
#     c4 = 0.04166622542551786f;
#     c5 = 0.008333561090240499f;
#     c6 = 0.001394818332705201f;
#     c7 = 0.0001977517158175297f;
# -----------------------------------------------------------------------------

HEADER_C = [
    0.9999999999876557,
    1.000000000027863,
    0.5000000053614374,
    0.16666666439294,
    0.04166635362288752,
    0.008333359419394903,
    0.001394106053653905,
    0.0001986611354469939,
]


def fmt16g(x: float) -> str:
    return format(float(x), ".16g")


def c_float_literal(x: float) -> str:
    return f"{fmt16g(float(x))}f;"


def poly_eval(coeffs, r: np.ndarray) -> np.ndarray:
    return np.polyval(np.array(list(coeffs)[::-1], dtype=np.float64), r)


def remez_exp_r_first_algorithm(
    degree: int,
    a: float = 0.0,
    b: float = LN2,
    max_iter: int = 50,
    n_fine: int = 3000,
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
        rhs = np.exp(ref)
        try:
            sol = np.linalg.solve(a_m, rhs)
        except np.linalg.LinAlgError:
            break
        coeffs = sol[: n + 1]

        x_fine = np.linspace(a, b, n_fine, dtype=np.float64)
        p_fine = np.polyval(coeffs[::-1], x_fine)
        err_fine = np.exp(x_fine) - p_fine
        max_err_abs = float(np.max(np.abs(err_fine)))
        diff_err = np.diff(err_fine)
        sign_changes = np.diff(np.sign(diff_err)) != 0
        extrema_idx = np.where(sign_changes)[0] + 1
        if len(extrema_idx) < n_ref:
            extrema_idx = np.array([int(np.argmax(np.abs(err_fine)))], dtype=int)
        x_cand = np.concatenate([[a], x_fine[extrema_idx], [b]])
        err_cand = np.exp(x_cand) - np.polyval(coeffs[::-1], x_cand)
        sort_idx = np.argsort(x_cand)
        x_cand = x_cand[sort_idx]
        err_cand = err_cand[sort_idx]
        ref_new = []
        j = 0
        for i in range(n_ref):
            need = (-1) ** i
            while j < len(x_cand) and np.sign(err_cand[j]) != need:
                j += 1
            if j < len(x_cand):
                ref_new.append(float(x_cand[j]))
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


def remez_exp_r_first_algorithm_rel(
    degree: int,
    a: float = 0.0,
    b: float = LN2,
    max_iter: int = 50,
    n_fine: int = 3000,
):
    """
    第一算法风格交换法（相对误差）：
      令 err_rel = (f-P)/f，迭代参考点满足等波纹。
    """
    n = degree
    n_ref = n + 2
    k = np.arange(1, n_ref + 1)
    x_cheb = np.cos((2 * k - 1) * np.pi / (2 * n_ref))
    ref = 0.5 * (a + b) + 0.5 * (b - a) * x_cheb
    ref = np.sort(ref)
    max_err_rel = 0.0
    coeffs = np.zeros(n + 1, dtype=float)

    for _ in range(max_iter):
        v = np.vander(ref, n + 1, increasing=True)
        f_ref = np.exp(ref)
        sgn = np.array([(-1) ** i for i in range(n_ref)], dtype=float)
        # P(x_i) - s_i * f(x_i) * E = f(x_i)
        col_e = -sgn * f_ref
        a_m = np.column_stack([v, col_e])
        rhs = f_ref
        try:
            sol = np.linalg.solve(a_m, rhs)
        except np.linalg.LinAlgError:
            break
        coeffs = sol[: n + 1]

        x_fine = np.linspace(a, b, n_fine, dtype=np.float64)
        f_fine = np.exp(x_fine)
        p_fine = np.polyval(coeffs[::-1], x_fine)
        err_rel = (f_fine - p_fine) / f_fine
        max_err_rel = float(np.max(np.abs(err_rel)))
        diff_err = np.diff(err_rel)
        sign_changes = np.diff(np.sign(diff_err)) != 0
        extrema_idx = np.where(sign_changes)[0] + 1
        if len(extrema_idx) < n_ref:
            extrema_idx = np.array([int(np.argmax(np.abs(err_rel)))], dtype=int)
        x_cand = np.concatenate([[a], x_fine[extrema_idx], [b]])
        f_cand = np.exp(x_cand)
        p_cand = np.polyval(coeffs[::-1], x_cand)
        err_cand = (f_cand - p_cand) / f_cand
        sort_idx = np.argsort(x_cand)
        x_cand = x_cand[sort_idx]
        err_cand = err_cand[sort_idx]
        ref_new = []
        j = 0
        for i in range(n_ref):
            need = (-1) ** i
            while j < len(x_cand) and np.sign(err_cand[j]) != need:
                j += 1
            if j < len(x_cand):
                ref_new.append(float(x_cand[j]))
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

    return coeffs.tolist(), float(max_err_rel)


def fit_lp_exp_rel(degree: int, a: float, b: float, n_grid: int = 15000):
    if not HAS_SCIPY:
        raise RuntimeError("lp-rel 需要 scipy.optimize.linprog")
    t = np.linspace(a, b, n_grid, dtype=np.float64)
    f = np.exp(t)
    d1 = degree + 1
    v = np.column_stack([t**i for i in range(d1)])
    n = t.shape[0]
    # |f - P| / f <= E  =>  P - E f <= f   and   -P - E f <= -f
    a_ub = np.vstack([np.hstack([v, -f.reshape(n, 1)]), np.hstack([-v, -f.reshape(n, 1)])])
    b_ub = np.hstack([f, -f])
    c_obj = np.zeros(d1 + 1, dtype=np.float64)
    c_obj[d1] = 1.0
    bounds = [(None, None)] * d1 + [(0.0, None)]
    r = linprog(c_obj, A_ub=a_ub, b_ub=b_ub, bounds=bounds, method="highs", options={"presolve": True})
    if not r.success:
        raise RuntimeError(f"linprog(lp-rel) failed: {r.message}")
    coeffs = r.x[:d1].tolist()
    p = np.polyval(np.array(coeffs[::-1], dtype=np.float64), t)
    max_rel = float(np.max(np.abs(f - p) / f))
    return coeffs, max_rel


def fit_remez2_exp_rel(
    degree: int,
    a: float,
    b: float,
    n_grid_lp_init: int,
    n_grid_objective: int,
    powell_maxiter: int,
):
    if not HAS_SCIPY:
        raise RuntimeError("remez2-rel 需要 scipy.optimize.minimize")
    c0 = np.array(
        fit_lp_exp_rel(degree, a, b, n_grid=max(3000, int(n_grid_lp_init)))[0],
        dtype=np.float64,
    )
    rs = np.linspace(a, b, int(n_grid_objective), dtype=np.float64)
    ref = np.exp(rs)

    def obj(vec: np.ndarray) -> float:
        appr = poly_eval(vec, rs)
        return float(np.max(np.abs(ref - appr) / ref))

    res = minimize(obj, c0, method="Powell", options={"maxiter": int(powell_maxiter), "ftol": 1e-15})
    if not res.success:
        raise RuntimeError(f"Powell 未收敛: {res.message}")
    coeffs = [float(v) for v in res.x]
    appr = poly_eval(coeffs, rs)
    return coeffs, float(np.max(np.abs(ref - appr) / ref))


def f32(x: float) -> float:
    return struct.unpack("f", struct.pack("f", float(x)))[0]


def max_rel_f32_horner(coeffs, r_lo: float, r_hi: float, n: int = 100000):
    r = np.linspace(float(r_lo), float(r_hi), n, dtype=np.float32)
    c = [f32(v) for v in coeffs]
    m = 0.0
    for ri in r:
        rr = f32(float(ri))
        t = f32(math.exp(float(rr)))
        p = c[-1]
        for j in range(len(c) - 2, -1, -1):
            p = f32(f32(c[j]) + f32(rr * p))
        m = max(m, abs(t - p) / max(t, 1e-30))
    return float(m)


def _round_away_from_zero(v: np.ndarray) -> np.ndarray:
    pos = np.floor(v + 0.5)
    neg = np.ceil(v - 0.5)
    return np.where(v >= 0.0, pos, neg)


def max_rel_f32_chain(coeffs, n: int = 200000, x_lo: float = X_LO_CHAIN_DEFAULT, x_hi: float = X_HI_CHAIN_DEFAULT):
    """
    仿真 common.hpp 链路:
      x clamp -> n=round(x/log2) -> r=x-n*hi-n*lo -> float Horner -> ldexp 重构
    """
    xs = np.linspace(float(x_lo), float(x_hi), int(n), dtype=np.float64)
    x_clamped = np.clip(xs, X_LO_CHAIN_DEFAULT, X_HI_CHAIN_DEFAULT).astype(np.float32)
    flt_n = (x_clamped * np.float32(K_LOG2_INV)).astype(np.float32)
    n_float = _round_away_from_zero(flt_n.astype(np.float64))
    n_int = n_float.astype(np.int32)
    fn = n_int.astype(np.float32)
    r = x_clamped - fn * np.float32(K_LOG2_HI) - fn * np.float32(K_LOG2_LO)

    c = np.array([f32(v) for v in coeffs], dtype=np.float32)
    p = np.full_like(r, c[-1], dtype=np.float32)
    for j in range(len(c) - 2, -1, -1):
        p = np.float32(c[j]) + r * p
    exp_r = p.astype(np.float32)
    two_n = np.ldexp(np.float32(1.0), n_int).astype(np.float32)
    approx = (exp_r * two_n).astype(np.float64)
    ref = np.exp(x_clamped.astype(np.float64))
    rel = np.abs(ref - approx) / np.maximum(ref, 1e-300)
    return float(np.max(rel))


def print_coeffs(coeffs, prefix: str = "c"):
    for i, c in enumerate(coeffs):
        print(f"{prefix}{i} = {c_float_literal(c)}")


def sollya_exp_fpminimax_script(degree: int, r_lo: float, r_hi: float, relative: bool = True) -> str:
    nd = degree + 1
    ds = ",".join(["D"] * nd)
    mode = "relative" if relative else "absolute"
    return (
        "display=decimal!;\n"
        f"I = [{fmt16g(r_lo)};{fmt16g(r_hi)}];\n"
        f"p = fpminimax(exp(x), {degree}, [|{ds}|], I, {mode});\n"
        "p;\n"
        "quit;\n"
    )


def run_and_print_sollya(deg: int, r_lo: float, r_hi: float, sollya_bin: str):
    print(f"# Sollya settings: degree={deg}, mode=relative, I=[{fmt16g(r_lo)}, {fmt16g(r_hi)}]")
    if not shutil.which(sollya_bin):
        print(f"# 未找到 {sollya_bin}，仅打印脚本；如需参数请安装后执行 --run-sollya")
        print(sollya_exp_fpminimax_script(deg, r_lo, r_hi, relative=True).rstrip())
        return

    rc, out, err = run_sollya_file(sollya_exp_fpminimax_script(deg, r_lo, r_hi, relative=True), sollya_bin=sollya_bin)
    lines = (out or "").strip().splitlines()
    horner_line = lines[-1] if lines else ""
    print(f"# sollya exit code: {rc}")
    if horner_line:
        try:
            coeffs = parse_sollya_horner_to_coeffs(horner_line)
            print("# Horner 链常数:")
            print_coeffs(coeffs, prefix="c")
            rr = np.linspace(r_lo, r_hi, 200000, dtype=np.float64)
            ref = np.exp(rr)
            appr = poly_eval(coeffs, rr)
            chk_abs = float(np.max(np.abs(ref - appr)))
            chk_rel = float(np.max(np.abs(ref - appr) / ref))
            print(f"# max|exp-P| (200k float64): {fmt16g(chk_abs)}")
            print(f"# max relative error (200k float64): {fmt16g(chk_rel)}")
            print(f"# max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(coeffs, r_lo, r_hi, n=100000))}")
            print(f"# max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(coeffs, n=200000))}")
        except (ValueError, TypeError) as ex:
            print(f"# 解析失败: {ex}")
            print("# stdout tail:", horner_line[:240])
    else:
        print("# 无 Horner 输出")
    if (err or "").strip():
        print("# stderr:", err.strip()[:300])


def print_report(deg: int, r_lo: float, r_hi: float, grid: int, powell_grid: int, iterations: int, sollya_bin: str):
    print(f"=== expf 参数六路 report, interval [{fmt16g(r_lo)}, {fmt16g(r_hi)}], degree={deg} ===")

    r = np.linspace(r_lo, r_hi, 200000, dtype=np.float64)

    print("=== (1) common.hpp baseline（当前头文件，remez1-rel）===")
    b = HEADER_C[: deg + 1]
    e64 = float(np.max(np.abs(np.exp(r) - poly_eval(b, r))))
    print(f"# max|exp-P| (200k float64): {fmt16g(e64)}")
    print(f"# max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(b, r_lo, r_hi, n=100000))}")
    print(f"# max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(b, n=200000))}")
    print_coeffs(b, prefix="c")
    print()

    print("=== (2) remez1（历史交换法，第一算法风格）===")
    c0, e0 = remez_exp_r_first_algorithm(deg, a=r_lo, b=r_hi, max_iter=iterations)
    print(f"# method est: {fmt16g(e0)}")
    print(f"# max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(c0, r_lo, r_hi, n=100000))}")
    print(f"# max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(c0, n=200000))}")
    print_coeffs(c0, prefix="c")
    print()

    print("=== (3) remez1-rel（第一算法风格，目标: max relative error）===")
    c1r, e1r = remez_exp_r_first_algorithm_rel(deg, a=r_lo, b=r_hi, max_iter=iterations)
    print(f"# method est (max rel on refine grid): {fmt16g(e1r)}")
    print(f"# max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(c1r, r_lo, r_hi, n=100000))}")
    print(f"# max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(c1r, n=200000))}")
    print_coeffs(c1r, prefix="c")
    print()

    print("=== (4) remez2-rel（LP 初值 + Powell 密栅 minmax，相对误差）===")
    c2, e2 = fit_remez2_exp_rel(
        deg,
        r_lo,
        r_hi,
        n_grid_lp_init=max(grid, 8000),
        n_grid_objective=powell_grid,
        powell_maxiter=iterations,
    )
    print(f"# max relative error on Powell grid: {fmt16g(e2)}")
    print(f"# max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(c2, r_lo, r_hi, n=100000))}")
    print(f"# max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(c2, n=200000))}")
    print_coeffs(c2, prefix="c")
    print()

    print("=== (5) lp-rel（离散 LP, HiGHS，相对误差）===")
    c3, e3 = fit_lp_exp_rel(deg, r_lo, r_hi, n_grid=grid)
    print(f"# max relative error on LP grid: {fmt16g(e3)}")
    print(f"# max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(c3, r_lo, r_hi, n=100000))}")
    print(f"# max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(c3, n=200000))}")
    print_coeffs(c3, prefix="c")
    print()

    print("=== (6) Sollya fpminimax（relative）===")
    print("# 注：Sollya 同为 relative 目标，但优化器与约束形式独立于 remez/LP。")
    run_and_print_sollya(deg, r_lo, r_hi, sollya_bin)


def main():
    ap = argparse.ArgumentParser(description="expf polynomial coefficients on [-ln(2)/2, ln(2)/2]")
    ap.add_argument("--degree", type=int, default=7, help="多项式次数")
    ap.add_argument(
        "--method",
        choices=[
            "report",
            "baseline",
            "remez1",
            "remez1-rel",
            "remez2-rel",
            "lp-rel",
            "sollya-script",
            "run-sollya",
        ],
        default="report",
        help="report(默认) / baseline / remez1 / remez1-rel / remez2-rel / lp-rel / sollya-script / run-sollya",
    )
    ap.add_argument("--iterations", type=int, default=80, help="remez1/remez1-rel/remez2-rel 最大迭代")
    ap.add_argument("--grid", type=int, default=15000, help="LP 栅格点数（lp-rel/remez2-rel 初值）")
    ap.add_argument("--powell-grid", type=int, default=200001, help="remez2-rel 目标栅格点数")
    ap.add_argument("--r-lo", type=float, default=R_LO_DEFAULT, help="拟合区间下界（默认 -ln2/2）")
    ap.add_argument("--r-hi", type=float, default=R_HI_DEFAULT, help="拟合区间上界（默认 ln2/2）")
    ap.add_argument("--sollya-bin", type=str, default="sollya", help="sollya 可执行文件")
    ap.add_argument("--output-header", type=str, default="")
    args = ap.parse_args()

    if not HAS_NUMPY:
        raise SystemExit("需要 numpy")
    if args.degree < 0:
        raise SystemExit("--degree must be >= 0")
    r_lo = float(args.r_lo)
    r_hi = float(args.r_hi)
    if not (r_lo < r_hi):
        raise SystemExit("require r_lo < r_hi")

    if args.method == "report":
        if not HAS_SCIPY:
            raise SystemExit("report 需要 scipy")
        print_report(
            args.degree,
            r_lo,
            r_hi,
            int(args.grid),
            int(args.powell_grid),
            int(args.iterations),
            args.sollya_bin,
        )
        return

    if args.method == "sollya-script":
        print(sollya_exp_fpminimax_script(args.degree, r_lo, r_hi, relative=True), end="")
        return

    if args.method == "run-sollya":
        run_and_print_sollya(args.degree, r_lo, r_hi, args.sollya_bin)
        return

    method = args.method

    if method == "baseline":
        coeffs = HEADER_C[: args.degree + 1]
        grid = np.linspace(r_lo, r_hi, 10000, dtype=np.float64)
        est = float(np.max(np.abs(np.exp(grid) - poly_eval(coeffs, grid))))
        title = "baseline(common.hpp current remez1-rel)"
    elif method == "remez1":
        coeffs, est = remez_exp_r_first_algorithm(args.degree, a=r_lo, b=r_hi, max_iter=args.iterations)
        title = "remez1(abs-error, first algorithm style)"
    elif method == "remez1-rel":
        coeffs, est = remez_exp_r_first_algorithm_rel(args.degree, a=r_lo, b=r_hi, max_iter=args.iterations)
        title = "remez1-rel(relative-error, first algorithm style)"
    else:
        if not HAS_SCIPY:
            raise SystemExit("remez2-rel / lp-rel 需要 scipy")
        if method == "lp-rel":
            coeffs, est = fit_lp_exp_rel(args.degree, r_lo, r_hi, n_grid=args.grid)
            title = "lp-rel(relative-error)"
        else:
            coeffs, est = fit_remez2_exp_rel(
                args.degree,
                r_lo,
                r_hi,
                n_grid_lp_init=max(args.grid, 8000),
                n_grid_objective=int(args.powell_grid),
                powell_maxiter=int(args.iterations),
            )
            title = "remez2-rel(relative-error, second algorithm proxy)"

    rr = np.linspace(r_lo, r_hi, 200000, dtype=np.float64)
    f_ref = np.exp(rr)
    chk_abs = float(np.max(np.abs(f_ref - poly_eval(coeffs, rr))))
    chk_rel = float(np.max(np.abs(f_ref - poly_eval(coeffs, rr)) / f_ref))
    print(f"{title}, degree={args.degree}")
    print(f"max |exp(r)-P| (200k float64): {fmt16g(chk_abs)}")
    print(f"max relative error (200k float64): {fmt16g(chk_rel)}  (method est: {fmt16g(est)})")
    print(f"max relative error (f32 Horner, 100k): {fmt16g(max_rel_f32_horner(coeffs, r_lo, r_hi, n=100000))}")
    print(f"max relative error (f32 chain, 200k x in [-88,88]): {fmt16g(max_rel_f32_chain(coeffs, n=200000))}")
    print_coeffs(coeffs, prefix="c")

    if args.output_header:
        with open(args.output_header, "w", encoding="utf-8") as f:
            f.write("/* Generated by parms_expf.py */\n")
            for i, c in enumerate(coeffs):
                f.write(f"static const float kExpfRemezC{i} = {fmt16g(c)}f;\n")
        print(f"Wrote {args.output_header}")


if __name__ == "__main__":
    main()
