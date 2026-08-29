#!/usr/bin/env python3
"""Generate Trajkovic 2D Evidence Doctor manifest.

本脚本只理解本 topic 的 board repeated benchmark 日志。它把每轮 Std/RVV
log 翻译成通用 evidence_manifest.json，供 test-rvv/script/evidence_doctor.py
检查 checksum、run count、metadata 和 B/A 分布。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path


CASE_METADATA = {
    "four_corners_320x240": {
        "evidence_role": "production-fallback-control",
        "case_kind": "fallback_control",
        "boundary": "public_overload",
        "wrapper": "TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/keypoints/trajkovic_2d_response_grid/build/asm/riscv/test_trajkovic_2d_response_grid_rvv.asm",
        "point_type": "PointXYZI",
        "size": "320x240 organized intensity grid",
        "group": "four-corners-finite",
        "gate": "pointxyzi_default_accessor_window3_four_corners",
    },
    "eight_corners_320x240": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/keypoints/trajkovic_2d_response_grid/build/asm/riscv/test_trajkovic_2d_response_grid_rvv.asm",
        "point_type": "PointXYZI",
        "size": "320x240 organized intensity grid",
        "group": "eight-corners-finite",
        "gate": "pointxyzi_default_accessor_window3_eight_corners",
    },
    "four_corners_641x481_tail": {
        "evidence_role": "production-fallback-control",
        "case_kind": "fallback_control",
        "boundary": "public_overload",
        "wrapper": "TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/keypoints/trajkovic_2d_response_grid/build/asm/riscv/test_trajkovic_2d_response_grid_rvv.asm",
        "point_type": "PointXYZI",
        "size": "641x481 organized intensity grid with tail",
        "group": "four-corners-tail",
        "gate": "pointxyzi_default_accessor_window3_four_corners_tail",
    },
    "eight_corners_641x481_tail": {
        "evidence_role": "production-public",
        "case_kind": "production_direct",
        "boundary": "public_overload",
        "wrapper": "TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute",
        "timer_boundary": "production_compute_after_synthetic_setup",
        "asm_boundary": "test-rvv/keypoints/trajkovic_2d_response_grid/build/asm/riscv/test_trajkovic_2d_response_grid_rvv.asm",
        "point_type": "PointXYZI",
        "size": "641x481 organized intensity grid with tail",
        "group": "eight-corners-tail",
        "gate": "pointxyzi_default_accessor_window3_eight_corners_tail",
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
    parser.add_argument("--title", default="trajkovic_2d response grid production manifest")
    parser.add_argument("--std-binary", default="build/riscv/bench_trajkovic_2d_response_grid_std")
    parser.add_argument("--rvv-binary", default="build/riscv/bench_trajkovic_2d_response_grid_rvv")
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
        for case_name in CASE_METADATA:
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
    fallback_controls = []
    summary_rows = []
    std_hash = file_hash(Path(args.std_binary))
    rvv_hash = file_hash(Path(args.rvv_binary))
    for case_name, values in grouped.items():
        metadata = CASE_METADATA[case_name]
        std_values = values["std_ms_values"]  # type: ignore[assignment]
        rvv_values = values["rvv_ms_values"]  # type: ignore[assignment]
        ba_values = values["ba_values"]  # type: ignore[assignment]
        row = {
            "name": case_name,
            "group": metadata["group"],
            "evidence_role": metadata["evidence_role"],
            "case_kind": metadata["case_kind"],
            "point_type": metadata["point_type"],
            "size": metadata["size"],
            "run_count": len(ba_values),
            "iterations": iterations,
            "warmup_iterations": warmup,
            "baseline": {
                "label": "std_build",
                "boundary": metadata["boundary"],
                "wrapper": metadata["wrapper"],
                "row_source": "organized_grid_internal_pixels",
                "solve": False,
                "checksum_policy": "keypoint_indices_and_response_intensity",
                "checksum": values["std_checksum"],
                "timer_boundary": metadata["timer_boundary"],
                "gate": metadata["gate"],
                "mask": "first_threshold_response_mask",
                "reduction": "none",
                "binary_hash": std_hash,
            },
            "candidate": {
                "label": "rvv_build",
                "boundary": metadata["boundary"],
                "wrapper": metadata["wrapper"],
                "row_source": "organized_grid_internal_pixels",
                "solve": False,
                "checksum_policy": "keypoint_indices_and_response_intensity",
                "checksum": values["rvv_checksum"],
                "timer_boundary": metadata["timer_boundary"],
                "gate": metadata["gate"],
                "mask": "first_threshold_response_mask",
                "reduction": "none",
                "binary_hash": rvv_hash,
            },
            "std_ms": statistics.mean(std_values),
            "rvv_ms": statistics.mean(rvv_values),
            "ba_values": ba_values,
            "mean_ba": statistics.mean(ba_values),
            "median_ba": statistics.median(ba_values),
            "binary_hash": f"std={std_hash};rvv={rvv_hash}",
            "asm_boundary": metadata["asm_boundary"],
            "rvv_instr_count": "checked_by_check_production_rvv_asm",
        }
        if metadata["case_kind"] == "fallback_control":
            row["notes"] = (
                "FOUR_CORNERS is intentionally routed to the scalar fallback in the RVV build. "
                "This row checks checksum and overhead only; it is not an RVV adoption comparison."
            )
            fallback_controls.append(row)
        else:
            comparisons.append(row)
        summary_rows.append(row)

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    with args.summary.open("w", encoding="utf-8") as handle:
        handle.write(f"# {args.title}\n\n")
        handle.write("| case | role | runs | mean std ms | mean rvv ms | mean B/A | median B/A | B/A values | checksum |\n")
        handle.write("| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | --- |\n")
        for comp in summary_rows:
            ba_text = ", ".join(f"{value:.3f}x" for value in comp["ba_values"])
            checksum = (
                "match"
                if comp["baseline"]["checksum"] == comp["candidate"]["checksum"]
                else "mismatch"
            )
            handle.write(
                f"| {comp['name']} | {comp['evidence_role']} | {comp['run_count']} | {comp['std_ms']:.6f} | "
                f"{comp['rvv_ms']:.6f} | {comp['mean_ba']:.3f}x | "
                f"{comp['median_ba']:.3f}x | {ba_text} | {checksum} |\n"
            )

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": "production-public",
            "decision_problem": "RVV-vs-scalar",
            "device": "from-board-config",
            "taskset": "not_recorded",
            "governor": "not_recorded",
            "freq": "not_recorded",
            "temperature": "not_recorded",
        },
        "comparisons": comparisons,
        "fallback_controls": fallback_controls,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {args.output} and {args.summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
