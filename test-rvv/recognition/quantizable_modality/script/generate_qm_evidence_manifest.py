#!/usr/bin/env python3
"""Generate evidence manifest for QuantizedMap spread repeated board runs.

本脚本只理解 `QuantizedMap::spreadQuantizedMap()` 的 benchmark 日志。
它把 5-run board 目录中的 Std/RVV 比较日志归一化成通用
evidence_manifest.json，并输出一个 topic-local summary，供 phase result /
evaluation / Handoff 引用。
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
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s+checksum:\s*(?P<checksum>[0-9]+)", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup(?:\s+Iterations)?\s*:\s*(?P<warmup>\d+)", re.MULTILINE)
DATASET_RE = re.compile(r"^\s*Dataset:\s*(?P<dataset>.+?)\s*$", re.MULTILINE)
BUILD_RE = re.compile(r"^\s*Build Path:\s*(?P<build>.+?)\s*$", re.MULTILINE)


def parse_context(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    if match := DATASET_RE.search(text):
        context["dataset"] = match.group("dataset").strip()
    if match := BUILD_RE.search(text):
        context["build_path"] = match.group("build").strip()
    return context


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


def parse_summary(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context = parse_context(path)
    comparisons = []
    for match in CASE_RE.finditer(text):
      comparisons.append(
          {
              "name": match.group("name"),
            "std_ms": float(match.group("std_ms")),
            "rvv_ms": float(match.group("rvv_ms")),
            "speedup": float(match.group("speedup")),
            "summary_path": str(path),
          }
      )
    return context, comparisons


def parse_checksum(path: Path, case_name: str) -> str | None:
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    for match in CHECKSUM_RE.finditer(text):
        if match.group("name") == case_name:
            return match.group("checksum")
    return None


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
        "shared_spread_320x240": "320x240 organized quantized byte map",
        "shared_spread_641x481_tail": "641x481 organized quantized byte map with RVV tail lanes",
    }
    if name not in sizes:
        raise ValueError(f"unknown QM case label: {name}")
    return {
        "group": "quantizable-modality-shared-spread",
        "case_kind": "production_detail",
        "evidence_role": "production_detail",
        "point_type": "not_applicable_quantized_byte_map",
        "row_source": "organized_quantized_byte_map",
        "boundary": "production_detail_helper",
        "wrapper": "test-rvv/recognition/quantizable_modality/src/bench_qm.cpp",
        "timer_boundary": "QuantizedMap::spreadQuantizedMap input map -> temporary map -> output map",
        "gate": "spreading_size must be 8 for RVV path; other values fall back to scalar helper",
        "mask": "byte OR window across horizontal then vertical passes",
        "reduction": "u8 bitwise OR reduction across 8-wide window",
        "checksum_policy": "FNV-style checksum over output QuantizedMap bytes",
        "asm_boundary": "QuantizedMap::spreadQuantizedMap / spreadQuantizedMapRVV in bench_qm_rvv",
        "size": sizes[name],
        "notes": "This benchmark measures the public QuantizedMap spread helper directly.",
    }


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle8", "vor", "vse8", "vsetvli")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--title", default="QuantizedMap shared spread repeated board summary")
    parser.add_argument("--run-label", default="qm_phase000_shared_spread_rvv_repeated")
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--evidence-role", default="production_detail")
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
        f"- iterations: `{unique_or_mixed([ctx.get('iterations') for ctx in contexts])}`",
        f"- warmup_iterations: `{unique_or_mixed([ctx.get('warmup_iterations') for ctx in contexts])}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `production_detail_helper`",
        f"- asm_rvv_line_count: `{asm_lines}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    comparisons_manifest: list[dict[str, Any]] = []
    all_speedups: list[float] = []
    for case_name, stats in sorted(grouped.items()):
        speedups = [float(value) for value in stats["speedup"]]
        std_values = [float(value) for value in stats["std"]]
        rvv_values = [float(value) for value in stats["rvv"]]
        paths = [str(value) for value in stats["paths"]]
        all_speedups.extend(speedups)
        meta = metadata_for_case(case_name)
        below_one = sum(1 for value in speedups if value < 1.0)
        summary_lines.append(
            "| `{}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
                case_name,
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
        std_checksums = [parse_checksum(Path(path).with_name("run_bench_std.log"), case_name) for path in paths]
        rvv_checksums = [parse_checksum(Path(path).with_name("run_bench_rvv.log"), case_name) for path in paths]
        std_ms = statistics.mean(std_values)
        rvv_ms = statistics.mean(rvv_values)
        comparisons_manifest.append(
            {
                "name": case_name,
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

    summary_lines += [
        "",
        "## Evidence boundary",
        "",
        "本 summary 是公共 `QuantizedMap::spreadQuantizedMap()` 的 production-detail（生产细节 helper）证据。"
        "它可以证明接入后 helper 本身的 Std/RVV 性能差异，但不能单独外推为三个 modality 公开入口的端到端收益。",
    ]

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
            "analysis_script": "test-rvv/recognition/quantizable_modality/script/generate_qm_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "board via Makefile config"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get(
                    "dataset",
                    "synthetic quantized byte maps for QuantizedMap::spreadQuantizedMap",
                ),
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
