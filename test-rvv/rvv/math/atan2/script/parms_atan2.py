#!/usr/bin/env python3
"""
在区间 [0,1] 上逼近 atan(t)（与 atan2 约化中 |y|≤|x| 分支一致），默认输出六路系数对照。

六路含义
--------
(1) 文章 mazzo.li：仅注释引用（数值以 common.hpp 为准；不随脚本重算）。
(2) Remez「第一算法」风格：Chebyshev 初值参考点 + 交替极值交换解线性方程组（`remez_atan_odd_first`）。
    当前实现对交换点选择做了稳健化；在 [0,1] 上可收敛到 ~1e-6 rad 量级（与 (3)(4) 同阶）。
(3) Remez「第二算法」的工程实现：在密栅上直接最小化 max|atan-P|（`scipy.optimize.minimize` + Powell，
    初值 LP）。与教材中「牛顿解交替方程组的 Remez 第二算法」目标相同，此处用无导数优化避免病态交换参考点。
(4) 离散 L∞（`linprog` / HiGHS），奇次单项式基 t^{1,3,...,11}。
(5) Sollya `fpminimax`：全次数 5（六个 D），得 Horner 常数 c0..c5；拟合形式为 P(t)=Σ c_i t^i，
    与 (2)(3)(4) 的 t*(a1+a3 t^2+...) 不同，但浮点参数个数均为 6。
(6) Sollya `fpminimax`：全次数 11（十二个 D），Horner 常数 c0..c11；与 (5) 同为 P(t) 全次数形式。

历史名 `remez_atan_odd_first_algorithm_legacy` 保留，供对照；正式入口为 `remez_atan_odd_first`（默认 a=0）。

依赖：numpy；lp / remez1 / remez2 / report 需 scipy。Sollya 之 (5)(6) 与 `--run-sollya` / `--run-sollya-deg5` 需要。

Usage:
  .venv/bin/python script/parms_atan2.py                     # 默认：六路对照（report）
  .venv/bin/python script/parms_atan2.py --method remez1
  .venv/bin/python script/parms_atan2.py --method lp
  .venv/bin/python script/parms_atan2.py --method remez2
  .venv/bin/python script/parms_atan2.py --run-sollya        # 仅跑 Sollya deg11（12 常数），与 report 第 (5) 节同源
  .venv/bin/python script/parms_atan2.py --run-sollya-deg5   # 仅跑 Sollya deg5（六常数）
"""

# -----------------------------------------------------------------------------
# 六路系数快照（与 ``print_five_way_report()`` 终端输出应对齐；更新时以运行脚本为准）
# (1) 文章 mazzo.li — https://mazzo.li/posts/vectorized-atan2.html （与 common.hpp / RVV 默认一致）
#     a1 = 0.99997726f;
#     a3 = -0.33262347f;
#     a5 = 0.19354346f;
#     a7 = -0.11643287f;
#     a9 = 0.05265332f;
#     a11 = -0.01172120f;
# (2) Remez 第一算法风格（交换参考点，稳健版），奇次 a1..a11，拟合区间 [0,1]（a=0）
#     a1 = 0.9999772190799245f;
#     a3 = -0.3326228278409405f;
#     a5 = 0.1935403757741253f;
#     a7 = -0.1164264811875351f;
#     a9 = 0.05264735061895011f;
#     a11 = -0.01171913540713792f;
# (3) Remez 第二算法（数值：Powell 最小化密栅 max|err|，初值 LP），奇次至 t^11
#     a1 = 0.9999775690468202f;
#     a3 = -0.3326270073651729f;
#     a5 = 0.1935518578599886f;
#     a7 = -0.1164322962576437f;
#     a9 = 0.05263769152193183f;
#     a11 = -0.01171128155647548f;
# (4) 离散 LP（HiGHS），奇次
#     a1 = 0.9999775803753757f;
#     a3 = -0.3326270971752202f;
#     a5 = 0.1935520249746597f;
#     a7 = -0.1164321024444021f;
#     a9 = 0.05263708887719762f;
#     a11 = -0.01171095541590521f;
# (5) Sollya fpminimax 全次数 5（absolute），Horner c0..c5 — P(t) 非 t*odd(t^2)
#     c[ 0] = 2.093957818039105e-05f;
#     c[ 1] = 0.9982532435543107f;
#     c[ 2] = 0.023660529957079426f;
#     c[ 3] = -0.4511472680586187f;
#     c[ 4] = 0.2641312491225646f;
#     c[ 5] = -0.04949959117788765f;
# (6) Sollya fpminimax 全次数 11（absolute），Horner c0..c11（见 sollya_atan_fpminimax_report.txt）
#     c[ 0] = 1.692908826312289e-09f;
#     c[ 1] = 0.999999497998623f;
#     c[ 2] = 2.462210154373695e-05f;
#     c[ 3] = -0.33380530086035f;
#     c[ 4] = 0.004653922613137158f;
#     c[ 5] = 0.1731460010057108f;
#     c[ 6] = 0.09664877047164804f;
#     c[ 7] = -0.3642341265792391f;
#     c[ 8] = 0.3129036457771687f;
#     c[ 9] = -0.122116900202231f;
#     c[10] = 0.01736722164089919f;
#     c[11] = 0.0008108094305379294f;
# -----------------------------------------------------------------------------


