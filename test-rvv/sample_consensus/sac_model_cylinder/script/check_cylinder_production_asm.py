#!/usr/bin/env python3
"""Check sac_model_cylinder production RVV asm attribution.

这个脚本只确认 production helper（生产辅助函数）或可归属符号中出现
关键 RVV 指令。它不做性能判断；性能结论仍来自 board（板卡）repeated
bench 和 Evidence Doctor（证据体检）。
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SYMBOL_HEADER_RE = re.compile(r"^[0-9a-fA-F]+ <(?P<symbol>.+)>:$")
RVV_RE = re.compile(r"\s+v[a-z0-9]+(?:\.[a-z0-9]+)*\s+")


def count_symbol_instructions(path: Path, symbol_hint: str) -> tuple[int, set[str]]:
    in_symbol = False
    count = 0
    mnemonics: set[str] = set()
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        header = SYMBOL_HEADER_RE.match(line)
        if header:
            in_symbol = symbol_hint in header.group("symbol")
            continue
        if not in_symbol:
            continue
        if not RVV_RE.search(line):
            continue
        count += 1
        parts = line.split()
        if len(parts) >= 3:
            mnemonics.add(parts[2])
    return count, mnemonics


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--asm", type=Path, required=True)
    args = parser.parse_args()

    required = {
        "countWithinDistanceRVVCylinder": {"vcpop.m", "vfsqrt.v"},
        "selectWithinDistanceRVVCylinder": {"vcompress.vm", "vse64.v"},
        "getDistancesToModelRVVCylinder": {"vfsqrt.v", "vfwcvt.f.f.v", "vse64.v"},
    }
    ok = True
    for symbol, expected in required.items():
        count, mnemonics = count_symbol_instructions(args.asm, symbol)
        missing = sorted(expected - mnemonics)
        print(f"{symbol}: rvv_lines={count} mnemonics={','.join(sorted(mnemonics))}")
        if count == 0 or missing:
            print(f"[ERROR] {symbol} missing expected RVV mnemonics: {', '.join(missing)}")
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
