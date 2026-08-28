#!/usr/bin/env python3
"""Generate the sac_model_circle board Evidence Doctor manifest.

这个脚本只理解当前 topic 的 `bench_sac_model_circle` 输出格式。它把
Std/RVV board bench logs 转成 Evidence Doctor（证据体检）可读取的 JSON
manifest。public select/count 行是 production direct（真实生产路径）证据。
getDistances 当前没有 RVV production path，Phase 020 只把 RVV binary 内的
public row 和 test-only candidate row 转成 production-shaped diagnostic
（生产形态诊断）证据，不能替代 production direct。
Phase 050 使用同一 public row，对比 full-RVV test-only candidate（完整 RVV
测试候选），专门验证 `vfsqrt + vfwcvt + vse64` 能否消除 Phase 020 的
scalar tail（标量尾段）成本。
Phase 060 在 production probe（生产探针）接入后只比较 public
`getDistancesToModel` 的 Std/RVV build（标量 / RVV 构建）结果，用来回答
真实公开入口是否仍保留收益。
Phase 080 比较同一个 RVV build 内的 public selectWithinDistance baseline
和 test-only compressed error full-RVV tail candidate（压缩后误差完整 RVV
尾段候选）。它是 RVV-vs-RVV detail A/B，只回答是否值得进入后续 production
probe，不能直接写成 adopted。
Phase 040 的 identity/shuffled mode（恒等 / 乱序索引模式）比较两个 RVV
production binary：baseline 是强制禁用 identity strided load（恒等索引跨步加载）
的 gather-only RVV，candidate 是当前生产 RVV。它回答实现族选择问题，
不能用 Std/RVV positive（标量 / RVV 正向）替代。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]

PRODUCTION_ITEM_METADATA: dict[str, dict[str, str]] = {
    "public selectWithinDistance": {
        "name": "sac_model_circle public selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCircle2D::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "circle_shell_inner_outer_inclusive",
        "reduction": "ordered_inlier_and_error_distance_output",
        "baseline_implementation_family": "standard_scalar_shell_and_error_sqrt",
        "candidate_implementation_family": "rvv_gather_shell_vcompress_then_scalar_error_sqrt",
        "asm_symbol": "selectWithinDistanceRVV",
    },
    "public countWithinDistance": {
        "name": "sac_model_circle countWithinDistance PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCircle2D::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "mask": "circle_shell_inner_outer_inclusive",
        "reduction": "inlier_count",
        "asm_symbol": "countWithinDistanceRVV",
    },
}

SELECT_ERROR_TAIL_PRODUCTION_ITEM_METADATA = {
    item: dict(metadata)
    for item, metadata in PRODUCTION_ITEM_METADATA.items()
}
SELECT_ERROR_TAIL_PRODUCTION_ITEM_METADATA["public selectWithinDistance"][
    "candidate_implementation_family"
] = "rvv_gather_shell_vcompress_then_vfsqrt_vfwcvt_vse64_error_store"

GETDISTANCES_PUBLIC_ITEM = "public getDistancesToModel"
GETDISTANCES_CANDIDATE_ITEM = "diagnostic candidate getDistancesToModel"
GETDISTANCES_FULL_RVV_CANDIDATE_ITEM = "diagnostic full-rvv getDistancesToModel"
SELECT_PUBLIC_ITEM = "public selectWithinDistance"
SELECT_ERROR_TAIL_ITEM = "diagnostic select full-rvv error tail"
GETDISTANCES_ITEM_METADATA: dict[str, dict[str, str]] = {
    GETDISTANCES_PUBLIC_ITEM: {
        "name": "sac_model_circle public getDistancesToModel PointXYZ 65536",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCircle2D::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "none_all_indices_written",
        "reduction": "scalar_sqrt_dense_double_store",
        "asm_symbol": "getDistancesToModel",
    },
    GETDISTANCES_CANDIDATE_ITEM: {
        "name": "sac_model_circle test-only getDistancesToModel candidate PointXYZ 65536",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelCircleBench::getDistancesToModelCandidate",
        "timer_boundary": "getDistancesToModel_candidate_entry_only",
        "mask": "none_all_indices_written",
        "reduction": "rvv_sqr_distance_then_scalar_sqrt_dense_double_store",
        "asm_symbol": "getDistancesToModelScalarSqrtCandidateRVV",
    },
    GETDISTANCES_FULL_RVV_CANDIDATE_ITEM: {
        "name": "sac_model_circle test-only getDistancesToModel full-RVV candidate PointXYZ 65536",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelCircleBench::getDistancesToModelFullRVVCandidate",
        "timer_boundary": "getDistancesToModel_full_rvv_candidate_entry_only",
        "mask": "none_all_indices_written",
        "reduction": "rvv_sqr_distance_vfsqrt_vfabs_vfwcvt_dense_double_store",
        "asm_symbol": "getDistancesToModelFullRVV",
    },
}

SELECT_ERROR_TAIL_METADATA: dict[str, dict[str, str]] = {
    SELECT_PUBLIC_ITEM: {
        "name": "sac_model_circle public selectWithinDistance adopted RVV PointXYZ 65536",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCircle2D::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "circle_shell_inner_outer_inclusive",
        "reduction": "vcompress_ordered_inlier_write_then_scalar_exact_error_sqrt",
        "asm_symbol": "selectWithinDistanceRVV",
    },
    SELECT_ERROR_TAIL_ITEM: {
        "name": "sac_model_circle test-only selectWithinDistance full-RVV error tail PointXYZ 65536",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelCircleBench::selectWithinDistanceFullRVVErrorTailCandidate",
        "timer_boundary": "selectWithinDistance_full_rvv_error_tail_candidate_entry_only",
        "mask": "circle_shell_inner_outer_inclusive",
        "reduction": "vcompress_ordered_inlier_write_then_vfsqrt_vfwcvt_dense_active_error_store",
        "asm_symbol": "selectWithinDistanceFullRVVErrorTail",
    },
}

GETDISTANCES_PRODUCTION_METADATA: dict[str, dict[str, str]] = {
    GETDISTANCES_PUBLIC_ITEM: {
        "name": "sac_model_circle production getDistancesToModel PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCircle2D::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "none_all_indices_written",
        "baseline_reduction": "scalar_sqrt_dense_double_store",
        "candidate_reduction": "rvv_sqr_distance_vfsqrt_vfabs_vfwcvt_dense_double_store",
        "asm_symbol": "getDistancesToModelRVV",
    },
}

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


def parse_bench_log(
    path: Path,
    expected_build: str,
    item_metadata: dict[str, dict[str, str]],
) -> tuple[dict[str, Any], dict[str, float]]:
    if not path.exists():
        raise FileNotFoundError(path)
    context: dict[str, Any] = {}
    values: dict[str, float] = {}
    item_checksums: dict[str, str] = {}
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
            if item in item_metadata:
                item_checksums[item] = match.group("checksum")
        elif match := BENCH_ITEM_RE.match(text):
            item = match.group("item")
            if item in item_metadata:
                values[item] = float(match.group("ms"))

    if context.get("build") != expected_build:
        raise ValueError(f"{path} build={context.get('build')!r}, expected {expected_build!r}")
    missing = sorted(set(item_metadata) - set(values))
    if missing:
        raise ValueError(f"{path} missing bench item(s): {', '.join(missing)}")
    context["item_checksums"] = item_checksums
    return context, values


def parse_bench_log_for_items(
    path: Path,
    expected_build: str,
    item_names: tuple[str, ...],
) -> tuple[dict[str, Any], dict[str, float]]:
    if not path.exists():
        raise FileNotFoundError(path)
    context: dict[str, Any] = {}
    values: dict[str, float] = {}
    item_checksums: dict[str, str] = {}
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
            if item in item_names:
                item_checksums[item] = match.group("checksum")
        elif match := BENCH_ITEM_RE.match(text):
            item = match.group("item")
            if item in item_names:
                values[item] = float(match.group("ms"))

    if context.get("build") != expected_build:
        raise ValueError(f"{path} build={context.get('build')!r}, expected {expected_build!r}")
    missing = sorted(set(item_names) - set(values))
    if missing:
        raise ValueError(f"{path} missing bench item(s): {', '.join(missing)}")
    context["item_checksums"] = item_checksums
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
    item_metadata: dict[str, dict[str, str]] = PRODUCTION_ITEM_METADATA,
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
    grouped_std: dict[str, list[float]] = {item: [] for item in item_metadata}
    grouped_rvv: dict[str, list[float]] = {item: [] for item in item_metadata}
    std_checksums: dict[str, str] = {}
    rvv_checksums: dict[str, str] = {}

    for run_dir in run_dirs:
        std_context, std_values = parse_bench_log(
            run_dir / "run_bench_std.log", "Std", item_metadata)
        rvv_context, rvv_values = parse_bench_log(
            run_dir / "run_bench_rvv.log", "RVV", item_metadata)
        for key in ("dataset", "size", "iterations", "warmup_iterations"):
            if std_context.get(key) != rvv_context.get(key):
                raise ValueError(f"{run_dir} context mismatch for {key}")
        if base_context is None:
            base_context = std_context
            std_checksums = {
                item: str(std_context.get("item_checksums", {}).get(item, ""))
                for item in item_metadata
            }
            rvv_checksums = {
                item: str(rvv_context.get("item_checksums", {}).get(item, ""))
                for item in item_metadata
            }
        else:
            for key in ("dataset", "size", "iterations", "warmup_iterations"):
                if base_context.get(key) != std_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            for item in item_metadata:
                std_item_checksum = str(std_context.get("item_checksums", {}).get(item, ""))
                rvv_item_checksum = str(rvv_context.get("item_checksums", {}).get(item, ""))
                if std_checksums.get(item) != std_item_checksum:
                    raise ValueError(f"{run_dir} Std item checksum changed for {item}")
                if rvv_checksums.get(item) != rvv_item_checksum:
                    raise ValueError(f"{run_dir} RVV item checksum changed for {item}")
        for item in item_metadata:
            grouped_std[item].append(std_values[item])
            grouped_rvv[item].append(rvv_values[item])

    assert base_context is not None
    missing_checksums = [
        item
        for item in item_metadata
        if not std_checksums.get(item) or not rvv_checksums.get(item)
    ]
    if missing_checksums:
        raise ValueError(
            "missing item checksum(s): " + ", ".join(sorted(missing_checksums))
        )
    return base_context, grouped_std, grouped_rvv, std_checksums, rvv_checksums


def build_comparison(
    item: str,
    std_values: list[float],
    rvv_values: list[float],
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
    item_metadata: dict[str, dict[str, str]] = PRODUCTION_ITEM_METADATA,
) -> dict[str, Any]:
    meta = item_metadata[item]
    ba_values = [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    common_side = {
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "aggregate_entry_output_hash",
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointXYZ_registered_single_float_xy_aos_u32_offset",
        "mask": meta["mask"],
        "reduction": meta["reduction"],
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
        "baseline": {
            "label": "Std build",
            **common_side,
            "implementation_family": meta.get("baseline_implementation_family", "standard_scalar"),
            "checksum": std_checksum,
            "asm_boundary": "std_build_no_rvv",
            "rvv_instr_count": 0,
            "std_ms": round(sum(std_values) / len(std_values), 6),
        },
        "candidate": {
            "label": "RVV build",
            **common_side,
            "implementation_family": meta.get("candidate_implementation_family", "production_rvv"),
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
    item_metadata: dict[str, dict[str, str]] = PRODUCTION_ITEM_METADATA,
) -> dict[str, Any]:
    context, std_values, rvv_values, std_checksums, rvv_checksums = parse_repeated_runs(
        repeat_root, item_metadata)
    comparisons = [
        build_comparison(
            item,
            std_values[item],
            rvv_values[item],
            context,
            std_checksums[item],
            rvv_checksums[item],
            asm_path,
            item_metadata,
        )
        for item in item_metadata
    ]
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": "production_direct",
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


def parse_getdistances_repeated_runs(
    repeat_root: Path,
    candidate_item: str,
) -> tuple[dict[str, Any], list[float], list[float], str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    public_values: list[float] = []
    candidate_values: list[float] = []
    rvv_checksum = ""

    for run_dir in run_dirs:
        rvv_context, rvv_values = parse_bench_log_for_items(
            run_dir / "run_bench_rvv.log", "RVV", (GETDISTANCES_PUBLIC_ITEM, candidate_item))
        if base_context is None:
            base_context = rvv_context
            rvv_checksum = str(rvv_context.get("checksum", ""))
        else:
            for key in ("dataset", "size", "iterations", "warmup_iterations"):
                if base_context.get(key) != rvv_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            if rvv_checksum != str(rvv_context.get("checksum", "")):
                raise ValueError(f"{run_dir} RVV checksum changed")

        public_values.append(rvv_values[GETDISTANCES_PUBLIC_ITEM])
        candidate_values.append(rvv_values[candidate_item])

    assert base_context is not None
    return base_context, public_values, candidate_values, rvv_checksum


def build_getdistances_comparison(
    public_values: list[float],
    candidate_values: list[float],
    context: dict[str, Any],
    rvv_checksum: str,
    asm_path: Path,
    candidate_item: str,
) -> dict[str, Any]:
    public_meta = GETDISTANCES_ITEM_METADATA[GETDISTANCES_PUBLIC_ITEM]
    candidate_meta = GETDISTANCES_ITEM_METADATA[candidate_item]
    ba_values = [
        round(public_ms / candidate_ms, 4)
        for public_ms, candidate_ms in zip(public_values, candidate_values)
        if candidate_ms > 0.0
    ]
    common_side = {
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "aggregate_bench_checksum_plus_getdistances_gtest",
        "gate": "PointXYZ_registered_single_float_xy_aos_u32_offset",
    }
    return {
        "name": (
            "sac_model_circle getDistancesToModel public vs test-only full-RVV candidate "
            "PointXYZ 65536"
            if candidate_item == GETDISTANCES_FULL_RVV_CANDIDATE_ITEM
            else "sac_model_circle getDistancesToModel public vs test-only RVV candidate PointXYZ 65536"
        ),
        "case_kind": "production_shaped",
        "evidence_role": "production_shaped_diagnostic",
        "build_comparison": "rvv_binary_public_vs_test_helper_repeated",
        "allowed_contract_mismatches": ["boundary", "wrapper", "timer_boundary", "reduction"],
        "point_type": "PointXYZ",
        "size": context["size"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "public_ms_values": public_values,
        "candidate_ms_values": candidate_values,
        "diagnostic_to_production_mismatch_audit": {
            "evidence_role": "production_shaped_diagnostic",
            "ab_boundary": "public overload companion vs test-only helper in one RVV binary",
            "decision_question": (
                "whether this full-RVV helper shape is worth a later bounded production probe"
                if candidate_item == GETDISTANCES_FULL_RVV_CANDIDATE_ITEM
                else "whether this helper shape is worth a later bounded production probe"
            ),
            "can_extrapolate_to_production": "no_direct_extrapolation",
            "baseline_mismatch_risk": "yes_public_wrapper_vs_test_helper",
            "clean_adoption_requires": "production direct public Std/RVV after explicit user approval",
        },
        "baseline": {
            "label": "RVV build public getDistancesToModel",
            **common_side,
            "boundary": public_meta["boundary"],
            "wrapper": public_meta["wrapper"],
            "timer_boundary": public_meta["timer_boundary"],
            "mask": public_meta["mask"],
            "reduction": public_meta["reduction"],
            "checksum": rvv_checksum,
            "asm_boundary": public_meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(asm_path, public_meta["asm_symbol"]),
            "rvv_ms": round(sum(public_values) / len(public_values), 6),
        },
        "candidate": {
            "label": "RVV build test-only getDistancesToModel candidate",
            **common_side,
            "boundary": candidate_meta["boundary"],
            "wrapper": candidate_meta["wrapper"],
            "timer_boundary": candidate_meta["timer_boundary"],
            "mask": candidate_meta["mask"],
            "reduction": candidate_meta["reduction"],
            "checksum": rvv_checksum,
            "asm_boundary": candidate_meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(asm_path, candidate_meta["asm_symbol"]),
            "rvv_ms": round(sum(candidate_values) / len(candidate_values), 6),
        },
    }


def build_getdistances_manifest(
    repeat_root: Path,
    asm_path: Path,
    output_path: Path,
    summary_title: str,
    candidate_item: str = GETDISTANCES_CANDIDATE_ITEM,
) -> dict[str, Any]:
    context, public_values, candidate_values, rvv_checksum = parse_getdistances_repeated_runs(
        repeat_root, candidate_item)
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": "production_shaped_diagnostic",
            "summary_path": repo_relative(output_path),
            "label": "5-run board repeated getDistances diagnostic summary",
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": len(public_values),
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
        "comparisons": [
            build_getdistances_comparison(
                public_values,
                candidate_values,
                context,
                rvv_checksum,
                asm_path,
                candidate_item,
            )
        ],
    }


def parse_select_error_tail_repeated_runs(
    repeat_root: Path,
) -> tuple[dict[str, Any], list[float], list[float], str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    public_values: list[float] = []
    candidate_values: list[float] = []
    rvv_checksum = ""

    for run_dir in run_dirs:
        rvv_context, rvv_values = parse_bench_log_for_items(
            run_dir / "run_bench_rvv.log", "RVV", (SELECT_PUBLIC_ITEM, SELECT_ERROR_TAIL_ITEM))
        if base_context is None:
            base_context = rvv_context
            rvv_checksum = str(rvv_context.get("checksum", ""))
        else:
            for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
                if base_context.get(key) != rvv_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            if rvv_checksum != str(rvv_context.get("checksum", "")):
                raise ValueError(f"{run_dir} RVV checksum changed")

        public_values.append(rvv_values[SELECT_PUBLIC_ITEM])
        candidate_values.append(rvv_values[SELECT_ERROR_TAIL_ITEM])

    assert base_context is not None
    return base_context, public_values, candidate_values, rvv_checksum


def build_select_error_tail_comparison(
    public_values: list[float],
    candidate_values: list[float],
    context: dict[str, Any],
    rvv_checksum: str,
    asm_path: Path,
) -> dict[str, Any]:
    public_meta = SELECT_ERROR_TAIL_METADATA[SELECT_PUBLIC_ITEM]
    candidate_meta = SELECT_ERROR_TAIL_METADATA[SELECT_ERROR_TAIL_ITEM]
    ba_values = [
        round(public_ms / candidate_ms, 4)
        for public_ms, candidate_ms in zip(public_values, candidate_values)
        if candidate_ms > 0.0
    ]
    common_side = {
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "aggregate_bench_checksum_plus_select_gtest",
        "gate": "PointXYZ_registered_single_float_xy_aos_u32_offset",
        "mask": "circle_shell_inner_outer_inclusive",
    }
    return {
        "name": (
            "sac_model_circle selectWithinDistance adopted RVV vs test-only "
            "full-RVV error tail PointXYZ 65536"
        ),
        "case_kind": "production-detail",
        "evidence_role": "strict_ab",
        "build_comparison": "rvv_vs_rvv_repeated",
        "decision_question": "RVV-family-selection",
        "allowed_contract_mismatches": ["boundary", "wrapper", "timer_boundary", "reduction"],
        "point_type": "PointXYZ",
        "size": context["size"],
        "layout": context["layout"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "baseline_ms_values": public_values,
        "candidate_ms_values": candidate_values,
        "baseline": {
            "label": "RVV build public selectWithinDistance adopted baseline",
            **common_side,
            "boundary": public_meta["boundary"],
            "wrapper": public_meta["wrapper"],
            "timer_boundary": public_meta["timer_boundary"],
            "reduction": public_meta["reduction"],
            "checksum": rvv_checksum,
            "asm_boundary": public_meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(asm_path, public_meta["asm_symbol"]),
            "rvv_ms": round(sum(public_values) / len(public_values), 6),
        },
        "candidate": {
            "label": "RVV build test-only select full-RVV error tail candidate",
            **common_side,
            "boundary": candidate_meta["boundary"],
            "wrapper": candidate_meta["wrapper"],
            "timer_boundary": candidate_meta["timer_boundary"],
            "reduction": candidate_meta["reduction"],
            "checksum": rvv_checksum,
            "asm_boundary": candidate_meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(asm_path, candidate_meta["asm_symbol"]),
            "rvv_ms": round(sum(candidate_values) / len(candidate_values), 6),
        },
    }


def build_select_error_tail_manifest(
    repeat_root: Path,
    asm_path: Path,
    output_path: Path,
    summary_title: str,
) -> dict[str, Any]:
    context, public_values, candidate_values, rvv_checksum = (
        parse_select_error_tail_repeated_runs(repeat_root))
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": "strict_ab",
            "summary_path": repo_relative(output_path),
            "label": "5-run board repeated select full-RVV error tail RVV-vs-RVV summary",
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": len(public_values),
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
            build_select_error_tail_comparison(
                public_values,
                candidate_values,
                context,
                rvv_checksum,
                asm_path,
            )
        ],
    }


def parse_getdistances_production_repeated_runs(
    repeat_root: Path,
) -> tuple[dict[str, Any], list[float], list[float], str, str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    std_values: list[float] = []
    rvv_values: list[float] = []
    std_checksum = ""
    rvv_checksum = ""

    for run_dir in run_dirs:
        std_context, std_run_values = parse_bench_log_for_items(
            run_dir / "run_bench_std.log", "Std", (GETDISTANCES_PUBLIC_ITEM,))
        rvv_context, rvv_run_values = parse_bench_log_for_items(
            run_dir / "run_bench_rvv.log", "RVV", (GETDISTANCES_PUBLIC_ITEM,))
        for key in ("dataset", "size", "iterations", "warmup_iterations"):
            if std_context.get(key) != rvv_context.get(key):
                raise ValueError(f"{run_dir} context mismatch for {key}")
        if base_context is None:
            base_context = std_context
            std_checksum = str(
                std_context.get("item_checksums", {}).get(
                    GETDISTANCES_PUBLIC_ITEM, std_context.get("checksum", "")))
            rvv_checksum = str(
                rvv_context.get("item_checksums", {}).get(
                    GETDISTANCES_PUBLIC_ITEM, rvv_context.get("checksum", "")))
        else:
            for key in ("dataset", "size", "iterations", "warmup_iterations"):
                if base_context.get(key) != std_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            std_run_checksum = str(
                std_context.get("item_checksums", {}).get(
                    GETDISTANCES_PUBLIC_ITEM, std_context.get("checksum", "")))
            rvv_run_checksum = str(
                rvv_context.get("item_checksums", {}).get(
                    GETDISTANCES_PUBLIC_ITEM, rvv_context.get("checksum", "")))
            if std_checksum != std_run_checksum:
                raise ValueError(f"{run_dir} Std checksum changed")
            if rvv_checksum != rvv_run_checksum:
                raise ValueError(f"{run_dir} RVV checksum changed")

        std_values.append(std_run_values[GETDISTANCES_PUBLIC_ITEM])
        rvv_values.append(rvv_run_values[GETDISTANCES_PUBLIC_ITEM])

    assert base_context is not None
    return base_context, std_values, rvv_values, std_checksum, rvv_checksum


def build_getdistances_production_comparison(
    std_values: list[float],
    rvv_values: list[float],
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
) -> dict[str, Any]:
    meta = GETDISTANCES_PRODUCTION_METADATA[GETDISTANCES_PUBLIC_ITEM]
    ba_values = [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    common_side = {
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "tolerance_gtest_public_standard_direct_rvv",
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointXYZ_registered_single_float_xy_aos_u32_offset",
        "mask": meta["mask"],
        "reduction": "dense_distance_vector_output",
    }
    return {
        "name": meta["name"],
        "case_kind": meta["case_kind"],
        "evidence_role": meta["evidence_role"],
        "build_comparison": "std_vs_rvv_repeated",
        "decision_question": "production-adoption-after-probe",
        "point_type": "PointXYZ",
        "size": context["size"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "std_ms_values": std_values,
        "rvv_ms_values": rvv_values,
        "correctness_gate": {
            "qemu_target": "make -C test-rvv/sample_consensus/sac_model_circle run_test_compare",
            "board_target": (
                "make -C test-rvv/sample_consensus/sac_model_circle "
                "run_board_circle_getdistances_production_test"
            ),
            "gtest_filter": (
                "SampleConsensusModelCircle2D.PublicGetDistancesMatchesStandardAndDirectRVV"
            ),
            "tolerance": "1e-6 absolute distance tolerance",
            "bitwise_checksum_required": False,
        },
        "raw_output_checksums": {
            "policy": (
                "The raw getDistances output hashes are kept for audit only. "
                "Std uses scalar sqrt/double conversion while RVV uses f32 vfsqrt "
                "followed by vfwcvt/vse64, so bitwise equality is not the correctness gate."
            ),
            "std_public_getDistancesToModel": std_checksum,
            "rvv_public_getDistancesToModel": rvv_checksum,
        },
        "baseline": {
            "label": "Std build public getDistancesToModel",
            **common_side,
            "implementation_family": meta["baseline_reduction"],
            "asm_boundary": "std_build_no_rvv",
            "rvv_instr_count": 0,
            "std_ms": round(sum(std_values) / len(std_values), 6),
        },
        "candidate": {
            "label": "RVV build public getDistancesToModel",
            **common_side,
            "implementation_family": meta["candidate_reduction"],
            "asm_boundary": meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(asm_path, meta["asm_symbol"]),
            "rvv_ms": round(sum(rvv_values) / len(rvv_values), 6),
        },
    }


def build_getdistances_production_manifest(
    repeat_root: Path,
    asm_path: Path,
    output_path: Path,
    summary_title: str,
) -> dict[str, Any]:
    context, std_values, rvv_values, std_checksum, rvv_checksum = (
        parse_getdistances_production_repeated_runs(repeat_root))
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": "production_direct",
            "summary_path": repo_relative(output_path),
            "label": "5-run board repeated getDistances production summary",
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": len(std_values),
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
        "comparisons": [
            build_getdistances_production_comparison(
                std_values,
                rvv_values,
                context,
                std_checksum,
                rvv_checksum,
                asm_path,
            )
        ],
    }


def parse_identity_repeated_runs(
    repeat_root: Path,
    mode: str,
) -> tuple[dict[str, Any], dict[str, list[float]], dict[str, list[float]], str, str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    expected_layout = IDENTITY_MODE_LABELS[mode]["layout"]
    base_context: dict[str, Any] | None = None
    grouped_baseline: dict[str, list[float]] = {item: [] for item in PRODUCTION_ITEM_METADATA}
    grouped_candidate: dict[str, list[float]] = {item: [] for item in PRODUCTION_ITEM_METADATA}
    baseline_checksum = ""
    candidate_checksum = ""

    for run_dir in run_dirs:
        baseline_context, baseline_values = parse_bench_log(
            run_dir / "run_bench_rvv_gather_baseline.log", "RVV", PRODUCTION_ITEM_METADATA)
        candidate_context, candidate_values = parse_bench_log(
            run_dir / "run_bench_rvv_identity.log", "RVV", PRODUCTION_ITEM_METADATA)
        for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
            if baseline_context.get(key) != candidate_context.get(key):
                raise ValueError(f"{run_dir} context mismatch for {key}")
        if baseline_context.get("layout") != expected_layout:
            raise ValueError(
                f"{run_dir} layout={baseline_context.get('layout')!r}, expected {expected_layout!r}")
        if base_context is None:
            base_context = baseline_context
            baseline_checksum = str(baseline_context.get("checksum", ""))
            candidate_checksum = str(candidate_context.get("checksum", ""))
        else:
            for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
                if base_context.get(key) != baseline_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            if baseline_checksum != str(baseline_context.get("checksum", "")):
                raise ValueError(f"{run_dir} gather baseline checksum changed")
            if candidate_checksum != str(candidate_context.get("checksum", "")):
                raise ValueError(f"{run_dir} identity candidate checksum changed")
        for item in PRODUCTION_ITEM_METADATA:
            grouped_baseline[item].append(baseline_values[item])
            grouped_candidate[item].append(candidate_values[item])

    assert base_context is not None
    return base_context, grouped_baseline, grouped_candidate, baseline_checksum, candidate_checksum


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
    meta = PRODUCTION_ITEM_METADATA[item]
    mode_meta = IDENTITY_MODE_LABELS[mode]
    ba_values = [
        round(baseline_ms / candidate_ms, 4)
        for baseline_ms, candidate_ms in zip(baseline_values, candidate_values)
        if candidate_ms > 0.0
    ]
    common_side = {
        "boundary": "public_overload",
        "wrapper": meta["wrapper"],
        "row_source": mode_meta["row_source"],
        "solve": False,
        "checksum_policy": "aggregate_entry_output_hash",
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointXYZ_registered_single_float_xy_aos_u32_offset",
        "mask": meta["mask"],
        "reduction": meta["reduction"],
    }
    return {
        "name": f"sac_model_circle {item} PointXYZ 65536 {mode} gather-only RVV vs identity-strided RVV",
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
            "candidate_switch": "PCL_RVV_CIRCLE_DISABLE_IDENTITY_STRIDED",
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
    context, baseline_values, candidate_values, baseline_checksum, candidate_checksum = (
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
                baseline_checksum,
                candidate_checksum,
                baseline_asm_path,
                candidate_asm_path,
                mode,
            )
            for item in PRODUCTION_ITEM_METADATA
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode",
        choices=(
            "production",
            "getdistances",
            "getdistances-full-rvv",
            "getdistances-production",
            "select-error-tail",
            "select-error-tail-production",
            "identity",
            "identity-shuffled",
        ),
        default="production",
    )
    parser.add_argument(
        "--repeat-root",
        type=Path,
        default=TOPIC_ROOT / "log/board/repeated-20260828-phase000-production-select",
    )
    parser.add_argument(
        "--asm",
        type=Path,
        default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_circle_rvv.full.asm",
    )
    parser.add_argument(
        "--baseline-asm",
        type=Path,
        default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_circle_rvv_gather_baseline.full.asm",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=TOPIC_ROOT / "doc/phases/000-circle-select-distance-production/production-repeated-evidence-manifest.json",
    )
    parser.add_argument(
        "--summary-title",
        default="sac_model_circle Phase 000 production select/count repeated board summary",
    )
    args = parser.parse_args()

    if args.mode == "production":
        manifest = build_manifest(args.repeat_root, args.asm, args.output, args.summary_title)
    elif args.mode == "getdistances":
        manifest = build_getdistances_manifest(
            args.repeat_root, args.asm, args.output, args.summary_title)
    elif args.mode == "getdistances-full-rvv":
        manifest = build_getdistances_manifest(
            args.repeat_root,
            args.asm,
            args.output,
            args.summary_title,
            candidate_item=GETDISTANCES_FULL_RVV_CANDIDATE_ITEM,
        )
    elif args.mode == "getdistances-production":
        manifest = build_getdistances_production_manifest(
            args.repeat_root, args.asm, args.output, args.summary_title)
    elif args.mode == "select-error-tail":
        manifest = build_select_error_tail_manifest(
            args.repeat_root, args.asm, args.output, args.summary_title)
    elif args.mode == "select-error-tail-production":
        manifest = build_manifest(
            args.repeat_root,
            args.asm,
            args.output,
            args.summary_title,
            item_metadata=SELECT_ERROR_TAIL_PRODUCTION_ITEM_METADATA,
        )
    else:
        manifest = build_identity_manifest(
            args.repeat_root,
            args.baseline_asm,
            args.asm,
            args.output,
            args.summary_title,
            args.mode,
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
