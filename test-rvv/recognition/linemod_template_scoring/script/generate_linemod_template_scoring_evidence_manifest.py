#!/usr/bin/env python3
"""Generate Evidence Doctor input for LINEMOD template scoring.

本脚本只解析当前 topic 的 bench 输出格式。它把 5-run board 目录中的
Std/RVV 日志转成通用 evidence_manifest.json，并额外生成一个 phase-local
summary，供 result / evaluation / Handoff 引用。默认证据边界是
production-shaped diagnostic（生产形态诊断）；Phase 070 的 production-direct
target 会显式传入 production-public（真实公开入口）角色。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = next(path for path in (TOPIC_ROOT, *TOPIC_ROOT.parents) if (path / ".git").exists())

DATASET_RE = re.compile(
    r"^Dataset:\s+(?P<dataset>.+?)\s+\(mem_size=(?P<mem_size>\d+),\s*nr_maps=(?P<nr_maps>\d+)\)"
)
ITERATIONS_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s+(?P<warmup>\d+)")
TIMING_RE = re.compile(
    r"^(?P<label>LINEMOD (?:energy map generation|linearized map copy|score accumulation|score scan|accumulate\+scan|full-chain energy map generation|full-chain linearized map copy|full-chain score accumulation|full-chain score scan|full-chain total|production matchTemplates total|production detectTemplates total|production semi-scale detectTemplates total)):\s+(?P<us>[0-9.]+)\s+us\s+/\s+iter"
)
CHECKSUM_RE = re.compile(
    r"^checksum=(?P<checksum>\S+)\s+mode=(?P<mode>\S+)\s+max_value=(?P<max_value>\d+)\s+max_index=(?P<max_index>\d+)"
    r"(?:\s+count_above_threshold=(?P<count_above_threshold>\d+))?"
    r"(?:\s+index_checksum=(?P<index_checksum>\S+))?"
    r"(?:\s+energy_checksum=(?P<energy_checksum>\S+))?"
    r"(?:\s+linearized_checksum=(?P<linearized_checksum>\S+))?"
)
RVV_INSTRUCTION_RE = re.compile(r"\s+v[a-z0-9]+(?:\.[a-z0-9]+)*\s+")
REQUIRED_MNEMONIC_RE = re.compile(
    r"\b(vle8\.v|vlse8\.v|vle16\.v|vzext\.vf2|vadd\.vv|vand\.v[ivx]|vmsne\.vx|vmerge\.v[ivx]m|vmsgtu\.vx|vcpop\.m|vse8\.v|vse16\.v)\b"
)

BENCH_CASES = {
    "LINEMOD energy map generation": {
        "group": "linemod-energy-map-generation",
        "checksum_policy": "energy_maps_fnv1a",
        "timer_boundary": "quantized_byte_stream_to_default_merged_energy_maps",
        "gate": "detectTemplates_default_energy_mask_semantics_tail_included",
        "mask": "u8_quantized_bit_hit_against_val0_to_val3",
        "reduction": "four_mask_hit_count_to_u8_energy_value",
        "point_type": "not_applicable_quantized_byte_map",
        "row_source": "quantized_map_byte_stream",
        "boundary_note": "只覆盖 synthetic QuantizedMap 字节流到默认合并 energy maps；不覆盖 separate-energy 编译分支、LinearizedMaps 拷贝或真实 production dispatch。",
    },
    "LINEMOD linearized map copy": {
        "group": "linemod-linearized-map-copy",
        "checksum_policy": "linearized_maps_fnv1a",
        "timer_boundary": "single_energy_map_to_64_linearized_offset_maps",
        "gate": "detectTemplates_step8_offset_copy_semantics_tail_included",
        "mask": "not_applicable_copy_only",
        "reduction": "not_applicable_strided_copy",
        "point_type": "not_applicable_energy_map_byte",
        "row_source": "energy_map_byte_stream",
        "boundary_note": "只覆盖 synthetic single-bin energy map 到 64 个 step=8 linearized offset maps；不覆盖 EnergyMaps / LinearizedMaps 对象分配、getOffsetMap 后续读取、score accumulation、NMS 或真实 production dispatch。",
    },
    "LINEMOD score accumulation": {
        "group": "linemod-score-accumulation",
        "checksum_policy": "score_sums_fnv1a",
        "timer_boundary": "score_accumulation_helper_only_no_energy_or_detection",
        "gate": "already_linearized_maps_mem_size_tail_included",
        "mask": "not_applicable_no_finite_mask",
        "reduction": "u8_maps_widened_to_u16_score_sums",
        "point_type": "not_applicable_byte_score_map",
        "row_source": "linearized_map_byte_stream",
        "boundary_note": "只覆盖 u8 linearized score maps 到 u16 score_sums 的累加核。",
    },
    "LINEMOD score scan": {
        "group": "linemod-score-scan",
        "checksum_policy": "score_scan_summary_and_order_fnv1a",
        "timer_boundary": "score_scan_threshold_count_max_and_candidate_index_sink",
        "gate": "strict_greater_threshold_and_earliest_max_tie_break",
        "mask": "u16_score_greater_than_raw_threshold",
        "reduction": "threshold_count_vector_mask_plus_scalar_ordered_index_append",
        "point_type": "not_applicable_score_sum_stream",
        "row_source": "linearized_score_sum_stream",
        "boundary_note": "覆盖 threshold count、max 最早位置和候选 index 保序写入，不覆盖 NMS 或 detection 对象构造。",
    },
    "LINEMOD accumulate+scan": {
        "group": "linemod-accumulate-and-scan",
        "checksum_policy": "score_sums_and_scan_summary_fnv1a",
        "timer_boundary": "score_accumulation_then_score_scan_no_energy_or_nms",
        "gate": "already_linearized_maps_then_strict_threshold_scan",
        "mask": "scan_threshold_mask_after_accumulation",
        "reduction": "u8_to_u16_accumulation_followed_by_threshold_count_and_ordered_index_append",
        "point_type": "not_applicable_byte_score_map_to_score_sum_stream",
        "row_source": "linearized_map_byte_stream",
        "boundary_note": "覆盖累加后立刻扫描的组合成本，不覆盖 energy map、linearized copy、NMS 或真实 production dispatch。",
    },
    "LINEMOD full-chain energy map generation": {
        "group": "linemod-full-chain-energy-map-generation",
        "checksum_policy": "full_chain_energy_maps_fnv1a",
        "timer_boundary": "full_chain_split_energy_generation_scalar_both_builds",
        "gate": "synthetic_quantized_map_to_8_bin_energy_maps",
        "mask": "u8_quantized_bit_hit_against_val0_to_val3",
        "reduction": "four_mask_hit_count_to_u8_energy_value",
        "point_type": "not_applicable_quantized_byte_map",
        "row_source": "quantized_map_byte_stream",
        "boundary_note": "Phase 050 分段计时中的 energy map generation；Std/RVV 两侧都强制走标量 helper，只用于解释 full-chain 成本占比，不作为 RVV energy candidate 采纳证据。",
    },
    "LINEMOD full-chain linearized map copy": {
        "group": "linemod-full-chain-linearized-map-copy",
        "checksum_policy": "full_chain_linearized_maps_fnv1a",
        "timer_boundary": "full_chain_split_8_bin_energy_maps_to_linearized_maps",
        "gate": "8_bin_step8_offset_copy_semantics_tail_included",
        "mask": "not_applicable_copy_only",
        "reduction": "not_applicable_strided_copy",
        "point_type": "not_applicable_energy_map_byte",
        "row_source": "energy_map_byte_stream",
        "boundary_note": "Phase 050 的核心候选：8 个 bin-major energy maps 到 8*64 个 linearized maps；只替换拷贝核，不覆盖 LinearizedMaps 对象分配、getOffsetMap、NMS 或真实 production dispatch。",
    },
    "LINEMOD full-chain score accumulation": {
        "group": "linemod-full-chain-score-accumulation",
        "checksum_policy": "full_chain_score_sums_fnv1a",
        "timer_boundary": "full_chain_split_score_accumulation_scalar_both_builds",
        "gate": "full_chain_linearized_maps_tail_included",
        "mask": "not_applicable_no_finite_mask",
        "reduction": "u8_maps_to_u16_score_sums_scalar_both_builds",
        "point_type": "not_applicable_byte_score_map",
        "row_source": "linearized_map_byte_stream",
        "boundary_note": "Phase 050 分段计时中的 score accumulation；Std/RVV 两侧都强制走标量 helper，用于解释成本占比，不作为 Phase 000 RVV accumulation candidate 的当前证据。",
    },
    "LINEMOD full-chain score scan": {
        "group": "linemod-full-chain-score-scan",
        "checksum_policy": "full_chain_score_scan_summary_and_order_fnv1a",
        "timer_boundary": "full_chain_split_score_scan_scalar_both_builds",
        "gate": "strict_greater_threshold_and_earliest_max_tie_break",
        "mask": "u16_score_greater_than_raw_threshold",
        "reduction": "scalar_threshold_count_max_and_ordered_index_append",
        "point_type": "not_applicable_score_sum_stream",
        "row_source": "linearized_score_sum_stream",
        "boundary_note": "Phase 050 分段计时中的 score scan；Std/RVV 两侧都强制走标量 helper，用于解释成本占比，不作为 Phase 010 RVV scan candidate 的当前证据。",
    },
    "LINEMOD full-chain total": {
        "group": "linemod-full-chain-total",
        "checksum_policy": "full_chain_scores_energy_linearized_and_scan_fnv1a",
        "timer_boundary": "full_chain_split_energy_linearize_accumulate_scan_no_nms",
        "gate": "synthetic_quantized_map_to_score_scan_tail_included",
        "mask": "energy_mask_then_score_threshold_mask",
        "reduction": "scalar_energy_generation_rvv_or_scalar_linearized_copy_scalar_accumulation_and_scan",
        "point_type": "not_applicable_byte_map_pipeline",
        "row_source": "synthetic_quantized_map_to_linearized_score_stream",
        "boundary_note": "Phase 050 的主判定项：包含 energy generation、8-bin linearization、score accumulation 和 threshold scan；只允许 linearized copy 在 RVV build 中变化，不覆盖 NMS、averaged detection、semi-scale 或真实 production dispatch。",
    },
    "LINEMOD production matchTemplates total": {
        "group": "linemod-production-matchtemplates-total",
        "checksum_policy": "production_match_detections_fnv1a",
        "timer_boundary": "LINEMOD::matchTemplates_public_total_current_source",
        "gate": "default_macro_energy_maps_to_linearized_maps_rvv_else_scalar",
        "mask": "not_applicable_copy_only",
        "reduction": "not_applicable_strided_copy",
        "point_type": "not_applicable_quantized_byte_map",
        "row_source": "production_quantized_modality_public_entry",
        "case_kind": "production_direct",
        "boundary": "public_overload_current_source",
        "wrapper": "linemod_template_scoring_production_direct_bench",
        "boundary_note": "Phase 070 production direct：真实调用当前源码编译出的 `LINEMOD::matchTemplates` 公开入口；计时包含 energy maps、linearized copy、score accumulation 和单个最佳 detection 构造，不覆盖 semi-scale 或 separate-energy 编译分支。",
    },
    "LINEMOD production detectTemplates total": {
        "group": "linemod-production-detecttemplates-total",
        "checksum_policy": "production_detect_detections_fnv1a",
        "timer_boundary": "LINEMOD::detectTemplates_public_total_current_source",
        "gate": "default_macro_energy_maps_to_linearized_maps_rvv_else_scalar",
        "mask": "not_applicable_copy_only",
        "reduction": "not_applicable_strided_copy",
        "point_type": "not_applicable_quantized_byte_map",
        "row_source": "production_quantized_modality_public_entry",
        "case_kind": "production_direct",
        "boundary": "public_overload_current_source",
        "wrapper": "linemod_template_scoring_production_direct_bench",
        "boundary_note": "Phase 070 production direct：真实调用当前源码编译出的 `LINEMOD::detectTemplates` 公开入口；计时包含默认 energy maps、linearized copy、score accumulation、threshold scan 和 detection 构造，不覆盖 NMS、averaged detection、semi-scale 或 separate-energy 编译分支。",
    },
    "LINEMOD production semi-scale detectTemplates total": {
        "group": "linemod-production-semiscale-detecttemplates-total",
        "checksum_policy": "production_semiscale_detections_fnv1a",
        "timer_boundary": "LINEMOD::detectTemplatesSemiScaleInvariant_public_total_current_source",
        "gate": "default_macro_semiscale_energy_maps_to_linearized_maps_rvv_else_scalar",
        "mask": "not_applicable_copy_only",
        "reduction": "not_applicable_strided_copy",
        "point_type": "not_applicable_quantized_byte_map",
        "row_source": "production_quantized_modality_public_entry",
        "case_kind": "production_direct",
        "boundary": "public_overload_current_source",
        "wrapper": "linemod_template_scoring_semiscale_production_direct_bench",
        "boundary_note": "Phase 080 production direct：真实调用当前源码编译出的 `LINEMOD::detectTemplatesSemiScaleInvariant` 公开入口；计时包含默认 energy maps、linearized copy、每个 scale 的 score accumulation、threshold scan 和 detection 构造，不覆盖 separate-energy 编译分支、NMS 或 averaged detection。",
    },
}


def repo_relative(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def parse_bench_log(path: Path, expected_mode: str) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(path)
    context: dict[str, Any] = {"path": repo_relative(path), "timings_us": {}}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        text = line.strip()
        if match := DATASET_RE.match(text):
            context.update(
                {
                    "dataset": match.group("dataset"),
                    "mem_size": int(match.group("mem_size")),
                    "nr_maps": int(match.group("nr_maps")),
                }
            )
        elif match := ITERATIONS_RE.match(text):
            context["iterations"] = int(match.group("iterations"))
        elif match := WARMUP_RE.match(text):
            context["warmup_iterations"] = int(match.group("warmup"))
        elif match := TIMING_RE.match(text):
            context["timings_us"][match.group("label")] = float(match.group("us"))
        elif match := CHECKSUM_RE.match(text):
            context.update(
                {
                    "checksum": match.group("checksum"),
                    "mode": match.group("mode"),
                    "max_value": int(match.group("max_value")),
                    "max_index": int(match.group("max_index")),
                }
            )
            if match.group("count_above_threshold") is not None:
                context["count_above_threshold"] = int(match.group("count_above_threshold"))
            if match.group("index_checksum") is not None:
                context["index_checksum"] = match.group("index_checksum")
            if match.group("energy_checksum") is not None:
                context["energy_checksum"] = match.group("energy_checksum")
            if match.group("linearized_checksum") is not None:
                context["linearized_checksum"] = match.group("linearized_checksum")

    missing = [key for key in ("mem_size", "nr_maps", "iterations", "warmup_iterations", "checksum", "mode") if key not in context]
    if missing:
        raise ValueError(f"{path} missing required field(s): {', '.join(missing)}")
    if not context["timings_us"]:
        raise ValueError(f"{path} missing required timing item(s)")
    if context["mode"] != expected_mode:
        raise ValueError(f"{path} mode={context['mode']!r}, expected {expected_mode!r}")
    return context


def collect_runs(repeated_dir: Path) -> list[dict[str, Any]]:
    runs = []
    for run_dir in sorted(path for path in repeated_dir.glob("run-*") if path.is_dir()):
        std = parse_bench_log(run_dir / "run_bench_std.log", "std")
        rvv = parse_bench_log(run_dir / "run_bench_rvv.log", "rvv")
        for key in ("mem_size", "nr_maps", "iterations", "warmup_iterations", "checksum", "max_value", "max_index"):
            if std[key] != rvv[key]:
                raise ValueError(f"{run_dir}: std/rvv {key} mismatch: {std[key]!r} vs {rvv[key]!r}")
        for key in ("count_above_threshold", "index_checksum", "energy_checksum", "linearized_checksum"):
            if key in std and key in rvv and std[key] != rvv[key]:
                raise ValueError(f"{run_dir}: std/rvv {key} mismatch: {std[key]!r} vs {rvv[key]!r}")
        common_timings = sorted(set(std["timings_us"]) & set(rvv["timings_us"]))
        if not common_timings:
            raise ValueError(f"{run_dir}: std/rvv have no common timing labels")
        runs.append(
            {
                "run_label": run_dir.name,
                "run_dir": repo_relative(run_dir),
                "std": std,
                "rvv": rvv,
                "speedups": {
                    label: std["timings_us"][label] / rvv["timings_us"][label]
                    for label in common_timings
                },
            }
        )
    if not runs:
        raise ValueError(f"no run-* directories found under {repeated_dir}")
    return runs


def count_rvv_instructions(asm_path: Path) -> tuple[int, dict[str, int]]:
    if not asm_path.exists():
        return 0, {}
    total = 0
    required: dict[str, int] = {}
    for line in asm_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if RVV_INSTRUCTION_RE.search(line):
            total += 1
        if match := REQUIRED_MNEMONIC_RE.search(line):
            required[match.group(1)] = required.get(match.group(1), 0) + 1
    return total, required


def choose_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    has_degradation = any(value < 1.0 for value in values)
    if has_degradation:
        return "unstable" if median >= 1.0 else "negative"
    if median >= 1.05:
        return "positive"
    if median >= 1.0:
        return "weak_positive"
    if median >= 0.97:
        return "neutral"
    return "negative"


def timing_labels(runs: list[dict[str, Any]], include_labels: list[str]) -> list[str]:
    labels = set(runs[0]["speedups"])
    for run in runs[1:]:
        labels &= set(run["speedups"])
    if include_labels:
        requested = list(dict.fromkeys(include_labels))
        missing = [label for label in requested if label not in labels]
        if missing:
            raise ValueError(f"requested timing label(s) not present in every run: {', '.join(missing)}")
        return requested
    preferred = [label for label in BENCH_CASES if label in labels]
    extras = sorted(labels - set(preferred))
    return preferred + extras


def checksum_fields(log: dict[str, Any]) -> dict[str, Any]:
    fields: dict[str, Any] = {
        "checksum": log["checksum"],
        "max_value": log["max_value"],
        "max_index": log["max_index"],
    }
    if "count_above_threshold" in log:
        fields["count_above_threshold"] = log["count_above_threshold"]
    if "index_checksum" in log:
        fields["index_checksum"] = log["index_checksum"]
    if "energy_checksum" in log:
        fields["energy_checksum"] = log["energy_checksum"]
    if "linearized_checksum" in log:
        fields["linearized_checksum"] = log["linearized_checksum"]
    return fields


def make_comparison(
    label: str,
    runs: list[dict[str, Any]],
    asm_boundary: str,
    rvv_instr_count: int,
    rvv_mnemonics: dict[str, int],
    evidence_role: str,
) -> dict[str, Any]:
    first = runs[0]
    case = BENCH_CASES.get(
        label,
        {
            "group": label.lower().replace(" ", "-"),
            "checksum_policy": "topic_specific_checksum",
            "timer_boundary": label.lower().replace(" ", "_"),
            "gate": "topic_specific_gate",
            "mask": "topic_specific_mask",
            "reduction": "topic_specific_reduction",
            "point_type": "not_applicable",
            "case_kind": "production-shaped",
            "boundary": "test_helper",
            "wrapper": "linemod_template_scoring_bench",
            "boundary_note": "topic-local wrapper 解析到的额外 bench 项。",
        },
    )
    speedups = [run["speedups"][label] for run in runs]
    baseline = {
        "label": "std",
        "boundary": case.get("boundary", "test_helper"),
        "wrapper": case.get("wrapper", "linemod_template_scoring_bench"),
        "row_source": case.get("row_source", "linearized_map_byte_stream"),
        "solve": False,
        "checksum_policy": case["checksum_policy"],
        "timer_boundary": case["timer_boundary"],
        "gate": case["gate"],
        "mask": case["mask"],
        "reduction": case["reduction"],
        "asm_boundary": "scalar_or_compiler_generated_std_build",
        "std_ms": first["std"]["timings_us"][label] / 1000.0,
        "rvv_ms": first["std"]["timings_us"][label] / 1000.0,
        **checksum_fields(first["std"]),
    }
    candidate = {
        "label": "rvv",
        "boundary": case.get("boundary", "test_helper"),
        "wrapper": case.get("wrapper", "linemod_template_scoring_bench"),
        "row_source": case.get("row_source", "linearized_map_byte_stream"),
        "solve": False,
        "checksum_policy": case["checksum_policy"],
        "timer_boundary": case["timer_boundary"],
        "gate": case["gate"],
        "mask": case["mask"],
        "reduction": case["reduction"],
        "asm_boundary": asm_boundary,
        "rvv_instr_count": rvv_instr_count,
        "rvv_mnemonics": rvv_mnemonics,
        "std_ms": first["rvv"]["timings_us"][label] / 1000.0,
        "rvv_ms": first["rvv"]["timings_us"][label] / 1000.0,
        **checksum_fields(first["rvv"]),
    }
    return {
        "name": f"{label} mem_size={first['std']['mem_size']} nr_maps={first['std']['nr_maps']}",
        "group": case["group"],
        "evidence_role": evidence_role,
        "case_kind": case.get("case_kind", "production-shaped"),
        "point_type": case["point_type"],
        "size": first["std"]["mem_size"],
        "run_count": len(runs),
        "requires_board_context": True,
        "metadata": {
            "decision_bucket": choose_bucket(speedups),
            "boundary_note": case["boundary_note"],
        },
        "baseline": baseline,
        "candidate": candidate,
        "ba_values": speedups,
        "repeated_runs": [
            {
                "run_label": run["run_label"],
                "std_us": run["std"]["timings_us"][label],
                "rvv_us": run["rvv"]["timings_us"][label],
                "speedup": run["speedups"][label],
                **checksum_fields(run["std"]),
            }
            for run in runs
        ],
    }


def make_manifest(args: argparse.Namespace, runs: list[dict[str, Any]]) -> dict[str, Any]:
    first = runs[0]
    rvv_instr_count, rvv_mnemonics = count_rvv_instructions(args.asm)
    binary_hash = sha256(args.rvv_binary) if args.rvv_binary and args.rvv_binary.exists() else ""
    asm_boundary = "linemod template scoring bench helper or inlined candidate; required RVV mnemonics present"
    comparisons = [
        make_comparison(label, runs, asm_boundary, rvv_instr_count, rvv_mnemonics, args.evidence_role)
        for label in timing_labels(runs, args.include_label)
    ]
    bucket_values = [comparison["metadata"]["decision_bucket"] for comparison in comparisons]

    metadata = {
        "device": args.device,
        "iterations": first["std"]["iterations"],
        "warmup_iterations": first["std"]["warmup_iterations"],
        "run_count": len(runs),
        "taskset": args.taskset,
        "governor": args.governor,
        "freq": args.freq,
        "temperature": args.temperature,
        "vlen": args.vlen,
        "binary_hash": binary_hash,
        "decision_bucket": "mixed" if len(set(bucket_values)) > 1 else bucket_values[0],
        "rerun_budget": args.rerun_budget,
        "summary_generated_from": repo_relative(args.repeated_dir),
    }
    return {
        "schema_version": 1,
        "summary": {
            "title": "LINEMOD template scoring repeated board summary",
            "evidence_role": args.evidence_role,
            "summary_path": repo_relative(args.summary),
            "label": f"{args.evidence_role} repeated board summary",
            "analysis_script": repo_relative(Path(__file__).resolve()),
            "metadata": metadata,
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
        },
        "comparisons": comparisons,
    }


def write_summary(path: Path, manifest: dict[str, Any], runs: list[dict[str, Any]]) -> None:
    metadata = manifest["summary"]["metadata"]
    evidence_role = manifest["summary"]["evidence_role"]
    path.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        "# LINEMOD Template Scoring Repeated Board Summary",
        "",
        "## Context",
        "",
        f"- evidence_role: `{manifest['summary']['evidence_role']}`",
        f"- device: `{metadata['device']}`",
        f"- iterations: `{metadata['iterations']}`",
        f"- warmup_iterations: `{metadata['warmup_iterations']}`",
        f"- run_count: `{metadata['run_count']}`",
        f"- decision_bucket: `{metadata['decision_bucket']}`",
        f"- binary_hash: `{metadata['binary_hash'] or 'not_recorded'}`",
        "",
        "## Statistics",
        "",
        "| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for comparison in manifest["comparisons"]:
        label = comparison["name"].split(" mem_size=", 1)[0]
        values = [run["speedup"] for run in comparison["repeated_runs"]]
        std_values = [run["std_us"] for run in comparison["repeated_runs"]]
        rvv_values = [run["rvv_us"] for run in comparison["repeated_runs"]]
        lines.append(
            "| {} | {:.6f} | {:.6f} | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | `{}` |".format(
                label,
                statistics.median(std_values),
                statistics.median(rvv_values),
                statistics.median(values),
                min(values),
                max(values),
                sum(1 for value in values if value < 1.0),
                len(values),
                comparison["metadata"]["decision_bucket"],
            )
        )
    lines.extend(["", "## Repeated Values", ""])
    for comparison in manifest["comparisons"]:
        label = comparison["name"].split(" mem_size=", 1)[0]
        lines.extend(
            [
                f"### {label}",
                "",
                f"- timer_boundary: `{comparison['baseline']['timer_boundary']}`",
                f"- row_source: `{comparison['baseline']['row_source']}`",
                f"- asm_boundary: `{comparison['candidate']['asm_boundary']}`",
                f"- checksum_policy: `{comparison['baseline']['checksum_policy']}`",
                f"- boundary_note: {comparison['metadata']['boundary_note']}",
                "",
                "| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |",
                "| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |",
            ]
        )
        for run in comparison["repeated_runs"]:
            lines.append(
                "| {} | {:.6f} | {:.6f} | {:.3f}x | `{}` | {} | `{}` | `{}` | `{}` |".format(
                    run["run_label"],
                    run["std_us"],
                    run["rvv_us"],
                    run["speedup"],
                    run["checksum"],
                    run.get("count_above_threshold", "not_recorded"),
                    run.get("index_checksum", "not_recorded"),
                    run.get("energy_checksum", "not_recorded"),
                    run.get("linearized_checksum", "not_recorded"),
                )
            )
        lines.append("")
    labels = [comparison["name"].split(" mem_size=", 1)[0] for comparison in manifest["comparisons"]]
    label_text = "、".join(f"`{label}`" for label in labels)
    if evidence_role == "production-public":
        boundary_text = (
            "本 summary 只证明上方 `Repeated Values` 中列出的 production public（真实公开入口）"
            f"timing label：{label_text} 在当前源码默认宏配置下命中 "
            "`EnergyMaps -> LinearizedMaps` RVV copy helper 后的整入口耗时。每个 "
            "label 的 `boundary_note` 是该项的主证据边界。它不证明 NMS、averaged detection、"
            "separate-energy 编译分支或新的 RVV family；若后续结果为弱、负、中性或不稳定，只能降级"
            "对应 production-public 边界。"
        )
    else:
        boundary_text = (
            "本 summary 只证明上方 `Repeated Values` 中列出的 timing label 对应的 test helper 边界；"
            "每个 label 的 `boundary_note` 是该项的主证据边界。它不证明未列出的 LINEMOD 子阶段、"
            "NMS、averaged detection 或真实 production dispatch。若某项结果为弱、负、中性或不稳定，"
            "只能降级该 diagnostic boundary，不能直接外推为完整 production no-go。"
        )
    lines.extend(["", "## Boundary", "", boundary_text, ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repeated-dir", type=Path, required=True)
    parser.add_argument("--asm", type=Path, required=True)
    parser.add_argument("--rvv-binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument("--taskset", default="not_recorded")
    parser.add_argument("--governor", default="not_recorded")
    parser.add_argument("--freq", default="not_recorded")
    parser.add_argument("--temperature", default="not_recorded")
    parser.add_argument("--vlen", default="see SoC / ELF (board)")
    parser.add_argument("--rerun-budget", default="5-run initial budget; no extra rerun unless bucket changes or doctor errors")
    parser.add_argument(
        "--evidence-role",
        default="production_shaped_diagnostic",
        help="写入 summary 和 comparison 的 evidence_role；production-direct 阶段传 production-public。",
    )
    parser.add_argument(
        "--include-label",
        action="append",
        default=[],
        help="只把指定 timing label 写入 summary / manifest；可重复传入。未传时包含所有共同 label。",
    )
    args = parser.parse_args()

    runs = collect_runs(args.repeated_dir)
    manifest = make_manifest(args, runs)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_summary(args.summary, manifest, runs)
    print(f"wrote manifest: {args.output}")
    print(f"wrote summary: {args.summary}")
    print(f"decision_bucket: {manifest['summary']['metadata']['decision_bucket']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
