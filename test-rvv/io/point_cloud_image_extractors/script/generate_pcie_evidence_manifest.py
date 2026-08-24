#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for point_cloud_image_extractors.

本脚本只解析本 topic 的 board bench 日志。它把 Std/RVV 日志里的
case label、平均耗时、checksum 和输出规模转成 Evidence Doctor（证据体检）
manifest，避免直接把 loose Markdown summary 当成完整证据。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+),\s*"
    r"(?:(?:bytes)|(?:pixels)):\s*(?P<size>[0-9]+)"
)
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+),\s*Warmup:\s*(?P<warmup>[0-9]+)")
DATASET_RE = re.compile(r"Dataset:\s*(?P<dataset>.+)")


CASE_METADATA = {
    "rgb_unpack_pointxyzrgb_640x480": {
        "group": "rgb-u32-stride-unpack",
        "point_type": "PointXYZRGB",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractRgbRvv / inlined bench path",
        "candidate_label": "rgb_u32_stride_unpack_v0",
    },
    "rgb_unpack_pointxyzrgba_640x480": {
        "group": "rgb-u32-stride-unpack",
        "point_type": "PointXYZRGBA",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractRgbRvv / inlined bench path",
        "candidate_label": "rgb_u32_stride_unpack_v0",
    },
    "rgb_segment_store_pointxyzrgb_640x480": {
        "group": "rgb-segment-store",
        "point_type": "PointXYZRGB",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractRgbSegmentStoreRvv / inlined bench path",
        "candidate_label": "rgb_segment_store_v1",
    },
    "rgb_segment_store_pointxyzrgba_640x480": {
        "group": "rgb-segment-store",
        "point_type": "PointXYZRGBA",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractRgbSegmentStoreRvv / inlined bench path",
        "candidate_label": "rgb_segment_store_v1",
    },
    "scaling_full_range_intensity_640x480": {
        "group": "scaling-float-stride",
        "point_type": "PointXYZI",
        "checksum_policy": "mono16_u16_fnv1a",
        "asm_boundary": "extractScalingRvv / inlined bench path",
        "candidate_label": "scaling_float_stride_v0",
    },
    "scaling_full_range_reduction_intensity_640x480": {
        "group": "scaling-reduction",
        "point_type": "PointXYZI",
        "checksum_policy": "mono16_u16_fnv1a",
        "asm_boundary": "extractScalingFullRangeReductionRvv / inlined bench path",
        "candidate_label": "scaling_reduction_v1",
        "reduction": "full_range_minmax",
    },
    "scaling_fixed_factor_intensity_640x480": {
        "group": "scaling-float-stride",
        "point_type": "PointXYZI",
        "checksum_policy": "mono16_u16_fnv1a",
        "asm_boundary": "extractScalingRvv / inlined bench path",
        "candidate_label": "scaling_float_stride_v0",
    },
    "normal_field_pointnormal_640x480": {
        "group": "normal-field-float-stride",
        "point_type": "PointNormal",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractNormalRvv / inlined bench path",
        "candidate_label": "normal_float_stride_v0",
    },
    "label_mono16_pointxyzl_640x480": {
        "group": "label-mono16-stride",
        "point_type": "PointXYZL",
        "checksum_policy": "mono16_u16_fnv1a",
        "asm_boundary": "extractLabelMono16Rvv / inlined bench path",
        "candidate_label": "label_mono16_stride_v0",
    },
    "production_rgb_pointxyzrgb_640x480": {
        "group": "production-rgb-segment-store",
        "point_type": "PointXYZRGB",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractRgbFieldRVV / PointCloudImageExtractorFromRGBField public entry",
        "candidate_label": "production_rgb_segment_store_v1",
        "evidence_role": "production-public",
        "case_kind": "public overload",
        "boundary": "public overload",
        "wrapper": "PointCloudImageExtractorFromRGBField",
    },
    "production_rgb_pointxyzrgba_640x480": {
        "group": "production-rgb-segment-store",
        "point_type": "PointXYZRGBA",
        "checksum_policy": "rgb8_bytes_fnv1a",
        "asm_boundary": "extractRgbFieldRVV / PointCloudImageExtractorFromRGBField public entry",
        "candidate_label": "production_rgb_segment_store_v1",
        "evidence_role": "production-public",
        "case_kind": "public overload",
        "boundary": "public overload",
        "wrapper": "PointCloudImageExtractorFromRGBField",
    },
    "production_scaling_full_range_intensity_640x480": {
        "group": "production-scaling-reduction",
        "point_type": "PointXYZI",
        "checksum_policy": "mono16_u16_fnv1a",
        "asm_boundary": "extractScalingFullRangeIntensityRVV / PointCloudImageExtractorFromIntensityField public entry",
        "candidate_label": "production_scaling_reduction_v1",
        "evidence_role": "production-public",
        "case_kind": "public overload",
        "boundary": "public overload",
        "wrapper": "PointCloudImageExtractorFromIntensityField",
        "reduction": "full_range_minmax",
    },
    "production_label_mono16_pointxyzl_640x480": {
        "group": "production-label-mono16",
        "point_type": "PointXYZL",
        "checksum_policy": "mono16_u16_fnv1a",
        "asm_boundary": "extractLabelMono16FieldRVV / PointCloudImageExtractorFromLabelField public entry",
        "candidate_label": "production_label_mono16_stride_v0",
        "evidence_role": "production-public",
        "case_kind": "public overload",
        "boundary": "public overload",
        "wrapper": "PointCloudImageExtractorFromLabelField",
        "reduction": "not_applicable",
    },
}


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
                    "size": int(total.group("size")),
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


