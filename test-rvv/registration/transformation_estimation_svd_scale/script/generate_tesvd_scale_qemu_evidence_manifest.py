#!/usr/bin/env python3
"""
生成 transformation_estimation_svd_scale 的 QEMU smoke manifest。

QEMU 只用于 build、correctness 和 log-shape（日志形状）证据。本脚本输出的 manifest
不能作为真实性能结论，只供 Evidence Doctor 暴露 metadata 缺口和 checksum 问题。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
MAX_ERROR_RE = re.compile(
    r"max_(?:reference|public)_error:\s*(?P<value>[0-9]+(?:\.[0-9]+)?(?:e[+-]?[0-9]+)?)",
    re.IGNORECASE,
)
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")


def size_suffix(label: str) -> str | None:
    match = re.search(r"(\d+K)", label)
    return match.group(1) if match else None


def size_value(suffix: str | None) -> int:
    if suffix is None:
        return 0
    return int(suffix.rstrip("K")) * 1024


def point_type_from_label(label: str, prefix: str) -> str:
    remainder = label[len(prefix):].strip()
    match = re.search(r"\s+\d+K$", remainder)
    if match:
        remainder = remainder[:match.start()].strip()
    return remainder if "->" in remainder else "PointXYZ"


def row_source_from_label(label: str) -> str:
    if label.startswith("row source generic scalar double production probe source-indexed"):
        return "source-indexed"
    if label.startswith("row source generic scalar double production probe dual-indexed"):
        return "dual-indexed"
    if label.startswith("row source generic scalar double production probe correspondence"):
        return "correspondence"
    if label.startswith("scalar double row source production probe source-indexed"):
        return "source-indexed"
    if label.startswith("scalar double row source production probe dual-indexed"):
        return "dual-indexed"
    if label.startswith("scalar double row source production probe correspondence"):
        return "correspondence"
    if label.startswith("custom alignment row source scale source-indexed"):
        return "source-indexed"
    if label.startswith("custom alignment row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("custom alignment row source scale correspondence"):
        return "correspondence"
    if label.startswith("custom padding row source scale source-indexed"):
        return "source-indexed"
    if label.startswith("custom padding row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("custom padding row source scale correspondence"):
        return "correspondence"
    if label.startswith("custom row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("custom row source scale correspondence"):
        return "correspondence"
    if label.startswith("row source correspondence sorted-copy production probe"):
        return "correspondence"
    if label.startswith("row source correspondence sorted-copy scalar double production probe"):
        return "correspondence"
    if label.startswith("row source scale source-indexed"):
        return "source-indexed"
    if label.startswith("row source scale dual-indexed"):
        return "dual-indexed"
    if label.startswith("row source scale correspondence"):
        return "correspondence"
    if label.startswith("row source scale source-indexed locality-"):
        return "source-indexed"
    if label.startswith("row source scale dual-indexed locality-"):
        return "dual-indexed"
    if label.startswith("row source scale correspondence locality-"):
        return "correspondence"
    return "ordered-cloud-pair"


def row_source_point_type_from_label(label: str) -> str:
    for prefix in (
        "row source generic scalar double production probe source-indexed",
        "row source generic scalar double production probe dual-indexed",
        "row source generic scalar double production probe correspondence",
        "scalar double row source production probe source-indexed",
        "scalar double row source production probe dual-indexed",
        "scalar double row source production probe correspondence",
        "custom alignment row source scale source-indexed",
        "custom alignment row source scale dual-indexed",
        "custom alignment row source scale correspondence",
        "custom padding row source scale source-indexed",
        "custom padding row source scale dual-indexed",
        "custom padding row source scale correspondence",
        "custom row source scale dual-indexed",
        "custom row source scale correspondence",
    ):
        if label.startswith(prefix):
            remainder = label[len(prefix):].strip()
            if remainder.startswith("locality-"):
                parts = remainder.split(maxsplit=1)
                remainder = parts[1] if len(parts) == 2 else ""
            if match := re.search(r"\s+\d+K$", remainder):
                remainder = remainder[: match.start()].strip()
            return remainder if "->" in remainder else "PointXYZ"
    for prefix in (
        "row source correspondence sorted-copy production probe",
        "row source correspondence sorted-copy scalar double production probe",
        "row source scale source-indexed",
        "row source scale dual-indexed",
        "row source scale correspondence",
        "row source scale source-indexed locality-",
        "row source scale dual-indexed locality-",
        "row source scale correspondence locality-",
    ):
        if label.startswith(prefix):
            remainder = label[len(prefix):].strip()
            if match := re.search(r"\s+\d+K$", remainder):
                remainder = remainder[: match.start()].strip()
            return remainder if "->" in remainder else "PointXYZ"
    return "PointXYZ"


def parse_log(path: Path) -> dict[str, Any]:
    metadata: dict[str, Any] = {}
    cases: dict[str, dict[str, Any]] = {}
    current_case: str | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := ITER_RE.match(line):
            metadata["iterations"] = int(match.group("value"))
            continue
        if match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
            continue
        if match := DATASET_RE.match(line):
            metadata["dataset"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["log_checksum"] = match.group("checksum")
        if current_case and (match := MAX_ERROR_RE.search(line)):
            cases[current_case]["max_reference_error"] = float(match.group("value"))
            current_case = None
    return {"metadata": metadata, "cases": cases}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", required=True)
    parser.add_argument("--rvv-log", required=True)
    parser.add_argument("--summary-md", required=True)
    parser.add_argument("--asm", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    std = parse_log(Path(args.std_log))
    rvv = parse_log(Path(args.rvv_log))
    common_cases = set(std["cases"]) & set(rvv["cases"])
    custom_row_source_profile = any(
        name.startswith("custom row source scale") for name in common_cases
    )
    custom_layout_padding_sensitivity = any(
        name.startswith("custom padding row source scale") for name in common_cases
    )
    custom_layout_alignment_sensitivity = any(
        name.startswith("custom alignment row source scale") for name in common_cases
    )
    scalar_double_production = any(
        name.startswith("scalar double production probe ordered-cloud-pair")
        for name in common_cases
    )
    generic_scalar_double_production = any(
        name.startswith("generic scalar double ordered production probe")
        for name in common_cases
    )
    generic_scalar_double_row_source_production = any(
        name.startswith("row source generic scalar double production probe")
        for name in common_cases
    )
    scalar_double_row_source_production = any(
        name.startswith("scalar double row source production probe")
        for name in common_cases
    )
    production_public = any(name.startswith("public scale ordered-cloud-pair")
                            for name in common_cases)
    generic_public = any(name.startswith("generic public scale")
                         for name in common_cases)
    more_generic_public = any(name.startswith("more generic public scale")
                              for name in common_cases)
    custom_layout_public = any(
        name.startswith("custom layout public scale") or
        "LocalPaddedXYZSource->LocalWideXYZTarget" in name
        for name in common_cases
    )
    row_source_probe_public = any(
        name.startswith("row source correspondence sorted-copy production probe")
        for name in common_cases
    )
    correspondence_sorted_copy_scalar_double_production = any(
        name.startswith("row source correspondence sorted-copy scalar double production probe")
        for name in common_cases
    )
    row_source_public = row_source_probe_public or any(
        name.startswith("row source scale") for name in common_cases
    )
    row_source_locality_public = row_source_public and any(
        " locality-" in name for name in common_cases
    )
    row_source_more_generic_public = row_source_public and any(
        row_source_point_type_from_label(name) in {
            "PointXYZRGBA->PointXYZRGBA",
            "PointNormal->PointXYZRGB",
            "PointWithViewpoint->PointXYZ",
        }
        for name in common_cases
    ) and not row_source_probe_public
    row_source_all_more_generic_public = row_source_public and any(
        row_source_point_type_from_label(name) in {
            "PointXYZL->PointXYZ",
            "PointWithRange->PointWithRange",
        }
        for name in common_cases
    ) and not row_source_probe_public
    if row_source_all_more_generic_public:
        row_source_more_generic_public = False
    row_source_generic_public = row_source_public and any(
        row_source_point_type_from_label(name) != "PointXYZ" for name in common_cases
    ) and not row_source_probe_public and not row_source_more_generic_public and not row_source_all_more_generic_public
    matrix_local = any(name.startswith("matrix local scale simplification")
                       for name in common_cases)
    if generic_scalar_double_row_source_production:
        case_kind = "qemu_generic_scalar_double_row_source_production_probe_smoke"
        boundary = "production_public_generic_scalar_double_row_source_probe"
        baseline_label = "std-generic-public-double-row-source-scale"
        candidate_label = "rvv-generic-public-double-row-source-f64-widened-probe"
        run_label = "qemu-tesvd-scale-row-source-generic-scalar-double-production-probe-smoke-phase069"
        baseline_reduction = "generic_scalar_double_public_row_source_fallback"
        candidate_reduction = "rvv_f64_widened_generic_row_source_public_accumulation"
        asm_boundary = str(args.asm)
        gate = "row_source_dense_common_pcl_xyz_aos_double"
    elif correspondence_sorted_copy_scalar_double_production:
        case_kind = "qemu_correspondence_sorted_copy_scalar_double_production_probe_smoke"
        boundary = "production_public_correspondence_sorted_copy_scalar_double_probe"
        baseline_label = "std-correspondence-sorted-copy-scalar-double-production-probe"
        candidate_label = "rvv-correspondence-sorted-copy-scalar-double-production-probe"
        run_label = "qemu-tesvd-scale-correspondence-sorted-copy-scalar-double-production-probe-smoke-phase072"
        baseline_reduction = "scalar_double_public_correspondence_fallback"
        candidate_reduction = "rvv_f64_widened_correspondence_sorted_copy_accumulation"
        asm_boundary = str(args.asm)
        gate = "correspondence_dense_pointxyz_double_sorted_copy_size_disorder"
    elif scalar_double_row_source_production:
        case_kind = "qemu_scalar_double_row_source_production_probe_smoke"
        boundary = "production_public_scalar_double_row_source_probe"
        baseline_label = "std-public-double-row-source-scale"
        candidate_label = "rvv-public-double-row-source-f64-widened-probe"
        run_label = "qemu-tesvd-scale-row-source-scalar-double-production-probe-smoke-phase067"
        baseline_reduction = "scalar_double_public_row_source_fallback"
        candidate_reduction = "rvv_f64_widened_row_source_public_accumulation"
        asm_boundary = str(args.asm)
        gate = "row_source_dense_pointxyz_double"
    elif scalar_double_production:
        case_kind = "qemu_scalar_double_production_probe_smoke"
        boundary = "production_public_scalar_double_ordered_cloud_pair_probe"
        baseline_label = "std-public-double-scale-ordered-cloud-pair"
        candidate_label = "rvv-public-double-f64-widened-probe"
        run_label = "qemu-tesvd-scale-scalar-double-production-probe-smoke-phase065"
        baseline_reduction = "scalar_double_public_fallback"
        candidate_reduction = "rvv_f64_widened_public_accumulation"
        asm_boundary = str(args.asm)
        gate = "ordered_dense_pointxyz_double"
    elif generic_scalar_double_production:
        case_kind = "qemu_generic_scalar_double_ordered_production_probe_smoke"
        boundary = "production_public_generic_scalar_double_ordered_cloud_pair_probe"
        baseline_label = "std-generic-public-double-scale-ordered-cloud-pair"
        candidate_label = "rvv-generic-public-double-f64-widened-probe"
        run_label = "qemu-tesvd-scale-generic-scalar-double-ordered-production-probe-smoke-phase068"
        baseline_reduction = "generic_scalar_double_public_fallback"
        candidate_reduction = "rvv_f64_widened_generic_public_accumulation"
        asm_boundary = str(args.asm)
        gate = "ordered_dense_common_pcl_xyz_aos_double"
    elif production_public:
        case_kind = "qemu_production_public_smoke"
        boundary = "production_public_ordered_cloud_pair"
        baseline_label = "std-public-scale-ordered-cloud-pair"
        candidate_label = "rvv-public-scale-ordered-cloud-pair"
        run_label = "qemu-tesvd-scale-public-scale-smoke-phase010"
        baseline_reduction = "scalar_ordered_accumulation"
        candidate_reduction = "rvv_vfredosum_scale_accumulation"
        asm_boundary = str(args.asm)
    elif custom_layout_alignment_sensitivity:
        case_kind = "qemu_custom_layout_alignment_sensitivity_smoke"
        boundary = "production_public_custom_layout_alignment_sensitivity"
        baseline_label = "std-custom-layout-alignment-sensitivity-scale"
        candidate_label = "rvv-custom-layout-alignment-sensitivity-scale"
        run_label = "qemu-tesvd-scale-custom-layout-alignment-sensitivity-smoke-phase059"
        baseline_reduction = "scalar_custom_layout_alignment_row_source_public_scale"
        candidate_reduction = "rvv_custom_layout_alignment_row_source_public_scale_accumulation"
        asm_boundary = str(args.asm)
        gate = "row_source_dense_traits_gated_custom_xyz_aos_float"
    elif custom_layout_padding_sensitivity:
        case_kind = "qemu_custom_layout_padding_sensitivity_smoke"
        boundary = "production_public_custom_layout_padding_sensitivity"
        baseline_label = "std-custom-layout-padding-sensitivity-scale"
        candidate_label = "rvv-custom-layout-padding-sensitivity-scale"
        run_label = "qemu-tesvd-scale-custom-layout-padding-sensitivity-smoke-phase057"
        baseline_reduction = "scalar_custom_layout_padding_row_source_public_scale"
        candidate_reduction = "rvv_custom_layout_padding_row_source_public_scale_accumulation"
        asm_boundary = str(args.asm)
        gate = "row_source_dense_traits_gated_custom_xyz_aos_float"
    elif custom_row_source_profile:
        case_kind = "qemu_custom_row_source_large_variance_profile_smoke"
        boundary = "production_public_custom_row_source_large_variance_profile"
        baseline_label = "std-custom-row-source-profile-scale"
        candidate_label = "rvv-custom-row-source-profile-scale"
        run_label = "qemu-tesvd-scale-custom-row-source-large-variance-profile-smoke-phase056"
        baseline_reduction = "scalar_custom_row_source_public_scale"
        candidate_reduction = "rvv_custom_row_source_public_scale_accumulation"
        asm_boundary = str(args.asm)
        gate = "row_source_dense_traits_gated_custom_xyz_aos_float"
    elif custom_layout_public:
        case_kind = "qemu_custom_xyz_aos_layout_sampling_smoke"
        boundary = "production_public_custom_xyz_aos_layout_sampling"
        baseline_label = "std-custom-xyz-aos-layout-public-scale"
        candidate_label = "rvv-custom-xyz-aos-layout-public-scale"
        run_label = "qemu-tesvd-scale-custom-xyz-aos-layout-sampling-smoke-phase055"
        baseline_reduction = "scalar_custom_xyz_aos_public_scale"
        candidate_reduction = "rvv_custom_xyz_aos_public_scale_accumulation"
        asm_boundary = str(args.asm)
        gate = "dense_traits_gated_custom_xyz_aos_float"
    elif generic_public or more_generic_public:
        case_kind = "qemu_generic_public_smoke"
        boundary = (
            "production_public_more_generic_xyz_aos_point_types"
            if more_generic_public
            else "production_public_generic_xyz_point_types"
        )
        baseline_label = (
            "std-more-generic-public-scale"
            if more_generic_public
            else "std-generic-public-scale"
        )
        candidate_label = (
            "rvv-more-generic-public-scale"
            if more_generic_public
            else "rvv-generic-public-scale"
        )
        run_label = (
            "qemu-tesvd-scale-more-generic-xyz-aos-point-types-public-smoke-phase051"
            if more_generic_public
            else "qemu-tesvd-scale-generic-xyz-point-types-public-smoke-phase041"
        )
        baseline_reduction = "scalar_generic_public_scale"
        candidate_reduction = "rvv_generic_public_scale_accumulation"
        asm_boundary = str(args.asm)
        gate = "ordered_dense_traits_gated_xyz_aos_float"
    elif row_source_public:
        case_kind = (
            "qemu_row_source_public_probe_smoke"
            if row_source_probe_public
            else
            "qemu_row_source_locality_order_profile_smoke"
            if row_source_locality_public
            else
            "qemu_row_source_more_generic_public_smoke"
            if row_source_more_generic_public
            else
            "qemu_row_source_all_more_generic_public_smoke"
            if row_source_all_more_generic_public
            else
            "qemu_row_source_generic_public_smoke"
            if row_source_generic_public
            else "qemu_row_source_public_smoke"
        )
        boundary = (
            "production_public_correspondence_sorted_copy_probe"
            if row_source_probe_public
            else
            "production_public_row_source_locality_order_profile"
            if row_source_locality_public
            else
            "production_public_row_source_more_generic_xyz_aos_point_types"
            if row_source_more_generic_public
            else
            "production_public_row_source_all_more_generic_xyz_aos_matrix"
            if row_source_all_more_generic_public
            else
            "production_public_row_source_generic_xyz_point_types"
            if row_source_generic_public
            else "production_public_row_source_scale"
        )
        baseline_label = (
            "std-correspondence-sorted-copy-production-probe"
            if row_source_probe_public
            else "std-row-source-public-scale"
        )
        candidate_label = (
            "rvv-correspondence-sorted-copy-production-probe"
            if row_source_probe_public
            else "rvv-row-source-public-scale"
        )
        run_label = (
            "qemu-tesvd-scale-correspondence-sorted-copy-production-probe-smoke-phase047"
            if row_source_probe_public
            else
            "qemu-tesvd-scale-row-source-locality-order-profile-smoke-phase045"
            if row_source_locality_public
            else
            "qemu-tesvd-scale-row-source-more-generic-xyz-aos-point-types-smoke-phase053"
            if row_source_more_generic_public
            else
            "qemu-tesvd-scale-row-source-all-more-generic-xyz-aos-matrix-smoke-phase054"
            if row_source_all_more_generic_public
            else
            "qemu-tesvd-scale-row-source-generic-xyz-point-types-smoke-phase044"
            if row_source_generic_public
            else "qemu-tesvd-scale-row-source-scale-smoke-phase043"
        )
        baseline_reduction = (
            "scalar_public_correspondence_scale"
            if row_source_probe_public
            else "scalar_row_source_public_scale"
        )
        candidate_reduction = (
            "rvv_correspondence_sorted_copy_scale_accumulation"
            if row_source_probe_public
            else "rvv_row_source_public_scale_accumulation"
        )
        asm_boundary = str(args.asm)
        gate = "row_source_dense_traits_gated_xyz_aos_float"
    elif matrix_local:
        case_kind = "implementation_shape_diagnostic"
        boundary = "test_helper_matrix_local_scale"
        baseline_label = "std-legacy-rotated-cloud-scale"
        candidate_label = "rvv-build-trace-rh-scale"
        run_label = "qemu-tesvd-scale-matrix-local-scale-smoke-phase020"
        baseline_reduction = "legacy_rotated_cloud_matrix_multiply"
        candidate_reduction = "trace_rh_scalar_formula"
        asm_boundary = "not_applicable_scalar_formula_diagnostic"
        gate = "ordered_dense_pointxyz_float"
    else:
        case_kind = "test-only-diagnostic"
        boundary = "test_helper"
        baseline_label = "std-fused-scale-reference"
        candidate_label = "rvv-fused-scale-candidate"
        run_label = "qemu-tesvd-scale-ordered-cloud-pair-smoke-phase000"
        baseline_reduction = "scalar_ordered_accumulation"
        candidate_reduction = "rvv_vfredosum_scale_accumulation"
        asm_boundary = str(args.asm)
        gate = "ordered_dense_pointxyz_float"
    if production_public:
        gate = "ordered_dense_pointxyz_float"
    comparisons = []
    for name in sorted(set(std["cases"]) & set(rvv["cases"])):
        std_case = std["cases"][name]
        rvv_case = rvv["cases"][name]
        if (custom_layout_alignment_sensitivity or custom_layout_padding_sensitivity or
                custom_row_source_profile or scalar_double_row_source_production or
                generic_scalar_double_row_source_production or
                correspondence_sorted_copy_scalar_double_production):
            point_type = row_source_point_type_from_label(name)
        elif custom_layout_public and name.startswith("custom layout public scale"):
            point_type = point_type_from_label(name, "custom layout public scale")
        elif more_generic_public:
            point_type = point_type_from_label(name, "more generic public scale")
        elif generic_public:
            point_type = point_type_from_label(name, "generic public scale")
        elif row_source_public:
            point_type = row_source_point_type_from_label(name)
        else:
            point_type = "PointXYZ"
        row_source = row_source_from_label(name)
        comparisons.append(
            {
                "name": name,
                "group": row_source,
                "evidence_role": "qemu_smoke_only",
                "case_kind": case_kind,
                "point_type": point_type,
                "size": size_value(size_suffix(name)),
                "run_count": 1,
                "observed_ms": {
                    "std": float(std_case["ms"]),
                    "rvv": float(rvv_case["ms"]),
                },
                "log_checksum": {
                    "std": str(std_case.get("log_checksum")),
                    "rvv": str(rvv_case.get("log_checksum")),
                },
                "max_reference_error": {
                    "std": std_case.get("max_reference_error"),
                    "rvv": rvv_case.get("max_reference_error"),
                    "tolerance": 5.0e-8
                    if (scalar_double_production
                        or scalar_double_row_source_production
                        or generic_scalar_double_production
                        or generic_scalar_double_row_source_production)
                        or correspondence_sorted_copy_scalar_double_production
                    else 2.0e-3,
                },
                "baseline": {
                    "label": baseline_label,
                    "boundary": boundary,
                    "wrapper": "same_outer_wrapper",
                    "row_source": row_source,
                    "solve": True,
                    "checksum_policy": "tolerance_checked_by_gtest_matrix_max_abs_error",
                    "timer_boundary": "estimate_only",
                    "gate": gate,
                    "mask": "none_dense_only",
                    "reduction": baseline_reduction,
                    "asm_boundary": "not_applicable_qemu_std",
                    "rvv_instr_count": 0,
                },
                "candidate": {
                    "label": candidate_label,
                    "boundary": boundary,
                    "wrapper": "same_outer_wrapper",
                    "row_source": row_source,
                    "solve": True,
                    "checksum_policy": "tolerance_checked_by_gtest_matrix_max_abs_error",
                    "timer_boundary": "estimate_only",
                    "gate": gate,
                    "mask": "none_dense_only",
                    "reduction": candidate_reduction,
                    "asm_boundary": asm_boundary,
                    "rvv_instr_count": (
                        0 if matrix_local else "see asm summary"
                    ),
                },
                "notes": (
                    "QEMU timing is smoke-only. log_checksum is retained as a fingerprint. "
                    "For matrix-local-scale, the Std/RVV binaries are only used as the "
                    "legacy-vs-trace formula A/B carrier; this is not RVV instruction evidence. "
                    "Correctness is gated by gtest and max_reference_error."
                ),
            }
        )

    manifest = {
        "schema_version": 1,
            "summary": {
            "title": (
                "transformation_estimation_svd_scale QEMU correspondence sorted-copy Scalar=double production probe smoke manifest"
                if correspondence_sorted_copy_scalar_double_production
                else
                "transformation_estimation_svd_scale QEMU row-source scalar-double production probe smoke manifest"
                if scalar_double_row_source_production
                else "transformation_estimation_svd_scale QEMU scalar-double production probe smoke manifest"
                if scalar_double_production
                else "transformation_estimation_svd_scale QEMU generic Scalar=double ordered production probe smoke manifest"
                if generic_scalar_double_production
                else "transformation_estimation_svd_scale QEMU public-scale smoke manifest"
                if production_public
                else "transformation_estimation_svd_scale QEMU custom layout alignment sensitivity smoke manifest"
                if custom_layout_alignment_sensitivity
                else "transformation_estimation_svd_scale QEMU custom row-source large variance profile smoke manifest"
                if custom_row_source_profile
                else "transformation_estimation_svd_scale QEMU custom xyz AoS layout sampling smoke manifest"
                if custom_layout_public
                else "transformation_estimation_svd_scale QEMU generic-public smoke manifest"
                if generic_public
                else "transformation_estimation_svd_scale QEMU row-source locality/order profile smoke manifest"
                if row_source_locality_public
                else "transformation_estimation_svd_scale QEMU row-source more-generic public smoke manifest"
                if row_source_more_generic_public
                else "transformation_estimation_svd_scale QEMU row-source all-more-generic public smoke manifest"
                if row_source_all_more_generic_public
                else "transformation_estimation_svd_scale QEMU row-source generic public smoke manifest"
                if row_source_generic_public
                else "transformation_estimation_svd_scale QEMU row-source public smoke manifest"
                if row_source_public
                else "transformation_estimation_svd_scale QEMU matrix-local-scale smoke manifest"
                if matrix_local
                else "transformation_estimation_svd_scale QEMU smoke manifest"
            ),
            "evidence_role": "qemu_smoke_only",
            "summary_path": args.summary_md,
            "label": run_label,
            "analysis_script": "test-rvv/registration/transformation_estimation_svd_scale/script/generate_tesvd_scale_qemu_evidence_manifest.py",
            "metadata": {
                "device": "qemu",
                "iterations": std["metadata"].get("iterations"),
                "warmup_iterations": std["metadata"].get("warmup_iterations"),
                "run_count": 1,
                "taskset": "not_applicable_qemu",
                "governor": "not_applicable_qemu",
                "freq": "not_applicable_qemu",
                "temperature": "not_applicable_qemu",
                "vlen": "qemu rv64gcv",
                "binary_hash": "not_recorded",
                "dataset": std["metadata"].get("dataset", "not_recorded"),
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
            ],
            "thresholds": {
                "min_repeated_runs": 1,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
                "near_threshold_margin": 0.05,
            },
        },
        "comparisons": comparisons,
    }
    Path(args.output).write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
