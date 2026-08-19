#!/usr/bin/env python3
"""Generate a small evidence manifest for organized_fast_mesh topic-local runs.

这个脚本读取 std/RVV bench log，生成 Evidence Doctor（证据体检）能检查的
JSON manifest。它只保存 summary（摘要）级元数据，不保存私有板卡地址。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROW_RE = re.compile(r"^(ofm_[A-Za-z0-9_]+)\s*:\s*([0-9]+(?:\.[0-9]+)?)\s*ms/iter\s*$")
ITER_RE = re.compile(r"^Iterations:\s*(\d+)\s*$", re.MULTILINE)
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(\d+)\s*$", re.MULTILINE)
CHECKSUM_RE = re.compile(r"checksum:\s*(\d+)")


def parse_log(path: Path) -> tuple[dict[str, float], dict[str, int], int, int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    iterations = int(ITER_RE.search(text).group(1)) if ITER_RE.search(text) else 0
    warmup = int(WARMUP_RE.search(text).group(1)) if WARMUP_RE.search(text) else 0
    times: dict[str, float] = {}
    checksums: dict[str, int] = {}
    current_name = ""
    for line in text.splitlines():
        row = ROW_RE.match(line.strip())
        if row:
            current_name = row.group(1)
            times[current_name] = float(row.group(2))
            continue
        checksum = CHECKSUM_RE.search(line)
        if checksum and current_name:
            checksums[current_name] = int(checksum.group(1))
    return times, checksums, iterations, warmup


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", required=True)
    parser.add_argument("--std-log", required=True)
    parser.add_argument("--rvv-log", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--row-source", default="ordered-cloud-pair")
    parser.add_argument("--boundary", default="test helper")
    parser.add_argument("--wrapper", default="organized_fast_mesh")
    parser.add_argument("--gate", default="PointXYZ_float_organized_store_shadowed_faces_true")
    parser.add_argument("--point-type", default="PointXYZ")
    parser.add_argument("--size", default="organized")
    args = parser.parse_args()
    std_times, std_checksums, iterations, warmup = parse_log(Path(args.std_log))
    rvv_times, rvv_checksums, rvv_iterations, rvv_warmup = parse_log(Path(args.rvv_log))

    comparisons = []
    for name in sorted(set(std_times) & set(rvv_times)):
        std_ms = std_times[name]
        rvv_ms = rvv_times[name]
        speedup = std_ms / rvv_ms if rvv_ms else 0.0
        comparisons.append(
            {
                "name": name,
                "evidence_role": args.evidence_role,
                "point_type": args.point_type,
                "size": args.size,
                "iterations": iterations,
                "warmup_iterations": warmup,
                "run_count": 1,
                "std_ms": std_ms,
                "rvv_ms": rvv_ms,
                "ba_values": [speedup],
                "baseline": {
                    "boundary": args.boundary,
                    "wrapper": args.wrapper,
                    "row_source": args.row_source,
                    "solve": False,
                    "checksum_policy": "polygon_checksum",
                    "timer_boundary": "mesh_generation_only",
                    "gate": args.gate,
                    "mask": "finite_mask",
                    "reduction": "none",
                    "checksum": std_checksums.get(name),
                },
                "candidate": {
                    "boundary": args.boundary,
                    "wrapper": args.wrapper,
                    "row_source": args.row_source,
                    "solve": False,
                    "checksum_policy": "polygon_checksum",
                    "timer_boundary": "mesh_generation_only",
                    "gate": args.gate,
                    "mask": "finite_mask",
                    "reduction": "none",
                    "checksum": rvv_checksums.get(name),
                },
            }
        )

    payload = {
        "schema_version": 1,
        "evidence_role": args.evidence_role,
        "summary": {
            "title": "organized_fast_mesh",
            "summary_path": args.summary,
            "metadata": {
                "boundary": args.boundary,
                "wrapper": args.wrapper,
                "gate": args.gate,
                "row_source": args.row_source,
                "point_type": args.point_type,
                "size": args.size,
                "iterations": iterations,
                "warmup_iterations": warmup,
                "rvv_iterations": rvv_iterations,
                "rvv_warmup_iterations": rvv_warmup,
            },
        },
        "environment": {
            "device": "Milkv-Jupiter",
            "taskset": "not_recorded",
            "governor": "not_recorded",
            "freq": "not_recorded",
            "temperature": "not_recorded",
        },
        "comparisons": comparisons,
        "checks": {"strict_ab": False},
    }
    Path(args.manifest).parent.mkdir(parents=True, exist_ok=True)
    Path(args.manifest).write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
