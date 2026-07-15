#!/usr/bin/env python3
"""
acos 参数脚本（四路默认 report）：

(1) PCL 八常数 sqrt 结构（与 common.hpp / RVV 同系）
    f_pcl(x) = (a0 + x*(a1 + x*a2))*sqrt(b0 + x*b1) + (c0 + x*(c1 + x*c2)

(2) 新约化 + remez1（第一算法风格：交换参考点 + 线性方程组）
    令 u = 1 - x，拟合 acos(x) ~= sqrt(u) * Q(u)

(3) 新约化 + remez2（Powell，初值 LP）

(4) 新约化 + 离散 LP（HiGHS）
    对 g(u)=acos(1-u)/sqrt(u) 在 [u_lo, u_hi] 上做多项式 minimax（离散 L∞）。

默认 report 输出上述四路，并给出可直接贴到 C++ 的系数。

与 acos_test.cpp / RVV 的精度对照
--------------------------------
拟合在 numpy/scipy 侧多为 float64。测试中：约化模型的标量路径用 double Horner + sqrt 再转 float；
RVV 约化核为全程 float32。故板卡上「RVV 约化 vs 标量约化」的 max|diff| 可出现约 1 个 float ULP
量级，与主误差表（相对 std::acos）不是同一指标。PCL 标量与 pcl::acos_RVV_f32m2 同为 float32，可对齐到 diff≈0。
"""

from __future__ import annotations

import argparse
import math
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MATH_SCRIPT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "script"))
for path in (SCRIPT_DIR, MATH_SCRIPT_DIR):
    if path not in sys.path:
        sys.path.insert(0, path)

from lp_minimax import minimax_polynomial_lp  # noqa: E402

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

# -----------------------------------------------------------------------------
# 四路系数快照（默认区间 [0, 0.999]；与 acos_test.cpp 保持同步）
# C++ float 字面量：.16g + 后缀 f;（与 parms_atan2.py 快照风格一致）
# (1) PCL8 见下方常量
# (2) remez1, deg=11（约化 sqrt(1-x)*Q(1-x)，稠密 Horner q0..q11）:
#     q0 = 1.414213562193966f;
#     q1 = 0.1178511461608607f;
#     q2 = 0.02651605183620386f;
#     q3 = 0.007897888366046083f;
#     q4 = 0.002639581213701285f;
#     q5 = 0.001201858781216681f;
#     q6 = -0.0002584332125616682f;
#     q7 = 0.001434126856504966f;
#     q8 = -0.001622669688140837f;
#     q9 = 0.001450402024837244f;
#     q10 = -0.0006958168594569422f;
#     q11 = 0.0001686291165455616f;
# (3) remez2, deg=11（约化模型 sqrt(1-x)*Q(1-x)）:
#     q0 = 1.414213564630177f;
#     q1 = 0.1178511294464069f;
#     q2 = 0.02651656650651231f;
#     q3 = 0.007890196791318107f;
#     q4 = 0.002703030232117461f;
#     q5 = 0.0009005233717898873f;
#     q6 = 0.0005798378741792223f;
#     q7 = 0f;
#     q8 = 0f;
#     q9 = 0.0001536018140430774f;
#     q10 = 0f;
#     q11 = -1.217035647128233e-05f;
# (4) LP, deg=11（约化模型 sqrt(1-x)*Q(1-x)）:
#     q0 = 1.414213562373779f;
#     q1 = 0.1178511294564023f;
#     q2 = 0.02651656650651231f;
#     q3 = 0.007890196791318107f;
#     q4 = 0.002703030232117461f;
#     q5 = 0.0009005233717898873f;
#     q6 = 0.0005798378741792223f;
#     q7 = 0f;
#     q8 = 0f;
#     q9 = 0.0001536018140430774f;
#     q10 = 0f;
#     q11 = -1.217035647128233e-05f;
# (2) remez1, deg=7:
#     q0 = 1.414213188912104f;
#     q1 = 0.1178641849866039f;
#     q2 = 0.02636458860921847f;
#     q3 = 0.008719535139112927f;
#     q4 = 0.0002768916562795434f;
#     q5 = 0.004891147985924504f;
#     q6 = -0.003017314299520197f;
#     q7 = 0.001484092403350398f;
# (3) remez2, deg=7:
#     q0 = 1.414213558747199f;
#     q1 = 0.1178539448393304f;
#     q2 = 0.02644738228454647f;
#     q3 = 0.008406567572921202f;
#     q4 = 0.0009190414245998239f;
#     q5 = 0.004154037817069804f;
#     q6 = -0.002572261363003802f;
#     q7 = 0.001374063533135086f;
# (4) LP, deg=7:
#     q0 = 1.41421354285702f;
#     q1 = 0.1178539448571087f;
#     q2 = 0.02644738228454647f;
#     q3 = 0.008406567572921202f;
#     q4 = 0.0009190414245998239f;
#     q5 = 0.004154037817069804f;
#     q6 = -0.002572261363003802f;
#     q7 = 0.001374063533135086f;
# (2) remez1, deg=5:
#     q0 = 1.414196327328033f;
#     q1 = 0.1181715632132089f;
#     q2 = 0.02451971606187768f;
#     q3 = 0.01346301021415174f;
#     q4 = -0.004724656324727283f;
#     q5 = 0.005169831352385951f;
# (3) remez2, deg=5:
#     q0 = 1.414212408248559f;
#     q1 = 0.117926522053977f;
#     q2 = 0.02571508511147162f;
#     q3 = 0.01095480727067022f;
#     q4 = -0.002360310714948563f;
#     q5 = 0.004346735271181379f;
# (4) LP, deg=5:
#     q0 = 1.414212391465758f;
#     q1 = 0.1179265220794355f;
#     q2 = 0.02571508511147162f;
#     q3 = 0.01095480727067022f;
#     q4 = -0.002360310714948563f;
#     q5 = 0.004346735271181379f;
# -----------------------------------------------------------------------------


