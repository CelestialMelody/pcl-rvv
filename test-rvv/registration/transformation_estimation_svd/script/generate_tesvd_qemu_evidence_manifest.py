#!/usr/bin/env python3
"""
生成 transformation_estimation_svd 的 QEMU smoke Evidence Doctor manifest（证据清单）。

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
INDEXED_DATASET_RE = re.compile(r"^Indexed Dataset:\s*(?P<value>.+?)\s*$")
DUAL_INDEXED_DATASET_RE = re.compile(r"^Dual-Indexed Dataset:\s*(?P<value>.+?)\s*$")
CORRESPONDENCE_DATASET_RE = re.compile(r"^Correspondence Dataset:\s*(?P<value>.+?)\s*$")
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")


def canonical_case_name(name: str) -> str:
    if name.startswith("public Umeyama full-cloud "):
        return name.replace("public Umeyama full-cloud ", "public Umeyama ordered-cloud-pair ", 1)
    if name.startswith("fused accum candidate full-cloud "):
        return name.replace(
            "fused accum candidate full-cloud ",
            "fused accum candidate ordered-cloud-pair ",
            1,
        )
    return name


def row_source_key(name: str) -> str:
    if "dual-indices-cloud-pair" in name:
        return "dual_indices_cloud_pair"
    if "correspondence-pair" in name:
        return "correspondence_pair"
    if "source-indexed-cloud-pair" in name:
        return "source_indexed_cloud_pair"
    return "ordered_cloud_pair"


def dataset_for_row_sources(metadata: dict[str, Any], row_sources: set[str]) -> str | None:
    labels: list[str] = []

    def add(label: Any) -> None:
        if label and label not in labels:
            labels.append(str(label))

    if "ordered_cloud_pair" in row_sources:
        add(metadata.get("dataset"))
    if "source_indexed_cloud_pair" in row_sources:
        add(metadata.get("indexed_dataset"))
    if "dual_indices_cloud_pair" in row_sources:
        add(metadata.get("dual_indexed_dataset"))
    if "correspondence_pair" in row_sources:
        add(metadata.get("correspondence_dataset"))
    if labels:
        return "; ".join(labels)
    return (
        metadata.get("dataset")
        or metadata.get("indexed_dataset")
        or metadata.get("dual_indexed_dataset")
        or metadata.get("correspondence_dataset")
    )


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
        if match := INDEXED_DATASET_RE.match(line):
            metadata["indexed_dataset"] = match.group("value")
            continue
        if match := DUAL_INDEXED_DATASET_RE.match(line):
            metadata["dual_indexed_dataset"] = match.group("value")
            continue
        if match := CORRESPONDENCE_DATASET_RE.match(line):
            metadata["correspondence_dataset"] = match.group("value")
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


def parse_compare(path: Path) -> dict[str, dict[str, float]]:
    rows: dict[str, dict[str, float]] = {}
    current_name = ""
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = [part.strip() for part in raw_line.split("|")]
        if len(parts) != 5:
            continue
        name, impl, avg_text, _total_text, speedup_text = parts
        if name.startswith("fused accum candidate"):
            current_name = name
        elif current_name:
            name = current_name
        if impl not in {"Std", "RVV"}:
            continue
        name = canonical_case_name(name)
        avg_match = re.search(r"([0-9.]+)\s+ms", avg_text)
        speedup_match = re.search(r"\[\s*([0-9.]+)x\s*\]", speedup_text)
        if not avg_match or not speedup_match:
            continue
        row = rows.setdefault(name, {})
        if impl == "Std":
            row["std_ms"] = float(avg_match.group(1))
        else:
            row["rvv_ms"] = float(avg_match.group(1))
            row["speedup"] = float(speedup_match.group(1))
    return rows


def size_for_case(name: str) -> int:
    if "256K" in name:
        return 256 * 1024
    if "64K" in name:
        return 64 * 1024
    if "4K" in name:
        return 4 * 1024
    return 0


def row_source_for_case(name: str) -> str:
    if "dual-indices-cloud-pair" in name:
        return "dual_indices_cloud_pair"
    if "correspondence-pair" in name:
        return "correspondence_pair"
    if "source-indexed-cloud-pair" in name:
        return "source_indexed_cloud_pair"
    return "ordered_cloud_pair"


def is_public_case(name: str) -> bool:
    return name.startswith("public Umeyama ")


def boundary_for_case(name: str) -> str:
    row_source = row_source_for_case(name)
    if is_public_case(name):
        return f"public_umeyama_{row_source}_smoke"
    return f"test_support_fused_{row_source}_candidate"


def timer_boundary_for_case(name: str) -> str:
    if is_public_case(name):
        return "current_public_umeyama_estimate_call"
    row_source = row_source_for_case(name)
    if row_source == "source_indexed_cloud_pair":
        return "fused_source_indexed_accumulation_plus_eigen_3x3_svd"
    if row_source == "dual_indices_cloud_pair":
        return "fused_dual_indices_accumulation_plus_eigen_3x3_svd"
    if row_source == "correspondence_pair":
        return "fused_correspondence_accumulation_plus_eigen_3x3_svd"
    return "fused_accumulation_plus_eigen_3x3_svd"


def gate_for_case(name: str) -> str:
    if is_public_case(name):
        return "public_entry_size_match_or_build_specific_dispatch"
    row_source = row_source_for_case(name)
    if row_source == "source_indexed_cloud_pair":
        return "dense_source_indexed_cloud_pair_float_xyz_aos_valid_indices_or_scalar_fallback"
    if row_source == "dual_indices_cloud_pair":
        return "dense_dual_indices_cloud_pair_float_xyz_aos_valid_indices_or_scalar_fallback"
    if row_source == "correspondence_pair":
        return "dense_correspondence_pair_float_xyz_aos_valid_correspondences_or_scalar_fallback"
    return "dense_ordered_cloud_pair_float_xyz_aos_or_scalar_fallback"


def reduction_for_case(name: str) -> str:
    if is_public_case(name):
        return "current_build_public_path"
    row_source = row_source_for_case(name)
    if row_source == "source_indexed_cloud_pair":
        return "rvv_source_indexed_sum_tree_or_scalar_same_chain"
    if row_source == "dual_indices_cloud_pair":
        return "rvv_dual_indices_sum_tree_or_scalar_same_chain"
    if row_source == "correspondence_pair":
        return "rvv_correspondence_sum_tree_or_scalar_same_chain"
    return "rvv_ordered_sum_tree_or_scalar_same_chain"


def make_side(name: str, case: dict[str, Any], build: str) -> dict[str, Any]:
    return {
        "label": build,
        "boundary": boundary_for_case(name),
        "wrapper": "bench_tesvd_public_entry" if is_public_case(name) else "bench_tesvd",
        "row_source": row_source_for_case(name),
        "solve": True,
        "checksum_policy": "path_fingerprint_only_correctness_covered_by_gtest_matrix_budget",
        "log_checksum": case.get("checksum"),
        "timer_boundary": timer_boundary_for_case(name),
        "gate": gate_for_case(name),
        "mask": "not_applicable_dense_only",
        "reduction": reduction_for_case(name),
        "asm_boundary": "not_attributed_qemu_public_smoke"
        if is_public_case(name)
        else "bench_binary_test_support_helper",
    }


def make_comparison(name: str,
                    std: dict[str, Any],
                    rvv: dict[str, Any],
                    compare_rows: dict[str, dict[str, float]]) -> dict[str, Any]:
    std_case = std["cases"][name]
    rvv_case = rvv["cases"][name]
    row = compare_rows.get(name, {})
    std_ms = row.get("std_ms", std_case["ms"])
    rvv_ms = row.get("rvv_ms", rvv_case["ms"])
    return {
        "name": name,
        "group": "qemu-smoke",
        "evidence_role": "diagnostic",
        "case_kind": "qemu_smoke_only",
        "row_source": row_source_for_case(name),
        "point_type": "PointXYZ",
        "scalar": "float",
        "size": size_for_case(name),
        "run_count": 1,
        "std_ms": std_ms,
        "rvv_ms": rvv_ms,
        "notes": (
            "QEMU smoke timing is recorded only to prove build/log shape; "
            "performance conclusion requires target hardware. ba_values are intentionally "
            "omitted so Evidence Doctor does not treat QEMU timing as repeated performance evidence."
        ),
        "baseline": make_side(name, std_case, "Std build"),
        "candidate": make_side(name, rvv_case, "RVV build"),
    }


def build_manifest(std_log: Path, rvv_log: Path, summary_path: Path, asm_path: Path) -> dict[str, Any]:
    std = parse_log(std_log)
    rvv = parse_log(rvv_log)
    compare_rows = parse_compare(summary_path)
    missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
    if missing:
        raise SystemExit("Std/RVV case set mismatch: " + ", ".join(missing))

    row_sources = {row_source_for_case(name) for name in std["cases"]}
    dataset = dataset_for_row_sources(std["metadata"], row_sources) or dataset_for_row_sources(
        rvv["metadata"], row_sources
    )

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
        "dataset": dataset,
    }

    return {
        "schema_version": 1,
        "summary": {
            "title": "transformation_estimation_svd QEMU smoke diagnostic",
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
            "summary_path": str(summary_path),
            "asm_path": str(asm_path),
        },
        "comparisons": [
            make_comparison(name, std, rvv, compare_rows) for name in sorted(std["cases"])
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", default="log/qemu/run_bench_std.log")
    parser.add_argument("--rvv-log", default="log/qemu/run_bench_rvv.log")
    parser.add_argument("--summary-md", default="log/qemu/analyze_bench_compare.log")
    parser.add_argument("--asm", default="build/asm/riscv/bench_transformation_estimation_svd_rvv.asm")
    parser.add_argument("--output", default="log/qemu/evidence_manifest.json")
    args = parser.parse_args()

    manifest = build_manifest(
        Path(args.std_log), Path(args.rvv_log), Path(args.summary_md), Path(args.asm)
    )
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