def atan_odd_fourway_reference_comment():
    """说明见模块顶部「六路系数快照」注释块（便于搜索与文档交叉引用）。"""
    pass


import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MATH_SCRIPT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "script"))
for path in (SCRIPT_DIR, MATH_SCRIPT_DIR):
    if path not in sys.path:
        sys.path.insert(0, path)

from sollya_utils import (  # noqa: E402
    SOLLYA_ATAN_DEG11_ABSOLUTE,
    SOLLYA_ATAN_DEG5_ABSOLUTE,
    build_sollya_report_file,
    build_sollya_run_multiline_log,
    format_float_nsig,
    parse_sollya_horner_to_coeffs,
    print_sollya_atan_deg11_working,
    print_sollya_atan_deg5_working,
    print_sollya_atan_odd_example,
    run_sollya_file,
)

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


def _eval_poly_odd(ts, coeffs):
    """
    Evaluate t * (c0 + c1*t^2 + c2*t^4 + ...).
    coeffs[k] 对应 t^(2k+1) 的系数 a_{2k+1}.
    """
    t2 = ts * ts
    q = np.zeros_like(ts, dtype=np.float64)
    for c in reversed(coeffs):
        q = q * t2 + c
    return ts * q


def minimax_atan_odd_lp(
    m: int, a: float = 0.0, b: float = 1.0, n_grid: int = 8000
):
    """
    离散 L_inf 在栅格上（奇多项式 t^{1,3,...,2m+1}）。
    返回 (coeffs 列表, 栅格上 max|err|).
    """
    t = np.linspace(a, b, n_grid, dtype=np.float64)
    f = np.arctan(t)
    n_c = m + 1
    cols = [t ** (2 * j + 1) for j in range(n_c)]
    v = np.column_stack(cols)
    n = t.shape[0]
    a_ub = np.vstack([np.hstack([v, -np.ones((n, 1))]), np.hstack([-v, -np.ones((n, 1))])])
    b_ub = np.hstack([f, -f])
    c_obj = np.zeros(n_c + 1, dtype=np.float64)
    c_obj[n_c] = 1.0
    bounds = [(None, None)] * n_c + [(0.0, None)]
    r = linprog(
        c_obj, A_ub=a_ub, b_ub=b_ub, bounds=bounds, method="highs", options={"presolve": True}
    )
    if not r.success:
        raise RuntimeError(f"linprog failed: {r.message}")
    coeffs = r.x[:n_c]
    p = _eval_poly_odd(t, coeffs)
    max_err = float(np.max(np.abs(f - p)))
    return coeffs.tolist(), max_err


