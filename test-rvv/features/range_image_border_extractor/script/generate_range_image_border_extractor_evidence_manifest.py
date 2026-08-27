#!/usr/bin/env python3
"""Generate RangeImageBorderExtractor repeated board summary and Evidence Doctor manifest."""

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
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s+checksum:\s+(?P<checksum>[-+0-9.eE]+)", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations\s*:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup(?: Iterations)?\s*:\s*(?P<warmup>\d+)", re.MULTILINE)


def parse_summary(path: Path) -> tuple[dict[str, object], list[dict[str, object]]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, object] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    if match := re.search(r"^\s*Dataset\s*:\s*(.+?)\s*$", text, re.MULTILINE):
        context["dataset"] = match.group(1).strip()
    return context, [
        {
            "name": match.group("name"),
            "std_ms": float(match.group("std_ms")),
            "rvv_ms": float(match.group("rvv_ms")),
            "speedup": float(match.group("speedup")),
            "summary_path": str(path),
        }
        for match in CASE_RE.finditer(text)
    ]


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


def decision_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.10 and below_one == 0:
        return "positive"
    if median >= 1.03 and below_one <= max(1, len(values) // 5):
        return "weak_positive"
    if 0.97 <= median < 1.03:
        return "neutral"
    if median < 0.97:
        return "negative"
    return "unstable"


def unique_or_mixed(values: list[object | None]) -> object | str | None:
    present = [value for value in values if value is not None]
    if not present:
        return None
    first = present[0]
    return first if all(value == first for value in present) else "mixed"


def metadata_for_case(name: str) -> dict[str, str]:
    size = {
        "score_update_320x240": "320x240 contiguous float score image",
        "score_update_641x481_tail": "641x481 contiguous float score image with RVV tail lanes",
        "score_update_threshold_sign_mix": "257x193 contiguous float score image with lower threshold and sign-mix gates",
        "score_pipeline_641x481_four_images": "four 641x481 contiguous float score images with RVV tail lanes",
        "range_image_score_generation_160x120": "160x120 RangeImage fixture with production score generation only",
        "range_image_generation_plus_update_160x120": "160x120 RangeImage fixture with production score generation plus four-image score update",
        "range_image_local_surface_160x120": "160x120 RangeImage fixture with extractLocalSurfaceStructure only",
        "range_image_border_scores_after_surface_160x120": "160x120 RangeImage fixture with local surface precomputed before timed extractBorderScoreImages",
    }.get(name, "synthetic contiguous float score image")
    if name == "score_pipeline_641x481_four_images":
        group = "range-image-border-four-score-pipeline"
    elif name.startswith("range_image_"):
        group = "range-image-border-production-shaped-score-generation"
    else:
        group = "range-image-border-score-update"
    evidence_role = "production-shaped diagnostic" if name.startswith("range_image_") else "diagnostic"
    timer_boundary = (
        "four-direction updatedScoresAccordingToNeighborValues-style score propagation"
        if name == "score_pipeline_641x481_four_images"
        else "RangeImageBorderExtractor extractBorderScoreImages production score generation plus four-direction test helper score propagation"
        if name == "range_image_generation_plus_update_160x120"
        else "RangeImageBorderExtractor extractLocalSurfaceStructure only"
        if name == "range_image_local_surface_160x120"
        else "RangeImageBorderExtractor extractBorderScoreImages only after precomputed local surface"
        if name == "range_image_border_scores_after_surface_160x120"
        else "RangeImageBorderExtractor extractBorderScoreImages production score generation only"
        if name == "range_image_score_generation_160x120"
        else "updatedScoresAccordingToNeighborValues-style 3x3 score propagation only"
    )
    notes = (
        "Production-shaped diagnostic evidence. It includes the real RangeImageBorderExtractor score-image generation boundary, but the RVV update remains a test helper and production dispatch is not wired."
        if name == "range_image_generation_plus_update_160x120"
        else "Production-shaped diagnostic baseline. It includes real RangeImageBorderExtractor score-image generation only and should stay near 1x because no RVV update is timed."
        if name == "range_image_score_generation_160x120"
        else "Production-shaped diagnostic component split. It isolates local surface extraction or border-score image generation to decide whether neighbor-score RVV is worth a follow-up candidate."
        if name in {"range_image_local_surface_160x120", "range_image_border_scores_after_surface_160x120"}
        else "Diagnostic component evidence only. It does not include RangeImage point lookup, LocalSurface classification, shadow/veil state, output construction, or production dispatch."
    )
    return {
        "group": group,
        "evidence_role": evidence_role,
        "size": size,
        "point_type": "not_applicable; score image is contiguous float buffer",
        "timer_boundary": timer_boundary,
        "row_source": "RangeImage fixture generated score images"
        if name.startswith("range_image_")
        else "synthetic row-major border score image",
        "gate": "width/height >= 3 interior RVV path; border rows and columns use scalar reference",
        "mask": "minimum-border-probability gate plus opposite-sign neighbor-average gate",
        "reduction": "fixed 8-neighbor local sum; no cross-lane vector reduction",
        "checksum_policy": "weighted output score checksum from every repeated update",
        "asm_boundary": "updateScoresRVV in bench_range_image_border_extractor_rvv",
        "notes": notes,
    }


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle32", "vse32", "vsetvli", "vfadd", "vfmul", "vfsub", "vfabs", "vmflt", "vmerge")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(op in line for op in needles)
    )


