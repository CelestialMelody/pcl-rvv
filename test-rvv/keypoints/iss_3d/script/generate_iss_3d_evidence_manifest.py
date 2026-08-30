#!/usr/bin/env python3
"""把 ISS 3D scatter repeated board bench 输出整理成 evidence manifest。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(
    r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<ms>[0-9.]+)\s*ms\s*/\s*iter\s*$",
    re.MULTILINE,
)
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s+checksum:\s*(?P<checksum>[0-9]+)\s*$", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)\s*$", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup Iterations:\s*(?P<warmup>\d+)\s*$", re.MULTILINE)


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def unique_or_mixed(values: list[Any]) -> Any:
    present = [value for value in values if value is not None]
    if not present:
        return None
    first = present[0]
    return first if all(value == first for value in present) else "mixed"


def decision_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median > 1.10 and below_one == 0:
        return "positive"
    if 1.03 <= median <= 1.10 and below_one <= 1:
        return "weak_positive"
    if 0.97 <= median < 1.03:
        return "neutral"
    if median < 0.97:
        return "negative"
    return "unstable"


def parse_context(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    return context


def parse_checksum(path: Path, case_name: str) -> str | None:
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    for match in CHECKSUM_RE.finditer(text):
        if match.group("name") == case_name:
            return match.group("checksum")
    return None


def parse_times(path: Path) -> dict[str, float]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    return {item.group("name"): float(item.group("ms")) for item in CASE_RE.finditer(text)}


def metadata_for_case(name: str, evidence_role: str) -> dict[str, str]:
    cases = {
        "scatter_indexed_256": "8192 synthetic PointXYZ points, 256 pseudo-random neighbor indices",
        "scatter_indexed_tail_73": "8192 synthetic PointXYZ points, 73 pseudo-random neighbor indices and RVV tail lanes",
        "scatter_contiguous_256": "8192 synthetic PointXYZ points, 256 contiguous-like neighbor indices",
        "public_iss_3d_grid_4096": "4096 synthetic PointXYZ grid points, public ISSKeypoint3D::compute() with KdTree search",
    }
    if name not in cases:
        raise ValueError(f"unknown ISS 3D case label: {name}")
    is_public = evidence_role == "production_public" or name.startswith("public_")
    return {
        "group": "iss_3d_scatter_f64",
        "case_kind": evidence_role,
        "evidence_role": evidence_role,
        "point_type": "pcl::PointXYZ",
        "row_source": "indexed_neighbor_list",
        "boundary": "public_compute" if is_public else "test_helper",
        "wrapper": (
            "keypoints/include/pcl/keypoints/impl/iss_3d.hpp"
            if is_public
            else "test-rvv/keypoints/iss_3d/include/impl/iss_3d_scatter.hpp"
        ),
        "timer_boundary": (
            "public compute(); includes KdTree radius search, scatter matrix, Eigen EVD, NMS and output construction"
            if is_public
            else "scatter helper only; excludes searchForNeighbors, Eigen EVD, NMS and output construction"
        ),
        "gate": (
            "RVV scatter is gated by xyz AoS traits, 32-bit byte offset range and neighbor_count >= 16"
            if is_public
            else "neighbor_count >= 16 takes RVV path when __RVV10__ is enabled; otherwise Std fallback"
        ),
        "mask": "no finite mask in this diagnostic; synthetic inputs are finite and neighbor indices are valid",
        "reduction": "six double covariance accumulators reduced with RVV vector reduction",
        "checksum_policy": (
            "FNV-1a over public output xyz values quantized at 1e-5 tolerance plus keypoint indices"
            if is_public
            else "FNV-1a over 3x3 double scatter matrix entries quantized at 1e-5 tolerance"
        ),
        "asm_boundary": (
            "keypoints/include/pcl/keypoints/impl/iss_3d.hpp / computeISSScatterMatrixRVV"
            if is_public
            else "test-rvv/keypoints/iss_3d/include/impl/iss_3d_scatter.hpp / computeScatterMatrixRVV"
        ),
        "size": cases[name],
        "notes": (
            "Production public benchmark; use it to decide whether the bounded scatter patch is worth adopting."
            if is_public
            else "Diagnostic benchmark only; production direct evidence requires a production patch and public/runtime path bench."
        ),
    }


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vluxei32", "vfred", "vfwcvt", "vfmacc", "vfsub", "vle32")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def file_sha256(path: Path) -> str | None:
    if not path.exists():
        return None
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--title", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    args = parser.parse_args()

    summary_paths = sorted(args.summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not summary_paths:
        raise SystemExit(f"no run_*/analyze_bench_compare.log files found under {args.summary_dir}")

    contexts: list[dict[str, Any]] = []
    grouped: dict[str, dict[str, list[Any]]] = {}
    for path in summary_paths:
        run_dir = path.parent
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        contexts.append(parse_context(std_log if std_log.exists() else rvv_log))
        std_times = parse_times(std_log)
        rvv_times = parse_times(rvv_log)
        for name in sorted(set(std_times) & set(rvv_times)):
            metadata_for_case(name, args.evidence_role)
            std_ms = std_times[name]
            rvv_ms = rvv_times[name]
            bucket = grouped.setdefault(name, {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(std_ms)
            bucket["rvv"].append(rvv_ms)
            bucket["speedup"].append(std_ms / rvv_ms if rvv_ms else 0.0)
            bucket["paths"].append(str(path))

    if not grouped:
        raise SystemExit("no benchmark comparisons parsed")

    asm_lines = count_rvv_asm_lines(args.asm)
    args.summary_output.parent.mkdir(parents=True, exist_ok=True)

    is_public_summary = args.evidence_role == "production_public"
    summary_lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(summary_paths)}`",
        f"- iterations: `{unique_or_mixed([ctx.get('iterations') for ctx in contexts])}`",
        f"- warmup_iterations: `{unique_or_mixed([ctx.get('warmup_iterations') for ctx in contexts])}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{'public_compute' if is_public_summary else 'test_helper'}`",
        f"- asm_rvv_line_count: `{asm_lines}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    comparisons = []
    all_speedups: list[float] = []
    for case_name, stats in sorted(grouped.items()):
        speedups = [float(value) for value in stats["speedup"]]
        std_values = [float(value) for value in stats["std"]]
        rvv_values = [float(value) for value in stats["rvv"]]
        paths = [str(value) for value in stats["paths"]]
        all_speedups.extend(speedups)
        meta = metadata_for_case(case_name, args.evidence_role)
        below_one = sum(1 for value in speedups if value < 1.0)
        summary_lines.append(
            "| `{}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
                case_name,
                len(speedups),
                statistics.median(speedups),
                min(speedups),
                max(speedups),
                percentile(speedups, 0.10),
                percentile(speedups, 0.90),
                below_one,
                len(speedups),
                ", ".join(f"{value:.3f}x" for value in speedups),
            )
        )
        std_checksum = unique_or_mixed([parse_checksum(Path(path).with_name("run_bench_std.log"), case_name) for path in paths])
        rvv_checksum = unique_or_mixed([parse_checksum(Path(path).with_name("run_bench_rvv.log"), case_name) for path in paths])
        comparisons.append(
            {
                "name": case_name,
                "group": meta["group"],
                "evidence_role": meta["evidence_role"],
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": len(speedups),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "std_ms": statistics.mean(std_values) if std_values else None,
                "rvv_ms": statistics.mean(rvv_values) if rvv_values else None,
                "ba_values": speedups,
                "std_checksum": std_checksum,
                "rvv_checksum": rvv_checksum,
                "checksum_equal": std_checksum == rvv_checksum,
                "boundary": meta["boundary"],
                "wrapper": meta["wrapper"],
                "row_source": meta["row_source"],
                "solve": False,
                "checksum_policy": meta["checksum_policy"],
                "timer_boundary": meta["timer_boundary"],
                "gate": meta["gate"],
                "mask": meta["mask"],
                "reduction": meta["reduction"],
                "asm_boundary": meta["asm_boundary"],
                "rvv_instr_count": asm_lines,
                "notes": meta["notes"],
                "source_logs": paths,
            }
        )

    args.summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "run_label": args.run_label,
            "evidence_role": args.evidence_role,
            "expected_runs": args.expected_runs,
            "collected_runs": len(summary_paths),
            "decision_bucket": decision_bucket(all_speedups),
            "summary_path": str(args.summary_output),
            "asm_path": str(args.asm),
            "asm_sha256": file_sha256(args.asm),
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
