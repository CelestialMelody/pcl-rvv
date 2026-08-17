#!/usr/bin/env python3
"""
汇总 correspondence_rejection_poly 的 repeated board（重复板卡）bench 输出。

这个脚本只解析当前 topic 的板卡 run-labelled 目录。它把每轮 Std / RVV
bench log 转成 summary.md 和 evidence_manifest.json，供 Evidence Doctor
（证据体检）检查。脚本不读取或记录私有板卡地址；summary 只写仓库相对
日志路径、case label（用例标签）、checksum（校验和）和 speedup（加速比）。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(
    r"^(?P<name>.+?)\s*:\s*(?P<value>[0-9]+(?:\.[0-9]+)?)\s*(?P<unit>u|m)s\s*/\s*iter\s*$"
)
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")


CASE_LABELS: dict[str, dict[str, Any]] = {
    "edge-batch candidate 64K": {
        "case_kind": "diagnostic_board_repeated",
        "candidate_family": "edge_length_batch",
        "row_source": "correspondences",
        "timer_boundary": "edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 65536,
        "mask": "edge_similarity_threshold",
        "reduction": "not_applicable",
    },
    "edge-batch candidate 256K": {
        "case_kind": "diagnostic_board_repeated",
        "candidate_family": "edge_length_batch",
        "row_source": "correspondences",
        "timer_boundary": "edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 262144,
        "mask": "edge_similarity_threshold",
        "reduction": "not_applicable",
    },
    "edge-gather-staging candidate 64K": {
        "case_kind": "production_shaped_board_repeated",
        "candidate_family": "edge_gather_staging",
        "row_source": "correspondences",
        "timer_boundary": "correspondence_index_gather_squared_distance_staging_edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 65536,
        "mask": "edge_similarity_threshold",
        "reduction": "scalar_gather_staging_then_rvv_formula",
    },
    "edge-gather-staging candidate 256K": {
        "case_kind": "production_shaped_board_repeated",
        "candidate_family": "edge_gather_staging",
        "row_source": "correspondences",
        "timer_boundary": "correspondence_index_gather_squared_distance_staging_edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 262144,
        "mask": "edge_similarity_threshold",
        "reduction": "scalar_gather_staging_then_rvv_formula",
    },
    "accept-rate filter candidate 64K": {
        "case_kind": "diagnostic_board_repeated",
        "candidate_family": "accept_rate_filter",
        "row_source": "contiguous_counters",
        "timer_boundary": "accept_rate_filter_and_checksum",
        "checksum_policy": "scaled_accept_rate_and_kept_index_fnv1a",
        "size": 65536,
        "mask": "num_samples_nonzero_and_accept_rate_cut",
        "reduction": "scalar_tail_output_append",
    },
    "accept-rate filter candidate 256K": {
        "case_kind": "diagnostic_board_repeated",
        "candidate_family": "accept_rate_filter",
        "row_source": "contiguous_counters",
        "timer_boundary": "accept_rate_filter_and_checksum",
        "checksum_policy": "scaled_accept_rate_and_kept_index_fnv1a",
        "size": 262144,
        "mask": "num_samples_nonzero_and_accept_rate_cut",
        "reduction": "scalar_tail_output_append",
    },
    "fixed-seed public entry 512 correspondences": {
        "case_kind": "public_entry_shaped_board_smoke",
        "candidate_family": "full_entry_diagnostic",
        "row_source": "correspondences",
        "timer_boundary": "full_get_remaining_correspondences_and_checksum",
        "checksum_policy": "kept_query_indices_fnv1a",
        "size": 512,
        "mask": "production_threshold_polygon_and_otsu",
        "reduction": "histogram_otsu_scalar",
    },
    "production-direct public entry 2048 correspondences": {
        "case_kind": "production_direct_board_repeated",
        "candidate_family": "production_edge_batch_rvv",
        "row_source": "correspondences",
        "timer_boundary": "production_get_remaining_correspondences_dispatch_and_checksum",
        "checksum_policy": "kept_query_match_indices_fnv1a",
        "size": 2048,
        "mask": "production_threshold_polygon_rvv_and_otsu",
        "reduction": "rvv_edge_predicate_then_scalar_polygon_histogram_otsu",
    },
    "production-direct public entry 8192 correspondences": {
        "case_kind": "production_direct_board_repeated",
        "candidate_family": "production_edge_batch_rvv",
        "row_source": "correspondences",
        "timer_boundary": "production_get_remaining_correspondences_dispatch_and_checksum",
        "checksum_policy": "kept_query_match_indices_fnv1a",
        "size": 8192,
        "mask": "production_threshold_polygon_rvv_and_otsu",
        "reduction": "rvv_edge_predicate_then_scalar_polygon_histogram_otsu",
    },
}


def repo_relative(path: Path) -> str:
    current = path.resolve()
    for parent in (current, *current.parents):
        if (parent / ".git").exists():
            try:
                return current.relative_to(parent).as_posix()
            except ValueError:
                break
    return path.as_posix()


def sha256(path: Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
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
            value = float(match.group("value"))
            if match.group("unit") == "u":
                value *= 0.001
            cases[current_case] = {"ms": value}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None

    return {"path": repo_relative(path), "metadata": metadata, "cases": cases}


def collect_runs(input_dir: Path) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    for run_dir in sorted(input_dir.glob("run-*")):
        if not run_dir.is_dir():
            continue
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if not std_log.exists() or not rvv_log.exists():
            continue
        std = parse_log(std_log)
        rvv = parse_log(rvv_log)
        missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
        if missing:
            raise SystemExit(f"{run_dir}: Std/RVV case set mismatch: {', '.join(missing)}")
        runs.append({"run_dir": repo_relative(run_dir), "std": std, "rvv": rvv})
    if not runs:
        raise SystemExit(f"no complete run-* directories found under {input_dir}")
    return runs


def median(values: list[float]) -> float:
    return float(statistics.median(values))


def decide_bucket(values_by_case: dict[str, list[float]], expected_runs: int) -> str:
    if any(len(values) < expected_runs for values in values_by_case.values()):
        return "unstable"
    medians = [median(values) for values in values_by_case.values()]
    degrade_count = sum(1 for values in values_by_case.values() for value in values if value < 1.0)
    total_count = sum(len(values) for values in values_by_case.values())
    degrade_fraction = degrade_count / total_count if total_count else 1.0

    if any(value < 0.95 for value in medians) or degrade_fraction > 0.20:
        return "negative"
    if all(value >= 1.20 for value in medians) and degrade_count == 0:
        return "positive"
    if all(value >= 1.05 for value in medians) and degrade_fraction <= 0.20:
        return "weak_positive"
    return "neutral"


def unique_or_join(values: list[Any]) -> str:
    normalized = sorted({str(value) for value in values if value is not None})
    if not normalized:
        return ""
    return normalized[0] if len(normalized) == 1 else ",".join(normalized)


def make_comparisons(runs: list[dict[str, Any]], expected_runs: int) -> list[dict[str, Any]]:
    comparisons: list[dict[str, Any]] = []
    case_names = sorted(runs[0]["std"]["cases"])
    for name in case_names:
        if name not in CASE_LABELS:
            raise SystemExit(f"unknown correspondence_rejection_poly case label: {name}")
        label = CASE_LABELS[name]
        ba_values: list[float] = []
        std_ms_values: list[float] = []
        rvv_ms_values: list[float] = []
        std_checksums: list[str] = []
        rvv_checksums: list[str] = []
        for run in runs:
            std_case = run["std"]["cases"][name]
            rvv_case = run["rvv"]["cases"][name]
            std_ms = float(std_case["ms"])
            rvv_ms = float(rvv_case["ms"])
            std_ms_values.append(std_ms)
            rvv_ms_values.append(rvv_ms)
            ba_values.append(std_ms / rvv_ms if rvv_ms > 0.0 else 0.0)
            if "checksum" in std_case:
                std_checksums.append(std_case["checksum"])
            if "checksum" in rvv_case:
                rvv_checksums.append(rvv_case["checksum"])

        if label["candidate_family"] == "full_entry_diagnostic":
            boundary = "real_public_entry_no_rvv_dispatch"
        elif label["candidate_family"] == "production_edge_batch_rvv":
            boundary = "real_public_entry_production_dispatch"
        else:
            boundary = "test_support_candidate_wrapper"
        wrapper = "bench_correspondence_rejection_poly"
        comparisons.append(
            {
                "name": name,
                "group": label["candidate_family"],
                "evidence_role": "strict_ab",
                "case_kind": label["case_kind"],
                "point_type": "PointXYZ",
                "size": label["size"],
                "candidate_family": label["candidate_family"],
                "row_source": label["row_source"],
                "run_count": len(ba_values),
                "expected_run_count": expected_runs,
                "std_ms": median(std_ms_values),
                "rvv_ms": median(rvv_ms_values),
                "ba_values": [round(value, 6) for value in ba_values],
                "baseline": {
                    "label": "Std build",
                    "boundary": boundary,
                    "wrapper": wrapper,
                    "row_source": label["row_source"],
                    "solve": False,
                    "checksum_policy": label["checksum_policy"],
                    "checksum": unique_or_join(std_checksums),
                    "timer_boundary": label["timer_boundary"],
                    "gate": "test_support_candidate_or_scalar_public_entry",
                    "mask": label["mask"],
                    "reduction": label["reduction"],
                    "asm_boundary": "not_applicable_std_build",
                    "std_ms": median(std_ms_values),
                    "rvv_ms": "",
                },
                "candidate": {
                    "label": "RVV build",
                    "boundary": boundary,
                    "wrapper": wrapper,
                    "row_source": label["row_source"],
                    "solve": False,
                    "checksum_policy": label["checksum_policy"],
                    "checksum": unique_or_join(rvv_checksums),
                    "timer_boundary": label["timer_boundary"],
                    "gate": "test_support_candidate_or_scalar_public_entry",
                    "mask": label["mask"],
                    "reduction": label["reduction"],
                    "asm_boundary": "bench_binary_smoke_only",
                    "std_ms": "",
                    "rvv_ms": median(rvv_ms_values),
                },
            }
        )
    return comparisons


def build_manifest(args: argparse.Namespace, runs: list[dict[str, Any]], summary_path: Path) -> dict[str, Any]:
    comparisons = make_comparisons(runs, args.expected_runs)
    values_by_case = {comp["name"]: comp["ba_values"] for comp in comparisons}
    bucket = decide_bucket(values_by_case, args.expected_runs)
    binary_hash = args.binary_hash_override or (
        f"std={sha256(args.std_bin) or 'not_recorded'}; "
        f"rvv={sha256(args.rvv_bin) or 'not_recorded'}"
    )

    metadata = {
        "device": args.device_label,
        "iterations": args.iterations,
        "warmup_iterations": args.warmup_iterations,
        "run_count": len(runs),
        "taskset": args.taskset or "not_recorded_board_run",
        "governor": args.governor or "not_recorded_board_run",
        "freq": args.freq or "not_recorded_board_run",
        "temperature": args.temperature or "not_recorded_board_run",
        "vlen": args.vlen_desc,
        "binary_hash": binary_hash,
        "dataset": runs[0]["std"]["metadata"].get("dataset", ""),
        "case_filter": args.case_filter,
        "decision_bucket": bucket,
    }
    return {
        "schema_version": 1,
        "summary": {
            "title": f"correspondence_rejection_poly board repeated {args.run_label}",
            "evidence_role": "strict_ab",
            "summary_path": repo_relative(summary_path),
            "label": f"board repeated strict diagnostic: {args.run_label}",
            "analysis_script": "test-rvv/registration/correspondence_rejection_poly/script/summarize_crpoly_board_repeated.py",
            "metadata": metadata,
        },
        "checks": {
            "strict_ab": True,
            "equal_fields": [
                "boundary",
                "wrapper",
                "row_source",
                "solve",
                "checksum_policy",
                "timer_boundary",
                "gate",
                "mask",
                "reduction",
            ],
        },
        "thresholds": {
            "min_repeated_runs": args.expected_runs,
            "high_degrade_fraction": 0.20,
            "long_tail_ratio": 1.20,
            "near_threshold_margin": 0.05,
        },
        "runs": [{"run_dir": run["run_dir"]} for run in runs],
        "comparisons": comparisons,
    }


def format_values(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}x" for value in values)


def write_summary(path: Path, manifest: dict[str, Any]) -> None:
    metadata = manifest["summary"]["metadata"]
    lines = [
        f"# Board repeated summary: {metadata['decision_bucket']}",
        "",
        "## 运行上下文",
        "",
        f"- run_label: `{Path(path).parent.name}`",
        f"- evidence_role: `{manifest['summary']['evidence_role']}`",
        f"- device: `{metadata['device']}`",
        f"- iterations: `{metadata['iterations']}`",
        f"- warmup_iterations: `{metadata['warmup_iterations']}`",
        f"- run_count: `{metadata['run_count']}`",
        f"- decision_bucket: `{metadata['decision_bucket']}`",
        f"- binary_hash: `{metadata['binary_hash']}`",
        "",
        "## 结果",
        "",
        "| case | family | size | runs | min | median | max | speedup_values | degradation_frequency | checksum |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for comp in manifest["comparisons"]:
        values = [float(value) for value in comp["ba_values"]]
        degraded = [value for value in values if value < 1.0]
        checksum = "match" if comp["baseline"]["checksum"] == comp["candidate"]["checksum"] else "mismatch"
        lines.append(
            "| {name} | `{family}` | {size} | {runs} | {min_v:.3f}x | {median_v:.3f}x | {max_v:.3f}x | {values} | {degrade}/{runs} | {checksum} |".format(
                name=comp["name"],
                family=comp["candidate_family"],
                size=comp["size"],
                runs=len(values),
                min_v=min(values),
                median_v=median(values),
                max_v=max(values),
                values=format_values(values),
                degrade=len(degraded),
                checksum=checksum,
            )
        )
    if metadata.get("case_filter") == "production-direct":
        boundary_line = (
            "- 这是 board / target hardware（板卡 / 目标硬件）上的 production-direct"
            "（真实生产入口）重复测试；重新采集前必须确认当前 production patch 是否存在。"
        )
        dispatch_line = (
            "- 该 target 不修改 production 源码；summary 的证据角色取决于运行时的 production diff。"
            "有 Standard / RVV dispatch 时才可按 production RVV evidence（生产 RVV 证据）审查。"
        )
    else:
        boundary_line = (
            "- 这是 board / target hardware（板卡 / 目标硬件）上的 test support candidate wrapper"
            "（测试支撑候选包装）重复测试。"
        )
        dispatch_line = (
            "- `full_entry_diagnostic` 若出现，只证明真实 public entry（公开入口）标量完整调用可运行；"
            "test support candidate 仍不能单独证明 production RVV dispatch。"
        )

    lines.extend(
        [
            "",
            "## 证据边界",
            "",
            boundary_line,
            "- `speedup = std_ms / rvv_ms`，大于 1 表示 RVV build 更快。",
            dispatch_line,
            "- summary、manifest 和 Evidence Doctor 是 summary artifact（摘要证据产物）。各 run 目录下的 `run_bench_*.log` 是 raw log（原始日志），默认不进入提交边界。",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-filter", required=True)
    parser.add_argument("--iterations", required=True, type=int)
    parser.add_argument("--warmup-iterations", required=True, type=int)
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--std-bin", type=Path, default=Path("build/riscv/bench_correspondence_rejection_poly_std"))
    parser.add_argument("--rvv-bin", type=Path, default=Path("build/riscv/bench_correspondence_rejection_poly_rvv"))
    parser.add_argument("--binary-hash-override", default="")
    parser.add_argument("--device-label", default="RISC-V board / target hardware")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    parser.add_argument("--taskset", default="")
    parser.add_argument("--governor", default="")
    parser.add_argument("--freq", default="")
    parser.add_argument("--temperature", default="")
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    runs = collect_runs(args.input_dir)
    summary_path = args.output_dir / "summary.md"
    manifest_path = args.output_dir / "evidence_manifest.json"
    manifest = build_manifest(args, runs, summary_path)
    write_summary(summary_path, manifest)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
