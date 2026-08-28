#!/usr/bin/env python3
"""Generate the sac_model_plane board Evidence Doctor manifest.

这个脚本只理解当前 topic 的 `bench_sac_model_plane` 输出格式。它把
Std/RVV board bench logs 转成 Evidence Doctor（证据体检）可读取的
JSON manifest，证据角色保持为 production-public（真实公开入口对比），
不把单次 board smoke 自动升级为 repeated board 稳定结论。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]

ITEM_METADATA: dict[str, dict[str, str]] = {
    "selectWithinDistance": {
        "name": "sac_model_plane selectWithinDistance PointXYZ 65536",
        "wrapper": "SampleConsensusModelPlaneBench::selectWithinDistance public entry",
        "timer_boundary": "selectWithinDistance_public_entry_only",
        "mask": "distance_less_than_threshold",
        "reduction": "ordered_inlier_and_error_distance_write",
        "asm_symbol": "selectWithinDistanceRVV",
    },
    "countWithinDistance": {
        "name": "sac_model_plane countWithinDistance PointXYZ 65536",
        "wrapper": "SampleConsensusModelPlaneBench::countWithinDistance public entry",
        "timer_boundary": "countWithinDistance_public_entry_only",
        "mask": "distance_less_than_threshold",
        "reduction": "inlier_count",
        "asm_symbol": "countWithinDistanceRVV",
    },
    "getDistancesToModel": {
        "name": "sac_model_plane getDistancesToModel PointXYZ 65536",
        "wrapper": "SampleConsensusModelPlaneBench::getDistancesToModel public entry",
        "timer_boundary": "getDistancesToModel_public_entry_only",
        "mask": "none_all_indices",
        "reduction": "dense_distance_store_per_index",
        "asm_symbol": "getDistancesToModelRVV",
    },
}

DATASET_RE = re.compile(
    r"^Dataset:\s+(?P<dataset>.+?)\s+\(points=(?P<size>\d+),\s*(?P<layout>.+)\)"
)
ITER_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s+(?P<warmup>\d+)")
BUILD_RE = re.compile(r"^Build:\s+(?P<build>Std|RVV)")
CHECKSUM_RE = re.compile(r"^Checksum:\s+(?P<checksum>\S+)")
BENCH_ITEM_RE = re.compile(r"^(?P<item>[A-Za-z0-9_]+)\s+:\s+(?P<ms>[0-9.]+)\s+ms/iter")
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


def parse_bench_log(path: Path, expected_build: str) -> tuple[dict[str, Any], dict[str, float]]:
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
            if item in ITEM_METADATA:
                values[item] = float(match.group("ms"))

    if context.get("build") != expected_build:
        raise ValueError(f"{path} build={context.get('build')!r}, expected {expected_build!r}")
    missing = sorted(set(ITEM_METADATA) - set(values))
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


def build_comparison(
    item: str,
    std_ms: float,
    rvv_ms: float,
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
) -> dict[str, Any]:
    meta = ITEM_METADATA[item]
    speedup = round(std_ms / rvv_ms, 4) if rvv_ms > 0.0 else 0.0
    rvv_instr_count = count_rvv_instructions(asm_path, meta["asm_symbol"])
    common_side = {
        "boundary": "public_overload",
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
        "case_kind": "production-public",
        "evidence_role": "production_direct",
        "build_comparison": "std_vs_rvv",
        "point_type": "PointXYZ",
        "size": context["size"],
        "run_count": 1,
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": [speedup],
        "baseline": {
            "label": "Std build public entry",
            **common_side,
            "checksum": std_checksum,
            "asm_boundary": "std_build_public_entry_no_rvv",
            "rvv_instr_count": 0,
            "std_ms": std_ms,
        },
        "candidate": {
            "label": "RVV build public entry",
            **common_side,
            "checksum": rvv_checksum,
            "asm_boundary": f"SampleConsensusModelPlane<PointXYZ>::{meta['asm_symbol']}",
            "rvv_instr_count": rvv_instr_count,
            "rvv_ms": rvv_ms,
        },
    }


def build_repeated_comparison(
    item: str,
    std_values: list[float],
    rvv_values: list[float],
    context: dict[str, Any],
    std_checksum: str,
    rvv_checksum: str,
    asm_path: Path,
) -> dict[str, Any]:
    meta = ITEM_METADATA[item]
    ba_values = [
        round(std_ms / rvv_ms, 4)
        for std_ms, rvv_ms in zip(std_values, rvv_values)
        if rvv_ms > 0.0
    ]
    rvv_instr_count = count_rvv_instructions(asm_path, meta["asm_symbol"])
    common_side = {
        "boundary": "public_overload",
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
        "case_kind": "production-public",
        "evidence_role": "production_direct",
        "build_comparison": "std_vs_rvv_repeated",
        "point_type": "PointXYZ",
        "size": context["size"],
        "run_count": len(ba_values),
        "iterations": context["iterations"],
        "warmup_iterations": context["warmup_iterations"],
        "ba_values": ba_values,
        "baseline": {
            "label": "Std build public entry",
            **common_side,
            "checksum": std_checksum,
            "asm_boundary": "std_build_public_entry_no_rvv",
            "rvv_instr_count": 0,
            "std_ms_values": std_values,
        },
        "candidate": {
            "label": "RVV build public entry",
            **common_side,
            "checksum": rvv_checksum,
            "asm_boundary": f"SampleConsensusModelPlane<PointXYZ>::{meta['asm_symbol']}",
            "rvv_instr_count": rvv_instr_count,
            "rvv_ms_values": rvv_values,
        },
    }


def parse_repeated_runs(repeat_root: Path) -> tuple[dict[str, Any], dict[str, list[float]], dict[str, list[float]], str, str]:
    run_dirs = [path for path in sorted(repeat_root.glob("run-*")) if path.is_dir()]
    if not run_dirs:
        raise ValueError(f"{repeat_root} has no run-* directories")

    base_context: dict[str, Any] | None = None
    grouped_std: dict[str, list[float]] = {item: [] for item in ITEM_METADATA}
    grouped_rvv: dict[str, list[float]] = {item: [] for item in ITEM_METADATA}
    std_checksum = ""
    rvv_checksum = ""

    for run_dir in run_dirs:
        log_dir = run_dir
        if not (log_dir / "run_bench_std.log").exists() and (run_dir / "board").is_dir():
            log_dir = run_dir / "board"
        std_context, std_values = parse_bench_log(log_dir / "run_bench_std.log", "Std")
        rvv_context, rvv_values = parse_bench_log(log_dir / "run_bench_rvv.log", "RVV")
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
        for item in ITEM_METADATA:
            grouped_std[item].append(std_values[item])
            grouped_rvv[item].append(rvv_values[item])

    assert base_context is not None
    return base_context, grouped_std, grouped_rvv, std_checksum, rvv_checksum


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
    if args.repeat_root:
        context, grouped_std, grouped_rvv, std_checksum, rvv_checksum = parse_repeated_runs(args.repeat_root)
        comparisons = [
            build_repeated_comparison(
                item,
                grouped_std[item],
                grouped_rvv[item],
                context,
                std_checksum,
                rvv_checksum,
                args.asm,
            )
            for item in ITEM_METADATA
        ]
        run_count = len(next(iter(grouped_std.values())))
        label = f"{run_count}-run board public overload Std/RVV"
        title = "sac_model_plane repeated board public Std/RVV"
    else:
        std_context, std_values = parse_bench_log(args.std_log, "Std")
        rvv_context, rvv_values = parse_bench_log(args.rvv_log, "RVV")

        for key in ("dataset", "size", "iterations", "warmup_iterations"):
            if std_context.get(key) != rvv_context.get(key):
                raise ValueError(f"context mismatch for {key}: {std_context.get(key)!r} vs {rvv_context.get(key)!r}")

        context = std_context
        comparisons = [
            build_comparison(
                item,
                std_values[item],
                rvv_values[item],
                std_context,
                str(std_context.get("checksum", "")),
                str(rvv_context.get("checksum", "")),
                args.asm,
            )
            for item in ITEM_METADATA
        ]
        run_count = 1
        label = "single-run board smoke public overload Std/RVV"
        title = "sac_model_plane board smoke public Std/RVV"

    return {
        "schema_version": 1,
        "summary": {
            "title": title,
            "evidence_role": "production_direct",
            "summary_path": repo_relative(args.summary),
            "label": label,
            "analysis_script": repo_relative(Path(__file__)),
            "metadata": {
                "device": args.device,
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": run_count,
                "vlen": args.vlen,
                "dataset": context.get("dataset"),
                "layout": context.get("layout"),
                "std_log": repo_relative(args.std_log),
                "rvv_log": repo_relative(args.rvv_log),
                "asm": repo_relative(args.asm),
                "repeat_root": repo_relative(args.repeat_root) if args.repeat_root else "",
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
        },
        "comparisons": comparisons,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--std-log", type=Path, default=TOPIC_ROOT / "log/board/run_bench_std.log")
    parser.add_argument("--rvv-log", type=Path, default=TOPIC_ROOT / "log/board/run_bench_rvv.log")
    parser.add_argument("--summary", type=Path, default=TOPIC_ROOT / "log/board/analyze_bench_compare.log")
    parser.add_argument("--asm", type=Path, default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_plane_rvv.full.asm")
    parser.add_argument("--repeat-root", type=Path)
    parser.add_argument("--output", type=Path, default=TOPIC_ROOT / "log/board/evidence_manifest.json")
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument("--vlen", default="board")
    args = parser.parse_args()

    manifest = build_manifest(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote manifest: {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
