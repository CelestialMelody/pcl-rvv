#!/usr/bin/env python3
"""Check line RVV helper symbols for identity strided-load attribution.

这个脚本服务 Phase 070 的 asm attribution（反汇编归属）验收。默认模式检查
production RVV helper 内同时存在 `vlse32.v` 或 `vlsseg3e32.v` identity fast
path（恒等索引快速路径）和 `vlux*` gather fallback（离散加载回退）；
`--expect-gather-only` 用来检查测试专用 baseline binary（基线二进制）
没有混入 identity strided load。
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SYMBOLS = ("countWithinDistanceRVV", "selectWithinDistanceRVV", "getDistancesToModelRVV")
SYMBOL_HEADER_RE = re.compile(r"^[0-9a-fA-F]+ <(?P<symbol>.+)>:$")


def helper_body(asm: str, helper: str) -> str:
    match = re.search(rf"^[0-9a-fA-F]+ <.*{re.escape(helper)}.*>:\n", asm, re.M)
    if not match:
        return ""
    start = match.end()
    next_symbol = re.search(r"^[0-9a-fA-F]+ <[^>]+>:\n", asm[start:], re.M)
    end = start + next_symbol.start() if next_symbol else len(asm)
    return asm[start:end]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--asm",
        type=Path,
        default=Path("build/asm/riscv/bench_sac_model_line_rvv.full.asm"),
    )
    parser.add_argument(
        "--expect-gather-only",
        action="store_true",
        help="expect helper symbols to keep vlux gather and not contain vlse strided loads",
    )
    args = parser.parse_args()

    asm = args.asm.read_text(encoding="utf-8", errors="replace")
    failures: list[str] = []

    for symbol in SYMBOLS:
        body = helper_body(asm, symbol)
        if not body:
            failures.append(f"missing helper symbol: {symbol}")
            continue
        has_vlux = "vlux" in body
        has_vlse = "vlse32.v" in body or "vlsseg3e32.v" in body
        if not has_vlux:
            failures.append(f"{symbol}: missing vlux gather fallback")
        if args.expect_gather_only:
            if has_vlse:
                failures.append(f"{symbol}: gather-only baseline unexpectedly contains strided load")
        elif not has_vlse:
            failures.append(f"{symbol}: missing identity strided load")

    if failures:
        print("line identity strided asm gate: FAIL")
        for failure in failures:
            print(f"- {failure}")
        return 1

    mode = "gather-only baseline" if args.expect_gather_only else "identity-strided candidate"
    print(f"line identity strided asm gate: PASS ({mode})")
    for symbol in SYMBOLS:
        print(f"- {symbol}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
