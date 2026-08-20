#!/usr/bin/env python3
"""
生成 marching_cubes Phase 060 RVV-vs-RVV A/B 摘要。

输入是成对的 baseline / candidate board run 目录，每个目录包含
`run_bench_rvv.log`。输出 summary.md 给 reviewer 阅读，manifest 给通用
Evidence Doctor 做同边界检查。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>mc_prod_xyz_64)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")


def parse_rvv_log(path: Path) -> dict[str, Any]:
    metadata: dict[str, Any] = {}
    case: dict[str, Any] | None = None
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
            case = {"name": match.group("name"), "ms": float(match.group("ms"))}
            continue
        if case and (match := CHECKSUM_RE.search(line)):
            case["checksum"] = match.group("checksum")
    if case is None:
        raise SystemExit(f"no mc_prod_xyz_64 RVV case parsed in {path}")
    return {"path": str(path), "metadata": metadata, "case": case}


def bucket_for(values: list[float]) -> str:
    median = statistics.median(values)
    if median >= 1.10 and min(values) >= 1.03:
        return "positive"
    if median >= 1.03 and min(values) >= 0.99:
        return "weak_positive"
    if all(0.99 <= value < 1.03 for value in values):
        return "neutral"
    if median < 0.99 or min(values) < 0.99:
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


def load_pairs(args: argparse.Namespace) -> list[dict[str, Any]]:
    if len(args.baseline_dir) != len(args.candidate_dir):
        raise SystemExit("--baseline-dir and --candidate-dir counts must match")
    pairs: list[dict[str, Any]] = []
    for index, (baseline_dir, candidate_dir) in enumerate(zip(args.baseline_dir, args.candidate_dir), start=1):
        baseline = parse_rvv_log(baseline_dir / "run_bench_rvv.log")
        candidate = parse_rvv_log(candidate_dir / "run_bench_rvv.log")
        b_case = baseline["case"]
        c_case = candidate["case"]
        if b_case.get("checksum") != c_case.get("checksum"):
            checksum_match = False
        else:
            checksum_match = True
        pairs.append(
            {
                "run": f"pair{index}",
                "baseline": baseline,
                "candidate": candidate,
                "ba": float(b_case["ms"]) / float(c_case["ms"]),
                "checksum_match": checksum_match,
            }
        )
    return pairs


def write_summary(path: Path, pairs: list[dict[str, Any]]) -> None:
    values = [pair["ba"] for pair in pairs]
    lines = [
        "# marching_cubes Phase 060 RVV-vs-RVV board summary",
        "",
        "- run_count: `{}`".format(len(pairs)),
        "- evidence_role: `production-detail RVV-family-selection`",
        "- A/B boundary: `performReconstruction() public wrapper, adopted RVV baseline vs finite-collapse RVV candidate`",
        "- timer boundary: synthetic voxelized grid public reconstruction; same `mc_prod_xyz_64` case; Hoppe/RBF voxelization excluded.",
        "- decision_bucket: `{}`".format(bucket_for(values)),
        "",
        "| case | runs | median | min | max | p10 | p90 | checksum match | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |",
    ]
    checksum_match = all(pair["checksum_match"] for pair in pairs)
    lines.append(
        "| mc_prod_xyz_64 | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {} | {} |".format(
            len(values),
            statistics.median(values),
            min(values),
            max(values),
            percentile(values, 0.10),
            percentile(values, 0.90),
            "yes" if checksum_match else "no",
            ", ".join(f"{value:.3f}x" for value in values),
        )
    )
    lines.extend(
        [
            "",
            "| run | baseline RVV ms | candidate RVV ms | candidate speedup |",
            "| --- | ---: | ---: | ---: |",
        ]
    )
    for pair in pairs:
        lines.append(
            "| {} | {:.4f} | {:.4f} | {:.3f}x |".format(
                pair["run"],
                pair["baseline"]["case"]["ms"],
                pair["candidate"]["case"]["ms"],
                pair["ba"],
            )
        )
    lines.append("")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def build_manifest(args: argparse.Namespace, pairs: list[dict[str, Any]]) -> dict[str, Any]:
    values = [pair["ba"] for pair in pairs]
    first_meta = pairs[0]["baseline"]["metadata"]
    baseline_med = statistics.median(float(pair["baseline"]["case"]["ms"]) for pair in pairs)
    candidate_med = statistics.median(float(pair["candidate"]["case"]["ms"]) for pair in pairs)
    return {
        "schema_version": 1,
        "topic": "marching_cubes",
        "module": "surface",
        "run_label": "phase060_rvv_ab",
        "evidence_role": "strict_ab",
        "boundary": "public reconstruction wrapper; RVV-vs-RVV detail A/B",
        "decision_question": "RVV-family-selection for active-z finite-collapse single-buffer candidate",
        "summary": {
            "title": "marching_cubes Phase 060 RVV-vs-RVV repeated board diagnostic",
            "evidence_role": "strict_ab",
            "summary_path": str(args.summary),
            "metadata": {
                "device": args.device,
                "iterations": first_meta.get("iterations", 0),
                "warmup_iterations": first_meta.get("warmup_iterations", 0),
                "run_count": len(pairs),
                "taskset": "not_recorded_board_target",
                "governor": "not_recorded_board_target",
                "freq": "not_recorded_board_target",
                "temperature": "not_recorded_board_target",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded_for_phase060_rvv_ab",
                "dataset": first_meta.get("dataset", "synthetic signed-distance grids"),
                "case_filter": "mc_prod_xyz_64",
            },
        },
        "checks": {"strict_ab": True},
        "comparisons": [
            {
                "name": "mc_prod_xyz_64",
                "group": "marching_cubes_phase060_rvv_ab",
                "case_kind": "board_public_production_detail_rvv_family_ab",
                "evidence_role": "strict_ab",
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": "mc_prod_xyz_64",
                "run_count": len(pairs),
                "iterations": first_meta.get("iterations", 0),
                "warmup_iterations": first_meta.get("warmup_iterations", 0),
                "ba_values": values,
                "decision_bucket": bucket_for(values),
                "std_ms": baseline_med,
                "rvv_ms": candidate_med,
                "allowed_contract_mismatches": ["reduction", "asm_boundary"],
                "baseline": {
                    "label": "Phase 050 adopted RVV active-cell prepass",
                    "boundary": "public reconstruction wrapper",
                    "wrapper": "bench_marching_cubes",
                    "row_source": "dense-grid-cell",
                    "solve": False,
                    "checksum_policy": "quantized_xyz_points_plus_counts",
                    "timer_boundary": "performReconstruction public path over synthetic voxelized grid; Hoppe/RBF excluded",
                    "gate": "__RVV10__ and RVVXYZAoSFloatLayout<PointNT>",
                    "mask": "NaN cell excluded before surface emit",
                    "reduction": "RVV active-cell prepass with cube_indices + finite_flags staging",
                    "asm_boundary": "Phase 050 adopted RVV binary",
                    "std_ms": baseline_med,
                },
                "candidate": {
                    "label": "Phase 060 finite-collapse single-buffer RVV active-cell prepass",
                    "boundary": "public reconstruction wrapper",
                    "wrapper": "bench_marching_cubes",
                    "row_source": "dense-grid-cell",
                    "solve": False,
                    "checksum_policy": "quantized_xyz_points_plus_counts",
                    "timer_boundary": "performReconstruction public path over synthetic voxelized grid; Hoppe/RBF excluded",
                    "gate": "__RVV10__ and RVVXYZAoSFloatLayout<PointNT>",
                    "mask": "NaN cell excluded before surface emit",
                    "reduction": "RVV active-cell prepass with single cube_indices staging",
                    "asm_boundary": "Phase 060 RVV binary; getActiveVoxelsZRVV contains one cube index vse32 store",
                    "rvv_ms": candidate_med,
                },
                "log_checksums": {
                    "std": sorted(str(pair["baseline"]["case"].get("checksum", "missing")) for pair in pairs),
                    "rvv": sorted(str(pair["candidate"]["case"].get("checksum", "missing")) for pair in pairs),
                    "exact_match": all(pair["checksum_match"] for pair in pairs),
                },
            }
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-dir", action="append", required=True, type=Path)
    parser.add_argument("--candidate-dir", action="append", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--device", default="Milkv-Jupiter")
    args = parser.parse_args()

    pairs = load_pairs(args)
    write_summary(args.summary, pairs)
    manifest = build_manifest(args, pairs)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