PCL8 = np.array(
    [
        1.59121552,
        -0.15461442,
        0.05354897,
        0.89286965,
        -0.89282669,
        0.06681017,
        -0.09402311,
        0.02708663,
    ],
    dtype=np.float64,
)


def fmt16g(x: float) -> str:
    return format(float(x), ".16g")


def fmt_cpp_float_f(x: float) -> str:
    """C++ float 系数字面量：.16g 与 parms_atan2.py 快照一致，带 f; 后缀。"""
    return f"{fmt16g(float(x))}f;"


def f_pcl(x: np.ndarray, s: np.ndarray) -> np.ndarray:
    a0, a1, a2, b0, b1, c0, c1, c2 = s
    inner = b0 + b1 * x
    inner = np.where(inner < 0.0, np.nan, inner)
    return (a0 + x * (a1 + x * a2)) * np.sqrt(inner) + (c0 + x * (c1 + x * c2))


def max_abs_pcl(s: np.ndarray, x_lo: float, x_hi: float, n: int = 8000) -> float:
    x = np.linspace(x_lo, x_hi, n, dtype=np.float64)
    ref = np.arccos(x)
    m = f_pcl(x, s)
    return float(np.max(np.fabs(ref - m)))


def eval_reduced_model(x: np.ndarray, coeffs: np.ndarray) -> np.ndarray:
    u = np.maximum(1.0 - x, 0.0)
    q = np.polyval(np.array(coeffs[::-1], dtype=np.float64), u)
    return np.sqrt(u) * q


def fit_reduced_lp(
    degree: int,
    x_lo: float,
    x_hi: float,
    n_grid: int = 10000,
):
    if degree < 0:
        raise ValueError("degree must be >= 0")
    u_lo = 1.0 - x_hi
    u_hi = 1.0 - x_lo

    def g(u: np.ndarray) -> np.ndarray:
        return np.arccos(1.0 - u) / np.sqrt(u)

    coeffs, est = minimax_polynomial_lp(g, u_lo, u_hi, degree, n_grid=n_grid, e_lower=0.0)
    return coeffs, float(est)


