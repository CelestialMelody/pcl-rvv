#!/usr/bin/env python3
"""Check circle RVV helper symbols for identity strided-load instructions.

这个脚本是 Phase 040 的 asm attribution（反汇编归属）验收 gate。
它只检查 `selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 符号内部是否
出现 `vlse32.v`。通过只说明 identity-index fast path（恒等索引快速路径）
已经进入目标 helper 的机器码；性能收益仍必须用板卡 RVV-vs-RVV A/B 证明。
"""

from __future__ import annotations

import argparse
from pathlib import Path


SYMBOLS = ("countWithinDistanceRVV", "selectWithinDistanceRVV")


def symbols_with_strided_load(asm_path: Path) -> dict[str, bool]:
    text = asm_path.read_text(encoding="utf-8", errors="replace").splitlines()
    found = {symbol: False for symbol in SYMBOLS}
    current_symbol = ""
    for line in text:
        stripped = line.strip()
        if stripped.endswith(">:") and " <" in stripped:
            current_symbol = stripped
            continue
        for symbol in SYMBOLS:
            if symbol in current_symbol and "vlse32.v" in line:
                found[symbol] = True
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--asm",
        type=Path,
        default=Path("build/asm/riscv/bench_sac_model_circle_rvv.full.asm"),
    )
    args = parser.parse_args()

    found = symbols_with_strided_load(args.asm)
    missing = [symbol for symbol, ok in found.items() if not ok]
    if missing:
        raise SystemExit(
            "missing identity strided load (vlse32.v) in RVV helper symbol(s): "
            + ", ".join(missing)
        )
    print("identity strided load asm gate passed for: " + ", ".join(SYMBOLS))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
