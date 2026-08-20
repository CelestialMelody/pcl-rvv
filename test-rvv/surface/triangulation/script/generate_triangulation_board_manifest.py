#!/usr/bin/env python3
"""Generate Evidence Doctor manifests for triangulation board logs.

本脚本只服务 test-rvv/surface/triangulation 的接入前诊断证据。它从
archived board 目录中的 run_bench_std.log / run_bench_rvv.log 解析
Std-vs-RVV case，并补齐 Evidence Doctor 需要的 A/B 边界字段。目录可以是
单次 smoke，也可以是包含 run_1..run_N 子目录的 repeated board 归档。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_META: dict[str, dict[str, Any]] = {
    "tri_param_grid_512": {
        "group": "triangulation-grid-sampling",
        "size": 512,
        "point_type": "PointXYZ",
        "timer_boundary": "param_grid_only",
        "row_source": "regular_uv_grid",
        "checksum_policy": "mesh_stats_checksum",
        "case_kind": "pre-production diagnostic",
        "notes": [
            "测试专用参数网格 helper；不命中 production on_nurbs 符号。",
            "B/A 口径为 Std 平均耗时 / RVV 平均耗时，只用于接入前方向判断。",
        ],
    },
    "tri_surface_eval_256": {
        "group": "triangulation-grid-sampling",
        "size": 256,
        "point_type": "PointXYZ",
        "timer_boundary": "param_grid_plus_evaluate_like_sink",
        "row_source": "regular_uv_grid",
        "checksum_policy": "mesh_stats_checksum",
        "case_kind": "pre-production diagnostic",
        "notes": [
            "测试专用 Evaluate-like scalar sink；不能替代真实 ON_NurbsSurface::Evaluate direct 证据。",
            "B/A 口径为 Std 平均耗时 / RVV 平均耗时，只用于接入前方向判断。",
        ],
    },
}


def parse_log(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    result: dict[str, Any] = {"path": str(path)}

    for key, pattern in (
        ("dataset", r"^Dataset:\s*(.+)$"),
        ("iterations", r"^Iterations:\s*(\d+)\s*$"),
        ("warmup_iterations", r"^Warmup Iterations:\s*(\d+)\s*$"),
        ("build", r"^Build:\s*(.+)$"),
    ):
        match = re.search(pattern, text, re.MULTILINE)
        if match:
            value: str = match.group(1).strip()
            result[key] = int(value) if value.isdigit() else value

    case_match = re.search(r"^(tri_[A-Za-z0-9_]+)\s*:\s*([0-9.]+)\s*ms/iter", text, re.MULTILINE)
    if not case_match:
        raise ValueError(f"cannot parse benchmark case from {path}")
    result["case"] = case_match.group(1)
    result["avg_ms"] = float(case_match.group(2))

    detail_match = re.search(
        r"Total Time:\s*([0-9.]+)\s*ms,\s*checksum:\s*([0-9]+),\s*points:\s*([0-9]+),\s*polygons:\s*([0-9]+)",
        text,
    )
    if not detail_match:
        raise ValueError(f"cannot parse detail line from {path}")
    result["total_ms"] = float(detail_match.group(1))
    result["checksum"] = detail_match.group(2)
    result["points"] = int(detail_match.group(3))
    result["polygons"] = int(detail_match.group(4))
    return result


def side(label: str, parsed: dict[str, Any], meta: dict[str, Any]) -> dict[str, Any]:
    is_rvv = "RVV" in str(parsed.get("build", ""))
    result: dict[str, Any] = {
        "label": label,
        "boundary": "test_helper",
        "wrapper": "bench_triangulation",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": meta["checksum_policy"],
        "checksum": parsed["checksum"],
        "timer_boundary": meta["timer_boundary"],
        "gate": "__RVV10__" if is_rvv else "scalar_reference",
        "mask": "not_applicable",
        "reduction": "not_applicable",
        "asm_boundary": "test-rvv/surface/triangulation/include/triangulation.h::createParamGridCandidate"
        if is_rvv
        else "test-rvv/surface/triangulation/include/triangulation.h::createParamGridReference",
        "rvv_instr_count": 5 if is_rvv else 0,
    }
    if is_rvv:
        result["rvv_ms"] = parsed["avg_ms"]
    else:
        result["std_ms"] = parsed["avg_ms"]
    return result


def generate_manifest(log_dir: Path) -> dict[str, Any]:
    repeated_runs = sorted(path for path in log_dir.glob("run_*") if path.is_dir())
    if repeated_runs:
        return generate_repeated_manifest(log_dir, repeated_runs)
    return generate_single_manifest(log_dir)


def generate_single_manifest(log_dir: Path) -> dict[str, Any]:
    std = parse_log(log_dir / "run_bench_std.log")
    rvv = parse_log(log_dir / "run_bench_rvv.log")
    if std["case"] != rvv["case"]:
        raise ValueError(f"case mismatch: std={std['case']} rvv={rvv['case']}")
    if std.get("iterations") != rvv.get("iterations") or std.get("warmup_iterations") != rvv.get("warmup_iterations"):
        raise ValueError("run contract mismatch between std and rvv logs")

    case = str(std["case"])
    if case not in CASE_META:
        raise ValueError(f"unknown triangulation case: {case}")
    meta = CASE_META[case]
    speedup = std["avg_ms"] / rvv["avg_ms"]

    return {
        "schema_version": 1,
        "summary": {
            "title": f"triangulation {case} board smoke",
            "evidence_role": "diagnostic",
            "summary_path": str(log_dir / "analyze_bench_compare.log"),
            "label": "diagnostic board smoke",
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": {
                "device": "Milkv-Jupiter",
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
                "mask",
                "reduction",
            ],
        },
        "comparisons": [
            {
                "name": case,
                "group": meta["group"],
                "evidence_role": "diagnostic",
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": 1,
                "iterations": std["iterations"],
                "warmup_iterations": std["warmup_iterations"],
                "build_comparison": True,
                "std_ms": std["avg_ms"],
                "rvv_ms": rvv["avg_ms"],
                "ba_values": [speedup],
                "baseline": side("scalar triangulation reference", std, meta),
                "candidate": side("RVV param-grid candidate (__RVV10__)", rvv, meta),
                "metrics": {
                    "std_total_ms": std["total_ms"],
                    "rvv_total_ms": rvv["total_ms"],
                    "speedup_std_over_rvv": speedup,
                    "points": std["points"],
                    "polygons": std["polygons"],
                },
                "notes": meta["notes"],
            }
        ],
    }


def generate_repeated_manifest(log_dir: Path, run_dirs: list[Path]) -> dict[str, Any]:
    parsed_runs: list[tuple[dict[str, Any], dict[str, Any], float]] = []
    for run_dir in run_dirs:
        std = parse_log(run_dir / "run_bench_std.log")
        rvv = parse_log(run_dir / "run_bench_rvv.log")
        if std["case"] != rvv["case"]:
            raise ValueError(f"case mismatch in {run_dir}: std={std['case']} rvv={rvv['case']}")
        if std.get("iterations") != rvv.get("iterations") or std.get("warmup_iterations") != rvv.get("warmup_iterations"):
            raise ValueError(f"run contract mismatch between std and rvv logs in {run_dir}")
        parsed_runs.append((std, rvv, std["avg_ms"] / rvv["avg_ms"]))

    first_std, first_rvv, _ = parsed_runs[0]
    case = str(first_std["case"])
    if case not in CASE_META:
        raise ValueError(f"unknown triangulation case: {case}")
    meta = CASE_META[case]
    for std, rvv, _speedup in parsed_runs:
        if std["case"] != case or rvv["case"] != case:
            raise ValueError(f"mixed cases in repeated directory {log_dir}")
        if std["checksum"] != first_std["checksum"] or rvv["checksum"] != first_rvv["checksum"]:
            raise ValueError(f"checksum drift in repeated directory {log_dir}")

    values = [speedup for _std, _rvv, speedup in parsed_runs]
    std_values = [std["avg_ms"] for std, _rvv, _speedup in parsed_runs]
    rvv_values = [rvv["avg_ms"] for _std, rvv, _speedup in parsed_runs]
    baseline = side("scalar triangulation reference", first_std, meta)
    candidate = side("RVV param-grid candidate (__RVV10__)", first_rvv, meta)
    baseline["std_ms"] = statistics.median(std_values)
    candidate["rvv_ms"] = statistics.median(rvv_values)

    return {
        "schema_version": 1,
        "summary": {
            "title": f"triangulation {case} repeated board diagnostic",
            "evidence_role": "diagnostic",
            "summary_path": str(log_dir / "summary.md"),
            "label": "diagnostic repeated board summary",
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": first_std["iterations"],
                "warmup_iterations": first_std["warmup_iterations"],
                "run_count": len(parsed_runs),
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
                "mask",
                "reduction",
            ],
        },
        "comparisons": [
            {
                "name": case,
                "group": meta["group"],
                "evidence_role": "diagnostic",
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": len(parsed_runs),
                "iterations": first_std["iterations"],
                "warmup_iterations": first_std["warmup_iterations"],
                "build_comparison": True,
                "std_ms": statistics.median(std_values),
                "rvv_ms": statistics.median(rvv_values),
                "ba_values": values,
                "baseline": baseline,
                "candidate": candidate,
                "metrics": {
                    "std_ms_values": std_values,
                    "rvv_ms_values": rvv_values,
                    "speedup_std_over_rvv_values": values,
                    "speedup_median": statistics.median(values),
                    "speedup_min": min(values),
                    "speedup_max": max(values),
                    "points": first_std["points"],
                    "polygons": first_std["polygons"],
                    "runs": [run_dir.name for run_dir in run_dirs],
                },
                "notes": meta["notes"],
            }
        ],
    }


def format_values(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}x" for value in values)


def write_summary(manifest: dict[str, Any], output: Path) -> None:
    comp = manifest["comparisons"][0]
    values = comp["ba_values"]
    metrics = comp["metrics"]
    lines = [
        "# Repeated Benchmark Speedup Summary",
        "",
        f"- case: `{comp['name']}`",
        f"- evidence_role: `{comp['evidence_role']}`",
        f"- run_count: `{comp['run_count']}`",
        f"- iterations: `{comp['iterations']}`",
        f"- warmup_iterations: `{comp['warmup_iterations']}`",
        f"- checksum_policy: `{comp['baseline']['checksum_policy']}`",
        f"- checksum: baseline `{comp['baseline']['checksum']}`, candidate `{comp['candidate']['checksum']}`",
        "",
        "| Benchmark Item | runs | median | min | max | values |",
        "| --- | ---: | ---: | ---: | ---: | --- |",
        f"| {comp['name']} | {comp['run_count']} | {metrics['speedup_median']:.3f}x | {metrics['speedup_min']:.3f}x | {metrics['speedup_max']:.3f}x | {format_values(values)} |",
        "",
    ]
    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log_dir", type=Path, help="archived board log directory")
    parser.add_argument("--output", type=Path, default=None, help="manifest output path")
    parser.add_argument("--summary-output", type=Path, default=None, help="summary.md output path for repeated dirs")
    args = parser.parse_args()

    manifest = generate_manifest(args.log_dir)
    output = args.output or args.log_dir / "evidence_manifest.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {output}")
    if (args.log_dir / "run_1").is_dir() or args.summary_output:
        summary_output = args.summary_output or args.log_dir / "summary.md"
        write_summary(manifest, summary_output)
        print(f"wrote {summary_output}")


if __name__ == "__main__":
    main()
