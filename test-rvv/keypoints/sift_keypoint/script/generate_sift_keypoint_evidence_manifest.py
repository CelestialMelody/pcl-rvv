#!/usr/bin/env python3
"""把 SIFT scale-space repeated board 输出整理成 Evidence Doctor 可读的 manifest。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_TIME_RE = re.compile(
    r"^(?P<name>(?:sift|public_sift_keypoint)_[A-Za-z0-9_]+):\s*(?P<ms>[0-9.]+)\s+ms\s*/\s*iter$",
    re.MULTILINE,
)
CHECKSUM_RE = re.compile(
    r"^checksum_(?P<name>(?:sift|public_sift_keypoint)_[A-Za-z0-9_]+)=(?P<checksum>[0-9]+)\s+iterations=(?P<iterations>\d+)\s+warmup=(?P<warmup>\d+)\s+rvv_chunks=(?P<chunks>\d+)$",
    re.MULTILINE,
)
EXTREMA_RE = re.compile(r"^extrema_(?P<name>sift_[A-Za-z0-9_]+)=(?P<checksum>[0-9]+)$", re.MULTILINE)
KEYPOINTS_RE = re.compile(r"^keypoints_(?P<name>public_sift_keypoint_[A-Za-z0-9_]+)=(?P<count>\d+)$", re.MULTILINE)


CASE_LABELS: dict[str, dict[str, str]] = {
    "sift_scale_space_96_points": {
        "group": "sift_scale_space_kernel",
        "case_kind": "diagnostic",
        "point_type": "synthetic float neighborhood batches",
        "row_source": "synthetic_sorted_neighbor_batches",
        "size": "96 synthetic points, 29 neighbors, 6 scales",
        "timer_boundary": "computeScaleSpace synthetic batch only; excludes radiusSearch, nearestKSearch and public dispatch",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper for Gaussian weights and row reduction; non-RVV build uses portable scalar fallback",
        "reduction": "Gaussian weights, numerator / denominator reductions and DoG subtraction",
        "notes": "诊断候选只覆盖 scale-space 热点，不覆盖 extremum search 的真实 search tree 成本。",
    },
    "sift_scale_space_192_points": {
        "group": "sift_scale_space_kernel",
        "case_kind": "diagnostic",
        "point_type": "synthetic float neighborhood batches",
        "row_source": "synthetic_sorted_neighbor_batches",
        "size": "192 synthetic points, 33 neighbors, 6 scales",
        "timer_boundary": "computeScaleSpace synthetic batch only; excludes radiusSearch, nearestKSearch and public dispatch",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper for Gaussian weights and row reduction; non-RVV build uses portable scalar fallback",
        "reduction": "Gaussian weights, numerator / denominator reductions and DoG subtraction",
        "notes": "扩大点数以观察 scale-space 局部核在更大 batch 下的稳定性。",
    },
    "sift_scale_space_tail_113_points": {
        "group": "sift_scale_space_kernel",
        "case_kind": "diagnostic",
        "point_type": "synthetic float neighborhood batches",
        "row_source": "synthetic_sorted_neighbor_batches",
        "size": "113 synthetic points, 25 neighbors, 6 scales",
        "timer_boundary": "computeScaleSpace synthetic batch only; excludes radiusSearch, nearestKSearch and public dispatch",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper for Gaussian weights and row reduction; non-RVV build uses portable scalar fallback",
        "reduction": "Gaussian weights, numerator / denominator reductions and DoG subtraction",
        "notes": "tail case 用来观察邻域长度不是整块倍数时的 vector lane 补齐行为。",
    },
    "public_sift_keypoint_320x240": {
        "group": "sift_public_compute",
        "case_kind": "production_public",
        "point_type": "PointXYZI organized cloud",
        "row_source": "organized dense public cloud",
        "size": "320x240 organized synthetic cloud",
        "timer_boundary": "public SIFTKeypoint::compute() on organized dense input; includes downsample, radiusSearch, scale-space, extrema and output assembly",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper inside computeScaleSpace; non-RVV build keeps scalar path",
        "reduction": "Gaussian weights, numerator / denominator reductions and DoG subtraction",
        "notes": "public compute smoke / production direct benchmark",
    },
    "public_sift_keypoint_641x481": {
        "group": "sift_public_compute",
        "case_kind": "production_public",
        "point_type": "PointXYZI organized cloud",
        "row_source": "organized dense public cloud",
        "size": "641x481 organized synthetic cloud",
        "timer_boundary": "public SIFTKeypoint::compute() on organized dense input; includes downsample, radiusSearch, scale-space, extrema and output assembly",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper inside computeScaleSpace; non-RVV build keeps scalar path",
        "reduction": "Gaussian weights, numerator / denominator reductions and DoG subtraction",
        "notes": "tail case for public compute on a larger organized cloud",
    },
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


def unique_or_mixed(values: list[Any]) -> Any:
    present = [value for value in values if value is not None]
    if not present:
        return None
    first = present[0]
    return first if all(value == first for value in present) else "mixed"


def decision_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.20 and below_one == 0:
        return "positive"
    if 1.05 <= median < 1.20 and below_one <= 1:
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def parse_bench_log(path: Path) -> dict[str, dict[str, Any]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    by_case: dict[str, dict[str, Any]] = {}
    for match in CASE_TIME_RE.finditer(text):
        by_case.setdefault(match.group("name"), {})["ms"] = float(match.group("ms"))
    for match in CHECKSUM_RE.finditer(text):
        entry = by_case.setdefault(match.group("name"), {})
        entry["checksum"] = match.group("checksum")
        entry["iterations"] = int(match.group("iterations"))
        entry["warmup_iterations"] = int(match.group("warmup"))
        entry["rvv_chunks"] = int(match.group("chunks"))
    for match in EXTREMA_RE.finditer(text):
        entry = by_case.setdefault(match.group("name"), {})
        entry["extrema_checksum"] = match.group("checksum")
    for match in KEYPOINTS_RE.finditer(text):
        entry = by_case.setdefault(match.group("name"), {})
        entry["keypoints"] = int(match.group("count"))
    return by_case


def binary_hash(path: Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vsetvl", "vle32", "vfredosum", "vfmul", "vfdiv", "vfmerge", "expf_RVV_f32m2")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def side_metadata(case_name: str, *, build: str, ms_values: list[float], checksums: list[str | None]) -> dict[str, Any]:
    meta = CASE_LABELS[case_name]
    is_public = meta["case_kind"] == "production_public"
    side = {
        "boundary": (
            "scale-space diagnostic helper / synthetic neighborhood batch"
            if not is_public
            else "public SIFTKeypoint::compute() / organized cloud"
        ),
        "wrapper": "test-rvv/keypoints/sift_keypoint/src/bench_sift_keypoint.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": (
            "sampled DoG checksum plus extrema checksum, warm-up excluded from final timed average"
            if not is_public
            else "public output checksum, warm-up excluded from final timed average"
        ),
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": (
            "all neighbors are synthetic finite floats; RVV lane mask only trims the tail chunk"
            if not is_public
            else "organized cloud inputs are finite; RVV lane mask only trims the tail chunk inside computeScaleSpace"
        ),
        "reduction": meta["reduction"],
        "asm_boundary": (
            "test-rvv/keypoints/sift_keypoint/include/impl/sift_keypoint_scale_space.hpp RVV helper"
            if not is_public
            else "keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp computeScaleSpace RVV helper"
        ),
        "checksum": unique_or_mixed(checksums),
    }
    side[f"{build}_ms"] = statistics.mean(ms_values)
    return side


def collect_runs(repeat_root: Path) -> list[tuple[Path, dict[str, dict[str, Any]], dict[str, dict[str, Any]]]]:
    run_dirs = sorted({path for pattern in ("run-*", "run_*") for path in repeat_root.glob(pattern) if path.is_dir()})
    if not run_dirs:
        run_dirs = [repeat_root]
    runs = []
    for run_dir in run_dirs:
        std_path = run_dir / "run_bench_std.log"
        rvv_path = run_dir / "run_bench_rvv.log"
        analyze_path = run_dir / "analyze_bench_compare.log"
        if not std_path.exists() or not rvv_path.exists():
            continue
        if not analyze_path.exists():
            raise SystemExit(f"missing analyze_bench_compare.log in {run_dir}")
        runs.append((run_dir, parse_bench_log(std_path), parse_bench_log(rvv_path)))
    if not runs:
        raise SystemExit(f"no run_bench_std.log/run_bench_rvv.log pairs found under {repeat_root}")
    return runs


def selected_case_names() -> list[str]:
    return list(CASE_LABELS)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", type=Path)
    parser.add_argument("--title", required=True)
    parser.add_argument("--summary-role", default="diagnostic")
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--case-kind", default="diagnostic")
    parser.add_argument("--ab-boundary", default="test_helper")
    parser.add_argument("--point-type", default="synthetic float neighborhood batches")
    parser.add_argument("--timer-boundary", default="computeScaleSpace synthetic batch only")
    parser.add_argument("--baseline-asm-boundary", default="scalar helper fallback")
    parser.add_argument("--asm-boundary", default="sift_keypoint test-support candidate / rvv candidate")
    parser.add_argument("--notes", default="")
    args = parser.parse_args()

    runs = collect_runs(args.summary_dir)
    summary_output = args.summary_output or args.output.with_name("repeated-evidence-summary.md")
    if len(runs) != args.expected_runs:
        raise SystemExit(f"expected {args.expected_runs} runs but collected {len(runs)}")
    asm_lines = count_rvv_asm_lines(args.asm)
    rvv_binary_hash = binary_hash(Path("build/riscv/bench_sift_keypoint_rvv"))
    std_binary_hash = binary_hash(Path("build/riscv/bench_sift_keypoint_std"))

    comparisons: list[dict[str, Any]] = []
    summary_lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{'test_helper' if args.evidence_role == 'diagnostic' else args.ab_boundary}`",
        f"- collected_runs: `{len(runs)}`",
        f"- asm_rvv_line_count: `{asm_lines}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    for case_name in selected_case_names():
        std_values: list[float] = []
        rvv_values: list[float] = []
        speedups: list[float] = []
        std_checksums: list[str | None] = []
        rvv_checksums: list[str | None] = []
        rvv_chunks: list[int | None] = []
        keypoints: list[int | None] = []
        iterations: list[int | None] = []
        warmups: list[int | None] = []
        for _, std_cases, rvv_cases in runs:
            if case_name not in std_cases or case_name not in rvv_cases:
                continue
            std_entry = std_cases[case_name]
            rvv_entry = rvv_cases[case_name]
            std_ms = float(std_entry["ms"])
            rvv_ms = float(rvv_entry["ms"])
            std_values.append(std_ms)
            rvv_values.append(rvv_ms)
            speedups.append(std_ms / rvv_ms if rvv_ms else 0.0)
            std_checksums.append(std_entry.get("checksum"))
            rvv_checksums.append(rvv_entry.get("checksum"))
            rvv_chunks.append(rvv_entry.get("rvv_chunks"))
            keypoints.append(rvv_entry.get("keypoints"))
            iterations.append(std_entry.get("iterations"))
            warmups.append(std_entry.get("warmup_iterations"))

        if not speedups:
            continue

        meta = CASE_LABELS[case_name]
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
        comparisons.append(
            {
                "name": case_name,
                "group": meta["group"],
                "evidence_role": args.evidence_role,
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "row_source": meta["row_source"],
                "size": meta["size"],
                "run_count": len(speedups),
                "iterations": unique_or_mixed(iterations),
                "warmup_iterations": unique_or_mixed(warmups),
                "std_ms": statistics.mean(std_values),
                "rvv_ms": statistics.mean(rvv_values),
                "speedup_median": statistics.median(speedups),
                "speedup_min": min(speedups),
                "speedup_max": max(speedups),
                "speedup_p10": percentile(speedups, 0.10),
                "speedup_p90": percentile(speedups, 0.90),
                "decision_bucket": decision_bucket(speedups),
                "below_one": below_one,
                "checksum_std": unique_or_mixed(std_checksums),
                "checksum_rvv": unique_or_mixed(rvv_checksums),
                "rvv_chunks": unique_or_mixed(rvv_chunks),
                "keypoints": unique_or_mixed(keypoints),
                "metadata": side_metadata(case_name, build="rvv", ms_values=rvv_values, checksums=rvv_checksums),
            }
        )

    if not comparisons:
        raise SystemExit("no benchmark comparisons parsed")

    summary_output.parent.mkdir(parents=True, exist_ok=True)
    summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "title": args.title,
        "run_label": args.run_label,
        "summary_role": args.summary_role,
        "evidence_role": args.evidence_role,
        "case_kind": args.case_kind,
        "ab_boundary": args.ab_boundary,
        "point_type": args.point_type,
        "timer_boundary": args.timer_boundary,
        "baseline_asm_boundary": args.baseline_asm_boundary,
        "asm_boundary": args.asm_boundary,
        "notes": args.notes,
        "expected_runs": args.expected_runs,
        "collected_runs": len(runs),
        "asm_rvv_line_count": asm_lines,
        "binary_hashes": {"std": std_binary_hash, "rvv": rvv_binary_hash},
        "comparisons": comparisons,
        "summary_paths": [str(path / "analyze_bench_compare.log") for path, _, _ in runs],
        "summary_output": str(summary_output),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
