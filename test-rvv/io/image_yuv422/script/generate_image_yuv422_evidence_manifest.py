#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for image_yuv422 board bench logs.

本脚本只解析 image_yuv422 topic 的 bench 输出。它把 Std/RVV 日志中的
case label、avg time、checksum 和像素规模转成通用 Evidence Doctor
（证据体检）manifest，避免把 loose Markdown summary 当成完整证据。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+),\s*"
    r"pixels:\s*(?P<pixels>[0-9]+),\s*line_step:\s*(?P<line_step>[0-9]+)"
)
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+),\s*Warmup:\s*(?P<warmup>[0-9]+)")
DATASET_RE = re.compile(r"Dataset:\s*(?P<dataset>.+)")


def parse_log(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8").splitlines()
    cases: dict[str, dict] = {}
    iterations = None
    warmup = None
    dataset = None
    index = 0
    while index < len(lines):
        line = lines[index]
        if iterations is None:
            match = ITER_RE.search(line)
            if match:
                iterations = int(match.group("iterations"))
                warmup = int(match.group("warmup"))
        if dataset is None:
            match = DATASET_RE.search(line)
            if match:
                dataset = match.group("dataset").strip()
        match = CASE_RE.match(line)
        if match and index + 1 < len(lines):
            total = TOTAL_RE.search(lines[index + 1])
            if total:
                cases[match.group("name")] = {
                    "avg_ms": float(match.group("avg")),
                    "total_ms": float(total.group("total")),
                    "checksum": total.group("checksum"),
                    "pixels": int(total.group("pixels")),
                    "line_step": int(total.group("line_step")),
                }
                index += 1
        index += 1
    if iterations is None or warmup is None:
        raise ValueError(f"missing iterations/warmup in {path}")
    return {
        "path": str(path),
        "iterations": iterations,
        "warmup_iterations": warmup,
        "dataset": dataset or "unknown",
        "cases": cases,
    }


def parse_log_pair(std_log: Path, rvv_log: Path) -> tuple[dict, dict]:
    return parse_log(std_log), parse_log(rvv_log)


def collect_log_pairs(args: argparse.Namespace) -> list[tuple[dict, dict]]:
    if args.repeat_dir is None:
        if args.std_log is None or args.rvv_log is None:
            raise ValueError("--std-log and --rvv-log are required when --repeat-dir is not used")
        return [parse_log_pair(args.std_log, args.rvv_log)]

    pairs: list[tuple[dict, dict]] = []
    for run_dir in sorted(args.repeat_dir.glob("run*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            pairs.append(parse_log_pair(std_log, rvv_log))
    if not pairs:
        raise ValueError(f"no run*/run_bench_std.log + run_bench_rvv.log pairs found in {args.repeat_dir}")
    return pairs


def row_source_for(name: str) -> str:
    if "downsample" in name:
        return "image_downsample_stride"
    return "image_row_contiguous"


def case_kind_for(name: str) -> str:
    if name.startswith("prod_"):
        return "production_direct"
    return "production_shaped"


def evidence_role_for(name: str) -> str:
    if name.startswith("prod_"):
        return "production_public"
    return "production_shaped_diagnostic"


def boundary_for(name: str) -> str:
    if name.startswith("prod_"):
        return "public_overload"
    return "test_helper"


def wrapper_for(name: str) -> str:
    if name.startswith("prod_"):
        return "pcl::io::ImageYUV422"
    return "image_yuv422_support"


def baseline_label_for(name: str) -> str:
    if name.startswith("prod_"):
        return "production-public-scalar"
    return "scalar-reference"


def candidate_label_for(name: str) -> str:
    if name.startswith("prod_"):
        return "production-public-rvv"
    return "rvv-candidate"


def asm_boundary_for(name: str) -> str:
    if name.startswith("prod_"):
        return "pcl::io::ImageYUV422::fillRGB"
    return "bench_image_yuv422_rvv"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--repeat-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    log_pairs = collect_log_pairs(args)
    first_std, first_rvv = log_pairs[0]
    run_count = len(log_pairs)
    case_names = set(first_std["cases"]) & set(first_rvv["cases"])
    for std, rvv in log_pairs[1:]:
        case_names &= set(std["cases"]) & set(rvv["cases"])
    comparisons = []
    for name in sorted(case_names):
        std_cases = [std["cases"][name] for std, _ in log_pairs]
        rvv_cases = [rvv["cases"][name] for _, rvv in log_pairs]
        std_checksums = {case["checksum"] for case in std_cases}
        rvv_checksums = {case["checksum"] for case in rvv_cases}
        if len(std_checksums) != 1 or len(rvv_checksums) != 1:
            raise ValueError(f"unstable checksum across repeated logs for {name}")
        std_ms_values = [case["avg_ms"] for case in std_cases]
        rvv_ms_values = [case["avg_ms"] for case in rvv_cases]
        ba_values = [std_ms / rvv_ms for std_ms, rvv_ms in zip(std_ms_values, rvv_ms_values)]
        std_case = std_cases[-1]
        rvv_case = rvv_cases[-1]
        evidence_role = evidence_role_for(name)
        boundary = boundary_for(name)
        wrapper = wrapper_for(name)
        comparisons.append({
            "name": name,
            "group": "image_yuv422",
            "evidence_role": evidence_role,
            "case_kind": case_kind_for(name),
            "point_type": "not_applicable",
            "size": std_case["pixels"],
            "run_count": run_count,
            "baseline": {
                "label": baseline_label_for(name),
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "output_bytes_fnv1a",
                "checksum": std_case["checksum"],
                "timer_boundary": "conversion_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": "scalar_reference",
                "rvv_instr_count": 0,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(std_ms_values),
            },
            "candidate": {
                "label": candidate_label_for(name),
                "boundary": boundary,
                "wrapper": wrapper,
                "row_source": row_source_for(name),
                "solve": False,
                "checksum_policy": "output_bytes_fnv1a",
                "checksum": rvv_case["checksum"],
                "timer_boundary": "conversion_only_checksum_after_timing",
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": asm_boundary_for(name),
                "rvv_instr_count": None,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(rvv_ms_values),
            },
            "ba_values": ba_values,
        })

    summary_role = "mixed_production_shaped_and_public"
    if comparisons:
        roles = {comparison["evidence_role"] for comparison in comparisons}
        if len(roles) == 1:
            summary_role = next(iter(roles))

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "image_yuv422 board smoke",
            "evidence_role": summary_role,
            "summary_path": str(args.summary_log),
            "label": "image_yuv422 board smoke",
            "analysis_script": "test-rvv/io/image_yuv422/script/generate_image_yuv422_evidence_manifest.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "dataset": first_rvv["dataset"],
                "iterations": first_rvv["iterations"],
                "warmup_iterations": first_rvv["warmup_iterations"],
                "run_count": run_count,
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see board / ELF",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {
            "strict_ab": True,
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
            "thresholds": {
                "min_repeated_runs": 5,
                "ba_degrade_threshold": 1.0,
                "high_degrade_fraction": 0.2,
                "direction_epsilon_fraction": 0.02,
            },
        },
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
