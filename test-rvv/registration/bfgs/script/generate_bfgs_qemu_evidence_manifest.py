#!/usr/bin/env python3
"""
生成 bfgs topic 的 QEMU smoke Evidence Doctor manifest（证据清单）。

本脚本只解析 RVV bench smoke 日志和 filtered asm（过滤后的反汇编）路径。
它把 case label、checksum（校验和）、运行参数、RVV 指令行数和 diagnostic
边界 metadata 写成通用 manifest，供全局 Evidence Doctor（证据体检）检查。
QEMU timing（QEMU 计时）只作为日志形状字段保留，不参与性能结论。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"^checksum:\s*(?P<checksum>[0-9]+)\s*$")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")


def parse_log(path: Path) -> dict[str, Any]:
    cases: dict[str, dict[str, Any]] = {}
    metadata: dict[str, Any] = {}
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
        if match := BUILD_RE.match(line):
            metadata["build"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"observed_ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.match(line)):
            cases[current_case]["log_checksum"] = match.group("checksum")
            current_case = None

    return {"path": str(path), "metadata": metadata, "cases": cases}


def vector_size_for_case(name: str) -> int:
    if "vector128" in name:
        return 128
    if "vector6" in name:
        return 6
    return 0


def case_kind_for_case(name: str) -> str:
    if name.startswith("direction-update"):
        return "direction_update_diagnostic"
    if name.startswith("move-to-slope"):
        return "move_to_slope_diagnostic"
    if name.startswith("gicp-shaped"):
        return "caller_shaped_smoke"
    return "qemu_smoke_only"


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip())


def make_comparison(name: str, case: dict[str, Any], args: argparse.Namespace, rvv_instr_count: int) -> dict[str, Any]:
    return {
        "name": name,
        "group": args.group,
        "evidence_role": args.comparison_evidence_role,
        "case_kind": case_kind_for_case(name),
        "row_source": "not_applicable_bfgs_state_vector",
        "point_type": "Vector6d" if "vector6" in name else "dynamic_double_vector",
        "scalar": "double",
        "size": vector_size_for_case(name),
        "run_count": 1,
        "observed_ms": case.get("observed_ms"),
        "log_checksum": case.get("log_checksum"),
        "checksum_policy": "fnv1a_over_candidate_result_only; rvv_use_recorded_by_build_metadata",
        "asm_boundary": "bench_binary_filtered_asm; helper may be inlined",
        "rvv_instr_count": rvv_instr_count,
        "notes": (
            "One RVV QEMU smoke case. It intentionally omits ba_values/std_ms/rvv_ms "
            "so Evidence Doctor will not treat QEMU timing as target-hardware performance evidence."
        ),
    }


def build_manifest(rvv_log: Path, asm_path: Path, args: argparse.Namespace) -> dict[str, Any]:
    rvv = parse_log(rvv_log)
    if not rvv["cases"]:
        raise SystemExit(f"no benchmark cases parsed from {rvv_log}")

    rvv_instr_count = count_rvv_asm_lines(asm_path)
    metadata = {
        "device": "QEMU rv64gcv smoke; not target hardware",
        "iterations": rvv["metadata"].get("iterations"),
        "warmup_iterations": rvv["metadata"].get("warmup_iterations"),
        "run_count": 1,
        "taskset": "not_applicable_qemu",
        "governor": "not_applicable_qemu",
        "freq": "not_applicable_qemu",
        "temperature": "not_recorded_qemu_smoke",
        "vlen": "256-bit qemu default from shared Makefile RUN_CMD",
        "binary_hash": "not_recorded_qemu_smoke",
        "dataset": rvv["metadata"].get("dataset"),
        "build": rvv["metadata"].get("build"),
    }

    return {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": args.summary_evidence_role,
            "summary_path": str(rvv_log),
            "label": args.label,
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": metadata,
        },
        "checks": {
            "strict_ab": False,
        },
        "artifacts": {
            "rvv_log": str(rvv_log),
            "asm_path": str(asm_path),
            "rvv_instr_count_filtered_binary": rvv_instr_count,
        },
        "comparisons": [
            make_comparison(name, case, args, rvv_instr_count)
            for name, case in sorted(rvv["cases"].items())
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rvv-log", default="log/qemu/run_bench_direction_update_rvv.log")
    parser.add_argument("--asm", default="build/asm/riscv/bench_bfgs_rvv.asm")
    parser.add_argument("--output", default="log/qemu/evidence_manifest.json")
    parser.add_argument("--title", default="bfgs QEMU smoke diagnostic")
    parser.add_argument("--label", default="BFGS diagnostic QEMU smoke; no performance conclusion")
    parser.add_argument("--summary-evidence-role", default="qemu_smoke_only")
    parser.add_argument("--comparison-evidence-role", default="qemu_smoke_only")
    parser.add_argument("--group", default="bfgs_qemu_direction_update_smoke")
    args = parser.parse_args()

    manifest = build_manifest(Path(args.rvv_log), Path(args.asm), args)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
