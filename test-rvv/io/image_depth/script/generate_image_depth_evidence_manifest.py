#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for image_depth board logs.

本脚本把 topic-local board bench 日志转换成 Evidence Doctor（证据体检）
可读取的 JSON manifest。非 `prod_*` case 是 production-shaped diagnostic
（生产形态诊断）；`prod_*` case 走真实 DepthImage public entry（公开入口），
作为 production-public（真实公开入口）证据。
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


ITER_RE = re.compile(r"Iterations:\s*(\d+),\s*Warmup:\s*(\d+)")
ROW_RE = re.compile(r"^([A-Za-z0-9_]+)\s*:\s*([0-9]+(?:\.[0-9]+)?)\s*ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*([0-9]+(?:\.[0-9]+)?)\s*ms,\s*checksum:\s*([0-9]+),\s*pixels:\s*([0-9]+),\s*line_step:\s*([0-9]+)"
)
COMPARE_STD_RE = re.compile(
    r"^(?P<name>.+?)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)
COMPARE_RVV_RE = re.compile(
    r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)


def parse_log(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    iter_match = ITER_RE.search(text)
    iterations = int(iter_match.group(1)) if iter_match else None
    warmup = int(iter_match.group(2)) if iter_match else None
    rows: dict[str, dict] = {}
    lines = text.splitlines()
    for index, line in enumerate(lines):
        row_match = ROW_RE.match(line)
        if not row_match:
            continue
        label = row_match.group(1)
        avg_ms = float(row_match.group(2))
        total_ms = None
        checksum = ""
        pixels = None
        line_step = None
        if index + 1 < len(lines):
            total_match = TOTAL_RE.search(lines[index + 1])
            if total_match:
                total_ms = float(total_match.group(1))
                checksum = total_match.group(2)
                pixels = int(total_match.group(3))
                line_step = int(total_match.group(4))
        rows[label] = {
            "avg_ms": avg_ms,
            "total_ms": total_ms,
            "checksum": checksum,
            "pixels": pixels,
            "line_step": line_step,
        }
    return {"iterations": iterations, "warmup_iterations": warmup, "rows": rows}


def parse_compare_log(path: Path) -> dict[str, float]:
    values: dict[str, float] = {}
    current_case: str | None = None
    current_std_avg: float | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
      if match := COMPARE_STD_RE.match(line):
          current_case = match.group("name").strip()
          current_std_avg = float(match.group("avg"))
          continue
      if current_case and (match := COMPARE_RVV_RE.match(line)):
          rvv_avg = float(match.group("avg"))
          if current_std_avg is not None and rvv_avg > 0.0:
              values[current_case] = current_std_avg / rvv_avg
          current_case = None
          current_std_avg = None
    return values


def case_group(label: str) -> str:
    if label.startswith("depth_") or label.startswith("prod_depth_"):
        return "depth-meters"
    if label.startswith("disparity_") or label.startswith("prod_disparity_"):
        return "disparity"
    return "unknown"


def timer_boundary(label: str) -> str:
    prefix = "production_public" if label.startswith("prod_") else "production_shaped"
    if "downsample" in label:
        return f"{prefix}_downsample_conversion"
    if "padded" in label:
        return f"{prefix}_contiguous_conversion_with_output_padding"
    return f"{prefix}_contiguous_conversion"


def gate(label: str) -> str:
    if label == "prod_depth_downsample_640x480_to_320x240":
        return "xStep_gt_1_depth_scalar_fallback_after_production_evidence_error"
    if "downsample" in label:
        return "xStep_gt_1_downsample_rvv" if label.startswith("prod_") else "xStep_gt_1_diagnostic_rvv"
    return "xStep_eq_1_contiguous_rvv"


def fallback_coverage_case(label: str) -> bool:
    # 当前 production patch 在深度下采样公开入口保留标量回退。这个 case
    # 仍进入 manifest，用于证明 PI5 后的回退覆盖边界，但不参与 RVV
    # speedup 异常频率判断。
    return label == "prod_depth_downsample_640x480_to_320x240"


def metadata_for(label: str, side: dict, impl: str) -> dict:
    production_public = label.startswith("prod_")
    return {
        "label": impl,
        "boundary": "DepthImage_public_entry" if production_public else "test_helper_production_shaped",
        "wrapper": "bench_image_depth_production_public" if production_public else "bench_image_depth",
        "row_source": "wrapper_backed_depth_image_buffer",
        "solve": False,
        "checksum_policy": "fnv1a_float_bits_including_padding",
        "checksum": side["checksum"],
        "timer_boundary": timer_boundary(label),
        "gate": gate(label),
        "mask": "invalid_zero_no_sample_shadow",
        "reduction": "not_applicable",
        "asm_boundary": "io/src/image_depth.cpp production helper or public entry inline boundary"
        if production_public else "bench_image_depth_rvv inline candidate helper",
        "std_ms": side["avg_ms"] if impl == "std-reference" else None,
        "rvv_ms": side["avg_ms"] if impl == "rvv-candidate" else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", required=True, type=Path)
    parser.add_argument("--rvv-log", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument(
        "--include-label",
        action="append",
        default=[],
        help="Only include this benchmark label. Repeat for multiple labels.",
    )
    parser.add_argument(
        "--compare-log",
        action="append",
        default=[],
        type=Path,
        help="Additional analyze_bench_compare.log files used as repeated B/A values.",
    )
    args = parser.parse_args()

    std = parse_log(args.std_log)
    rvv = parse_log(args.rvv_log)
    include_labels = set(args.include_label or [
        "depth_full_640x480",
        "depth_full_padded_640x480",
        "disparity_full_640x480",
    ])
    repeated_values: dict[str, list[float]] = defaultdict(list)
    for compare_log in args.compare_log:
        for label, speedup in parse_compare_log(compare_log).items():
            if label in include_labels:
                repeated_values[label].append(speedup)

    comparisons = []
    for label in sorted(std["rows"]):
        if label not in include_labels:
            continue
        if label not in rvv["rows"]:
            continue
        std_row = std["rows"][label]
        rvv_row = rvv["rows"][label]
        speedups = repeated_values.get(label)
        if not speedups:
            speedups = [std_row["avg_ms"] / rvv_row["avg_ms"]] if rvv_row["avg_ms"] else [0.0]
        fallback_coverage = fallback_coverage_case(label)
        evidence_role = "production-fallback-coverage" if fallback_coverage else (
            "production-public" if label.startswith("prod_") else "production_shaped_diagnostic")
        comparison = {
            "name": label,
            "group": case_group(label),
            "evidence_role": evidence_role,
            "case_kind": "production fallback coverage" if fallback_coverage else (
                "production public" if label.startswith("prod_") else "production-shaped diagnostic"),
            "point_type": "not_applicable_image_pixels",
            "size": f"pixels={std_row['pixels']}, line_step={std_row['line_step']}",
            "run_count": len(speedups),
            "baseline": metadata_for(label, std_row, "std-reference"),
            "candidate": metadata_for(label, rvv_row, "rvv-candidate"),
        }
        if fallback_coverage:
            comparison["notes"] = (
                "Depth downsample remains on the scalar fallback path after the production "
                "Evidence Doctor error; summary speedup is reported for context only.")
            comparison["context_speedup_values"] = speedups
        else:
            comparison["ba_values"] = speedups
        comparisons.append(comparison)

    comparison_roles = {item["evidence_role"] for item in comparisons}
    manifest_role = "production-public" if comparisons and comparison_roles <= {
        "production-public", "production-fallback-coverage"
    } else "production_shaped_diagnostic"
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "image_depth board production public compare"
            if manifest_role == "production-public" else "image_depth board production-shaped diagnostic compare",
            "evidence_role": manifest_role,
            "summary_path": str(args.summary),
            "label": "repeated board compare with per-case iterations",
            "analysis_script": "test-rvv/io/image_depth/script/generate_image_depth_evidence_manifest.py",
            "metadata": {
                "device": args.device,
                "iterations": std["iterations"],
                "warmup_iterations": std["warmup_iterations"],
                "run_count": max((len(values) for values in repeated_values.values()), default=1),
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
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
