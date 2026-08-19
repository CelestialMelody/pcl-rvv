#!/usr/bin/env python3
"""Generate ndt board repeated summary and Evidence Doctor manifest."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


MD_CASE_RE = re.compile(
    r"^\|\s*`?(?P<case>[^`|]+?)`?\s*\|\s*(?P<std>[0-9.]+)\s*\|\s*(?P<rvv>[0-9.]+)\s*\|\s*(?P<speed>[0-9.]+)x\s*\|"
)
STD_ROW_RE = re.compile(r"^(?P<case>[A-Za-z0-9_.-]+)\s+\|\s+Std\s+\|\s+(?P<std>[0-9.]+)\s+ms\s+\|")
RVV_ROW_RE = re.compile(r"^\s*\|\s+RVV\s+\|\s+(?P<rvv>[0-9.]+)\s+ms\s+\|")
INT_RE = re.compile(
    r"^(?P<key>Iterations|Warmup Iterations|Samples|Public Points|Public Max Iterations):\s*(?P<value>[0-9]+)\s*$"
)


CASE_METADATA = {
    "derivative-hessian-staged": {
        "component": "NDT derivative accumulation with Hessian update",
        "reduction": "RVV reductions for gradient and 6x6 hessian",
    },
    "derivative-gradient-staged": {
        "component": "NDT derivative accumulation gradient-only line-search path",
        "reduction": "RVV reductions for gradient only",
        "case_kind": "component_diagnostic",
        "evidence_role": "diagnostic",
        "boundary": "test helper",
        "wrapper": "bench_ndt derivative accumulation diagnostic",
        "row_source": "staged point-neighbor samples",
        "solve": "excluded",
        "timer_boundary": "pre-production staged math helper; excludes voxel neighbor search, point derivative precompute, line search and Eigen solver",
    },
    "production-public-align-pointxyz": {
        "component": "NDT public align entry",
        "reduction": "production scalar reduction order; no NDT RVV production patch",
        "case_kind": "production_public_profile",
        "evidence_role": "production_public_profile",
        "boundary": "public overload",
        "wrapper": "bench_ndt production-public align probe",
        "row_source": "real transformed source cloud and voxel neighborhood lookup",
        "solve": "included",
        "timer_boundary": "public align; includes voxel grid target setup inside reg setup only outside timed loop, and includes per-align derivative, line search and solver work",
    },
}


def parse_compare_log(path: Path) -> dict[str, tuple[float, float, float]]:
    cases: dict[str, tuple[float, float, float]] = {}
    pending_case: str | None = None
    pending_std: float | None = None
    if not path.exists():
        return cases
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := MD_CASE_RE.match(line.strip()):
            cases[match.group("case").strip()] = (
                float(match.group("std")),
                float(match.group("rvv")),
                float(match.group("speed")),
            )
            continue
        if match := STD_ROW_RE.match(line):
            pending_case = match.group("case").strip()
            pending_std = float(match.group("std"))
            continue
        if (match := RVV_ROW_RE.match(line)) and pending_case is not None and pending_std is not None:
            rvv_ms = float(match.group("rvv"))
            cases[pending_case] = (pending_std, rvv_ms, pending_std / rvv_ms if rvv_ms else 0.0)
            pending_case = None
            pending_std = None
    return cases


def parse_run_context(path: Path) -> dict[str, int]:
    context: dict[str, int] = {}
    if not path.exists():
        return context
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := INT_RE.match(line.strip()):
            context[match.group("key").lower().replace(" ", "_")] = int(match.group("value"))
    return context


def bucket(values: list[float]) -> str:
    if not values:
        return "missing"
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.20 and below_one == 0:
        return "positive"
    if median >= 1.05 and below_one <= max(1, len(values) // 5):
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--expected-runs", type=int, required=True)
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--boundary", default="test helper")
    parser.add_argument(
        "--timer-boundary",
        default="excludes voxel neighbor search, point derivative precompute, line search, Eigen solver and production dispatch.",
    )
    parser.add_argument(
        "--title",
        default="NDT board repeated summary",
    )
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    run_dirs = sorted(path for path in input_dir.glob("run-*") if path.is_dir())
    per_case: dict[str, list[dict[str, float | str]]] = {}
    context: dict[str, int] = {}
    for run_dir in run_dirs:
        if not context:
            context = parse_run_context(run_dir / "run_bench_std.log")
        for name, (std_ms, rvv_ms, speedup) in parse_compare_log(run_dir / "analyze_bench_compare.log").items():
            per_case.setdefault(name, []).append(
                {"run": run_dir.name, "std_ms": std_ms, "rvv_ms": rvv_ms, "speedup": speedup}
            )

    lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(run_dirs)}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{args.boundary}`",
        f"- timer_boundary: {args.timer_boundary}",
        "",
        "| case | runs | median speedup | min | max | B/A < 1 | decision bucket |",
        "| --- | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    comparisons = []
    for name in sorted(per_case):
        values = [float(item["speedup"]) for item in per_case[name]]
        std_values = [float(item["std_ms"]) for item in per_case[name]]
        rvv_values = [float(item["rvv_ms"]) for item in per_case[name]]
        below_one = sum(1 for value in values if value < 1.0)
        decision = bucket(values)
        lines.append(
            f"| `{name}` | {len(values)} | {statistics.median(values):.3f}x | "
            f"{min(values):.3f}x | {max(values):.3f}x | {below_one}/{len(values)} | `{decision}` |"
        )
        metadata = CASE_METADATA.get(name, {})
        common = {
            "boundary": metadata.get("boundary", "test helper"),
            "wrapper": metadata.get("wrapper", "bench_ndt derivative accumulation diagnostic"),
            "row_source": metadata.get("row_source", "staged point-neighbor samples"),
            "solve": metadata.get("solve", "excluded"),
            "checksum_policy": "candidate result checksum in bench log",
            "timer_boundary": metadata.get(
                "timer_boundary",
                "pre-production staged math helper; excludes voxel neighbor search, point derivative precompute, line search and Eigen solver",
            ),
            "gate": "__RVV10__ build switch",
            "mask": "invalid gaussian weight skip",
            "asm_boundary": "bench_ndt_rvv filtered asm; helper may be inlined",
        }
        comparisons.append(
            {
                "name": name,
                "case_kind": metadata.get("case_kind", "component_diagnostic"),
                "evidence_role": metadata.get("evidence_role", "diagnostic"),
                "component": metadata.get("component", name),
                "iterations": context.get("iterations"),
                "warmup_iterations": context.get("warmup_iterations"),
                "run_count": len(values),
                "baseline": {
                    "label": "std",
                    **common,
                    "reduction": "scalar order",
                    "std_ms": statistics.median(std_values),
                },
                "candidate": {
                    "label": "rvv",
                    **common,
                    "reduction": metadata.get("reduction", "RVV vector reduction"),
                    "rvv_ms": statistics.median(rvv_values),
                },
                "ba_values": values,
            }
        )

    lines += [
        "",
        "## Evidence boundary",
        "",
        "This is pre-production diagnostic evidence. It supports only a bounded",
        "production probe discussion after staging, fallback and public-entry costs are audited.",
    ]
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "topic": "ndt",
        "module": "registration",
        "run_label": args.run_label,
        "evidence_role": args.evidence_role,
        "boundary": args.boundary,
        "decision_question": "NDT public-entry profile or staged derivative diagnostic, depending on case metadata",
        "summary": {
            "iterations": context.get("iterations"),
            "warmup_iterations": context.get("warmup_iterations"),
            "samples": context.get("samples"),
            "public_points": context.get("public_points"),
            "public_max_iterations": context.get("public_max_iterations"),
            "run_count": len(run_dirs),
            "device": "board",
            "taskset": "not_recorded",
            "governor": "not_recorded",
            "freq": "not_recorded",
            "temperature": "not_recorded",
            "vlen": "see SoC / ELF (board)",
            "binary_hash": "not_recorded",
        },
        "comparisons": comparisons,
    }
    (output.parent / "evidence_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
