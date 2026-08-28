#!/usr/bin/env python3
"""Generate the sac_model_stick board Evidence Doctor manifest.

这个脚本只理解当前 topic 的 `bench_sac_model_stick` 输出格式。它把
Std/RVV board bench logs 转成 Evidence Doctor（证据体检）可读取的 JSON manifest。
Phase 080 以后，`public ...` 行代表 production direct（真实生产路径）证据。
Phase 100 用 `production-getdistances` focus（只抽 public getDistances 行）记录
当前向量写回实现；历史 diagnostic candidate（诊断候选）行只用于 Phase 000/020/040
的旧日志解析。
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
        "name": "sac_model_stick public countWithinDistance PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelStick::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "mask": "stick_inner_outer_threshold_band",
        "reduction": "nr_i_minus_nr_o_saturating_zero",
        "asm_symbol": "countWithinDistanceRVV",
    },
    "diagnostic candidate countWithinDistance": {
        "name": "sac_model_stick diagnostic candidate countWithinDistance PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelStickDiagnostic::countWithinDistanceCandidate",
        "timer_boundary": "countWithinDistance_candidate_only",
        "mask": "stick_inner_outer_threshold_band",
        "reduction": "nr_i_minus_nr_o_saturating_zero",
        "asm_symbol": "countWithinDistanceCandidateRVV",
    },
    "public selectWithinDistance": {
        "name": "sac_model_stick public selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelStick::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "stick_single_inner_threshold",
        "reduction": "ordered_inlier_and_error_output",
        "asm_symbol": "selectWithinDistanceRVV",
    },
    "diagnostic candidate selectWithinDistance": {
        "name": "sac_model_stick diagnostic candidate selectWithinDistance PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelStickDiagnostic::selectWithinDistanceCandidate",
        "timer_boundary": "selectWithinDistance_candidate_only",
        "mask": "stick_single_inner_threshold",
        "reduction": "indexed_vcompress_ordered_inlier_and_error_output",
        "asm_symbol": "selectWithinDistanceCandidateRVV",
    },
    "public getDistancesToModel": {
        "name": "sac_model_stick public getDistancesToModel PointXYZ 65536",
        "evidence_role": "production_direct",
        "case_kind": "production-public",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelStick::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "radius_max_penalty_threshold",
        "reduction": "dense_distance_output_with_penalty",
        "asm_symbol": "getDistancesToModelRVV",
    },
    "diagnostic candidate getDistancesToModel": {
        "name": "sac_model_stick diagnostic candidate getDistancesToModel PointXYZ 65536",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "test-only candidate",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelStickDiagnostic::getDistancesToModelCandidate",
        "timer_boundary": "getDistancesToModel_candidate_only",
        "mask": "radius_max_penalty_threshold",
        "reduction": "rvv_sqrt_scalar_double_dense_distance_output_with_penalty",
        "asm_symbol": "getDistancesToModelCandidateRVV",
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


def metadata_for_focus(focus: str) -> dict[str, dict[str, str]]:
    if focus == "count":
        return {
            key: value
            for key, value in ITEM_METADATA.items()
            if "countWithinDistance" in key
        }
    if focus == "select":
        return {
            key: value
            for key, value in ITEM_METADATA.items()
            if "selectWithinDistance" in key
        }
    if focus == "getdistances":
        return {
            key: value
            for key, value in ITEM_METADATA.items()
            if "getDistancesToModel" in key
        }
    if focus == "production":
        return {
            key: value
            for key, value in ITEM_METADATA.items()
            if key.startswith("public ")
        }
    if focus == "production-getdistances":
        return {"public getDistancesToModel": ITEM_METADATA["public getDistancesToModel"]}
    return ITEM_METADATA


def parse_bench_log(
    path: Path,
    expected_build: str,
    item_metadata: dict[str, dict[str, str]],
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
        elif match := BENCH_ITEM_RE.match(text):
            item = match.group("item")
            if item in item_metadata:
                values[item] = float(match.group("ms"))
    if context.get("build") != expected_build:
        raise ValueError(f"{path} build={context.get('build')!r}, expected {expected_build!r}")
    missing = sorted(set(item_metadata) - set(values))
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
    item_metadata: dict[str, dict[str, str]],
) -> tuple[dict[str, Any], dict[str, list[float]], dict[str, list[float]], str, str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    grouped_std: dict[str, list[float]] = {item: [] for item in item_metadata}
    grouped_rvv: dict[str, list[float]] = {item: [] for item in item_metadata}
    std_checksum = ""
    rvv_checksum = ""

    for run_dir in run_dirs:
        std_context, std_values = parse_bench_log(
            run_dir / "run_bench_std.log", "Std", item_metadata
        )
        rvv_context, rvv_values = parse_bench_log(
            run_dir / "run_bench_rvv.log", "RVV", item_metadata
        )
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
        for item in item_metadata:
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
    item_metadata: dict[str, dict[str, str]],
) -> dict[str, Any]:
    meta = item_metadata[item]
    speedup_values = [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    common_side = {
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": "direct_indexed_indices",
        "solve": False,
        "checksum_policy": "aggregate_entry_output_size_checksum",
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointXYZ_registered_single_float_xyz_aos_u32_offset",
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
        "run_count": len(speedup_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": speedup_values,
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
            "rvv_instr_count": count_rvv_instructions(asm_path, meta["asm_symbol"]),
            "rvv_ms": round(sum(rvv_values) / len(rvv_values), 6),
        },
    }


def build_manifest(
    repeat_root: Path,
    asm_path: Path,
    output_path: Path,
    summary_title: str,
    summary_role: str,
    focus: str,
) -> dict[str, Any]:
    item_metadata = metadata_for_focus(focus)
    context, std_values, rvv_values, std_checksum, rvv_checksum = parse_repeated_runs(
        repeat_root,
        item_metadata,
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
        "comparisons": [
            build_comparison(
                item,
                std_values[item],
                rvv_values[item],
                context,
                std_checksum,
                rvv_checksum,
                asm_path,
                item_metadata,
            )
            for item in item_metadata
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
        default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_stick_rvv.full.asm",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=TOPIC_ROOT / "doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json",
    )
    parser.add_argument(
        "--summary-title",
        default="sac_model_stick repeated board summary",
    )
    parser.add_argument(
        "--summary-role",
        default="production_shaped_diagnostic",
    )
    parser.add_argument(
        "--focus",
        choices=("all", "count", "select", "getdistances", "production", "production-getdistances"),
        default="all",
        help="Limit manifest comparisons to the current phase evidence scope.",
    )
    args = parser.parse_args()

    manifest = build_manifest(
        args.repeat_root,
        args.asm,
        args.output,
        args.summary_title,
        args.summary_role,
        args.focus,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
