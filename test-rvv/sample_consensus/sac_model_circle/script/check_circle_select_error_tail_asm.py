#!/usr/bin/env python3
"""Check selectWithinDistance RVV symbols for full error-tail instructions.

这个脚本是 Phase 080/090 的 asm attribution（反汇编归属）验收 gate。它检查
候选 helper 或 production helper 符号内部是否出现 `vcompress.vm`、`vfsqrt.v`、
`vfwcvt.f.f.v` 和 `vse64.v`。通过只说明机器码包含目标 RVV 指令；性能收益仍由
板卡 repeated benchmark（重复性能测试）和 Evidence Doctor（证据体检）决定。
"""

from __future__ import annotations

import argparse
from pathlib import Path


REQUIRED_INSTRUCTIONS = ("vcompress.vm", "vfsqrt.v", "vfwcvt.f.f.v", "vse64.v")


def symbol_body(asm_path: Path, symbol_hint: str) -> list[str]:
    lines = asm_path.read_text(encoding="utf-8", errors="replace").splitlines()
    body: list[str] = []
    in_symbol = False
    for line in lines:
        stripped = line.strip()
        if stripped.endswith(">:") and " <" in stripped:
            if in_symbol:
                break
            in_symbol = symbol_hint in stripped
            if in_symbol:
                body.append(line)
            continue
        if in_symbol:
            body.append(line)
    return body


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--asm",
        type=Path,
        default=Path("build/asm/riscv/bench_sac_model_circle_rvv.full.asm"),
    )
    parser.add_argument(
        "--symbol",
        default="selectWithinDistanceRVV",
        help="Demangled symbol substring to inspect.",
    )
    args = parser.parse_args()

    body = symbol_body(args.asm, args.symbol)
    if not body:
        raise SystemExit(f"symbol not found: {args.symbol}")

    missing = [
        instruction
        for instruction in REQUIRED_INSTRUCTIONS
        if not any(instruction in line for line in body)
    ]
    if missing:
        raise SystemExit(
            f"missing instruction(s) in {args.symbol}: " + ", ".join(missing)
        )
    print(
        f"{args.symbol} asm gate passed for: "
        + ", ".join(REQUIRED_INSTRUCTIONS)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
