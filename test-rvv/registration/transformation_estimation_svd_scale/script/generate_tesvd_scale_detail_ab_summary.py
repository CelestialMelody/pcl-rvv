#!/usr/bin/env python3
"""
Generate RVV-vs-RVV detail A/B summaries for transformation_estimation_svd_scale.

The input is either one QEMU RVV bench log or a board repeated directory whose
run-* children contain RVV bench logs. The default B/A value is current RVV ms
divided by sorted-copy RVV ms; callers can override the paired label prefixes
for later mitigation families such as staged selected-cloud.
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
MAX_ERROR_RE = re.compile(
    r"max_reference_error:\s*(?P<value>[0-9]+(?:\.[0-9]+)?(?:e[+-]?[0-9]+)?)",
    re.IGNORECASE,
)
SIZE_RE = re.compile(r"\b(?P<size>\d+)K\b")
ITERATIONS_RE = re.compile(r"^Iterations:\s*(?P<value>\d+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>\d+)\s*$")


def sha256_file(path: Path) -> str:
    if not path.is_file():
      return "not_recorded"
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def parse_log(path: Path) -> dict[str, dict[str, Any]]:
    cases: dict[str, dict[str, Any]] = {}
    current_case: str | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
        if current_case and (match := MAX_ERROR_RE.search(line)):
            cases[current_case]["max_reference_error"] = float(match.group("value"))
            current_case = None
    return cases


def parse_run_metadata(path: Path) -> dict[str, Any]:
    metadata: dict[str, Any] = {}
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := ITERATIONS_RE.match(line):
            metadata["iterations"] = int(match.group("value"))
        elif match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
    return metadata


def run_logs(input_path: Path) -> list[tuple[str, Path]]:
    if input_path.is_file():
        return [("qemu", input_path)]
    logs: list[tuple[str, Path]] = []
    for directory in sorted(input_path.iterdir()):
        if directory.is_dir() and directory.name.startswith("run-"):
            log = directory / "run_bench_rvv.log"
            if not log.exists():
                raise SystemExit(f"missing RVV log under {directory}")
            logs.append((directory.name, log))
    if not logs:
        raise SystemExit(f"no RVV logs found in {input_path}")
    return logs


def metric(values: list[float]) -> dict[str, Any]:
    med = statistics.median(values)
    if all(v > 1.20 for v in values):
        bucket = "positive"
    elif med >= 1.05 and min(values) >= 0.97:
        bucket = "weak_positive"
    elif all(0.97 <= v <= 1.03 for v in values):
        bucket = "neutral"
    elif med < 0.97 or min(values) < 0.97:
        bucket = "negative"
    else:
        bucket = "unstable"
    return {
        "values": values,
        "median": med,
        "min": min(values),
        "max": max(values),
        "bucket": bucket,
    }


def paired_key(label: str, prefix: str) -> str:
    key = label.removeprefix(prefix).strip()
    if not key:
        raise ValueError(f"empty detail A/B key from {label}")
    return key


def size_from_key(key: str) -> int:
    match = SIZE_RE.search(key)
    if not match:
        raise ValueError(f"cannot infer size from detail A/B key: {key}")
    return int(match.group("size")) * 1024


def collect_metrics(args: argparse.Namespace,
                    logs: list[tuple[str, Path]]) -> dict[str, dict[str, Any]]:
    rows_by_key: dict[str, list[dict[str, Any]]] = {}
    for run_name, path in logs:
        cases = parse_log(path)
        current = {
            paired_key(name, args.baseline_prefix): item
            for name, item in cases.items()
            if name.startswith(args.baseline_prefix)
        }
        candidate = {
            paired_key(name, args.candidate_prefix): item
            for name, item in cases.items()
            if name.startswith(args.candidate_prefix)
        }
        missing = sorted(set(current) ^ set(candidate))
        if missing:
            raise SystemExit(f"detail A/B pair mismatch in {path}: {', '.join(missing)}")
        for key in sorted(current):
            rows_by_key.setdefault(key, []).append(
                {
                    "run": run_name,
                    "current_ms": float(current[key]["ms"]),
                    "candidate_ms": float(candidate[key]["ms"]),
                    "ba": float(current[key]["ms"]) / float(candidate[key]["ms"]),
                    "current_checksum": current[key].get("checksum", "not_recorded"),
                    "candidate_checksum": candidate[key].get("checksum", "not_recorded"),
                    "max_reference_error": candidate[key].get("max_reference_error"),
                }
            )
    result: dict[str, dict[str, Any]] = {}
    for key, rows in rows_by_key.items():
        result[key] = {
            "key": key,
            "size": size_from_key(key),
            "metric": metric([row["ba"] for row in rows]),
            "rows": rows,
            "max_reference_error": max(
                [row["max_reference_error"] for row in rows if row["max_reference_error"] is not None],
                default=None,
            ),
            "checksum_match": {
                str(row["current_checksum"]) == str(row["candidate_checksum"])
                for row in rows
            }
            == {True},
        }
    return result


def overall_bucket(metrics: dict[str, dict[str, Any]]) -> str:
    buckets = {item["metric"]["bucket"] for item in metrics.values()}
    if buckets == {"positive"}:
        return "positive"
    if "negative" in buckets:
        return "negative"
    if "unstable" in buckets:
        return "unstable"
    if "weak_positive" in buckets:
        return "weak_positive"
    return "neutral"


def write_summary(path: Path, args: argparse.Namespace, metrics: dict[str, dict[str, Any]]) -> None:
    lines = [
        "# transformation_estimation_svd_scale RVV detail A/B summary",
        "",
        "## 运行合同",
        "",
        f"- run label: `{args.run_label}`",
        "- evidence role: `production_detail_rvv_vs_rvv`",
        f"- B/A = current RVV ms / {args.candidate_name} RVV ms；大于 1 表示 {args.candidate_name} mitigation 更快。",
        f"- {args.candidate_name} case 的计时边界：{args.candidate_timer_note}",
        "",
        "## 结果表",
        "",
        "| case | runs B/A | median | min | max | bucket | max ref error | log checksum |",
        "| --- | --- | ---: | ---: | ---: | --- | ---: | --- |",
    ]
    for key, item in sorted(metrics.items(), key=lambda kv: (kv[1]["size"], kv[0])):
        m = item["metric"]
        values = ", ".join(f"{value:.3f}" for value in m["values"])
        max_error = item["max_reference_error"]
        max_error_text = "n/a" if max_error is None else f"{max_error:.3e}"
        checksum = "match" if item["checksum_match"] else "mismatch"
        lines.append(
            f"| `{key}` | {values} | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` | {max_error_text} | `{checksum}` |"
        )
    lines += [
        "",
        "## Evidence Doctor 输入说明",
        "",
        f"- overall decision bucket: `{overall_bucket(metrics)}`",
        "- 本 summary 是 production detail RVV-vs-RVV evidence，不替代 public Std/RVV production adoption evidence。",
        "- raw run logs 默认 local-only；提交前需按 evidence policy 单独审查。",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def build_manifest(args: argparse.Namespace,
                   logs: list[tuple[str, Path]],
                   metrics: dict[str, dict[str, Any]]) -> dict[str, Any]:
    first_metadata = parse_run_metadata(logs[0][1])
    comparisons = []
    for key, item in metrics.items():
        row_source = args.row_source_policy or key
        m = item["metric"]
        comparisons.append(
            {
                "name": key,
                "group": args.group,
                "evidence_role": "production_detail_rvv_vs_rvv",
                "case_kind": "board_detail_rvv_vs_rvv" if len(logs) > 1 else "qemu_detail_smoke",
                "point_type": "PointXYZ",
                "size": item["size"],
                "run_count": len(logs),
                "allowed_contract_mismatches": [
                    "wrapper",
                    "timer_boundary",
                    "reduction",
                ],
                "max_reference_error": {
                    "rvv": item["max_reference_error"],
                    "tolerance": 2.0e-3,
                },
                "baseline": {
                    "label": args.baseline_label,
                    "boundary": args.boundary,
                    "wrapper": args.baseline_wrapper,
                    "row_source": row_source,
                    "solve": args.solve,
                    "checksum_policy": "tolerance_checked_by_max_reference_error_not_bit_identical",
                    "timer_boundary": args.baseline_timer_boundary,
                    "gate": args.gate,
                    "mask": args.mask,
                    "reduction": args.baseline_reduction,
                    "asm_boundary": args.baseline_asm_boundary,
                    "rvv_ms": statistics.median([row["current_ms"] for row in item["rows"]]),
                },
                "candidate": {
                    "label": args.candidate_label,
                    "boundary": args.boundary,
                    "wrapper": args.candidate_wrapper,
                    "row_source": row_source,
                    "solve": args.solve,
                    "checksum_policy": "tolerance_checked_by_max_reference_error_not_bit_identical",
                    "timer_boundary": args.candidate_timer_boundary,
                    "gate": args.gate,
                    "mask": args.mask,
                    "reduction": args.candidate_reduction,
                    "asm_boundary": args.candidate_asm_boundary,
                    "rvv_ms": statistics.median([row["candidate_ms"] for row in item["rows"]]),
                },
                "ba_values": m["values"],
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": "production_detail_rvv_vs_rvv",
            "summary_path": str(Path(args.output_dir) / "summary.md"),
            "label": args.run_label,
            "analysis_script": "test-rvv/registration/transformation_estimation_svd_scale/script/generate_tesvd_scale_detail_ab_summary.py",
                "metadata": {
                    "device": "board" if len(logs) > 1 else "qemu",
                    "iterations": first_metadata.get("iterations"),
                    "warmup_iterations": first_metadata.get("warmup_iterations"),
                    "run_count": len(logs),
                    "taskset": "not_recorded",
                    "governor": "not_recorded",
                    "freq": "not_recorded",
                    "temperature": "not_recorded",
                    "vlen": "qemu rv64gcv" if len(logs) == 1 else "see SoC / ELF (board)",
                    "binary_hash": {
                        "rvv": sha256_file(Path(args.rvv_bin)) if args.rvv_bin else "not_recorded",
                    },
            },
        },
        "checks": {
            "strict_ab": True,
            "equal_fields": [
                "boundary",
                "row_source",
                "solve",
                "checksum_policy",
                "gate",
                "mask",
            ],
            "thresholds": {
                "min_repeated_runs": len(logs),
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
                "near_threshold_margin": 0.05,
            },
        },
        "comparisons": comparisons,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--rvv-bin", default="")
    parser.add_argument("--baseline-prefix", default="row source shuffle current")
    parser.add_argument("--candidate-prefix", default="row source shuffle sorted-copy")
    parser.add_argument("--candidate-name", default="sorted-copy")
    parser.add_argument("--baseline-label", default="rvv-current-row-source-shuffle")
    parser.add_argument("--candidate-label", default="rvv-sorted-copy-row-source-shuffle")
    parser.add_argument("--baseline-wrapper", default="same_outer_wrapper")
    parser.add_argument("--candidate-wrapper", default="same_outer_wrapper_with_sorted_copy")
    parser.add_argument("--baseline-timer-boundary", default="estimate_only")
    parser.add_argument("--candidate-timer-boundary", default="copy_sort_plus_estimate")
    parser.add_argument("--candidate-timer-note", default="包含 index/correspondence copy 与 sort。")
    parser.add_argument("--baseline-reduction", default="rvv_current_gather_scale_accumulation")
    parser.add_argument("--candidate-reduction", default="rvv_sorted_copy_gather_scale_accumulation")
    parser.add_argument("--baseline-asm-boundary", default="production row-source public overloads")
    parser.add_argument("--candidate-asm-boundary", default="production row-source public overloads")
    parser.add_argument("--boundary", default="production_public_row_source_detail")
    parser.add_argument("--row-source-policy", default="")
    parser.add_argument("--solve", default="svd_scale_public_solve")
    parser.add_argument("--gate", default="row_source_dense_traits_gated_xyz_aos_float")
    parser.add_argument("--mask", default="none_dense_only")
    parser.add_argument("--group", default="row-source-shuffle-sorted-copy-detail-ab")
    parser.add_argument(
        "--title",
        default="transformation_estimation_svd_scale row-source shuffle sorted-copy detail A/B",
    )
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    logs = run_logs(Path(args.input))
    metrics = collect_metrics(args, logs)
    write_summary(output_dir / "summary.md", args, metrics)
    manifest = build_manifest(args, logs, metrics)
    (output_dir / "evidence_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