def fit_reduced_remez1(
    degree: int,
    x_lo: float,
    x_hi: float,
    max_iter: int = 80,
    n_fine: int = 200001,
):
    """
    约化模型 Remez 第一算法风格：在 u=1-x 上拟合 acos(1-u) ≈ sqrt(u)*Q(u)，
    交换交替极值参考点并解 (deg+2)×(deg+2) 线性方程组。
    """
    if not HAS_NUMPY:
        raise RuntimeError("需要 numpy")
    if degree < 0:
        raise ValueError("degree must be >= 0")
    u_lo = max(1.0 - x_hi, 1e-14)
    u_hi = 1.0 - x_lo
    n_c = degree + 1
    n_ref = n_c + 1
    k = np.arange(n_ref)
    ref = np.cos((2 * k + 1) * np.pi / (2 * n_ref))
    ref = 0.5 * (u_lo + u_hi) + 0.5 * (u_hi - u_lo) * ref
    ref = np.clip(ref, u_lo, u_hi)
    max_abs_err = float("nan")
    coeffs = np.zeros(n_c, dtype=np.float64)
    n_fine_i = int(n_fine)
    for _ in range(int(max_iter)):
        rows = []
        rhs = []
        for i, ui in enumerate(ref):
            ui = max(float(ui), 1e-30)
            sq = math.sqrt(ui)
            row = [sq * (ui ** j) for j in range(n_c)]
            row.append((-1.0) ** i)
            rows.append(row)
            rhs.append(math.acos(1.0 - ui))
        a_m = np.array(rows, dtype=np.float64)
        b_v = np.array(rhs, dtype=np.float64)
        try:
            sol = np.linalg.solve(a_m, b_v)
        except np.linalg.LinAlgError:
            break
        coeffs = sol[:n_c]
        u_f = np.linspace(u_lo, u_hi, n_fine_i)
        x_f = 1.0 - u_f
        ref_y = np.arccos(np.clip(x_f, -1.0, 1.0))
        appr = np.sqrt(u_f) * np.polyval(coeffs[::-1], u_f)
        err = ref_y - appr
        max_abs_err = float(np.max(np.abs(err)))
        de = np.diff(err)
        sign_changes = np.diff(np.sign(de)) != 0
        ext_idx = np.where(sign_changes)[0] + 1
        if len(ext_idx) == 0:
            ext_idx = np.array([np.argmax(np.abs(err))])
        t_cand = np.concatenate([[u_lo], u_f[ext_idx], [u_hi]])
        e_cand = np.arccos(np.clip(1.0 - t_cand, -1.0, 1.0)) - np.sqrt(t_cand) * np.polyval(
            coeffs[::-1], t_cand
        )
        order = np.argsort(t_cand)
        t_cand = t_cand[order]
        e_cand = e_cand[order]
        ref_new = []
        j = 0
        for ii in range(n_ref):
            need = (-1) ** ii
            while j < len(t_cand):
                s = np.sign(e_cand[j])
                if s == 0 or s == need:
                    ref_new.append(float(t_cand[j]))
                    j += 1
                    break
                j += 1
        if len(ref_new) < n_ref:
            ref = np.linspace(u_lo, u_hi, n_ref)
        else:
            ref = np.array(sorted(ref_new[:n_ref]), dtype=np.float64)
    return coeffs.tolist(), max_abs_err


