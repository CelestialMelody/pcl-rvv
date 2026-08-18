#!/usr/bin/env python3
"""
生成 transformation_estimation_dual_quaternion 的 QEMU smoke Evidence Doctor manifest（证据清单）。

这个脚本只解析当前 topic 的 QEMU bench smoke 日志。它补齐 case label、checksum
（校验和）、运行参数和诊断边界 metadata，让通用 Evidence Doctor（证据体检）能检查
证据形状；它不会把 QEMU timing（QEMU 计时）升级为性能证据。
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


def size_for_case(name: str) -> int:
    if "256K" in name:
        return 256 * 1024
    if "64K" in name:
        return 64 * 1024
    if "4K" in name:
        return 4 * 1024
    return 0


def boundary_for_case(name: str) -> str:
    if name.startswith("public dual quaternion"):
        return f"production_public_{row_source_for_case(name).replace('-', '_')}_current_entry"
    if "correspondence-pair" in name:
        return "test_support_correspondence_pair_candidate"
    return "test_support_rvv_accumulation_candidate"


def row_source_for_case(name: str) -> str:
    for row_source in (
        "source-indexed-cloud-pair",
        "dual-indexed-cloud-pair",
        "correspondence-pair",
        "ordered-cloud-pair",
    ):
        if row_source in name:
            return row_source
    return "ordered-cloud-pair"


def point_type_for_case(name: str) -> str:
    for point_type in ("PointXYZRGB", "PointXYZI", "PointXYZ"):
        if point_type in name:
            return point_type
    return "PointXYZ"


def make_side(case: dict[str, Any], name: str, build: str) -> dict[str, Any]:
    row_source = row_source_for_case(name)
    point_type = point_type_for_case(name)
    return {
        "label": build,
        "boundary": boundary_for_case(name),
        "wrapper": "bench_tedq",
        "row_source": row_source,
        "solve": True,
        "checksum_policy": "path_fingerprint_only_correctness_covered_by_gtest_matrix_budget",
        "log_checksum": case.get("checksum"),
        "timer_boundary": "c1_c2_accumulation_plus_eigen_4x4_solve",
        "gate": f"dense_{row_source}_{point_type}_float_xyz_aos_or_scalar_fallback",
        "mask": "not_applicable_dense_only",
        "reduction": "rvv_f64_tree_or_scalar_same_chain",
        "asm_boundary": "bench_binary_test_support_helper",
    }


def make_comparison(name: str, std: dict[str, Any], rvv: dict[str, Any]) -> dict[str, Any]:
    std_case = std["cases"][name]
    rvv_case = rvv["cases"][name]
    return {
        "name": name,
        "group": "qemu-smoke",
        "evidence_role": "diagnostic",
        "case_kind": "qemu_smoke_only",
        "row_source": row_source_for_case(name),
        "point_type": point_type_for_case(name),
        "scalar": "float",
        "size": size_for_case(name),
        "run_count": 1,
        "std_ms": std_case["ms"],
        "rvv_ms": rvv_case["ms"],
        "notes": (
            "QEMU smoke timing is recorded only to prove build/log shape; "
            "performance conclusion requires target hardware. ba_values are intentionally "
            "omitted so Evidence Doctor does not treat QEMU timing as repeated performance evidence."
        ),
        "baseline": make_side(std_case, name, "Std build"),
        "candidate": make_side(rvv_case, name, "RVV build"),
    }


def build_manifest(std_log: Path, rvv_log: Path, asm_path: Path) -> dict[str, Any]:
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
            "title": "transformation_estimation_dual_quaternion QEMU smoke diagnostic",
            "evidence_role": "diagnostic",
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
        "comparisons": [make_comparison(name, std, rvv) for name in sorted(std["cases"])],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", default="log/qemu/run_bench_std.log")
    parser.add_argument("--rvv-log", default="log/qemu/run_bench_rvv.log")
    parser.add_argument("--asm", default="build/asm/riscv/bench_transformation_estimation_dual_quaternion_rvv.asm")
    parser.add_argument("--output", default="log/qemu/evidence_manifest.json")
    args = parser.parse_args()

    manifest = build_manifest(Path(args.std_log), Path(args.rvv_log), Path(args.asm))
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