def remez_atan_odd_second(
    m: int,
    a: float = 0.0,
    b: float = 1.0,
    n_grid_lp_init: int = 12000,
    n_grid_objective: int = 200001,
    powell_maxiter: int = 800,
    powell_ftol: float = 1e-15,
):
    """
    「Remez 第二算法」常用数值替代：在等距密栅上最小化 max|atan(t)-P(t)|（无约束 6 元奇次系数），
    Powell + 初值 LP。在 [0,1] 上通常略优于纯 LP 一格点目标，与连续 minimax 非常接近。

    返回 (coeffs 列表, n_grid_objective 上的 max|err| 估值).
    """
    if not HAS_SCIPY:
        raise RuntimeError("remez2 需要 SciPy.optimize.minimize")
    n_c = m + 1
    t_obj = np.linspace(a, b, int(n_grid_objective), dtype=np.float64)
    c0 = np.array(
        minimax_atan_odd_lp(m, a, b, n_grid=n_grid_lp_init)[0],
        dtype=np.float64,
    )

    def obj(vec: np.ndarray) -> float:
        return float(np.max(np.abs(np.arctan(t_obj) - _eval_poly_odd(t_obj, vec))))

    r = minimize(
        obj,
        c0,
        method="Powell",
        options={"maxiter": int(powell_maxiter), "ftol": float(powell_ftol)},
    )
    if not r.success:
        raise RuntimeError(f"Powell 未收敛: {r.message}")
    coeffs = [float(x) for x in r.x]
    max_e = float(
        np.max(np.abs(np.arctan(t_obj) - _eval_poly_odd(t_obj, np.array(coeffs)))))
    return coeffs, max_e


def remez_atan_odd_first_algorithm_legacy(
    m, a=1e-8, b=1.0, max_iter=40, n_fine=20000
):
    """
    Remez 第一算法雏形：交换参考点失败时退化为均匀节点；atan 上 max|err| 常为 ~1e-4 rad 量级。
    生产请用 ``remez_atan_odd_second``；教科书对照请用 ``remez_atan_odd_first``（a=0）。
    """
    n = m
    n_ref = n + 2

    k = np.arange(1, n_ref + 1)
    x_cheb = np.cos((2 * k - 1) * np.pi / (2 * n_ref))
    ref = 0.5 * (a + b) + 0.5 * (b - a) * x_cheb
    ref = np.sort(ref)

    max_abs_err = None
    coeffs = np.zeros(n + 1, dtype=float)

    for _ in range(max_iter):
        cols = []
        for j in range(n + 1):
            cols.append(ref ** (2 * j + 1))
        v = np.column_stack(cols)
        col_e = np.array([(-1) ** i for i in range(n_ref)], dtype=float)
        a_m = np.column_stack([v, col_e])
        rhs = np.arctan(ref)

        try:
            sol = np.linalg.solve(a_m, rhs)
        except np.linalg.LinAlgError:
            break
        coeffs = sol[: n + 1]

        t_fine = np.linspace(a, b, n_fine)
        p_fine = _eval_poly_odd(t_fine, coeffs)
        e_fine = np.arctan(t_fine) - p_fine
        max_abs_err = float(np.max(np.abs(e_fine)))

        de = np.diff(e_fine)
        sign_changes = np.diff(np.sign(de)) != 0
        ext_idx = np.where(sign_changes)[0] + 1
        if len(ext_idx) == 0:
            ext_idx = np.array([np.argmax(np.abs(e_fine))])

        t_cand = np.concatenate([[a], t_fine[ext_idx], [b]])
        e_cand = np.arctan(t_cand) - _eval_poly_odd(t_cand, coeffs)
        order = np.argsort(t_cand)
        t_cand = t_cand[order]
        e_cand = e_cand[order]

        ref_new = []
        j = 0
        for i in range(n_ref):
            need = (-1) ** i
            while j < len(t_cand):
                s = np.sign(e_cand[j])
                if s == 0 or s == need:
                    ref_new.append(t_cand[j])
                    j += 1
                    break
                j += 1

        if len(ref_new) < n_ref:
            ref = np.linspace(a, b, n_ref)
        else:
            ref = np.array(sorted(ref_new[:n_ref]))

    return coeffs.tolist(), (max_abs_err if max_abs_err is not None else float("nan"))


