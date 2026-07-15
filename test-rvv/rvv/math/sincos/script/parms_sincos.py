#!/usr/bin/env python3
"""
sinf/cosf finite-domain paired helper 参数脚本。

目标：
  - scratch-only，不对应 production helper；
  - 输入合同先限定 x in [-pi, pi]，另报告 [-pi/2, pi/2]；
  - bounded mask 分段，把 r 约化到 [-pi/4, pi/4]；
  - 对比 Taylor baseline 与离散 LP/minimax-on-grid absolute-error 候选。

报告口径：
  - kernel error：只看 reduced interval 上的 sin(r)/cos(r) 多项式；
  - dense-chain simulation：包含 float32 输入、mask 分段、float32 FMA Horner 和象限重构；
  - relative error 不作为主指标，因为 sin 在过零点附近会失效。
"""

from __future__ import annotations

import math
import struct

try:
    import numpy as np

    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    from scipy.optimize import linprog

    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


def f32(x):
    return np.float32(x)


def fma32(a, b, c):
    return np.float32(
        np.asarray(a, dtype=np.float64) * np.asarray(b, dtype=np.float64)
        + np.asarray(c, dtype=np.float64)
    )


K_PI = f32(math.pi)
K_PI_2 = f32(math.pi / 2.0)
K_PI_4 = f32(math.pi / 4.0)
K_3PI_4 = f32(3.0 * math.pi / 4.0)
K_PI_LO = f32(math.pi - float(K_PI))
K_PI_2_LO = f32(math.pi / 2.0 - float(K_PI_2))

SIN_TAYLOR = [
    f32(-1.0 / 6.0),
    f32(1.0 / 120.0),
    f32(-1.0 / 5040.0),
    f32(1.0 / 362880.0),
]

COS_TAYLOR = [
    f32(-1.0 / 2.0),
    f32(1.0 / 24.0),
    f32(-1.0 / 720.0),
    f32(1.0 / 40320.0),
]


def bits_of_f32(v: np.float32) -> str:
    return f"0x{struct.unpack('<I', struct.pack('<f', float(v)))[0]:08x}"


def fit_lp_abs_sincos(degree_terms: int = 4, n_grid: int = 40001):
    if not HAS_SCIPY:
        raise RuntimeError("lp-abs candidate requires scipy.optimize.linprog")

    r = np.linspace(-float(K_PI_4), float(K_PI_4), int(n_grid), dtype=np.float64)

    def solve_lp(basis, target):
        n = target.shape[0]
        a_ub = np.vstack(
            [
                np.hstack([basis, -np.ones((n, 1), dtype=np.float64)]),
                np.hstack([-basis, -np.ones((n, 1), dtype=np.float64)]),
            ]
        )
        b_ub = np.hstack([target, -target])
        c_obj = np.zeros(basis.shape[1] + 1, dtype=np.float64)
        c_obj[-1] = 1.0
        bounds = [(None, None)] * basis.shape[1] + [(0.0, None)]
        res = linprog(c_obj, A_ub=a_ub, b_ub=b_ub, bounds=bounds, method="highs")
        if not res.success:
            raise RuntimeError(f"linprog failed: {res.message}")
        coeffs64 = res.x[:-1]
        grid_err64 = float(np.max(np.abs(target - basis @ coeffs64)))
        return [f32(v) for v in coeffs64], grid_err64

    sin_basis = np.column_stack([r ** (2 * j + 3) for j in range(degree_terms)])
    sin_target = np.sin(r) - r
    cos_basis = np.column_stack([r ** (2 * j + 2) for j in range(degree_terms)])
    cos_target = np.cos(r) - 1.0
    sin_coeffs, sin_lp_grid_err = solve_lp(sin_basis, sin_target)
    cos_coeffs, cos_lp_grid_err = solve_lp(cos_basis, cos_target)
    return sin_coeffs, cos_coeffs, sin_lp_grid_err, cos_lp_grid_err


def sin_kernel_f32(r, coeffs):
    r = np.asarray(r, dtype=np.float32)
    r2 = np.float32(r * r)
    p = np.full_like(r, coeffs[3], dtype=np.float32)
    p = fma32(r2, p, coeffs[2])
    p = fma32(r2, p, coeffs[1])
    p = fma32(r2, p, coeffs[0])
    return fma32(np.float32(r * r2), p, r)


def cos_kernel_f32(r, coeffs):
    r = np.asarray(r, dtype=np.float32)
    r2 = np.float32(r * r)
    p = np.full_like(r, coeffs[3], dtype=np.float32)
    p = fma32(r2, p, coeffs[2])
    p = fma32(r2, p, coeffs[1])
    p = fma32(r2, p, coeffs[0])
    return fma32(r2, p, np.float32(1.0))


def sub_pi_f32(x, use_hi_lo):
    if not use_hi_lo:
        return np.float32(x - K_PI)
    return np.float32(np.float32(x - K_PI) - K_PI_LO)


