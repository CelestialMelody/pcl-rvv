#!/usr/bin/env python3
"""Generate the normal-plane board Evidence Doctor manifest.

这个脚本只理解当前 topic 的 `bench_sac_normal_plane` 输出格式。它把
Std/RVV board bench logs 翻译成 Evidence Doctor（证据体检）可读取的
规范 manifest，不把 protected helper bench 自动升级为公开入口性能证据。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]

BASE_ITEM_METADATA: dict[str, dict[str, Any]] = {
    "selectWithinDistance": {
        "name": "normal-plane selectWithinDistance PointXYZ Normal 3283",
        "wrapper": "SampleConsensusModelNormalPlaneBench::benchSelectWithinDistance",
        "checksum_policy": "gtest_public_and_helper_equivalence",
        "timer_boundary": "selectWithinDistance_helper_loop",
        "mask": "weighted_distance_less_than_threshold",
        "reduction": "ordered_inlier_and_error_distance_write",
        "asm_symbol": "selectWithinDistanceRVV",
    },
    "countWithinDistance": {
        "name": "normal-plane countWithinDistance PointXYZ Normal 3283",
        "wrapper": "SampleConsensusModelNormalPlaneBench::benchCountWithinDistance",
        "checksum_policy": "gtest_helper_equivalence",
        "timer_boundary": "countWithinDistance_helper_loop",
        "mask": "weighted_distance_less_than_threshold",
        "reduction": "inlier_count",
        "asm_symbol": "countWithinDistanceRVV",
    },
    "getDistancesToModel": {
        "name": "normal-plane getDistancesToModel PointXYZ Normal 3283",
        "wrapper": "SampleConsensusModelNormalPlaneBench::benchGetDistancesToModel",
        "checksum_policy": "gtest_helper_equivalence",
        "timer_boundary": "getDistancesToModel_helper_loop",
        "mask": "none_all_indices",
        "reduction": "distance_store_per_index",
        "asm_symbol": "getDistancesToModelRVV",
    },
}
ITEM_METADATA: dict[str, dict[str, Any]] = dict(BASE_ITEM_METADATA)


def make_source_metadata(source_label: str) -> dict[str, dict[str, Any]]:
    point_type = f"{source_label} + Normal"
    gate = f"{source_label}_Normal_AoS_float_layout"
    return {
        f"selectWithinDistance_{source_label}_Normal": {
            "name": f"normal-plane selectWithinDistance {source_label} Normal 3283",
            "wrapper": f"SampleConsensusModelNormalPlaneBench<{source_label}, Normal>::benchSelectWithinDistance",
            "checksum_policy": "gtest_public_and_helper_equivalence",
            "timer_boundary": "selectWithinDistance_helper_loop",
            "mask": "weighted_distance_less_than_threshold",
            "reduction": "ordered_inlier_and_error_distance_write",
            "asm_symbol": "selectWithinDistanceRVV",
            "point_type": point_type,
            "gate": gate,
        },
        f"countWithinDistance_{source_label}_Normal": {
            "name": f"normal-plane countWithinDistance {source_label} Normal 3283",
            "wrapper": f"SampleConsensusModelNormalPlaneBench<{source_label}, Normal>::benchCountWithinDistance",
            "checksum_policy": "gtest_helper_equivalence",
            "timer_boundary": "countWithinDistance_helper_loop",
            "mask": "weighted_distance_less_than_threshold",
            "reduction": "inlier_count",
            "asm_symbol": "countWithinDistanceRVV",
            "point_type": point_type,
            "gate": gate,
        },
        f"getDistancesToModel_{source_label}_Normal": {
            "name": f"normal-plane getDistancesToModel {source_label} Normal 3283",
            "wrapper": f"SampleConsensusModelNormalPlaneBench<{source_label}, Normal>::benchGetDistancesToModel",
            "checksum_policy": "gtest_helper_equivalence",
            "timer_boundary": "getDistancesToModel_helper_loop",
            "mask": "none_all_indices",
            "reduction": "distance_store_per_index",
            "asm_symbol": "getDistancesToModelRVV",
            "point_type": point_type,
            "gate": gate,
        },
    }


def metadata_for_case_set(case_set: str) -> dict[str, dict[str, Any]]:
    if case_set == "existing":
        return dict(BASE_ITEM_METADATA)
    if case_set == "representative-aos":
        metadata: dict[str, dict[str, Any]] = {}
        for source_label in ("PointXYZ", "PointXYZI", "PointXYZINormal"):
            metadata.update(make_source_metadata(source_label))
        return metadata
    raise ValueError(f"unsupported case set: {case_set}")

BENCH_ITEM_RE = re.compile(r"^(?P<item>[A-Za-z0-9_]+)\s+:\s+(?P<ms>[0-9.]+)\s+ms/iter")
DATASET_RE = re.compile(r"^Dataset:\s+(?P<dataset>.+?)\s+\((?P<size>\d+)\s+points\)")
ITER_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
COMPARE_DEVICE_RE = re.compile(r"^\s*Device\s*:\s*(?P<device>.+?)\s*$")
COMPARE_DATASET_RE = re.compile(r"^\s*Dataset\s*:\s+(?P<dataset>.+?)\s+\((?P<size>\d+)\s+points\)")
COMPARE_ITER_RE = re.compile(r"^\s*Iterations\s*:\s+(?P<iterations>\d+)")
COMPARE_STD_ROW_RE = re.compile(
    r"^(?P<item>[A-Za-z0-9_]+)\s*\|\s*Std\s*\|\s*(?P<ms>[0-9.]+)\s+ms\s*\|"
)
COMPARE_RVV_ROW_RE = re.compile(r"^\s*\|\s*RVV\s*\|\s*(?P<ms>[0-9.]+)\s+ms\s*\|")
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


def parse_bench_log(path: Path) -> tuple[dict[str, Any], dict[str, float]]:
    if not path.exists():
        raise FileNotFoundError(path)
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    values: dict[str, float] = {}
    for line in text.splitlines():
        if match := DATASET_RE.match(line.strip()):
            context["dataset"] = match.group("dataset")
            context["size"] = int(match.group("size"))
        if match := ITER_RE.match(line.strip()):
            context["iterations"] = int(match.group("iterations"))
        if match := BENCH_ITEM_RE.match(line.strip()):
            item = match.group("item")
            if item in ITEM_METADATA:
                values[item] = float(match.group("ms"))
    missing = sorted(set(ITEM_METADATA) - set(values))
    if missing:
        raise ValueError(f"{path} missing bench item(s): {', '.join(missing)}")
    return context, values


def parse_compare_log(path: Path) -> tuple[dict[str, Any], dict[str, tuple[float, float]]]:
    if not path.exists():
        raise FileNotFoundError(path)
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    values: dict[str, tuple[float, float]] = {}
    current_item: str | None = None
    current_std: float | None = None
    for line in text.splitlines():
        if match := COMPARE_DEVICE_RE.match(line):
            context["device"] = match.group("device")
        if match := COMPARE_DATASET_RE.match(line):
            context["dataset"] = match.group("dataset")
            context["size"] = int(match.group("size"))
        if match := COMPARE_ITER_RE.match(line):
            context["iterations"] = int(match.group("iterations"))
        if match := COMPARE_STD_ROW_RE.match(line):
            item = match.group("item")
            if item in ITEM_METADATA:
                current_item = item
                current_std = float(match.group("ms"))
            continue
        if current_item and current_std is not None and (match := COMPARE_RVV_ROW_RE.match(line)):
            values[current_item] = (current_std, float(match.group("ms")))
            current_item = None
            current_std = None
    missing = sorted(set(ITEM_METADATA) - set(values))
    if missing:
        raise ValueError(f"{path} missing compare item(s): {', '.join(missing)}")
    return context, values


def parse_compare_logs(paths: list[Path]) -> tuple[dict[str, Any], dict[str, list[tuple[float, float]]]]:
    if not paths:
        raise ValueError("at least one --compare-log is required")
    contexts: list[dict[str, Any]] = []
    grouped: dict[str, list[tuple[float, float]]] = {item: [] for item in ITEM_METADATA}
    for path in paths:
        context, values = parse_compare_log(path)
        contexts.append(context)
        for item, pair in values.items():
            grouped[item].append(pair)
    context: dict[str, Any] = {}
    for key in ("device", "dataset", "size", "iterations"):
        values_for_key = [entry.get(key) for entry in contexts if entry.get(key) not in (None, "")]
        if values_for_key:
            context[key] = values_for_key[0]
    context["run_count"] = len(paths)
    return context, grouped


def count_rvv_instructions(asm_path: Path, symbol_hint: str) -> int:
    if not asm_path.exists():
        return 0
    in_symbol = False
    count = 0
    for raw_line in asm_path.read_text(encoding="utf-8", errors="replace").splitlines():
        header = SYMBOL_HEADER_RE.match(raw_line)
        if header:
            in_symbol = symbol_hint in header.group("symbol")
            continue
        if in_symbol and RVV_INSTRUCTION_RE.search(raw_line):
            count += 1
    return count


def build_comparison(
    item: str,
    std_ms: float,
    rvv_ms: float,
    size: int,
    asm_path: Path,
) -> dict[str, Any]:
    meta = ITEM_METADATA[item]
    speedup = round(std_ms / rvv_ms, 2) if rvv_ms > 0.0 else 0.0
    rvv_instr_count = count_rvv_instructions(asm_path, str(meta["asm_symbol"]))
    return {
        "name": meta["name"],
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "diagnostic",
        "build_comparison": "std_vs_rvv",
        "point_type": meta.get("point_type", "PointXYZ + Normal"),
        "size": size,
        "group": "existing_f32m2_normal_plane_helper",
        "run_count": 1,
        "ba_values": [speedup],
        "baseline": {
            "label": "Std build",
            "boundary": "protected_helper_hot_path",
            "wrapper": meta["wrapper"],
            "row_source": "ordered_indices_over_indices_",
            "solve": False,
            "checksum_policy": meta["checksum_policy"],
            "timer_boundary": meta["timer_boundary"],
            "gate": meta.get("gate", "PointXYZ_Normal_float_layout"),
            "mask": meta["mask"],
            "reduction": meta["reduction"],
            "asm_boundary": "std_non_rvv_path_not_attributed",
            "rvv_instr_count": 0,
            "std_ms": std_ms,
        },
        "candidate": {
            "label": "RVV build",
            "boundary": "protected_helper_hot_path",
            "wrapper": meta["wrapper"],
            "row_source": "ordered_indices_over_indices_",
            "solve": False,
            "checksum_policy": meta["checksum_policy"],
            "timer_boundary": meta["timer_boundary"],
            "gate": meta.get("gate", "PointXYZ_Normal_float_layout"),
            "mask": meta["mask"],
            "reduction": meta["reduction"],
            "asm_boundary": (
                "SampleConsensusModelNormalPlane<PointXYZ, Normal>::"
                f"{meta['asm_symbol']}"
            ),
            "rvv_instr_count": rvv_instr_count,
            "rvv_ms": rvv_ms,
        },
    }


def build_repeated_comparison(
    item: str,
    pairs: list[tuple[float, float]],
    size: int,
    asm_path: Path,
) -> dict[str, Any]:
    meta = ITEM_METADATA[item]
    ba_values = [round(std_ms / rvv_ms, 4) for std_ms, rvv_ms in pairs if rvv_ms > 0.0]
    rvv_instr_count = count_rvv_instructions(asm_path, str(meta["asm_symbol"]))
    std_values = [pair[0] for pair in pairs]
    rvv_values = [pair[1] for pair in pairs]
    return {
        "name": meta["name"],
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "diagnostic",
        "build_comparison": "std_vs_rvv_repeated",
        "point_type": meta.get("point_type", "PointXYZ + Normal"),
        "size": size,
        "group": "existing_f32m2_normal_plane_helper",
        "run_count": len(ba_values),
        "ba_values": ba_values,
        "baseline": {
            "label": "Std build",
            "boundary": "protected_helper_hot_path",
            "wrapper": meta["wrapper"],
            "row_source": "ordered_indices_over_indices_",
            "solve": False,
            "checksum_policy": meta["checksum_policy"],
            "timer_boundary": meta["timer_boundary"],
            "gate": meta.get("gate", "PointXYZ_Normal_float_layout"),
            "mask": meta["mask"],
            "reduction": meta["reduction"],
            "asm_boundary": "std_non_rvv_path_not_attributed",
            "rvv_instr_count": 0,
            "std_ms_values": std_values,
        },
        "candidate": {
            "label": "RVV build",
            "boundary": "protected_helper_hot_path",
            "wrapper": meta["wrapper"],
            "row_source": "ordered_indices_over_indices_",
            "solve": False,
            "checksum_policy": meta["checksum_policy"],
            "timer_boundary": meta["timer_boundary"],
            "gate": meta.get("gate", "PointXYZ_Normal_float_layout"),
            "mask": meta["mask"],
            "reduction": meta["reduction"],
            "asm_boundary": (
                "SampleConsensusModelNormalPlane<PointXYZ, Normal>::"
                f"{meta['asm_symbol']}"
            ),
            "rvv_instr_count": rvv_instr_count,
            "rvv_ms_values": rvv_values,
        },
    }


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
    if args.compare_log:
        compare_context, compare_values = parse_compare_logs(args.compare_log)
        size = int(compare_context.get("size") or 0)
        iterations = int(compare_context.get("iterations") or 0)
        run_count = int(compare_context.get("run_count") or len(args.compare_log))
        return {
            "schema_version": 1,
            "summary": {
                "title": "sample_consensus normal-plane repeated board manifest",
                "evidence_role": "diagnostic",
                "summary_path": repo_relative(args.summary),
                "label": "SampleConsensusModelNormalPlane protected-helper repeated board compare",
                "metadata": {
                    "device": args.device or compare_context.get("device", "Milkv-Jupiter"),
                    "iterations": iterations,
                    "warmup_iterations": 3,
                    "run_count": run_count,
                    "taskset": "not_recorded",
                    "governor": "not_recorded",
                    "freq": "not_recorded",
                    "temperature": "not_recorded",
                    "vlen": "see SoC / ELF (board)",
                    "binary_hash": "not_recorded",
                },
                "source_paths": {
                    "compare_logs": [repo_relative(path) for path in args.compare_log],
                    "summary": repo_relative(args.summary),
                    "asm_full": repo_relative(args.asm_full),
                    "asm_rvv": repo_relative(args.asm_rvv),
                },
                "source_boundary_note": (
                    "Repeated board benchmark calls the normal-plane protected helpers "
                    "through the same wrapper in each run. It strengthens hot-path "
                    "performance evidence but still does not replace public-entry "
                    "dispatch and fallback tests."
                ),
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
                    "min_repeated_runs": 5,
                    "ba_degrade_threshold": 1.0,
                    "high_degrade_fraction": 0.2,
                    "long_tail_ratio": 1.15,
                },
            },
            "comparisons": [
                build_repeated_comparison(item, compare_values[item], size, args.asm_full)
                for item in ITEM_METADATA
            ],
        }

    std_context, std_values = parse_bench_log(args.std_log)
    rvv_context, rvv_values = parse_bench_log(args.rvv_log)
    size = int(std_context.get("size") or rvv_context.get("size") or 0)
    iterations = int(std_context.get("iterations") or rvv_context.get("iterations") or 0)
    return {
        "schema_version": 1,
        "summary": {
            "title": "sample_consensus normal-plane public-boundary phase board manifest",
            "evidence_role": "diagnostic",
            "summary_path": repo_relative(args.summary),
            "label": "SampleConsensusModelNormalPlane protected-helper hot path board compare",
            "metadata": {
                "device": args.device,
                "iterations": iterations,
                "warmup_iterations": "not_recorded",
                "run_count": 1,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
            "source_paths": {
                "board_compare": repo_relative(args.summary),
                "board_std": repo_relative(args.std_log),
                "board_rvv": repo_relative(args.rvv_log),
                "board_test": repo_relative(args.board_test),
                "qemu_std": repo_relative(args.qemu_std),
                "qemu_rvv": repo_relative(args.qemu_rvv),
                "asm_full": repo_relative(args.asm_full),
                "asm_rvv": repo_relative(args.asm_rvv),
            },
            "source_boundary_note": (
                "Board benchmark calls the normal-plane protected helpers through a "
                "same-wrapper bench class, so it is hot-path performance evidence. "
                "Public-entry RVV dispatch and fallback are covered by QEMU/board "
                "unit tests in this phase."
            ),
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
                "min_repeated_runs": 1,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": [
            build_comparison(item, std_values[item], rvv_values[item], size, args.asm_full)
            for item in ITEM_METADATA
        ],
    }


def main() -> int:
    global ITEM_METADATA

    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path, default=TOPIC_ROOT / "log/board/run_bench_std.log")
    parser.add_argument("--rvv-log", type=Path, default=TOPIC_ROOT / "log/board/run_bench_rvv.log")
    parser.add_argument(
        "--compare-log",
        nargs="+",
        type=Path,
        default=[],
        help="Run-labelled analyze_bench_compare.log files for repeated board evidence.",
    )
    parser.add_argument("--summary", type=Path, default=TOPIC_ROOT / "log/board/analyze_bench_compare.log")
    parser.add_argument("--board-test", type=Path, default=TOPIC_ROOT / "log/board/run_test.log")
    parser.add_argument("--qemu-std", type=Path, default=TOPIC_ROOT / "log/qemu/run_test_std.log")
    parser.add_argument("--qemu-rvv", type=Path, default=TOPIC_ROOT / "log/qemu/run_test_rvv.log")
    parser.add_argument("--asm-full", type=Path, default=TOPIC_ROOT / "build/asm/riscv/bench_sac_normal_plane_rvv.full.asm")
    parser.add_argument("--asm-rvv", type=Path, default=TOPIC_ROOT / "build/asm/riscv/bench_sac_normal_plane_rvv.asm")
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument(
        "--case-set",
        choices=("existing", "representative-aos"),
        default="existing",
        help="Benchmark case labels expected in the compare logs.",
    )
    parser.add_argument("--output", type=Path, default=TOPIC_ROOT / "doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json")
    args = parser.parse_args()

    ITEM_METADATA = metadata_for_case_set(args.case_set)
    manifest = build_manifest(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {repo_relative(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
