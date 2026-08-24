#!/usr/bin/env python3
"""Summarize repeated color_coding board bench logs.

本脚本读取 run*/run_bench_std.log 和 run*/run_bench_rvv.log，生成
summary（摘要）Markdown。raw log 默认只留在 topic-local log/board 下。
"""

from __future__ import annotations

import argparse
import re
import statistics
from pathlib import Path

CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+),\s*(?:warmup|Warmup):\s*(?P<warmup>[0-9]+)")


def parse_log(path: Path) -> dict:
    cases: dict[str, dict] = {}
    iterations = None
    warmup = None
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    index = 0
    while index < len(lines):
        if iterations is None:
            match = ITER_RE.search(lines[index])
            if match:
                iterations = int(match.group("iterations"))
                warmup = int(match.group("warmup"))
        match = CASE_RE.match(lines[index])
        if match and index + 1 < len(lines):
            total = TOTAL_RE.search(lines[index + 1])
            if total:
                cases[match.group("name")] = {
                    "avg_ms": float(match.group("avg")),
                    "checksum": total.group("checksum"),
                }
                index += 1
        index += 1
    if iterations is None or warmup is None:
        raise ValueError(f"missing iterations/warmup in {path}")
    return {"iterations": iterations, "warmup": warmup, "cases": cases}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeat-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--title", default="color_coding repeated board summary")
    parser.add_argument("--evidence-role", default="component_ablation")
    args = parser.parse_args()

    pairs = []
    for run_dir in sorted(args.repeat_dir.glob("run*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            pairs.append((parse_log(std_log), parse_log(rvv_log)))
    if not pairs:
        raise ValueError(f"no repeated logs found under {args.repeat_dir}")

    case_names = set(pairs[0][0]["cases"]) & set(pairs[0][1]["cases"])
    for std, rvv in pairs[1:]:
        case_names &= set(std["cases"]) & set(rvv["cases"])

    first_std = pairs[0][0]
    rows = [
        f"# {args.title}",
        "",
        f"- evidence_role: {args.evidence_role}",
        f"- iterations: {first_std['iterations']}",
        f"- warmup_iterations: {first_std['warmup']}",
        f"- run_count: {len(pairs)}",
        "- device: Milkv-Jupiter",
        "",
        "| case | mean speedup | median | min | max | mean Std ms | mean RVV ms |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for name in sorted(case_names):
        std_values = [std["cases"][name]["avg_ms"] for std, _ in pairs]
        rvv_values = [rvv["cases"][name]["avg_ms"] for _, rvv in pairs]
        speedups = [std / rvv for std, rvv in zip(std_values, rvv_values)]
        rows.append(
            f"| {name} | {statistics.fmean(speedups):.4f}x | "
            f"{statistics.median(speedups):.4f}x | {min(speedups):.4f}x | "
            f"{max(speedups):.4f}x | {statistics.fmean(std_values):.4f} | "
            f"{statistics.fmean(rvv_values):.4f} |"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(rows) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
