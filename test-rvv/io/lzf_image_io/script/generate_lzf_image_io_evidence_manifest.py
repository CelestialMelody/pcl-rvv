#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for lzf_image_io board bench logs."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path

CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+),\s*"
    r"pixels:\s*(?P<pixels>[0-9]+)"
)
ITER_RE = re.compile(
    r"Iterations:\s*(?P<iterations>[0-9]+),\s*(?:warmup|Warmup):\s*(?P<warmup>[0-9]+)"
)
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
                    "pixels": int(total.group("pixels")),
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
    if name.startswith("depth"):
        return "depth16_plane_contiguous"
    if name.startswith("yuv422"):
        return "planar_yuv422_contiguous"
    return "rgb_buffer_contiguous"


def evidence_role_for(name: str) -> str:
    if "_production_" in name:
        return "production_detail"
    return "production_shaped_diagnostic"


def case_kind_for(name: str) -> str:
    if "_production_" in name:
        return "production_detail"
    return "production_shaped"


def boundary_for(name: str) -> str:
    if "_production_" in name:
        return "production_detail_helper"
    return "test_helper"


def point_type_for(name: str) -> str:
    if "_production_" in name:
        return "pcl::PointXYZRGB"
    return "PointXYZ/PointXYZRGB test POD"


def wrapper_for(name: str) -> str:
    if name.startswith("depth"):
        return "lzf_depth_post_decompress_helper"
    if name.startswith("yuv422"):
        if "_production_" in name:
            return "pcl::io::detail::convertPlanarYuv422ToPointCloud"
        return "lzf_yuv422_post_decompress_helper"
    return "lzf_rgb_buffer_to_cloud_helper"


def asm_boundary_for(name: str) -> str:
    if name.startswith("depth"):
        return "convertDepthToCloudRVV"
    if name.startswith("yuv422"):
        return "convertPlanarYuv422ToRgbRVV"
    return "copyRgbBufferToCloudRVV"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--repeat-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
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
        wrapper = wrapper_for(name)
        boundary = boundary_for(name)
        comparisons.append({
            "name": name,
            "group": "lzf_image_io",
            "evidence_role": evidence_role_for(name),
            "case_kind": case_kind_for(name),
            "point_type": point_type_for(name),
            "size": std_case["pixels"],
            "run_count": run_count,
            "baseline": {
                "label": "scalar-reference",
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "cloud_fnv1a",
                "checksum": std_case["checksum"],
                "timer_boundary": "conversion_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "depth_zero_to_nan_or_not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": "scalar_reference",
                "rvv_instr_count": 0,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(std_ms_values),
            },
            "candidate": {
                "label": "rvv-candidate",
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "cloud_fnv1a",
                "checksum": rvv_case["checksum"],
                "timer_boundary": "conversion_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "depth_zero_to_nan_or_not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": asm_boundary_for(name),
                "rvv_instr_count": None,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(rvv_ms_values),
            },
            "ba_values": ba_values,
        })

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "lzf_image_io board repeated summary",
            "evidence_role": "mixed" if len({comparison["evidence_role"] for comparison in comparisons}) > 1 else comparisons[0]["evidence_role"],
            "summary_path": str(args.summary_log),
            "label": "lzf_image_io board repeated phase000",
            "analysis_script": "test-rvv/io/lzf_image_io/script/generate_lzf_image_io_evidence_manifest.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "dataset": first_rvv["dataset"],
                "iterations": first_rvv["iterations"],
                "warmup_iterations": first_rvv["warmup_iterations"],
                "run_count": run_count,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see board / ELF",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {
            "strict_ab": True,
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
                "direction_epsilon_fraction": 0.02,
            },
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
