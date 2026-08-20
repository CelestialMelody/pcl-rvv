#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for marching_cubes_rbf board logs.

本脚本把 topic-local board bench 日志转换成 Evidence Doctor（证据体检）
可读取的 JSON manifest。`mcrbf_prod_*` 表示 production direct
（真实生产入口证据）；其它历史 case 仍是 component ablation（组件消融）诊断边界。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ITER_RE = re.compile(r"Iterations:\s*(\d+),\s*Warmup:\s*(\d+)")
ROW_RE = re.compile(r"^(mcrbf_[A-Za-z0-9_]+)\s*:\s*([0-9]+(?:\.[0-9]+)?)\s*ms/iter")
TOTAL_RE = re.compile(r"Total Time:\s*([0-9]+(?:\.[0-9]+)?)\s*ms,\s*checksum:\s*([0-9]+)")


def parse_log(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    iter_match = ITER_RE.search(text)
    iterations = int(iter_match.group(1)) if iter_match else None
    warmup = int(iter_match.group(2)) if iter_match else None
    rows: dict[str, dict] = {}
    lines = text.splitlines()
    for index, line in enumerate(lines):
        row_match = ROW_RE.match(line)
        if not row_match:
            continue
        label = row_match.group(1)
        avg_ms = float(row_match.group(2))
        total_ms = None
        checksum = ""
        if index + 1 < len(lines):
            total_match = TOTAL_RE.search(lines[index + 1])
            if total_match:
                total_ms = float(total_match.group(1))
                checksum = total_match.group(2)
        rows[label] = {
            "avg_ms": avg_ms,
            "total_ms": total_ms,
            "checksum": checksum,
        }
    return {"iterations": iterations, "warmup_iterations": warmup, "rows": rows}


def is_production_case(label: str) -> bool:
    return label.startswith("mcrbf_prod_")


def point_type_for(label: str) -> str:
    if label.startswith("mcrbf_prod_pointnormal_"):
        return "pcl::PointNormal"
    if label.startswith("mcrbf_prod_pointxyzinormal_"):
        return "pcl::PointXYZINormal"
    if label.startswith("mcrbf_prod_pointxyzrgbnormal_"):
        return "pcl::PointXYZRGBNormal"
    return "synthetic_rbf_centers"


def metadata_for(label: str, side: dict, impl: str) -> dict:
    full_pipeline = "full_pipeline" in label
    production = is_production_case(label)
    return {
        "label": impl,
        "boundary": "public_overload_voxelizeData" if production else "test_helper_component_ablation",
        "wrapper": "bench_marching_cubes_rbf",
        "row_source": "single_input_cloud_off_surface_pair_expansion",
        "solve": full_pipeline or production,
        "checksum_policy": "grid_sign_fingerprint" if production else "quantized_1e-6_matrix_sum_or_grid_sign_fingerprint",
        "checksum": side["checksum"],
        "timer_boundary": (
            "production_voxelizeData_including_matrix_solve_grid"
            if production else
            ("full_pipeline_including_solve" if full_pipeline else "matrix_fill_only")
        ),
        "gate": "exact_PointNormal_N_ge_16_RVV_else_scalar" if production else "same_synthetic_rbf_input",
        "mask": "not_applicable",
        "reduction": (
            "production_vector_reduction" if impl == "rvv-candidate" and production
            else ("vector_reduction_candidate" if impl == "rvv-candidate" and full_pipeline else "scalar_or_component_sum")
        ),
        "asm_boundary": (
            "MarchingCubesRBF voxelizeData RVV helper"
            if production else "bench_marching_cubes_rbf_rvv inline helper"
        ),
        "std_ms": side["avg_ms"] if impl == "std-reference" else None,
        "rvv_ms": side["avg_ms"] if impl == "rvv-candidate" else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", required=True, type=Path)
    parser.add_argument("--rvv-log", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", default="Milkv-Jupiter")
    args = parser.parse_args()

    std = parse_log(args.std_log)
    rvv = parse_log(args.rvv_log)
    comparisons = []
    for label in sorted(std["rows"]):
        if label not in rvv["rows"]:
            continue
        std_row = std["rows"][label]
        rvv_row = rvv["rows"][label]
        speedup = std_row["avg_ms"] / rvv_row["avg_ms"] if rvv_row["avg_ms"] else 0.0
        production = is_production_case(label)
        comparisons.append({
            "name": label,
            "group": "production-public" if production else ("matrix-only" if "matrix_fill" in label else "full-pipeline"),
            "evidence_role": "production_direct" if production else "component_ablation",
            "case_kind": "production direct" if production else ("production-shaped diagnostic" if "full_pipeline" in label else "component diagnostic"),
            "point_type": point_type_for(label),
            "size": label,
            "run_count": 1,
            "baseline": metadata_for(label, std_row, "std-reference"),
            "candidate": metadata_for(label, rvv_row, "rvv-candidate"),
            "ba_values": [speedup],
        })

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "marching_cubes_rbf board diagnostic and production direct compare",
            "evidence_role": "mixed_component_and_production_direct",
            "summary_path": str(args.summary),
            "label": "single board compare with per-case iterations",
            "analysis_script": "test-rvv/surface/marching_cubes_rbf/script/generate_marching_cubes_rbf_evidence_manifest.py",
            "metadata": {
                "device": args.device,
                "iterations": std["iterations"],
                "warmup_iterations": std["warmup_iterations"],
                "run_count": 1,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
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
            ],
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
