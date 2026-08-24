#!/usr/bin/env python3
"""Generate a topic-local Evidence Doctor manifest for normal_3d board compare logs."""

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


def build_comparison(
    name: str,
    std_values: list[float],
    rvv_values: list[float],
    speedup_values: list[float],
    run_paths: list[str],
) -> dict[str, object]:
    is_component = name.startswith("component_")
    boundary = "test helper" if is_component else "public overload"
    case_kind = "component-ablation" if is_component else "production-shaped"
    group = "common-covariance-component" if is_component else "normal-estimation-public"
    solve = True
    timer_boundary = "computePointNormal loop repeated 512x" if is_component else "NormalEstimation::compute"
    checksum = "see matching checksum lines in run_bench_std.log and run_bench_rvv.log"
    std_ms = statistics.mean(std_values)
    rvv_ms = statistics.mean(rvv_values)
    side_common = {
        "boundary": boundary,
        "wrapper": "test-rvv/features/normal_3d/src/bench_normal_3d.cpp",
        "row_source": "dense indexed neighborhood",
        "solve": solve,
        "checksum_policy": "normal checksum line",
        "checksum": checksum,
        "timer_boundary": timer_boundary,
        "gate": "PointXYZ dense k>=3",
        "mask": "production finite checks through NormalEstimation / computePointNormal",
        "reduction": "common covariance reduction",
        "asm_boundary": "computeMeanAndCovarianceMatrixRVV<PointXYZ,float>",
    }
    return {
        "name": name,
        "group": group,
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": case_kind,
        "point_type": "PointXYZ -> Normal",
        "size": 9216,
        "run_count": len(speedup_values),
        "iterations": 20,
        "warmup_iterations": 3,
        "build_comparison": True,
        "std_ms": std_ms,
        "rvv_ms": rvv_ms,
        "ba_values": speedup_values,
        "std_ms_values": std_values,
        "rvv_ms_values": rvv_values,
        "run_paths": run_paths,
        "baseline": {"label": "Std build", "std_ms": std_ms, "rvv_ms": std_ms, **side_common},
        "candidate": {"label": "RVV build", "std_ms": std_ms, "rvv_ms": rvv_ms, **side_common},
        "notes": "Repeated board compare manifest when run_count > 1; evidence remains production-shaped diagnostic, not production direct.",
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
            bucket = grouped.setdefault(
                str(item["name"]),
                {"std": [], "rvv": [], "speedup": [], "paths": []},
            )
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
    summary_label = "normal_3d board repeated compare" if len(summary_paths) > 1 else "normal_3d board smoke compare"
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "normal_3d board Std/RVV compare",
            "evidence_role": "production_shaped_diagnostic",
            "summary_path": str(args.summary or args.summary_dir),
            "summary_paths": [str(path) for path in summary_paths],
            "label": summary_label,
            "analysis_script": "test-rvv/features/normal_3d/script/generate_normal_3d_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "Milkv-Jupiter"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic normal_3d plane grid"),
                "iterations": first_context.get("iterations", 20),
                "warmup_iterations": first_context.get("warmup_iterations", 3),
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