def sha256_file(path: Path) -> str | None:
    if not path.exists():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return f"sha256:{digest.hexdigest()}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--title", default="RangeImageBorderExtractor score-update diagnostic repeated board summary")
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--std-binary", type=Path)
    parser.add_argument("--rvv-binary", type=Path)
    args = parser.parse_args()

    summary_paths = sorted(args.summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not summary_paths:
        raise SystemExit(f"no run_*/analyze_bench_compare.log files found under {args.summary_dir}")

    contexts: list[dict[str, object]] = []
    grouped: dict[str, dict[str, list[float] | list[str]]] = {}
    for path in summary_paths:
        context, comparisons = parse_summary(path)
        contexts.append(context)
        for item in comparisons:
            bucket = grouped.setdefault(str(item["name"]), {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(float(item["std_ms"]))  # type: ignore[union-attr]
            bucket["rvv"].append(float(item["rvv_ms"]))  # type: ignore[union-attr]
            bucket["speedup"].append(float(item["speedup"]))  # type: ignore[union-attr]
            bucket["paths"].append(str(item["summary_path"]))  # type: ignore[union-attr]

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
        "- evidence_role: `diagnostic`",
        "- A/B boundary: `test helper`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    comparisons_manifest = []
    for name, values in sorted(grouped.items()):
        if args.case_name and name != args.case_name:
            continue
        speedups = list(values["speedup"])  # type: ignore[arg-type]
        std_values = list(values["std"])  # type: ignore[arg-type]
        rvv_values = list(values["rvv"])  # type: ignore[arg-type]
        paths = list(values["paths"])  # type: ignore[arg-type]
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
            "boundary": "test_helper",
            "wrapper": "test-rvv/features/range_image_border_extractor/src/bench_range_image_border_extractor.cpp",
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
        std_binary_hash = sha256_file(args.std_binary) if args.std_binary else None
        rvv_binary_hash = sha256_file(args.rvv_binary) if args.rvv_binary else None
        evidence_role = str(meta["evidence_role"])
        comparisons_manifest.append(
            {
                "name": name,
                "group": meta["group"],
                "evidence_role": evidence_role,
                "case_kind": "component-ablation",
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": len(speedups),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "build_comparison": True,
                "std_ms": statistics.mean(std_values),
                "rvv_ms": statistics.mean(rvv_values),
                "ba_values": speedups,
                "std_ms_values": std_values,
                "rvv_ms_values": rvv_values,
                "run_paths": paths,
                "binary_hash": f"std={std_binary_hash}; rvv={rvv_binary_hash}",
                "baseline": {
                    "label": "Std build",
                    "checksum": unique_or_mixed(std_checksums),
                    "binary_hash": std_binary_hash,
                    **common,
                },
                "candidate": {
                    "label": "RVV build",
                    "checksum": unique_or_mixed(rvv_checksums),
                    "binary_hash": rvv_binary_hash,
                    **common,
                },
                "notes": meta["notes"],
            }
        )

    if not comparisons_manifest:
        raise SystemExit("no benchmark comparisons matched the requested case")

    summary_lines[5] = f"- evidence_role: `{comparisons_manifest[0]['evidence_role']}`"
    summary_lines += [
        "",
        "## Evidence boundary",
        "",
        "这是未接 production（生产源码）的 score-update diagnostic（诊断）性能证据。Std/RVV 两侧共享同一个 bench wrapper；Std build 走 scalar reference，RVV build 走 test-only RVV candidate。RangeImage case 会把真实 extractBorderScoreImages() 计入边界，但仍不代表 production dispatch 已接入，也不覆盖完整 public computeFeature 的后续 shadow/veil 和输出构造。",
    ]
    args.summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    first_context = contexts[0] if contexts else {}
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": comparisons_manifest[0]["evidence_role"],
            "summary_path": str(args.summary_output),
            "summary_paths": [str(path) for path in summary_paths],
            "label": args.run_label,
            "analysis_script": "test-rvv/features/range_image_border_extractor/script/generate_range_image_border_extractor_evidence_manifest.py",
            "metadata": {
                "device": "board via Makefile config",
                "dataset": first_context.get("dataset", "synthetic range image border score images"),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "run_count": len(summary_paths),
                "expected_runs": args.expected_runs,
                "decision_bucket": decision_bucket(list(comparisons_manifest[0]["ba_values"])),
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
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.summary_output}")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
