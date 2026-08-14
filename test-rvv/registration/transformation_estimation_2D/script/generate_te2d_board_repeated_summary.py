#!/usr/bin/env python3
"""
生成 transformation_estimation_2D 的 board repeated summary（板卡重复测试摘要）。

输入是一组 `run-*` 目录。每个目录包含 Std / RVV bench log，来自同一
`ordered-cloud-pair-fused` case-filter。脚本输出：

- `summary.md`：给 reviewer 阅读的板卡重复摘要。
- `evidence_manifest.json`：给全局 Evidence Doctor（证据体检）读取的 manifest。

本脚本处理两类当前 topic 证据：pre-production diagnostic（接入生产前诊断）和
production-public probe（生产公开入口探针）。后者只说明某次生产接入闭环中的公开入口
bench 结果，是否保留 production patch 仍由 PI5 EvidenceDecision 决定。
"""

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
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")


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
        if match := BUILD_RE.match(line):
            metadata["build"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["log_checksum"] = match.group("checksum")
            current_case = None
    return {"path": str(path), "metadata": metadata, "cases": cases}


def run_dirs(root: Path) -> list[Path]:
    return sorted(path for path in root.iterdir() if path.is_dir() and path.name.startswith("run-"))


def collect_runs(root: Path, expected_runs: int) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    for directory in run_dirs(root):
        std_path = directory / "run_bench_std.log"
        rvv_path = directory / "run_bench_rvv.log"
        analyze_path = directory / "analyze_bench_compare.log"
        if not std_path.exists() or not rvv_path.exists():
            raise SystemExit(f"missing Std/RVV logs under {directory}")
        std = parse_log(std_path)
        rvv = parse_log(rvv_path)
        missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
        if missing:
            raise SystemExit(f"case set mismatch under {directory}: {', '.join(missing)}")
        runs.append(
            {
                "name": directory.name,
                "std": std,
                "rvv": rvv,
                "analyze": str(analyze_path) if analyze_path.exists() else "missing",
            }
        )
    if len(runs) != expected_runs:
        raise SystemExit(f"expected {expected_runs} run-* dirs under {root}, got {len(runs)}")
    return runs


def size_suffix(label: str) -> str:
    match = re.search(r"(\d+K)", label)
    if not match:
        raise ValueError(f"cannot infer size suffix from case label: {label}")
    return match.group(1)


def size_value(suffix: str) -> int:
    return int(suffix.rstrip("K")) * 1024


def fused_case(size: str, row_source: str = "ordered-cloud-pair") -> str:
    if row_source == "source-indexed-cloud-pair":
        return f"fused 2D correlation source-indexed-cloud-pair {size}"
    if row_source == "dual-indexed-cloud-pair":
        return f"fused 2D correlation dual-indexed-cloud-pair {size}"
    if row_source == "correspondence-pair":
        return f"fused 2D correlation correspondence-pair {size}"
    return f"fused 2D correlation ordered-cloud-pair {size}"


def case_label(size: str, case_filter: str) -> str:
    if case_filter == "ordered-cloud-pair-public":
        return f"public 2D ordered-cloud-pair {size}"
    return fused_case(size)


def row_source_for_label(label: str) -> str:
    if "source-indexed-cloud-pair" in label:
        return "source_indexed_cloud_pair"
    if "dual-indexed-cloud-pair" in label:
        return "dual_indexed_cloud_pair"
    if "correspondence-pair" in label:
        return "correspondence_pair"
    return "ordered_cloud_pair"


def case_specs(case_filter: str, first_names: list[str]) -> list[tuple[str, str, str]]:
    sizes = sorted({size_suffix(name) for name in first_names}, key=size_value)
    if case_filter != "row-source-fused":
        return [
            (
                case_label(suffix, case_filter),
                suffix,
                row_source_for_label(case_label(suffix, case_filter)),
            )
            for suffix in sizes
        ]
    return [
        (
            fused_case(suffix, row_source),
            suffix,
            row_source_for_label(fused_case(suffix, row_source)),
        )
        for row_source in (
            "source-indexed-cloud-pair",
            "dual-indexed-cloud-pair",
            "correspondence-pair",
        )
        for suffix in sizes
    ]


def case_group(case_filter: str) -> str:
    if case_filter == "ordered-cloud-pair-public":
        return "te2d_ordered_cloud_pair_public"
    if case_filter == "row-source-fused":
        return "te2d_row_source_fused"
    return "te2d_ordered_cloud_pair_fused"


def case_kind(case_filter: str) -> str:
    if case_filter == "ordered-cloud-pair-public":
        return "board_production_public_same_boundary"
    if case_filter == "row-source-fused":
        return "board_diagnostic_row_source_same_boundary"
    return "board_diagnostic_same_boundary"


def summary_title(case_filter: str) -> str:
    if case_filter == "ordered-cloud-pair-public":
        return "transformation_estimation_2D ordered-cloud-pair production public repeated board"
    if case_filter == "row-source-fused":
        return "transformation_estimation_2D row-source repeated board diagnostic"
    return "transformation_estimation_2D ordered-cloud-pair repeated board diagnostic"


def summary_label(case_filter: str) -> str:
    if case_filter == "ordered-cloud-pair-public":
        return "post-production public dispatch evidence"
    if case_filter == "row-source-fused":
        return "row-source diagnostic; materialize-to-ordered candidate; no production dispatch"
    return "pre-production diagnostic; no production dispatch"


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


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def metric(values: list[float]) -> dict[str, Any]:
    return {
        "values": values,
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
        "p10": percentile(values, 0.10),
        "p90": percentile(values, 0.90),
        "bucket": bucket_for(values),
        "below_one_count": sum(1 for value in values if value < 1.0),
    }


def build_case_metrics(runs: list[dict[str, Any]], case_filter: str) -> dict[str, dict[str, Any]]:
    first_names = sorted(runs[0]["std"]["cases"])
    result: dict[str, dict[str, Any]] = {}
    for label, suffix, row_source in case_specs(case_filter, first_names):
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        std_checksums: set[str] = set()
        rvv_checksums: set[str] = set()
        for run in runs:
            std_case = run["std"]["cases"][label]
            rvv_case = run["rvv"]["cases"][label]
            std_ms = float(std_case["ms"])
            rvv_ms = float(rvv_case["ms"])
            values.append(std_ms / rvv_ms)
            std_checksums.add(str(std_case.get("log_checksum")))
            rvv_checksums.add(str(rvv_case.get("log_checksum")))
            rows.append(
                {
                    "run": run["name"],
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    "ba": std_ms / rvv_ms,
                }
            )
        result[label] = {
            "key": label,
            "size": size_value(suffix),
            "label": label,
            "row_source": row_source,
            "metric": metric(values),
            "runs": rows,
            "checksums": {
                "std_log": sorted(std_checksums),
                "rvv_log": sorted(rvv_checksums),
            },
        }
    return result


def overall_decision(case_metrics: dict[str, dict[str, Any]]) -> str:
    target = [info["metric"] for info in case_metrics.values() if info["size"] != 4 * 1024]
    if not target:
        target = [info["metric"] for info in case_metrics.values()]
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


def side(label: str,
         reduction: str,
         asm_boundary: str,
         ms_key: str,
         ms_value: float,
         args: argparse.Namespace,
         row_source: str) -> dict[str, Any]:
    if args.case_filter == "ordered-cloud-pair-public":
        boundary = "production_public_ordered_cloud_pair_overload"
        timer_boundary = "public_ordered_cloud_pair_estimate_plus_2d_solve"
        gate = "exact_pointxyz_float_dense_finite_ordered_cloud_pair_size_ge_16_or_scalar_fallback"
    elif args.case_filter == "row-source-fused":
        boundary = "test_support_row_source_materialize_to_ordered_candidate"
        timer_boundary = "row_source_materialize_plus_two_pass_centered_correlation_plus_2d_solve"
        gate = "valid_indices_or_correspondences_pointxyz_float_dense_finite_size_ge_16_or_scalar_fallback"
    else:
        boundary = "test_support_fused_2d_correlation_candidate"
        timer_boundary = "two_pass_centered_correlation_plus_2d_solve"
        gate = "dense_finite_pointxyz_float_ordered_cloud_pair_size_ge_16_or_scalar_fallback"
    return {
        "label": label,
        "boundary": boundary,
        "wrapper": "bench_te2d",
        "row_source": row_source,
        "solve": True,
        "checksum_policy": "matrix_path_fingerprint_plus_rvv_gate_flag_correctness_checked_by_gtest",
        "timer_boundary": timer_boundary,
        "gate": gate,
        "mask": "dense_finite_input_prechecked",
        "reduction": reduction,
        "asm_boundary": asm_boundary,
        ms_key: ms_value,
    }


def build_manifest(runs: list[dict[str, Any]],
                   case_metrics: dict[str, dict[str, Any]],
                   summary_path: Path,
                   args: argparse.Namespace) -> dict[str, Any]:
    first_meta = runs[0]["std"]["metadata"]
    binary_hash = (
        f"std={sha256_file(Path(args.std_bin))}; rvv={sha256_file(Path(args.rvv_bin))}"
    )
    comparisons: list[dict[str, Any]] = []
    for suffix, info in case_metrics.items():
        values = info["metric"]["values"]
        std_med = statistics.median(row["std_ms"] for row in info["runs"])
        rvv_med = statistics.median(row["rvv_ms"] for row in info["runs"])
        if args.case_filter == "ordered-cloud-pair-public":
            baseline_label = "Std build public ordered-cloud-pair"
            candidate_label = "RVV build public ordered-cloud-pair"
            baseline_reduction = "scalar_iterator_centroid_demean_correlation"
        elif args.case_filter == "row-source-fused":
            baseline_label = "Std build row-source materialize-to-ordered candidate"
            candidate_label = "RVV build row-source materialize-to-ordered candidate"
            baseline_reduction = "scalar_materialize_then_two_pass_centered_sum"
        else:
            baseline_label = "Std build fused candidate"
            candidate_label = "RVV build fused candidate"
            baseline_reduction = "scalar_two_pass_centered_sum"
        comparisons.append(
            {
                "name": info["label"],
                "group": case_group(args.case_filter),
                "case_kind": case_kind(args.case_filter),
                "evidence_role": "strict_ab",
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "row_source": info["row_source"],
                "run_count": len(runs),
                "ba_values": values,
                "decision_bucket": info["metric"]["bucket"],
                "std_ms": std_med,
                "rvv_ms": rvv_med,
                "allowed_contract_mismatches": [
                    "reduction",
                    "asm_boundary",
                ],
                "baseline": side(
                    baseline_label,
                    baseline_reduction,
                    "not_applicable_std_build",
                    "std_ms",
                    std_med,
                    args,
                    info["row_source"],
                ),
                "candidate": side(
                    candidate_label,
                    "rvv_materialize_then_ordered_sum_tree"
                    if args.case_filter == "row-source-fused"
                    else "rvv_ordered_sum_tree",
                    "runPublicCase_lambda_production_public_boundary"
                    if args.case_filter == "ordered-cloud-pair-public"
                    else (
                        "runRowSourceCase_lambda_test_support_boundary"
                        if args.case_filter == "row-source-fused"
                        else "runFusedCase_lambda_test_support_boundary"
                    ),
                    "rvv_ms",
                    rvv_med,
                    args,
                    info["row_source"],
                ),
            }
        )

    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title(args.case_filter),
            "evidence_role": "strict_ab",
            "summary_path": str(summary_path),
            "label": summary_label(args.case_filter),
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": {
                "device": args.device_label,
                "iterations": first_meta.get("iterations"),
                "warmup_iterations": first_meta.get("warmup_iterations"),
                "run_count": len(runs),
                "taskset": args.taskset,
                "governor": args.governor,
                "freq": args.freq,
                "temperature": args.temperature,
                "vlen": args.vlen_desc,
                "binary_hash": binary_hash,
                "dataset": first_meta.get("dataset"),
                "case_filter": args.case_filter,
                "row_source": (
                    "mixed_row_sources"
                    if args.case_filter == "row-source-fused"
                    else next(iter(case_metrics.values()))["row_source"]
                ),
                "decision_bucket": overall_decision(case_metrics),
            },
        },
        "checks": {
            "strict_ab": True,
        },
        "comparisons": comparisons,
    }


