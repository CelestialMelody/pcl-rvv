#!/usr/bin/env python3
"""Generate the sac_model_line board Evidence Doctor manifest.

这个脚本只解析当前 topic 的 `bench_sac_model_line` 输出。它把 5-run
board（板卡）日志转换成 Evidence Doctor（证据体检）可读取的 JSON
manifest（证据清单）。默认模式下，`public countWithinDistance`、
`public selectWithinDistance` 和 `public getDistancesToModel` 只作为
production public companion（公开生产入口伴随项）；带 `--production-direct`
运行时，这些 public rows（公开入口行）表示接入后的 production direct
（真实生产入口直连）证据。`diagnostic candidate ...` 行是
production-shaped diagnostic（生产形态诊断），用于判断后续是否值得做有界
production probe（生产探针）。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]

ITEM_METADATA: dict[str, dict[str, str]] = {
    "public countWithinDistance": {
        "name": "sac_model_line public countWithinDistance PointXYZ 65536",
        "evidence_role": "summary_only_unknown",
        "case_kind": "production-public-companion",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelLineDiagnostic::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "mask": "line_cross3_sqr_distance_strict_less_than_threshold",
        "reduction": "inlier_count",
        "asm_symbol": "not_applicable_no_production_rvv_dispatch",
    },
    "diagnostic candidate countWithinDistance": {
        "name": "sac_model_line diagnostic candidate countWithinDistance PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelLineDiagnostic::countWithinDistanceCandidate",
        "timer_boundary": "countWithinDistance_candidate_only",
        "mask": "line_cross3_sqr_distance_strict_less_than_threshold",
        "reduction": "rvv_mask_popcount_inlier_count",
        "asm_symbol": "countWithinDistanceCandidateRVV",
    },
    "public selectWithinDistance": {
        "name": "sac_model_line public selectWithinDistance PointXYZ 65536",
        "evidence_role": "summary_only_unknown",
        "case_kind": "production-public-companion",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelLineDiagnostic::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "line_cross3_sqr_distance_strict_less_than_threshold",
        "reduction": "ordered_inliers_and_error_sqr_dists",
        "asm_symbol": "not_applicable_no_production_rvv_dispatch",
    },
    "diagnostic candidate selectWithinDistance": {
        "name": "sac_model_line diagnostic candidate selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelLineDiagnostic::selectWithinDistanceCandidate",
        "timer_boundary": "selectWithinDistance_candidate_only",
        "mask": "line_cross3_sqr_distance_strict_less_than_threshold",
        "reduction": "vcompress_ordered_inliers_and_error_sqr_dists",
        "asm_symbol": "selectWithinDistanceCandidateRVV",
    },
    "public getDistancesToModel": {
        "name": "sac_model_line public getDistancesToModel PointXYZ 65536",
        "evidence_role": "summary_only_unknown",
        "case_kind": "production-public-companion",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelLineDiagnostic::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "not_applicable_dense_distance_output",
        "reduction": "sqrt_dense_double_distance_vector",
        "asm_symbol": "not_applicable_no_production_rvv_dispatch",
    },
    "diagnostic candidate getDistancesToModel": {
        "name": "sac_model_line diagnostic candidate getDistancesToModel PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelLineDiagnostic::getDistancesToModelCandidate",
        "timer_boundary": "getDistancesToModel_candidate_only",
        "mask": "not_applicable_dense_distance_output",
        "reduction": "rvv_sqr_distance_then_scalar_sqrt_dense_double_store",
        "asm_symbol": "getDistancesToModelCandidateRVV",
    },
    "diagnostic candidate getDistancesToModel vfsqrt": {
        "name": "sac_model_line diagnostic candidate getDistancesToModel vfsqrt PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelLineDiagnostic::getDistancesToModelVFSqrtCandidate",
        "timer_boundary": "getDistancesToModel_vfsqrt_candidate_only",
        "mask": "not_applicable_dense_distance_output",
        "reduction": "rvv_sqr_distance_then_vfsqrt_and_dense_double_store",
        "asm_symbol": "getDistancesToModelVFSqrtCandidateRVV",
    },
}

PRODUCTION_ITEMS = [
    "public countWithinDistance",
    "public selectWithinDistance",
    "public getDistancesToModel",
]

IDENTITY_MODE_LABELS = {
    "identity": {
        "case_kind": "production-detail",
        "row_source": "direct_indexed_identity_indices",
        "layout": "identity indices",
        "summary_label": "5-run board repeated identity RVV-vs-RVV summary",
    },
    "identity-shuffled": {
        "case_kind": "production-detail-non-regression",
        "row_source": "direct_indexed_shuffled_adjacent_pairs",
        "layout": "shuffled adjacent pairs",
        "summary_label": "5-run board repeated shuffled RVV-vs-RVV non-regression summary",
    },
}

DATASET_RE = re.compile(
    r"^Dataset:\s+(?P<dataset>.+?)\s+\(points=(?P<size>\d+),\s*(?P<layout>.+)\)"
)
ITER_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s+(?P<warmup>\d+)")
BUILD_RE = re.compile(r"^Build:\s+(?P<build>Std|RVV)")
CHECKSUM_RE = re.compile(r"^Checksum:\s+(?P<checksum>\S+)")
ITEM_CHECKSUM_RE = re.compile(r"^Checksum\s+(?P<item>.+?)\s+:\s+(?P<checksum>\S+)")
BENCH_ITEM_RE = re.compile(r"^(?P<item>.+?)\s+:\s+(?P<ms>[0-9.]+)\s+ms/iter")
SYMBOL_HEADER_RE = re.compile(r"^[0-9a-fA-F]+ <(?P<symbol>.+)>:$")
RVV_INSTRUCTION_RE = re.compile(r"\s+v[a-z0-9]+(?:\.[a-z0-9]+)*\s+")


def repo_root(start: Path) -> Path:
    for path in (start.resolve(), *start.resolve().parents):
        if (path / ".git").exists():
            return path
    return Path.cwd().resolve()


def repo_relative(path: Path) -> str:
    root = repo_root(TOPIC_ROOT)
    try:
        return path.resolve().relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


DEFAULT_ITEMS = ["public countWithinDistance", "diagnostic candidate countWithinDistance"]


def parse_item_list(text: str | None) -> list[str]:
    if not text:
        return list(DEFAULT_ITEMS)
    aliases = {
        "count": ["public countWithinDistance", "diagnostic candidate countWithinDistance"],
        "select": ["public selectWithinDistance", "diagnostic candidate selectWithinDistance"],
        "getdist": ["public getDistancesToModel", "diagnostic candidate getDistancesToModel"],
        "getDistances": ["public getDistancesToModel", "diagnostic candidate getDistancesToModel"],
        "get_distances": ["public getDistancesToModel", "diagnostic candidate getDistancesToModel"],
        "getdist_vfsqrt": [
            "public getDistancesToModel",
            "diagnostic candidate getDistancesToModel vfsqrt",
        ],
        "production": [
            "public countWithinDistance",
            "public selectWithinDistance",
            "public getDistancesToModel",
        ],
        "all": list(ITEM_METADATA),
    }
    items: list[str] = []
    for raw_part in text.split(","):
        part = raw_part.strip()
        if not part:
            continue
        selected = aliases.get(part, [part])
        for item in selected:
            if item not in ITEM_METADATA:
                raise ValueError(f"unknown item for --items: {item}")
            if item not in items:
                items.append(item)
    if not items:
        raise ValueError("--items resolved to an empty item list")
    return items


def parse_bench_log(
    path: Path,
    expected_build: str,
    selected_items: list[str],
) -> tuple[dict[str, Any], dict[str, float]]:
    if not path.exists():
        raise FileNotFoundError(path)
    context: dict[str, Any] = {}
    values: dict[str, float] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        text = line.strip()
        if match := DATASET_RE.match(text):
            context["dataset"] = match.group("dataset")
            context["size"] = int(match.group("size"))
            context["layout"] = match.group("layout")
        elif match := ITER_RE.match(text):
            context["iterations"] = int(match.group("iterations"))
        elif match := WARMUP_RE.match(text):
            context["warmup_iterations"] = int(match.group("warmup"))
        elif match := BUILD_RE.match(text):
            context["build"] = match.group("build")
        elif match := CHECKSUM_RE.match(text):
            context["checksum"] = match.group("checksum")
        elif match := ITEM_CHECKSUM_RE.match(text):
            item = match.group("item")
            if item in selected_items:
                context.setdefault("item_checksums", {})[item] = match.group("checksum")
        elif match := BENCH_ITEM_RE.match(text):
            item = match.group("item")
            if item in selected_items:
                values[item] = float(match.group("ms"))

    if context.get("build") != expected_build:
        raise ValueError(f"{path} build={context.get('build')!r}, expected {expected_build!r}")
    missing = sorted(set(selected_items) - set(values))
    if missing:
        raise ValueError(f"{path} missing bench item(s): {', '.join(missing)}")
    return context, values


def count_rvv_instructions(asm_path: Path, symbol_hint: str) -> int:
    if not asm_path.exists():
        return 0
    in_symbol = False
    count = 0
    for line in asm_path.read_text(encoding="utf-8", errors="replace").splitlines():
        header = SYMBOL_HEADER_RE.match(line)
        if header:
            in_symbol = symbol_hint in header.group("symbol")
            continue
        if in_symbol and RVV_INSTRUCTION_RE.search(line):
            count += 1
    return count


def parse_repeated_runs(
    repeat_root: Path,
    selected_items: list[str],
) -> tuple[
    dict[str, Any],
    dict[str, list[float]],
    dict[str, list[float]],
    dict[str, str],
    dict[str, str],
]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    grouped_std: dict[str, list[float]] = {item: [] for item in selected_items}
    grouped_rvv: dict[str, list[float]] = {item: [] for item in selected_items}
    std_checksums: dict[str, str] = {}
    rvv_checksums: dict[str, str] = {}

    for run_dir in run_dirs:
        std_context, std_values = parse_bench_log(run_dir / "run_bench_std.log", "Std", selected_items)
        rvv_context, rvv_values = parse_bench_log(run_dir / "run_bench_rvv.log", "RVV", selected_items)
        for key in ("dataset", "size", "iterations", "warmup_iterations"):
            if std_context.get(key) != rvv_context.get(key):
                raise ValueError(f"{run_dir} context mismatch for {key}")
        if base_context is None:
            base_context = std_context
            std_checksums = {
                item: str(std_context.get("item_checksums", {}).get(item, std_context.get("checksum", "")))
                for item in selected_items
            }
            rvv_checksums = {
                item: str(rvv_context.get("item_checksums", {}).get(item, rvv_context.get("checksum", "")))
                for item in selected_items
            }
        else:
            for key in ("dataset", "size", "iterations", "warmup_iterations"):
                if base_context.get(key) != std_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            current_std_checksums = {
                item: str(std_context.get("item_checksums", {}).get(item, std_context.get("checksum", "")))
                for item in selected_items
            }
            current_rvv_checksums = {
                item: str(rvv_context.get("item_checksums", {}).get(item, rvv_context.get("checksum", "")))
                for item in selected_items
            }
            if std_checksums != current_std_checksums:
                raise ValueError(f"{run_dir} Std checksum changed")
            if rvv_checksums != current_rvv_checksums:
                raise ValueError(f"{run_dir} RVV checksum changed")
        for item in selected_items:
            grouped_std[item].append(std_values[item])
            grouped_rvv[item].append(rvv_values[item])

    assert base_context is not None
    return base_context, grouped_std, grouped_rvv, std_checksums, rvv_checksums


def production_direct_metadata(item: str) -> dict[str, str]:
    meta = dict(ITEM_METADATA[item])
    if item == "public countWithinDistance":
        meta.update(
            evidence_role="production_direct",
            case_kind="production-public",
            wrapper="SampleConsensusModelLine::countWithinDistance public entry",
            asm_symbol="countWithinDistanceRVV",
        )
    elif item == "public selectWithinDistance":
        meta.update(
            evidence_role="production_direct",
            case_kind="production-public",
            wrapper="SampleConsensusModelLine::selectWithinDistance public entry",
            asm_symbol="selectWithinDistanceRVV",
        )
    elif item == "public getDistancesToModel":
        meta.update(
            evidence_role="production_direct",
            case_kind="production-public",
            wrapper="SampleConsensusModelLine::getDistancesToModel public entry",
            asm_symbol="getDistancesToModelRVV",
        )
    return meta


def build_comparison(
    item: str,
    meta: dict[str, str],
    std_values: list[float],
    rvv_values: list[float],
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
) -> dict[str, Any]:
    is_public_companion = meta["evidence_role"] == "summary_only_unknown"
    ba_values = [] if is_public_companion else [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    common_side = {
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": (
            "dense_distance_vector_size_only_numeric_tolerance_owned_by_gtest"
            if "getDistancesToModel" in item else "aggregate_entry_output_size_checksum"
        ),
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointXYZ_registered_single_float_xyz_aos_u32_offset",
        "mask": meta["mask"],
        "reduction": meta["reduction"],
        "diagnostic_note": (
            "getDistances checksum records dense distance vector size only; gtest owns numerical tolerance"
            if "getDistancesToModel" in item else "checksum covers current item output contract"
        ),
    }
    return {
        "name": meta["name"],
        "case_kind": meta["case_kind"],
        "evidence_role": meta["evidence_role"],
        "build_comparison": "std_vs_rvv_repeated",
        "point_type": "PointXYZ",
        "size": context["size"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "std_ms_values": std_values,
        "rvv_ms_values": rvv_values,
        "comparison_note": (
            "public companion only; production source has no line RVV dispatch"
            if is_public_companion else "diagnostic candidate Std/RVV repeated board comparison"
        ),
        "baseline": {
            "label": "Std build",
            **common_side,
            "checksum": std_checksum,
            "asm_boundary": "std_build_no_rvv",
            "rvv_instr_count": 0,
            "std_ms": round(sum(std_values) / len(std_values), 6),
        },
        "candidate": {
            "label": "RVV build",
            **common_side,
            "checksum": rvv_checksum,
            "asm_boundary": meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(asm_path, meta["asm_symbol"]),
            "rvv_ms": round(sum(rvv_values) / len(rvv_values), 6),
        },
    }


def build_manifest(
    repeat_root: Path,
    asm_path: Path,
    output_path: Path,
    summary_title: str,
    selected_items: list[str],
    production_direct: bool,
) -> dict[str, Any]:
    context, std_values, rvv_values, std_checksums, rvv_checksums = parse_repeated_runs(
        repeat_root,
        selected_items,
    )
    comparisons = [
        build_comparison(
            item,
            production_direct_metadata(item) if production_direct else ITEM_METADATA[item],
            std_values[item],
            rvv_values[item],
            context,
            std_checksums[item],
            rvv_checksums[item],
            asm_path,
        )
        for item in selected_items
    ]
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": "production_direct" if production_direct else "production_shaped_diagnostic",
            "summary_path": repo_relative(output_path),
            "label": "5-run board repeated summary",
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": len(next(iter(std_values.values()))),
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {
            "strict_ab": False,
            "thresholds": {
                "min_repeated_runs": 5,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": comparisons,
    }


def parse_identity_repeated_runs(
    repeat_root: Path,
    mode: str,
) -> tuple[
    dict[str, Any],
    dict[str, list[float]],
    dict[str, list[float]],
    dict[str, str],
    dict[str, str],
]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    expected_layout = IDENTITY_MODE_LABELS[mode]["layout"]
    base_context: dict[str, Any] | None = None
    grouped_baseline: dict[str, list[float]] = {item: [] for item in PRODUCTION_ITEMS}
    grouped_candidate: dict[str, list[float]] = {item: [] for item in PRODUCTION_ITEMS}
    baseline_checksums: dict[str, str] = {}
    candidate_checksums: dict[str, str] = {}

    for run_dir in run_dirs:
        baseline_context, baseline_values = parse_bench_log(
            run_dir / "run_bench_rvv_gather_baseline.log", "RVV", PRODUCTION_ITEMS)
        candidate_context, candidate_values = parse_bench_log(
            run_dir / "run_bench_rvv_identity.log", "RVV", PRODUCTION_ITEMS)
        for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
            if baseline_context.get(key) != candidate_context.get(key):
                raise ValueError(f"{run_dir} context mismatch for {key}")
        if baseline_context.get("layout") != expected_layout:
            raise ValueError(
                f"{run_dir} layout={baseline_context.get('layout')!r}, expected {expected_layout!r}")
        if base_context is None:
            base_context = baseline_context
            baseline_checksums = {
                item: str(baseline_context.get("item_checksums", {}).get(
                    item, baseline_context.get("checksum", "")))
                for item in PRODUCTION_ITEMS
            }
            candidate_checksums = {
                item: str(candidate_context.get("item_checksums", {}).get(
                    item, candidate_context.get("checksum", "")))
                for item in PRODUCTION_ITEMS
            }
        else:
            for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
                if base_context.get(key) != baseline_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            current_baseline_checksums = {
                item: str(baseline_context.get("item_checksums", {}).get(
                    item, baseline_context.get("checksum", "")))
                for item in PRODUCTION_ITEMS
            }
            current_candidate_checksums = {
                item: str(candidate_context.get("item_checksums", {}).get(
                    item, candidate_context.get("checksum", "")))
                for item in PRODUCTION_ITEMS
            }
            if baseline_checksums != current_baseline_checksums:
                raise ValueError(f"{run_dir} gather baseline checksum changed")
            if candidate_checksums != current_candidate_checksums:
                raise ValueError(f"{run_dir} identity candidate checksum changed")
        for item in PRODUCTION_ITEMS:
            grouped_baseline[item].append(baseline_values[item])
            grouped_candidate[item].append(candidate_values[item])

    assert base_context is not None
    return base_context, grouped_baseline, grouped_candidate, baseline_checksums, candidate_checksums


def build_identity_comparison(
    item: str,
    baseline_values: list[float],
    candidate_values: list[float],
    context: dict[str, Any],
    baseline_checksum: str,
    candidate_checksum: str,
    baseline_asm_path: Path,
    candidate_asm_path: Path,
    mode: str,
) -> dict[str, Any]:
    meta = production_direct_metadata(item)
    mode_meta = IDENTITY_MODE_LABELS[mode]
    ba_values = [
        round(baseline_ms / candidate_ms, 4)
        for baseline_ms, candidate_ms in zip(baseline_values, candidate_values)
        if candidate_ms > 0.0
    ]
    checksum_policy = (
        "dense_distance_vector_size_only_numeric_tolerance_owned_by_gtest"
        if "getDistancesToModel" in item else "aggregate_entry_output_size_checksum"
    )
    common_side = {
        "boundary": "public_overload",
        "wrapper": meta["wrapper"],
        "row_source": mode_meta["row_source"],
        "solve": False,
        "checksum_policy": checksum_policy,
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointXYZ_registered_single_float_xyz_aos_u32_offset",
        "mask": meta["mask"],
        "reduction": meta["reduction"],
    }
    return {
        "name": f"sac_model_line {item} PointXYZ 65536 {mode} gather-only RVV vs identity-strided RVV",
        "case_kind": mode_meta["case_kind"],
        "evidence_role": "strict_ab",
        "build_comparison": "rvv_vs_rvv_repeated",
        "decision_question": "RVV-family-selection",
        "point_type": "PointXYZ",
        "size": context["size"],
        "layout": context["layout"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "baseline_ms_values": baseline_values,
        "candidate_ms_values": candidate_values,
        "baseline": {
            "label": "RVV gather-only baseline",
            **common_side,
            "implementation_family": "gather_only_rvv",
            "candidate_switch": "PCL_RVV_LINE_DISABLE_IDENTITY_STRIDED",
            "checksum": baseline_checksum,
            "asm_boundary": meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(baseline_asm_path, meta["asm_symbol"]),
            "rvv_ms": round(sum(baseline_values) / len(baseline_values), 6),
        },
        "candidate": {
            "label": "RVV identity-strided candidate",
            **common_side,
            "implementation_family": "identity_strided_or_gather_rvv",
            "candidate_switch": "default_production_rvv",
            "checksum": candidate_checksum,
            "asm_boundary": meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(candidate_asm_path, meta["asm_symbol"]),
            "rvv_ms": round(sum(candidate_values) / len(candidate_values), 6),
        },
    }


def build_identity_manifest(
    repeat_root: Path,
    baseline_asm_path: Path,
    candidate_asm_path: Path,
    output_path: Path,
    summary_title: str,
    mode: str,
) -> dict[str, Any]:
    context, baseline_values, candidate_values, baseline_checksums, candidate_checksums = (
        parse_identity_repeated_runs(repeat_root, mode))
    mode_meta = IDENTITY_MODE_LABELS[mode]
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": "strict_ab",
            "summary_path": repo_relative(output_path),
            "label": mode_meta["summary_label"],
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": len(next(iter(baseline_values.values()))),
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {
            "strict_ab": True,
            "thresholds": {
                "min_repeated_runs": 5,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": [
            build_identity_comparison(
                item,
                baseline_values[item],
                candidate_values[item],
                context,
                baseline_checksums[item],
                candidate_checksums[item],
                baseline_asm_path,
                candidate_asm_path,
                mode,
            )
            for item in PRODUCTION_ITEMS
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repeat-root",
        type=Path,
        default=TOPIC_ROOT / "log/board/repeated-20260828-phase000",
    )
    parser.add_argument(
        "--asm",
        type=Path,
        default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_line_rvv.full.asm",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=TOPIC_ROOT / "doc/phases/000-line-count-diagnostic/repeated-evidence-manifest.json",
    )
    parser.add_argument(
        "--summary-title",
        default="sac_model_line Phase 000 count repeated board summary",
    )
    parser.add_argument(
        "--items",
        default="count",
        help="comma separated item labels or aliases: count, select, getdist, getDistances, get_distances, getdist_vfsqrt, production, all",
    )
    parser.add_argument(
        "--production-direct",
        action="store_true",
        help="mark public rows as production direct evidence after production RVV dispatch is integrated",
    )
    parser.add_argument(
        "--identity-mode",
        choices=("identity", "identity-shuffled"),
        help="generate strict RVV-vs-RVV manifest for identity strided-load A/B",
    )
    parser.add_argument(
        "--baseline-asm",
        type=Path,
        default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_line_rvv_gather_baseline.full.asm",
    )
    args = parser.parse_args()

    if args.identity_mode:
        manifest = build_identity_manifest(
            args.repeat_root,
            args.baseline_asm,
            args.asm,
            args.output,
            args.summary_title,
            args.identity_mode,
        )
    else:
        selected_items = parse_item_list(args.items)
        manifest = build_manifest(
            args.repeat_root,
            args.asm,
            args.output,
            args.summary_title,
            selected_items,
            args.production_direct,
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
