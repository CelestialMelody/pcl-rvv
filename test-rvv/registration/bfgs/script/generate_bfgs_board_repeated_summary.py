#!/usr/bin/env python3
"""
生成 bfgs direction-update board repeated summary（板卡重复测试摘要）。

输入目录包含若干 `run-*` 子目录，每个子目录来自一次 board Std/RVV bench
compare，至少包含：

- `run_bench_std.log`
- `run_bench_rvv.log`
- 可选 `analyze_bench_compare.log`

输出：

- `summary.md`：给 reviewer 阅读的摘要。
- `evidence_manifest.json`：给 Evidence Doctor（证据体检）读取的清单。
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
CHECKSUM_RE = re.compile(r"^checksum:\s*(?P<checksum>[0-9]+)\s*$")
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
        if current_case and (match := CHECKSUM_RE.match(line)):
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
        std_cases = {name for name in std["cases"] if name.startswith("direction-update")}
        rvv_cases = {name for name in rvv["cases"] if name.startswith("direction-update")}
        missing = sorted(std_cases ^ rvv_cases)
        if missing:
            raise SystemExit(f"direction-update case set mismatch under {directory}: {', '.join(missing)}")
        if not std_cases:
            raise SystemExit(f"no direction-update cases parsed under {directory}")
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


def vector_size(label: str) -> int:
    match = re.search(r"vector(\d+)", label)
    if not match:
        raise ValueError(f"cannot infer vector size from case label: {label}")
    return int(match.group(1))


def point_type(label: str) -> str:
    return "Vector6d" if vector_size(label) == 6 else "dynamic_double_vector"


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


def build_case_metrics(runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    first_names = sorted(name for name in runs[0]["std"]["cases"] if name.startswith("direction-update"))
    result: dict[str, dict[str, Any]] = {}
    for label in first_names:
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
            std_checksums.add(str(std_case.get("log_checksum", "missing")))
            rvv_checksums.add(str(rvv_case.get("log_checksum", "missing")))
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
            "size": vector_size(label),
            "label": label,
            "point_type": point_type(label),
            "metric": metric(values),
            "runs": rows,
            "checksums": {
                "std_log": sorted(std_checksums),
                "rvv_log": sorted(rvv_checksums),
                "exact_match": sorted(std_checksums) == sorted(rvv_checksums),
            },
        }
    return result


def overall_decision(case_metrics: dict[str, dict[str, Any]]) -> str:
    primary = case_metrics.get("direction-update-vector6")
    if primary:
        return str(primary["metric"]["bucket"])
    buckets = {info["metric"]["bucket"] for info in case_metrics.values()}
    if buckets <= {"positive"}:
        return "positive"
    if buckets <= {"positive", "weak_positive"}:
        return "weak_positive"
    if "negative" in buckets:
        return "negative"
    if "unstable" in buckets:
        return "unstable"
    return "neutral"


def side(label: str, reduction: str, asm_boundary: str, ms_key: str, ms_value: float) -> dict[str, Any]:
    return {
        "label": label,
        "boundary": "test_support_bfgs_direction_update_candidate",
        "wrapper": "bench_bfgs",
        "row_source": "not_applicable_bfgs_state_vector",
        "solve": False,
        "checksum_policy": "fnv1a_over_candidate_result_only; exact_log_checksum_is_diagnostic_sample",
        "timer_boundary": "direction_update_candidate_only",
        "gate": "same_deterministic_direction_input_vector_size_or_scalar_fallback",
        "mask": "dense_double_vector_no_nan_input",
        "reduction": reduction,
        "asm_boundary": asm_boundary,
        ms_key: ms_value,
    }


def build_manifest(
    runs: list[dict[str, Any]],
    case_metrics: dict[str, dict[str, Any]],
    summary_path: Path,
    args: argparse.Namespace,
) -> dict[str, Any]:
    first_meta = runs[0]["std"]["metadata"]
    binary_hash = f"std={sha256_file(Path(args.std_bin))}; rvv={sha256_file(Path(args.rvv_bin))}"
    comparisons: list[dict[str, Any]] = []
    for _, info in sorted(case_metrics.items(), key=lambda item: item[1]["size"]):
        std_med = statistics.median(row["std_ms"] for row in info["runs"])
        rvv_med = statistics.median(row["rvv_ms"] for row in info["runs"])
        comparisons.append(
            {
                "name": info["label"],
                "group": "bfgs_board_direction_update_repeated",
                "case_kind": "board_pre_production_direction_update_diagnostic",
                "evidence_role": "strict_ab",
                "point_type": info["point_type"],
                "scalar": "double",
                "size": info["size"],
                "run_count": len(runs),
                "ba_values": info["metric"]["values"],
                "decision_bucket": info["metric"]["bucket"],
                "std_ms": std_med,
                "rvv_ms": rvv_med,
                "allowed_contract_mismatches": [
                    "reduction",
                    "asm_boundary",
                ],
                "baseline": side(
                    "Std build direction-update diagnostic",
                    "scalar_dot_norm_linear_helpers",
                    "not_applicable_std_build",
                    "std_ms",
                    std_med,
                ),
                "candidate": side(
                    "RVV build direction-update diagnostic",
                    "rvv_dot_norm_linear_helpers_with_scalar_fallback",
                    "bench_bfgs_rvv_binary; helper may be inlined",
                    "rvv_ms",
                    rvv_med,
                ),
                "log_checksums": info["checksums"],
            }
        )

    return {
        "schema_version": 1,
        "summary": {
            "title": "bfgs direction-update repeated board diagnostic",
            "evidence_role": "strict_ab",
            "summary_path": str(summary_path),
            "label": "pre-production diagnostic; no production dispatch",
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


def format_checksums(values: list[str]) -> str:
    if len(values) <= 2:
        return ", ".join(f"`{value}`" for value in values)
    return f"{len(values)} distinct"


def render_summary(case_metrics: dict[str, dict[str, Any]], args: argparse.Namespace) -> str:
    lines: list[str] = []
    lines.append("# bfgs Board Direction-Update Repeated Summary")
    lines.append("")
    lines.append("## 运行合同")
    lines.append("")
    lines.append("- evidence_role：pre-production diagnostic（接入生产前诊断）")
    lines.append(f"- run_label：`{args.run_label}`")
    lines.append(f"- case_filter：`{args.case_filter}`")
    lines.append(f"- expected_runs：{args.expected_runs}")
    lines.append(f"- device：{args.device_label}")
    lines.append("")
    lines.append("`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV build 更快。")
    lines.append("本摘要不证明 production dispatch；它只回答 test-only direction-update 是否值得继续进入 caller hotspot audit。")
    lines.append("")
    lines.append("## 结果")
    lines.append("")
    lines.append("| case | size | runs | median | min | max | p10 | p90 | B/A<1 | bucket | values |")
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |")
    for _, info in sorted(case_metrics.items(), key=lambda item: item[1]["size"]):
        metric_info = info["metric"]
        lines.append(
            f"| `{info['label']}` | {info['size']} | {len(metric_info['values'])} | "
            f"{metric_info['median']:.3f}x | {metric_info['min']:.3f}x | "
            f"{metric_info['max']:.3f}x | {metric_info['p10']:.3f}x | "
            f"{metric_info['p90']:.3f}x | {metric_info['below_one_count']} | "
            f"`{metric_info['bucket']}` | {format_values(metric_info['values'])} |"
        )
    lines.append("")
    lines.append("## Checksum 边界")
    lines.append("")
    lines.append("bench log checksum 只覆盖计算结果，不再混入 `used_rvv` 路径标志。")
    lines.append("由于 double reduction（双精度归约）顺序可能导致 bit-level（逐位）差异，exact log checksum 只作为 diagnostic sample；数值正确性主证据仍是 `run_test_compare`。")
    lines.append("")
    lines.append("| case | Std checksum sample | RVV checksum sample | exact match |")
    lines.append("| --- | --- | --- | --- |")
    for _, info in sorted(case_metrics.items(), key=lambda item: item[1]["size"]):
        checksums = info["checksums"]
        lines.append(
            f"| `{info['label']}` | {format_checksums(checksums['std_log'])} | "
            f"{format_checksums(checksums['rvv_log'])} | `{checksums['exact_match']}` |"
        )
    lines.append("")
    lines.append("## 阶段解释")
    lines.append("")
    decision = overall_decision(case_metrics)
    lines.append(f"- primary_decision_bucket：`{decision}`（优先按 `direction-update-vector6` 判断，因为它接近 GICP 状态维度）。")
    lines.append("- `direction-update-vector128` 只检查 VL chunk 形态，不代表当前 production caller 工作集。")
    if decision in {"positive", "weak_positive"}:
        lines.append("- 下一步只允许进入 caller hotspot audit；production patch 仍需用户授权。")
    elif decision == "neutral":
        lines.append("- 当前只支持保留 diagnostic 结果；是否扩展 move-to+slope 需要另起阶段计划。")
    else:
        lines.append("- 当前不支持继续 production 接入路径；除非用户明确要求做 bounded negative-analysis，否则 topic 应停在 diagnostic closeout。")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--run-label", default="direction_update_repeated")
    parser.add_argument("--case-filter", default="direction-update")
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
    metrics = build_case_metrics(runs)
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
