#!/usr/bin/env python3
"""把 Harris 2D repeated board 输出整理成 Evidence Doctor 可读的 manifest。"""

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
CHECKSUM_RE = re.compile(
    r"^(?P<name>[A-Za-z0-9_]+):\s+[0-9.]+\s+ms/iter\s*\n"
    r"\s+Total Time:.*?checksum:\s*(?P<checksum>[0-9]+)",
    re.MULTILINE,
)
CORRECTNESS_RE = re.compile(
    r"^(?P<name>[A-Za-z0-9_]+):\s+[0-9.]+\s+ms/iter\s*\n"
    r"\s+Total Time:.*?checksum:\s*(?P<checksum>[0-9]+).*?\n"
    r"\s+Correctness:\s+max_abs_error:\s*(?P<max_abs_error>[0-9.eE+-]+),\s*"
    r"tolerance:\s*(?P<tolerance>[0-9.eE+-]+),\s*"
    r"tolerance_pass:\s*(?P<tolerance_pass>yes|no)",
    re.MULTILINE,
)
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup(?:\s+Iterations)?\s*:\s*(?P<warmup>\d+)", re.MULTILINE)


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


def parse_context(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    return context


def parse_checksums(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    return {match.group("name"): match.group("checksum") for match in CHECKSUM_RE.finditer(text)}


def parse_correctness(path: Path) -> dict[str, dict[str, Any]]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    results: dict[str, dict[str, Any]] = {}
    for match in CORRECTNESS_RE.finditer(text):
        results[match.group("name")] = {
            "raw_response_checksum": match.group("checksum"),
            "max_abs_error": float(match.group("max_abs_error")),
            "tolerance": float(match.group("tolerance")),
            "tolerance_pass": match.group("tolerance_pass") == "yes",
        }
    return results


def checksum_for_case(path: str, case_name: str) -> str | None:
    return parse_checksums(Path(path).with_name("run_bench_std.log")).get(case_name)


def rvv_checksum_for_case(path: str, case_name: str) -> str | None:
    return parse_checksums(Path(path).with_name("run_bench_rvv.log")).get(case_name)


def correctness_for_case(path: str, build: str, case_name: str) -> dict[str, Any] | None:
    log_name = "run_bench_std.log" if build == "std" else "run_bench_rvv.log"
    return parse_correctness(Path(path).with_name(log_name)).get(case_name)


def combine_correctness(items: list[dict[str, Any] | None]) -> dict[str, Any] | None:
    present = [item for item in items if item is not None]
    if not present:
        return None
    max_errors = [float(item["max_abs_error"]) for item in present]
    tolerances = [float(item["tolerance"]) for item in present]
    passes = [bool(item["tolerance_pass"]) for item in present]
    return {
        "max_abs_error": max(max_errors),
        "tolerance": unique_or_mixed(tolerances),
        "tolerance_pass": all(passes),
        "sample_count": len(present),
    }


def semantic_checksum(
    case_name: str, correctness: dict[str, Any] | None, raw_checksum: Any, side: str
) -> str | None:
    if correctness is None:
        return str(raw_checksum) if raw_checksum is not None else None
    tolerance = correctness["tolerance"]
    tolerance_text = f"{tolerance:g}" if isinstance(tolerance, float) else str(tolerance)
    pass_text = "pass" if correctness["tolerance_pass"] else f"fail:{side}"
    return f"semantic_response_tolerance:{case_name}:tolerance={tolerance_text}:{pass_text}"


def metadata_for_case(name: str, args: argparse.Namespace) -> dict[str, str]:
    sizes = {
        "harris2d_harris_320x240": "320x240 organized synthetic intensity image, Harris formula, 3x3 configured window",
        "harris2d_tomasi_320x240": "320x240 organized synthetic intensity image, Tomasi sqrt formula, 3x3 configured window",
        "harris2d_noble_tail_641x481": "641x481 organized synthetic intensity image with RVV tail lanes, Noble formula, 5x5 configured window",
    }
    if name not in sizes:
        raise ValueError(f"unknown Harris 2D case label: {name}")
    return {
        "group": "harris_2d_response_map",
        "case_kind": args.case_kind,
        "evidence_role": args.evidence_role,
        "point_type": args.point_type,
        "row_source": "organized_full_image_no_indices",
        "boundary": args.ab_boundary,
        "wrapper": "test-rvv/keypoints/harris_2d/src/bench_harris_2d.cpp",
        "timer_boundary": args.timer_boundary,
        "gate": "organized width*height image, window_width/window_height odd and >=3 in production-shaped inputs, __RVV10__ build must hit RVV candidate",
        "mask": "finite xyz gate keeps non-finite point response at zero; derivative NaN propagation follows production comment",
        "reduction": "per-pixel window accumulation of ix*ix, ix*iy and iy*iy followed by Harris/Noble/Lowe/Tomasi response",
        "checksum_policy": "semantic response tolerance fingerprint; raw_response_checksum retains FNV-style response intensity drift",
        "asm_boundary": args.asm_boundary,
        "size": sizes[name],
        "notes": args.notes,
    }


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle32", "vse32", "vlse32", "vfmacc", "vfsqrt", "vfdiv", "vfmul", "vfadd", "vfsub")
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
    parser.add_argument("--title", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--evidence-role", default="production_shaped_diagnostic")
    parser.add_argument("--case-kind", default="production_shaped_diagnostic")
    parser.add_argument("--ab-boundary", default="test_helper")
    parser.add_argument("--point-type", default="test synthetic organized float intensity image")
    parser.add_argument(
        "--timer-boundary",
        default=(
            "computeResponsesCandidate includes derivative map, second-moment accumulation "
            "and response formula; excludes NMS sort, occupancy map and public dispatch"
        ),
    )
    parser.add_argument("--asm-boundary", default="harris_2d test-support candidate / inlined bench_harris_2d_rvv")
    parser.add_argument(
        "--baseline-asm-boundary",
        default="scalar public compute build; no responseRVV dispatch expected",
    )
    parser.add_argument(
        "--notes",
        default=(
            "Production-shaped diagnostic only; production direct evidence requires harris_2d.hpp "
            "dispatch patch and public entry tests."
        ),
    )
    args = parser.parse_args()

    summary_paths = sorted(args.summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not summary_paths:
        raise SystemExit(f"no run_*/analyze_bench_compare.log files found under {args.summary_dir}")

    contexts: list[dict[str, Any]] = []
    grouped: dict[str, dict[str, list[Any]]] = {}
    for path in summary_paths:
        text = path.read_text(encoding="utf-8", errors="replace")
        contexts.append(parse_context(path))
        for item in CASE_RE.finditer(text):
            name = item.group("name")
            metadata_for_case(name, args)
            bucket = grouped.setdefault(name, {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(float(item.group("std_ms")))
            bucket["rvv"].append(float(item.group("rvv_ms")))
            bucket["speedup"].append(float(item.group("speedup")))
            bucket["paths"].append(str(path))

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
        f"- A/B boundary: `{args.ab_boundary}`",
        f"- asm_rvv_line_count: `{asm_lines}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    comparisons = []
    all_speedups: list[float] = []
    for case_name, stats in sorted(grouped.items()):
        speedups = [float(value) for value in stats["speedup"]]
        std_values = [float(value) for value in stats["std"]]
        rvv_values = [float(value) for value in stats["rvv"]]
        paths = [str(value) for value in stats["paths"]]
        std_checksums = [checksum_for_case(path, case_name) for path in paths]
        rvv_checksums = [rvv_checksum_for_case(path, case_name) for path in paths]
        std_raw_checksum = unique_or_mixed(std_checksums)
        rvv_raw_checksum = unique_or_mixed(rvv_checksums)
        std_correctness = combine_correctness(
            [correctness_for_case(path, "std", case_name) for path in paths]
        )
        rvv_correctness = combine_correctness(
            [correctness_for_case(path, "rvv", case_name) for path in paths]
        )
        std_checksum = semantic_checksum(case_name, std_correctness, std_raw_checksum, "std")
        rvv_checksum = semantic_checksum(case_name, rvv_correctness, rvv_raw_checksum, "rvv")
        correctness_pass = (
            std_correctness is not None
            and rvv_correctness is not None
            and bool(std_correctness["tolerance_pass"])
            and bool(rvv_correctness["tolerance_pass"])
        )
        all_speedups.extend(speedups)
        meta = metadata_for_case(case_name, args)
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
        comparisons.append(
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
                "std_ms": statistics.mean(std_values),
                "rvv_ms": statistics.mean(rvv_values),
                "ba_values": speedups,
                "std_checksum": std_checksum,
                "rvv_checksum": rvv_checksum,
                "std_raw_response_checksum": std_raw_checksum,
                "rvv_raw_response_checksum": rvv_raw_checksum,
                "checksum_equal": std_checksum == rvv_checksum,
                "correctness": {
                    "policy": "each build output is compared with scalar reference using the bench tolerance",
                    "pass": correctness_pass,
                    "std": std_correctness,
                    "rvv": rvv_correctness,
                },
                "baseline": {
                    "label": "std",
                    "boundary": meta["boundary"],
                    "wrapper": meta["wrapper"],
                    "row_source": meta["row_source"],
                    "solve": False,
                    "checksum_policy": meta["checksum_policy"],
                    "checksum": std_checksum,
                    "raw_response_checksum": std_raw_checksum,
                    "correctness": std_correctness,
                    "timer_boundary": meta["timer_boundary"],
                    "gate": meta["gate"],
                    "mask": meta["mask"],
                    "reduction": meta["reduction"],
                    "asm_boundary": args.baseline_asm_boundary,
                    "rvv_instr_count": 0,
                },
                "candidate": {
                    "label": "rvv",
                    "boundary": meta["boundary"],
                    "wrapper": meta["wrapper"],
                    "row_source": meta["row_source"],
                    "solve": False,
                    "checksum_policy": meta["checksum_policy"],
                    "checksum": rvv_checksum,
                    "raw_response_checksum": rvv_raw_checksum,
                    "correctness": rvv_correctness,
                    "timer_boundary": meta["timer_boundary"],
                    "gate": meta["gate"],
                    "mask": meta["mask"],
                    "reduction": meta["reduction"],
                    "asm_boundary": meta["asm_boundary"],
                    "rvv_instr_count": asm_lines,
                },
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
                "notes": meta["notes"],
                "source_logs": paths,
            }
        )

    args.summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "run_label": args.run_label,
            "evidence_role": args.evidence_role,
            "expected_runs": args.expected_runs,
            "collected_runs": len(summary_paths),
            "decision_bucket": decision_bucket(all_speedups),
            "summary_path": str(args.summary_output),
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
