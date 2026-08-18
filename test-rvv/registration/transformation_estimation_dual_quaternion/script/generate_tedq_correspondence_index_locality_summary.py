#!/usr/bin/env python3
"""生成 correspondence index locality 消融的板卡摘要和 Evidence manifest。"""

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

PATTERNS = ("contiguous", "local-window", "strided")
SIZES = ("4K", "64K", "256K")


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
    runs = []
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


def case_label(pattern: str, suffix: str) -> str:
    return f"direct index stream candidate correspondence-pair {pattern} {suffix}"


def build_metrics(runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    metrics: dict[str, dict[str, Any]] = {}
    for suffix in SIZES:
        rows: dict[str, list[dict[str, Any]]] = {pattern: [] for pattern in PATTERNS}
        for run in runs:
            cases = run["rvv"]["cases"]
            for pattern in PATTERNS:
                label = case_label(pattern, suffix)
                if label not in cases:
                    raise SystemExit(f"missing {label} in {run['name']}")
                rows[pattern].append(cases[label])
        strided_ms = [float(row["ms"]) for row in rows["strided"]]
        comparisons = {}
        for pattern in ("contiguous", "local-window"):
            values = [
                baseline["ms"] / candidate["ms"]
                for baseline, candidate in zip(
                    rows["strided"], rows[pattern], strict=True
                )
            ]
            comparisons[pattern] = {
                "values": values,
                "median": statistics.median(values),
                "min": min(values),
                "max": max(values),
                "bucket": bucket_for(values),
            }
        metrics[suffix] = {
            "size": int(suffix.rstrip("K")) * 1024,
            "patterns": {
                pattern: {
                    "values": [float(row["ms"]) for row in rows[pattern]],
                    "median_ms": statistics.median(float(row["ms"]) for row in rows[pattern]),
                    "checksums": sorted({str(row.get("checksum")) for row in rows[pattern]}),
                }
                for pattern in PATTERNS
            },
            "comparisons": comparisons,
            "strided_ms": strided_ms,
        }
    return metrics


def side(
    boundary: str,
    pattern: str,
    value_key: str,
    value: float,
    checksum: list[str],
    output_checksum: list[str],
) -> dict[str, Any]:
    return {
        "boundary": boundary,
        "wrapper": "bench_tedq",
        "row_source": "correspondence-pair",
        "point_type": "PointXYZ",
        "scalar": "float",
        "pattern": pattern,
        "solve": True,
        "checksum_policy": "input_corpus_identity_matrix_correctness_checked_by_qemu_and_gtest",
        "checksum": checksum,
        "output_checksum": output_checksum,
        "timer_boundary": "correspondence_row_ingress_plus_c1_c2_accumulation_plus_eigen_4x4_solve",
        "gate": "dense_correspondence_pair_pointxyz_float_xyz_aos",
        "mask": "not_applicable_dense_only",
        "reduction": "rvv_f64_tree",
        "asm_boundary": "bench_binary_correspondence_direct_index_stream",
        value_key: value,
    }


def build_manifest(
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    summary_path: Path,
    args: argparse.Namespace,
) -> dict[str, Any]:
    comparisons = []
    for suffix, info in metrics.items():
        for pattern in ("contiguous", "local-window"):
            comparison = info["comparisons"][pattern]
            input_checksum = [f"correspondence-pattern-corpus:{suffix}:PointXYZ:float"]
            comparisons.append(
                {
                    "name": f"correspondence strided-vs-{pattern} index locality {suffix}",
                    "group": "tedq_correspondence_index_locality",
                    "case_kind": "input_distribution_ablation",
                    "evidence_role": "diagnostic",
                    "row_source": "correspondence-pair",
                    "point_type": "PointXYZ",
                    "scalar": "float",
                    "size": info["size"],
                    "run_count": len(runs),
                    "ba_values": comparison["values"],
                    "decision_bucket": comparison["bucket"],
                    "std_ms": info["patterns"]["strided"]["median_ms"],
                    "rvv_ms": info["patterns"][pattern]["median_ms"],
                    "allowed_contract_mismatches": ["input_distribution"],
                    "notes": (
                        "B/A = strided Phase 008 direct index-stream RVV ms / "
                        f"{pattern} direct index-stream RVV ms. Only query/match index "
                        "distribution changes; formula, gather implementation, solve and "
                        "input corpus identity remains fixed. Output matrix fingerprints "
                        "are retained separately because reduction order can change them."
                    ),
                    "baseline": side(
                        "test_support_correspondence_direct_index_stream",
                        "strided",
                        "std_ms",
                        info["patterns"]["strided"]["median_ms"],
                        input_checksum,
                        info["patterns"]["strided"]["checksums"],
                    ),
                    "candidate": side(
                        "test_support_correspondence_direct_index_stream",
                        pattern,
                        "rvv_ms",
                        info["patterns"][pattern]["median_ms"],
                        input_checksum,
                        info["patterns"][pattern]["checksums"],
                    ),
                }
            )
    first_meta = runs[0]["rvv"]["metadata"]
    return {
        "schema_version": 1,
        "summary": {
            "title": "transformation_estimation_dual_quaternion correspondence index locality ablation",
            "evidence_role": "diagnostic",
            "summary_path": str(summary_path),
            "label": "Phase 008 direct index stream under contiguous, local-window and strided correspondence patterns",
            "analysis_script": (
                "test-rvv/registration/transformation_estimation_dual_quaternion/"
                "script/generate_tedq_correspondence_index_locality_summary.py"
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
            },
        },
        "checks": {
            "strict_ab": False,
            "equal_fields": [
                "wrapper",
                "row_source",
                "point_type",
                "scalar",
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
        "# transformation_estimation_dual_quaternion correspondence index locality summary",
        "",
        "## 运行合同",
        "",
        f"- run_label：`{args.run_label}`",
        f"- runs：`{', '.join(run['name'] for run in runs)}`",
        f"- device：`{args.device_label}`",
        f"- case-filter：`{args.case_filter}`",
        "- evidence_role：`diagnostic`；只覆盖 correspondence-pair，不是 production direct。",
        f"- iterations：`{runs[0]['rvv']['metadata'].get('iterations')}`",
        f"- warm-up iterations：`{runs[0]['rvv']['metadata'].get('warmup_iterations')}`",
        "- B/A：strided Phase 008 direct index stream RVV ms / alternative pattern RVV ms；大于 1 表示 alternative pattern 更快。",
        "- 三种 pattern 共用 direct index stream、C1/C2 公式、Eigen 4x4 solve 和输入 corpus identity；输出 fingerprint 单独保留。",
        f"- manifest：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
        "",
        "| size | pattern | median ms | checksum |",
        "| --- | --- | ---: | --- |",
    ]
    for suffix, info in metrics.items():
        for pattern in PATTERNS:
            pattern_info = info["patterns"][pattern]
            checksum = (
                "stable output fingerprint"
                if len(pattern_info["checksums"]) == 1
                else str(pattern_info["checksums"])
            )
            lines.append(
                f"| {suffix} | `{pattern}` | {pattern_info['median_ms']:.4f} | {checksum} |"
            )
        for pattern in ("contiguous", "local-window"):
            comparison = info["comparisons"][pattern]
            lines.append(
                f"| {suffix} | `strided-vs-{pattern}` | "
                f"median B/A `{comparison['median']:.3f}x`，"
                f"min `{comparison['min']:.3f}x`，max `{comparison['max']:.3f}x`，"
                f"bucket `{comparison['bucket']}` | input corpus identity same；output fingerprint intentionally separate |"
            )
    lines.extend(
        [
            "",
            "## 决策边界",
            "",
            "- 这是输入分布消融，不是 production direct 性能结论；`input_distribution` 是有意的比较差异。",
            "- 跨 pattern 的可比 checksum 是 input corpus identity；不同 output fingerprint 由 reduction order / floating-point rounding 产生，必须结合 correctness 解释。",
            "- QEMU 只用于 correctness、checksum 和日志形状；性能方向只看目标板卡 repeated。",
            "- 若只有某一 pattern positive，最多保留 locality-sensitive diagnostic hypothesis，不自动修改 production。",
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