def format_values(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}x" for value in values)


def render_summary(case_metrics: dict[str, dict[str, Any]], args: argparse.Namespace) -> str:
    lines: list[str] = []
    lines.append("# transformation_estimation_2D Board Repeated Summary")
    lines.append("")
    lines.append("## 运行合同")
    lines.append("")
    if args.case_filter == "ordered-cloud-pair-public":
        evidence_role = "post-production public dispatch（接入生产后公开入口证据）"
    elif args.case_filter == "row-source-fused":
        evidence_role = "row-source diagnostic（行来源诊断）"
    else:
        evidence_role = "pre-production diagnostic（接入生产前诊断）"
    lines.append(f"- evidence_role：{evidence_role}")
    lines.append(f"- run_label：`{args.run_label}`")
    lines.append(f"- case_filter：`{args.case_filter}`")
    lines.append(f"- expected_runs：{args.expected_runs}")
    lines.append(f"- device：{args.device_label}")
    lines.append("")
    lines.append("`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。")
    if args.case_filter == "ordered-cloud-pair-public":
        lines.append("本摘要用于 PI4 production public evidence；QEMU timing 仍不参与性能结论。")
    elif args.case_filter == "row-source-fused":
        lines.append("本摘要包含 materialize-to-ordered 展开成本；它只用于 row-source 诊断，不证明 production dispatch。")
    else:
        lines.append("本摘要不证明 production dispatch；production 接入需要 PI1-PI5 证据闭环。")
    lines.append("")
    lines.append("## 结果")
    lines.append("")
    lines.append("| case | runs | median | min | max | p10 | p90 | B/A<1 | bucket | values |")
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |")
    for _, info in sorted(
        case_metrics.items(), key=lambda item: (item[1]["row_source"], item[1]["size"])
    ):
        metric_info = info["metric"]
        lines.append(
            f"| `{info['label']}` | {len(metric_info['values'])} | "
            f"{metric_info['median']:.3f}x | {metric_info['min']:.3f}x | "
            f"{metric_info['max']:.3f}x | {metric_info['p10']:.3f}x | "
            f"{metric_info['p90']:.3f}x | {metric_info['below_one_count']} | "
            f"`{metric_info['bucket']}` | {format_values(metric_info['values'])} |"
        )
    lines.append("")
    lines.append("## Checksum 边界")
    lines.append("")
    lines.append("bench log checksum 包含 RVV gate flag（路径标记），因此 Std / RVV log checksum 不要求相同。")
    lines.append("数值正确性主证据仍是 `run_test_compare` 和 near-cancellation correctness corpus。")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--run-label", default="ordered_cloud_pair_repeated")
    parser.add_argument("--case-filter", default="ordered-cloud-pair-fused")
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--std-bin", required=True)
    parser.add_argument("--rvv-bin", required=True)
    parser.add_argument("--device-label", default="RVV board")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    parser.add_argument("--taskset", default="not_recorded_board_target")
    parser.add_argument("--governor", default="not_recorded_board_target")
    parser.add_argument("--freq", default="not_recorded_board_target")
    parser.add_argument("--temperature", default="not_recorded_board_target")
    args = parser.parse_args()

    runs = collect_runs(args.input_dir, args.expected_runs)
    metrics = build_case_metrics(runs, args.case_filter)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = args.output_dir / "summary.md"
    manifest_path = args.output_dir / "evidence_manifest.json"
    summary_path.write_text(render_summary(metrics, args), encoding="utf-8")
    manifest = build_manifest(runs, metrics, summary_path, args)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