def _fill_zero_signs(arr: np.ndarray) -> np.ndarray:
    """将 sign=0 的点用邻近非零符号填充，便于分段提取误差波瓣。"""
    s = np.sign(arr).astype(np.int8)
    n = s.shape[0]
    if n == 0:
        return s
    for i in range(1, n):
        if s[i] == 0:
            s[i] = s[i - 1]
    for i in range(n - 2, -1, -1):
        if s[i] == 0:
            s[i] = s[i + 1]
    if s[0] == 0:
        s[:] = 1
    return s


def _extract_lobe_extrema(ts: np.ndarray, err: np.ndarray):
    """
    按误差符号波瓣分段；每段取 |err| 最大点，得到天然交替符号的候选参考点。
    返回 [(t, e), ...]（按 t 升序）。
    """
    s = _fill_zero_signs(err)
    bounds = [0]
    for i in range(1, s.shape[0]):
        if s[i] != s[i - 1]:
            bounds.append(i)
    bounds.append(s.shape[0])
    lobes = []
    for bi in range(len(bounds) - 1):
        lo = bounds[bi]
        hi = bounds[bi + 1]
        if hi <= lo:
            continue
        seg = np.abs(err[lo:hi])
        if seg.size == 0:
            continue
        k = lo + int(np.argmax(seg))
        lobes.append((float(ts[k]), float(err[k])))
    return lobes


def _pick_alternating_refs_from_lobes(lobes, n_ref: int, old_ref: np.ndarray):
    """
    从交替波瓣候选中选 n_ref 个点。
    - 候选足够：取“连续窗口”中 |e| 总和最大的窗口（保持交替与有序）
    - 候选不足：用旧参考点补齐（保持单调）
    """
    if len(lobes) >= n_ref:
        best_i = 0
        best_score = -1.0
        abs_e = [abs(e) for _, e in lobes]
        win = float(sum(abs_e[:n_ref]))
        best_score = win
        for i in range(1, len(lobes) - n_ref + 1):
            win += abs_e[i + n_ref - 1] - abs_e[i - 1]
            if win > best_score:
                best_score = win
                best_i = i
        pts = [lobes[i][0] for i in range(best_i, best_i + n_ref)]
        return np.array(pts, dtype=np.float64)

    # 候选不足：以旧参考点补齐，避免粗暴退回均匀网格
    pts = [t for t, _ in lobes]
    if len(pts) == 0:
        return old_ref.copy()
    for t in old_ref:
        if len(pts) >= n_ref:
            break
        # 避免重复插入太近的点
        if min(abs(t - p) for p in pts) > 1e-12:
            pts.append(float(t))
    pts = np.array(sorted(pts), dtype=np.float64)
    if pts.shape[0] > n_ref:
        idx = np.linspace(0, pts.shape[0] - 1, n_ref, dtype=int)
        pts = pts[idx]
    return pts


