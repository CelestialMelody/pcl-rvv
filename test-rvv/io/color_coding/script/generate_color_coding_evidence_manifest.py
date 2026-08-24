#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for color_coding board bench logs."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path

CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+),\s*(?:warmup|Warmup):\s*(?P<warmup>[0-9]+)")
DATASET_RE = re.compile(r"Dataset:\s*(?P<dataset>.+)")
LEAF_RE = re.compile(r"leaf(?P<size>[0-9]+)")
COUNT_RE = re.compile(r"color_(?P<size>[0-9]+)")


def parse_log(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    cases: dict[str, dict] = {}
    iterations = None
    warmup = None
    dataset = None
    index = 0
    while index < len(lines):
        if iterations is None:
            match = ITER_RE.search(lines[index])
            if match:
                iterations = int(match.group("iterations"))
                warmup = int(match.group("warmup"))
        if dataset is None:
            match = DATASET_RE.search(lines[index])
            if match:
                dataset = match.group("dataset").strip()
        match = CASE_RE.match(lines[index])
        if match and index + 1 < len(lines):
            total = TOTAL_RE.search(lines[index + 1])
            if total:
                cases[match.group("name")] = {
                    "avg_ms": float(match.group("avg")),
                    "checksum": total.group("checksum"),
                }
                index += 1
        index += 1
    if iterations is None or warmup is None:
        raise ValueError(f"missing iterations/warmup in {path}")
    return {
        "path": str(path),
        "iterations": iterations,
        "warmup_iterations": warmup,
        "dataset": dataset or "synthetic_color_points",
        "cases": cases,
    }


def collect_log_pairs(repeat_dir: Path) -> list[tuple[dict, dict]]:
    pairs = []
    for run_dir in sorted(repeat_dir.glob("run*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            pairs.append((parse_log(std_log), parse_log(rvv_log)))
    if not pairs:
        raise ValueError(f"no run*/run_bench_std.log + run_bench_rvv.log pairs found in {repeat_dir}")
    return pairs


def row_source_for(name: str) -> str:
    local_name = name[3:] if name.startswith("ps_") else name
    local_name = local_name[5:] if local_name.startswith("prod_") else local_name
    if local_name.startswith("encode"):
        return "indexed_leaf_gather"
    return "contiguous_output_range"


def wrapper_for(name: str) -> str:
    production_shaped = name.startswith("ps_")
    production_direct = name.startswith("prod_")
    local_name = name[3:] if production_shaped else name
    local_name = local_name[5:] if production_direct else local_name
    if production_direct:
        if local_name.startswith("encode_average"):
            return "ColorCoding::encodeAverageOfPoints"
        if local_name.startswith("encode_points"):
            return "ColorCoding::encodePoints"
        return "ColorCoding::setDefaultColor"
    suffix = "PointVector" if production_shaped else ""
    if local_name.startswith("decode_points_staged"):
        return f"decodePointsCandidate{suffix}StagedStore"
    if local_name.startswith("encode_average"):
        return f"encodeAverageOfPointsCandidate{suffix}"
    if local_name.startswith("encode_points"):
        return f"encodePointsCandidate{suffix}"
    if local_name.startswith("decode_points"):
        return f"decodePointsCandidate{suffix}"
    return f"setDefaultColorCandidate{suffix}"


def asm_boundary_for(name: str) -> str:
    if name.startswith("prod_"):
        return "ColorCoding production RVV helper"
    local_name = name[3:] if name.startswith("ps_") else name
    if local_name.startswith("encode"):
        return "vluxei32/vredsum candidate helper"
    if local_name.startswith("decode_points_staged"):
        return "vlse8/vse32 staged scratch helper plus scalar AoS store"
    if local_name.startswith("decode"):
        return "vlse8/vsse32 candidate helper"
    return "vsse32 default-color helper"


def size_for(name: str) -> int:
    if match := LEAF_RE.search(name):
        return int(match.group("size"))
    if match := COUNT_RE.search(name):
        return int(match.group("size"))
    return 0


def evidence_role_for(name: str) -> str:
    if name.startswith("prod_"):
        return "production_public"
    if "decode_points_staged" in name:
        return "production_shaped_implementation_shape_diagnostic" if name.startswith("ps_") else "implementation_shape_diagnostic"
    if name.startswith("ps_"):
        return "production_shaped_diagnostic"
    return "component_ablation"


def case_kind_for(name: str) -> str:
    if name.startswith("prod_"):
        return "production_direct"
    if "decode_points_staged" in name:
        return "implementation_shape_diagnostic"
    if name.startswith("ps_"):
        return "production_shaped_diagnostic"
    return "component_diagnostic"


def point_type_for(name: str) -> str:
    if name.startswith("prod_"):
        return "pcl::PointXYZRGBA production public ColorCoding"
    if name.startswith("ps_"):
        return "pcl::PointXYZRGBA production-shaped vector"
    return "ColorPoint test POD with 32-bit RGBA field"


def boundary_for(name: str) -> str:
    if name.startswith("prod_"):
        return "public_overload"
    if name.startswith("ps_"):
        return "production_shaped_helper"
    return "test_helper"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--repeat-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    log_pairs = collect_log_pairs(args.repeat_dir)
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
        std_ms_values = [case["avg_ms"] for case in std_cases]
        rvv_ms_values = [case["avg_ms"] for case in rvv_cases]
        ba_values = [std_ms / rvv_ms for std_ms, rvv_ms in zip(std_ms_values, rvv_ms_values)]
        boundary = boundary_for(name)
        wrapper = wrapper_for(name)
        comparisons.append({
            "name": name,
            "group": "color_coding",
            "evidence_role": evidence_role_for(name),
            "case_kind": case_kind_for(name),
            "point_type": point_type_for(name),
            "size": size_for(name),
            "run_count": run_count,
            "baseline": {
                "label": "scalar-reference",
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "fnv1a_component_output",
                "checksum": next(iter(std_checksums)) if std_checksums else "missing",
                "timer_boundary": "component_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "byte_average_or_not_applicable",
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
                "checksum_policy": "fnv1a_component_output",
                "checksum": next(iter(rvv_checksums)) if rvv_checksums else "missing",
                "timer_boundary": "component_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "byte_average_or_not_applicable",
                "asm_boundary": asm_boundary_for(name),
                "rvv_instr_count": None,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(rvv_ms_values),
            },
            "ba_values": ba_values,
        })

    summary_role = (
        "production_public"
        if comparisons and all(item["name"].startswith("prod_") for item in comparisons)
        else "mixed_component_production_shaped_and_decode_shape_diagnostic"
    )
    summary_label = (
        "color_coding board repeated phase060 production"
        if summary_role == "production_public"
        else "color_coding board repeated phase050"
    )

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "color_coding board repeated summary",
            "evidence_role": summary_role,
            "summary_path": str(args.summary_log),
            "label": summary_label,
            "analysis_script": "test-rvv/io/color_coding/script/generate_color_coding_evidence_manifest.py",
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
