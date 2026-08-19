#!/usr/bin/env python3
"""生成 ndt topic 的 QEMU smoke Evidence Doctor manifest（证据清单）。"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"^checksum:\s*(?P<checksum>[0-9]+)\s*$")
INT_RE = re.compile(
    r"^(?P<key>Iterations|Warmup Iterations|Samples|Public Points|Public Max Iterations):\s*(?P<value>[0-9]+)\s*$"
)
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")


def parse_log(path: Path) -> dict[str, Any]:
    metadata: dict[str, Any] = {}
    cases: dict[str, dict[str, Any]] = {}
    current_case: str | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := INT_RE.match(line):
            key = match.group("key").lower().replace(" ", "_")
            metadata[key] = int(match.group("value"))
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
    return {"metadata": metadata, "cases": cases}


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip())


def case_kind(name: str) -> str:
    if "production-public" in name:
        return "production_public_profile"
    if "hessian" in name:
        return "derivative_hessian_accumulation"
    if "gradient" in name:
        return "derivative_gradient_accumulation"
    return "ndt_derivative_accumulation"


def build_manifest(rvv_log: Path, asm_path: Path, args: argparse.Namespace) -> dict[str, Any]:
    parsed = parse_log(rvv_log)
    if not parsed["cases"]:
        raise SystemExit(f"no benchmark cases parsed from {rvv_log}")
    rvv_instr_count = count_rvv_asm_lines(asm_path)
    metadata = {
        "device": "QEMU rv64gcv smoke; not target hardware",
        "iterations": parsed["metadata"].get("iterations"),
        "warmup_iterations": parsed["metadata"].get("warmup_iterations"),
        "run_count": 1,
        "taskset": "not_applicable_qemu",
        "governor": "not_applicable_qemu",
        "freq": "not_applicable_qemu",
        "temperature": "not_recorded_qemu_smoke",
        "vlen": "256-bit qemu default from shared Makefile RUN_CMD",
        "binary_hash": "not_recorded_qemu_smoke",
        "dataset": parsed["metadata"].get("dataset"),
        "samples": parsed["metadata"].get("samples"),
        "public_points": parsed["metadata"].get("public_points"),
        "public_max_iterations": parsed["metadata"].get("public_max_iterations"),
        "build": parsed["metadata"].get("build"),
    }
    common = {
        "boundary": "test helper",
        "wrapper": "bench_ndt derivative accumulation diagnostic",
        "row_source": "staged point-neighbor samples",
        "solve": "excluded",
        "checksum_policy": "candidate result checksum in bench log",
        "timer_boundary": "pre-production staged math helper; excludes voxel neighbor search, point derivative precompute, line search and Eigen solver",
        "gate": "__RVV10__ build switch",
        "mask": "invalid gaussian weight samples keep production-style skip semantics",
        "reduction": "RVV vector reduction over staged samples",
        "asm_boundary": "bench_ndt_rvv filtered asm; helper may be inlined",
        "rvv_instr_count": rvv_instr_count,
    }
    return {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": "qemu_smoke_only",
            "summary_path": str(rvv_log),
            "label": "NDT derivative accumulation QEMU smoke; no performance conclusion",
            "metadata": metadata,
        },
        "checks": {"strict_ab": False},
        "artifacts": {
            "rvv_log": str(rvv_log),
            "asm_path": str(asm_path),
            "rvv_instr_count_filtered_binary": rvv_instr_count,
        },
        "comparisons": [
            {
                "name": name,
                "case_kind": case_kind(name),
                "evidence_role": "qemu_smoke_only",
                "observed_ms": case.get("observed_ms"),
                "log_checksum": case.get("log_checksum"),
                **(
                    {
                        **common,
                        "boundary": "public overload",
                        "wrapper": "bench_ndt production-public align probe",
                        "row_source": "real transformed source cloud and voxel neighborhood lookup",
                        "solve": "included",
                        "timer_boundary": "public align; QEMU log-shape smoke only, not performance evidence",
                        "reduction": "production scalar reduction order; no NDT RVV production patch",
                    }
                    if "production-public" in name
                    else common
                ),
                "notes": "QEMU timing is retained only for log-shape smoke and must not be used as performance evidence.",
            }
            for name, case in sorted(parsed["cases"].items())
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rvv-log", default="log/qemu/run_bench_all_rvv.log")
    parser.add_argument("--asm", default="build/asm/riscv/bench_ndt_rvv.asm")
    parser.add_argument("--output", default="log/qemu/evidence_manifest.json")
    parser.add_argument("--title", default="ndt QEMU derivative accumulation smoke")
    args = parser.parse_args()
    manifest = build_manifest(Path(args.rvv_log), Path(args.asm), args)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