def _remez_atan_odd_first_stable(
    m: int, a: float = 0.0, b: float = 1.0, max_iter: int = 80, n_fine: int = 200001
):
    """
    稳健版 Remez 第一算法风格（交换法）：
    - 线性方程组求交替误差系数
    - 误差曲线按符号波瓣提取极值点
    - 以最大振幅连续窗口更新参考点，减少退化
    """
    n = int(m)
    n_ref = n + 2
    k = np.arange(1, n_ref + 1)
    x_cheb = np.cos((2 * k - 1) * np.pi / (2 * n_ref))
    ref = np.sort(0.5 * (a + b) + 0.5 * (b - a) * x_cheb).astype(np.float64)

    coeffs = np.zeros(n + 1, dtype=np.float64)
    max_abs_err = float("nan")
    last_e = None

    for _ in range(int(max_iter)):
        v = np.column_stack([ref ** (2 * j + 1) for j in range(n + 1)])
        col_e = np.array([(-1) ** i for i in range(n_ref)], dtype=np.float64)
        a_m = np.column_stack([v, col_e])
        rhs = np.arctan(ref)
        try:
            sol = np.linalg.solve(a_m, rhs)
        except np.linalg.LinAlgError:
            sol, *_ = np.linalg.lstsq(a_m, rhs, rcond=None)
        coeffs = sol[: n + 1]
        e_alt = float(sol[n + 1])

        t_fine = np.linspace(a, b, int(n_fine), dtype=np.float64)
        e_fine = np.arctan(t_fine) - _eval_poly_odd(t_fine, coeffs)
        max_abs_err = float(np.max(np.abs(e_fine)))

        lobes = _extract_lobe_extrema(t_fine, e_fine)
        ref_new = _pick_alternating_refs_from_lobes(lobes, n_ref, ref)
        ref_new = np.clip(ref_new, a, b)
        ref_new = np.unique(ref_new)
        if ref_new.shape[0] < n_ref:
            # 极端情况下补齐，保持严格单调
            merged = np.unique(np.concatenate([ref_new, ref]))
            if merged.shape[0] < n_ref:
                merged = np.linspace(a, b, n_ref)
            if merged.shape[0] > n_ref:
                idx = np.linspace(0, merged.shape[0] - 1, n_ref, dtype=int)
                merged = merged[idx]
            ref_new = merged

        # 收敛：参考点几乎不动且交替误差变化小
        ref_shift = float(np.max(np.abs(ref_new - ref)))
        if last_e is not None and ref_shift < 1e-12 and abs(e_alt - last_e) < 1e-15:
            ref = ref_new
            break
        ref = ref_new
        last_e = e_alt

    return coeffs.tolist(), max_abs_err


def remez_atan_odd_first(
    m: int,
    a: float = 0.0,
    b: float = 1.0,
    max_iter: int = 80,
    n_fine: int = 200001,
):
    """Remez 第一算法风格（交换极值点）的稳健实现。默认 a=0 覆盖完整 [0,1]。"""
    return _remez_atan_odd_first_stable(m, a=a, b=b, max_iter=max_iter, n_fine=n_fine)


def validate_on_signed_interval(coeffs, n=200000):
    xs = np.linspace(-1.0, 1.0, n, dtype=float)
    ref = np.arctan(xs)
    appr = _eval_poly_odd(xs, np.array(coeffs, dtype=float))
    abs_err = np.abs(ref - appr)
    rel_err = abs_err / np.maximum(np.abs(ref), 1e-12)
    return float(np.max(abs_err)), float(np.max(rel_err))


def _c_float_literal(c: float) -> str:
    return format(float(c), ".16g") + "f;"


def print_coeffs(coeffs):
    for i, c in enumerate(coeffs):
        p = 2 * i + 1
        print("a%d =" % p, _c_float_literal(c))


def print_full_horner_coeffs(coeffs):
    """Sollya Horner 链上的常数 c0..cN（全次数多项式）。"""
    for i, c in enumerate(coeffs):
        print("c[%2d] =" % i, _c_float_literal(c))


