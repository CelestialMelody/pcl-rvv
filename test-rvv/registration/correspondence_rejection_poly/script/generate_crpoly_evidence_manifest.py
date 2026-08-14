#!/usr/bin/env python3
"""
生成 correspondence_rejection_poly 的 QEMU smoke Evidence Doctor manifest（证据清单）。

这个 wrapper（主题本地包装脚本）只解析当前 topic 的 bench smoke 日志。它补齐
case kind（用例形态）、row source（行来源）、timer boundary（计时边界）和
checksum（校验和）metadata，让通用 Evidence Doctor（证据体检）能检查证据角色。
QEMU timing（QEMU 计时）只保留为 build / log-shape smoke（构建和日志形状验证），
不能写成性能结论。
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


CASE_LABELS: dict[str, dict[str, Any]] = {
    "edge-batch candidate 64K": {
        "case_kind": "qemu_smoke_only",
        "candidate_family": "edge_length_batch",
        "row_source": "correspondences",
        "timer_boundary": "edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 65536,
        "mask": "edge_similarity_threshold",
        "reduction": "not_applicable",
    },
    "edge-batch candidate 256K": {
        "case_kind": "qemu_smoke_only",
        "candidate_family": "edge_length_batch",
        "row_source": "correspondences",
        "timer_boundary": "edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 262144,
        "mask": "edge_similarity_threshold",
        "reduction": "not_applicable",
    },
    "edge-gather-staging candidate 64K": {
        "case_kind": "production_shaped_qemu_smoke_only",
        "candidate_family": "edge_gather_staging",
        "row_source": "correspondences",
        "timer_boundary": "correspondence_index_gather_squared_distance_staging_edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 65536,
        "mask": "edge_similarity_threshold",
        "reduction": "scalar_gather_staging_then_rvv_formula",
    },
    "edge-gather-staging candidate 256K": {
        "case_kind": "production_shaped_qemu_smoke_only",
        "candidate_family": "edge_gather_staging",
        "row_source": "correspondences",
        "timer_boundary": "correspondence_index_gather_squared_distance_staging_edge_similarity_candidate_and_checksum",
        "checksum_policy": "fnv1a_edge_accept_mask",
        "size": 262144,
        "mask": "edge_similarity_threshold",
        "reduction": "scalar_gather_staging_then_rvv_formula",
    },
    "accept-rate filter candidate 64K": {
        "case_kind": "qemu_smoke_only",
        "candidate_family": "accept_rate_filter",
        "row_source": "contiguous_counters",
        "timer_boundary": "accept_rate_filter_and_checksum",
        "checksum_policy": "scaled_accept_rate_and_kept_index_fnv1a",
        "size": 65536,
        "mask": "num_samples_nonzero_and_accept_rate_cut",
        "reduction": "scalar_tail_output_append",
    },
    "accept-rate filter candidate 256K": {
        "case_kind": "qemu_smoke_only",
        "candidate_family": "accept_rate_filter",
        "row_source": "contiguous_counters",
        "timer_boundary": "accept_rate_filter_and_checksum",
        "checksum_policy": "scaled_accept_rate_and_kept_index_fnv1a",
        "size": 262144,
        "mask": "num_samples_nonzero_and_accept_rate_cut",
        "reduction": "scalar_tail_output_append",
    },
    "fixed-seed public entry 512 correspondences": {
        "case_kind": "public_entry_shaped_smoke",
        "candidate_family": "full_entry_diagnostic",
        "row_source": "correspondences",
        "timer_boundary": "full_get_remaining_correspondences_and_checksum",
        "checksum_policy": "kept_query_indices_fnv1a",
        "size": 512,
        "mask": "production_threshold_polygon_and_otsu",
        "reduction": "histogram_otsu_scalar",
    },
    "production-direct public entry 2048 correspondences": {
        "case_kind": "production_direct_qemu_smoke_only",
        "candidate_family": "production_edge_batch_rvv",
        "row_source": "correspondences",
        "timer_boundary": "production_get_remaining_correspondences_dispatch_and_checksum",
        "checksum_policy": "kept_query_match_indices_fnv1a",
        "size": 2048,
        "mask": "production_threshold_polygon_rvv_and_otsu",
        "reduction": "rvv_edge_predicate_then_scalar_polygon_histogram_otsu",
    },
    "production-direct public entry 8192 correspondences": {
        "case_kind": "production_direct_qemu_smoke_only",
        "candidate_family": "production_edge_batch_rvv",
        "row_source": "correspondences",
        "timer_boundary": "production_get_remaining_correspondences_dispatch_and_checksum",
        "checksum_policy": "kept_query_match_indices_fnv1a",
        "size": 8192,
        "mask": "production_threshold_polygon_rvv_and_otsu",
        "reduction": "rvv_edge_predicate_then_scalar_polygon_histogram_otsu",
    },
}


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


def make_comparison(name: str, std: dict[str, Any], rvv: dict[str, Any]) -> dict[str, Any]:
    if name not in CASE_LABELS:
        raise SystemExit(f"unknown correspondence_rejection_poly case label: {name}")
    label = CASE_LABELS[name]
    std_case = std["cases"][name]
    rvv_case = rvv["cases"][name]
    if label["candidate_family"] == "full_entry_diagnostic":
        boundary = "real_public_entry_no_rvv_dispatch"
    elif label["candidate_family"] == "production_edge_batch_rvv":
        boundary = "real_public_entry_production_dispatch"
    else:
        boundary = "test_support_candidate_wrapper"
    wrapper = "bench_correspondence_rejection_poly"

    return {
        "name": name,
        "group": label["candidate_family"],
        "evidence_role": "diagnostic",
        "case_kind": label["case_kind"],
        "point_type": "PointXYZ",
        "size": label["size"],
        "candidate_family": label["candidate_family"],
        "row_source": label["row_source"],
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
            "row_source": label["row_source"],
            "solve": False,
            "checksum_policy": label["checksum_policy"],
            "checksum": std_case.get("checksum"),
            "timer_boundary": label["timer_boundary"],
            "gate": "test_support_candidate_or_scalar_public_entry",
            "mask": label["mask"],
            "reduction": label["reduction"],
            "asm_boundary": "not_applicable_std_build",
            "std_ms": std_case["ms"],
        },
        "candidate": {
            "label": "RVV build",
            "boundary": boundary,
            "wrapper": wrapper,
            "row_source": label["row_source"],
            "solve": False,
            "checksum_policy": label["checksum_policy"],
            "checksum": rvv_case.get("checksum"),
            "timer_boundary": label["timer_boundary"],
            "gate": "test_support_candidate_or_scalar_public_entry",
            "mask": label["mask"],
            "reduction": label["reduction"],
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
            "title": "correspondence_rejection_poly QEMU smoke diagnostic",
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
    parser.add_argument("--asm", default="build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm")
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
