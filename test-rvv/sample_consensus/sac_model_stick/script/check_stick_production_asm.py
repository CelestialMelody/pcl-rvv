#!/usr/bin/env python3
"""Check production RVV helper symbols for sac_model_stick.

这个脚本是 Phase 080 的 asm attribution（反汇编归属）验收 gate。
它只证明 bench 二进制里的真实 production helper（生产 helper）包含 RVV
指令；性能收益必须继续由 board（板卡）repeated evidence 证明。
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SYMBOLS = (
    "countWithinDistanceRVV",
    "selectWithinDistanceRVV",
    "getDistancesToModelRVV",
)
RVV_INSTRUCTION_RE = re.compile(r"\s+v[a-z0-9]+(?:\.[a-z0-9]+)*\s+")
VECTOR_WIDEN_CONVERT_RE = re.compile(r"\bvfwcvt\.f\.f\.v\b")
VECTOR_DOUBLE_STORE_RE = re.compile(r"\bvse64\.v\b")
GETDIST_SOURCE_SHAPE_RE = re.compile(
    r"getDistancesToModelRVV\s*\([^{}]*\)\s*const\s*\{(?P<body>.*?)\n\}",
    re.DOTALL,
)


def symbol_instruction_lines(asm_path: Path) -> dict[str, list[str]]:
    text = asm_path.read_text(encoding="utf-8", errors="replace").splitlines()
    lines = {symbol: [] for symbol in SYMBOLS}
    current_symbol = ""
    for line in text:
        stripped = line.strip()
        if stripped.endswith(">:") and " <" in stripped:
            current_symbol = stripped
            continue
        if not RVV_INSTRUCTION_RE.search(line):
            continue
        for symbol in SYMBOLS:
            if symbol in current_symbol:
                lines[symbol].append(line)
    return lines


def check_getdistances_source_shape(source_path: Path) -> list[str]:
    text = source_path.read_text(encoding="utf-8", errors="replace")
    match = GETDIST_SOURCE_SHAPE_RE.search(text)
    if not match:
        return ["could not locate getDistancesToModelRVV source body"]

    body = match.group("body")
    problems = []
    if "staged_sqr" in body or "staged_dist" in body:
        problems.append("getDistancesToModelRVV still stages sqr/dist through float buffers")
    if re.search(r"for\s*\(\s*std::size_t\s+lane\s*=\s*0\s*;\s*lane\s*<\s*vl", body):
        problems.append("getDistancesToModelRVV still has a scalar lane writeback loop")
    if "__riscv_vmerge_vvm_f32m2" not in body:
        problems.append("getDistancesToModelRVV does not merge penalty distances with an RVV mask")
    if "__riscv_vfwcvt_f_f_v_f64m4" not in body:
        problems.append("getDistancesToModelRVV does not widen float distances to double in RVV")
    if "__riscv_vse64_v_f64m4" not in body:
        problems.append("getDistancesToModelRVV does not write dense distances with an RVV double store")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--asm",
        type=Path,
        default=Path("build/asm/riscv/bench_sac_model_stick_rvv.full.asm"),
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=Path(
            "../../../sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp"
        ),
    )
    args = parser.parse_args()

    source_problems = check_getdistances_source_shape(args.source)
    if source_problems:
        raise SystemExit(
            "getDistancesToModelRVV source shape is not vector-writeback clean: "
            + "; ".join(source_problems)
        )

    lines = symbol_instruction_lines(args.asm)
    missing = [symbol for symbol, symbol_lines in lines.items() if not symbol_lines]
    if missing:
        raise SystemExit(
            "missing RVV instructions in production helper symbol(s): "
            + ", ".join(missing)
        )
    getdist_lines = "\n".join(lines["getDistancesToModelRVV"])
    getdist_missing = []
    if not VECTOR_WIDEN_CONVERT_RE.search(getdist_lines):
        getdist_missing.append("vfwcvt.f.f.v")
    if not VECTOR_DOUBLE_STORE_RE.search(getdist_lines):
        getdist_missing.append("vse64.v")
    if getdist_missing:
        raise SystemExit(
            "missing vector double writeback in getDistancesToModelRVV: "
            + ", ".join(getdist_missing)
        )
    print("production RVV asm gate passed for: " + ", ".join(SYMBOLS))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
