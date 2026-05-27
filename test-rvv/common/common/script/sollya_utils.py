#!/usr/bin/env python3
"""
Shared helpers for Sollya script generation, execution and output parsing.
"""

from __future__ import annotations

import re
import textwrap
from datetime import datetime, timezone
from typing import List, Optional, Tuple

# Sollya 8 上可收敛：在 [0,1] 上对 atan(x) 的 11 次全项 minimax，absolute 模式。
# 与 PCL/mazzo 的奇次因式 t*(a1+a3 t^2+...) 不是同一基；仅作「连续区间 minimax」参照。
SOLLYA_ATAN_DEG11_ABSOLUTE = """display=decimal!;
I = [0;1];
p = fpminimax(atan(x), 11, [|D,D,D,D,D,D,D,D,D,D,D,D|], I, absolute);
p;
quit;
"""

# Sollya：全次数 5（六个 D）→ 与奇次「六个浮点」数量对齐；核为 P(t)=Σ c_i t^i（Horner），非 t*(a1+a3 t^2+…)。
SOLLYA_ATAN_DEG5_ABSOLUTE = """display=decimal!;
I = [0;1];
p = fpminimax(atan(x), 5, [|D,D,D,D,D,D|], I, absolute);
p;
quit;
"""


def print_sollya_atan_deg11_working() -> str:
    """可直接 `sollya <file.sollya>` 运行并得到 Horner 形式多项式。"""
    return "\n".join(
        [
            "// fpminimax: atan on [0,1], full degree 11, absolute (Sollya 8+ OK on typical installs)",
            "// 与 mazzo/PCL 的奇次因式不同；系数对照请仍用 --method lp 或文章常数。",
        ]
    ) + "\n" + SOLLYA_ATAN_DEG11_ABSOLUTE


def print_sollya_atan_deg5_working() -> str:
    """与 parms_atan2 四路对照中 Sollya(6) 同源。"""
    return "\n".join(
        [
            "// fpminimax: atan on [0,1], full degree 5, absolute — 6×D, Horner 常数 c0..c5",
            "// 与 PCL 奇次核不同：atan2 中应使用 P(t) 全次数 Horner，而非 t*odd(t^2)。",
        ]
    ) + "\n" + SOLLYA_ATAN_DEG5_ABSOLUTE


def print_sollya_atan_odd_example(max_odd: int = 11) -> str:
    """
    仅奇次幂 monomial 列表；在 Sollya 8 上常因非 Haar/不收敛而失败。保留作试验。
    """
    mons = ", ".join(str(i) for i in range(1, max_odd + 1, 2))
    return f"""// WARNING: odd-only fpminimax often fails on Sollya 8 (non-Haar / no convergence). Prefer --run-sollya or --method lp.
display=decimal!;
I = [0;1];
p = fpminimax(atan(x), [|{mons}|], [|D,D,D,D,D,D|], I, relative);
p;
""".strip()


# Sollya 打印的 Horner：c0 + x * (c1 + x * (... + x * cN)) ；最末可为 + x * 常数 而无内层括号。
_HORNER_FLOAT = r"^([+-]?(?:\d+\.?\d*|\d*\.?\d+)(?:[eE][+-]?\d+)?)"


def _bal_inner_after_open(s: str, open_i: int) -> Tuple[str, int]:
    """s[open_i] == '('，返回与之匹配的 (内层子串, 右括号后一位置)。"""
    d = 0
    for j in range(open_i, len(s)):
        if s[j] == "(":
            d += 1
        elif s[j] == ")":
            d -= 1
            if d == 0:
                return s[open_i + 1 : j], j + 1
    raise ValueError("unbalanced '()' in sollya horner string")


def parse_sollya_horner_to_coeffs(horner_line: str) -> List[float]:
    """
    将 Sollya 输出的单行 Horner 式解析为链上常数 [c0, c1, ..., cN]（与嵌套求值一致）。
    与 P(t)=sum b_k t^k 的幂系数不同；仅用于对照 Sollya 行。
    """
    s = re.sub(r"\s+", "", (horner_line or "").strip())
    if not s:
        raise ValueError("empty horner string")

    def _rec(tail: str) -> List[float]:
        m = re.match(_HORNER_FLOAT, tail)
        if not m:
            raise ValueError("expected float: " + tail[:50])
        c0 = float(m.group(1))
        rest = tail[len(m.group(1)) :]
        if not rest:
            return [c0]
        if not rest.startswith("+x*"):
            raise ValueError("expected +x* after Horner constant")
        u = rest[3:]
        if u.startswith("("):
            inner, end = _bal_inner_after_open(u, 0)
            if end != len(u):
                raise ValueError("trailing after (): " + u[end : end + 20])
            return [c0] + _rec(inner)
        m2 = re.match(_HORNER_FLOAT + r"$", u)
        if m2:
            return [c0, float(m2.group(1))]
        raise ValueError("expected '(' or final float: " + u[:50])

    return _rec(s)


