#!/usr/bin/env python3
"""Generate a topic-local Evidence Doctor manifest for pfhrgb board compare logs."""

from __future__ import annotations

import argparse
import hashlib
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
    if name == "candidate_pfhrgb_pair_batch_rvv":
        return {
            "boundary": "test helper",
            "case_kind": "component-ablation",
            "evidence_role": "diagnostic",
            "group": "pfhrgb-color-pair-batch-rvv",
            "point_type": "PointXYZRGBNormal -> PFHRGBSignature250",
            "gate": "PointXYZRGBNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "computePointPFHRGBSignaturePairBatchRVV repeated 128x, including SoA staging and scalar histogram scatter",
            "row_source": "fixed wrapped neighborhood indices",
            "reduction": "RVV geometry and RGB ratio tuple math with scalar ordered histogram scatter",
            "notes": "Phase 000 test-only pair-batch RVV candidate. It can screen candidate value but does not prove production dispatch.",
        }
    if name == "public_pfhrgb_k":
        return {
            "boundary": "public overload",
            "case_kind": "production-public",
            "evidence_role": "production_public",
            "group": "pfhrgb-public-estimation",
            "point_type": "PointXYZRGBNormal -> PFHRGBSignature250",
            "gate": "PointXYZRGBNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "PFHRGBEstimation::compute with KdTree neighbor search",
            "row_source": "public KSearch neighborhood",
            "reduction": "all-pairs PFHRGB histogram scatter plus output copy",
            "notes": "Production-public evidence after pfhrgb.hpp gained an exact-gated RVV dispatch. It proves the current public RVV path against the current public scalar path for this narrow point-type and KSearch boundary.",
        }
    if name == "public_pfhrgb_k_with_candidate":
        return {
            "boundary": "public-shaped wrapper",
            "case_kind": "public-with-candidate-diagnostic",
            "evidence_role": "production_shaped_diagnostic",
            "group": "pfhrgb-public-with-candidate",
            "point_type": "PointXYZRGBNormal -> PFHRGBSignature250",
            "gate": "PointXYZRGBNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "topic-local KdTree nearestKSearch loop plus computePointPFHRGBSignaturePairBatchRVV per point",
            "row_source": "public KSearch neighborhood",
            "reduction": "public-shaped outer loop with RVV geometry/RGB tuple math and scalar ordered histogram scatter",
            "notes": "Phase 010 public-shaped diagnostic. It remains a topic-local wrapper and is kept only as implementation-shape context after production dispatch was added.",
        }
    if name == "public_pfhrgb_k_with_candidate_reuse":
        return {
            "boundary": "public-shaped wrapper",
            "case_kind": "allocation-staging-ablation",
            "evidence_role": "production_shaped_diagnostic",
            "group": "pfhrgb-staging-reuse",
            "point_type": "PointXYZRGBNormal -> PFHRGBSignature250",
            "gate": "PointXYZRGBNormal dense finite input, k>=4, nr_split=5",
            "timer_boundary": "topic-local KdTree nearestKSearch loop plus reusable PairBatchWorkspace across points",
            "row_source": "public KSearch neighborhood",
            "reduction": "public-shaped outer loop with reusable staging, RVV geometry/RGB tuple math, and scalar ordered histogram scatter",
            "notes": "Phase 030 allocation/staging reuse diagnostic. It compares implementation shape inside the topic-local wrapper and does not prove production dispatch.",
        }
    return {
        "boundary": "production-shaped helper",
        "case_kind": "component-ablation",
        "evidence_role": "production_shaped_diagnostic",
        "group": "pfhrgb-component",
        "point_type": "PointXYZRGBNormal -> PFHRGBSignature250",
        "gate": "PointXYZRGBNormal dense finite input, k>=4, nr_split=5",
        "timer_boundary": "computePointPFHRGBReference repeated 128x",
        "row_source": "fixed wrapped neighborhood indices",
        "reduction": "all-pairs geometry and RGB ratio math plus 250-bin histogram scatter",
        "notes": "Component scalar baseline for pfhrgb.hpp::computePointPFHRGBSignature. It does not prove production dispatch.",
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
        "wrapper": "test-rvv/features/pfhrgb/src/bench_pfhrgb.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": "PFHRGB descriptor checksum line",
        "checksum": "see matching checksum lines in run_bench_std.log and run_bench_rvv.log",
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": "zero-distance and zero-cross fallback follow computeRGBPairFeatures; public case follows PFHRGBEstimation::compute",
        "reduction": meta["reduction"],
        "asm_boundary": "computePointPFHRGBSignaturePairBatchRVV or inlined helper for candidate case",
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


def file_sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return f"sha256:{digest.hexdigest()}"


def binary_identity() -> str | None:
    hashes = []
    for label, path in (
        ("bench_std", Path("build/riscv/bench_pfhrgb_std")),
        ("bench_rvv", Path("build/riscv/bench_pfhrgb_rvv")),
    ):
        digest = file_sha256(path)
        if digest:
            hashes.append(f"{label}={digest}")
    return "; ".join(hashes) if hashes else None


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
    summary_role = roles.pop() if len(roles) == 1 else "mixed_diagnostic_and_public_like"

    metadata = {
        "device": first_context.get("device", "Milkv-Jupiter"),
        "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
        "dataset": first_context.get("dataset", "synthetic pfhrgb rgb-normal grid"),
        "iterations": first_context.get("iterations", 8),
        "warmup_iterations": first_context.get("warmup_iterations", 2),
        "run_count": len(summary_paths),
    }
    binary_hash = binary_identity()
    if binary_hash:
        metadata["binary_hash"] = binary_hash

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "pfhrgb board Std/RVV compare",
            "evidence_role": summary_role,
            "summary_path": str(args.summary or args.summary_dir),
            "summary_paths": [str(path) for path in summary_paths],
            "label": "pfhrgb board repeated compare" if len(summary_paths) > 1 else "pfhrgb board smoke compare",
            "analysis_script": "test-rvv/features/pfhrgb/script/generate_pfhrgb_evidence_manifest.py",
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
            "allowed_contract_mismatches": [
                "Std build candidate case uses scalar reference fallback while RVV build uses RVV intrinsics.",
                "Topic-local public-shaped candidate cases are diagnostic wrappers; production adoption uses public_pfhrgb_k after pfhrgb.hpp dispatch is present.",
            ],
        },
        "comparisons": comparisons,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
