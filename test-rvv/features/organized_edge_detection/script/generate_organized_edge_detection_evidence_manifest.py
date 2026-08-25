#!/usr/bin/env python3
"""Generate organized_edge_detection Evidence Doctor manifest.

本脚本只理解本 topic 的 benchmark case（性能测试用例）和日志格式。它把
board repeated run 目录里的 Std/RVV log 翻译成通用 evidence_manifest.json，
供 test-rvv/script/evidence_doctor.py 做数据契约检查。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path


CASE_METADATA = {
    "depth_labels_finite_320x240": {
        "evidence_role": "diagnostic",
        "case_kind": "test_helper",
        "boundary": "test_helper",
        "wrapper": "computeDepthLabelsRVV",
        "timer_boundary": "depth_label_helper_only_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_rvv.asm",
        "point_type": "PointXYZ_Label",
        "size": "320x240 finite organized grid",
        "group": "depth-labels-finite",
        "gate": "all_neighbors_finite",
    },
    "depth_labels_finite_641x481_tail": {
        "evidence_role": "diagnostic",
        "case_kind": "test_helper",
        "boundary": "test_helper",
        "wrapper": "computeDepthLabelsRVV",
        "timer_boundary": "depth_label_helper_only_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_rvv.asm",
        "point_type": "PointXYZ_Label",
        "size": "641x481 finite organized grid with tail",
        "group": "depth-labels-finite-tail",
        "gate": "all_neighbors_finite_tail",
    },
    "depth_labels_nan_boundary_320x240": {
        "evidence_role": "diagnostic",
        "case_kind": "test_helper",
        "boundary": "test_helper",
        "wrapper": "computeDepthLabelsRVV",
        "timer_boundary": "depth_label_helper_only_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_rvv.asm",
        "point_type": "PointXYZ_Label",
        "size": "320x240 organized grid with NaN boundary",
        "group": "depth-labels-invalid-neighbor",
        "gate": "invalid_neighbor_scalar_fallback",
    },
    "prod_depth_finite_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZ, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZ_Label",
        "size": "320x240 finite organized grid",
        "group": "prod-depth-labels-finite",
        "gate": "all_neighbors_finite",
    },
    "prod_depth_finite_641x481_tail": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZ, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZ_Label",
        "size": "641x481 finite organized grid with tail",
        "group": "prod-depth-labels-finite-tail",
        "gate": "all_neighbors_finite_tail",
    },
    "prod_depth_nan_boundary_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZ, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZ_Label",
        "size": "320x240 organized grid with NaN boundary",
        "group": "prod-depth-labels-invalid-neighbor",
        "gate": "invalid_neighbor_scalar_fallback",
    },
    "prod_depth_pointxyzi_finite_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZI, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZI_Label",
        "size": "320x240 finite organized grid",
        "group": "prod-depth-labels-point-type-expansion",
        "gate": "pointxyzi_aos_float_xyz",
    },
    "prod_depth_pointxyzrgb_finite_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZRGB, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZRGB_Label",
        "size": "320x240 finite organized grid",
        "group": "prod-depth-labels-point-type-expansion",
        "gate": "pointxyzrgb_aos_float_xyz",
    },
    "prod_depth_pointxyzrgbnormal_finite_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZRGBNormal, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZRGBNormal_Label",
        "size": "320x240 finite organized grid",
        "group": "prod-depth-labels-point-type-expansion",
        "gate": "pointxyzrgbnormal_aos_float_xyz",
    },
    "prod_depth_pointxyzrgb_nan_boundary_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "OrganizedEdgeBase<PointXYZRGB, Label>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/features/organized_edge_detection/build/asm/riscv/bench_organized_edge_detection_production_rvv.asm",
        "point_type": "PointXYZRGB_Label",
        "size": "320x240 organized grid with NaN boundary",
        "group": "prod-depth-labels-point-type-expansion-invalid-neighbor",
        "gate": "pointxyzrgb_invalid_neighbor_scalar_fallback",
    },
}

TIME_RE = re.compile(r"^(?P<case>[A-Za-z0-9_]+)\s*:\s*(?P<ms>[0-9.]+)\s*ms\s*/\s*iter")
CHECKSUM_RE = re.compile(r"^(?P<case>[A-Za-z0-9_]+)\s+checksum:\s*(?P<checksum>[-+0-9.eE]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<warmup>\d+)", re.MULTILINE)


def parse_log(path: Path) -> dict[str, dict[str, float | str]]:
    cases: dict[str, dict[str, float | str]] = {}
    if not path.exists():
        return cases
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := TIME_RE.match(line.strip()):
            cases.setdefault(match.group("case"), {})["ms"] = float(match.group("ms"))
        if match := CHECKSUM_RE.match(line.strip()):
            cases.setdefault(match.group("case"), {})["checksum"] = match.group("checksum")
    return cases


def first_int(pattern: re.Pattern[str], text: str, default: int) -> int:
    match = pattern.search(text)
    return int(match.group(1)) if match else default


def file_hash(path: Path) -> str:
    if not path.exists():
        return "not_recorded"
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", action="append", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--title", default="organized_edge_detection depth-label diagnostic manifest")
    parser.add_argument("--std-binary", default="build/riscv/bench_organized_edge_detection_std")
    parser.add_argument("--rvv-binary", default="build/riscv/bench_organized_edge_detection_rvv")
    args = parser.parse_args()

    grouped: dict[str, dict[str, list[float] | str | int]] = {}
    iterations = 0
    warmup = 0
    for run_dir in args.run_dir:
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        std_text = std_log.read_text(encoding="utf-8", errors="replace") if std_log.exists() else ""
        rvv_text = rvv_log.read_text(encoding="utf-8", errors="replace") if rvv_log.exists() else ""
        iterations = iterations or first_int(ITER_RE, std_text or rvv_text, 0)
        warmup = warmup or first_int(WARMUP_RE, std_text or rvv_text, 0)
        std_cases = parse_log(std_log)
        rvv_cases = parse_log(rvv_log)
        for case_name, metadata in CASE_METADATA.items():
            if case_name not in std_cases or case_name not in rvv_cases:
                continue
            std_ms = float(std_cases[case_name]["ms"])
            rvv_ms = float(rvv_cases[case_name]["ms"])
            entry = grouped.setdefault(
                case_name,
                {
                    "std_ms_values": [],
                    "rvv_ms_values": [],
                    "ba_values": [],
                    "std_checksum": str(std_cases[case_name].get("checksum", "missing")),
                    "rvv_checksum": str(rvv_cases[case_name].get("checksum", "missing")),
                },
            )
            entry["std_ms_values"].append(std_ms)  # type: ignore[index]
            entry["rvv_ms_values"].append(rvv_ms)  # type: ignore[index]
            entry["ba_values"].append(std_ms / rvv_ms if rvv_ms else 0.0)  # type: ignore[index]

    comparisons = []
    std_hash = file_hash(Path(args.std_binary))
    rvv_hash = file_hash(Path(args.rvv_binary))
    for case_name, values in grouped.items():
        metadata = CASE_METADATA[case_name]
        std_values = values["std_ms_values"]  # type: ignore[assignment]
        rvv_values = values["rvv_ms_values"]  # type: ignore[assignment]
        comparisons.append(
            {
                "name": case_name,
                "group": metadata["group"],
                "evidence_role": metadata["evidence_role"],
                "case_kind": metadata["case_kind"],
                "point_type": metadata["point_type"],
                "size": metadata["size"],
                "run_count": len(values["ba_values"]),  # type: ignore[arg-type]
                "iterations": iterations,
                "warmup_iterations": warmup,
                "baseline": {
                    "label": "std_build",
                    "boundary": metadata["boundary"],
                    "wrapper": metadata["wrapper"],
                    "row_source": "organized_grid_internal_pixels",
                    "solve": False,
                    "checksum_policy": "weighted_label_bits",
                    "checksum": values["std_checksum"],
                    "timer_boundary": metadata["timer_boundary"],
                    "gate": metadata["gate"],
                    "mask": "finite_depth_and_invalid_neighbor_search",
                    "reduction": "none",
                    "asm_boundary": "not_checked_for_std",
                    "binary_hash": std_hash,
                    "std_ms": sum(std_values) / len(std_values),
                    "rvv_ms": sum(std_values) / len(std_values),
                },
                "candidate": {
                    "label": "rvv_build",
                    "boundary": metadata["boundary"],
                    "wrapper": metadata["wrapper"],
                    "row_source": "organized_grid_internal_pixels",
                    "solve": False,
                    "checksum_policy": "weighted_label_bits",
                    "checksum": values["rvv_checksum"],
                    "timer_boundary": metadata["timer_boundary"],
                    "gate": metadata["gate"],
                    "mask": "finite_depth_and_invalid_neighbor_search",
                    "reduction": "none",
                    "asm_boundary": metadata["asm_boundary"],
                    "rvv_instr_count": None,
                    "binary_hash": rvv_hash,
                    "std_ms": sum(std_values) / len(std_values),
                    "rvv_ms": sum(rvv_values) / len(rvv_values),
                },
                "ba_values": values["ba_values"],
                "device": "Milkv-Jupiter",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": rvv_hash,
            }
        )

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    summary_lines = [
        f"# {args.title}",
        "",
        "| case | runs | mean B/A | median B/A | min B/A | max B/A | checksum status |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for case_name, values in grouped.items():
        ba_values = values["ba_values"]  # type: ignore[assignment]
        checksum_status = "match" if values["std_checksum"] == values["rvv_checksum"] else "mismatch"
        summary_lines.append(
            "| {case} | {runs} | {mean:.3f}x | {median:.3f}x | {minv:.3f}x | {maxv:.3f}x | {checksum} |".format(
                case=case_name,
                runs=len(ba_values),
                mean=statistics.mean(ba_values),
                median=statistics.median(ba_values),
                minv=min(ba_values),
                maxv=max(ba_values),
                checksum=checksum_status,
            )
        )
    summary_lines.extend(
        [
            "",
            "Evidence role 和 A/B boundary（A/B 边界）逐 case 写入 manifest；本 summary 只保留统计摘要。",
            "QEMU timing 不参与本 summary；性能数字来自 board repeated run directories。",
        ]
    )
    args.summary.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": ",".join(sorted({str(CASE_METADATA[name]["evidence_role"]) for name in grouped})),
            "run_count": max((len(v["ba_values"]) for v in grouped.values()), default=0),
            "iterations": iterations,
            "warmup_iterations": warmup,
            "source_summary": str(args.summary),
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
