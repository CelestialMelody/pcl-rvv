#!/usr/bin/env python3
"""Generate the sac_model_sphere board Evidence Doctor manifest.

这个脚本只理解当前 topic 的 `bench_sac_model_sphere` 输出格式。它把
Std/RVV board bench logs 转成 Evidence Doctor（证据体检）可读取的 JSON
manifest。public count 与 production 接入后的 public select 行是
production-direct（真实生产路径）证据；getDistances candidate 行仍是
production-shaped diagnostic（生产形态诊断）证据，不能直接外推成
production dispatch（生产分流）已接入。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]

ITEM_METADATA: dict[str, dict[str, str]] = {
    "public selectWithinDistance": {
        "name": "sac_model_sphere public selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "public-entry baseline",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelSphereBench::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "sphere_shell_inner_outer_inclusive",
        "reduction": "ordered_inlier_and_error_distance_write",
        "asm_symbol": "selectWithinDistance",
    },
    "public countWithinDistance": {
        "name": "sac_model_sphere countWithinDistance PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelSphereBench::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "mask": "sphere_shell_inner_outer_inclusive",
        "reduction": "inlier_count",
        "asm_symbol": "countWithinDistanceRVV",
    },
    "public getDistancesToModel": {
        "name": "sac_model_sphere public getDistancesToModel PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "public-entry baseline",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelSphereBench::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "none_all_indices",
        "reduction": "dense_distance_store_per_index",
        "asm_symbol": "getDistancesToModel",
    },
    "diagnostic candidate selectWithinDistance": {
        "name": "sac_model_sphere diagnostic candidate selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelSphereBench::selectWithinDistanceCandidate",
        "timer_boundary": "selectWithinDistance_candidate_only",
        "mask": "sphere_shell_inner_outer_inclusive",
        "reduction": "ordered_inlier_and_error_distance_write",
        "asm_symbol": "selectWithinDistanceCandidateRVV",
    },
    "diagnostic candidate getDistancesToModel": {
        "name": "sac_model_sphere diagnostic candidate getDistancesToModel PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelSphereBench::getDistancesToModelCandidate",
        "timer_boundary": "getDistancesToModel_candidate_only",
        "mask": "none_all_indices",
        "reduction": "dense_distance_store_per_index",
        "asm_symbol": "getDistancesToModelCandidateRVV",
    },
    "vcompress candidate selectWithinDistance": {
        "name": "sac_model_sphere vcompress candidate selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_detail",
        "case_kind": "test-only implementation-family candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelSphereBench::selectWithinDistanceVCompressCandidate",
        "timer_boundary": "selectWithinDistance_vcompress_candidate_only",
        "mask": "sphere_shell_inner_outer_inclusive",
        "reduction": "vcompress_ordered_inlier_and_error_distance_write",
        "asm_symbol": "selectWithinDistanceVCompressCandidateRVV",
    },
}

DATASET_RE = re.compile(
    r"^Dataset:\s+(?P<dataset>.+?)\s+\(points=(?P<size>\d+),\s*(?P<layout>.+)\)"
)
ITER_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s+(?P<warmup>\d+)")
BUILD_RE = re.compile(r"^Build:\s+(?P<build>Std|RVV)")
CHECKSUM_RE = re.compile(r"^Checksum:\s+(?P<checksum>\S+)")
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
    path: Path, expected_build: str, metadata: dict[str, dict[str, str]]
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
            context["point_type"] = parse_point_type(match.group("dataset"))
        elif match := ITER_RE.match(text):
            context["iterations"] = int(match.group("iterations"))
        elif match := WARMUP_RE.match(text):
            context["warmup_iterations"] = int(match.group("warmup"))
        elif match := BUILD_RE.match(text):
            context["build"] = match.group("build")
        elif match := CHECKSUM_RE.match(text):
            context["checksum"] = match.group("checksum")
        elif match := BENCH_ITEM_RE.match(text):
            item = match.group("item")
            if item in metadata:
                values[item] = float(match.group("ms"))

    if context.get("build") != expected_build:
        raise ValueError(f"{path} build={context.get('build')!r}, expected {expected_build!r}")
    missing = sorted(set(metadata) - set(values))
    if missing:
        raise ValueError(f"{path} missing bench item(s): {', '.join(missing)}")
    return context, values


def parse_point_type(dataset: str) -> str:
    for point_type in ("PointXYZRGBA", "PointXYZRGB", "PointXYZI", "PointXYZ"):
        if f" {point_type} " in f" {dataset} ":
            return point_type
    return "PointXYZ"


def metadata_name(meta: dict[str, str], context: dict[str, Any]) -> str:
    name = meta["name"].replace("PointXYZ", context["point_type"])
    return re.sub(r"\b65536\b", str(context["size"]), name)


def count_rvv_instructions(
    asm_path: Path, symbol_hint: str, point_type: str | None = None
) -> int:
    if not asm_path.exists():
        return 0
    in_symbol = False
    count = 0
    for line in asm_path.read_text(encoding="utf-8", errors="replace").splitlines():
        header = SYMBOL_HEADER_RE.match(line)
        if header:
            symbol = header.group("symbol")
            in_symbol = symbol_hint in symbol and (
                point_type is None or f"pcl::{point_type}>" in symbol
            )
            continue
        if in_symbol and RVV_INSTRUCTION_RE.search(line):
            count += 1
    return count


def parse_repeated_runs(
    repeat_root: Path, metadata: dict[str, dict[str, str]]
) -> tuple[dict[str, Any], dict[str, list[float]], dict[str, list[float]], str, str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    grouped_std: dict[str, list[float]] = {item: [] for item in metadata}
    grouped_rvv: dict[str, list[float]] = {item: [] for item in metadata}
    std_checksum = ""
    rvv_checksum = ""

    for run_dir in run_dirs:
        std_context, std_values = parse_bench_log(run_dir / "run_bench_std.log", "Std", metadata)
        rvv_context, rvv_values = parse_bench_log(run_dir / "run_bench_rvv.log", "RVV", metadata)
        for key in ("dataset", "size", "iterations", "warmup_iterations"):
            if std_context.get(key) != rvv_context.get(key):
                raise ValueError(f"{run_dir} context mismatch for {key}")
        if base_context is None:
            base_context = std_context
            std_checksum = str(std_context.get("checksum", ""))
            rvv_checksum = str(rvv_context.get("checksum", ""))
        else:
            for key in ("dataset", "size", "iterations", "warmup_iterations"):
                if base_context.get(key) != std_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
            if std_checksum != str(std_context.get("checksum", "")):
                raise ValueError(f"{run_dir} Std checksum changed")
            if rvv_checksum != str(rvv_context.get("checksum", "")):
                raise ValueError(f"{run_dir} RVV checksum changed")
        for item in metadata:
            grouped_std[item].append(std_values[item])
            grouped_rvv[item].append(rvv_values[item])

    assert base_context is not None
    return base_context, grouped_std, grouped_rvv, std_checksum, rvv_checksum


def build_comparison(
    item: str,
    std_values: list[float],
    rvv_values: list[float],
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
    metadata: dict[str, dict[str, str]],
) -> dict[str, Any]:
    meta = metadata[item]
    ba_values = [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    rvv_instr_count = count_rvv_instructions(
        asm_path, meta["asm_symbol"], context["point_type"]
    )
    common_side = {
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "aggregate_entry_output_size_checksum",
        "timer_boundary": meta["timer_boundary"],
        "gate": f"{context['point_type']}_registered_single_float_xyz_aos_u32_offset",
        "mask": meta["mask"],
        "reduction": meta["reduction"],
    }
    return {
        "name": metadata_name(meta, context),
        "case_kind": meta["case_kind"],
        "evidence_role": meta["evidence_role"],
        "build_comparison": "std_vs_rvv_repeated",
        "point_type": context["point_type"],
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
            "rvv_instr_count": rvv_instr_count,
            "rvv_ms": round(sum(rvv_values) / len(rvv_values), 6),
        },
    }


def select_metadata_for_mode(select_mode: str) -> dict[str, dict[str, str]]:
    metadata = {key: value.copy() for key, value in ITEM_METADATA.items()}
    if select_mode in {"diagnostic", "production"}:
        metadata.pop("vcompress candidate selectWithinDistance", None)
    if select_mode == "production":
        metadata["public selectWithinDistance"].update(
            {
                "evidence_role": "production_direct",
                "case_kind": "production-public",
                "asm_symbol": "selectWithinDistanceRVV",
            }
        )
    elif select_mode == "vcompress":
        metadata["public selectWithinDistance"].update(
            {
                "evidence_role": "production_direct",
                "case_kind": "production-public",
                "asm_symbol": "selectWithinDistanceRVV",
            }
        )
    elif select_mode != "diagnostic":
        raise ValueError(f"unknown --select-mode: {select_mode}")
    return metadata


def build_vcompress_detail_comparison(
    current_values: list[float],
    candidate_values: list[float],
    context: dict[str, Any],
    checksum: str,
    asm_path: Path,
    metadata: dict[str, dict[str, str]],
) -> dict[str, Any]:
    candidate_meta = metadata["vcompress candidate selectWithinDistance"]
    ba_values = [
        round(current_ms / candidate_ms, 4)
        for current_ms, candidate_ms in zip(current_values, candidate_values)
        if candidate_ms > 0.0
    ]
    return {
        "name": f"sac_model_sphere vcompress select vs current production select {context['point_type']} {context['size']}",
        "case_kind": "implementation-family comparison",
        "evidence_role": "production_detail",
        "build_comparison": "rvv_vs_rvv_repeated",
        "point_type": context["point_type"],
        "size": context["size"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "baseline_ms_values": current_values,
        "candidate_ms_values": candidate_values,
        "baseline": {
            "label": "current production RVV helper",
            "boundary": "public_overload",
            "wrapper": "SampleConsensusModelSphereBench::selectWithinDistance public entry",
            "row_source": "direct_indexed_indices",
            "solve": False,
            "checksum_policy": "aggregate_entry_output_size_checksum",
            "timer_boundary": "selectWithinDistance_public_entry_only",
            "gate": f"{context['point_type']}_registered_single_float_xyz_aos_u32_offset",
            "mask": "sphere_shell_inner_outer_inclusive",
            "reduction": "ordered_inlier_and_error_distance_write",
            "checksum": checksum,
            "asm_boundary": "selectWithinDistanceRVV",
            "rvv_instr_count": count_rvv_instructions(
                asm_path, "selectWithinDistanceRVV", context["point_type"]
            ),
            "rvv_ms": round(sum(current_values) / len(current_values), 6),
        },
        "candidate": {
            "label": "test-only vcompress candidate",
            "boundary": candidate_meta["boundary"],
            "wrapper": candidate_meta["wrapper"],
            "row_source": "direct_indexed_indices",
            "solve": False,
            "checksum_policy": "aggregate_entry_output_size_checksum",
            "timer_boundary": candidate_meta["timer_boundary"],
            "gate": f"{context['point_type']}_registered_single_float_xyz_aos_u32_offset",
            "mask": candidate_meta["mask"],
            "reduction": candidate_meta["reduction"],
            "checksum": checksum,
            "asm_boundary": candidate_meta["asm_symbol"],
            "rvv_instr_count": count_rvv_instructions(
                asm_path, candidate_meta["asm_symbol"], context["point_type"]
            ),
            "rvv_ms": round(sum(candidate_values) / len(candidate_values), 6),
        },
    }


def build_manifest(
    repeat_root: Path,
    asm_path: Path,
    output_path: Path,
    summary_title: str,
    summary_role: str,
    select_mode: str,
) -> dict[str, Any]:
    metadata = select_metadata_for_mode(select_mode)
    context, std_values, rvv_values, std_checksum, rvv_checksum = parse_repeated_runs(repeat_root, metadata)
    comparisons = [
        build_comparison(item, std_values[item], rvv_values[item], context, std_checksum, rvv_checksum, asm_path, metadata)
        for item in metadata
    ]
    if select_mode == "vcompress":
        comparisons.append(
            build_vcompress_detail_comparison(
                rvv_values["public selectWithinDistance"],
                rvv_values["vcompress candidate selectWithinDistance"],
                context,
                rvv_checksum,
                asm_path,
                metadata,
            )
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": summary_role,
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repeat-root",
        type=Path,
        default=TOPIC_ROOT / "log/board/repeated-20260827-phase000",
    )
    parser.add_argument(
        "--asm",
        type=Path,
        default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_sphere_rvv.full.asm",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=TOPIC_ROOT / "doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json",
    )
    parser.add_argument(
        "--summary-title",
        default="sac_model_sphere repeated board summary",
    )
    parser.add_argument(
        "--summary-role",
        default="production_shaped_diagnostic",
    )
    parser.add_argument(
        "--select-mode",
        choices=("diagnostic", "production", "vcompress"),
        default="diagnostic",
    )
    args = parser.parse_args()

    original_metadata = ITEM_METADATA.copy()
    ITEM_METADATA.update(select_metadata_for_mode(args.select_mode))
    manifest = build_manifest(
        args.repeat_root,
        args.asm,
        args.output,
        args.summary_title,
        args.summary_role,
        args.select_mode,
    )
    ITEM_METADATA.clear()
    ITEM_METADATA.update(original_metadata)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
