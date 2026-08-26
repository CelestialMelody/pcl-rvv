#!/usr/bin/env python3
"""Generate a topic-local Evidence Doctor manifest for cppf board compare logs."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


CASE_RE = re.compile(
    r"^(?P<name>[A-Za-z0-9_]+)\s+\|\s+Std\s+\|\s+(?P<std_ms>[0-9.]+)\s+ms\s+\|.*?\n"
    r"\s+\|\s+RVV\s+\|\s+(?P<rvv_ms>[0-9.]+)\s+ms\s+\|.*?\[\s*(?P<speedup>[0-9.]+)x\s*\]",
    re.MULTILINE,
)


def parse_context(text: str) -> dict[str, object]:
    context: dict[str, object] = {}
    for key, manifest_key in (
        ("Device", "device"),
        ("VLEN", "vlen"),
        ("Dataset", "dataset"),
        ("Std log", "std_log"),
        ("RVV log", "rvv_log"),
    ):
        match = re.search(rf"^\s*{re.escape(key)}\s*:\s*(.+?)\s*$", text, re.MULTILINE)
        if match:
            context[manifest_key] = match.group(1).strip()
    if match := re.search(r"^\s*Iterations\s*:\s*(\d+)", text, re.MULTILINE):
        context["iterations"] = int(match.group(1))
    if match := re.search(r"^\s*Warmup\s*:\s*(\d+)", text, re.MULTILINE):
        context["warmup_iterations"] = int(match.group(1))
    return context


def comparison_metadata(name: str) -> dict[str, object]:
    if name == "candidate_cppf_pair_hsv_batch_rvv":
        return {
            "boundary": "test helper",
            "case_kind": "component-ablation",
            "evidence_role": "diagnostic",
            "group": "cppf-pair-hsv-staged-rvv",
            "point_type": "PointXYZRGBNormal -> CPPFSignature",
            "gate": "PointXYZRGBNormal dense finite input, RGB and normal fields, sequential prefix indices",
            "timer_boundary": "computeCPPFPairHSVBatchRVV repeated, including SoA staging, RVV pair/HSV math, scalar alpha_m and output write",
            "row_source": "ordered index_i x full input",
            "reduction": "RVV pair and HSV math plus scalar alpha_m",
            "notes": "Phase 000 test-only SoA-staged pair/HSV RVV candidate. It screens component value but does not prove production dispatch.",
        }
    if name == "candidate_cppf_alpha_m_batch_rvv":
        return {
            "boundary": "test helper",
            "case_kind": "component-ablation",
            "evidence_role": "diagnostic",
            "group": "cppf-alpha-m-rvv",
            "point_type": "PointXYZRGBNormal -> CPPFSignature",
            "gate": "PointXYZRGBNormal dense finite input, RGB and normal fields, sequential prefix indices",
            "timer_boundary": "computeCPPFAlphaMBatchRVV repeated, including scalar f1..f10, alpha_m staging, RVV atan2 and output write",
            "row_source": "ordered index_i x full input",
            "reduction": "scalar CPPF pair/HSV math plus RVV alpha_m closed-form",
            "notes": "Phase 000 test-only alpha_m RVV candidate. It screens the CPPF rotation-angle tail but does not prove production dispatch.",
        }
    if name == "public_cppf_compute":
        return {
            "boundary": "public overload",
            "case_kind": "public-smoke-baseline",
            "evidence_role": "diagnostic",
            "group": "cppf-public-estimation",
            "point_type": "PointXYZRGBNormal -> CPPFSignature",
            "gate": "current production has no RVV dispatch",
            "timer_boundary": "CPPFEstimation::compute with explicit indices",
            "row_source": "ordered index_i x full input",
            "reduction": "all-pairs output, scalar CPPF pair feature, scalar alpha_m and push_back output",
            "notes": "Public smoke for current production baseline. Std/RVV comparison here is not production RVV evidence because production has no RVV dispatch.",
        }
    return {
        "boundary": "test helper",
        "case_kind": "component-baseline",
        "evidence_role": "diagnostic",
        "group": "cppf-reference-component",
        "point_type": "PointXYZRGBNormal -> CPPFSignature",
        "gate": "dense finite input, RGB and normal fields, sequential prefix indices",
        "timer_boundary": "computeCPPFReference repeated, including scalar CPPF pair feature, scalar alpha_m and output write",
        "row_source": "ordered index_i x full input",
        "reduction": "scalar CPPF pair/HSV math plus scalar alpha_m",
        "notes": "Component scalar reference for strict same-chain checks. It does not prove production dispatch.",
    }


def build_comparison(
    name: str,
    std_values: list[float],
    rvv_values: list[float],
    speedup_values: list[float],
    run_paths: list[str],
) -> dict[str, object]:
    meta = comparison_metadata(name)
    side_common = {
        "boundary": meta["boundary"],
        "wrapper": "test-rvv/features/cppf/src/bench_cppf.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": "CPPF descriptor checksum line",
        "checksum": "see matching checksum lines in run_bench_std.log and run_bench_rvv.log",
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": "identity pairs are NaN; dense finite non-identity pairs are timed",
        "reduction": meta["reduction"],
        "asm_boundary": "computeCPPFPairHSVBatchRVV / computePairHSVFeaturesRVV / computeAlphaMClosedFormRVV or inlined helper",
    }
    std_ms = statistics.mean(std_values)
    rvv_ms = statistics.mean(rvv_values)
    return {
        "name": name,
        "group": meta["group"],
        "evidence_role": meta["evidence_role"],
        "case_kind": meta["case_kind"],
        "point_type": meta["point_type"],
        "size": "side/index-count from dataset line",
        "run_count": len(speedup_values),
        "iterations": 8,
        "warmup_iterations": 2,
        "build_comparison": True,
        "std_ms": std_ms,
        "rvv_ms": rvv_ms,
        "ba_values": speedup_values,
        "std_ms_values": std_values,
        "rvv_ms_values": rvv_values,
        "run_paths": run_paths,
        "baseline": {"label": "Std build", "std_ms": std_ms, "rvv_ms": std_ms, **side_common},
        "candidate": {"label": "RVV build", "std_ms": std_ms, "rvv_ms": rvv_ms, **side_common},
        "notes": meta["notes"],
    }


def parse_summary(path: Path) -> tuple[dict[str, object], list[dict[str, object]]]:
    text = path.read_text(encoding="utf-8")
    context = parse_context(text)
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


def find_summary_paths(summary: Path | None, summary_dir: Path | None) -> list[Path]:
    if summary is not None:
        return [summary]
    if summary_dir is None:
        raise ValueError("either --summary or --summary-dir is required")
    paths = sorted(summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not paths:
        raise ValueError(f"no run_*/analyze_bench_compare.log files found under {summary_dir}")
    return paths


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", type=Path)
    parser.add_argument("--summary-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    summary_paths = find_summary_paths(args.summary, args.summary_dir)
    contexts: list[dict[str, object]] = []
    grouped: dict[str, dict[str, list[float] | list[str]]] = {}
    for path in summary_paths:
        context, comparisons_for_path = parse_summary(path)
        contexts.append(context)
        for item in comparisons_for_path:
            bucket = grouped.setdefault(str(item["name"]), {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(float(item["std_ms"]))  # type: ignore[union-attr]
            bucket["rvv"].append(float(item["rvv_ms"]))  # type: ignore[union-attr]
            bucket["speedup"].append(float(item["speedup"]))  # type: ignore[union-attr]
            bucket["paths"].append(str(item["summary_path"]))  # type: ignore[union-attr]

    if not grouped:
        raise ValueError("no benchmark comparisons parsed")

    first_context = contexts[0] if contexts else {}
    comparisons = [
        build_comparison(
            name,
            list(values["std"]),  # type: ignore[arg-type]
            list(values["rvv"]),  # type: ignore[arg-type]
            list(values["speedup"]),  # type: ignore[arg-type]
            list(values["paths"]),  # type: ignore[arg-type]
        )
        for name, values in sorted(grouped.items())
    ]

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "cppf board Std/RVV compare",
            "evidence_role": "diagnostic",
            "summary_path": str(args.summary or args.summary_dir),
            "summary_paths": [str(path) for path in summary_paths],
            "label": "cppf board repeated compare" if len(summary_paths) > 1 else "cppf board smoke compare",
            "analysis_script": "test-rvv/features/cppf/script/generate_cppf_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "Milkv-Jupiter"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic cppf grid"),
                "iterations": first_context.get("iterations", 8),
                "warmup_iterations": first_context.get("warmup_iterations", 2),
                "run_count": len(summary_paths),
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
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
