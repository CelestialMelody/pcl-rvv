#!/usr/bin/env python3
"""Generate a SHOT topic Evidence Doctor manifest from board compare logs.

本脚本只理解 test-rvv/features/shot 的 bench label（性能用例标签）和日志格式。
它把 board compare Markdown、Std log 和 RVV log 翻译成通用 evidence_manifest.json，
供 test-rvv/script/evidence_doctor.py 做数据契约检查。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


CASE_METADATA = {
    "public_shot352_fixed_lrf": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "point_type": "PointXYZ_Normal_SHOT352",
        "wrapper": "SHOTEstimation_computeFeature_fixed_lrf",
        "group": "shot352-fixed-lrf",
        "boundary": "public_overload_shape",
        "row_source": "radius_search_neighbors",
        "timer_boundary": "compute_only_after_synthetic_setup",
        "gate": "provided_lrf_radius_search",
        "mask": "production_finite_checks",
        "reduction": "descriptor_l2_normalization",
        "checksum_policy": "descriptor_weighted_sum",
    },
    "public_shot1344_fixed_lrf": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "point_type": "PointXYZRGBA_Normal_SHOT1344",
        "wrapper": "SHOTColorEstimation_computeFeature_fixed_lrf",
        "group": "shot1344-fixed-lrf",
        "boundary": "public_overload_shape",
        "row_source": "radius_search_neighbors",
        "timer_boundary": "compute_only_after_synthetic_setup",
        "gate": "provided_lrf_radius_search",
        "mask": "production_finite_checks",
        "reduction": "descriptor_l2_normalization",
        "checksum_policy": "descriptor_weighted_sum",
    },
    "normalize_352_component": {
        "case_kind": "component",
        "point_type": "float_descriptor_SHOT352",
        "wrapper": "normalizeDescriptorRVV_test_helper",
        "group": "normalize-component",
        "boundary": "test_helper",
        "row_source": "contiguous_descriptor_array",
        "timer_boundary": "normalize_batch_only_after_synthetic_setup",
        "gate": "finite_nonzero_descriptor",
        "mask": "none",
        "reduction": "descriptor_l2_normalization_component",
        "checksum_policy": "descriptor_batch_weighted_sum",
    },
    "normalize_1344_component": {
        "case_kind": "component",
        "point_type": "float_descriptor_SHOT1344",
        "wrapper": "normalizeDescriptorRVV_test_helper",
        "group": "normalize-component",
        "boundary": "test_helper",
        "row_source": "contiguous_descriptor_array",
        "timer_boundary": "normalize_batch_only_after_synthetic_setup",
        "gate": "finite_nonzero_descriptor",
        "mask": "none",
        "reduction": "descriptor_l2_normalization_component",
        "checksum_policy": "descriptor_batch_weighted_sum",
    },
    "shape_bin_component": {
        "case_kind": "component",
        "point_type": "float_normal_soa_to_double_bins",
        "wrapper": "computeShapeBinDistanceRVV_test_helper",
        "group": "shape-bin-component",
        "boundary": "test_helper",
        "row_source": "soa_neighbor_normal_batch",
        "timer_boundary": "shape_bin_batch_only_after_synthetic_setup",
        "gate": "finite_normal_and_clamp",
        "mask": "finite_normal_mask",
        "reduction": "none",
        "checksum_policy": "shape_bin_weighted_sum",
    },
    "shape_bin_aos_component": {
        "case_kind": "component",
        "point_type": "pcl_Normal_aos_to_double_bins",
        "wrapper": "computeShapeBinDistanceAoSRVV_test_helper",
        "group": "shape-bin-aos-component",
        "boundary": "test_helper",
        "row_source": "contiguous_aos_normal_batch",
        "timer_boundary": "shape_bin_aos_batch_only_after_synthetic_setup",
        "gate": "finite_normal_and_clamp",
        "mask": "finite_normal_mask",
        "reduction": "none",
        "checksum_policy": "shape_bin_weighted_sum",
    },
    "shape_bin_indexed_component": {
        "case_kind": "component",
        "point_type": "pcl_Normal_aos_indices_to_double_bins",
        "wrapper": "computeShapeBinDistanceIndexedRVV_test_helper",
        "group": "shape-bin-indexed-component",
        "boundary": "test_helper",
        "row_source": "indexed_aos_normal_batch",
        "timer_boundary": "shape_bin_indexed_batch_only_after_synthetic_setup",
        "gate": "valid_indices_finite_normal_and_clamp",
        "mask": "finite_normal_mask",
        "reduction": "nan_count_vcpop",
        "checksum_policy": "shape_bin_weighted_sum_plus_nan_count",
    },
    "production_shape_bin_direct": {
        "evidence_role": "production-detail",
        "case_kind": "production_detail",
        "point_type": "PointXYZ_Normal_SHOT352",
        "wrapper": "SHOTEstimationBase_createBinDistanceShape",
        "group": "shape-bin-production-detail",
        "boundary": "production_detail_helper",
        "row_source": "indexed_aos_normal_batch",
        "timer_boundary": "createBinDistanceShape_only_after_synthetic_setup",
        "gate": "normal_traits_aos_u32_offset_small_neighborhood_fallback",
        "mask": "finite_normal_mask",
        "reduction": "nan_count_vcpop",
        "checksum_policy": "shape_bin_weighted_sum",
    },
    "interpolation_geometry_component": {
        "case_kind": "component",
        "point_type": "pcl_PointXYZ_aos_indices_to_geometry_staging",
        "wrapper": "computeInterpolationGeometryIndexedRVV_test_helper",
        "group": "interpolation-geometry-component",
        "boundary": "test_helper",
        "row_source": "indexed_aos_surface_point_batch",
        "timer_boundary": "interpolation_geometry_staging_only_after_synthetic_setup",
        "gate": "valid_indices_finite_bin_distance_and_nonzero_distance",
        "mask": "finite_bin_distance_and_nonzero_distance_mask",
        "reduction": "none",
        "checksum_policy": "projection_distance_valid_weighted_sum",
    },
    "interpolation_bin_selection_component": {
        "case_kind": "component",
        "point_type": "double_interpolation_geometry_to_scalar_tail_staging",
        "wrapper": "computeInterpolationBinSelectionRVV_test_helper",
        "group": "interpolation-bin-selection-component",
        "boundary": "test_helper",
        "row_source": "post_geometry_interpolation_values",
        "timer_boundary": "bin_selection_staging_only_after_synthetic_setup",
        "gate": "finite_bin_distance_and_nonzero_distance",
        "mask": "finite_bin_distance_and_nonzero_distance_mask",
        "reduction": "none",
        "checksum_policy": "desc_step_adjacent_weight_valid_sum",
    },
    "color_lab_distance_component": {
        "case_kind": "component",
        "point_type": "normalized_lab_arrays_to_color_bins",
        "wrapper": "computeColorBinDistanceRVV_test_helper",
        "group": "color-lab-distance-component",
        "boundary": "test_helper",
        "row_source": "contiguous_normalized_lab_arrays",
        "timer_boundary": "color_lab_distance_batch_only_after_synthetic_setup",
        "gate": "pre_normalized_lab_values_and_clamp",
        "mask": "none",
        "reduction": "none",
        "checksum_policy": "color_bin_weighted_sum",
    },
    "color_rgb_lut_indexed_component": {
        "case_kind": "component",
        "point_type": "pcl_PointXYZRGBA_indices_to_color_bins",
        "wrapper": "computeColorBinDistanceIndexedRGBRVV_test_helper",
        "group": "color-rgb-lut-indexed-component",
        "boundary": "test_helper",
        "row_source": "indexed_aos_color_point_batch_with_scalar_lab_staging",
        "timer_boundary": "rgb_lut_lab_staging_plus_color_distance_after_synthetic_setup",
        "gate": "valid_indices_rgb_to_lab_and_color_clamp",
        "mask": "none",
        "reduction": "none",
        "checksum_policy": "color_bin_weighted_sum",
    },
}

TIME_RE = re.compile(r"^(?P<case>[A-Za-z0-9_]+):\s*(?P<ms>[0-9.]+)\s*ms\s*/\s*iter")
CHECKSUM_RE = re.compile(r"^(?P<case>[A-Za-z0-9_]+)\s+checksum:\s*(?P<checksum>[-+0-9.eE]+)")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<dataset>.+)$", re.MULTILINE)
ITER_RE = re.compile(r"^Iterations:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<warmup>\d+)", re.MULTILINE)
DEVICE_RE = re.compile(r"^\s*Device\s*:\s*(?P<device>.+)$", re.MULTILINE)
VLEN_RE = re.compile(r"^\s*VLEN\s*:\s*(?P<vlen>.+)$", re.MULTILINE)


def parse_log(path: Path) -> dict[str, dict[str, float | str]]:
    cases: dict[str, dict[str, float | str]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if match := TIME_RE.match(line.strip()):
            cases.setdefault(match.group("case"), {})["ms"] = float(match.group("ms"))
        if match := CHECKSUM_RE.match(line.strip()):
            cases.setdefault(match.group("case"), {})["checksum"] = match.group("checksum")
    return cases


def first_match(pattern: re.Pattern[str], text: str, group: str, default: str) -> str:
    match = pattern.search(text)
    return match.group(group).strip() if match else default


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--std-log", required=True, type=Path)
    parser.add_argument("--rvv-log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    summary_text = args.summary.read_text(encoding="utf-8")
    std_text = args.std_log.read_text(encoding="utf-8")
    rvv_text = args.rvv_log.read_text(encoding="utf-8")
    std_cases = parse_log(args.std_log)
    rvv_cases = parse_log(args.rvv_log)

    comparisons = []
    for case_name, metadata in CASE_METADATA.items():
        if case_name not in std_cases or case_name not in rvv_cases:
            continue
        std_ms = float(std_cases[case_name]["ms"])
        rvv_ms = float(rvv_cases[case_name]["ms"])
        std_checksum = str(std_cases[case_name].get("checksum", "missing"))
        rvv_checksum = str(rvv_cases[case_name].get("checksum", "missing"))
        comparisons.append(
            {
                "name": case_name,
                "group": metadata["group"],
                "evidence_role": metadata.get("evidence_role", "diagnostic"),
                "case_kind": metadata["case_kind"],
                "point_type": metadata["point_type"],
                "size": first_match(DATASET_RE, std_text, "dataset", "unknown"),
                "run_count": 1,
                "baseline": {
                    "label": "std",
                    "boundary": metadata["boundary"],
                    "wrapper": metadata["wrapper"],
                    "row_source": metadata["row_source"],
                    "solve": False,
                    "checksum_policy": metadata["checksum_policy"],
                    "checksum": std_checksum,
                    "timer_boundary": metadata["timer_boundary"],
                    "gate": metadata["gate"],
                    "mask": metadata["mask"],
                    "reduction": metadata["reduction"],
                    "asm_boundary": "not_checked_in_manifest",
                    "rvv_instr_count": 0,
                    "std_ms": std_ms,
                    "rvv_ms": std_ms,
                },
                "candidate": {
                    "label": "rvv_build",
                    "boundary": metadata["boundary"],
                    "wrapper": metadata["wrapper"],
                    "row_source": metadata["row_source"],
                    "solve": False,
                    "checksum_policy": metadata["checksum_policy"],
                    "checksum": rvv_checksum,
                    "timer_boundary": metadata["timer_boundary"],
                    "gate": metadata["gate"],
                    "mask": metadata["mask"],
                    "reduction": metadata["reduction"],
                    "asm_boundary": "test-rvv/features/shot/build/asm/riscv/bench_shot_rvv.asm",
                    "rvv_instr_count": None,
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                },
                "ba_values": [std_ms / rvv_ms if rvv_ms else 0.0],
            }
        )

    comparison_roles = {str(comp["evidence_role"]) for comp in comparisons}
    summary_role = (
        next(iter(comparison_roles))
        if len(comparison_roles) == 1
        else ("mixed-production" if comparison_roles else "diagnostic")
    )

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "SHOT board evidence manifest",
            "evidence_role": summary_role,
            "summary_path": str(args.summary),
            "label": "board smoke diagnostic",
            "analysis_script": "test-rvv/features/shot/script/generate_shot_evidence_manifest.py",
            "metadata": {
                "device": first_match(DEVICE_RE, summary_text, "device", "unknown"),
                "iterations": int(first_match(ITER_RE, std_text, "iterations", "0")),
                "warmup_iterations": int(first_match(WARMUP_RE, std_text, "warmup", "0")),
                "run_count": 1,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": first_match(VLEN_RE, summary_text, "vlen", "unknown"),
                "binary_hash": "not_recorded",
            },
        },
        "checks": {
            "strict_ab": False,
            "thresholds": {
                "min_repeated_runs": 5,
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