def format_float_nsig(x: float, n_sig: int) -> str:
    """约 n 位十进制有效数字（四舍五入）。"""
    if n_sig < 1:
        n_sig = 1
    if n_sig > 17:
        n_sig = 17
    return f"{x:.{n_sig}g}"


def _sollya_horner_prettify_one_line(s: str) -> str:
    """
    在 ' + x * (' 前换行，便于阅读；不在数字词法内部切断。
    """
    t = (s or "").strip()
    if not t:
        return ""
    t = re.sub(r"\s+", " ", t)
    t = t.replace(" + x * (", "\n+ x * (")
    t = t.replace(" +x*(", "\n+ x * (")
    return t


def build_sollya_run_multiline_log(
    sollya_stdout: str,
    sollya_stderr: str,
    exit_code: int,
    horner_coeffs: Optional[List[float]],
    horner_parse_error: str,
    line_width: int = 100,
) -> str:
    """
    多行、人类可读：含 stderr、分行后的 sollya stdout、全精度 Horner 链、解析错误信息。
    """
    lines: List[str] = []
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines.append(f"# sollya fpminimax run ({now})")
    lines.append(f"sollya exit code: {exit_code}")
    lines.append("")
    se = (sollya_stderr or "").rstrip()
    lines.append("--- sollya stderr ---")
    lines.append(se if se else "(empty)")
    lines.append("")
    raw = (sollya_stdout or "").rstrip()
    if "\n" in raw:
        lines.append("--- sollya stdout (multiple lines) ---")
        for ln in raw.splitlines():
            lines.append(ln)
    else:
        lines.append("--- sollya stdout 原始（在 ` + x * (` 前换行；过长段再按列宽续行）---")
        w = max(40, int(line_width))
        pretty = _sollya_horner_prettify_one_line(raw)
        if not pretty:
            lines.append("(empty)")
        else:
            for seg in pretty.splitlines():
                if len(seg) <= w:
                    lines.append(seg)
                else:
                    lines.append(
                        textwrap.fill(seg, width=w, break_long_words=False, break_on_hyphens=False)
                    )
    lines.append("")
    lines.append("--- Horner 链常数 c0, c1, ... (与 Sollya 行一致, repr 全精度) ---")
    if horner_parse_error:
        lines.append(f"(parse error: {horner_parse_error})")
    if horner_coeffs is not None:
        for i, c in enumerate(horner_coeffs):
            lines.append(f"c[{i:2d}] = {c!r}")
    else:
        lines.append("(未解析; 请见上方原始行)")
    lines.append("")
    return "\n".join(lines) + "\n"


def run_sollya_file(script: str, sollya_bin: str = "sollya", timeout: float = 120.0):
    """
    Run sollya with script content. Returns (returncode, stdout, stderr).
    """
    import os
    import subprocess
    import tempfile

    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".sollya", delete=False, encoding="utf-8", newline="\n"
    ) as tf:
        tf.write(script)
        path = tf.name
    try:
        proc = subprocess.run(
            [sollya_bin, path],
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
        return proc.returncode, proc.stdout, proc.stderr
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass


def build_sollya_report_file(
    source_lines_commentable: str,
    exit_code: int,
    stdout: str,
    stderr: str,
) -> str:
    """
    整份文件以 // 注释为主：先附源码（可手工去掉 // 以再跑），再附运行输出。
    """
    lines: list[str] = []
    lines.append(
        "// Auto-generated: Sollya fpminimax report (see parms_atan2.py --run-sollya)"
    )
    lines.append("// === source (uncomment to run in sollya) ===")
    for raw in source_lines_commentable.strip().splitlines():
        if raw.strip().startswith("//"):
            lines.append(raw)
        else:
            lines.append("// " + raw)
    lines.append("// === sollya exit code: %d ===" % exit_code)
    lines.append("// === sollya stdout (polynomial) ===")
    for raw in (stdout or "").rstrip().splitlines():
        lines.append("// " + raw)
    if stderr and stderr.strip():
        lines.append("// === sollya stderr ===")
        for raw in stderr.rstrip().splitlines():
            lines.append("// " + raw)
    return "\n".join(lines) + "\n"