def collect_log_pairs(args: argparse.Namespace) -> list[tuple[dict, dict]]:
    if args.repeat_dir is None:
        if args.std_log is None or args.rvv_log is None:
            raise ValueError("--std-log and --rvv-log are required when --repeat-dir is not used")
        return [(parse_log(args.std_log), parse_log(args.rvv_log))]

    pairs: list[tuple[dict, dict]] = []
    for run_dir in sorted(args.repeat_dir.glob("run-*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            pairs.append((parse_log(std_log), parse_log(rvv_log)))
    if not pairs:
        raise ValueError(f"no run-*/run_bench_std.log + run_bench_rvv.log pairs found in {args.repeat_dir}")
    return pairs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--repeat-dir", type=Path)
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    log_pairs = collect_log_pairs(args)
    first_std, first_rvv = log_pairs[0]
    case_names = set(first_std["cases"]) & set(first_rvv["cases"])
    for std, rvv in log_pairs[1:]:
        case_names &= set(std["cases"]) & set(rvv["cases"])
    comparisons = []
    for name in sorted(case_names):
        metadata = CASE_METADATA.get(name, {})
        evidence_role = metadata.get("evidence_role", "production_shaped_diagnostic")
        boundary = metadata.get("boundary", "test_helper")
        wrapper = metadata.get("wrapper", "pcie_support")
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
            "group": metadata.get("group", "point_cloud_image_extractors"),
            "evidence_role": evidence_role,
            "case_kind": metadata.get("case_kind", "test_helper"),
            "point_type": metadata.get("point_type", "unknown"),
            "size": std_case["size"],
            "run_count": len(log_pairs),
            "baseline": {
                "label": "scalar-reference",
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": "organized_cloud_order",
                "solve": False,
                "checksum_policy": metadata.get("checksum_policy", "fnv1a"),
                "checksum": std_case["checksum"],
                "timer_boundary": "conversion_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": metadata.get("reduction", "not_applicable"),
                "asm_boundary": "scalar_reference",
                "rvv_instr_count": 0,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(std_ms_values),
            },
            "candidate": {
                "label": metadata.get("candidate_label", "rvv-candidate"),
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": "organized_cloud_order",
                "solve": False,
                "checksum_policy": metadata.get("checksum_policy", "fnv1a"),
                "checksum": rvv_case["checksum"],
                "timer_boundary": "conversion_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": metadata.get("reduction", "not_applicable"),
                "asm_boundary": metadata.get("asm_boundary", "bench_pcie_rvv"),
                "rvv_instr_count": None,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(rvv_ms_values),
            },
            "ba_values": ba_values,
        })

    summary_evidence_role = (
        "production-public"
        if comparisons and all(comp.get("evidence_role") == "production-public" for comp in comparisons)
        else "mixed"
        if comparisons and any(comp.get("evidence_role") == "production-public" for comp in comparisons)
        else "production_shaped_diagnostic"
    )

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "point_cloud_image_extractors board smoke",
            "evidence_role": summary_evidence_role,
            "summary_path": str(args.summary_log),
            "label": "point_cloud_image_extractors board smoke",
            "analysis_script": "test-rvv/io/point_cloud_image_extractors/script/generate_pcie_evidence_manifest.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "dataset": first_rvv["dataset"],
                "iterations": first_rvv["iterations"],
                "warmup_iterations": first_rvv["warmup_iterations"],
                "run_count": len(log_pairs),
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
