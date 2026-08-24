#!/usr/bin/env python3
"""Generate a DON topic Evidence Doctor manifest from board bench logs.

本脚本只理解 `bench_don.cpp` 的日志格式：一条 `don_normal_pair,...`
计时行和一条 `Checksum:` 行。它把 topic-specific（当前主题特定）
日志翻译成 Evidence Doctor（证据体检）可读的通用 manifest。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROW_RE = re.compile(r"^don_normal_pair,points=(\d+)\s*:\s*([0-9]+(?:\.[0-9]+)?)\s+us/iter\s*$", re.MULTILINE)
CHECKSUM_RE = re.compile(r"^\s*Checksum:\s*(\d+)\s*$", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations:\s*(\d+)\s*$", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup Iterations:\s*(\d+)\s*$", re.MULTILINE)


def read_run(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8")
    row = ROW_RE.search(text)
    checksum = CHECKSUM_RE.search(text)
    iterations = ITER_RE.search(text)
    warmup = WARMUP_RE.search(text)
    if not row or not checksum:
        raise SystemExit(f"cannot parse DON bench log: {path}")
    return {
        "path": str(path),
        "points": int(row.group(1)),
        "avg_us": float(row.group(2)),
        "checksum": checksum.group(1),
        "iterations": int(iterations.group(1)) if iterations else None,
        "warmup_iterations": int(warmup.group(1)) if warmup else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", required=True, type=Path)
    parser.add_argument("--rvv-log", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", default="Milkv-Jupiter")
    parser.add_argument("--vlen", default="see SoC / ELF (board)")
    args = parser.parse_args()

    std = read_run(args.std_log)
    rvv = read_run(args.rvv_log)
    if std["points"] != rvv["points"]:
      raise SystemExit("Std/RVV point counts differ")

    iterations = std["iterations"] or rvv["iterations"]
    warmup = std["warmup_iterations"] or rvv["warmup_iterations"]
    speedup = float(std["avg_us"]) / float(rvv["avg_us"])
    asm_lines = 0
    if args.asm.exists():
        asm_lines = sum(1 for line in args.asm.read_text(encoding="utf-8", errors="ignore").splitlines()
                        if any(op in line for op in ("vlse32", "vfsub", "vfmul", "vfmacc", "vfsqrt", "vsse32")))

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "DON diagnostic board smoke",
            "evidence_role": "diagnostic",
            "summary_path": str(args.summary),
            "label": "DON diagnostic board smoke",
            "analysis_script": "test-rvv/features/don/script/generate_don_evidence_manifest.py",
            "metadata": {
                "device": args.device,
                "iterations": iterations,
                "warmup_iterations": warmup,
                "run_count": 1,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": args.vlen,
                "binary_hash": "not_recorded",
            },
        },
        "checks": {"strict_ab": False},
        "comparisons": [
            {
                "name": f"don_normal_pair points={std['points']}",
                "group": "don-normal-pair-diagnostic",
                "evidence_role": "diagnostic",
                "case_kind": "component_ablation",
                "point_type": "pcl::Normal",
                "size": std["points"],
                "run_count": 1,
                "ba_values": [speedup],
                "baseline": {
                    "label": "Std build",
                    "boundary": "test_support_helper",
                    "wrapper": "computeDoNRVV",
                    "row_source": "ordered_normal_cloud",
                    "solve": False,
                    "checksum_policy": "output_normals_and_curvature",
                    "checksum": std["checksum"],
                    "timer_boundary": "helper_only_no_setup",
                    "gate": "same_input_size",
                    "mask": "finite_normal_output_zeroing",
                    "reduction": "not_applicable",
                    "asm_boundary": "std_build_no_rvv",
                    "rvv_instr_count": 0,
                    "std_ms": float(std["avg_us"]) * 0.001,
                    "rvv_ms": float(std["avg_us"]) * 0.001,
                },
                "candidate": {
                    "label": "RVV build",
                    "boundary": "test_support_helper",
                    "wrapper": "computeDoNRVV",
                    "row_source": "ordered_normal_cloud",
                    "solve": False,
                    "checksum_policy": "output_normals_and_curvature",
                    "checksum": rvv["checksum"],
                    "timer_boundary": "helper_only_no_setup",
                    "gate": "same_input_size",
                    "mask": "finite_normal_output_zeroing",
                    "reduction": "not_applicable",
                    "asm_boundary": str(args.asm),
                    "rvv_instr_count": asm_lines,
                    "std_ms": float(rvv["avg_us"]) * 0.001,
                    "rvv_ms": float(rvv["avg_us"]) * 0.001,
                },
            }
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
