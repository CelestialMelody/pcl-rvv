#!/usr/bin/env python3
"""Generate sac_model_circle3d Evidence Doctor manifests.

这个脚本解析当前 topic 的 board（板卡）bench 日志。`component` 模式把
public path（公开入口路径）和 test-only projection candidate（仅测试使用的投影候选）
转换成 Evidence Doctor（证据体检）可读取的 manifest（证据清单）；
`production-select` 模式只比较 Std/RVV 两个 binary（二进制）的真实公开
`selectWithinDistance` 入口，供生产接入后决策使用。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]
BENCH_ITEM_RE = re.compile(r"^(?P<item>.+?)\s+:\s+(?P<ms>[0-9.]+)\s+ms/iter")
COUNT_RE = re.compile(r"^Count\s+(?P<item>public|candidate)\s+:\s+(?P<count>\d+)")
CHECKSUM_RE = re.compile(r"^Checksum\s+(?P<item>.+?)\s+:\s+(?P<checksum>\S+)")
SIZE_RE = re.compile(r"^size:\s+(?P<size>\d+)")
ITER_RE = re.compile(r"^iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^warmup_iterations:\s+(?P<warmup>\d+)")
POINT_TYPE_RE = re.compile(r"^point_type:\s+(?P<point_type>\S+)")
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


def parse_log(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(path)
    values: dict[str, Any] = {"timings": {}, "counts": {}, "checksums": {}}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
      text = raw.strip()
      if match := SIZE_RE.match(text):
          values["size"] = int(match.group("size"))
      elif match := ITER_RE.match(text):
          values["iterations"] = int(match.group("iterations"))
      elif match := WARMUP_RE.match(text):
          values["warmup_iterations"] = int(match.group("warmup"))
      elif match := POINT_TYPE_RE.match(text):
          values["point_type"] = match.group("point_type")
      elif match := BENCH_ITEM_RE.match(text):
          values["timings"][match.group("item")] = float(match.group("ms"))
      elif match := COUNT_RE.match(text):
          values["counts"][match.group("item")] = int(match.group("count"))
      elif match := CHECKSUM_RE.match(text):
          values["checksums"][match.group("item")] = match.group("checksum")
    return values


def count_rvv_instructions(asm_path: Path, symbol_hint: str) -> int:
    if not asm_path.exists():
        return 0
    in_symbol = False
    count = 0
    for line in asm_path.read_text(encoding="utf-8", errors="replace").splitlines():
        header = SYMBOL_HEADER_RE.match(line.strip())
        if header:
            if in_symbol:
                break
            in_symbol = symbol_hint in header.group("symbol")
            continue
        if in_symbol and RVV_INSTRUCTION_RE.search(line):
            count += 1
    return count


def component_comparison(
    name: str,
    baseline_item: str,
    candidate_item: str,
    reduction: str,
    asm_symbol: str,
    runs: list[dict[str, Any]],
    asm_path: Path,
) -> dict[str, Any]:
    baseline_ms = [run["timings"][baseline_item] for run in runs]
    candidate_ms = [run["timings"][candidate_item] for run in runs]
    ba_values = [a / b for a, b in zip(baseline_ms, candidate_ms)]
    first = runs[0]
    checksum_baseline = first["checksums"].get(baseline_item, "not_recorded")
    checksum_candidate = first["checksums"].get(candidate_item, "not_recorded")
    return {
        "name": name,
        "case_kind": "component_ablation",
        "evidence_role": "component_ablation",
        "build_comparison": "rvv_binary_public_vs_test_helper_repeated",
        "allowed_contract_mismatches": ["boundary", "wrapper", "timer_boundary", "reduction"],
        "point_type": "PointXYZ",
        "size": first.get("size", "not_recorded"),
        "run_count": len(runs),
        "iterations": first.get("iterations", "not_recorded"),
        "warmup_iterations": first.get("warmup_iterations", "not_recorded"),
        "ba_values": ba_values,
        "public_ms_values": baseline_ms,
        "candidate_ms_values": candidate_ms,
        "diagnostic_to_production_mismatch_audit": {
            "evidence_role": "component_ablation",
            "ab_boundary": "public overload companion vs test-only helper in one RVV binary",
            "decision_question": "whether projection RVV candidate is worth a bounded production probe",
            "can_extrapolate_to_production": "no_direct_extrapolation",
            "baseline_mismatch_risk": "yes_public_wrapper_vs_test_helper",
            "clean_adoption_requires": "production direct public Std/RVV after explicit user approval",
        },
        "baseline": {
            "label": f"RVV build {baseline_item}",
            "row_source": "direct_indexed_indices",
            "solve": False,
            "checksum_policy": "bench_checksum_plus_gtest_tolerance",
            "gate": "PointXYZ_registered_single_float_xyz_aos_u32_offset",
            "boundary": "public_overload",
            "wrapper": f"SampleConsensusModelCircle3D::{baseline_item}",
            "timer_boundary": f"{baseline_item}_public_entry_only",
            "mask": "circle3d_projection_squared_distance_less_than_threshold",
            "reduction": "scalar_eigen_projection",
            "checksum": checksum_baseline,
            "asm_boundary": "not_applicable_no_production_rvv_dispatch",
            "rvv_instr_count": 0,
            "rvv_ms": sum(baseline_ms) / len(baseline_ms),
        },
        "candidate": {
            "label": f"RVV build {candidate_item}",
            "row_source": "direct_indexed_indices",
            "solve": False,
            "checksum_policy": "bench_checksum_plus_gtest_tolerance",
            "gate": "PointXYZ_registered_single_float_xyz_aos_u32_offset",
            "boundary": "test_helper",
            "wrapper": f"SampleConsensusModelCircle3DAccess::{asm_symbol}",
            "timer_boundary": f"{candidate_item}_test_helper_only",
            "mask": "circle3d_projection_squared_distance_less_than_threshold",
            "reduction": reduction,
            "checksum": checksum_candidate,
            "asm_boundary": asm_symbol,
            "rvv_instr_count": count_rvv_instructions(asm_path, asm_symbol),
            "rvv_ms": sum(candidate_ms) / len(candidate_ms),
        },
    }


def production_select_comparison(
    std_runs: list[dict[str, Any]],
    rvv_runs: list[dict[str, Any]],
    asm_path: Path,
    candidate_gate: str | None,
) -> dict[str, Any]:
    item = "public selectWithinDistance"
    checksum_item = "public select"
    baseline_ms = [run["timings"][item] for run in std_runs]
    candidate_ms = [run["timings"][item] for run in rvv_runs]
    ba_values = [a / b for a, b in zip(baseline_ms, candidate_ms)]
    first_std = std_runs[0]
    first_rvv = rvv_runs[0]
    point_type = str(first_std.get("point_type", first_rvv.get("point_type", "PointXYZ")))
    rvv_gate = candidate_gate
    if rvv_gate is None:
        if point_type == "PointXYZ":
            rvv_gate = "exact_PointXYZ_signed_i32_index_u32_offset_non_degenerate_projection"
        else:
            rvv_gate = "candidate_gate_not_recorded"
    checksum_baseline = first_std["checksums"].get(checksum_item, "not_recorded")
    checksum_candidate = first_rvv["checksums"].get(checksum_item, "not_recorded")
    return {
        "name": f"sac_model_circle3d selectWithinDistance production public Std/RVV {point_type} 65536",
        "case_kind": "production_public",
        "evidence_role": "production-public",
        "build_comparison": "std_vs_rvv_public_repeated",
        "allowed_contract_mismatches": ["gate", "reduction"],
        "point_type": point_type,
        "size": first_std.get("size", "not_recorded"),
        "run_count": len(std_runs),
        "iterations": first_std.get("iterations", "not_recorded"),
        "warmup_iterations": first_std.get("warmup_iterations", "not_recorded"),
        "ba_values": ba_values,
        "public_ms_values": baseline_ms,
        "candidate_ms_values": candidate_ms,
        "diagnostic_to_production_mismatch_audit": {
            "evidence_role": "production-public",
            "ab_boundary": "Std binary public overload vs RVV binary public overload",
            "decision_question": "whether the production selectWithinDistance RVV dispatch is worth keeping",
            "can_extrapolate_to_production": "yes_for_current_public_boundary_only",
            "baseline_mismatch_risk": "low_same_public_wrapper_and_timer_boundary",
            "clean_adoption_requires": "no existing RVV family in this public boundary; future RVV families need production-detail A/B",
        },
        "baseline": {
            "label": "Std build public selectWithinDistance",
            "row_source": "direct_indexed_indices",
            "solve": False,
            "checksum_policy": "bench_checksum_plus_gtest_tolerance",
            "gate": "not_applicable_std_build",
            "boundary": "public_overload",
            "wrapper": "SampleConsensusModelCircle3D::selectWithinDistance",
            "timer_boundary": "public selectWithinDistance_public_entry_only",
            "mask": "circle3d_projection_squared_distance_less_than_threshold",
            "reduction": "scalar_eigen_projection",
            "checksum": checksum_baseline,
            "asm_boundary": "not_applicable_std_build",
            "rvv_instr_count": 0,
            "rvv_ms": sum(baseline_ms) / len(baseline_ms),
        },
        "candidate": {
            "label": "RVV build public selectWithinDistance",
            "row_source": "direct_indexed_indices",
            "solve": False,
            "checksum_policy": "bench_checksum_plus_gtest_tolerance",
            "gate": rvv_gate,
            "boundary": "public_overload",
            "wrapper": "SampleConsensusModelCircle3D::selectWithinDistance",
            "timer_boundary": "public selectWithinDistance_public_entry_only",
            "mask": "circle3d_projection_squared_distance_less_than_threshold",
            "reduction": "rvv_projection_radius_sqrt_vcompress_error_store",
            "checksum": checksum_candidate,
            "asm_boundary": "selectWithinDistanceRVVCircle3D",
            "rvv_instr_count": count_rvv_instructions(asm_path, "selectWithinDistanceRVVCircle3D"),
            "rvv_ms": sum(candidate_ms) / len(candidate_ms),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeat-root", type=Path, required=True)
    parser.add_argument("--asm", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary-title", default="sac_model_circle3d Phase 000 projection repeated board summary")
    parser.add_argument("--mode", choices=("component", "production-select"), default="component")
    parser.add_argument("--candidate-gate", default=None)
    args = parser.parse_args()

    if args.mode == "component":
        run_logs = sorted(args.repeat_root.glob("run-*/run_bench_rvv.log"))
        if not run_logs:
            run_logs = sorted(args.repeat_root.glob("run-*/run_bench.log"))
        if not run_logs:
            raise SystemExit(f"no run logs found under {args.repeat_root}")
        runs = [parse_log(path) for path in run_logs]
        comparisons = [
            component_comparison(
                "sac_model_circle3d countWithinDistance public vs projection candidate PointXYZ 65536",
                "public countWithinDistance",
                "candidate count projection",
                "rvv_projection_radius_sqrt_mask_popcount",
                "countWithinDistanceProjectionRVV",
                runs,
                args.asm,
            ),
            component_comparison(
                "sac_model_circle3d selectWithinDistance public vs projection candidate PointXYZ 65536",
                "public selectWithinDistance",
                "candidate select projection",
                "rvv_projection_radius_sqrt_vcompress_error_store",
                "selectWithinDistanceProjectionRVV",
                runs,
                args.asm,
            ),
        ]
        evidence_role = "component_ablation"
        metadata_source = runs[0]
        run_count = len(runs)
        label = f"{run_count}-run board repeated circle3d projection component summary"
    else:
        std_logs = sorted(args.repeat_root.glob("run-*/run_bench_std.log"))
        rvv_logs = sorted(args.repeat_root.glob("run-*/run_bench_rvv.log"))
        if not std_logs or not rvv_logs:
            raise SystemExit(f"std/rvv run logs not found under {args.repeat_root}")
        if len(std_logs) != len(rvv_logs):
            raise SystemExit(f"std/rvv run log count mismatch under {args.repeat_root}")
        std_runs = [parse_log(path) for path in std_logs]
        rvv_runs = [parse_log(path) for path in rvv_logs]
        comparisons = [production_select_comparison(std_runs, rvv_runs, args.asm, args.candidate_gate)]
        evidence_role = "production-public"
        metadata_source = std_runs[0]
        run_count = len(std_runs)
        label = f"{run_count}-run board repeated circle3d select production public summary"

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.summary_title,
            "evidence_role": evidence_role,
            "summary_path": repo_relative(args.output),
            "label": label,
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": metadata_source.get("iterations", "not_recorded"),
                "warmup_iterations": metadata_source.get("warmup_iterations", "not_recorded"),
                "run_count": run_count,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {
            "strict_ab": args.mode == "production-select",
            "thresholds": {
                "min_repeated_runs": 5,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": comparisons,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
