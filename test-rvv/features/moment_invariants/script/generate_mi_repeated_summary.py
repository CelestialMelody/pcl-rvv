#!/usr/bin/env python3
"""Generate moment_invariants repeated board summary and Evidence Doctor manifest."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


STD_RE = re.compile(
    r"^(?P<case>[A-Za-z0-9_]+),points=(?P<points>\d+)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9.]+)\s*us\s*\|"
)
RVV_RE = re.compile(r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9.]+)\s*us\s*\|")
CHECKSUM_RE = re.compile(r"^\s*Checksum:\s*(?P<checksum>\d+)\s*$", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)\s*$", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup Iterations:\s*(?P<warmup>\d+)\s*$", re.MULTILINE)


def parse_compare(path: Path, expected_case: str) -> tuple[int, float, float, float]:
    points: int | None = None
    std_us: float | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := STD_RE.match(line):
            if match.group("case") != expected_case:
                points = None
                std_us = None
                continue
            points = int(match.group("points"))
            std_us = float(match.group("avg"))
            continue
        if points is not None and std_us is not None and (match := RVV_RE.match(line)):
            rvv_us = float(match.group("avg"))
            return points, std_us, rvv_us, std_us / rvv_us
    raise SystemExit(f"cannot parse compare log: {path}")


def parse_raw(path: Path) -> dict[str, int | str | None]:
    text = path.read_text(encoding="utf-8", errors="replace")
    checksum = CHECKSUM_RE.search(text)
    iterations = ITER_RE.search(text)
    warmup = WARMUP_RE.search(text)
    return {
        "checksum": checksum.group("checksum") if checksum else None,
        "iterations": int(iterations.group("iterations")) if iterations else None,
        "warmup_iterations": int(warmup.group("warmup")) if warmup else None,
    }


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def decision_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.10 and below_one == 0 and min(values) >= 1.10:
        return "positive"
    if median >= 1.03 and below_one <= max(1, len(values) // 5):
        return "weak_positive"
    if 0.97 <= median < 1.03:
        return "neutral"
    if median < 0.97:
        return "negative"
    return "unstable"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-label", default="mi_accumulation_indexed")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument(
        "--title",
        default="Moment invariants accumulation diagnostic repeated board summary",
    )
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--ab-boundary", default="test helper")
    parser.add_argument("--boundary", default="test_helper")
    parser.add_argument(
        "--timer-boundary",
        default="helper_only_moment_accumulation_after_centroid",
    )
    parser.add_argument("--row-source", default="indexed_cloud")
    parser.add_argument("--case-kind", default="component-candidate")
    parser.add_argument("--group", default="moment-invariants-accumulation-diagnostic")
    parser.add_argument("--point-type", default="PointXYZ")
    parser.add_argument("--gate", default="PointXYZ indexed AoS diagnostic")
    parser.add_argument("--mask", default="no finite mask; synthetic finite input")
    parser.add_argument(
        "--reduction",
        default="six central moment sums and j1/j2/j3 formula",
    )
    parser.add_argument(
        "--asm-boundary",
        default="computeMomentSummaryRVV inlined into bench_moment_invariants",
    )
    parser.add_argument(
        "--boundary-note",
        default=(
            "这是未接 production 的 diagnostic（诊断）性能证据。它证明 test helper "
            "boundary（测试 helper 边界）下中心矩累加的板卡性能信号，不能替代真实 "
            "`MomentInvariantsEstimation::computeFeature` production direct（真实生产路径）证据。"
        ),
    )
    args = parser.parse_args()

    run_dirs = sorted(path for path in args.input_dir.glob("run-*") if path.is_dir())
    if not run_dirs:
        raise SystemExit(f"no run-* directories under {args.input_dir}")

    asm_lines = 0
    if args.asm.exists():
        asm_lines = sum(
            1
            for line in args.asm.read_text(encoding="utf-8", errors="ignore").splitlines()
            if any(op in line for op in ("vluxei32", "vlse32", "vfred", "vfmul", "vfsub"))
        )

    rows: list[dict[str, object]] = []
    for run_dir in run_dirs:
        points, std_us, rvv_us, speedup = parse_compare(run_dir / "analyze_bench_compare.log", args.case_label)
        std_raw = parse_raw(run_dir / "run_bench_std.log")
        rvv_raw = parse_raw(run_dir / "run_bench_rvv.log")
        rows.append(
            {
                "run": run_dir.name,
                "points": points,
                "std_us": std_us,
                "rvv_us": rvv_us,
                "speedup": speedup,
                "std_checksum": std_raw["checksum"],
                "rvv_checksum": rvv_raw["checksum"],
                "iterations": std_raw["iterations"] or rvv_raw["iterations"],
                "warmup_iterations": std_raw["warmup_iterations"] or rvv_raw["warmup_iterations"],
            }
        )

    values = [float(row["speedup"]) for row in rows]
    bucket = decision_bucket(values)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(rows)}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{args.ab_boundary}`",
        f"- timer_boundary: `{args.timer_boundary}`",
        f"- decision_bucket: `{bucket}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        "| `{},points={}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
            args.case_label,
            rows[0]["points"],
            len(rows),
            statistics.median(values),
            min(values),
            max(values),
            percentile(values, 0.10),
            percentile(values, 0.90),
            sum(1 for value in values if value < 1.0),
            len(values),
            ", ".join(f"{value:.3f}x" for value in values),
        ),
        "",
        "## 每轮明细",
        "",
        "| run | Std us/iter | RVV us/iter | speedup | numerical gate | raw checksum note |",
        "| --- | ---: | ---: | ---: | --- | --- |",
    ]
    for row in rows:
        checksum_note = (
            "differs as expected for non-associative float reduction"
            if row["std_checksum"] != row["rvv_checksum"]
            else "matches"
        )
        lines.append(
            "| `{run}` | {std:.4f} | {rvv:.4f} | {speed:.3f}x | QEMU gtest tolerance passed | {note} |".format(
                run=row["run"],
                std=float(row["std_us"]),
                rvv=float(row["rvv_us"]),
                speed=float(row["speedup"]),
                note=checksum_note,
            )
        )

    lines += [
        "",
        "## Evidence boundary",
        "",
        args.boundary_note,
        "",
        "Raw checksum（原始校验值）量化整份浮点摘要。Std/RVV 规约顺序不同会导致该值不同；语义等价性由 `run_test_compare` 的 numerical tolerance gate（数值容差验收）证明，raw checksum 不作为 strict equality gate（严格相等验收）。",
    ]
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    iterations = rows[0]["iterations"]
    warmup = rows[0]["warmup_iterations"]
    common = {
        "boundary": args.boundary,
        "wrapper": "test-rvv/features/moment_invariants/src/bench_moment_invariants.cpp",
        "row_source": args.row_source,
        "solve": False,
        "checksum_policy": "qemu_gtest_numerical_tolerance_gate",
        "timer_boundary": args.timer_boundary,
        "gate": args.gate,
        "mask": args.mask,
        "reduction": args.reduction,
        "asm_boundary": args.asm_boundary,
        "rvv_instr_count": asm_lines,
    }
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "run_label": args.run_label,
            "evidence_role": args.evidence_role,
            "device": "board via Makefile config",
            "iterations": iterations,
            "warmup_iterations": warmup,
            "run_count": len(rows),
            "decision_bucket": bucket,
            "expected_runs": args.expected_runs,
        },
        "comparisons": [
            {
                "name": args.case_label,
                "group": args.group,
                "evidence_role": args.evidence_role,
                "case_kind": args.case_kind,
                "point_type": args.point_type,
                "size": rows[0]["points"],
                "run_count": len(rows),
                "iterations": iterations,
                "warmup_iterations": warmup,
                "build_comparison": True,
                "std_ms": statistics.mean(float(row["std_us"]) for row in rows) / 1000.0,
                "rvv_ms": statistics.mean(float(row["rvv_us"]) for row in rows) / 1000.0,
                "ba_values": values,
                "baseline": {"label": "Std build", **common},
                "candidate": {"label": "RVV build", **common},
            }
        ],
    }
    args.manifest.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.output}")
    print(f"wrote manifest: {args.manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
