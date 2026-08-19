#!/usr/bin/env python3
"""Generate gicp board repeated summary and Evidence Doctor manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path


MD_CASE_RE = re.compile(
    r"^\|\s*`?(?P<case>[^`|]+?)`?\s*\|\s*(?P<std>[0-9.]+)\s*\|\s*(?P<rvv>[0-9.]+)\s*\|\s*(?P<speed>[0-9.]+)x\s*\|"
)
STD_ROW_RE = re.compile(
    r"^(?P<case>[A-Za-z0-9_.-]+)\s+\|\s+Std\s+\|\s+(?P<std>[0-9.]+)\s+ms\s+\|"
)
RVV_ROW_RE = re.compile(r"^\s*\|\s+RVV\s+\|\s+(?P<rvv>[0-9.]+)\s+ms\s+\|")


CASE_METADATA = {
    "residual-mahalanobis-dense": {
        "component": "residual / Mahalanobis dense-row accumulation",
        "implementation_shape": "RVV f64m4 dense-row reduction",
        "row_source": "post-correspondence dense-row",
        "timer_boundary": "pre-production component helper; excludes KdTree, correspondence search, solver and output",
    },
    "residual-indexed-gather": {
        "component": "residual / Mahalanobis indexed-gather accumulation",
        "implementation_shape": "RVV f64m4 indexed loads for source/target/Mahalanobis rows",
        "row_source": "post-correspondence indexed gather",
        "timer_boundary": "pre-production component helper; models tmp_idx_src_/tmp_idx_tgt_ gather but excludes solver and output",
    },
    "dfddf-loop-dense": {
        "component": "dfddf Hessian loop dense-row accumulation",
        "implementation_shape": "RVV f64m4 dense-row reduction for dfddf loop accumulators",
        "row_source": "post-correspondence dense-row",
        "timer_boundary": "pre-production component helper; covers dfddf per-correspondence accumulator loop and excludes derivative post-processing / eigensolver",
    },
    "covariance-post-knn-default-k": {
        "component": "covariance post-KNN accumulation",
        "implementation_shape": "RVV f64m1 post-KNN reduction",
        "row_source": "post-nearestKSearch neighborhood rows",
        "timer_boundary": "pre-production component helper; excludes KdTree nearestKSearch and 3x3 SVD",
    },
    "production-public-align-pointxyz": {
        "component": "production public GICP PointXYZ align",
        "implementation_shape": "public GICP with RVV dfddfLoopRVV probe",
        "row_source": "public setInputSource/setInputTarget/align",
        "solve": "included",
        "checksum_policy": "convergence flag, fitness score, final transform and output size",
        "timer_boundary": "production-public bench; constructs GICP object, sets source/target and calls align per iteration; excludes cloud construction",
    },
}


def sha256_file(path: Path) -> str:
    if not path.exists():
        return "missing"
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_compare_log(path: Path) -> dict[str, tuple[float, float, float]]:
    cases: dict[str, tuple[float, float, float]] = {}
    pending_case: str | None = None
    pending_std: float | None = None
    if not path.exists():
        return cases
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = MD_CASE_RE.match(line.strip())
        if not match:
            std_match = STD_ROW_RE.match(line)
            if std_match:
                pending_case = std_match.group("case").strip()
                pending_std = float(std_match.group("std"))
                continue
            rvv_match = RVV_ROW_RE.match(line)
            if rvv_match and pending_case is not None and pending_std is not None:
                rvv_ms = float(rvv_match.group("rvv"))
                speedup = pending_std / rvv_ms if rvv_ms else 0.0
                cases[pending_case] = (pending_std, rvv_ms, speedup)
                pending_case = None
                pending_std = None
            continue
        name = match.group("case").strip()
        cases[name] = (
            float(match.group("std")),
            float(match.group("rvv")),
            float(match.group("speed")),
        )
    return cases


def parse_run_context(path: Path) -> tuple[int | None, int | None]:
    iterations: int | None = None
    warmup: int | None = None
    if not path.exists():
        return iterations, warmup
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("Iterations:"):
            iterations = int(line.split(":", 1)[1].strip())
        elif line.startswith("Warmup Iterations:"):
            warmup = int(line.split(":", 1)[1].strip())
    return iterations, warmup


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
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--expected-runs", type=int, required=True)
    parser.add_argument("--std-bin", required=True)
    parser.add_argument("--rvv-bin", required=True)
    parser.add_argument("--device-label", required=True)
    parser.add_argument("--vlen-desc", required=True)
    parser.add_argument("--evidence-role", default="pre_production_diagnostic")
    parser.add_argument("--boundary", default="test helper")
    parser.add_argument(
        "--decision-question",
        default="RVV-vs-scalar pre-production component diagnostic",
    )
    parser.add_argument(
        "--summary-title",
        default="GICP component diagnostic board repeated summary",
    )
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    per_case: dict[str, list[dict[str, float | str]]] = {}
    iterations: int | None = None
    warmup_iterations: int | None = None
    run_dirs = sorted(path for path in input_dir.glob("run-*") if path.is_dir())
    for run_dir in run_dirs:
      run_iterations, run_warmup = parse_run_context(run_dir / "run_bench_std.log")
      iterations = iterations if iterations is not None else run_iterations
      warmup_iterations = warmup_iterations if warmup_iterations is not None else run_warmup
      cases = parse_compare_log(run_dir / "analyze_bench_compare.log")
      for name, (std_ms, rvv_ms, speedup) in cases.items():
          per_case.setdefault(name, []).append(
              {"run": run_dir.name, "std_ms": std_ms, "rvv_ms": rvv_ms, "speedup": speedup}
          )

    summary_lines = [
        f"# {args.summary_title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(run_dirs)}`",
        f"- device: `{args.device_label}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{args.boundary}`",
        f"- decision_question: `{args.decision_question}`",
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
        decision_bucket = bucket(values)
        summary_lines.append(
            f"| `{name}` | {len(values)} | {statistics.median(values):.3f}x | "
            f"{min(values):.3f}x | {max(values):.3f}x | {below_one}/{len(values)} | "
            f"`{decision_bucket}` |"
        )
        metadata = CASE_METADATA.get(name, {})
        common_side = {
            "boundary": args.boundary,
            "wrapper": metadata.get("component", "bench_gicp"),
            "row_source": metadata.get("row_source", "unknown"),
            "solve": metadata.get("solve", "excluded"),
            "checksum_policy": metadata.get(
                "checksum_policy", "component output checksum in bench log"
            ),
            "timer_boundary": metadata.get("timer_boundary", "pre-production component helper"),
            "gate": "__RVV10__ build switch",
            "mask": "none; deterministic finite samples",
            "reduction": "component reduction",
            "asm_boundary": "bench_gicp_rvv component helper",
        }
        comparisons.append(
            {
                "name": name,
                "case_kind": args.evidence_role,
                "evidence_role": args.evidence_role,
                "component": metadata.get("component", name),
                "implementation_shape": metadata.get("implementation_shape", "unknown"),
                "iterations": iterations,
                "warmup_iterations": warmup_iterations,
                "run_count": len(values),
                "baseline": {
                    "label": "std",
                    **common_side,
                    "reduction": "scalar order",
                    "std_ms": statistics.median(std_values),
                },
                "candidate": {
                    "label": "rvv",
                    **common_side,
                    "reduction": "RVV chunk reduction",
                    "rvv_ms": statistics.median(rvv_values),
                },
                "ba_values": values,
            }
        )

    summary_lines += ["", "## Evidence boundary", ""]
    if args.evidence_role == "production_public":
        summary_lines += [
            "This summary is production-public evidence for the explicit benchmarked",
            "PointXYZ entry only. It does not prove generic point-type coverage or",
            "clean adoption without PI5 user confirmation.",
        ]
    else:
        summary_lines += [
            "This summary is pre-production diagnostic evidence. It can support a bounded",
            "production probe only after public-entry profile and fallback / dispatch planning.",
        ]
    (output_dir / "summary.md").write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "topic": "gicp",
        "module": "registration",
        "run_label": args.run_label,
        "evidence_role": args.evidence_role,
        "boundary": args.boundary,
        "decision_question": args.decision_question,
        "summary": {
            "iterations": iterations,
            "warmup_iterations": warmup_iterations,
            "run_count": len(run_dirs),
            "device": args.device_label,
            "taskset": "not_recorded",
            "governor": "not_recorded",
            "freq": "not_recorded",
            "temperature": "not_recorded",
            "vlen": args.vlen_desc,
            "binary_hash": {
                "std": sha256_file(Path(args.std_bin)),
                "rvv": sha256_file(Path(args.rvv_bin)),
            },
        },
        "comparisons": comparisons,
    }
    (output_dir / "evidence_manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
