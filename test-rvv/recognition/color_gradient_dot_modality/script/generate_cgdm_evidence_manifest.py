#!/usr/bin/env python3
"""Generate CGDM repeated board summary and Evidence Doctor manifest."""

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
    if 1.05 <= median < 1.20 and below_one == 0:
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def metadata_for_case(name: str, run_label: str = "") -> dict[str, str]:
    common = {
        "group": "cgdm-dominant-map-production-shaped-diagnostic",
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "point_type": "pcl::PointXYZRGB",
        "row_source": "organized_rgb_image_inner_pixels",
        "boundary": "test_helper",
        "wrapper": "test-rvv/recognition/color_gradient_dot_modality/src/bench_cgdm.cpp",
        "timer_boundary": "computeDominantMapCandidate, RGB gradient selection plus dominant bin reduction on inner bins",
        "gate": "organized pcl::PointXYZRGB image, bin_size>0, width>=3, height>=3, borders are zero-filled",
        "mask": "gradient magnitude threshold and 3x3 dominant-bin equality masks",
        "reduction": "lane-wise dominant bin selection and thresholded one-hot writeback",
        "checksum_policy": "FNV-style checksum over dominant map and gradient count",
        "asm_boundary": "computeDominantMapCandidate / inlined RVV math in bench_cgdm_rvv",
        "notes": "Phase 000 production-shaped diagnostic. It does not call a production RVV path yet, and it does not cover computeInvariantQuantizedMap().",
    }
    sizes = {
        "dominant_map_320x240": "320x240 organized RGB image",
        "dominant_map_641x481_tail": "641x481 organized RGB image with RVV tail lanes",
        "process_input_320x240": "320x240 organized PointXYZRGB cloud through processInputData()",
        "process_input_641x481_tail": "641x481 organized PointXYZRGB cloud through processInputData() with RVV tail lanes",
    }
    if name not in sizes:
        raise ValueError(f"unknown CGDM case label: {name}")
    if name.startswith("process_input"):
        production_direct = "production" in run_label or "phase010" in run_label
        common = {
            **common,
            "group": "cgdm-production-public-color-gradient-dot-process"
            if production_direct
            else "cgdm-production-shaped-preprocess-diagnostic",
            "case_kind": "production_direct" if production_direct else "production_shaped_diagnostic",
            "evidence_role": "production_direct" if production_direct else "production_shaped_diagnostic",
            "boundary": "public_entry" if production_direct else "test_helper",
            "wrapper": "ColorGradientDOTModality<PointXYZRGB>::processInputData via src/bench_cgdm.cpp"
            if production_direct
            else common["wrapper"],
            "timer_boundary": "ColorGradientDOTModality::processInputData, computeMaxColorGradients plus computeDominantQuantizedGradients",
            "mask": "gradient magnitude threshold and dominant-bin equality masks on production object state",
            "reduction": "stateful gradient generation and dominant bin selection, then writeback to QuantizedMap",
            "checksum_policy": "FNV-style checksum over dominant map and gradient count",
            "asm_boundary": "ColorGradientDOTModality::computeMaxColorGradientsRVV / inlined production helper in bench_cgdm_rvv"
            if production_direct
            else "ColorGradientDOTModality::processInputData / inlined helper chain in bench_cgdm_rvv",
            "notes": "Production direct evidence. It calls the public ColorGradientDOTModality<PointXYZRGB>::processInputData entry after the production RVV patch."
            if production_direct
            else "Production-shaped diagnostic only. It uses the real public entry, but this topic has not yet inserted a production RVV patch.",
        }
    return {"name": name, "label": sizes[name], **common, "run_label": run_label}


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle", "vlse", "vse", "vsetvli", "vfsqrt", "vfcvt", "vf", "vm")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def evidence_boundary_text(case_names: list[str], run_label: str) -> str:
    if case_names and all(name.startswith("process_input") for name in case_names):
        if "production" in run_label or "phase010" in run_label:
            return (
                "本 summary 是 production direct（真实生产路径）证据。Std/RVV 两侧都通过 "
                "`ColorGradientDOTModality<PointXYZRGB>::processInputData()` 公开入口计时；"
                "计时边界包含 production `computeMaxColorGradients` / "
                "`computeMaxColorGradientsRVV` 分流和原有 dominant bin 标量扫描。"
            )
        return (
            "本 summary 是 production-shaped diagnostic（生产形态诊断）的 public-entry "
            "cross-check（公开入口交叉检查）。本阶段还没有修改 production 源码，所以 "
            "process_input_* 的 Std/RVV 对比只说明当前 public entry 没有 RVV dispatch，"
            "不能作为 production adoption（生产采纳）证据。"
        )
    if case_names and all(name.startswith("dominant_map") for name in case_names):
        return (
            "本 summary 是 Phase 000 未接 production 的 production-shaped diagnostic "
            "证据。Std/RVV 两侧共享同一个 bench wrapper，计时边界只覆盖 "
            "computeDominantMapCandidate 的 dominant-map helper，不能替代 "
            "`ColorGradientDOTModality::processInputData()` 的 production direct 证据。"
        )
    return (
        "本 summary 混合了 test helper 与 public entry cross-check。dominant_map_* "
        "用于判断候选是否值得进入 production integration loop；process_input_* 在本阶段"
        "只证明 production 尚未接入 RVV。"
    )


