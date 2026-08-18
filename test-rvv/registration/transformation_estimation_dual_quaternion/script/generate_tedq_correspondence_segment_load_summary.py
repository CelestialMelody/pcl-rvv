#!/usr/bin/env python3
"""生成 correspondence segment-load 的板卡摘要和 Evidence manifest。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
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
        elif match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
        elif match := DATASET_RE.match(line):
            metadata["dataset"] = match.group("value")
        elif match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
        elif current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None
    return {"metadata": metadata, "cases": cases}


def collect_runs(root: Path, expected_runs: int) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    for directory in sorted(
        path for path in root.iterdir() if path.is_dir() and path.name.startswith("run-")
    ):
        rvv_path = directory / "run_bench_rvv.log"
        if not rvv_path.exists():
            raise SystemExit(f"missing RVV log under {directory}")
        runs.append({"name": directory.name, "rvv": parse_log(rvv_path)})
    if len(runs) != expected_runs:
        raise SystemExit(f"expected {expected_runs} run-* dirs under {root}, got {len(runs)}")
    return runs


def size_suffix(label: str) -> str:
    match = re.search(r"(\d+K)", label)
    if not match:
        raise SystemExit(f"cannot infer size suffix from case label: {label}")
    return match.group(1)


def bucket_for(values: list[float]) -> str:
    if not values:
        return "blocked"
    median = statistics.median(values)
    if all(value > 1.15 for value in values):
        return "positive"
    if median >= 1.03 and min(values) >= 0.97:
        return "weak_positive"
    if all(0.97 <= value <= 1.03 for value in values):
        return "neutral"
    if median < 0.97 or min(values) < 0.97:
        return "negative"
    return "unstable"


def build_metrics(runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    baseline_prefix = "direct index stream candidate correspondence-pair"
    labels = sorted(
        {
            name
            for name in runs[0]["rvv"]["cases"]
            if name.startswith(baseline_prefix)
        }
    )
    if not labels:
        raise SystemExit(f"no {baseline_prefix} cases found")

    metrics: dict[str, dict[str, Any]] = {}
    for baseline_label in labels:
        suffix = size_suffix(baseline_label)
        candidate_label = f"direct segment stream candidate correspondence-pair {suffix}"
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        baseline_checksums: set[str] = set()
        candidate_checksums: set[str] = set()
        for run in runs:
            cases = run["rvv"]["cases"]
            if candidate_label not in cases:
                raise SystemExit(f"missing {candidate_label} in {run['name']}")
            baseline = cases[baseline_label]
            candidate = cases[candidate_label]
            baseline_ms = float(baseline["ms"])
            candidate_ms = float(candidate["ms"])
            values.append(baseline_ms / candidate_ms)
            rows.append(
                {
                    "run": run["name"],
                    "baseline_ms": baseline_ms,
                    "candidate_ms": candidate_ms,
                    "ba": values[-1],
                }
            )
            baseline_checksums.add(str(baseline.get("checksum")))
            candidate_checksums.add(str(candidate.get("checksum")))
        metrics[suffix] = {
            "size": int(suffix.rstrip("K")) * 1024,
            "values": values,
            "median": statistics.median(values),
            "min": min(values),
            "max": max(values),
            "bucket": bucket_for(values),
            "rows": rows,
            "baseline_checksums": sorted(baseline_checksums),
            "candidate_checksums": sorted(candidate_checksums),
        }
    return metrics


def overall_decision(metrics: dict[str, dict[str, Any]]) -> str:
    target = [
        info for suffix, info in metrics.items() if suffix != "4K"
    ] or list(metrics.values())
    buckets = {info["bucket"] for info in target}
    if buckets <= {"positive"}:
        return "positive"
    if buckets <= {"positive", "weak_positive"}:
        return "weak_positive"
    if "negative" in buckets:
        return "negative"
    if "unstable" in buckets:
        return "unstable"
    return "neutral"


def side(
    boundary: str,
    asm_boundary: str,
    value_key: str,
    value: float,
    checksum: list[str],
) -> dict[str, Any]:
    return {
        "boundary": boundary,
        "wrapper": "bench_tedq",
        "row_source": "correspondence-pair",
        "solve": True,
        "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
        "checksum": checksum,
        "timer_boundary": "correspondence_row_ingress_plus_c1_c2_accumulation_plus_eigen_4x4_solve",
        "gate": "dense_correspondence_pair_pointxyz_float_xyz_aos",
        "mask": "not_applicable_dense_only",
        "reduction": "rvv_f64_tree",
        "asm_boundary": asm_boundary,
        value_key: value,
    }


def build_manifest(
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    summary_path: Path,
    args: argparse.Namespace,
) -> dict[str, Any]:
    first_meta = runs[0]["rvv"]["metadata"]
    comparisons = []
    for suffix, info in metrics.items():
        baseline_ms = statistics.median(row["baseline_ms"] for row in info["rows"])
        candidate_ms = statistics.median(row["candidate_ms"] for row in info["rows"])
        comparisons.append(
            {
                "name": f"correspondence direct index-vs-segment stream {suffix}",
                "group": "tedq_correspondence_index_stream_shape",
                "case_kind": "implementation_family_comparison",
                "evidence_role": "strict_ab",
                "row_source": "correspondence-pair",
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "run_count": len(runs),
                "ba_values": info["values"],
                "decision_bucket": info["bucket"],
                "std_ms": baseline_ms,
                "rvv_ms": candidate_ms,
                "allowed_contract_mismatches": ["boundary", "asm_boundary"],
                "notes": (
                    "B/A = Phase 008 direct index-stream RVV ms / segment-load RVV ms. "
                    "Both sides use the same correspondence rows, point gathers, formula, "
                    "solve and checksum policy; only index ingress instruction shape differs."
                ),
                "baseline": side(
                    "test_support_correspondence_direct_index_stream",
                    "bench_binary_correspondence_direct_index_stream",
                    "std_ms",
                    baseline_ms,
                    info["baseline_checksums"],
                ),
                "candidate": side(
                    "test_support_correspondence_segment_index_stream",
                    "bench_binary_correspondence_segment_index_stream",
                    "rvv_ms",
                    candidate_ms,
                    info["candidate_checksums"],
                ),
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": "transformation_estimation_dual_quaternion correspondence segment-load comparison",
            "evidence_role": "diagnostic",
            "summary_path": str(summary_path),
            "label": "correspondence direct index stream vs vlseg3e32 segment stream; test-rvv only",
            "analysis_script": (
                "test-rvv/registration/transformation_estimation_dual_quaternion/"
                "script/generate_tedq_correspondence_segment_load_summary.py"
            ),
            "metadata": {
                "device": args.device_label,
                "iterations": first_meta.get("iterations"),
                "warmup_iterations": first_meta.get("warmup_iterations"),
                "run_count": len(runs),
                "taskset": "not_pinned",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": args.vlen_desc,
                "binary_hash": f"rvv={sha256_file(Path(args.rvv_bin))}",
                "dataset": first_meta.get("dataset"),
                "decision_bucket": overall_decision(metrics),
            },
        },
        "checks": {
            "strict_ab": False,
            "equal_fields": [
                "wrapper",
                "row_source",
                "solve",
                "checksum_policy",
                "timer_boundary",
                "gate",
                "mask",
                "reduction",
            ],
            "thresholds": {"min_repeated_runs": args.expected_runs},
        },
        "runs": [run["name"] for run in runs],
        "artifacts": {
            "input_dir": str(Path(args.input_dir)),
            "summary_path": str(summary_path),
            "rvv_bin": args.rvv_bin,
        },
        "comparisons": comparisons,
    }


def write_summary(
    path: Path,
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    manifest_path: Path,
    doctor_path: Path,
    args: argparse.Namespace,
) -> None:
    lines = [
        "# transformation_estimation_dual_quaternion correspondence segment-load summary",
        "",
        "## 运行合同",
        "",
        f"- run_label：`{args.run_label}`",
        f"- runs：`{', '.join(run['name'] for run in runs)}`",
        f"- device：`{args.device_label}`",
        f"- case-filter：`{args.case_filter}`",
        "- evidence_role：`diagnostic`；只覆盖 `correspondence-pair`，不是 production direct。",
        f"- iterations：`{runs[0]['rvv']['metadata'].get('iterations')}`",
        f"- warm-up iterations：`{runs[0]['rvv']['metadata'].get('warmup_iterations')}`",
        "- B/A：Phase 008 direct index stream RVV ms / segment-load RVV ms，大于 1 表示 segment load 更快。",
        "- 两侧共享同一 RVV binary、C1/C2 公式、Eigen 4x4 solve 和 checksum policy。",
        f"- manifest：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
        "",
        "| size | values | median | min | max | bucket | checksum |",
        "| --- | --- | ---: | ---: | ---: | --- | --- |",
    ]
    for suffix, info in metrics.items():
        checksum = (
            "same"
            if info["baseline_checksums"] == info["candidate_checksums"]
            else f"baseline `{info['baseline_checksums']}` / candidate `{info['candidate_checksums']}`"
        )
        lines.append(
            f"| {suffix} | `{', '.join(f'{value:.3f}' for value in info['values'])}` | "
            f"{info['median']:.3f} | {info['min']:.3f} | {info['max']:.3f} | "
            f"`{info['bucket']}` | {checksum} |"
        )
    lines.extend(
        [
            "",
            "## 决策边界",
            "",
            f"- overall_decision_bucket：`{overall_decision(metrics)}`（默认按 64K / 256K 判断）。",
            "- segment-load 只替换 correspondence index ingress；正向结果不能外推到其它点型、其它 AoS 或 production。",
            "- 64K 若出现长尾，保留 min/median/max，并按 cache、调度、AoS locality、segment-load 和 solver 稀释解释。",
            "- 若 segment-load 无收益或不稳定，保留 Phase 008 direct index stream 作为 baseline。",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-filter", required=True)
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--rvv-bin", required=True)
    parser.add_argument("--device-label", default="RVV board")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    summary_path = output_dir / "summary.md"
    manifest_path = output_dir / "evidence_manifest.json"
    doctor_path = output_dir / "evidence_doctor.md"
    runs = collect_runs(Path(args.input_dir), args.expected_runs)
    metrics = build_metrics(runs)
    manifest = build_manifest(runs, metrics, summary_path, args)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_summary(summary_path, runs, metrics, manifest_path, doctor_path, args)
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
