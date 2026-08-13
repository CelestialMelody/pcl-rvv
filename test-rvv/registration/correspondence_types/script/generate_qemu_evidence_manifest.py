#!/usr/bin/env python3
"""
生成 correspondence_types 的 QEMU smoke Evidence Doctor manifest（证据清单）。

这个脚本只解析当前 topic 的 QEMU bench smoke 日志。它补齐 case 名、
checksum（校验和）、运行参数和诊断边界 metadata，让通用 Evidence Doctor
能检查“证据角色是否完整”，但它不会把 QEMU timing（QEMU 计时）升级为性能证据。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")


def parse_log(path: Path) -> dict[str, Any]:
    cases: dict[str, dict[str, Any]] = {}
    current_case: str | None = None
    metadata: dict[str, Any] = {}

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
        if match := BUILD_RE.match(line):
            metadata["build"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None

    return {"path": str(path), "metadata": metadata, "cases": cases}


def case_row_source(name: str) -> str:
    return "correspondences"


def case_reduction(name: str) -> str:
    if "distance-stats" in name:
        return "float_square_same_chain_scalar_order"
    return "not_applicable_index_extract"


def case_timer_boundary(name: str) -> str:
    if "query+match" in name:
        return "two_candidate_calls_and_checksum"
    return "one_candidate_call_and_checksum"


def make_comparison(name: str, std: dict[str, Any], rvv: dict[str, Any]) -> dict[str, Any]:
    std_case = std["cases"][name]
    rvv_case = rvv["cases"][name]
    checksum_policy = "fnv1a_indices" if "index" in name or "query+match" in name else "scaled_mean_stddev"
    boundary = "test_support_candidate_wrapper"
    wrapper = "bench_correspondence_types"
    row_source = case_row_source(name)
    timer_boundary = case_timer_boundary(name)
    reduction = case_reduction(name)
    gate = "correspondence_layout_supported_or_scalar_fallback"
    mask = "not_applicable"

    return {
        "name": name,
        "group": "qemu-smoke",
        "evidence_role": "diagnostic",
        "case_kind": "qemu_smoke_only",
        "row_source": row_source,
        "run_count": 1,
        "std_ms": std_case["ms"],
        "rvv_ms": rvv_case["ms"],
        "notes": (
            "QEMU smoke timing is recorded only to prove build/log shape; "
            "it is not performance evidence."
        ),
        "baseline": {
            "label": "Std build",
            "boundary": boundary,
            "wrapper": wrapper,
            "row_source": row_source,
            "solve": False,
            "checksum_policy": checksum_policy,
            "checksum": std_case.get("checksum"),
            "timer_boundary": timer_boundary,
            "gate": gate,
            "mask": mask,
            "reduction": reduction,
            "asm_boundary": "not_applicable_std_build",
            "std_ms": std_case["ms"],
        },
        "candidate": {
            "label": "RVV build",
            "boundary": boundary,
            "wrapper": wrapper,
            "row_source": row_source,
            "solve": False,
            "checksum_policy": checksum_policy,
            "checksum": rvv_case.get("checksum"),
            "timer_boundary": timer_boundary,
            "gate": gate,
            "mask": mask,
            "reduction": reduction,
            "asm_boundary": "bench_binary_smoke_only",
            "rvv_ms": rvv_case["ms"],
        },
    }


def build_manifest(std_log: Path, rvv_log: Path, asm_path: Path, summary_path: Path) -> dict[str, Any]:
    std = parse_log(std_log)
    rvv = parse_log(rvv_log)
    missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
    if missing:
        raise SystemExit("Std/RVV case set mismatch: " + ", ".join(missing))

    metadata = {
        "device": "QEMU rv64gcv smoke; not target hardware",
        "iterations": std["metadata"].get("iterations"),
        "warmup_iterations": std["metadata"].get("warmup_iterations"),
        "run_count": 1,
        "taskset": "not_applicable_qemu",
        "governor": "not_applicable_qemu",
        "freq": "not_applicable_qemu",
        "temperature": "not_recorded_qemu_smoke",
        "vlen": "256-bit (qemu -cpu rv64,v=true,vlen=256,elen=64)",
        "binary_hash": "not_recorded_qemu_smoke",
        "dataset": std["metadata"].get("dataset") or rvv["metadata"].get("dataset"),
    }

    return {
        "schema_version": 1,
        "summary": {
            "title": "correspondence_types QEMU smoke diagnostic",
            "evidence_role": "diagnostic",
            "summary_path": str(summary_path),
            "label": "QEMU smoke diagnostic; no performance conclusion",
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": metadata,
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
        "artifacts": {
            "std_log": str(std_log),
            "rvv_log": str(rvv_log),
            "asm_path": str(asm_path),
        },
        "comparisons": [
            make_comparison(name, std, rvv) for name in sorted(std["cases"])
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", default="log/qemu/run_bench_std.log")
    parser.add_argument("--rvv-log", default="log/qemu/run_bench_rvv.log")
    parser.add_argument("--summary-md", default="log/qemu/analyze_bench_compare.log")
    parser.add_argument("--asm", default="build/asm/riscv/bench_correspondence_types_rvv.asm")
    parser.add_argument("--output", default="log/qemu/evidence_manifest.json")
    args = parser.parse_args()

    manifest = build_manifest(
        Path(args.std_log), Path(args.rvv_log), Path(args.asm), Path(args.summary_md)
    )
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
