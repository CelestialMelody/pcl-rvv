#!/usr/bin/env python3
"""
生成 transformation_estimation_svd 的 board repeated summary（板卡重复测试摘要）。

输入是一组 `run-*` 目录，每个目录包含 Std/RVV bench log 和 analyze log。脚本同时输出：

- `summary.md`：给 reviewer 阅读的板卡重复摘要。
- `evidence_manifest.json`：给全局 Evidence Doctor（证据体检）读取的 manifest。

本脚本把 same-boundary fused Std/RVV（同边界 fused 标量 / RVV 对比）和
mixed-boundary public-vs-fused-RVV（混合边界 public baseline 与 fused RVV 对照）分开。
前者说明 RVV fused accumulation 本身是否有收益；后者只能判断是否值得进入 PI1，不能写成
production direct（真实生产路径证据）。

PI2 生产补丁接入后，同一个脚本也支持 production_direct 模式：只解析 public Umeyama
公开入口用例，把 Std build 与 RVV build 的当前 row source 路径作为生产直连证据。
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
INDEXED_DATASET_RE = re.compile(r"^Indexed Dataset:\s*(?P<value>.+?)\s*$")
DUAL_INDEXED_DATASET_RE = re.compile(r"^Dual-Indexed Dataset:\s*(?P<value>.+?)\s*$")
CORRESPONDENCE_DATASET_RE = re.compile(r"^Correspondence Dataset:\s*(?P<value>.+?)\s*$")


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
        if match := INDEXED_DATASET_RE.match(line):
            metadata["indexed_dataset"] = match.group("value")
            continue
        if match := DUAL_INDEXED_DATASET_RE.match(line):
            metadata["dual_indexed_dataset"] = match.group("value")
            continue
        if match := CORRESPONDENCE_DATASET_RE.match(line):
            metadata["correspondence_dataset"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None
    return {"path": str(path), "metadata": metadata, "cases": cases}


def run_dirs(root: Path) -> list[Path]:
    return sorted(p for p in root.iterdir() if p.is_dir() and p.name.startswith("run-"))


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


def row_source_key(row_source: str) -> str:
    return row_source.replace("-", "_")


def dataset_for_row_source(metadata: dict[str, Any], row_source: str) -> str | None:
    if row_source == "source-indexed-cloud-pair":
        return metadata.get("indexed_dataset") or metadata.get("dataset")
    if row_source == "dual-indices-cloud-pair":
        return metadata.get("dual_indexed_dataset") or metadata.get("indexed_dataset") or metadata.get("dataset")
    if row_source == "correspondence-pair":
        return (
            metadata.get("correspondence_dataset")
            or metadata.get("dual_indexed_dataset")
            or metadata.get("indexed_dataset")
            or metadata.get("dataset")
        )
    return metadata.get("dataset")


def public_case(size: str, row_source: str) -> str:
    return f"public Umeyama {row_source} {size}"


def fused_case(size: str, row_source: str) -> str:
    return f"fused accum candidate {row_source} {size}"


def legacy_public_case(size: str) -> str:
    return f"public Umeyama full-cloud {size}"


def legacy_fused_case(size: str) -> str:
    return f"fused accum candidate full-cloud {size}"


def lookup_case(cases: dict[str, dict[str, Any]], *labels: str) -> dict[str, Any]:
    for label in labels:
        if label in cases:
            return cases[label]
    raise SystemExit("missing case labels: " + ", ".join(labels))


def bucket_for(values: list[float]) -> str:
    if not values:
        return "blocked"
    med = statistics.median(values)
    if all(v > 1.15 for v in values):
        return "positive"
    if med >= 1.03 and min(values) >= 0.97:
        return "weak_positive"
    if all(0.97 <= v <= 1.03 for v in values):
        return "neutral"
    if med < 0.97 or min(values) < 0.97:
        return "negative"
    return "unstable"


def fmt_values(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}" for value in values)


def metric(values: list[float]) -> dict[str, Any]:
    return {
        "values": values,
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
        "bucket": bucket_for(values),
    }


def build_case_metrics(runs: list[dict[str, Any]],
                       evidence_role: str,
                       row_source: str) -> dict[str, dict[str, Any]]:
    sizes = sorted(
        {
            size_suffix(name)
            for name in runs[0]["std"]["cases"]
            if name.startswith(f"public Umeyama {row_source} ")
        },
        key=size_value,
    )
    if not sizes:
        raise SystemExit(f"no public Umeyama cases found for row source: {row_source}")
    result: dict[str, dict[str, Any]] = {}
    production_direct = evidence_role == "production_direct"
    for suffix in sizes:
        public_label = public_case(suffix, row_source)
        fused_label = fused_case(suffix, row_source)
        public_lookup = [public_label]
        fused_lookup = [fused_label]
        if row_source == "ordered-cloud-pair":
            public_lookup.append(legacy_public_case(suffix))
            fused_lookup.append(legacy_fused_case(suffix))
        public_build_values: list[float] = []
        fused_same_boundary_values: list[float] = []
        public_vs_fused_values: list[float] = []
        rows: list[dict[str, Any]] = []
        public_std_checksums: set[str] = set()
        public_rvv_checksums: set[str] = set()
        fused_std_checksums: set[str] = set()
        fused_rvv_checksums: set[str] = set()
        for run in runs:
            std_public = lookup_case(run["std"]["cases"], *public_lookup)
            rvv_public = lookup_case(run["rvv"]["cases"], *public_lookup)
            public_std = float(std_public["ms"])
            public_rvv = float(rvv_public["ms"])
            public_build_values.append(public_std / public_rvv)
            public_std_checksums.add(str(std_public.get("checksum")))
            public_rvv_checksums.add(str(rvv_public.get("checksum")))
            row = {
                "run": run["name"],
                "public_std_ms": public_std,
                "public_rvv_ms": public_rvv,
                "public_build_ba": public_std / public_rvv,
            }
            if not production_direct:
                std_fused = lookup_case(run["std"]["cases"], *fused_lookup)
                rvv_fused = lookup_case(run["rvv"]["cases"], *fused_lookup)
                fused_std = float(std_fused["ms"])
                fused_rvv = float(rvv_fused["ms"])
                fused_same_boundary_values.append(fused_std / fused_rvv)
                public_vs_fused_values.append(public_std / fused_rvv)
                fused_std_checksums.add(str(std_fused.get("checksum")))
                fused_rvv_checksums.add(str(rvv_fused.get("checksum")))
                row.update(
                    {
                        "fused_std_ms": fused_std,
                        "fused_rvv_ms": fused_rvv,
                        "fused_same_boundary_ba": fused_std / fused_rvv,
                        "public_vs_fused_ba": public_std / fused_rvv,
                    }
                )
            rows.append(row)
        result[suffix] = {
            "size": size_value(suffix),
            "public_build": metric(public_build_values),
            "fused_same_boundary": metric(fused_same_boundary_values)
            if fused_same_boundary_values
            else None,
            "public_vs_fused_rvv": metric(public_vs_fused_values)
            if public_vs_fused_values
            else None,
            "runs": rows,
            "checksums": {
                "public_std": sorted(public_std_checksums),
                "public_rvv": sorted(public_rvv_checksums),
                "fused_std": sorted(fused_std_checksums),
                "fused_rvv": sorted(fused_rvv_checksums),
            },
        }
    return result


def primary_metric_name(evidence_role: str) -> str:
    if evidence_role == "production_direct":
        return "public_build"
    return "public_vs_fused_rvv"


def overall_decision(case_metrics: dict[str, dict[str, Any]], evidence_role: str) -> str:
    metric_name = primary_metric_name(evidence_role)
    target = [
        info[metric_name]
        for suffix, info in case_metrics.items()
        if suffix != "4K" and info.get(metric_name)
    ]
    if not target:
        target = [info[metric_name] for info in case_metrics.values() if info.get(metric_name)]
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


def side(boundary: str,
         wrapper: str,
         row_source: str,
         timer_boundary: str,
         gate: str,
         reduction: str,
         asm_boundary: str,
         ms_key: str,
         ms_value: float) -> dict[str, Any]:
    return {
        "boundary": boundary,
        "wrapper": wrapper,
        "row_source": row_source_key(row_source),
        "solve": True,
        "checksum_policy": "matrix_path_fingerprint_correctness_checked_by_gtest_budget",
        "timer_boundary": timer_boundary,
        "gate": gate,
        "mask": "not_applicable_dense_only",
        "reduction": reduction,
        "asm_boundary": asm_boundary,
        ms_key: ms_value,
    }


def build_manifest(runs: list[dict[str, Any]],
                   case_metrics: dict[str, dict[str, Any]],
                   summary_path: Path,
                   args: argparse.Namespace) -> dict[str, Any]:
    first_meta = runs[0]["std"]["metadata"]
    dataset = dataset_for_row_source(first_meta, args.row_source)
    binary_hash = (
        f"std={sha256_file(Path(args.std_bin))}; rvv={sha256_file(Path(args.rvv_bin))}"
    )
    comparisons: list[dict[str, Any]] = []
    row_key = row_source_key(args.row_source)
    fused_boundary = f"test_support_fused_{row_key}_candidate"
    fused_timer = {
        "ordered-cloud-pair": "fused_accumulation_plus_eigen_3x3_svd",
        "source-indexed-cloud-pair": "fused_source_indexed_accumulation_plus_eigen_3x3_svd",
        "dual-indices-cloud-pair": "fused_dual_indices_accumulation_plus_eigen_3x3_svd",
        "correspondence-pair": "fused_correspondence_accumulation_plus_eigen_3x3_svd",
    }[args.row_source]
    fused_gate = {
        "ordered-cloud-pair": "dense_ordered_cloud_pair_float_xyz_aos",
        "source-indexed-cloud-pair": "dense_source_indexed_cloud_pair_float_xyz_aos_valid_indices",
        "dual-indices-cloud-pair": "dense_dual_indices_cloud_pair_float_xyz_aos_valid_indices",
        "correspondence-pair": "dense_correspondence_pair_float_xyz_aos_valid_correspondences",
    }[args.row_source]
    rvv_reduction = {
        "ordered-cloud-pair": "rvv_ordered_sum_tree",
        "source-indexed-cloud-pair": "rvv_source_indexed_sum_tree",
        "dual-indices-cloud-pair": "rvv_dual_indices_sum_tree",
        "correspondence-pair": "rvv_correspondence_sum_tree",
    }[args.row_source]
    for suffix, info in case_metrics.items():
        public_sanity = info["public_build"]
        public_std_med = statistics.median(row["public_std_ms"] for row in info["runs"])
        public_rvv_med = statistics.median(row["public_rvv_ms"] for row in info["runs"])
        if args.evidence_role == "production_direct":
            comparisons.append(
                {
                    "name": f"production direct {args.row_source} {suffix}",
                    "group": f"tesvd_production_{row_key}",
                    "case_kind": "board_production_direct",
                    "evidence_role": "production_direct",
                    "point_type": "PointXYZ",
                    "scalar": "float",
                    "size": info["size"],
                    "run_count": len(runs),
                    "ba_values": public_sanity["values"],
                    "decision_bucket": public_sanity["bucket"],
                    "std_ms": public_std_med,
                    "rvv_ms": public_rvv_med,
                    "allowed_contract_mismatches": [
                        "gate",
                        "reduction",
                        "asm_boundary",
                    ],
                    "notes": (
                        f"B/A = public {args.row_source} Std build ms / public "
                        f"{args.row_source} RVV build ms. Correctness is covered by "
                        "QEMU and board gtests, including PointXYZI/PointXYZRGB "
                        "representative layouts."
                    ),
                    "baseline": side(
                        f"production_public_{row_key}",
                        "bench_tesvd_public_entry",
                        args.row_source,
                        f"public_estimateRigidTransformation_{row_key}",
                        "std_build_no_rvv_dispatch",
                        "eigen_umeyama_internal_path",
                        "not_applicable_std_build",
                        "std_ms",
                        public_std_med,
                    ),
                    "candidate": side(
                        f"production_public_{row_key}",
                        "bench_tesvd_public_entry",
                        args.row_source,
                        f"public_estimateRigidTransformation_{row_key}",
                        "layout_gated_scalar_float_rvv_or_fallback",
                        rvv_reduction,
                        "production_estimateRigidTransformation_symbol",
                        "rvv_ms",
                        public_rvv_med,
                    ),
                }
            )
            continue

        fused_metric = info["fused_same_boundary"]
        public_metric = info["public_vs_fused_rvv"]
        if fused_metric is None or public_metric is None:
            raise SystemExit("diagnostic mode requires fused and public cases")
        fused_std_med = statistics.median(row["fused_std_ms"] for row in info["runs"])
        fused_rvv_med = statistics.median(row["fused_rvv_ms"] for row in info["runs"])
        comparisons.append(
            {
                "name": f"fused same-boundary {args.row_source} {suffix}",
                "group": f"tesvd_fused_same_boundary_{row_key}",
                "case_kind": "board_diagnostic_same_boundary",
                "evidence_role": "strict_ab",
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "run_count": len(runs),
                "ba_values": fused_metric["values"],
                "decision_bucket": fused_metric["bucket"],
                "std_ms": fused_std_med,
                "rvv_ms": fused_rvv_med,
                "allowed_contract_mismatches": [
                    "reduction",
                    "asm_boundary",
                ],
                "baseline": side(
                    fused_boundary,
                    "bench_tesvd",
                    args.row_source,
                    fused_timer,
                    fused_gate,
                    "scalar_same_chain",
                    "not_applicable_std_build",
                    "std_ms",
                    fused_std_med,
                ),
                "candidate": side(
                    fused_boundary,
                    "bench_tesvd",
                    args.row_source,
                    fused_timer,
                    fused_gate,
                    rvv_reduction,
                    "bench_binary_test_support_helper",
                    "rvv_ms",
                    fused_rvv_med,
                ),
            }
        )
        comparisons.append(
            {
                "name": f"mixed-boundary public baseline vs fused RVV {args.row_source} {suffix}",
                "group": f"tesvd_public_vs_fused_rvv_{row_key}",
                "case_kind": "mixed_boundary_cross_check",
                "evidence_role": "mixed_boundary_cross_check",
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "run_count": len(runs),
                "ba_values": public_metric["values"],
                "decision_bucket": public_metric["bucket"],
                "std_ms": public_std_med,
                "rvv_ms": fused_rvv_med,
                "requires_board_context": True,
                "allowed_contract_mismatches": [
                    "boundary",
                    "timer_boundary",
                    "gate",
                    "reduction",
                    "asm_boundary",
                ],
                "notes": (
                    "B/A = public Umeyama Std ms / fused candidate RVV ms. "
                    "This is a pre-production diagnostic cross-check, not production direct evidence."
                ),
                "baseline": side(
                    f"production_public_{row_key}_current_baseline",
                    "bench_tesvd",
                    args.row_source,
                    "dynamic_3xn_matrix_fill_plus_pcl_umeyama",
                    "current_public_entry_size_match",
                    "eigen_umeyama_internal_path",
                    "not_applicable_std_build",
                    "std_ms",
                    public_std_med,
                ),
                "candidate": side(
                    fused_boundary,
                    "bench_tesvd",
                    args.row_source,
                    fused_timer,
                    fused_gate,
                    rvv_reduction,
                    "bench_binary_test_support_helper",
                    "rvv_ms",
                    fused_rvv_med,
                ),
            }
        )
        comparisons.append(
            {
                "name": f"public build sanity {args.row_source} {suffix}",
                "group": f"tesvd_public_build_sanity_{row_key}",
                "case_kind": "board_build_sanity",
                "evidence_role": "diagnostic",
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "run_count": len(runs),
                "decision_bucket": public_sanity["bucket"],
                "std_ms": public_std_med,
                "rvv_ms": public_rvv_med,
                "notes": "Sanity row only; ba_values omitted so doctor does not treat public build drift as candidate evidence.",
                "baseline": side(
                    f"production_public_{row_key}_current_baseline",
                    "bench_tesvd",
                    args.row_source,
                    "dynamic_3xn_matrix_fill_plus_pcl_umeyama",
                    "current_public_entry_size_match",
                    "eigen_umeyama_internal_path",
                    "not_applicable_std_build",
                    "std_ms",
                    public_std_med,
                ),
                "candidate": side(
                    f"production_public_{row_key}_current_baseline",
                    "bench_tesvd",
                    args.row_source,
                    "dynamic_3xn_matrix_fill_plus_pcl_umeyama",
                    "current_public_entry_size_match",
                    "eigen_umeyama_internal_path",
                    "production_has_no_tesvd_rvv_symbol",
                    "rvv_ms",
                    public_rvv_med,
                ),
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": (
                f"transformation_estimation_svd production direct {args.row_source} board repeated"
                if args.evidence_role == "production_direct"
                else "transformation_estimation_svd board repeated diagnostic"
            ),
            "evidence_role": args.evidence_role,
            "summary_path": str(summary_path),
            "label": (
                f"board repeated production direct {args.row_source}"
                if args.evidence_role == "production_direct"
                else f"board repeated {args.row_source} diagnostic; not production direct"
            ),
            "analysis_script": "test-rvv/registration/transformation_estimation_svd/script/generate_tesvd_board_repeated_summary.py",
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
                "binary_hash": binary_hash,
                "dataset": dataset,
                "decision_bucket": overall_decision(case_metrics, args.evidence_role),
            },
        },
        "checks": {
            "strict_ab": False,
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
            "thresholds": {
                "min_repeated_runs": args.expected_runs,
            },
        },
        "runs": [run["name"] for run in runs],
        "artifacts": {
            "input_dir": str(Path(args.input_dir)),
            "summary_path": str(summary_path),
            "std_bin": args.std_bin,
            "rvv_bin": args.rvv_bin,
        },
        "comparisons": comparisons,
    }


def write_summary(path: Path,
                  runs: list[dict[str, Any]],
                  case_metrics: dict[str, dict[str, Any]],
                  manifest_path: Path,
                  doctor_path: Path,
                  args: argparse.Namespace) -> None:
    production_direct = args.evidence_role == "production_direct"
    unsupported_rows = {
        "ordered-cloud-pair": "source-indexed、dual-indices、correspondences",
        "source-indexed-cloud-pair": "dual-indices、correspondences",
        "dual-indices-cloud-pair": "correspondences",
        "correspondence-pair": "none",
    }[args.row_source]
    title = (
        f"# transformation_estimation_svd production direct {args.row_source} board repeated summary"
        if production_direct
        else f"# transformation_estimation_svd {args.row_source} board repeated summary"
    )
    lines = [
        title,
        "",
        "## 运行合同",
        "",
        f"- run_label：`{args.run_label}`",
        f"- runs：`{', '.join(run['name'] for run in runs)}`",
        f"- device：`{args.device_label}`",
        f"- case-filter：`{args.case_filter}`",
        f"- evidence_role：`{args.evidence_role}`",
        f"- iterations：`{runs[0]['std']['metadata'].get('iterations')}`",
        f"- warm-up iterations：`{runs[0]['std']['metadata'].get('warmup_iterations')}`",
        "- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。",
        f"- manifest（默认本地证据清单）：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
        "",
    ]

    if production_direct:
        lines.extend(
            [
                "## public Std/RVV（生产直连）",
                "",
                f"这一表只使用真实 public Umeyama 公开入口。Std build 是 baseline，RVV build 在满足 gate 时命中 production {args.row_source} RVV 分流；不覆盖其它 row source 或 `Scalar=double`。",
                "",
                "| size | values | median | min | max | bucket |",
                "| --- | --- | ---: | ---: | ---: | --- |",
            ]
        )
        for suffix, info in case_metrics.items():
            m = info["public_build"]
            lines.append(
                f"| {suffix} | `{fmt_values(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` |"
            )
        lines.extend(
            [
                "",
                "## 决策桶",
                "",
                f"- overall_decision_bucket：`{overall_decision(case_metrics, args.evidence_role)}`",
                f"- `positive` / `weak_positive` 只批准当前 gate 下的 dense {args.row_source}、`Scalar=float`、xyz AoS 生产路径。",
                "- 未逐项上板的 gate-allowed 点型只继承 correctness 与代表性性能判断，不能写成逐类型性能已证明。",
                f"- {unsupported_rows} 和 `Scalar=double` 本摘要均不接 production。",
                "",
            ]
        )
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("\n".join(lines), encoding="utf-8")
        return

    lines.extend(
        [
            "- 诊断模式说明：不是 production direct（真实生产路径证据）。",
            "",
            "## public baseline vs fused RVV（混合边界）",
            "",
            f"这一表用当前 public Umeyama {args.row_source} Std 作为 baseline，用 test-only fused RVV 作为 candidate。它只回答该 row source 是否值得进入 production integration loop；不能直接证明 production dispatch。",
            "",
            "| size | values | median | min | max | bucket |",
            "| --- | --- | ---: | ---: | ---: | --- |",
        ]
    )
    for suffix, info in case_metrics.items():
        m = info["public_vs_fused_rvv"]
        lines.append(
            f"| {suffix} | `{fmt_values(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` |"
        )
    lines.extend(
        [
            "",
            "## fused Std/RVV（同边界）",
            "",
            "这一表只比较同一 test-support fused helper 的 Std 和 RVV 构建，用来判断 RVV accumulation 本身是否有收益。",
            "",
            "| size | values | median | min | max | bucket |",
            "| --- | --- | ---: | ---: | ---: | --- |",
        ]
    )
    for suffix, info in case_metrics.items():
        m = info["fused_same_boundary"]
        lines.append(
            f"| {suffix} | `{fmt_values(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` |"
        )
    lines.extend(
        [
            "",
            "## public build sanity（公开入口构建一致性）",
            "",
            "这一表只检查 public Umeyama 在 Std/RVV build 中是否有明显构建漂移，不作为候选收益。",
            "",
            "| size | values | median | min | max | bucket |",
            "| --- | --- | ---: | ---: | ---: | --- |",
        ]
    )
    for suffix, info in case_metrics.items():
        m = info["public_build"]
        lines.append(
            f"| {suffix} | `{fmt_values(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` |"
        )
    lines.extend(
        [
            "",
            "## 决策桶",
            "",
            f"- overall_decision_bucket：`{overall_decision(case_metrics, args.evidence_role)}`",
            "- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback 和 production bench。",
            "- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。",
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", default="log/board/fused_full_cloud_repeated")
    parser.add_argument("--output-dir", default="log/board/fused_full_cloud_repeated")
    parser.add_argument("--run-label", default="fused_full_cloud_repeated")
    parser.add_argument("--case-filter", default="all")
    parser.add_argument(
        "--row-source",
        choices=(
            "ordered-cloud-pair",
            "source-indexed-cloud-pair",
            "dual-indices-cloud-pair",
            "correspondence-pair",
        ),
        default="ordered-cloud-pair",
    )
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--std-bin", default="build/riscv/bench_transformation_estimation_svd_std")
    parser.add_argument("--rvv-bin", default="build/riscv/bench_transformation_estimation_svd_rvv")
    parser.add_argument("--device-label", default="RVV board")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    parser.add_argument(
        "--evidence-role",
        choices=("diagnostic", "production_direct"),
        default="diagnostic",
    )
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    summary_path = output_dir / "summary.md"
    manifest_path = output_dir / "evidence_manifest.json"
    doctor_path = output_dir / "evidence_doctor.md"
    runs = collect_runs(Path(args.input_dir), args.expected_runs)
    case_metrics = build_case_metrics(runs, args.evidence_role, args.row_source)
    manifest = build_manifest(runs, case_metrics, summary_path, args)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_summary(summary_path, runs, case_metrics, manifest_path, doctor_path, args)
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
