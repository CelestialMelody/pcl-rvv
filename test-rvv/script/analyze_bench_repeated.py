#!/usr/bin/env python3
"""
Aggregate repeated board benchmark compare logs.

This script reads one or more analyze_bench_compare.py outputs and reports
per-case speedup count/median/min/max/p10/p90. It recomputes speedup from
the raw Std/RVV ms columns instead of trusting the rounded speedup printed in
the compare log. It is intentionally log-only: it does not run benchmarks and
does not decide production readiness.
"""

from __future__ import annotations

import argparse
import re
import statistics
from pathlib import Path


STD_ROW_RE = re.compile(
    r"^(?P<name>.+?)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)
RVV_ROW_RE = re.compile(
    r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|.*"
    r"\[\s*(?P<speedup>[0-9]+(?:\.[0-9]+)?)x\s*\]"
)
DEVICE_RE = re.compile(r"^\s*Device\s*:\s*(.+?)\s*$")
ITERATIONS_RE = re.compile(r"^\s*Iterations\s*:\s*(\d+)\b")


def parse_log(path: Path) -> tuple[dict[str, float], dict[str, str]]:
    cases: dict[str, float] = {}
    context: dict[str, str] = {}
    current_case: str | None = None
    current_std_avg: float | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := DEVICE_RE.match(line):
            context["device"] = match.group(1)
        if match := ITERATIONS_RE.match(line):
            context["iterations"] = match.group(1)
        if match := STD_ROW_RE.match(line):
            current_case = match.group("name").strip()
            current_std_avg = float(match.group("avg"))
            continue
        if current_case and (match := RVV_ROW_RE.match(line)):
            rvv_avg = float(match.group("avg"))
            if current_std_avg is not None and rvv_avg > 0.0:
                cases[current_case] = current_std_avg / rvv_avg
            current_case = None
            current_std_avg = None
    return cases, context


def format_values(values: list[float]) -> str:
    return ", ".join(f"{value:.2f}x" for value in values)


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("percentile() requires at least one value")
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Aggregate repeated Std/RVV benchmark compare logs."
    )
    parser.add_argument("logs", nargs="+", type=Path, help="analyze_bench_compare.log files")
    parser.add_argument(
        "--case-filter",
        default="",
        help="Only include benchmark names containing this substring.",
    )
    args = parser.parse_args()

    grouped: dict[str, list[float]] = {}
    contexts: list[str] = []
    for path in args.logs:
        cases, context = parse_log(path)
        label = str(path)
        if context:
            device = context.get("device", "?")
            iterations = context.get("iterations", "?")
            label = f"{path} (device={device}, iterations={iterations})"
        contexts.append(label)
        for name, speedup in cases.items():
            if args.case_filter and args.case_filter not in name:
                continue
            grouped.setdefault(name, []).append(speedup)

    print("# Repeated Benchmark Speedup Summary")
    print()
    print("Input logs:")
    for context in contexts:
        print(f"- {context}")
    print()
    print("| Benchmark Item | runs | median | min | max | p10 | p90 | values |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |")
    for name in sorted(grouped):
        values = grouped[name]
        median = statistics.median(values)
        p10 = percentile(values, 0.10)
        p90 = percentile(values, 0.90)
        print(
            f"| {name} | {len(values)} | {median:.2f}x | "
            f"{min(values):.2f}x | {max(values):.2f}x | "
            f"{p10:.2f}x | {p90:.2f}x | {format_values(values)} |"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
