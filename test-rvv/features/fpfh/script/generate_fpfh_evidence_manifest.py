#!/usr/bin/env python3
"""Generate a topic-local Evidence Doctor manifest for fpfh board compare logs."""

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

    iterations_match = re.search(r"^\s*Iterations\s*:\s*(\d+)", text, re.MULTILINE)
    if iterations_match:
        context["iterations"] = int(iterations_match.group(1))
    warmup_match = re.search(r"^\s*Warmup\s*:\s*(\d+)", text, re.MULTILINE)
    if warmup_match:
        context["warmup_iterations"] = int(warmup_match.group(1))
    return context


def comparison_metadata(name: str) -> dict[str, object]:
    if name == "public_fpfh_k":
        return {
            "boundary": "public overload",
            "case_kind": "production-public",
            "evidence_role": "production_public",
            "group": "fpfh-public-estimation",
            "timer_boundary": "FPFHEstimation::compute with KdTree neighbor search",
            "row_source": "public KSearch neighborhood",
            "reduction": "SPFH histograms plus weighted FPFH normalization",
            "notes": "Production public Std/RVV compare after the fpfh.hpp production probe is compiled into the RVV build.",
        }
    if name == "component_spfh_signature":
        return {
            "boundary": "test helper",
            "case_kind": "component-ablation",
            "evidence_role": "production_shaped_diagnostic",
            "group": "spfh-component",
            "timer_boundary": "computePointSPFHSignature repeated 256x",
            "row_source": "fixed wrapped neighborhood indices",
            "reduction": "per-feature histogram scatter",
            "notes": "Unchanged component dilution check; it does not exercise the new weighted-SPFH RVV helper.",
        }
    if name == "candidate_weighted_spfh_dense_rows":
        return {
            "boundary": "test helper",
            "case_kind": "component-candidate",
            "evidence_role": "diagnostic",
            "group": "weighted-spfh-component-candidate",
            "timer_boundary": "weightPointSPFHDenseRowsRVV repeated 2048x",
            "row_source": "dense sequential SPFH matrix rows",
            "reduction": "test-only RVV weighted 33-bin accumulation and normalization",
            "notes": "Historical test-only candidate comparison; it is not production adoption evidence.",
        }
    return {
        "boundary": "production detail helper",
        "case_kind": "production-detail",
        "evidence_role": "production_detail",
        "group": "weighted-spfh-component",
        "timer_boundary": "weightPointSPFHSignature repeated 2048x",
        "row_source": "valid SPFH matrix row indices after production lookup",
        "reduction": "production RVV weighted 33-bin accumulation and normalization",
        "notes": "Production detail helper evidence for features/include/pcl/features/impl/fpfh.hpp::weightPointSPFHSignature.",
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
        "wrapper": "test-rvv/features/fpfh/src/bench_fpfh.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": "weighted descriptor checksum line",
        "checksum": "see matching checksum lines in run_bench_std.log and run_bench_rvv.log",
        "timer_boundary": meta["timer_boundary"],
        "gate": "PointNormal dense finite input, k>=4, 11+11+11 bins",
        "mask": "production finite query check only in public case; component cases use finite synthetic points",
        "reduction": meta["reduction"],
        "asm_boundary": "FPFHEstimation::computePointSPFHSignature / weightPointSPFHSignature or inlined helper",
    }
    std_ms = statistics.mean(std_values)
    rvv_ms = statistics.mean(rvv_values)
    return {
        "name": name,
        "group": meta["group"],
        "evidence_role": meta["evidence_role"],
        "case_kind": meta["case_kind"],
        "point_type": "PointNormal -> FPFHSignature33",
        "size": "side/k from dataset line",
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
    roles = {str(comparison["evidence_role"]) for comparison in comparisons}
    summary_role = roles.pop() if len(roles) == 1 else "mixed_diagnostic_production_detail_and_public"

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "fpfh board Std/RVV compare",
            "evidence_role": summary_role,
            "summary_path": str(args.summary or args.summary_dir),
            "summary_paths": [str(path) for path in summary_paths],
            "label": "fpfh board repeated compare" if len(summary_paths) > 1 else "fpfh board smoke compare",
            "analysis_script": "test-rvv/features/fpfh/script/generate_fpfh_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "Milkv-Jupiter"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic fpfh point-normal grid"),
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
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