def print_five_way_report(
    max_odd: int = 11,
    grid_lp: int = 8000,
    powell_grid: int = 200001,
    sollya_bin: str = "sollya",
):
    """打印 (1) 文章 (2) remez1 (3) remez2 (4) lp (5) sollya deg5 (6) sollya deg11。"""
    import shutil

    if not HAS_NUMPY or not HAS_SCIPY:
        raise SystemExit("六路报告需要 numpy 与 scipy")
    if max_odd < 1 or (max_odd % 2 == 0):
        raise SystemExit("--max-odd 须为正奇数")
    m = (max_odd - 1) // 2

    print("=== (1) 文章 mazzo.li（仅注释；工程用 common.hpp 与下文脚本数值交叉核对）===")
    print("https://mazzo.li/posts/vectorized-atan2.html")
    print(
        "# 文章常用：a1=0.99997726f; a3=-0.33262347f; a5=0.19354346f; "
        "a7=-0.11643287f; a9=0.05265332f; a11=-0.01172120f;"
    )
    print()

    print("=== (2) Remez 第一算法风格（交换参考点 + 线性方程组）奇次 a1..a11，[0,1] a=0 ===")
    cr1, er1 = remez_atan_odd_first(m)
    t_chk = np.linspace(0.0, 1.0, int(powell_grid), dtype=np.float64)
    er1_grid = float(
        np.max(
            np.abs(np.arctan(t_chk) - _eval_poly_odd(t_chk, np.array(cr1, dtype=np.float64)))
        )
    )
    print(
        f"# max|atan-P|：迭代末 {er1:.16g}；与 remez2 同宽栅格 {powell_grid} 点复检 {er1_grid:.16g}"
    )
    print_coeffs(cr1)
    print()

    print("=== (3) Remez 第二算法（数值实现：密栅 max|err| + Powell，初值 LP）奇次 a1..a11 ===")
    cr2, er2 = remez_atan_odd_second(m, n_grid_objective=int(powell_grid))
    print(f"# 估计 max|atan-P| on Powell 栅格: {er2:.16g}")
    print_coeffs(cr2)
    print()

    print("=== (4) 离散 LP（HiGHS）奇次 a1..a11 ===")
    clp, elp = minimax_atan_odd_lp(m, 0.0, 1.0, n_grid=grid_lp)
    print(f"# 估计 max|atan-P| on LP 栅格: {elp:.16g}")
    print_coeffs(clp)
    print()

    print("=== (5) Sollya fpminimax 全次数 5（六常数 c0..c5；核为 P(t) 非 t*odd(t^2)）===")
    print("# 脚本: " + print_sollya_atan_deg5_working().splitlines()[0])
    if not shutil.which(sollya_bin):
        print(f"# （未找到 {sollya_bin}，跳过运行；可用静态快照见模块顶注释）")
    else:
        code, out, err = run_sollya_file(SOLLYA_ATAN_DEG5_ABSOLUTE, sollya_bin=sollya_bin)
        line = (out or "").strip().splitlines()
        horner_line = line[-1] if line else ""
        if horner_line:
            try:
                hc = parse_sollya_horner_to_coeffs(horner_line)
                print(f"# sollya exit code: {code}")
                print_full_horner_coeffs(hc)
            except (ValueError, TypeError) as ex:
                print(f"# 解析失败: {ex}")
                print("# stdout:", horner_line[:200])
        else:
            print("# 无 Horner 输出")
        if (err or "").strip():
            print("# stderr:", err.strip()[:300])
    print()

    print("=== (6) Sollya fpminimax 全次数 11（十二常数 c0..c11；absolute）===")
    print("# 脚本: " + print_sollya_atan_deg11_working().splitlines()[0])
    if not shutil.which(sollya_bin):
        print(f"# （未找到 {sollya_bin}，跳过运行；可用静态快照见模块顶注释 (5)）")
    else:
        code, out, err = run_sollya_file(SOLLYA_ATAN_DEG11_ABSOLUTE, sollya_bin=sollya_bin)
        line = (out or "").strip().splitlines()
        horner_line = line[-1] if line else ""
        if horner_line:
            try:
                hc = parse_sollya_horner_to_coeffs(horner_line)
                print(f"# sollya exit code: {code}")
                print_full_horner_coeffs(hc)
            except (ValueError, TypeError) as ex:
                print(f"# 解析失败: {ex}")
                print("# stdout:", horner_line[:200])
        else:
            print("# 无 Horner 输出")
        if (err or "").strip():
            print("# stderr:", err.strip()[:300])
    print()
    print("# 同步维护：请将本输出与文件顶部「六路系数快照」及 atan2_test.cpp / remez-coeffs.zh.md 对齐。")