def fit_reduced_remez2(
    degree: int,
    x_lo: float,
    x_hi: float,
    n_grid_lp_init: int = 12000,
    n_grid_objective: int = 200001,
    powell_maxiter: int = 800,
    powell_ftol: float = 1e-15,
):
    if not HAS_SCIPY:
        raise RuntimeError("remez2 需要 SciPy.optimize.minimize")
    c0 = np.array(
        fit_reduced_lp(degree, x_lo, x_hi, n_grid=n_grid_lp_init)[0],
        dtype=np.float64,
    )
    xs = np.linspace(x_lo, x_hi, int(n_grid_objective), dtype=np.float64)
    ref = np.arccos(xs)

    def obj(vec: np.ndarray) -> float:
        appr = eval_reduced_model(xs, vec)
        return float(np.max(np.abs(ref - appr)))

    r = minimize(
        obj,
        c0,
        method="Powell",
        options={"maxiter": int(powell_maxiter), "ftol": float(powell_ftol)},
    )
    if not r.success:
        raise RuntimeError(f"Powell 未收敛: {r.message}")
    coeffs = [float(v) for v in r.x]
    max_e = float(np.max(np.abs(ref - eval_reduced_model(xs, np.array(coeffs, dtype=np.float64)))))
    return coeffs, max_e


def print_coeffs(coeffs, prefix: str = "q"):
    for i, v in enumerate(coeffs):
        print(f"  {prefix}{i} = {fmt_cpp_float_f(v)}")


def print_pcl8_coeffs():
    names = ["a0", "a1", "a2", "b0", "b1", "c0", "c1", "c2"]
    for n, v in zip(names, PCL8):
        print(f"  {n} = {fmt_cpp_float_f(v)}")


def print_three_way_report(
    x_lo: float,
    x_hi: float,
    degree: int,
    grid_lp: int,
    powell_grid: int,
):
    if not HAS_NUMPY or not HAS_SCIPY:
        raise SystemExit("report 需要 numpy 与 scipy")

    b = max_abs_pcl(PCL8, x_lo, x_hi)
    print("=== (1) PCL 八常数 sqrt 型（common.hpp / RVV 同系）===")
    print(
        f"max|acos-f_pcl| on [{fmt16g(x_lo)}, {fmt16g(x_hi)}] (float64): {fmt16g(b)} rad  ({b * 180 / math.pi:.4f} deg)"
    )
    print("PCL8:")
    print_pcl8_coeffs()
    print()

    print(
        f"=== (2) 约化模型 + remez1（第一算法风格）：acos(x) ~= sqrt(1-x)*Q(1-x), deg={degree} ==="
    )
    cr1, er1 = fit_reduced_remez1(int(degree), x_lo, x_hi, max_iter=80, n_fine=int(powell_grid))
    print(f"# 估计 max|acos-f| on remez1 栅格: {fmt16g(er1)}")
    print_coeffs(cr1, prefix="q")
    print()

    print(
        f"=== (3) 约化模型 + remez2：acos(x) ~= sqrt(1-x)*Q(1-x), deg={degree} ==="
    )
    cr2, er2 = fit_reduced_remez2(
        degree,
        x_lo,
        x_hi,
        n_grid_lp_init=max(grid_lp, 8000),
        n_grid_objective=powell_grid,
    )
    print(f"# 估计 max|acos-f| on Powell 栅格: {fmt16g(er2)}")
    print_coeffs(cr2, prefix="q")
    print()

    print(f"=== (4) 约化模型 + 离散 LP（HiGHS）：deg={degree} ===")
    clp, elp = fit_reduced_lp(degree, x_lo, x_hi, n_grid=grid_lp)
    print(f"# 估计 max|acos-f| on LP 栅格: {fmt16g(elp)}")
    print_coeffs(clp, prefix="q")
    print()
    print(
        "# 同步：将 (2)(3)(4) 系数更新到脚本顶部快照与 acos_test.cpp。"
        "提示：标量 double Horner（测试）与 RVV float32（板卡）的逐点 diff 可约 1 ULP，见 acos_test 说明。"
    )


