#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for openni2_grabber board logs.

本脚本把 OpenNI2Grabber frame-to-cloud（帧到点云）topic 的 repeated
board compare logs（板卡重复对比日志）整理成 Evidence Doctor（证据体检）
可读取的 JSON manifest。默认 label 描述 Phase 000 production-shaped
diagnostic（生产形态诊断）；`prod_xyz_depth_full_640x480` 描述 Phase 020
production-detail（生产内部边界）证据。
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
    r"Total Time:\s*([0-9]+(?:\.[0-9]+)?)\s*ms,\s*checksum:\s*([0-9]+),\s*pixels:\s*([0-9]+)"
)
COMPARE_STD_RE = re.compile(
    r"^(?P<name>.+?)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)
COMPARE_RVV_RE = re.compile(
    r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)


DEFAULT_LABELS = [
    "xyz_depth_full_640x480",
    "prod_xyz_depth_full_640x480",
    "xyzrgba_depth_mismatch_320_to_640",
    "rgba_overlay_full_640x480",
    "xyzi_depth_ir_full_640x480",
]


def parse_log(path: Path) -> dict:
    text = path.read_text(encoding="utf-8", errors="replace")
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
        if index + 1 < len(lines):
            total_match = TOTAL_RE.search(lines[index + 1])
            if total_match:
                total_ms = float(total_match.group(1))
                checksum = total_match.group(2)
                pixels = int(total_match.group(3))
        rows[label] = {
            "avg_ms": avg_ms,
            "total_ms": total_ms,
            "checksum": checksum,
            "pixels": pixels,
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
    if label.startswith("prod_xyz_depth") or label.startswith("xyz_depth"):
        return "depth_xyz_contiguous"
    if label.startswith("xyzrgba_depth_mismatch"):
        return "depth_xyz_mismatch_stride"
    if label.startswith("rgba_overlay"):
        return "rgb_overlay"
    if label.startswith("xyzi_depth_ir"):
        return "ir_depth_intensity"
    return "openni2_grabber_unknown"


def point_type(label: str) -> str:
    if label.startswith("prod_xyz_depth") or label.startswith("xyz_depth"):
        return "PointXYZ"
    if label.startswith("xyzrgba"):
        return "PointXYZRGBA"
    if label.startswith("rgba_overlay"):
        return "PointXYZRGBA"
    if label.startswith("xyzi"):
        return "PointXYZI"
    return "unknown"


def row_source(label: str) -> str:
    if "mismatch" in label:
        return "synthetic_depth_to_wider_organized_cloud_integer_step"
    if label.startswith("rgba_overlay"):
        return "synthetic_rgb_contiguous_frame_pixels"
    if label.startswith("xyzi"):
        return "synthetic_depth_ir_contiguous_frame_pixels"
    if label.startswith("prod_xyz_depth"):
        return "synthetic_depth_contiguous_frame_pixels_production_detail"
    return "synthetic_depth_contiguous_frame_pixels"


def timer_boundary(label: str) -> str:
    if label.startswith("prod_xyz_depth"):
        return "production_detail_xyz_depth_projection"
    if label.startswith("rgba_overlay"):
        return "production_shaped_rgb_overlay_only"
    if "mismatch" in label:
        return "production_shaped_depth_projection_with_mismatch_initialization"
    if label.startswith("xyzi"):
        return "production_shaped_xyzi_depth_ir_conversion"
    return "production_shaped_xyz_depth_projection"


def gate(label: str) -> str:
    if label.startswith("prod_xyz_depth"):
        return "rvv_depth_projection_production_detail_pointxyz"
    if label == "xyzi_depth_ir_full_640x480":
        return "scalar_ir_candidate_no_rvv_intensity_path"
    if "mismatch" in label:
        return "rvv_depth_projection_integer_step_strided_store"
    if label.startswith("rgba_overlay"):
        return "rvv_rgb_pack_strided_rgba_store"
    return "rvv_depth_projection_contiguous_pointxyz"


def asm_boundary(label: str) -> str:
    if label == "xyzi_depth_ir_full_640x480":
        return "not_applicable_scalar_ir_candidate"
    if label.startswith("prod_xyz_depth"):
        return "openni2_grabber.cpp production detail helper via test hook"
    return "bench_openni2_grabber_rvv inline test helper"


def evidence_role(label: str) -> str:
    if label.startswith("prod_xyz_depth"):
        return "production_detail"
    return "production_shaped_diagnostic"


def case_kind(label: str) -> str:
    if label.startswith("prod_xyz_depth"):
        return "production detail"
    return "production-shaped diagnostic"


def metadata_for(label: str, side: dict, impl: str) -> dict:
    boundary = "production_detail_helper" if label.startswith("prod_xyz_depth") else "test_helper_production_shaped"
    wrapper = (
        "bench_openni2_grabber_production_hook"
        if label.startswith("prod_xyz_depth")
        else "bench_openni2_grabber"
    )
    return {
        "label": impl,
        "boundary": boundary,
        "wrapper": wrapper,
        "row_source": row_source(label),
        "solve": False,
        "checksum_policy": "fnv1a_float_or_rgba_bits",
        "checksum": side["checksum"],
        "timer_boundary": timer_boundary(label),
        "gate": gate(label),
        "mask": "invalid_zero_no_sample_shadow",
        "reduction": "not_applicable",
        "asm_boundary": asm_boundary(label),
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
    parser.add_argument("--compare-log", action="append", default=[], type=Path)
    parser.add_argument("--include-label", action="append", default=[])
    args = parser.parse_args()

    std = parse_log(args.std_log)
    rvv = parse_log(args.rvv_log)
    labels = set(args.include_label or DEFAULT_LABELS)
    summary_evidence_role = (
        "production_detail"
        if labels == {"prod_xyz_depth_full_640x480"}
        else "production_shaped_diagnostic"
    )
    summary_title = (
        "openni2_grabber board production-detail compare"
        if summary_evidence_role == "production_detail"
        else "openni2_grabber board production-shaped diagnostic compare"
    )
    repeated_values: dict[str, list[float]] = defaultdict(list)
    for compare_log in args.compare_log:
        for label, speedup in parse_compare_log(compare_log).items():
            if label in labels:
                repeated_values[label].append(speedup)

    comparisons = []
    for label in DEFAULT_LABELS:
        if label not in labels or label not in std["rows"] or label not in rvv["rows"]:
            continue
        std_row = std["rows"][label]
        rvv_row = rvv["rows"][label]
        speedups = repeated_values.get(label)
        if not speedups:
            speedups = [std_row["avg_ms"] / rvv_row["avg_ms"]] if rvv_row["avg_ms"] else [0.0]
        comparisons.append({
            "name": label,
            "group": case_group(label),
            "evidence_role": evidence_role(label),
            "case_kind": case_kind(label),
            "point_type": point_type(label),
            "size": f"pixels={std_row['pixels']}",
            "run_count": len(speedups),
            "baseline": metadata_for(label, std_row, "std-reference"),
            "candidate": metadata_for(label, rvv_row, "rvv-candidate"),
            "ba_values": speedups,
        })

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": summary_evidence_role,
            "summary_path": str(args.summary),
            "label": "openni2_grabber repeated board compare",
            "analysis_script": "test-rvv/io/openni2_grabber/script/generate_openni2_grabber_evidence_manifest.py",
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
