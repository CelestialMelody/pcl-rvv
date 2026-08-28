#!/usr/bin/env python3
"""Check Circle3D RVV asm attribution.

这个脚本有两个模式。`component` 检查 Phase 000 test-only helper（仅测试使用的
helper）；`production-select` 检查 Phase 010 production helper（生产 helper）。
通过只说明目标符号里能归属 RVV 指令；性能收益仍必须由 board（板卡） repeated
benchmark 和 Evidence Doctor（证据体检）决定。
"""

from __future__ import annotations

import argparse
from pathlib import Path


COMPONENT_REQUIRED = {
    "countWithinDistanceProjectionRVV": ("vluxei32.v", "vfmacc", "vfnmsac", "vfsqrt.v"),
    "selectWithinDistanceProjectionRVV": (
        "vluxei32.v",
        "vfmacc",
        "vfnmsac",
        "vfsqrt.v",
        "vcompress.vm",
    ),
}

PRODUCTION_SELECT_REQUIRED = {
    "selectWithinDistanceRVVCircle3D": (
        "vfmacc",
        "vfnmsac",
        "vfsqrt.v",
        "vcompress.vm",
    ),
}

PRODUCTION_SELECT_LOAD_CHOICES = ("vluxei32.v", "vluxseg3ei32.v")


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
        default=Path("build/asm/riscv/bench_sac_model_circle3d_rvv.full.asm"),
    )
    parser.add_argument(
        "--mode",
        choices=("component", "production-select"),
        default="component",
    )
    args = parser.parse_args()

    required = COMPONENT_REQUIRED
    if args.mode == "production-select":
        required = PRODUCTION_SELECT_REQUIRED

    failures: list[str] = []
    for symbol, instructions in required.items():
        body = symbol_body(args.asm, symbol)
        if not body:
            failures.append(f"{symbol}: symbol not found")
            continue
        if args.mode == "production-select" and not any(
            instruction in line for instruction in PRODUCTION_SELECT_LOAD_CHOICES for line in body
        ):
            failures.append(
                f"{symbol}: missing indexed xyz load ({' or '.join(PRODUCTION_SELECT_LOAD_CHOICES)})"
            )
        for instruction in instructions:
            if not any(instruction in line for line in body):
                failures.append(f"{symbol}: missing {instruction}")

    if failures:
        raise SystemExit("\n".join(failures))
    print(f"circle3d projection asm gate passed ({args.mode})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