def main():
    ap = argparse.ArgumentParser(
        description="acos 四路参数：PCL8 / 约化 remez1 / remez2 / 约化 LP"
    )
    ap.add_argument(
        "--method",
        choices=[
            "report",
            "lp",
            "remez1",
            "remez2",
            "pcl-baseline",
            "report-deg7",
            "lp-deg7",
            "remez1-deg7",
            "remez2-deg7",
            "report-deg5",
            "lp-deg5",
            "remez1-deg5",
            "remez2-deg5",
        ],
        default="report",
        help="report(默认) 四路；单路 lp / remez1 / remez2 / pcl-baseline；快捷 *-deg7/*-deg5",
    )
    ap.add_argument("--x-lo", type=float, default=0.0)
    ap.add_argument("--x-hi", type=float, default=0.999)
    ap.add_argument("--deg", type=int, default=11, help="约化多项式 Q(u) 的次数")
    ap.add_argument("--grid", type=int, default=10000, help="LP 栅格点数")
    ap.add_argument("--powell-grid", type=int, default=200001, help="remez1 迭代误差栅格 / remez2 Powell 目标栅格点数")
    args = ap.parse_args()

    if not HAS_NUMPY:
        raise SystemExit("需要 numpy")

    lo, hi = float(args.x_lo), float(args.x_hi)
    if not (0.0 <= lo < hi < 1.0):
        raise SystemExit("require 0 <= x_lo < x_hi < 1")
    if args.deg < 0:
        raise SystemExit("--deg must be >= 0")

    method = args.method
    deg = int(args.deg)
    if method == "report-deg7":
        method = "report"
        deg = 7
    elif method == "lp-deg7":
        method = "lp"
        deg = 7
    elif method == "remez1-deg7":
        method = "remez1"
        deg = 7
    elif method == "remez2-deg7":
        method = "remez2"
        deg = 7
    elif method == "report-deg5":
        method = "report"
        deg = 5
    elif method == "lp-deg5":
        method = "lp"
        deg = 5
    elif method == "remez1-deg5":
        method = "remez1"
        deg = 5
    elif method == "remez2-deg5":
        method = "remez2"
        deg = 5

    if method == "report":
        if not HAS_SCIPY:
            raise SystemExit("report 需要 scipy")
        print_three_way_report(lo, hi, deg, int(args.grid), int(args.powell_grid))
        return

    if method == "pcl-baseline":
        b = max_abs_pcl(PCL8, lo, hi)
        print(
            f"PCL baseline max|acos-f_pcl| on [{fmt16g(lo)}, {fmt16g(hi)}] (float64): {fmt16g(b)} rad  ({b * 180 / math.pi:.4f} deg)"
        )
        print("PCL8:")
        print_pcl8_coeffs()
        return

    if method == "remez1":
        c, est = fit_reduced_remez1(deg, lo, hi, max_iter=80, n_fine=int(args.powell_grid))
        title = f"约化模型 + remez1（第一算法风格），deg={deg}"
    elif method == "lp":
        if not HAS_SCIPY:
            raise SystemExit("lp 需要 scipy")
        c, est = fit_reduced_lp(deg, lo, hi, n_grid=args.grid)
        title = f"约化模型 + LP（HiGHS），deg={deg}"
    else:
        if not HAS_SCIPY:
            raise SystemExit("remez2 需要 scipy")
        c, est = fit_reduced_remez2(
            deg,
            lo,
            hi,
            n_grid_lp_init=max(args.grid, 8000),
            n_grid_objective=int(args.powell_grid),
        )
        title = f"约化模型 + remez2（Powell），deg={deg}"

    xs = np.linspace(lo, hi, 200000, dtype=np.float64)
    chk = float(np.max(np.abs(np.arccos(xs) - eval_reduced_model(xs, np.array(c, dtype=np.float64)))))
    print(title)
    print(f"acos(x) ~= sqrt(1-x) * (q0 + q1*u + ... + q{deg}*u^{deg}), u=1-x")
    print(f"估计 {fmt16g(est)}  200k 校验 {fmt16g(chk)}")
    print_coeffs(c, prefix="q")


if __name__ == "__main__":
    main()
