#!/usr/bin/env python3
"""Generate image_depth repeated board summary from compare logs.

本脚本只解析 image_depth topic 的 repeated board compare（重复板卡对比）
日志，把 Std/RVV speedup（标量 / RVV 加速比）汇总成稳定 Markdown
summary。它不读取 raw bench timing 以外的生产状态，因此输出仍是
production-shaped diagnostic（生产形态诊断）证据。
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from statistics import median


DEVICE_RE = re.compile(r"Device\s*:\s*(.+)")
ITER_RE = re.compile(r"Iterations:\s*(\d+)")
STD_ROW_RE = re.compile(
    r"^(?P<name>.+?)\s*\|\s*Std\s*\|\s*(?P<std>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)
RVV_ROW_RE = re.compile(r"^\s*\|\s*RVV\s*\|\s*(?P<rvv>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|")


def parse_compare_log(path: Path) -> tuple[str, int | None, dict[str, float]]:
    device = "unknown"
    iterations: int | None = None
    values: dict[str, float] = {}
    current_case: str | None = None
    current_std_ms: float | None = None

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := DEVICE_RE.search(line):
            device = match.group(1).strip()
            continue
        if match := ITER_RE.search(line):
            iterations = int(match.group(1))
            continue
        if match := STD_ROW_RE.match(line):
            current_case = match.group("name").strip()
            current_std_ms = float(match.group("std"))
            continue
        if current_case and (match := RVV_ROW_RE.match(line)):
            rvv_ms = float(match.group("rvv"))
            if current_std_ms is not None and rvv_ms > 0.0:
                values[current_case] = current_std_ms / rvv_ms
            current_case = None
            current_std_ms = None

    return device, iterations, values


def percentile(values: list[float], q: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return 0.0
    position = (len(ordered) - 1) * q
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    fraction = position - low
    return ordered[low] * (1.0 - fraction) + ordered[high] * fraction


def fmt(value: float) -> str:
    return f"{value:.2f}x"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--include-label",
        action="append",
        default=None,
    )
    args = parser.parse_args()
    include_labels = args.include_label or [
        "depth_full_640x480",
        "depth_full_padded_640x480",
        "disparity_full_640x480",
    ]

    logs = sorted(args.input_dir.glob("analyze_bench_compare_*.log"))
    values: dict[str, list[float]] = {label: [] for label in include_labels}
    input_lines: list[str] = []

    for path in logs:
        device, iterations, parsed = parse_compare_log(path)
        iter_text = "unknown" if iterations is None else str(iterations)
        input_lines.append(
            f"- {path.as_posix()} (device={device}, iterations={iter_text})"
        )
        for label in include_labels:
            if label in parsed:
                values[label].append(parsed[label])

    lines = [
        "# Repeated Benchmark Speedup Summary",
        "",
        "Input logs:",
        *input_lines,
        "",
        "| Benchmark Item | runs | median | min | max | p10 | p90 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for label in include_labels:
        series = values[label]
        if not series:
            lines.append(f"| {label} | 0 | n/a | n/a | n/a | n/a | n/a | missing |")
            continue
        lines.append(
            f"| {label} | {len(series)} | {fmt(median(series))} | {fmt(min(series))} | "
            f"{fmt(max(series))} | {fmt(percentile(series, 0.10))} | "
            f"{fmt(percentile(series, 0.90))} | "
            f"{', '.join(fmt(value) for value in series)} |"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
