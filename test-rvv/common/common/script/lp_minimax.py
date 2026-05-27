#!/usr/bin/env python3
"""
Discrete L_infinity (minimax) polynomial approximation helpers.
"""

from __future__ import annotations

from typing import Callable, List, Tuple

import numpy as np

try:
    from scipy.optimize import linprog

    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


def minimax_polynomial_lp(
    f: Callable[[np.ndarray], np.ndarray],
    a: float,
    b: float,
    degree: int,
    n_grid: int = 8000,
    e_lower: float = 1e-300,
) -> Tuple[List[float], float]:
    """
    Return (coeffs c0..cD, max_abs_error_on_grid) for P(t)=sum c_i t^i minimizing
    discrete L_inf error to f(t) on linspace(a,b,n_grid).
    """
    if not HAS_SCIPY:
        raise RuntimeError("scipy required for LP minimax (pip install scipy)")
    if degree < 0:
        raise ValueError("degree must be >= 0")
    t = np.linspace(a, b, n_grid, dtype=np.float64)
    y = np.asarray(f(t), dtype=np.float64)
    d1 = degree + 1
    v = np.column_stack([t**i for i in range(d1)])
    n = t.shape[0]
    a_ub = np.vstack([np.hstack([v, -np.ones((n, 1))]), np.hstack([-v, -np.ones((n, 1))])])
    b_ub = np.hstack([y, -y])
    c_obj = np.zeros(d1 + 1, dtype=np.float64)
    c_obj[d1] = 1.0
    bounds = [(None, None)] * d1 + [(max(e_lower, 0.0), None)]
    r = linprog(
        c_obj,
        A_ub=a_ub,
        b_ub=b_ub,
        bounds=bounds,
        method="highs",
        options={"presolve": True},
    )
    if not r.success:
        raise RuntimeError(f"linprog failed: {r.message}")
    coeffs = r.x[:d1].tolist()
    p = np.polyval(np.array(coeffs[::-1], dtype=np.float64), t)
    max_err = float(np.max(np.abs(y - p)))
    return coeffs, max_err
