#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for organized_pointcloud_conversion.

本脚本只解析本 topic 的 board bench 日志。它把 Std/RVV 日志中的 case
label、平均耗时和 checksum 转成 Evidence Doctor（证据体检）可读取的
manifest。diagnostic（诊断）case 和 production direct（真实生产入口直连）
case 在 manifest 中分开标注，避免把诊断收益外推成生产采纳结论。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+)"
)
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+)")
WARMUP_RE = re.compile(r"Warmup Iterations:\s*(?P<warmup>[0-9]+)")
DATASET_RE = re.compile(r"Dataset:\s*(?P<dataset>.+)")


CASE_METADATA = {
    "pointxyz_disparity_dense_307k": {
        "group": "pointxyz-cloud-to-disparity",
        "case_kind": "test-helper",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "disparity_vector_checksum",
        "candidate_label": "RVV diagnostic candidate",
        "candidate_asm": "test-only candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyz_disparity_mixed_invalid_307k": {
        "group": "pointxyz-cloud-to-disparity",
        "case_kind": "test-helper",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "disparity_vector_checksum",
        "candidate_label": "RVV diagnostic candidate",
        "candidate_asm": "test-only candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyz_disparity_dense_1m": {
        "group": "pointxyz-cloud-to-disparity",
        "case_kind": "test-helper",
        "point_type": "PointXYZ",
        "size": 1048576,
        "checksum_policy": "disparity_vector_checksum",
        "candidate_label": "RVV diagnostic candidate",
        "candidate_asm": "test-only candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "analyze_pointxyz_organized_dense_307k": {
        "group": "analyze-organized-cloud",
        "case_kind": "component_diagnostic",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "analyze_max_depth_focal_bits",
        "candidate_label": "RVV analyze max-depth diagnostic",
        "candidate_asm": "test-only analyze candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "analyze_pointxyz_organized_mixed_invalid_307k": {
        "group": "analyze-organized-cloud",
        "case_kind": "component_diagnostic",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "analyze_max_depth_focal_bits",
        "candidate_label": "RVV analyze max-depth diagnostic",
        "candidate_asm": "test-only analyze candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "production_analyze_detail_pointxyz_dense_307k": {
        "group": "production-analyze-organized-cloud-detail",
        "case_kind": "production_detail_helper",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "analyze_max_depth_focal_bits",
    },
    "production_analyze_detail_pointxyz_mixed_invalid_307k": {
        "group": "production-analyze-organized-cloud-detail",
        "case_kind": "production_detail_helper",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "analyze_max_depth_focal_bits",
    },
    "production_analyze_detail_pointxyzi_mixed_invalid_307k": {
        "group": "production-analyze-organized-cloud-detail",
        "case_kind": "production_detail_helper",
        "point_type": "PointXYZI",
        "size": 307200,
        "checksum_policy": "analyze_max_depth_focal_bits",
    },
    "pointxyzrgb_disparity_rgb_dense_307k": {
        "group": "pointxyzrgb-cloud-to-disparity-color",
        "case_kind": "test-helper",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "disparity_and_color_byte_checksum",
        "candidate_label": "RVV fused disparity/color diagnostic",
        "candidate_asm": "test-only fused color helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyzrgb_disparity_mono_dense_307k": {
        "group": "pointxyzrgb-cloud-to-disparity-color",
        "case_kind": "test-helper",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "disparity_and_mono_byte_checksum",
        "candidate_label": "RVV fused disparity/color diagnostic",
        "candidate_asm": "test-only fused color helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyzrgb_disparity_rgb_mixed_invalid_307k": {
        "group": "pointxyzrgb-cloud-to-disparity-color",
        "case_kind": "test-helper",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "disparity_and_color_byte_checksum",
        "candidate_label": "RVV fused disparity/color diagnostic",
        "candidate_asm": "test-only fused color helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyz_decode_disparity_dense_307k": {
        "group": "pointxyz-disparity-to-cloud",
        "case_kind": "test-helper",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "pointxyz_cloud_checksum",
        "candidate_label": "RVV decode diagnostic candidate",
        "candidate_asm": "test-only decode candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyz_decode_disparity_mixed_invalid_307k": {
        "group": "pointxyz-disparity-to-cloud",
        "case_kind": "test-helper",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "pointxyz_cloud_checksum",
        "candidate_label": "RVV decode diagnostic candidate",
        "candidate_asm": "test-only decode candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "pointxyz_decode_disparity_dense_1m": {
        "group": "pointxyz-disparity-to-cloud",
        "case_kind": "test-helper",
        "point_type": "PointXYZ",
        "size": 1048576,
        "checksum_policy": "pointxyz_cloud_checksum",
        "candidate_label": "RVV decode diagnostic candidate",
        "candidate_asm": "test-only decode candidate helper / inlined bench path",
        "rvv_instr_count": 1,
    },
    "production_pointxyz_disparity_dense_307k": {
        "group": "production-pointxyz-cloud-to-disparity",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "disparity_vector_checksum",
    },
    "production_pointxyz_disparity_mixed_invalid_307k": {
        "group": "production-pointxyz-cloud-to-disparity",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "disparity_vector_checksum",
    },
    "production_pointxyzi_disparity_dense_307k": {
        "group": "production-pointxyz-like-cloud-to-disparity",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZI",
        "size": 307200,
        "checksum_policy": "disparity_vector_checksum",
    },
    "production_pointxyzi_disparity_mixed_invalid_307k": {
        "group": "production-pointxyz-like-cloud-to-disparity",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZI",
        "size": 307200,
        "checksum_policy": "disparity_vector_checksum",
    },
    "production_pointxyzrgb_disparity_rgb_dense_307k": {
        "group": "production-pointxyzrgb-cloud-to-disparity-color",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "disparity_and_color_byte_checksum",
    },
    "production_pointxyzrgb_disparity_mono_dense_307k": {
        "group": "production-pointxyzrgb-cloud-to-disparity-color",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "disparity_and_mono_byte_checksum",
    },
    "production_pointxyzrgb_disparity_rgb_mixed_invalid_307k": {
        "group": "production-pointxyzrgb-cloud-to-disparity-color",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "disparity_and_color_byte_checksum",
    },
    "production_pointxyzrgba_disparity_rgb_dense_307k": {
        "group": "production-pointxyzrgba-cloud-to-disparity-color",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZRGBA",
        "size": 307200,
        "checksum_policy": "disparity_and_color_byte_checksum",
    },
    "production_pointxyzrgba_disparity_rgb_mixed_invalid_307k": {
        "group": "production-pointxyzrgba-cloud-to-disparity-color",
        "case_kind": "production_direct_public_overload",
        "point_type": "PointXYZRGBA",
        "size": 307200,
        "checksum_policy": "disparity_and_color_byte_checksum",
    },
    "production_full_pointxyz_encode_dense_307k": {
        "group": "production-full-encode-pointcloud",
        "case_kind": "production_direct_end_to_end",
        "point_type": "PointXYZ",
        "size": 307200,
        "checksum_policy": "compressed_stream_byte_checksum",
    },
    "production_full_pointxyzrgb_encode_rgb_dense_307k": {
        "group": "production-full-encode-pointcloud-color",
        "case_kind": "production_direct_end_to_end",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "compressed_stream_byte_checksum",
    },
    "production_full_pointxyzrgb_encode_mono_dense_307k": {
        "group": "production-full-encode-pointcloud-color",
        "case_kind": "production_direct_end_to_end",
        "point_type": "PointXYZRGB",
        "size": 307200,
        "checksum_policy": "compressed_stream_byte_checksum",
    },
}


def parse_log(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
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
                }
                index += 1
        index += 1
    if iterations is None or warmup is None:
        raise ValueError(f"missing iterations or warmup in {path}")
    return {
        "path": str(path),
        "iterations": iterations,
        "warmup_iterations": warmup,
        "dataset": dataset or "unknown",
        "cases": cases,
    }


def is_production_case(name: str) -> bool:
    return name.startswith("production_")


def side_metadata(name: str, row: dict, side: str) -> dict:
    meta = CASE_METADATA[name]
    production = is_production_case(name)
    production_analyze_detail = name.startswith("production_analyze_detail_")
    analyze = name.startswith("analyze_")
    if production:
        full_encode = name.startswith("production_full_")
        label = (
            "encodePointCloud-shaped helper scalar build"
            if full_encode and side == "std"
            else "encodePointCloud-shaped helper RVV build"
            if full_encode
            else "production analyze detail scalar build"
            if production_analyze_detail and side == "std"
            else "production analyze detail RVV build"
            if production_analyze_detail
            else "public OrganizedConversion overload scalar build"
            if side == "std"
            else "public OrganizedConversion overload RVV build"
        )
        asm_boundary = (
            "scalar encodePointCloud-shaped helper"
            if full_encode and side == "std"
            else "encodePointCloud-shaped helper with RVV conversion helper"
            if full_encode
            else "production analyze detail scalar helper"
            if production_analyze_detail and side == "std"
            else "production analyze detail RVV helper"
            if production_analyze_detail
            else "scalar production public overload"
            if side == "std"
            else "production public overload RVV helper"
        )
        rvv_instr_count = 0 if side == "std" else 1
        boundary = (
            "production_shaped_encodePointCloud_helper"
            if full_encode
            else "production_detail_helper"
            if production_analyze_detail
            else "public_overload"
        )
        mask = "xyz_finite_semantics" if production_analyze_detail else "public_isFinite_semantics"
        gate = (
            "__RVV10__ production detail dispatch with scalar fallback"
            if production_analyze_detail
            else "__RVV10__ public dispatch with scalar fallback"
        )
    else:
        label = (
            "std analyzeOrganizedCloud scalar path"
            if analyze and side == "std"
            else "std OrganizedConversion scalar path"
            if side == "std"
            else meta["candidate_label"]
        )
        asm_boundary = "scalar bench binary" if side == "std" else meta["candidate_asm"]
        rvv_instr_count = 0 if side == "std" else meta["rvv_instr_count"]
        boundary = "test_helper"
        mask = "xyz_finite_semantics" if analyze else "pcl_isFinite" if side == "std" else "vfclass finite mask"
        gate = "same_input_cloud"
    return {
        "label": label,
        "boundary": boundary,
        "wrapper": "bench_organized_pointcloud_conversion",
        "row_source": "organized_cloud_order",
        "solve": False,
        "checksum_policy": meta["checksum_policy"],
        "checksum": row["checksum"],
        "timer_boundary": (
            "encodePointCloud_end_to_end_checksum_after_timing"
            if name.startswith("production_full_")
            else "analyze_checksum_after_timing"
            if production_analyze_detail or analyze
            else "analyze_checksum_after_timing"
            if analyze
            else "conversion_only_checksum_after_timing"
        ),
        "gate": gate,
        "mask": mask,
        "reduction": "none",
        "asm_boundary": asm_boundary,
        "rvv_instr_count": rvv_instr_count,
        "std_ms": row["avg_ms"] if side == "std" else None,
        "rvv_ms": row["avg_ms"] if side == "rvv" else None,
    }


def comparison_for(name: str, std_row: dict, rvv_row: dict) -> dict:
    meta = CASE_METADATA[name]
    production = is_production_case(name)
    full_encode = name.startswith("production_full_")
    production_analyze_detail = name.startswith("production_analyze_detail_")
    role = (
        "production_shaped_diagnostic"
        if full_encode
        else "production_detail"
        if production_analyze_detail
        else "production_direct"
        if production
        else "diagnostic"
    )
    label = (
        "production-shaped encodePointCloud helper Std/RVV board smoke"
        if full_encode
        else "production detail analyzeOrganizedCloud helper Std/RVV board smoke"
        if production_analyze_detail
        else "production direct public overload Std/RVV board smoke"
        if production
        else "diagnostic helper Std/RVV board smoke"
    )
    comparison = {
        "name": name,
        "label": label,
        "group": meta["group"],
        "evidence_role": role,
        "case_kind": meta["case_kind"],
        "point_type": meta["point_type"],
        "size": meta["size"],
        "run_count": 1,
        "baseline": side_metadata(name, std_row, "std"),
        "candidate": side_metadata(name, rvv_row, "rvv"),
        "ba_values": [std_row["avg_ms"] / rvv_row["avg_ms"]],
    }
    if production:
        comparison["decision_question"] = (
            "RVV-vs-scalar encodePointCloud-shaped end-to-end"
            if full_encode
            else "RVV-vs-scalar production detail analyzeOrganizedCloud"
            if production_analyze_detail
            else "RVV-vs-scalar public overload"
        )
        comparison["notes"] = (
            "production_full here is production-shaped, not the real "
            "OrganizedPointCloudCompression class. The current no-OpenNI cross build cannot "
            "instantiate organized_pointcloud_compression.h because ShiftToDepthConverter is "
            "hidden by HAVE_OPENNI. The helper still includes analyzeOrganizedCloud-shaped scan, "
            "production OrganizedConversion, PNG encode and stream write."
            if full_encode
            else "production_analyze_detail is a production detail helper boundary. It is "
            "called by OrganizedPointCloudCompression::analyzeOrganizedCloud, but this "
            "evidence is not a full public OrganizedPointCloudCompression entry because "
            "the current no-OpenNI cross build cannot instantiate the public class."
            if production_analyze_detail
            else "production_direct here means public overload Std build versus public overload "
            "RVV build. It does not compare two RVV implementation families."
        )
    else:
        comparison["decision_question"] = (
            "component diagnostic RVV-vs-scalar"
            if name.startswith("analyze_")
            else "diagnostic RVV-vs-scalar"
        )
        comparison["notes"] = (
            "analyze diagnostic isolates max-depth and focal-length scan only; it cannot "
            "replace real OrganizedPointCloudCompression production evidence."
            if name.startswith("analyze_")
            else "diagnostic result cannot replace production direct evidence."
        )
    return comparison


def comparison_for_runs(name: str, runs: list[tuple[dict, dict]]) -> dict:
    std_row, rvv_row = runs[-1]
    comparison = comparison_for(name, std_row, rvv_row)
    comparison["run_count"] = len(runs)
    comparison["ba_values"] = [
        std_case["avg_ms"] / rvv_case["avg_ms"] for std_case, rvv_case in runs
    ]
    return comparison


def render_repeated_summary(manifest: dict) -> str:
    evidence_role = manifest["summary"].get("evidence_role", "production_direct")
    title = (
        "# organized_pointcloud_conversion production-shaped end-to-end repeated summary"
        if evidence_role == "production_shaped_diagnostic"
        else "# organized_pointcloud_conversion production-detail repeated summary"
        if evidence_role == "production_detail"
        else "# organized_pointcloud_conversion diagnostic repeated summary"
        if evidence_role == "diagnostic"
        else "# organized_pointcloud_conversion production direct repeated summary"
    )
    boundary = (
        "production-shaped helper"
        if evidence_role == "production_shaped_diagnostic"
        else "production detail helper"
        if evidence_role == "production_detail"
        else "diagnostic helper"
        if evidence_role == "diagnostic"
        else "同一公开入口"
    )
    lines = [
        title,
        "",
        "本摘要由 `script/generate_opc_board_evidence_manifest.py` 从 `run-*` 目录生成。",
        f"每个 B/A 值表示{boundary}下 Std build 平均耗时 / RVV build 平均耗时。",
        "",
        "| case | runs | min | median | max | values |",
        "| --- | ---: | ---: | ---: | ---: | --- |",
    ]
    for comp in manifest["comparisons"]:
        values = sorted(comp["ba_values"])
        median = values[len(values) // 2]
        value_text = ", ".join(f"{value:.3f}x" for value in comp["ba_values"])
        lines.append(
            f"| `{comp['name']}` | {len(values)} | {min(values):.3f}x | "
            f"{median:.3f}x | {max(values):.3f}x | {value_text} |"
        )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument(
        "--run-dir",
        action="append",
        default=[],
        type=Path,
        help="Repeated board run directory containing run_bench_std.log and run_bench_rvv.log.",
    )
    parser.add_argument(
        "--production-only",
        action="store_true",
        help="Only include production_* public-overload cases.",
    )
    args = parser.parse_args()

    run_pairs: list[tuple[dict, dict]] = []
    if args.run_dir:
        for run_dir in args.run_dir:
            run_pairs.append((
                parse_log(run_dir / "run_bench_std.log"),
                parse_log(run_dir / "run_bench_rvv.log"),
            ))
    else:
        if args.std_log is None or args.rvv_log is None:
            raise ValueError("--std-log and --rvv-log are required when --run-dir is not used")
        run_pairs.append((parse_log(args.std_log), parse_log(args.rvv_log)))

    std = run_pairs[-1][0]
    rvv = run_pairs[-1][1]
    comparisons = []
    for name in CASE_METADATA:
        if args.production_only and not is_production_case(name):
            continue
        complete_runs = [
            (std_run["cases"][name], rvv_run["cases"][name])
            for std_run, rvv_run in run_pairs
            if name in std_run["cases"] and name in rvv_run["cases"]
        ]
        if not complete_runs:
            continue
        comparisons.append(comparison_for_runs(name, complete_runs))

    title = "organized_pointcloud_conversion board smoke manifest"
    label = "single-run board smoke; diagnostic and production direct cases are separated per comparison"
    summary_role = "mixed_diagnostic_and_production_direct"
    if args.production_only:
        title = "organized_pointcloud_conversion production direct repeated manifest"
        label = "production direct public overload repeated board summary"
        summary_role = "production_direct"
    if comparisons and all(
        comp.get("evidence_role") == "production_shaped_diagnostic"
        for comp in comparisons
    ):
        title = "organized_pointcloud_conversion production-shaped end-to-end repeated manifest"
        label = "production-shaped encodePointCloud helper repeated board summary"
        summary_role = "production_shaped_diagnostic"
    elif comparisons and all(comp.get("evidence_role") == "production_detail" for comp in comparisons):
        title = "organized_pointcloud_conversion production-detail repeated manifest"
        label = "production detail analyzeOrganizedCloud helper repeated board summary"
        summary_role = "production_detail"
    elif comparisons and all(comp.get("evidence_role") == "diagnostic" for comp in comparisons):
        title = "organized_pointcloud_conversion diagnostic repeated manifest"
        label = "diagnostic helper repeated board summary"
        summary_role = "diagnostic"
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": title,
            "evidence_role": summary_role,
            "summary_path": str(args.summary_log),
            "label": label,
            "analysis_script": "test-rvv/io/organized_pointcloud_conversion/script/generate_opc_board_evidence_manifest.py",
            "metadata": {
                "device": args.device,
                "dataset": rvv["dataset"],
                "iterations": rvv["iterations"],
                "warmup_iterations": rvv["warmup_iterations"],
                "run_count": len(run_pairs),
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
            "thresholds": {
                "min_repeated_runs": 5,
                "stable_runs": 20,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "long_tail_ratio": 1.15,
                "point_outlier_fraction": 0.2,
                "direction_epsilon_fraction": 0.02,
                "near_threshold_margin": 0.05,
                "component_full_epsilon_fraction": 0.05,
                "solve_delta_outlier_ratio": 3.0,
                "solve_delta_outlier_ms": 2.0,
            },
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.run_dir:
        args.summary_log.parent.mkdir(parents=True, exist_ok=True)
        args.summary_log.write_text(render_repeated_summary(manifest), encoding="utf-8")
        print(f"wrote summary: {args.summary_log}")
    print(f"wrote manifest: {args.output}")
    print(f"comparisons: {len(comparisons)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