def add_pi_f32(x, use_hi_lo):
    if not use_hi_lo:
        return np.float32(x + K_PI)
    return np.float32(np.float32(x + K_PI) + K_PI_LO)


def sub_pi2_f32(x, use_hi_lo):
    if not use_hi_lo:
        return np.float32(x - K_PI_2)
    return np.float32(np.float32(x - K_PI_2) - K_PI_2_LO)


def add_pi2_f32(x, use_hi_lo):
    if not use_hi_lo:
        return np.float32(x + K_PI_2)
    return np.float32(np.float32(x + K_PI_2) + K_PI_2_LO)


def sincos_chain_f32(xs, sin_coeffs, cos_coeffs, use_hi_lo=False):
    x = np.asarray(xs, dtype=np.float32)
    r = x.copy()
    sin_sel = np.zeros_like(x, dtype=np.int8)
    cos_sel = np.ones_like(x, dtype=np.int8)

    hi = x > K_3PI_4
    mid = (x > K_PI_4) & (x <= K_3PI_4)
    center = (x >= -K_PI_4) & (x <= K_PI_4)
    neg_mid = (x >= -K_3PI_4) & (x < -K_PI_4)
    neg_hi = x < -K_3PI_4

    r[hi] = sub_pi_f32(x[hi], use_hi_lo)
    sin_sel[hi] = 1
    cos_sel[hi] = 3

    r[mid] = sub_pi2_f32(x[mid], use_hi_lo)
    sin_sel[mid] = 2
    cos_sel[mid] = 1

    r[center] = x[center]
    sin_sel[center] = 0
    cos_sel[center] = 2

    r[neg_mid] = add_pi2_f32(x[neg_mid], use_hi_lo)
    sin_sel[neg_mid] = 3
    cos_sel[neg_mid] = 0

    r[neg_hi] = add_pi_f32(x[neg_hi], use_hi_lo)
    sin_sel[neg_hi] = 1
    cos_sel[neg_hi] = 3

    sin_r = sin_kernel_f32(r, sin_coeffs)
    cos_r = cos_kernel_f32(r, cos_coeffs)

    s = np.empty_like(x, dtype=np.float32)
    c = np.empty_like(x, dtype=np.float32)
    s[sin_sel == 0] = sin_r[sin_sel == 0]
    s[sin_sel == 1] = np.float32(-sin_r[sin_sel == 1])
    s[sin_sel == 2] = cos_r[sin_sel == 2]
    s[sin_sel == 3] = np.float32(-cos_r[sin_sel == 3])

    c[cos_sel == 0] = sin_r[cos_sel == 0]
    c[cos_sel == 1] = np.float32(-sin_r[cos_sel == 1])
    c[cos_sel == 2] = cos_r[cos_sel == 2]
    c[cos_sel == 3] = np.float32(-cos_r[cos_sel == 3])

    zero = x == np.float32(0.0)
    s[zero] = x[zero]
    c[zero] = np.float32(1.0)
    return s, c


def print_constants():
    print("=== sin/cos finite-domain setup ===")
    print("reduced interval: r in [-pi/4, pi/4]")
    print("range reduction: bounded mask segmentation, no vfcvt/FRM dependency")
    print(f"pi      = {float(K_PI):.16g}f  ({bits_of_f32(K_PI)})")
    print(f"pi_lo   = {float(K_PI_LO):.16g}f  ({bits_of_f32(K_PI_LO)})")
    print(f"pi/2    = {float(K_PI_2):.16g}f  ({bits_of_f32(K_PI_2)})")
    print(f"pi/2_lo = {float(K_PI_2_LO):.16g}f  ({bits_of_f32(K_PI_2_LO)})")
    print(f"pi/4    = {float(K_PI_4):.16g}f  ({bits_of_f32(K_PI_4)})")
    print(f"3*pi/4  = {float(K_3PI_4):.16g}f  ({bits_of_f32(K_3PI_4)})")
    print("model:")
    print("  sin(r) = fma(r^3, P(r^2), r), P by float32 FMA Horner")
    print("  cos(r) = fma(r^2, Q(r^2), 1), Q by float32 FMA Horner")


def print_coeffs(label: str, sin_coeffs, cos_coeffs, lp_grid=None):
    print(f"\n=== coefficients: {label} ===")
    if lp_grid is not None:
        sin_grid, cos_grid = lp_grid
        print(f"LP double-grid max abs before f32 quantization: sin={sin_grid:.9e}, cos={cos_grid:.9e}")
    print("sin coefficients s0..s3:")
    for i, v in enumerate(sin_coeffs):
        print(f"  s{i} = {float(v):.16g}f;  {bits_of_f32(v)}")
    print("cos coefficients c0..c3:")
    for i, v in enumerate(cos_coeffs):
        print(f"  c{i} = {float(v):.16g}f;  {bits_of_f32(v)}")


