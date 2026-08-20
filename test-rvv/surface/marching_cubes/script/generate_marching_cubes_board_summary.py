#!/usr/bin/env python3
"""
生成 marching_cubes board repeated summary（板卡重复测试摘要）。

输入目录可以是直接包含 `run_bench_std.log` / `run_bench_rvv.log` 的 board 输出目录。
脚本输出：

- `summary.md`：给 reviewer 阅读的摘要，可用 `--case-prefix` 把 edge / prepass 证据分开。
- `evidence_manifest.json`：给 Evidence Doctor（证据体检）读取的机器可读清单。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>mc_(?:edge|prepass|prod)_[A-Za-z0-9_]+)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")


def sha256_file(path: Path) -> str:
    if not path.is_file():
        return "not_recorded"
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def parse_log(path: Path) -> dict[str, Any]:
    cases: dict[str, dict[str, Any]] = {}
    metadata: dict[str, Any] = {}
    current_case: str | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := ITER_RE.match(line):
            metadata["iterations"] = int(match.group("value"))
            continue
        if match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
            continue
        if match := DATASET_RE.match(line):
            metadata["dataset"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name")
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None
    return {"path": str(path), "metadata": metadata, "cases": cases}


def filter_cases(cases: dict[str, dict[str, Any]], case_prefix: str) -> dict[str, dict[str, Any]]:
    if not case_prefix:
        return dict(cases)
    return {name: info for name, info in cases.items() if name.startswith(case_prefix)}


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def bucket_for(values: list[float]) -> str:
    median = statistics.median(values)
    if median >= 1.10 and min(values) >= 1.03:
        return "positive"
    if median >= 1.03 and min(values) >= 0.97:
        return "weak_positive"
    if all(0.97 <= value < 1.03 for value in values):
        return "neutral"
    if median < 0.97 or min(values) < 0.97:
        return "negative"
    return "unstable"


def collect_runs(run_dirs: list[Path], case_prefix: str) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    for directory in run_dirs:
        std_path = directory / "run_bench_std.log"
        rvv_path = directory / "run_bench_rvv.log"
        if not std_path.is_file() or not rvv_path.is_file():
            raise SystemExit(f"missing run_bench_std.log or run_bench_rvv.log under {directory}")
        std = parse_log(std_path)
        rvv = parse_log(rvv_path)
        std["cases"] = filter_cases(std["cases"], case_prefix)
        rvv["cases"] = filter_cases(rvv["cases"], case_prefix)
        std_cases = set(std["cases"])
        rvv_cases = set(rvv["cases"])
        if std_cases != rvv_cases:
            missing = ", ".join(sorted(std_cases ^ rvv_cases))
            raise SystemExit(f"case set mismatch under {directory}: {missing}")
        if not std_cases:
            prefix_desc = case_prefix or "mc_(edge|prepass)_"
            raise SystemExit(f"no {prefix_desc} cases parsed under {directory}")
        runs.append({"name": directory.name, "std": std, "rvv": rvv})
    return runs


def build_case_metrics(runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    labels = sorted(runs[0]["std"]["cases"])
    result: dict[str, dict[str, Any]] = {}
    for label in labels:
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        std_checksums: set[str] = set()
        rvv_checksums: set[str] = set()
        for run in runs:
            std_case = run["std"]["cases"][label]
            rvv_case = run["rvv"]["cases"][label]
            std_ms = float(std_case["ms"])
            rvv_ms = float(rvv_case["ms"])
            ba = std_ms / rvv_ms
            values.append(ba)
            std_checksums.add(str(std_case.get("checksum", "missing")))
            rvv_checksums.add(str(rvv_case.get("checksum", "missing")))
            rows.append({"run": run["name"], "std_ms": std_ms, "rvv_ms": rvv_ms, "ba": ba})
        result[label] = {
            "values": values,
            "median": statistics.median(values),
            "min": min(values),
            "max": max(values),
            "p10": percentile(values, 0.10),
            "p90": percentile(values, 0.90),
            "bucket": bucket_for(values),
            "below_one_count": sum(1 for value in values if value < 1.0),
            "rows": rows,
            "checksums": {
                "std": sorted(std_checksums),
                "rvv": sorted(rvv_checksums),
                "exact_match": sorted(std_checksums) == sorted(rvv_checksums),
            },
        }
    return result


def family_title(case_prefix: str) -> str:
    if case_prefix == "mc_edge_":
        return "edge-interpolation"
    if case_prefix == "mc_prepass_":
        return "cube-index prepass"
    if case_prefix == "mc_prod_":
        return "production-direct"
    return "filtered"


def family_case_kind(case_prefix: str) -> str:
    if case_prefix == "mc_edge_":
        return "board_pre_production_edge_interpolation_diagnostic"
    if case_prefix == "mc_prepass_":
        return "board_pre_production_cube_index_prepass_diagnostic"
    if case_prefix == "mc_prod_":
        return "board_public_production_direct_diagnostic"
    return "board_pre_production_filtered_diagnostic"


def evidence_role_label(case_prefix: str) -> str:
    if case_prefix == "mc_prod_":
        return "production-public diagnostic"
    return "production-shaped diagnostic"


def boundary_label(case_prefix: str) -> str:
    if case_prefix == "mc_prod_":
        return "public reconstruction wrapper"
    return "test helper"


def timer_boundary_label(case_prefix: str) -> str:
    if case_prefix == "mc_prod_":
        return (
            "performReconstruction public path over synthetic voxelized grid; "
            "includes grid copy, bounding box, surface scan and polygon output; "
            "excludes synthetic grid generation and Hoppe/RBF solver."
        )
    return "grid scan + surface emission; excludes grid generation, Hoppe kd-tree and RBF solver."


def reduction_label(case_prefix: str, is_candidate: bool) -> str:
    if case_prefix == "mc_edge_":
        return "RVV 12-edge interpolation staging" if is_candidate else "scalar_edge_interpolation"
    if case_prefix == "mc_prod_":
        return "RVV production active-cell prepass" if is_candidate else "scalar_public_cell_scan"
    return "RVV z-lane active-cell staging" if is_candidate else "scalar_cube_index_prepass"


def point_type_label(case_name: str) -> str:
    if case_name.startswith("mc_prod_xyzi_"):
        return "PointXYZI"
    if case_name.startswith("mc_prod_xyzrgb_"):
        return "PointXYZRGB"
    if case_name.startswith("mc_prod_xyzrgba_"):
        return "PointXYZRGBA"
    if case_name.startswith("mc_prod_xyz_"):
        return "PointXYZ"
    return "PointNormal"


def write_summary(
    path: Path,
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    family: str,
    case_prefix: str,
) -> None:
    lines = [
        f"# marching_cubes {family} board repeated summary",
        "",
        f"- run_count: `{len(runs)}`",
        f"- evidence_role: `{evidence_role_label(case_prefix)}`",
        f"- A/B boundary: `{boundary_label(case_prefix)}`",
        f"- timer boundary: {timer_boundary_label(case_prefix)}",
        "",
        "| case | runs | median | min | max | p10 | p90 | below 1.0 | checksum match | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |",
    ]
    for name, info in sorted(metrics.items()):
        values = ", ".join(f"{value:.3f}x" for value in info["values"])
        checksum = "yes" if info["checksums"]["exact_match"] else "no"
        lines.append(
            f"| {name} | {len(info['values'])} | {info['median']:.3f}x | "
            f"{info['min']:.3f}x | {info['max']:.3f}x | {info['p10']:.3f}x | "
            f"{info['p90']:.3f}x | {info['below_one_count']} | {checksum} | {values} |"
        )
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def build_manifest(
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    summary_path: Path,
    args: argparse.Namespace,
    case_prefix: str,
) -> dict[str, Any]:
    first_meta = runs[0]["std"]["metadata"]
    family = family_title(case_prefix)
    case_kind = family_case_kind(case_prefix)
    comparisons: list[dict[str, Any]] = []
    for name, info in sorted(metrics.items()):
        std_med = statistics.median(row["std_ms"] for row in info["rows"])
        rvv_med = statistics.median(row["rvv_ms"] for row in info["rows"])
        comparisons.append(
            {
                "name": name,
                "group": f"marching_cubes_{family.replace(' ', '_').replace('-', '_')}_repeated",
                "case_kind": case_kind,
                "evidence_role": "strict_ab",
                "point_type": point_type_label(name),
                "scalar": "float",
                "size": name,
                "run_count": len(runs),
                "iterations": first_meta.get("iterations", 0),
                "warmup_iterations": first_meta.get("warmup_iterations", 0),
                "ba_values": info["values"],
                "decision_bucket": info["bucket"],
                "std_ms": std_med,
                "rvv_ms": rvv_med,
                "allowed_contract_mismatches": ["reduction", "asm_boundary"],
                "baseline": {
                    "label": f"Std build {family} diagnostic",
                    "boundary": boundary_label(case_prefix),
                    "wrapper": "bench_marching_cubes",
                    "row_source": "dense-grid-cell",
                    "solve": False,
                    "checksum_policy": "quantized_xyz_points_plus_counts",
                    "timer_boundary": timer_boundary_label(case_prefix),
                    "gate": "__RVV10__ build switch; same synthetic grid",
                    "mask": "NaN cell skipped before surface emit",
                    "reduction": reduction_label(case_prefix, False),
                    "asm_boundary": "not_applicable_std_build",
                    "std_ms": std_med,
                },
                "candidate": {
                    "label": f"RVV build {family} diagnostic",
                    "boundary": boundary_label(case_prefix),
                    "wrapper": "bench_marching_cubes",
                    "row_source": "dense-grid-cell",
                    "solve": False,
                    "checksum_policy": "quantized_xyz_points_plus_counts",
                    "timer_boundary": timer_boundary_label(case_prefix),
                    "gate": "__RVV10__ build switch; same synthetic grid",
                    "mask": "NaN cell skipped before surface emit",
                    "reduction": reduction_label(case_prefix, True),
                    "asm_boundary": "bench_marching_cubes_rvv binary; helper may be inlined",
                    "rvv_ms": rvv_med,
                },
                "log_checksums": info["checksums"],
            }
        )
    return {
        "schema_version": 1,
        "topic": "marching_cubes",
        "module": "surface",
        "run_label": args.run_label,
        "evidence_role": "strict_ab",
        "boundary": boundary_label(case_prefix),
        "decision_question": args.decision_question or f"RVV-vs-scalar pre-production {family} diagnostic",
        "summary": {
            "title": args.summary_title or f"marching_cubes {family} repeated board diagnostic",
            "evidence_role": "strict_ab",
            "summary_path": str(summary_path),
            "metadata": {
                "device": args.device,
                "iterations": first_meta.get("iterations", 0),
                "warmup_iterations": first_meta.get("warmup_iterations", 0),
                "run_count": len(runs),
                "taskset": "not_recorded_board_target",
                "governor": "not_recorded_board_target",
                "freq": "not_recorded_board_target",
                "temperature": "not_recorded_board_target",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": f"std={sha256_file(Path(args.std_bin))}; rvv={sha256_file(Path(args.rvv_bin))}",
                "dataset": first_meta.get("dataset", "synthetic signed-distance grids"),
                "case_filter": case_prefix + "*" if case_prefix else "mc_(edge|prepass|prod)_*",
            },
        },
        "checks": {"strict_ab": True},
        "comparisons": comparisons,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", action="append", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--run-label", default="edge_interpolation_repeated")
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument("--std-bin", default="build/riscv/bench_marching_cubes_std")
    parser.add_argument("--rvv-bin", default="build/riscv/bench_marching_cubes_rvv")
    parser.add_argument("--case-prefix", default="")
    parser.add_argument("--summary-title", default=None)
    parser.add_argument("--decision-question", default=None)
    args = parser.parse_args()

    runs = collect_runs(args.run_dir, args.case_prefix)
    metrics = build_case_metrics(runs)
    write_summary(args.summary, runs, metrics, family_title(args.case_prefix), args.case_prefix)
    manifest = build_manifest(runs, metrics, args.summary, args, args.case_prefix)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