def build_manifest(summary_dir: Path, output: Path, summary_output: Path, title: str, run_label: str,
                   case_name: str | None, expected_runs: int, asm_path: str) -> None:
    summary_files = sorted(summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not summary_files:
        raise SystemExit(f"no run_*/analyze_bench_compare.log files found under {summary_dir}")
    contexts: list[dict[str, Any]] = []
    comparisons: list[dict[str, Any]] = []
    checksums: dict[str, list[str]] = {}

    for summary in summary_files:
        context, run_cases = parse_summary(summary)
        contexts.append(context)
        comparisons.extend(run_cases)
        for case in run_cases:
            std_checksum = parse_checksum(summary.with_name("run_bench_std.log"), case["name"])
            rvv_checksum = parse_checksum(summary.with_name("run_bench_rvv.log"), case["name"])
            checksum = std_checksum if std_checksum == rvv_checksum else "mixed"
            if checksum is not None:
                checksums.setdefault(case["name"], []).append(checksum)

    if case_name:
        comparisons = [entry for entry in comparisons if entry["name"] == case_name]
    if not comparisons:
        raise SystemExit("no benchmark comparisons parsed")

    grouped_names = sorted({entry["name"] for entry in comparisons})
    case_roles = sorted({metadata_for_case(name, run_label)["evidence_role"] for name in grouped_names})
    summary_role = case_roles[0] if len(case_roles) == 1 else "mixed_boundary_cross_check"
    case_boundaries = sorted({metadata_for_case(name, run_label)["boundary"] for name in grouped_names})
    summary_boundary = case_boundaries[0] if len(case_boundaries) == 1 else "mixed"
    asm_lines = count_rvv_asm_lines(Path(asm_path))
    summary_output.parent.mkdir(parents=True, exist_ok=True)
    summary_lines = [
        f"# {title}",
        "",
        f"- run_label: `{run_label}`",
        f"- expected_runs: `{expected_runs}`",
        f"- collected_runs: `{len(summary_files)}`",
        f"- evidence_role: `{summary_role}`",
        f"- A/B boundary: `{summary_boundary}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    cases: list[dict[str, Any]] = []
    for name in grouped_names:
        values = [item["speedup"] for item in comparisons if item["name"] == name]
        std_values = [item["std_ms"] for item in comparisons if item["name"] == name]
        rvv_values = [item["rvv_ms"] for item in comparisons if item["name"] == name]
        paths = [item["summary_path"] for item in comparisons if item["name"] == name]
        metadata = metadata_for_case(name, run_label)
        below_one = sum(1 for value in values if value < 1.0)
        summary_lines.append(
            "| `{}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
                name,
                len(values),
                statistics.median(values),
                min(values),
                max(values),
                percentile(values, 0.10),
                percentile(values, 0.90),
                below_one,
                len(values),
                ", ".join(f"{value:.3f}x" for value in values),
            )
        )
        common = {
            "boundary": metadata["boundary"],
            "wrapper": metadata["wrapper"],
            "row_source": metadata["row_source"],
            "solve": False,
            "checksum_policy": metadata["checksum_policy"],
            "timer_boundary": metadata["timer_boundary"],
            "gate": metadata["gate"],
            "mask": metadata["mask"],
            "reduction": metadata["reduction"],
            "asm_boundary": metadata["asm_boundary"],
            "rvv_instr_count": asm_lines,
        }
        std_ms = statistics.mean(std_values)
        rvv_ms = statistics.mean(rvv_values)
        cases.append(
            {
                **metadata,
                "summary_path": paths[0],
                "run_paths": paths,
                "checksum": unique_or_mixed(checksums.get(name, [])),
                "run_count": len(values),
                "observed_runs": len(values),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "build_comparison": True,
                "std_ms": std_ms,
                "rvv_ms": rvv_ms,
                "std_ms_median": statistics.median(std_values),
                "rvv_ms_median": statistics.median(rvv_values),
                "std_ms_values": std_values,
                "rvv_ms_values": rvv_values,
                "ba_values": values,
                "speedup_median": statistics.median(values),
                "speedup_p10": percentile(values, 0.10),
                "speedup_p90": percentile(values, 0.90),
                "decision_bucket": decision_bucket(values),
                "baseline": {
                    "label": "Std build",
                    "checksum": unique_or_mixed(checksums.get(name, [])),
                    "std_ms": std_ms,
                    "rvv_ms": std_ms,
                    **common,
                },
                "candidate": {
                    "label": "RVV build",
                    "checksum": unique_or_mixed(checksums.get(name, [])),
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    **common,
                },
            }
        )

    summary_lines += [
        "",
        "## Evidence boundary",
        "",
        evidence_boundary_text(grouped_names, run_label),
    ]
    summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    first_context = contexts[0] if contexts else {}
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": title,
            "evidence_role": summary_role,
            "summary_path": str(summary_output),
            "summary_paths": [str(path) for path in summary_files],
            "label": run_label,
            "analysis_script": "test-rvv/recognition/color_gradient_dot_modality/script/generate_cgdm_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "board via Makefile config"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic organized RGB image"),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "run_count": len(summary_files),
                "expected_runs": expected_runs,
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
        "title": title,
        "run_label": run_label,
        "summary_dir": str(summary_dir),
        "summary_output": str(summary_output),
        "asm_path": asm_path,
        "contexts": contexts,
        "cases": cases,
        "comparisons": cases,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--summary-output", required=True)
    parser.add_argument("--title", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--asm", required=True)
    args = parser.parse_args()

    build_manifest(
        Path(args.summary_dir),
        Path(args.output),
        Path(args.summary_output),
        args.title,
        args.run_label,
        args.case_name,
        args.expected_runs,
        args.asm,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
