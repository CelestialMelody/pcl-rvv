#!/usr/bin/env python3
"""Generate the sac_model_cylinder board Evidence Doctor manifest.

这个 wrapper（主题本地包装脚本）只解析当前 topic 的 repeated board
（重复板卡）bench 输出，并生成 Evidence Doctor（证据体检）可读取的
manifest（证据清单）。`--mode diagnostic` 保留 Phase 000 的
production-shaped diagnostic（生产形态诊断）证据；`--mode production`
只抽真实 public entry（公开入口），用于 Phase 020 production direct
（真实生产路径证据）。
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
        "name": "sac_model_cylinder public countWithinDistance PointXYZ+Normal 65536",
        "point_type": "PointXYZ+Normal",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCylinder::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "mask": "weighted_euclid_early_gate_then_normal_angle_threshold",
        "reduction": "inlier_count",
        "asm_symbol": "countWithinDistanceRVVCylinder",
    },
    "diagnostic candidate countWithinDistance": {
        "name": "sac_model_cylinder diagnostic candidate countWithinDistance PointXYZ+Normal 65536",
        "point_type": "PointXYZ+Normal",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelCylinderDiagnostic::countWithinDistanceCandidate",
        "timer_boundary": "countWithinDistance_candidate_only",
        "mask": "rvv_weighted_euclid_early_gate_then_normal_angle_threshold",
        "reduction": "rvv_mask_popcount_inlier_count",
        "asm_symbol": "countWithinDistanceCandidateRVV",
    },
    "public selectWithinDistance": {
        "name": "sac_model_cylinder public selectWithinDistance PointXYZ+Normal 65536",
        "point_type": "PointXYZ+Normal",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCylinder::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "weighted_euclid_early_gate_then_normal_angle_threshold",
        "reduction": "ordered_inliers_and_error_sqr_dists",
        "asm_symbol": "selectWithinDistanceRVVCylinder",
    },
    "public getDistancesToModel": {
        "name": "sac_model_cylinder public getDistancesToModel PointXYZ+Normal 65536",
        "point_type": "PointXYZ+Normal",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCylinder::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "weighted_euclid_and_normal_angle_all_indices",
        "reduction": "dense_distance_vector",
        "asm_symbol": "getDistancesToModelRVVCylinder",
    },
    "diagnostic candidate selectWithinDistance": {
        "name": "sac_model_cylinder diagnostic candidate selectWithinDistance PointXYZ+Normal 65536",
        "point_type": "PointXYZ+Normal",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelCylinderDiagnostic::selectWithinDistanceCandidate",
        "timer_boundary": "selectWithinDistance_candidate_only",
        "mask": "rvv_weighted_euclid_early_gate_then_normal_angle_threshold",
        "reduction": "vcompress_ordered_inliers_and_error_sqr_dists",
        "asm_symbol": "selectWithinDistanceCandidateRVV",
    },
}

for _point_type in ("PointXYZI+Normal", "PointXYZRGB+Normal", "PointXYZ+PointNormal"):
    ITEM_METADATA[f"public {_point_type} countWithinDistance"] = {
        "name": f"sac_model_cylinder public countWithinDistance {_point_type} 65536",
        "point_type": _point_type,
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCylinder::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "gate": "traits_gated_float_xyz_normal_aos_u32_offset",
        "mask": "weighted_euclid_early_gate_then_normal_angle_threshold",
        "reduction": "inlier_count",
        "asm_symbol": "countWithinDistanceRVVCylinder",
    }
    ITEM_METADATA[f"public {_point_type} selectWithinDistance"] = {
        "name": f"sac_model_cylinder public selectWithinDistance {_point_type} 65536",
        "point_type": _point_type,
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCylinder::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "gate": "traits_gated_float_xyz_normal_aos_u32_offset",
        "mask": "weighted_euclid_early_gate_then_normal_angle_threshold",
        "reduction": "ordered_inliers_and_error_sqr_dists",
        "asm_symbol": "selectWithinDistanceRVVCylinder",
    }
    ITEM_METADATA[f"public {_point_type} getDistancesToModel"] = {
        "name": f"sac_model_cylinder public getDistancesToModel {_point_type} 65536",
        "point_type": _point_type,
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelCylinder::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "gate": "traits_gated_float_xyz_normal_aos_u32_offset",
        "mask": "weighted_euclid_and_normal_angle_all_indices",
        "reduction": "dense_distance_vector",
        "asm_symbol": "getDistancesToModelRVVCylinder",
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


def parse_item_list(text: str | None) -> list[str]:
    aliases = {
        "count": ["public countWithinDistance", "diagnostic candidate countWithinDistance"],
        "select": ["public selectWithinDistance", "diagnostic candidate selectWithinDistance"],
        "getdistances": ["public getDistancesToModel"],
        "pointtypes": [
            "public PointXYZI+Normal countWithinDistance",
            "public PointXYZI+Normal selectWithinDistance",
            "public PointXYZI+Normal getDistancesToModel",
            "public PointXYZRGB+Normal countWithinDistance",
            "public PointXYZRGB+Normal selectWithinDistance",
            "public PointXYZRGB+Normal getDistancesToModel",
            "public PointXYZ+PointNormal countWithinDistance",
            "public PointXYZ+PointNormal selectWithinDistance",
            "public PointXYZ+PointNormal getDistancesToModel",
        ],
        "all": list(ITEM_METADATA),
    }
    if not text:
        return aliases["all"]
    items: list[str] = []
    for raw_part in text.split(","):
        part = raw_part.strip()
        if not part:
            continue
        for item in aliases.get(part, [part]):
            if item not in ITEM_METADATA:
                raise ValueError(f"unknown item for --items: {item}")
            if item not in items:
                items.append(item)
    if not items:
        raise ValueError("--items resolved to an empty item list")
    return items


def items_for_mode(items: list[str], mode: str) -> list[str]:
    if mode == "production":
        return [item for item in items if item.startswith("public ")]
    return items


def parse_bench_log(path: Path, expected_build: str, selected_items: list[str]) -> tuple[dict[str, Any], dict[str, float]]:
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
    if not asm_path.exists() or symbol_hint.startswith("not_applicable"):
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
) -> tuple[dict[str, Any], dict[str, list[float]], dict[str, list[float]], dict[str, str], dict[str, str]]:
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
        for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
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
            for key in ("dataset", "size", "layout", "iterations", "warmup_iterations"):
                if base_context.get(key) != std_context.get(key):
                    raise ValueError(f"{run_dir} repeated context mismatch for {key}")
        for item in selected_items:
            grouped_std[item].append(std_values[item])
            grouped_rvv[item].append(rvv_values[item])

    assert base_context is not None
    return base_context, grouped_std, grouped_rvv, std_checksums, rvv_checksums


def build_comparison(
    item: str,
    std_values: list[float],
    rvv_values: list[float],
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
    mode: str,
) -> dict[str, Any]:
    meta = ITEM_METADATA[item]
    is_public_companion = mode == "diagnostic" and item.startswith("public ")
    ba_values = [] if is_public_companion else [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    evidence_role = "summary_only_unknown" if is_public_companion else meta["evidence_role"]
    case_kind = "production-public-companion" if is_public_companion else meta["case_kind"]
    wrapper = (
        meta["wrapper"] + " historical companion"
        if is_public_companion else meta["wrapper"]
    )
    asm_symbol = (
        "not_applicable_no_production_rvv_dispatch"
        if is_public_companion else meta["asm_symbol"]
    )
    common_side = {
        "boundary": meta["boundary"],
        "wrapper": wrapper,
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "aggregate_entry_output_size_checksum",
        "timer_boundary": meta["timer_boundary"],
        "gate": meta.get("gate", "PointXYZ_Normal_registered_float_xyz_normal_aos_u32_offset"),
        "mask": meta["mask"],
        "reduction": meta["reduction"],
    }
    return {
        "name": meta["name"],
        "case_kind": case_kind,
        "evidence_role": evidence_role,
        "build_comparison": "std_vs_rvv_repeated",
        "point_type": meta.get("point_type", "PointXYZ+Normal"),
        "size": context["size"],
        "layout": context["layout"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "std_ms_values": std_values,
        "rvv_ms_values": rvv_values,
        "comparison_note": (
            "public companion only; Phase 000 production source had no cylinder RVV dispatch"
            if is_public_companion else
            "production public Std/RVV repeated board comparison"
            if item.startswith("public ") else
            "diagnostic candidate Std/RVV repeated board comparison"
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
            "asm_boundary": asm_symbol,
            "rvv_instr_count": count_rvv_instructions(asm_path, asm_symbol),
            "rvv_ms": round(sum(rvv_values) / len(rvv_values), 6),
        },
    }


def build_manifest(repeat_root: Path, asm_path: Path, output_path: Path, selected_items: list[str], mode: str) -> dict[str, Any]:
    selected_items = items_for_mode(selected_items, mode)
    if not selected_items:
        raise ValueError(f"--mode {mode} selected no benchmark items")
    context, std_values, rvv_values, std_checksums, rvv_checksums = parse_repeated_runs(
        repeat_root,
        selected_items,
    )
    return {
        "schema_version": 1,
        "summary": {
            "title": (
                "sac_model_cylinder Phase 040 production count/select/getDistances point-type repeated board summary"
                if mode == "production" else
                "sac_model_cylinder Phase 000 count/select repeated board summary"
            ),
            "evidence_role": "production_direct" if mode == "production" else "production_shaped_diagnostic",
            "summary_path": repo_relative(output_path),
            "label": "5-run board repeated summary",
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "RVV board",
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
            "strict_ab": mode == "production",
            "thresholds": {
                "min_repeated_runs": 5,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": [
            build_comparison(
                item,
                std_values[item],
                rvv_values[item],
                context,
                std_checksums[item],
                rvv_checksums[item],
                asm_path,
                mode,
            )
            for item in selected_items
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeat-root", type=Path, default=TOPIC_ROOT / "log/board/repeated-phase000-cylinder")
    parser.add_argument("--asm", type=Path, default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_cylinder_rvv.full.asm")
    parser.add_argument("--output", type=Path, default=TOPIC_ROOT / "doc/phases/000-cylinder-count-select-diagnostic/repeated-evidence-manifest.json")
    parser.add_argument("--items", default="all", help="comma separated labels or aliases: count, select, getdistances, pointtypes, all")
    parser.add_argument("--mode", choices=("diagnostic", "production"), default="diagnostic")
    args = parser.parse_args()

    manifest = build_manifest(args.repeat_root, args.asm, args.output, parse_item_list(args.items), args.mode)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
