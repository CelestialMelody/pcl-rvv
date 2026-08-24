#!/usr/bin/env python3
"""Generate DON repeated board summary and Evidence Doctor manifest.

本脚本读取 repeated board target 抓回的多轮 `analyze_bench_compare.log`
和 bench raw log（原始日志），生成 summary（摘要）以及 Evidence Doctor
（证据体检）可读的 manifest。默认服务 DON topic 的 `don_normal_pair` case，
也可通过参数切换到 production direct（真实生产路径证据）case。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


COMPARE_RVV_RE = re.compile(r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9.]+)\s*us\s*\|")
CHECKSUM_RE = re.compile(r"^\s*Checksum:\s*(?P<checksum>\d+)\s*$", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)\s*$", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup Iterations:\s*(?P<warmup>\d+)\s*$", re.MULTILINE)


def compare_std_re(case_label: str) -> re.Pattern[str]:
    return re.compile(
        rf"^{re.escape(case_label)},points=(?P<points>\d+)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9.]+)\s*us\s*\|"
    )


def bench_row_re(case_label: str) -> re.Pattern[str]:
    return re.compile(
        rf"^{re.escape(case_label)},points=(?P<points>\d+)\s*:\s*(?P<avg>[0-9.]+)\s+us/iter\s*$",
        re.MULTILINE,
    )


def parse_compare(path: Path, case_label: str) -> tuple[int, float, float, float]:
    current_points: int | None = None
    std_us: float | None = None
    std_re = compare_std_re(case_label)
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := std_re.match(line):
            current_points = int(match.group("points"))
            std_us = float(match.group("avg"))
            continue
        if current_points is not None and std_us is not None and (match := COMPARE_RVV_RE.match(line)):
            rvv_us = float(match.group("avg"))
            if rvv_us <= 0.0:
                raise SystemExit(f"invalid RVV timing in {path}")
            return current_points, std_us, rvv_us, std_us / rvv_us
    raise SystemExit(f"cannot parse DON compare log for {case_label}: {path}")


def parse_raw_log(path: Path, case_label: str) -> dict[str, int | float | str | None]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    row = bench_row_re(case_label).search(text)
    checksum = CHECKSUM_RE.search(text[row.end() :]) if row else None
    iterations = ITER_RE.search(text)
    warmup = WARMUP_RE.search(text)
    return {
        "points": int(row.group("points")) if row else None,
        "avg_us": float(row.group("avg")) if row else None,
        "checksum": checksum.group("checksum") if checksum else None,
        "iterations": int(iterations.group("iterations")) if iterations else None,
        "warmup_iterations": int(warmup.group("warmup")) if warmup else None,
    }


def bucket(values: list[float]) -> str:
    if not values:
        return "missing"
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.20 and below_one == 0:
        return "positive"
    if median >= 1.05 and below_one <= max(1, len(values) // 5):
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def unique_or_mixed(values: list[object | None]) -> object | str | None:
    present = [value for value in values if value is not None]
    if not present:
        return None
    first = present[0]
    if all(value == first for value in present):
        return first
    return "mixed"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--case-label", action="append", default=None)
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--ab-boundary", default="test helper")
    parser.add_argument("--wrapper", default="computeDoNRVV")
    parser.add_argument("--timer-boundary", default="helper_only_no_setup")
    parser.add_argument("--case-kind", default="component_ablation")
    parser.add_argument("--title", default="DON diagnostic repeated board summary")
    args = parser.parse_args()

    case_labels = args.case_label or ["don_normal_pair"]
    run_dirs = sorted(path for path in args.input_dir.glob("run-*") if path.is_dir())
    if not run_dirs:
        raise SystemExit(f"no repeated run directories found under {args.input_dir}")

    asm_lines = 0
    if args.asm.exists():
        asm_lines = sum(
            1
            for line in args.asm.read_text(encoding="utf-8", errors="ignore").splitlines()
            if any(op in line for op in ("vlse32", "vfsub", "vfmul", "vmerge", "vfmacc", "vfsqrt", "vsse32"))
        )

    rows_by_case: dict[str, list[dict[str, object]]] = {}
    for case_label in case_labels:
        rows: list[dict[str, object]] = []
        for run_dir in run_dirs:
            points, std_us, rvv_us, speedup = parse_compare(run_dir / "analyze_bench_compare.log", case_label)
            std_raw = parse_raw_log(run_dir / "run_bench_std.log", case_label)
            rvv_raw = parse_raw_log(run_dir / "run_bench_rvv.log", case_label)
            rows.append(
                {
                    "run": run_dir.name,
                    "points": points,
                    "std_us": std_us,
                    "rvv_us": rvv_us,
                    "speedup": speedup,
                    "std_checksum": std_raw.get("checksum"),
                    "rvv_checksum": rvv_raw.get("checksum"),
                    "iterations": std_raw.get("iterations") or rvv_raw.get("iterations"),
                    "warmup_iterations": std_raw.get("warmup_iterations") or rvv_raw.get("warmup_iterations"),
                }
            )
        rows_by_case[case_label] = rows

    all_rows = [row for rows in rows_by_case.values() for row in rows]
    decisions = {
        case_label: bucket([float(row["speedup"]) for row in rows])
        for case_label, rows in rows_by_case.items()
    }
    overall_decision = (
        next(iter(decisions.values()))
        if len(set(decisions.values())) == 1
        else "mixed"
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(run_dirs)}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{args.ab_boundary}`",
        f"- timer_boundary: `{args.timer_boundary}`",
        "- decision_bucket: `{}`".format(overall_decision),
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for case_label, rows in rows_by_case.items():
        values = [float(row["speedup"]) for row in rows]
        below_one = sum(1 for value in values if value < 1.0)
        lines.append(
            "| `{},points={}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
                case_label,
                rows[0]["points"],
                len(rows),
                statistics.median(values),
                min(values),
                max(values),
                percentile(values, 0.10),
                percentile(values, 0.90),
                below_one,
                len(rows),
                ", ".join(f"{value:.3f}x" for value in values),
            )
        )
    lines += ["", "## 每轮明细"]
    for case_label, rows in rows_by_case.items():
        lines += [
            "",
            f"### `{case_label}`",
            "",
            "| run | Std us/iter | RVV us/iter | speedup | checksum match |",
            "| --- | ---: | ---: | ---: | --- |",
        ]
        for row in rows:
            lines.append(
                "| `{run}` | {std:.4f} | {rvv:.4f} | {speed:.3f}x | {match} |".format(
                    run=row["run"],
                    std=float(row["std_us"]),
                    rvv=float(row["rvv_us"]),
                    speed=float(row["speedup"]),
                    match="yes" if row["std_checksum"] == row["rvv_checksum"] else "no",
                )
            )
    lines += [
        "",
        "## Evidence boundary",
        "",
        "本摘要的证据角色由 `evidence_role` 和 `A/B boundary` 决定；QEMU timing（QEMU 计时）不参与这里的性能结论。",
    ]
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    iterations = unique_or_mixed([row["iterations"] for row in all_rows])
    warmup = unique_or_mixed([row["warmup_iterations"] for row in all_rows])
    common = {
        "boundary": args.ab_boundary.replace(" ", "_"),
        "wrapper": args.wrapper,
        "row_source": "ordered_normal_cloud",
        "solve": False,
        "checksum_policy": "output_normals_and_curvature",
        "timer_boundary": args.timer_boundary,
        "gate": "same_input_size",
        "mask": "finite_normal_output_zeroing",
        "reduction": "not_applicable",
    }
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": args.evidence_role,
            "summary_path": str(args.output),
            "label": args.run_label,
            "analysis_script": "test-rvv/features/don/script/generate_don_repeated_summary.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": iterations,
                "warmup_iterations": warmup,
                "run_count": len(run_dirs),
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {"strict_ab": False},
        "comparisons": [],
    }
    for case_label, rows in rows_by_case.items():
        values = [float(row["speedup"]) for row in rows]
        std_values = [float(row["std_us"]) for row in rows]
        rvv_values = [float(row["rvv_us"]) for row in rows]
        checksum_std = unique_or_mixed([row["std_checksum"] for row in rows])
        checksum_rvv = unique_or_mixed([row["rvv_checksum"] for row in rows])
        manifest["comparisons"].append(
            {
                "name": f"{case_label} points={rows[0]['points']}",
                "group": f"{case_label}-repeated",
                "evidence_role": args.evidence_role,
                "case_kind": args.case_kind,
                "point_type": "pcl::Normal",
                "size": rows[0]["points"],
                "run_count": len(rows),
                "ba_values": values,
                "decision_bucket": decisions[case_label],
                "baseline": {
                    "label": "Std build",
                    **common,
                    "checksum": checksum_std,
                    "asm_boundary": "std_build_no_rvv",
                    "rvv_instr_count": 0,
                    "std_ms": statistics.median(std_values) * 0.001,
                    "rvv_ms": statistics.median(std_values) * 0.001,
                },
                "candidate": {
                    "label": "RVV build",
                    **common,
                    "checksum": checksum_rvv,
                    "asm_boundary": str(args.asm),
                    "rvv_instr_count": asm_lines,
                    "std_ms": statistics.median(rvv_values) * 0.001,
                    "rvv_ms": statistics.median(rvv_values) * 0.001,
                },
            }
        )
    (args.output.parent / "evidence_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
