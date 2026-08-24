#!/usr/bin/env python3
"""
生成 transformation_estimation_svd_scale 的 board repeated summary（板卡重复测试摘要）。

输入是一组 `run-*` 目录，每个目录包含 Std/RVV bench log。脚本输出：

- `summary.md`：给 reviewer 阅读的 repeated board 摘要。
- `evidence_manifest.json`：给全局 Evidence Doctor（证据体检）读取的 manifest。

本脚本服务当前 topic 的 ordered-cloud-pair diagnostic（顺序点云对诊断）和
production public ordered-cloud-pair（生产公开顺序点云对）repeated evidence。
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
    r"max_(?:reference|public)_error:\s*(?P<value>[0-9]+(?:\.[0-9]+)?(?:e[+-]?[0-9]+)?)",
    re.IGNORECASE,
)
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
            continue
        if match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
            continue
        if match := DATASET_RE.match(line):
            metadata["dataset"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["log_checksum"] = match.group("checksum")
        if current_case and (match := MAX_ERROR_RE.search(line)):
            cases[current_case]["max_reference_error"] = float(match.group("value"))
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


def case_prefix(case_filter: str, evidence_role: str) -> str:
    if (case_filter == "more-custom-layout-scalar-double-sampling"
            or evidence_role == "production_public_more_custom_layout_scalar_double_sampling"):
        return "more custom layout scalar double sampling"
    if (case_filter == "custom-layout-scalar-double-diagnostic-scout"
            or evidence_role == "production_public_custom_layout_scalar_double_scout"):
        return "custom layout scalar double scout"
    if (case_filter == "row-source-generic-scalar-double-production-probe"
            or evidence_role == "production_public_generic_scalar_double_row_source_probe"):
        return "row source generic scalar double production probe"
    if (case_filter == "correspondence-sorted-copy-scalar-double-production-probe"
            or evidence_role == "production_public_correspondence_sorted_copy_scalar_double_probe"):
        return "row source correspondence sorted-copy scalar double production probe"
    if case_filter == "scalar-double-row-source-production-probe" or evidence_role == "production_public_scalar_double_row_source_probe":
        return "scalar double row source production probe"
    if case_filter == "scalar-double-production-probe" or evidence_role == "production_public_scalar_double_probe":
        return "scalar double production probe ordered-cloud-pair"
    if (case_filter == "generic-scalar-double-ordered-production-probe"
            or evidence_role == "production_public_generic_scalar_double_probe"):
        return "generic scalar double ordered production probe"
    if case_filter == "scalar-double-diagnostic-scout" or evidence_role == "scalar_double_board_diagnostic":
        return "scalar double diagnostic scout ordered-cloud-pair"
    if case_filter == "custom-layout-alignment-sensitivity" or evidence_role == "production_public_custom_layout_alignment_sensitivity":
        return "custom alignment row source scale"
    if case_filter == "custom-layout-padding-sensitivity" or evidence_role == "production_public_custom_layout_padding_sensitivity":
        return "custom padding row source scale"
    if case_filter == "custom-row-source-large-variance-profile" or evidence_role == "production_public_custom_row_source_profile":
        return "custom row source scale"
    if case_filter == "custom-xyz-aos-layout-sampling" or evidence_role == "production_public_custom_xyz_aos_layout":
        return "custom-xyz-aos-layout-sampling"
    if case_filter == "more-generic-xyz-aos-point-types-public" or evidence_role == "production_public_more_generic":
        return "more generic public scale"
    if case_filter == "generic-xyz-point-types-public" or evidence_role == "production_public_generic":
        return "generic public scale"
    if case_filter == "public-scale" or evidence_role == "production_direct":
        return "public scale ordered-cloud-pair"
    if case_filter == "correspondence-sorted-copy-production-probe" or evidence_role == "production_public_row_source_probe":
        return "row source correspondence sorted-copy production probe"
    if case_filter == "row-source-affine-index-fast-path-production-probe" or evidence_role == "production_public_affine_index_fast_path_probe":
        return "row source affine production probe"
    if case_filter == "row-source-scale" or evidence_role == "production_public_row_source":
        return "row source scale"
    if (case_filter in {
        "row-source-more-generic-xyz-aos-point-types",
        "row-source-all-more-generic-xyz-aos-matrix",
    } or evidence_role in {
        "production_public_row_source_more_generic",
        "production_public_row_source_all_more_generic",
    }):
        return "row source scale"
    if case_filter == "row-source-locality-order-profile" or evidence_role == "production_public_row_source_profile":
        return "row source scale"
    if case_filter == "matrix-local-scale" or evidence_role == "implementation_shape_diagnostic":
        return "matrix local scale simplification ordered-cloud-pair"
    return "fused scale candidate ordered-cloud-pair"


def point_type_from_label(label: str, prefix: str) -> str:
    remainder = label[len(prefix):].strip()
    if not remainder:
        return "PointXYZ"
    match = re.search(r"\s+\d+K$", remainder)
    if match:
        remainder = remainder[:match.start()].strip()
    if prefix == "custom layout scalar double scout":
        if remainder.startswith("ordered "):
            remainder = remainder[len("ordered "):].strip()
        return remainder if "->" in remainder else "PointXYZ"
    if prefix == "more custom layout scalar double sampling":
        if remainder.startswith("ordered "):
            remainder = remainder[len("ordered "):].strip()
        return remainder if "->" in remainder else "PointXYZ"
    if prefix in {
        "row source generic scalar double production probe",
        "scalar double row source production probe",
        "row source scale",
        "row source affine production probe",
        "row source correspondence sorted-copy scalar double production probe",
        "custom row source scale",
        "custom padding row source scale",
        "custom alignment row source scale",
    }:
        for row_prefix in ("source-indexed", "dual-indexed", "correspondence"):
            if remainder == row_prefix:
                return "PointXYZ"
            if remainder.startswith(row_prefix + " "):
                remainder = remainder[len(row_prefix):].strip()
                if remainder.startswith("locality-"):
                    parts = remainder.split(maxsplit=1)
                    remainder = parts[1] if len(parts) == 2 else ""
                return remainder if "->" in remainder else "PointXYZ"
        return remainder if "->" in remainder else "PointXYZ"
    return remainder if "->" in remainder else "PointXYZ"


def row_source_from_label(label: str, fallback: str) -> str:
    if label.startswith("more custom layout scalar double sampling ordered"):
        return "ordered-cloud-pair"
    if label.startswith("custom layout scalar double scout ordered"):
        return "ordered-cloud-pair"
    if label.startswith("row source generic scalar double production probe source-indexed"):
        return "source-indexed"
    if label.startswith("row source generic scalar double production probe dual-indexed"):
        return "dual-indexed"
    if label.startswith("row source generic scalar double production probe correspondence"):
        return "correspondence"
    if label.startswith("scalar double row source production probe source-indexed"):
        return "source-indexed"
    if label.startswith("scalar double row source production probe dual-indexed"):
        return "dual-indexed"
    if label.startswith("scalar double row source production probe correspondence"):
        return "correspondence"
    if label.startswith("custom alignment row source scale source-indexed"):
        return "source-indexed"
    if label.startswith("custom alignment row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("custom alignment row source scale correspondence"):
        return "correspondence"
    if label.startswith("custom padding row source scale source-indexed"):
        return "source-indexed"
    if label.startswith("custom padding row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("custom padding row source scale correspondence"):
        return "correspondence"
    if label.startswith("custom row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("custom row source scale correspondence"):
        return "correspondence"
    if label.startswith("row source affine production probe source-indexed"):
        return "source-indexed"
    if label.startswith("row source affine production probe dual-indexed"):
        return "dual-indexed"
    if label.startswith("row source affine production probe correspondence"):
        return "correspondence"
    if label.startswith("row source correspondence sorted-copy scalar double production probe"):
        return "correspondence"
    return fallback


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


def build_metrics(args: argparse.Namespace, runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    prefix = case_prefix(args.case_filter, args.evidence_role)
    custom_scalar_double_scout = (
        args.case_filter == "custom-layout-scalar-double-diagnostic-scout"
        or args.evidence_role == "production_public_custom_layout_scalar_double_scout"
    )
    more_custom_scalar_double_sampling = (
        args.case_filter == "more-custom-layout-scalar-double-sampling"
        or args.evidence_role == "production_public_more_custom_layout_scalar_double_sampling"
    )
    if custom_scalar_double_scout or more_custom_scalar_double_sampling:
        labels = sorted(
            [
                name for name in runs[0]["std"]["cases"]
                if (
                    "LocalPaddedXYZSource->LocalWideXYZTarget" in name
                    if custom_scalar_double_scout
                    else (
                        name.startswith("more custom layout scalar double sampling ordered")
                        or name.startswith("row source generic scalar double production probe")
                    )
                )
            ],
            key=lambda name: (
                row_source_from_label(name, "ordered-cloud-pair"),
                point_type_from_label(
                    name,
                    "custom layout scalar double scout"
                    if custom_scalar_double_scout and name.startswith("custom layout scalar double scout")
                    else "more custom layout scalar double sampling"
                    if name.startswith("more custom layout scalar double sampling")
                    else "row source generic scalar double production probe",
                ),
                size_value(size_suffix(name)),
            ),
        )
    elif prefix == "custom-xyz-aos-layout-sampling":
        labels = sorted(
            [
                name for name in runs[0]["std"]["cases"]
                if name.startswith("custom layout public scale") or
                "LocalPaddedXYZSource->LocalWideXYZTarget" in name
            ],
            key=lambda name: (
                point_type_from_label(
                    name,
                    "custom layout public scale"
                    if name.startswith("custom layout public scale")
                    else "row source scale",
                ),
                size_value(size_suffix(name)),
            ),
        )
    else:
        labels = sorted(
            [name for name in runs[0]["std"]["cases"] if name.startswith(prefix)],
            key=lambda name: (point_type_from_label(name, prefix), size_value(size_suffix(name))),
        )
    if not labels:
        raise SystemExit(f"no {prefix} cases found")
    result: dict[str, dict[str, Any]] = {}
    for label in labels:
        suffix = size_suffix(label)
        label_prefix = (
            "custom layout scalar double scout"
            if custom_scalar_double_scout and label.startswith("custom layout scalar double scout")
            else "more custom layout scalar double sampling"
            if more_custom_scalar_double_sampling and label.startswith("more custom layout scalar double sampling")
            else "row source generic scalar double production probe"
            if (custom_scalar_double_scout or more_custom_scalar_double_sampling)
            and label.startswith("row source generic scalar double production probe")
            else prefix
        )
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        std_checksums: set[str] = set()
        rvv_checksums: set[str] = set()
        max_errors: list[float] = []
        for run in runs:
            std_case = run["std"]["cases"][label]
            rvv_case = run["rvv"]["cases"][label]
            std_ms = float(std_case["ms"])
            rvv_ms = float(rvv_case["ms"])
            values.append(std_ms / rvv_ms)
            std_checksums.add(str(std_case.get("log_checksum")))
            rvv_checksums.add(str(rvv_case.get("log_checksum")))
            if rvv_case.get("max_reference_error") is not None:
                max_errors.append(float(rvv_case["max_reference_error"]))
            rows.append(
                {
                    "run": run["name"],
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    "ba": std_ms / rvv_ms,
                }
            )
        result[label] = {
            "label": label,
            "point_type": point_type_from_label(label, label_prefix),
            "size": size_value(suffix),
            "metric": metric(values),
            "rows": rows,
            "std_checksums": sorted(std_checksums),
            "rvv_checksums": sorted(rvv_checksums),
            "checksum_match": std_checksums == rvv_checksums,
            "max_reference_error": max(max_errors) if max_errors else None,
        }
    return result


def write_summary(path: Path,
                  args: argparse.Namespace,
                  metrics: dict[str, dict[str, Any]]) -> None:
    production_direct = args.evidence_role == "production_direct"
    generic_public = args.evidence_role == "production_public_generic"
    more_generic_public = args.evidence_role == "production_public_more_generic"
    custom_layout_public = args.evidence_role == "production_public_custom_xyz_aos_layout"
    custom_layout_alignment_sensitivity = args.evidence_role == "production_public_custom_layout_alignment_sensitivity"
    custom_layout_padding_sensitivity = args.evidence_role == "production_public_custom_layout_padding_sensitivity"
    custom_row_source_profile = args.evidence_role == "production_public_custom_row_source_profile"
    row_source_probe_public = args.evidence_role == "production_public_row_source_probe"
    affine_index_fast_path_probe = args.evidence_role == "production_public_affine_index_fast_path_probe"
    row_source_public = args.evidence_role == "production_public_row_source"
    row_source_more_generic = args.evidence_role == "production_public_row_source_more_generic"
    row_source_all_more_generic = args.evidence_role == "production_public_row_source_all_more_generic"
    row_source_profile = args.evidence_role == "production_public_row_source_profile"
    scalar_double_row_source_production = args.evidence_role == "production_public_scalar_double_row_source_probe"
    scalar_double_production = args.evidence_role == "production_public_scalar_double_probe"
    generic_scalar_double_production = args.evidence_role == "production_public_generic_scalar_double_probe"
    generic_scalar_double_row_source_production = args.evidence_role == "production_public_generic_scalar_double_row_source_probe"
    correspondence_sorted_copy_scalar_double_production = args.evidence_role == "production_public_correspondence_sorted_copy_scalar_double_probe"
    custom_scalar_double_scout = args.evidence_role == "production_public_custom_layout_scalar_double_scout"
    more_custom_scalar_double_sampling = args.evidence_role == "production_public_more_custom_layout_scalar_double_sampling"
    scalar_double_diagnostic = args.evidence_role == "scalar_double_board_diagnostic"
    evidence_role_text = (
        "`production_direct`（真实生产路径证据）"
        if production_direct
        else "`production_public_more_custom_layout_scalar_double_sampling`（更多自定义 layout 的 Scalar=double 生产公开采样证据，用户确认前不能采纳）"
        if more_custom_scalar_double_sampling
        else "`production_public_custom_layout_scalar_double_scout`（测试本地 custom xyz AoS layout 的 Scalar=double 生产公开候选侦察证据，用户确认前不能采纳）"
        if custom_scalar_double_scout
        else "`production_public_correspondence_sorted_copy_scalar_double_probe`（correspondence sorted-copy Scalar=double 生产公开探针证据；用户确认前不能采纳，且 clean adoption 还需要同边界 RVV-vs-RVV detail A/B）"
        if correspondence_sorted_copy_scalar_double_production
        else "`production_public_generic_scalar_double_row_source_probe`（常见 PCL xyz AoS 点型的 row-source Scalar=double 生产公开探针证据，PI5 后仍需用户确认）"
        if generic_scalar_double_row_source_production
        else "`production_public_scalar_double_row_source_probe`（Scalar=double row-source 生产公开探针证据，PI5 后仍需用户确认）"
        if scalar_double_row_source_production
        else "`production_public_scalar_double_probe`（Scalar=double 生产公开探针证据，PI5 后仍需用户确认）"
        if scalar_double_production
        else "`production_public_generic_scalar_double_probe`（常见 PCL xyz AoS 点型的 ordered Scalar=double 生产公开证据）"
        if generic_scalar_double_production
        else "`scalar_double_board_diagnostic`（Scalar=double 板卡诊断证据，不是生产证据）"
        if scalar_double_diagnostic
        else "`production_public_generic`（生产公开泛型点型证据）"
        if generic_public
        else "`production_public_more_generic`（生产公开更多常见泛型点型证据）"
        if more_generic_public
        else "`production_public_custom_xyz_aos_layout`（生产公开自定义 xyz AoS layout 采样证据）"
        if custom_layout_public
        else "`production_public_custom_layout_alignment_sensitivity`（生产公开自定义 layout alignment 敏感性证据）"
        if custom_layout_alignment_sensitivity
        else "`production_public_custom_layout_padding_sensitivity`（生产公开自定义 layout padding 敏感性证据）"
        if custom_layout_padding_sensitivity
        else "`production_public_custom_row_source_profile`（生产公开自定义 xyz AoS row-source 顺序剖析证据）"
        if custom_row_source_profile
        else "`production_public_row_source_probe`（生产公开 row-source probe 证据）"
        if row_source_probe_public
        else "`production_public_affine_index_fast_path_probe`（生产公开连续 index fast path 探针证据）"
        if affine_index_fast_path_probe
        else "`production_public_row_source`（生产公开 row-source 证据）"
        if row_source_public
        else "`production_public_row_source_more_generic`（生产公开 row-source 更多常见泛型点型证据）"
        if row_source_more_generic
        else "`production_public_row_source_all_more_generic`（生产公开 row-source 更多常见泛型点型全交叉证据）"
        if row_source_all_more_generic
        else "`production_public_row_source_profile`（生产公开 row-source 顺序剖析证据）"
        if row_source_profile
        else "`implementation_shape_diagnostic`（实现形态诊断证据）"
        if args.evidence_role == "implementation_shape_diagnostic"
        else "`pre_production_diagnostic`（接入生产前诊断）"
    )
    ba_text = (
        "B/A = Std public scale ms / RVV public scale ms；大于 1 表示 RVV production path 更快。"
        if production_direct
        else "B/A = Std more custom layout double public fallback ms / RVV more custom layout double public candidate ms；大于 1 表示 Phase 071 新增 custom layout sample 的 `Scalar=double` public RVV path 更快，用户确认前不能写成 adopted。"
        if more_custom_scalar_double_sampling
        else "B/A = Std custom layout double public fallback ms / RVV custom layout double public candidate ms；大于 1 表示本阶段 custom layout sample 的 `Scalar=double` public RVV path 更快，用户确认前不能写成 adopted。"
        if custom_scalar_double_scout
        else "B/A = Std correspondence double public fallback ms / RVV correspondence sorted-copy double production probe ms；大于 1 表示有界 sorted-copy double public RVV path 更快，但不能单独证明它优于既有 D64 gather RVV family。"
        if correspondence_sorted_copy_scalar_double_production
        else "B/A = Std generic public double row-source fallback ms / RVV generic public double row-source production probe ms；大于 1 表示常见 PCL xyz AoS row-source double public RVV path 更快，PI5 后仍需用户确认。"
        if generic_scalar_double_row_source_production
        else "B/A = Std public double row-source fallback ms / RVV public double row-source production probe ms；大于 1 表示有界 row-source production probe 更快，PI5 后仍需用户确认。"
        if scalar_double_row_source_production
        else "B/A = Std public double fallback ms / RVV public double production probe ms；大于 1 表示有界 production probe 更快，PI5 后仍需用户确认才能采纳。"
        if scalar_double_production
        else "B/A = Std generic public double fallback ms / RVV generic public double production path ms；大于 1 表示常见 PCL xyz AoS ordered double public RVV path 更快。"
        if generic_scalar_double_production
        else "B/A = Std scalar double fallback ms / RVV f64 widened diagnostic ms；大于 1 只表示测试专用 double RVV 诊断更快，不能替代 production evidence。"
        if scalar_double_diagnostic
        else "B/A = Std generic public scale ms / RVV generic public scale ms；大于 1 表示代表点型 public RVV path 更快。"
        if generic_public
        else "B/A = Std more-generic public scale ms / RVV more-generic public scale ms；大于 1 表示 Phase 051 新增点型 public RVV path 更快。"
        if more_generic_public
        else "B/A = Std custom xyz AoS public scale ms / RVV custom xyz AoS public scale ms；大于 1 表示自定义 layout 采样的 public RVV path 更快。"
        if custom_layout_public
        else "B/A = Std custom layout alignment public scale ms / RVV custom layout alignment public scale ms；大于 1 表示新增 alignment / stride layout 的 public RVV path 更快。"
        if custom_layout_alignment_sensitivity
        else "B/A = Std custom layout padding public scale ms / RVV custom layout padding public scale ms；大于 1 表示新增 padding / stride layout 的 public RVV path 更快。"
        if custom_layout_padding_sensitivity
        else "B/A = Std custom row-source profile public scale ms / RVV custom row-source profile public scale ms；大于 1 表示自定义 layout 大规模 row-source public RVV path 更快。"
        if custom_row_source_profile
        else "B/A = Std public probe ms / RVV public probe ms；大于 1 表示生产 probe 路径更快。"
        if row_source_probe_public
        else "B/A = Std affine index public probe ms / RVV affine index public probe ms；大于 1 表示连续 index fast path 生产探针更快。"
        if affine_index_fast_path_probe
        else "B/A = Std row-source public scale ms / RVV row-source public scale ms；大于 1 表示 row-source public RVV path 更快。"
        if row_source_public
        else "B/A = Std row-source more-generic public scale ms / RVV row-source more-generic public scale ms；大于 1 表示更多常见点型 row-source public RVV path 更快。"
        if row_source_more_generic
        else "B/A = Std row-source all-more-generic public scale ms / RVV row-source all-more-generic public scale ms；大于 1 表示更多常见点型 row-source 全交叉 public RVV path 更快。"
        if row_source_all_more_generic
        else "B/A = Std row-source profile public scale ms / RVV row-source profile public scale ms；大于 1 表示 profile 公开入口更快。"
        if row_source_profile
        else "B/A = Std legacy formula ms / RVV-build trace formula ms；该 B/A 只表示实现形态差异，不表示 RVV 指令收益。"
        if args.evidence_role == "implementation_shape_diagnostic"
        else "B/A = Std candidate ms / RVV candidate ms；大于 1 表示 RVV candidate 更快。"
    )
    lines = [
        "# transformation_estimation_svd_scale board repeated summary",
        "",
        "## 运行合同",
        "",
        f"- run label: `{args.run_label}`",
        f"- case-filter: `{args.case_filter}`",
        f"- evidence role: {evidence_role_text}",
        f"- {ba_text}",
        "",
        "## 结果表",
        "",
        "| case | runs B/A | median | min | max | bucket | max ref error | log checksum |",
        "| --- | --- | ---: | ---: | ---: | --- | ---: | --- |",
    ]
    for suffix, item in metrics.items():
        m = item["metric"]
        values = ", ".join(f"{v:.3f}" for v in m["values"])
        checksum = "match" if item["checksum_match"] else "mismatch"
        max_error = item["max_reference_error"]
        max_error_text = "n/a" if max_error is None else f"{max_error:.3e}"
        lines.append(
            f"| `{item['label']}` | {values} | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` | {max_error_text} | `{checksum}` |"
        )
    overall_buckets = {item["metric"]["bucket"] for item in metrics.values()}
    if overall_buckets == {"positive"}:
        overall = "positive"
    elif "negative" in overall_buckets:
        overall = "negative"
    elif "unstable" in overall_buckets:
        overall = "unstable"
    elif "weak_positive" in overall_buckets:
        overall = "weak_positive"
    else:
        overall = "neutral"
    lines += [
        "",
        "## Evidence Doctor 输入说明",
        "",
        f"- overall decision bucket: `{overall}`",
        (
            "- 本 summary 是 production direct evidence（真实生产路径证据），调用 public scale ordered-cloud-pair 入口。"
            if production_direct
            else "- 本 summary 是 more custom layout Scalar=double production public sampling evidence（更多自定义 layout 的生产公开采样证据），调用测试本地注册 custom xyz AoS 点型的 ordered、source-indexed、dual-indexed 和 correspondence public overload；它只覆盖 `LocalSVDScaleCompactXYZ*`、`LocalSVDScaleHugePaddingXYZ*`、`LocalSVDScaleAligned*XYZ*` 三组 dense 64K 样本，不外推到全部自定义点型、sorted-copy double 或 Phase 069/070 adoption。"
            if more_custom_scalar_double_sampling
            else "- 本 summary 是 custom layout Scalar=double production public scout evidence（生产公开候选侦察证据），调用测试本地注册 custom xyz AoS 点型的 ordered、source-indexed、dual-indexed 和 correspondence public overload；它只覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget`、dense、64K 和合法 index / correspondence，不外推到全部自定义点型、sorted-copy double 或 Phase 069 adoption。"
            if custom_scalar_double_scout
            else "- 本 summary 是 correspondence sorted-copy Scalar=double production public probe evidence（生产公开探针证据），调用真实 correspondence public overload；它只覆盖 `PointXYZ -> PointXYZ`、`Scalar=double`、dense、合法 shuffled correspondences 和 sorted-copy size/disorder gate。public Std/RVV positive 不能替代同边界 RVV-vs-RVV detail A/B，因此用户确认前不能写成 adopted。"
            if correspondence_sorted_copy_scalar_double_production
            else "- 本 summary 是 generic Scalar=double row-source production public probe evidence（生产公开 row-source 探针证据），调用常见 PCL xyz AoS 点型的真实 source-indexed、dual-indexed 和 correspondence public overload；它只覆盖白名单点型、dense、64K 和 row-source，不外推到 custom layout double、sorted-copy double 或任意自定义点型全集；PI5 后仍需用户确认才能写成 adopted。"
            if generic_scalar_double_row_source_production
            else "- 本 summary 是 Scalar=double row-source production public probe evidence（生产公开 row-source 探针证据），调用真实 source-indexed、dual-indexed 和 correspondence public overload；它只覆盖 `PointXYZ -> PointXYZ` / `Scalar=double` / dense / 64K，PI5 后仍需用户确认才能写成 adopted。"
            if scalar_double_row_source_production
            else "- 本 summary 是 Scalar=double production public probe evidence（生产公开探针证据），调用真实 public ordered overload；它只覆盖 `PointXYZ -> PointXYZ` / `Scalar=double` / dense / 64K，PI5 后仍需用户确认才能写成 adopted。"
            if scalar_double_production
            else "- 本 summary 是 generic Scalar=double production public evidence（生产公开证据），调用常见 PCL xyz AoS 点型的真实 ordered public overload；它只覆盖白名单点型、dense、64K 和 ordered-cloud-pair，不外推到 row-source generic double 或 custom layout double。"
            if generic_scalar_double_production
            else "- 本 summary 是 Scalar=double board diagnostic evidence（板卡诊断证据），只比较测试专用 scalar double fallback 与 RVV f64 widened candidate；它不能替代 production dispatch、fallback、ASM 或 public overload evidence。"
            if scalar_double_diagnostic
            else "- 本 summary 是 production public generic evidence（生产公开泛型点型证据），调用代表点型 public generic 入口。"
            if generic_public
            else "- 本 summary 是 production public more-generic evidence（生产公开更多常见泛型点型证据），调用 Phase 051 新增点型 public generic 入口。"
            if more_generic_public
            else "- 本 summary 是 production public custom xyz AoS layout sampling evidence（生产公开自定义 xyz AoS layout 采样证据），调用测试本地注册点型的 ordered 和 row-source 公开入口；它不能外推到任意自定义点型全集。"
            if custom_layout_public
            else "- 本 summary 是 production public custom layout alignment sensitivity evidence（生产公开自定义 layout alignment 敏感性证据），只覆盖新增 alignas 注册点型组合的 64K/256K row-source 公开入口。"
            if custom_layout_alignment_sensitivity
            else "- 本 summary 是 production public custom layout padding sensitivity evidence（生产公开自定义 layout padding 敏感性证据），只覆盖两个新增注册点型组合的 64K/256K row-source 公开入口。"
            if custom_layout_padding_sensitivity
            else "- 本 summary 是 production public custom row-source profile evidence（生产公开自定义 row-source 顺序剖析证据），只覆盖两个测试本地注册点型的 256K dual-indexed / correspondence order pattern。"
            if custom_row_source_profile
            else "- 本 summary 是 production public row-source probe evidence（生产公开 row-source 探针证据），调用真实 correspondence 公开入口；它证明当前有界生产探针相对标量公开路径的收益，但 PI5 后仍需用户确认才能写成 adopted。"
            if row_source_probe_public
            else "- 本 summary 是 production public affine index fast path probe evidence（生产公开连续 index fast path 探针证据），调用 source-indexed、dual-indexed 和 correspondence 公开入口；它证明包含连续性检测成本后的有界生产探针收益，但 PI5 后仍需用户确认才能写成 adopted。"
            if affine_index_fast_path_probe
            else "- 本 summary 是 production public row-source evidence（生产公开 row-source 证据），调用 source-indexed、dual-indexed 和 correspondence 公开入口。"
            if row_source_public
            else "- 本 summary 是 production public row-source more-generic evidence（生产公开 row-source 更多常见泛型点型证据），调用更多常见 PCL xyz AoS 点型的 row-source 公开入口。"
            if row_source_more_generic
            else "- 本 summary 是 production public row-source all-more-generic evidence（生产公开 row-source 更多常见泛型点型全交叉证据），调用 Phase 051 五个常见 PCL xyz AoS 点型组合的 source-indexed、dual-indexed 和 correspondence 公开入口。"
            if row_source_all_more_generic
            else "- 本 summary 是 production public row-source profile evidence（生产公开 row-source 顺序剖析证据），调用不同 order pattern 的 row-source 公开入口。"
            if row_source_profile
            else "- 本 summary 是 diagnostic evidence（诊断证据），不能替代 production direct evidence。"
        ),
        "- `log checksum` 是 1e-6 量化日志指纹；RVV reduction tree（规约树）改变时允许与 scalar 不 bit-identical，correctness 以 gtest 和 `max ref error <= 2e-3` 为准。",
        (
            "- matrix-local-scale 的 Std/RVV binary 只承载 legacy formula 与 trace formula 的 A/B；它不是 RVV intrinsic 或 production dispatch 证据。"
            if args.evidence_role == "implementation_shape_diagnostic"
            else ""
        ),
        "- raw run logs 默认 local-only；提交前需按 evidence policy 单独审查。",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def build_manifest(args: argparse.Namespace,
                   runs: list[dict[str, Any]],
                   metrics: dict[str, dict[str, Any]]) -> dict[str, Any]:
    first_meta = runs[0]["std"]["metadata"] if runs else {}
    production_direct = args.evidence_role == "production_direct"
    generic_public = args.evidence_role == "production_public_generic"
    more_generic_public = args.evidence_role == "production_public_more_generic"
    custom_layout_public = args.evidence_role == "production_public_custom_xyz_aos_layout"
    custom_layout_alignment_sensitivity = args.evidence_role == "production_public_custom_layout_alignment_sensitivity"
    custom_layout_padding_sensitivity = args.evidence_role == "production_public_custom_layout_padding_sensitivity"
    custom_row_source_profile = args.evidence_role == "production_public_custom_row_source_profile"
    row_source_probe_public = args.evidence_role == "production_public_row_source_probe"
    affine_index_fast_path_probe = args.evidence_role == "production_public_affine_index_fast_path_probe"
    row_source_public = args.evidence_role == "production_public_row_source"
    row_source_more_generic = args.evidence_role == "production_public_row_source_more_generic"
    row_source_all_more_generic = args.evidence_role == "production_public_row_source_all_more_generic"
    row_source_profile = args.evidence_role == "production_public_row_source_profile"
    implementation_shape = args.evidence_role == "implementation_shape_diagnostic"
    scalar_double_row_source_production = args.evidence_role == "production_public_scalar_double_row_source_probe"
    scalar_double_production = args.evidence_role == "production_public_scalar_double_probe"
    generic_scalar_double_production = args.evidence_role == "production_public_generic_scalar_double_probe"
    generic_scalar_double_row_source_production = args.evidence_role == "production_public_generic_scalar_double_row_source_probe"
    correspondence_sorted_copy_scalar_double_production = args.evidence_role == "production_public_correspondence_sorted_copy_scalar_double_probe"
    custom_scalar_double_scout = args.evidence_role == "production_public_custom_layout_scalar_double_scout"
    more_custom_scalar_double_sampling = args.evidence_role == "production_public_more_custom_layout_scalar_double_sampling"
    scalar_double_diagnostic = args.evidence_role == "scalar_double_board_diagnostic"
    case_kind = args.case_kind or (
        "board_production_direct" if production_direct else "test-only-diagnostic"
    )
    if scalar_double_diagnostic and not args.case_kind:
        case_kind = "board_scalar_double_diagnostic"
    if scalar_double_row_source_production and not args.case_kind:
        case_kind = "board_scalar_double_row_source_production_probe"
    if scalar_double_production and not args.case_kind:
        case_kind = "board_scalar_double_production_probe"
    if generic_scalar_double_production and not args.case_kind:
        case_kind = "board_generic_scalar_double_ordered_production_probe"
    if generic_scalar_double_row_source_production and not args.case_kind:
        case_kind = "board_generic_scalar_double_row_source_production_probe"
    if correspondence_sorted_copy_scalar_double_production and not args.case_kind:
        case_kind = "board_correspondence_sorted_copy_scalar_double_production_probe"
    if custom_scalar_double_scout and not args.case_kind:
        case_kind = "board_custom_layout_scalar_double_diagnostic_scout"
    if more_custom_scalar_double_sampling and not args.case_kind:
        case_kind = "board_more_custom_layout_scalar_double_sampling"
    if generic_public and not args.case_kind:
        case_kind = "board_generic_public"
    if more_generic_public and not args.case_kind:
        case_kind = "board_more_generic_public"
    if custom_layout_public and not args.case_kind:
        case_kind = "board_custom_xyz_aos_layout_sampling"
    if custom_layout_alignment_sensitivity and not args.case_kind:
        case_kind = "board_custom_layout_alignment_sensitivity"
    if custom_layout_padding_sensitivity and not args.case_kind:
        case_kind = "board_custom_layout_padding_sensitivity"
    if custom_row_source_profile and not args.case_kind:
        case_kind = "board_custom_row_source_large_variance_profile"
    if row_source_probe_public and not args.case_kind:
        case_kind = "board_row_source_public_probe"
    if affine_index_fast_path_probe and not args.case_kind:
        case_kind = "board_affine_index_fast_path_public_probe"
    if row_source_public and not args.case_kind:
        case_kind = "board_row_source_public"
    if row_source_more_generic and not args.case_kind:
        case_kind = "board_row_source_more_generic_public"
    if row_source_all_more_generic and not args.case_kind:
        case_kind = "board_row_source_all_more_generic_public"
    if row_source_profile and not args.case_kind:
        case_kind = "board_row_source_profile"
    if implementation_shape and not args.case_kind:
        case_kind = "board_implementation_shape"
    baseline_label = (
        "std-public-scale-ordered-cloud-pair"
        if production_direct
        else "std-more-custom-layout-double-public-fallback"
        if more_custom_scalar_double_sampling
        else "std-custom-layout-double-public-fallback"
        if custom_scalar_double_scout
        else "std-correspondence-sorted-copy-scalar-double-production-probe"
        if correspondence_sorted_copy_scalar_double_production
        else "std-public-double-row-source-scale"
        if scalar_double_row_source_production
        else "std-public-double-scale-ordered-cloud-pair"
        if scalar_double_production
        else "std-generic-public-double-scale-ordered-cloud-pair"
        if generic_scalar_double_production
        else "std-scalar-double-fallback"
        if scalar_double_diagnostic
        else "std-generic-public-scale"
        if generic_public
        else "std-more-generic-public-scale"
        if more_generic_public
        else "std-custom-xyz-aos-layout-public-scale"
        if custom_layout_public
        else "std-custom-layout-alignment-sensitivity-scale"
        if custom_layout_alignment_sensitivity
        else "std-custom-layout-padding-sensitivity-scale"
        if custom_layout_padding_sensitivity
        else "std-custom-row-source-profile-scale"
        if custom_row_source_profile
        else "std-correspondence-sorted-copy-production-probe"
        if row_source_probe_public
        else "std-affine-index-fast-path-production-probe"
        if affine_index_fast_path_probe
        else "std-row-source-public-scale"
        if row_source_public
        else "std-row-source-more-generic-public-scale"
        if row_source_more_generic
        else "std-row-source-all-more-generic-public-scale"
        if row_source_all_more_generic
        else "std-row-source-profile-scale"
        if row_source_profile
        else "std-legacy-rotated-cloud-scale"
        if implementation_shape
        else "std-fused-scale-reference"
    )
    candidate_label = (
        "rvv-public-scale-ordered-cloud-pair"
        if production_direct
        else "rvv-more-custom-layout-double-f64-widened-sampling"
        if more_custom_scalar_double_sampling
        else "rvv-custom-layout-double-f64-widened-scout"
        if custom_scalar_double_scout
        else "rvv-correspondence-sorted-copy-scalar-double-production-probe"
        if correspondence_sorted_copy_scalar_double_production
        else "rvv-public-double-row-source-f64-widened-probe"
        if scalar_double_row_source_production
        else "rvv-public-double-f64-widened-probe"
        if scalar_double_production
        else "rvv-generic-public-double-f64-widened-probe"
        if generic_scalar_double_production
        else "rvv-f64-widened-diagnostic"
        if scalar_double_diagnostic
        else "rvv-generic-public-scale"
        if generic_public
        else "rvv-more-generic-public-scale"
        if more_generic_public
        else "rvv-custom-xyz-aos-layout-public-scale"
        if custom_layout_public
        else "rvv-custom-layout-alignment-sensitivity-scale"
        if custom_layout_alignment_sensitivity
        else "rvv-custom-layout-padding-sensitivity-scale"
        if custom_layout_padding_sensitivity
        else "rvv-custom-row-source-profile-scale"
        if custom_row_source_profile
        else "rvv-correspondence-sorted-copy-production-probe"
        if row_source_probe_public
        else "rvv-affine-index-fast-path-production-probe"
        if affine_index_fast_path_probe
        else "rvv-row-source-public-scale"
        if row_source_public
        else "rvv-row-source-more-generic-public-scale"
        if row_source_more_generic
        else "rvv-row-source-all-more-generic-public-scale"
        if row_source_all_more_generic
        else "rvv-row-source-profile-scale"
        if row_source_profile
        else "rvv-build-trace-rh-scale"
        if implementation_shape
        else "rvv-fused-scale-candidate"
    )
    boundary = (
        "production_public_ordered_cloud_pair"
        if production_direct
        else "production_public_more_custom_layout_scalar_double_sampling"
        if more_custom_scalar_double_sampling
        else "production_public_custom_layout_scalar_double_scout"
        if custom_scalar_double_scout
        else "production_public_correspondence_sorted_copy_scalar_double_probe"
        if correspondence_sorted_copy_scalar_double_production
        else "production_public_scalar_double_row_source_probe"
        if scalar_double_row_source_production
        else "production_public_scalar_double_ordered_cloud_pair_probe"
        if scalar_double_production
        else "production_public_generic_scalar_double_ordered_cloud_pair_probe"
        if generic_scalar_double_production
        else "test_helper_scalar_double_ordered_cloud_pair"
        if scalar_double_diagnostic
        else "production_public_generic_xyz_point_types"
        if generic_public
        else "production_public_more_generic_xyz_aos_point_types"
        if more_generic_public
        else "production_public_custom_xyz_aos_layout_sampling"
        if custom_layout_public
        else "production_public_custom_layout_alignment_sensitivity"
        if custom_layout_alignment_sensitivity
        else "production_public_custom_layout_padding_sensitivity"
        if custom_layout_padding_sensitivity
        else "production_public_custom_row_source_large_variance_profile"
        if custom_row_source_profile
        else "production_public_correspondence_sorted_copy_probe"
        if row_source_probe_public
        else "production_public_affine_index_fast_path_probe"
        if affine_index_fast_path_probe
        else "production_public_row_source_scale"
        if row_source_public
        else "production_public_row_source_more_generic_xyz_aos_point_types"
        if row_source_more_generic
        else "production_public_row_source_all_more_generic_xyz_aos_matrix"
        if row_source_all_more_generic
        else "production_public_row_source_profile"
        if row_source_profile
        else "test_helper_matrix_local_scale"
        if implementation_shape
        else "test_helper"
    )
    asm_boundary = (
        "production TransformationEstimationSVDScale ordered public overload"
        if production_direct
        else "production TransformationEstimationSVDScale more custom layout double public overloads"
        if more_custom_scalar_double_sampling
        else "production TransformationEstimationSVDScale custom layout double public overloads"
        if custom_scalar_double_scout
        else "production row-source correspondence sorted-copy double public overload"
        if correspondence_sorted_copy_scalar_double_production
        else "production TransformationEstimationSVDScale row-source double public overloads"
        if scalar_double_row_source_production
        else "production TransformationEstimationSVDScale ordered double public overload"
        if scalar_double_production
        else "production TransformationEstimationSVDScale generic ordered double public overload"
        if generic_scalar_double_production
        else "test-only RVV f64 widened accumulation helper"
        if scalar_double_diagnostic
        else "production generic public overload"
        if generic_public
        else "production more-generic public overload"
        if more_generic_public
        else "production custom xyz AoS layout public overloads"
        if custom_layout_public
        else "production custom layout alignment row-source public overloads"
        if custom_layout_alignment_sensitivity
        else "production custom layout padding row-source public overloads"
        if custom_layout_padding_sensitivity
        else "production custom xyz AoS row-source public overloads"
        if custom_row_source_profile
        else "production row-source correspondence sorted-copy public overload"
        if row_source_probe_public
        else "production row-source affine-index fast-path public overloads"
        if affine_index_fast_path_probe
        else "production row-source public overloads"
        if row_source_public
        else "production row-source more-generic public overloads"
        if row_source_more_generic
        else "production row-source all-more-generic public overloads"
        if row_source_all_more_generic
        else "production row-source profile public overloads"
        if row_source_profile
        else "not_applicable_scalar_formula_diagnostic"
        if implementation_shape
        else "bench_test_helper"
    )
    gate = (
        "ordered_dense_traits_gated_xyz_aos_float"
        if generic_public or more_generic_public or custom_layout_public
        else "dense_traits_gated_more_custom_xyz_aos_double"
        if more_custom_scalar_double_sampling
        else "dense_traits_gated_custom_xyz_aos_double"
        if custom_scalar_double_scout
        else "correspondence_dense_pointxyz_double_sorted_copy_size_disorder"
        if correspondence_sorted_copy_scalar_double_production
        else "row_source_dense_pointxyz_double"
        if scalar_double_row_source_production
        else "ordered_dense_pointxyz_double"
        if scalar_double_production
        else "ordered_dense_common_pcl_xyz_aos_double"
        if generic_scalar_double_production
        else "ordered_dense_pointxyz_double"
        if scalar_double_diagnostic
        else "row_source_dense_traits_gated_custom_xyz_aos_float"
        if custom_layout_alignment_sensitivity
        else "row_source_dense_traits_gated_custom_xyz_aos_float"
        if custom_layout_padding_sensitivity
        else "row_source_dense_traits_gated_custom_xyz_aos_float"
        if custom_row_source_profile
        else "row_source_dense_traits_gated_xyz_aos_float"
        if row_source_probe_public
        else "row_source_dense_traits_gated_xyz_aos_float_contiguous_index_offsets"
        if affine_index_fast_path_probe
        else "row_source_dense_traits_gated_xyz_aos_float"
        if row_source_public
        else "row_source_dense_traits_gated_xyz_aos_float"
        if row_source_more_generic
        else "row_source_dense_traits_gated_xyz_aos_float"
        if row_source_all_more_generic
        else "row_source_dense_traits_gated_xyz_aos_float"
        if row_source_profile
        else "ordered_dense_pointxyz_float"
    )
    comparisons = []
    for suffix, item in metrics.items():
        m = item["metric"]
        std_checksums = item["std_checksums"]
        rvv_checksums = item["rvv_checksums"]
        checksum = std_checksums[0] if len(std_checksums) == 1 else "varies"
        candidate_checksum = rvv_checksums[0] if len(rvv_checksums) == 1 else "varies"
        max_reference_error = item["max_reference_error"]
        row_source = row_source_from_label(item["label"], args.row_source)
        comparisons.append(
            {
                "name": item["label"],
                "group": row_source,
                "evidence_role": args.evidence_role,
                "case_kind": case_kind,
                "point_type": item["point_type"],
                "size": item["size"],
                "run_count": len(runs),
                "log_checksum": {
                    "std": checksum,
                    "rvv": candidate_checksum,
                },
                    "max_reference_error": {
                        "rvv": max_reference_error,
                        "tolerance": 5.0e-8
                        if (scalar_double_production
                            or scalar_double_row_source_production
                            or generic_scalar_double_production
                            or generic_scalar_double_row_source_production
                            or more_custom_scalar_double_sampling
                            or custom_scalar_double_scout
                            or correspondence_sorted_copy_scalar_double_production)
                        else 2.0e-3,
                },
                "baseline": {
                    "label": baseline_label,
                    "boundary": boundary,
                    "wrapper": "same_outer_wrapper",
                    "row_source": row_source,
                    "solve": True,
                    "checksum_policy": "tolerance_checked_by_gtest_matrix_max_abs_error",
                    "timer_boundary": "estimate_only",
                    "gate": gate,
                    "mask": "none_dense_only",
                    "reduction": (
                        "legacy_rotated_cloud_matrix_multiply"
                        if implementation_shape
                        else "more_custom_layout_scalar_double_public_fallback"
                        if more_custom_scalar_double_sampling
                        else "custom_layout_scalar_double_public_fallback"
                        if custom_scalar_double_scout
                        else "scalar_double_public_correspondence_fallback"
                        if correspondence_sorted_copy_scalar_double_production
                        else "scalar_double_public_row_source_fallback"
                        if scalar_double_row_source_production
                        else "scalar_double_public_fallback"
                        if scalar_double_production
                        else "generic_scalar_double_public_fallback"
                        if generic_scalar_double_production
                        else "scalar_double_fused_accumulation"
                        if scalar_double_diagnostic
                        else "scalar_ordered_accumulation"
                    ),
                    "asm_boundary": "not_applicable_std",
                    "rvv_instr_count": 0,
                    "std_ms": statistics.median([row["std_ms"] for row in item["rows"]]),
                    "rvv_ms": statistics.median([row["std_ms"] for row in item["rows"]]),
                },
                "candidate": {
                    "label": candidate_label,
                    "boundary": boundary,
                    "wrapper": "same_outer_wrapper",
                    "row_source": row_source,
                    "solve": True,
                    "checksum_policy": "tolerance_checked_by_gtest_matrix_max_abs_error",
                    "timer_boundary": "estimate_only",
                    "gate": gate,
                    "mask": "none_dense_only",
                    "reduction": (
                        "trace_rh_scalar_formula"
                        if implementation_shape
                        else "rvv_f64_widened_more_custom_layout_public_accumulation"
                        if more_custom_scalar_double_sampling
                        else "rvv_f64_widened_custom_layout_public_accumulation"
                        if custom_scalar_double_scout
                        else "rvv_f64_widened_correspondence_sorted_copy_accumulation"
                        if correspondence_sorted_copy_scalar_double_production
                        else "rvv_f64_widened_row_source_public_accumulation"
                        if scalar_double_row_source_production
                        else "rvv_f64_widened_public_accumulation"
                        if scalar_double_production
                        else "rvv_f64_widened_generic_public_accumulation"
                        if generic_scalar_double_production
                        else "rvv_f64_widened_accumulation"
                        if scalar_double_diagnostic
                        else "rvv_vfredosum_scale_accumulation"
                    ),
                    "asm_boundary": asm_boundary,
                    "rvv_instr_count": (
                        0
                        if implementation_shape
                        else "see asm summary"
                        if (scalar_double_production
                            or scalar_double_row_source_production
                            or generic_scalar_double_production
                            or generic_scalar_double_row_source_production
                            or more_custom_scalar_double_sampling
                            or custom_scalar_double_scout
                            or correspondence_sorted_copy_scalar_double_production)
                        else "not_collected_for_phase064"
                        if scalar_double_diagnostic
                        else "see asm summary"
                    ),
                    "std_ms": statistics.median([row["rvv_ms"] for row in item["rows"]]),
                    "rvv_ms": statistics.median([row["rvv_ms"] for row in item["rows"]]),
                },
                "ba_values": m["values"],
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": (
                "transformation_estimation_svd_scale ordered-cloud-pair repeated board production direct"
                if production_direct
                else "transformation_estimation_svd_scale more custom layout Scalar=double repeated board sampling"
                if more_custom_scalar_double_sampling
                else "transformation_estimation_svd_scale custom layout Scalar=double repeated board scout"
                if custom_scalar_double_scout
                else "transformation_estimation_svd_scale correspondence sorted-copy Scalar=double repeated board production probe"
                if correspondence_sorted_copy_scalar_double_production
                else "transformation_estimation_svd_scale row-source Scalar=double repeated board production probe"
                if scalar_double_row_source_production
                else "transformation_estimation_svd_scale Scalar=double repeated board production probe"
                if scalar_double_production
                else "transformation_estimation_svd_scale generic Scalar=double ordered repeated board production evidence"
                if generic_scalar_double_production
                else "transformation_estimation_svd_scale Scalar=double repeated board diagnostic"
                if scalar_double_diagnostic
                else "transformation_estimation_svd_scale generic public repeated board evidence"
                if generic_public
                else "transformation_estimation_svd_scale more-generic public repeated board evidence"
                if more_generic_public
                else "transformation_estimation_svd_scale custom xyz AoS layout sampling repeated board evidence"
                if custom_layout_public
                else "transformation_estimation_svd_scale custom layout alignment sensitivity repeated board evidence"
                if custom_layout_alignment_sensitivity
                else "transformation_estimation_svd_scale custom layout padding sensitivity repeated board evidence"
                if custom_layout_padding_sensitivity
                else "transformation_estimation_svd_scale custom row-source large variance profile repeated board evidence"
                if custom_row_source_profile
                else "transformation_estimation_svd_scale affine index fast-path production probe repeated board evidence"
                if affine_index_fast_path_probe
                else "transformation_estimation_svd_scale row-source repeated board evidence"
                if row_source_public
                else "transformation_estimation_svd_scale row-source more-generic repeated board evidence"
                if row_source_more_generic
                else "transformation_estimation_svd_scale row-source all-more-generic repeated board evidence"
                if row_source_all_more_generic
                else "transformation_estimation_svd_scale row-source locality/order profile repeated board evidence"
                if row_source_profile
                else "transformation_estimation_svd_scale ordered-cloud-pair repeated board diagnostic"
            ),
            "evidence_role": args.evidence_role,
            "summary_path": str(Path(args.output_dir) / "summary.md"),
            "label": args.run_label,
            "analysis_script": "test-rvv/registration/transformation_estimation_svd_scale/script/generate_tesvd_scale_board_repeated_summary.py",
            "metadata": {
                "device": "board",
                "iterations": first_meta.get("iterations"),
                "warmup_iterations": first_meta.get("warmup_iterations"),
                "run_count": len(runs),
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": {
                    "std": sha256_file(Path(args.std_bin)),
                    "rvv": sha256_file(Path(args.rvv_bin)),
                },
                "dataset": first_meta.get("dataset", "not_recorded"),
            },
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
            ],
            "thresholds": {
                "min_repeated_runs": args.expected_runs,
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
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-filter", required=True)
    parser.add_argument("--row-source", required=True)
    parser.add_argument("--evidence-role", default="pre_production_diagnostic")
    parser.add_argument("--case-kind", default=None)
    parser.add_argument("--expected-runs", type=int, required=True)
    parser.add_argument("--std-bin", required=True)
    parser.add_argument("--rvv-bin", required=True)
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    runs = collect_runs(input_dir, args.expected_runs)
    metrics = build_metrics(args, runs)
    write_summary(output_dir / "summary.md", args, metrics)
    manifest = build_manifest(args, runs, metrics)
    (output_dir / "evidence_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
