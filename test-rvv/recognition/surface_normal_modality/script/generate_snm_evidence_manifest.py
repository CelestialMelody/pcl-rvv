#!/usr/bin/env python3
"""Generate SNM repeated board summary and Evidence Doctor manifest.

本脚本只理解 surface_normal_modality topic 的 bench label（性能测试用例名）。
它把每次板卡 repeated run（重复板卡运行）的 analyze log 归一化成通用
Evidence Doctor（证据体检）manifest，避免全局脚本猜 topic-specific（主题特定）
case 含义。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(
    r"^(?P<name>[A-Za-z0-9_]+)\s+\|\s+Std\s+\|\s+(?P<std_ms>[0-9.]+)\s+ms\s+\|.*?\n"
    r"\s+\|\s+RVV\s+\|\s+(?P<rvv_ms>[0-9.]+)\s+ms\s+\|.*?\[\s*(?P<speedup>[0-9.]+)x\s*\]",
    re.MULTILINE,
)
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+).*?checksum:\s*(?P<checksum>[0-9]+)", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations\s*:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup(?: Iterations)?\s*:\s*(?P<warmup>\d+)", re.MULTILINE)
DATASET_RE = re.compile(r"^\s*Dataset\s*:\s*(?P<dataset>.+?)\s*$", re.MULTILINE)
DEVICE_RE = re.compile(r"^\s*Device\s*:\s*(?P<device>.+?)\s*$", re.MULTILINE)
VLEN_RE = re.compile(r"^\s*VLEN\s*:\s*(?P<vlen>.+?)\s*$", re.MULTILINE)


def parse_context(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    if match := DATASET_RE.search(text):
        context["dataset"] = match.group("dataset").strip()
    if match := DEVICE_RE.search(text):
        context["device"] = match.group("device").strip()
    if match := VLEN_RE.search(text):
        context["vlen"] = match.group("vlen").strip()
    return context


def parse_summary(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context = parse_context(path)
    comparisons = [
        {
            "name": match.group("name"),
            "std_ms": float(match.group("std_ms")),
            "rvv_ms": float(match.group("rvv_ms")),
            "speedup": float(match.group("speedup")),
            "summary_path": str(path),
        }
        for match in CASE_RE.finditer(text)
    ]
    return context, comparisons


def parse_checksum(path: Path, case_name: str) -> str | None:
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    for match in CHECKSUM_RE.finditer(text):
        if match.group("name") == case_name:
            return match.group("checksum")
    return None


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def unique_or_mixed(values: list[Any]) -> Any:
    present = [value for value in values if value is not None]
    if not present:
        return None
    first = present[0]
    return first if all(value == first for value in present) else "mixed"


def decision_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.20 and below_one == 0:
        return "positive"
    if 1.05 <= median < 1.20 and below_one <= 1:
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def metadata_for_case(name: str) -> dict[str, str]:
    sizes = {
        "depth_quantize_320x240": "320x240 organized PointXYZ depth cloud",
        "depth_quantize_641x481_tail": "641x481 organized PointXYZ depth cloud with RVV tail lanes",
        "production_process_320x240": "320x240 organized PointXYZRGBA cloud through SurfaceNormalModality::processInputData",
        "production_process_641x481_tail": "641x481 organized PointXYZRGBA cloud through SurfaceNormalModality::processInputData with RVV tail lanes",
    }
    if name not in sizes:
        raise ValueError(f"unknown SNM case label: {name}")
    common = {
        "group": "snm-depth-to-normal-quantize-production-shaped-diagnostic",
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "point_type": "pcl::PointXYZ",
        "row_source": "organized_depth_cloud_inner_pixels",
        "boundary": "test_helper",
        "wrapper": "test-rvv/recognition/surface_normal_modality/src/bench_snm.cpp",
        "timer_boundary": "computeDepthNormalQuantizedCandidate, depth millimeter conversion plus 8-neighbor normal solve and angle quantize",
        "gate": "organized PointXYZ cloud, width>=12, height>=12, finite z values become uint16 depth; non-finite depth becomes zero",
        "mask": "depth<2000 predicate plus abs(neighbor-depth)<50 bilateral predicate; border pixels are left zero",
        "reduction": "RVV per-lane 8-neighbor integer accumulation for det/ddx/ddy; scalar per-lane sqrt/atan2/angle quantize keeps Phase 000 exactness",
        "checksum_policy": "FNV-style checksum over quantized bin and angle degrees scaled by 1000",
        "asm_boundary": "computeDepthNormalQuantizedCandidate / inlined RVV depth and neighbor accumulation in bench_snm_rvv",
        "size": sizes[name],
        "notes": (
            "Phase 000 production-shaped diagnostic. It does not call "
            "SurfaceNormalModality::processInputData and does not include "
            "filterQuantizedSurfaceNormals, spreadQuantizedMap or extractFeatures."
        ),
    }
    if name.startswith("production_process"):
        common = {
            **common,
            "group": "snm-production-public-depth-modality-process",
            "case_kind": "production_direct",
            "evidence_role": "production_direct",
            "point_type": "pcl::PointXYZRGBA",
            "row_source": "organized_pointxyzrgba_public_entry",
            "boundary": "public_overload",
            "wrapper": "SurfaceNormalModality<PointXYZRGBA>::processInputData via src/bench_snm.cpp",
            "timer_boundary": "processInputData includes depth millimeter conversion, 8-neighbor normal solve, angle quantize, 5x5 histogram filter and spread; excludes extractFeatures",
            "gate": "organized PointXYZRGBA cloud, width>=12, height>=12, __RVV10__ candidate build dispatches depth-to-normal/quantize subchain and falls back otherwise",
            "mask": "production depth<2000 predicate plus abs(neighbor-depth)<50 bilateral predicate; downstream filter/spread use existing scalar masks",
            "reduction": "RVV per-lane 8-neighbor integer accumulation for det/ddx/ddy, scalar per-lane sqrt/atan2/angle quantize, current production patch may additionally dispatch the 5x5 filter to RVV; spread remains scalar",
            "checksum_policy": "FNV-style checksum over filtered quantized map, spreaded quantized map and orientation map scaled by 1000",
            "asm_boundary": "SurfaceNormalModality<PointXYZRGBA>::computeAndQuantizeSurfaceNormals2RVV and filterQuantizedSurfaceNormals / inlined production helpers in bench_snm_rvv",
            "notes": (
                "Production direct evidence for the current surface_normal_modality patch. It calls the public "
                "SurfaceNormalModality<PointXYZRGBA>::processInputData entry; "
                "extractFeatures remains outside this timer boundary."
            ),
        }
    return common


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle", "vlse", "vse", "vsetvli", "vzext", "vmul", "vadd", "vsub", "vmerge", "vms")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def evidence_boundary_text() -> str:
    return (
        "本 summary 是 Phase 000 未接 production（生产源码）的 production-shaped diagnostic "
        "（生产形态诊断）证据。Std/RVV 两侧共享同一个 bench wrapper（性能测试包装入口），"
        "计时边界只覆盖 manifest 中的 depth-to-normal / quantize helper，不能替代 "
        "`SurfaceNormalModality::processInputData()` 的 production direct（真实生产路径）证据。"
    )


def evidence_boundary_text_for_cases(case_names: list[str]) -> str:
    if case_names and all(name.startswith("production_process") for name in case_names):
        return (
            "本 summary 是 surface_normal_modality 当前 production direct（真实生产路径）证据。Std/RVV 两侧都通过 "
            "`SurfaceNormalModality<PointXYZRGBA>::processInputData()` 公开入口计时；计时边界包含 "
            "depth-to-normal / quantize、5x5 filter 和 spread，不包含 `extractFeatures()`。"
            "本轮用户已授权接入后板卡有收益即可采纳，因此该 summary 可作为 adopted production "
            "behavior（已采纳生产行为）的生产性能依据。"
        )
    return evidence_boundary_text()


def ab_boundary_for_cases(case_names: list[str]) -> str:
    if case_names and all(name.startswith("production_process") for name in case_names):
        return "public_overload"
    return "test_helper"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--title", default="SNM depth-to-normal quantize repeated board summary")
    parser.add_argument("--run-label", default="snm_phase000_depth_quantize_repeated")
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--evidence-role", default="production_shaped_diagnostic")
    args = parser.parse_args()

    summary_paths = sorted(args.summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not summary_paths:
        raise SystemExit(f"no run_*/analyze_bench_compare.log files found under {args.summary_dir}")

    contexts: list[dict[str, Any]] = []
    grouped: dict[str, dict[str, list[Any]]] = {}
    for path in summary_paths:
        context, comparisons = parse_summary(path)
        contexts.append(context)
        for item in comparisons:
            name = str(item["name"])
            if args.case_name and name != args.case_name:
                continue
            metadata_for_case(name)
            bucket = grouped.setdefault(name, {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(float(item["std_ms"]))
            bucket["rvv"].append(float(item["rvv_ms"]))
            bucket["speedup"].append(float(item["speedup"]))
            bucket["paths"].append(str(item["summary_path"]))

    if not grouped:
        raise SystemExit("no benchmark comparisons parsed")

    asm_lines = count_rvv_asm_lines(args.asm)
    args.summary_output.parent.mkdir(parents=True, exist_ok=True)
    summary_lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(summary_paths)}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{ab_boundary_for_cases(sorted(grouped))}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    comparisons_manifest: list[dict[str, Any]] = []
    all_speedups: list[float] = []
    for name, values in sorted(grouped.items()):
        speedups = [float(value) for value in values["speedup"]]
        std_values = [float(value) for value in values["std"]]
        rvv_values = [float(value) for value in values["rvv"]]
        paths = [str(value) for value in values["paths"]]
        all_speedups.extend(speedups)
        meta = metadata_for_case(name)
        below_one = sum(1 for value in speedups if value < 1.0)
        summary_lines.append(
            "| `{}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
                name,
                len(speedups),
                statistics.median(speedups),
                min(speedups),
                max(speedups),
                percentile(speedups, 0.10),
                percentile(speedups, 0.90),
                below_one,
                len(speedups),
                ", ".join(f"{value:.3f}x" for value in speedups),
            )
        )

        common = {
            "boundary": meta["boundary"],
            "wrapper": meta["wrapper"],
            "row_source": meta["row_source"],
            "solve": False,
            "checksum_policy": meta["checksum_policy"],
            "timer_boundary": meta["timer_boundary"],
            "gate": meta["gate"],
            "mask": meta["mask"],
            "reduction": meta["reduction"],
            "asm_boundary": meta["asm_boundary"],
            "rvv_instr_count": asm_lines,
        }
        std_checksums = [parse_checksum(Path(path).with_name("run_bench_std.log"), name) for path in paths]
        rvv_checksums = [parse_checksum(Path(path).with_name("run_bench_rvv.log"), name) for path in paths]
        std_ms = statistics.mean(std_values)
        rvv_ms = statistics.mean(rvv_values)
        comparisons_manifest.append(
            {
                "name": name,
                "group": meta["group"],
                "evidence_role": meta["evidence_role"],
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": len(speedups),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "build_comparison": True,
                "std_ms": std_ms,
                "rvv_ms": rvv_ms,
                "ba_values": speedups,
                "std_ms_values": std_values,
                "rvv_ms_values": rvv_values,
                "run_paths": paths,
                "baseline": {
                    "label": "Std build",
                    "checksum": unique_or_mixed(std_checksums),
                    "std_ms": std_ms,
                    "rvv_ms": std_ms,
                    **common,
                },
                "candidate": {
                    "label": "RVV build",
                    "checksum": unique_or_mixed(rvv_checksums),
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    **common,
                },
                "notes": meta["notes"],
            }
        )

    summary_lines += ["", "## Evidence boundary", "", evidence_boundary_text_for_cases(sorted(grouped))]
    args.summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    first_context = contexts[0] if contexts else {}
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": args.evidence_role,
            "summary_path": str(args.summary_output),
            "summary_paths": [str(path) for path in summary_paths],
            "label": args.run_label,
            "analysis_script": "test-rvv/recognition/surface_normal_modality/script/generate_snm_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "board via Makefile config"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic organized PointXYZ depth cloud"),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "run_count": len(summary_paths),
                "expected_runs": args.expected_runs,
                "decision_bucket": decision_bucket(all_speedups),
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
                "reduction",
            ],
        },
        "comparisons": comparisons_manifest,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.summary_output}")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
