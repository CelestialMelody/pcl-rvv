#!/usr/bin/env python3
"""Generate a topic-local Evidence Doctor manifest for pfh board compare logs."""

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
    if name == "public_pfh_k":
        return {
            "boundary": "public overload",
            "case_kind": "production-public-baseline",
            "evidence_role": "production_public",
            "group": "pfh-public-estimation",
            "point_type": "PointNormal -> PFHSignature125",
            "gate": "PointNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "PFHEstimation::compute with KdTree neighbor search",
            "row_source": "public KSearch neighborhood",
            "reduction": "all-pairs PFH histogram scatter plus output copy",
            "notes": "Public-like dilution check. Before a PFH production RVV patch exists this is a baseline, not adoption evidence.",
        }
    if name == "public_pfh_pointxyz_normal_k":
        return {
            "boundary": "public overload",
            "case_kind": "production-public",
            "evidence_role": "production_public",
            "group": "pfh-pointxyz-normal-public-estimation",
            "point_type": "PointXYZ + Normal -> PFHSignature125",
            "gate": "PointXYZ source cloud plus Normal cloud, dense finite source input, k>=4, nr_split=5",
            "timer_boundary": "PFHEstimation<PointXYZ, Normal>::compute with KdTree neighbor search",
            "row_source": "public KSearch neighborhood",
            "reduction": "all-pairs PFH histogram scatter plus output copy",
            "notes": "Phase 060 production-public evidence for the split PointXYZ source cloud and Normal normals cloud path.",
        }
    if name == "candidate_pfh_pair_batch_rvv":
        return {
            "boundary": "test helper",
            "case_kind": "component-ablation",
            "evidence_role": "diagnostic",
            "group": "pfh-pair-feature-staged-rvv",
            "point_type": "PointNormal -> PFHSignature125",
            "gate": "PointNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "computePointPFHSignaturePairBatchRVV repeated 128x, including scalar staging and histogram scatter",
            "row_source": "fixed wrapped neighborhood indices",
            "reduction": "RVV pair tuple math with scalar ordered histogram scatter",
            "notes": "Phase 010 test-only staged pair-feature RVV candidate. It can screen candidate value but does not prove production dispatch.",
        }
    if name == "candidate_pfh_direct_aos_rvv":
        return {
            "boundary": "test helper",
            "case_kind": "component-ablation",
            "evidence_role": "diagnostic",
            "group": "pfh-direct-point-load-rvv",
            "point_type": "PointNormal -> PFHSignature125",
            "gate": "PointNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "computePointPFHSignatureDirectAoSRVV repeated 128x, including pair-index staging and scalar histogram scatter",
            "row_source": "fixed wrapped neighborhood indices",
            "reduction": "RVV direct AoS pair tuple math with scalar ordered histogram scatter",
            "notes": "Phase 030 test-only direct AoS pair-feature RVV candidate. It compares family shape against staged SoA, not production dispatch.",
        }
    if name == "component_pfh_signature_pointxyz_normal":
        return {
            "boundary": "production detail helper",
            "case_kind": "component-ablation",
            "evidence_role": "production_detail",
            "group": "pfh-pointxyz-normal-component",
            "point_type": "PointXYZ + Normal -> PFHSignature125",
            "gate": "PointXYZ source cloud plus Normal cloud, dense finite source input, k>=4, nr_split=5",
            "timer_boundary": "computePointPFHSignature<PointXYZ, Normal> repeated 128x",
            "row_source": "fixed wrapped neighborhood indices",
            "reduction": "RVV direct AoS pair tuple math with scalar ordered histogram scatter",
            "notes": "Phase 060 production-detail evidence for the split PointXYZ source cloud and Normal normals cloud path.",
        }
    return {
        "boundary": "production-shaped helper",
        "case_kind": "component-ablation",
        "evidence_role": "production_shaped_diagnostic",
        "group": "pfh-component",
        "point_type": "PointNormal -> PFHSignature125",
        "gate": "PointNormal dense finite input, k>=4, nr_split=5",
        "timer_boundary": "computePointPFHSignature repeated 128x",
        "row_source": "fixed wrapped neighborhood indices",
        "reduction": "all-pairs pair feature math and 125-bin histogram scatter",
        "notes": "Component baseline for pfh.hpp::computePointPFHSignature. It does not prove production dispatch.",
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
        "wrapper": "test-rvv/features/pfh/src/bench_pfh.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": "PFH descriptor checksum line",
        "checksum": "see matching checksum lines in run_bench_std.log and run_bench_rvv.log",
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": "production finite point check in component loop; public case follows PFHEstimation::compute",
        "reduction": meta["reduction"],
        "asm_boundary": "PFHEstimation::computePointPFHSignature or inlined helper",
    }
    std_ms = statistics.mean(std_values)
    rvv_ms = statistics.mean(rvv_values)
    return {
        "name": name,
        "group": meta["group"],
        "evidence_role": meta["evidence_role"],
        "case_kind": meta["case_kind"],
        "point_type": meta["point_type"],
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
    summary_role = roles.pop() if len(roles) == 1 else "mixed_diagnostic_and_public_baseline"

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "pfh board Std/RVV compare",
            "evidence_role": summary_role,
            "summary_path": str(args.summary or args.summary_dir),
            "summary_paths": [str(path) for path in summary_paths],
            "label": "pfh board repeated compare" if len(summary_paths) > 1 else "pfh board smoke compare",
            "analysis_script": "test-rvv/features/pfh/script/generate_pfh_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "Milkv-Jupiter"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic pfh point-normal grid"),
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
