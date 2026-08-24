#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for pcd_io_templated_writer logs."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path

CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+),\s*"
    r"points:\s*(?P<points>[0-9]+),\s*point_step:\s*(?P<point_step>[0-9]+)"
)
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+)")
WARMUP_RE = re.compile(r"Warmup Iterations:\s*(?P<warmup>[0-9]+)")
DATASET_RE = re.compile(r"Dataset:\s*(?P<dataset>.+)")


def parse_log(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8").splitlines()
    cases: dict[str, dict] = {}
    iterations = None
    warmup = None
    dataset = None
    index = 0
    while index < len(lines):
        line = lines[index]
        if iterations is None:
            match = ITER_RE.search(line)
            if match:
                iterations = int(match.group("iterations"))
        if warmup is None:
            match = WARMUP_RE.search(line)
            if match:
                warmup = int(match.group("warmup"))
        if dataset is None:
            match = DATASET_RE.search(line)
            if match:
                dataset = match.group("dataset").strip()
        match = CASE_RE.match(line)
        if match and index + 1 < len(lines):
            total = TOTAL_RE.search(lines[index + 1])
            if total:
                cases[match.group("name")] = {
                    "avg_ms": float(match.group("avg")),
                    "total_ms": float(total.group("total")),
                    "checksum": total.group("checksum"),
                    "points": int(total.group("points")),
                    "point_step": int(total.group("point_step")),
                }
                index += 1
        index += 1
    if iterations is None or warmup is None:
        raise ValueError(f"missing iterations/warmup in {path}")
    return {
        "path": str(path),
        "iterations": iterations,
        "warmup_iterations": warmup,
        "dataset": dataset or "unknown",
        "cases": cases,
    }


def parse_log_pair(std_log: Path, rvv_log: Path) -> tuple[dict, dict]:
    return parse_log(std_log), parse_log(rvv_log)


def collect_log_pairs(args: argparse.Namespace) -> list[tuple[dict, dict]]:
    if args.repeat_dir is None:
        if args.std_log is None or args.rvv_log is None:
            raise ValueError("--std-log and --rvv-log are required when --repeat-dir is not used")
        return [parse_log_pair(args.std_log, args.rvv_log)]

    pairs: list[tuple[dict, dict]] = []
    for run_dir in sorted(args.repeat_dir.glob("run*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            pairs.append(parse_log_pair(std_log, rvv_log))
    if not pairs:
        raise ValueError(f"no run*/run_bench_std.log + run_bench_rvv.log pairs found in {args.repeat_dir}")
    return pairs


def row_source_for(name: str) -> str:
    if "padding" in name:
        return "contiguous_pointcloud_rows_with_tail_padding"
    if "small" in name:
        return "contiguous_pointcloud_rows_small"
    return "contiguous_pointcloud_rows"


def case_kind_for(name: str) -> str:
    if name.startswith("production_compressed_"):
        return "production_public_compressed_writer"
    if name.startswith("production_binary_tuple_"):
        return "production_public_binary_tuple_writer"
    if name.startswith("production_binary_"):
        return "production_public_binary_writer"
    if name.startswith("binary_tuple_"):
        return "binary_tuple_segment_diagnostic"
    if name.startswith("binary_"):
        return "binary_writer_component_pack"
    if name.startswith("compressed_"):
        return "production_shaped_pack_lzf"
    return "test_helper_component"


def wrapper_for(name: str) -> str:
    if name.startswith("production_compressed_"):
        return "PCDWriter::writeBinaryCompressed<PointT>"
    if name.startswith("production_binary_tuple_"):
        return "PCDWriter::writeBinary<PointT>"
    if name.startswith("production_binary_"):
        return "PCDWriter::writeBinary<PointT>"
    if name.startswith("binary_tuple_"):
        return "pcdtw_binary_tuple_segment_pack"
    if name.startswith("binary_"):
        return "pcdtw_binary_component_pack"
    if name.startswith("compressed_"):
        return "pcdtw_pack_and_lzf_compress"
    return "pcdtw_component_pack"


def timer_boundary_for(name: str) -> str:
    if name.startswith("production_compressed_"):
        return "public_writer_header_pack_lzf_mmap_write_checksum_after_timing"
    if name.startswith("production_binary_tuple_"):
        return "public_writer_header_tuple_pack_mmap_write_checksum_after_timing"
    if name.startswith("production_binary_"):
        return "public_writer_header_pack_mmap_write_checksum_after_timing"
    if name.startswith("binary_tuple_"):
        return "binary_tuple_segment_pack_only_checksum_after_timing"
    if name.startswith("binary_"):
        return "binary_pack_only_checksum_after_timing"
    if name.startswith("compressed_"):
        return "pack_and_lzf_compress_checksum_after_timing"
    return "pack_only_checksum_after_timing"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--repeat-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--evidence-role", default="component_ablation")
    parser.add_argument("--label", default="pcd_io_templated_writer board repeated")
    args = parser.parse_args()

    log_pairs = collect_log_pairs(args)
    first_std, first_rvv = log_pairs[0]
    run_count = len(log_pairs)
    case_names = set(first_std["cases"]) & set(first_rvv["cases"])
    for std, rvv in log_pairs[1:]:
        case_names &= set(std["cases"]) & set(rvv["cases"])

    comparisons = []
    for name in sorted(case_names):
        std_cases = [std["cases"][name] for std, _ in log_pairs]
        rvv_cases = [rvv["cases"][name] for _, rvv in log_pairs]
        std_checksums = {case["checksum"] for case in std_cases}
        rvv_checksums = {case["checksum"] for case in rvv_cases}
        if len(std_checksums) != 1 or len(rvv_checksums) != 1:
            raise ValueError(f"unstable checksum across repeated logs for {name}")
        std_ms_values = [case["avg_ms"] for case in std_cases]
        rvv_ms_values = [case["avg_ms"] for case in rvv_cases]
        ba_values = [std_ms / rvv_ms for std_ms, rvv_ms in zip(std_ms_values, rvv_ms_values)]
        std_case = std_cases[-1]
        rvv_case = rvv_cases[-1]
        comparisons.append({
            "name": name,
            "group": "pcd_io_templated_writer",
            "evidence_role": args.evidence_role,
            "case_kind": case_kind_for(name),
            "point_type": "synthetic PointXYZRGB-like byte layout",
            "size": std_case["points"],
            "run_count": run_count,
            "baseline": {
                "label": "scalar-reference",
                "boundary": (
                    "public_overload"
                    if name.startswith(("production_compressed_", "production_binary_"))
                    else "test_helper"
                ),
                "wrapper": wrapper_for(name),
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "packed_bytes_fnv1a",
                "checksum": std_case["checksum"],
                "timer_boundary": timer_boundary_for(name),
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": "scalar_reference",
                "rvv_instr_count": 0,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(std_ms_values),
            },
            "candidate": {
                "label": "rvv-candidate",
                "boundary": (
                    "public_overload"
                    if name.startswith(("production_compressed_", "production_binary_"))
                    else "test_helper"
                ),
                "wrapper": wrapper_for(name),
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "packed_bytes_fnv1a",
                "checksum": rvv_case["checksum"],
                "timer_boundary": timer_boundary_for(name),
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": (
                    "PCDWriter production public overload in bench_pcdtw"
                    if name.startswith(("production_compressed_", "production_binary_"))
                    else (
                        "pcdtw::packBinaryFieldsTupleCandidate inlined in bench_pcdtw"
                        if name.startswith("binary_tuple_")
                        else "pcdtw::packFieldsCandidate inlined in bench_pcdtw"
                    )
                ),
                "rvv_instr_count": None,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(rvv_ms_values),
            },
            "ba_values": ba_values,
        })

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "pcd_io_templated_writer board repeated summary",
            "evidence_role": args.evidence_role,
            "summary_path": str(args.summary_log),
            "label": args.label,
            "analysis_script": "test-rvv/io/pcd_io_templated_writer/script/generate_pcdtw_evidence_manifest.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": first_std["iterations"],
                "warmup_iterations": first_std["warmup_iterations"],
                "run_count": run_count,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "not_recorded",
                "binary_hash": "not_recorded",
                "dataset": first_std["dataset"],
            },
        },
        "checks": {
            "strict_ab": False,
        },
        "comparisons": comparisons,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