def _run_sollya_common(args, script: str, label: str, report_builder_lines):
    import shutil

    if not shutil.which(args.sollya_bin):
        raise SystemExit(f"未找到 {args.sollya_bin}，请安装 Sollya 或指定 --sollya-bin")
    code, out, err = run_sollya_file(script, sollya_bin=args.sollya_bin)
    line = (out or "").strip().splitlines()
    horner_line = line[-1] if line else ""
    perr = ""
    horner_coeffs = None
    if horner_line:
        try:
            horner_coeffs = parse_sollya_horner_to_coeffs(horner_line)
        except (ValueError, TypeError) as e:
            perr = str(e)

    print(f"=== {label}：Horner（.16g + f;）===")
    _nsig = max(1, min(17, int(args.sollya_sig)))
    if horner_coeffs is not None:
        for i, c in enumerate(horner_coeffs):
            if _nsig == 16:
                print("c[%2d] =" % i, _c_float_literal(c))
            else:
                print("c[%2d] =" % i, format_float_nsig(c, _nsig) + "f;")
    else:
        print("  (无法解析 Sollya 行)")
    print(f"sollya exit code: {code}")
    if perr:
        print(f"（解析注：{perr}）", file=sys.stderr)
    if err.strip():
        print("--- sollya stderr（摘要）---")
        print(err.rstrip()[:500] + ("…" if len(err) > 500 else ""))
    if not args.sollya_no_log:
        log_p = (args.sollya_log or "").strip() or os.path.join(
            os.path.dirname(SCRIPT_DIR), "output", "sollya_atan_remez.log"
        )
        log_p = os.path.abspath(log_p)
        os.makedirs(os.path.dirname(log_p), exist_ok=True)
        body = build_sollya_run_multiline_log(
            out or "", err, code, horner_coeffs, perr, line_width=int(args.sollya_wrap)
        )
        with open(log_p, "w", encoding="utf-8") as fp:
            fp.write(body)
        print(f"多行全量已写入: {log_p}")
    if args.sollya_save:
        report = build_sollya_report_file("\n".join(report_builder_lines), code, out, err)
        save_p = os.path.abspath(args.sollya_save)
        d = os.path.dirname(save_p)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(save_p, "w", encoding="utf-8") as fp:
            fp.write(report)
        print(f"已写入（// 注释报告）: {save_p}")