def error_summary(values, approx, ref):
    err = np.abs(ref - approx.astype(np.float64))
    idx = int(np.argmax(err))
    return {
        "max": float(err[idx]),
        "mean": float(np.mean(err)),
        "at": float(values[idx]),
    }


def make_adversarial_points():
    vals = []
    for base in [
        f32(0.0),
        f32(-0.0),
        K_PI_4,
        f32(-K_PI_4),
        K_PI_2,
        f32(-K_PI_2),
        K_3PI_4,
        f32(-K_3PI_4),
        K_PI,
        f32(-K_PI),
    ]:
        vals.append(np.nextafter(base, f32(-np.inf), dtype=np.float32))
        vals.append(base)
        vals.append(np.nextafter(base, f32(np.inf), dtype=np.float32))
    return np.array(vals, dtype=np.float32)


def report_candidate(label: str, sin_coeffs, cos_coeffs, use_hi_lo=False):
    print(f"\n=== report: {label} ===")
    r = np.linspace(-float(K_PI_4), float(K_PI_4), 200001, dtype=np.float32)
    rr = r.astype(np.float64)
    sin_k = error_summary(rr, sin_kernel_f32(r, sin_coeffs), np.sin(rr))
    cos_k = error_summary(rr, cos_kernel_f32(r, cos_coeffs), np.cos(rr))
    print("kernel r in [-pi/4, pi/4]:")
    print(f"  sin max abs = {sin_k['max']:.9e} at r = {sin_k['at']:.9e}, mean = {sin_k['mean']:.9e}")
    print(f"  cos max abs = {cos_k['max']:.9e} at r = {cos_k['at']:.9e}, mean = {cos_k['mean']:.9e}")

    for lo, hi, n, name in [
        (-float(K_PI), float(K_PI), 400001, "x in [-pi, pi]"),
        (-float(K_PI_2), float(K_PI_2), 200001, "x in [-pi/2, pi/2]"),
    ]:
        xs = np.linspace(lo, hi, n, dtype=np.float32)
        xd = xs.astype(np.float64)
        sin_approx, cos_approx = sincos_chain_f32(xs, sin_coeffs, cos_coeffs, use_hi_lo)
        sin_e = error_summary(xd, sin_approx, np.sin(xd))
        cos_e = error_summary(xd, cos_approx, np.cos(xd))
        print(f"dense-chain {name}:")
        print(f"  sin max abs = {sin_e['max']:.9e} at x = {sin_e['at']:.9e}, mean = {sin_e['mean']:.9e}")
        print(f"  cos max abs = {cos_e['max']:.9e} at x = {cos_e['at']:.9e}, mean = {cos_e['mean']:.9e}")

    xs = make_adversarial_points()
    xd = xs.astype(np.float64)
    sin_approx, cos_approx = sincos_chain_f32(xs, sin_coeffs, cos_coeffs, use_hi_lo)
    sin_e = error_summary(xd, sin_approx, np.sin(xd))
    cos_e = error_summary(xd, cos_approx, np.cos(xd))
    print("adversarial: 0, +/-pi/4, +/-pi/2, +/-3pi/4, +/-pi and nextafter neighbors:")
    print(f"  sin max abs = {sin_e['max']:.9e} at x = {sin_e['at']:.9e}, mean = {sin_e['mean']:.9e}")
    print(f"  cos max abs = {cos_e['max']:.9e} at x = {cos_e['at']:.9e}, mean = {cos_e['mean']:.9e}")


def main():
    if not HAS_NUMPY:
        raise SystemExit("parms_sincos.py requires numpy")
    if not HAS_SCIPY:
        raise SystemExit("parms_sincos.py lp-abs candidate requires scipy")

    sin_lp, cos_lp, sin_grid, cos_grid = fit_lp_abs_sincos()
    coefficient_sets = [
        ("taylor", SIN_TAYLOR, COS_TAYLOR, None),
        ("lp-abs", sin_lp, cos_lp, (sin_grid, cos_grid)),
    ]
    reports = [
        ("taylor", SIN_TAYLOR, COS_TAYLOR, False),
        ("taylor-hi-lo", SIN_TAYLOR, COS_TAYLOR, True),
        ("lp-abs", sin_lp, cos_lp, False),
        ("lp-abs-hi-lo", sin_lp, cos_lp, True),
    ]

    print_constants()
    for label, sin_c, cos_c, lp_grid in coefficient_sets:
        print_coeffs(label, sin_c, cos_c, lp_grid)
    for label, sin_c, cos_c, use_hi_lo in reports:
        report_candidate(label, sin_c, cos_c, use_hi_lo)

    print("\nnotes:")
    print("  - lp-abs is generated by scipy.optimize.linprog on an equal-spaced grid.")
    print("  - Coefficients are quantized to float32 before all reported errors.")
    print("  - This is a finite-domain helper, not an arbitrary finite float libm replacement.")
    print("  - Domain-out lanes must not be silently clamped in production experiments.")


if __name__ == "__main__":
    main()