def main():
    ap = argparse.ArgumentParser(
        description="atan 奇次系数 / 六路对照 / Sollya"
    )
    ap.add_argument("--max-odd", type=int, default=11, help="最高奇次，例如 11 => a1..a11")
    ap.add_argument(
        "--method",
        choices=["report", "lp", "remez1", "remez2"],
        default="report",
        help="report(默认): 六路对照; lp / remez1 / remez2: 仅打印一路系数",
    )
    ap.add_argument(
        "--powell-grid",
        type=int,
        default=200001,
        help="(report/remez2) Powell 目标栅格点数（report 中 remez1 亦用此栅格复检）",
    )
    ap.add_argument("--grid", type=int, default=8000, help="(lp / report) LP 栅格点数")
    ap.add_argument(
        "--print-sollya",
        action="store_true",
        help="打印可保存的 Sollya 脚本：deg11 与 odd-only 试验片段",
    )
    ap.add_argument("--run-sollya", action="store_true", help="运行 Sollya deg11（12 常数）")
    ap.add_argument(
        "--run-sollya-deg5",
        action="store_true",
        help="运行 Sollya deg5（6 常数，与 report 第 (4) 节一致）",
    )
    ap.add_argument("--sollya-save", metavar="PATH", default="")
    ap.add_argument("--sollya-bin", default="sollya")
    ap.add_argument("--sollya-sig", type=int, default=16, metavar="N")
    ap.add_argument("--sollya-no-log", action="store_true")
    ap.add_argument("--sollya-log", default="", metavar="PATH")
    ap.add_argument("--sollya-wrap", type=int, default=100, metavar="W")
    args = ap.parse_args()

    if args.run_sollya:
        lines = print_sollya_atan_deg11_working().splitlines()
        _run_sollya_common(
            args,
            SOLLYA_ATAN_DEG11_ABSOLUTE,
            "fpminimax atan deg11 absolute",
            lines,
        )
        return

    if args.run_sollya_deg5:
        lines = print_sollya_atan_deg5_working().splitlines()
        _run_sollya_common(
            args,
            SOLLYA_ATAN_DEG5_ABSOLUTE,
            "fpminimax atan deg5 absolute（六常数）",
            lines,
        )
        return

    if args.print_sollya:
        print(print_sollya_atan_deg11_working())
        print()
        print("--- deg5（六常数，report 第 (4) 节）---")
        print(print_sollya_atan_deg5_working())
        print()
        print("--- odd-only monomials (多数 Sollya 版本会失败，仅作试验) ---")
        if args.max_odd < 1 or (args.max_odd % 2 == 0):
            raise SystemExit("--max-odd 须为正奇数")
        print(print_sollya_atan_odd_example(args.max_odd))
        return

    if args.max_odd < 1 or (args.max_odd % 2 == 0):
        raise SystemExit("--max-odd 必须为正奇数，例如 11")
    if not HAS_NUMPY:
        raise SystemExit("需要 numpy")

    m = (args.max_odd - 1) // 2

    if args.method == "report":
        if not HAS_SCIPY:
            raise SystemExit("五路 report 需要 scipy")
        print_five_way_report(
            max_odd=args.max_odd,
            grid_lp=args.grid,
            powell_grid=args.powell_grid,
            sollya_bin=args.sollya_bin,
        )
        return

    if not HAS_SCIPY:
        raise SystemExit("需要 SciPy")

    if args.method == "lp":
        coeffs, max_err_01 = minimax_atan_odd_lp(m, 0.0, 1.0, n_grid=args.grid)
        title = "离散 L_inf（HiGHS）"
    elif args.method == "remez1":
        coeffs, max_err_01 = remez_atan_odd_first(m)
        title = "Remez 第一算法风格（交换参考点）"
    else:
        coeffs, max_err_01 = remez_atan_odd_second(
            m, n_grid_objective=int(args.powell_grid)
        )
        title = "Remez 第二算法（Powell 密栅 min-max）"

    print(title)
    print(
        f"atan(t) ~= t * (a1 + a3*t^2 + ... + a{args.max_odd}*t^{args.max_odd - 1})"
    )
    t2 = np.linspace(0, 1, 200_000, dtype=np.float64)
    e2 = float(
        np.max(
            np.abs(
                np.arctan(t2)
                - _eval_poly_odd(t2, np.array(coeffs, dtype=np.float64))
            )
        )
    )
    print(f"max |atan - P| on [0,1] (200k 校验点): {e2:.16g}  (估计: {max_err_01:.16g})")
    max_abs_signed, max_rel_signed = validate_on_signed_interval(coeffs)
    print(f"max abs err on [-1,1] (作为 atan 奇延拓): {max_abs_signed:.16g}")
    print(f"max rel err on [-1,1] (|err|/|ref|):      {max_rel_signed:.16g}")
    print("")
    print_coeffs(coeffs)


if __name__ == "__main__":
    main()
