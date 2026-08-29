#!/usr/bin/env python3
"""Generate Hough3D vote-generation summary and Evidence Doctor manifest.

本脚本解析当前 topic 的 repeated board（重复板卡测试）日志，把每个
run_* 目录里的 Std/RVV benchmark 输出转成 `summary.md` 和
`evidence_manifest.json`，供全局 Evidence Doctor（证据体检）脚本检查。
当前 phase 输出是 production-shaped diagnostic（生产形态诊断）证据：
它证明 vote generation 候选是否值得继续，不证明真实 production dispatch。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = next(path for path in (TOPIC_ROOT, *TOPIC_ROOT.parents) if (path / ".git").exists())

DATASET_RE = re.compile(r"^Dataset:\s+(?P<dataset>.+?)\s*$")
ITERATIONS_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s+(?P<warmup>\d+)")
VOTE_COUNT_RE = re.compile(r"^Vote Count:\s+(?P<vote_count>\d+)")
INTERP_RE = re.compile(r"^Use Interpolation:\s+(?P<value>[01])")
DIST_WEIGHT_RE = re.compile(r"^Use Distance Weight:\s+(?P<value>[01])")
CHECKSUM_RE = re.compile(r"^checksum:\s+(?P<checksum>[0-9]+)")
TIMING_RE = re.compile(
    r"^(?P<label>Hough3D (?:vote generation (?:scalar reference|candidate)|production houghVoting)):\s+"
    r"(?P<us>[0-9.eE+-]+)\s+us\s+/\s+iter"
)


def repo_relative(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def parse_bench_log(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(path)
    data: dict[str, Any] = {"path": repo_relative(path), "timings_us": {}}
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := DATASET_RE.match(line):
            data["dataset"] = match.group("dataset")
        elif match := ITERATIONS_RE.match(line):
            data["iterations"] = int(match.group("iterations"))
        elif match := WARMUP_RE.match(line):
            data["warmup_iterations"] = int(match.group("warmup"))
        elif match := VOTE_COUNT_RE.match(line):
            data["vote_count"] = int(match.group("vote_count"))
        elif match := INTERP_RE.match(line):
            data["use_interpolation"] = match.group("value") == "1"
        elif match := DIST_WEIGHT_RE.match(line):
            data["use_distance_weight"] = match.group("value") == "1"
        elif match := CHECKSUM_RE.match(line):
            data["checksum"] = match.group("checksum")
        elif match := TIMING_RE.match(line):
            data["timings_us"][match.group("label")] = float(match.group("us"))
    missing = [
        key
        for key in ("dataset", "iterations", "warmup_iterations", "vote_count", "checksum")
        if key not in data
    ]
    if missing:
        raise ValueError(f"{path} missing required field(s): {', '.join(missing)}")
    return data


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
    if median >= 1.20 and below_one == 0:
        return "positive"
    if 1.05 <= median < 1.20 and below_one == 0:
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def asm_has_expected_instructions(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"path": repo_relative(path), "exists": False, "rvv_instr_count": 0}
    text = path.read_text(encoding="utf-8", errors="replace")
    instr_count = len(re.findall(r"\s+v[a-z0-9]+(?:\.[a-z0-9]+)*\s+", text))
    return {
        "path": repo_relative(path),
        "exists": True,
        "rvv_instr_count": instr_count,
        "has_symbol": "computeSceneVotesCandidate" in text,
        "has_vlse32": "vlse32.v" in text,
        "has_vfmul": "vfmul.vv" in text,
        "has_vfadd": "vfadd.vv" in text,
    }


def build_outputs(
    input_dir: Path,
    run_label: str,
    asm_path: Path,
    row_label: str,
    case_name: str,
    evidence_role: str,
    group: str,
    title: str,
    timer_boundary: str,
    gate: str,
    asm_boundary: str,
    notes: str,
) -> tuple[str, dict[str, Any]]:
    runs = sorted(path for path in input_dir.iterdir() if path.is_dir() and path.name.startswith("run"))
    if not runs:
        raise ValueError(f"no run directories under {input_dir}")

    parsed: list[dict[str, Any]] = []
    for run_dir in runs:
        std_log = parse_bench_log(run_dir / "run_bench_std.log")
        rvv_log = parse_bench_log(run_dir / "run_bench_rvv.log")
        if std_log["dataset"] != rvv_log["dataset"]:
            raise ValueError(f"{run_dir} dataset mismatch")
        if std_log["iterations"] != rvv_log["iterations"]:
            raise ValueError(f"{run_dir} iteration mismatch")
        if std_log["warmup_iterations"] != rvv_log["warmup_iterations"]:
            raise ValueError(f"{run_dir} warmup mismatch")
        if std_log["vote_count"] != rvv_log["vote_count"]:
            raise ValueError(f"{run_dir} vote count mismatch")
        if std_log["checksum"] != rvv_log["checksum"]:
            raise ValueError(f"{run_dir} checksum mismatch")
        parsed.append({"run_dir": run_dir, "std": std_log, "rvv": rvv_log})

    label = row_label
    speedups: list[float] = []
    std_values: list[float] = []
    rvv_values: list[float] = []
    for item in parsed:
        std_us = item["std"]["timings_us"][label]
        rvv_us = item["rvv"]["timings_us"][label]
        speedups.append(std_us / rvv_us)
        std_values.append(std_us)
        rvv_values.append(rvv_us)

    context = parsed[0]["std"]
    bucket = decision_bucket(speedups)
    asm = asm_has_expected_instructions(asm_path)
    summary = "\n".join(
        [
            f"# {title}",
            "",
            "## Context",
            "",
            f"- run_label: `{run_label}`",
            f"- dataset: `{context['dataset']}`",
            f"- vote_count: `{context['vote_count']}`",
            f"- iterations: `{context['iterations']}`",
            f"- warmup_iterations: `{context['warmup_iterations']}`",
            f"- evidence_role: `{evidence_role}`",
            "",
            "## Results",
            "",
            "| case | runs | median | min | max | p10 | p90 | B/A < 1 | decision_bucket | values |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |",
            "| `{case_name}` | {runs} | {median:.3f}x | {min_v:.3f}x | {max_v:.3f}x | {p10:.3f}x | {p90:.3f}x | {below}/{runs} | {bucket} | {values} |".format(
                case_name=case_name,
                runs=len(speedups),
                median=statistics.median(speedups),
                min_v=min(speedups),
                max_v=max(speedups),
                p10=percentile(speedups, 0.10),
                p90=percentile(speedups, 0.90),
                below=sum(1 for value in speedups if value < 1.0),
                bucket=bucket,
                values=", ".join(f"{value:.3f}x" for value in speedups),
            ),
            "",
            "## Evidence Doctor Input Notes",
            "",
            f"- asm_path: `{asm['path']}`",
            f"- asm_rvv_instr_count: `{asm['rvv_instr_count']}`",
            f"- checksum: `{context['checksum']}`",
            f"- notes: {notes}",
            "",
        ]
    )

    metadata = {
        "boundary": "test_helper" if evidence_role == "production_shaped_diagnostic" else "production_direct",
        "wrapper": "test-rvv/recognition/hough_3d/src/bench_hough_3d.cpp",
        "row_source": "correspondence_pair_synthetic_vote_generation",
        "solve": "not_applicable",
        "checksum_policy": "FNV-style checksum over generated scene vote coordinates",
        "timer_boundary": timer_boundary,
        "gate": gate,
        "mask": "not_applicable_no_invalid_lane_filter_in_phase000",
        "reduction": "scene vote xyz generation with scalar min/max finalization",
        "asm_boundary": asm_boundary,
        "point_type": "PointXYZ-style / float / AoS",
        "size": f"vote_count={context['vote_count']}",
    }
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": title,
            "evidence_role": evidence_role,
            "summary_path": repo_relative(input_dir / "summary.md"),
        "metadata": {
            "device": "Milkv-Jupiter",
            "iterations": context["iterations"],
            "warmup_iterations": context["warmup_iterations"],
            "run_count": len(parsed),
            "use_interpolation": context.get("use_interpolation", "not_recorded"),
            "use_distance_weight": context.get("use_distance_weight", "not_recorded"),
            "taskset": "not_recorded",
            "governor": "not_recorded",
            "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {"strict_ab": False},
        "comparisons": [
            {
                "name": case_name,
                "case_kind": evidence_role,
                "evidence_role": evidence_role,
                "group": group,
                "point_type": metadata["point_type"],
                "size": metadata["size"],
                "run_count": len(parsed),
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "ba_values": [round(value, 6) for value in speedups],
                "std_ms": statistics.median(std_values) / 1000.0,
                "rvv_ms": statistics.median(rvv_values) / 1000.0,
                "baseline": {**metadata, "checksum": context["checksum"]},
                "candidate": {
                    **metadata,
                    "checksum": context["checksum"],
                    "rvv_instr_count": asm["rvv_instr_count"],
                },
                "requires_board_context": True,
                "notes": notes,
            }
        ],
    }
    return summary, manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--summary", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--row-label", default="Hough3D vote generation candidate")
    parser.add_argument("--case-name", default="hough_3d_vote_generation_candidate")
    parser.add_argument("--evidence-role", default="production_shaped_diagnostic")
    parser.add_argument("--group", default="hough_3d_phase000_vote_generation")
    parser.add_argument("--title", default="Hough3D Phase 000 Repeated Board Summary")
    parser.add_argument(
        "--timer-boundary",
        default="Hough3D vote generation helper only; excludes HoughSpace accumulator scatter and findMaxima",
    )
    parser.add_argument(
        "--gate",
        default="synthetic PointXYZ-style float AoS vote inputs; RVV build must report RvvVoteGeneration",
    )
    parser.add_argument("--asm-boundary", default="pcl::test::hough_3d_rvv::computeSceneVotesCandidate")
    parser.add_argument(
        "--notes",
        default="Candidate row compares Std build scalar fallback with RVV build vote-generation candidate. It does not include Hough accumulator scatter.",
    )
    args = parser.parse_args()

    summary, manifest = build_outputs(
        args.input_dir,
        args.run_label,
        args.asm,
        args.row_label,
        args.case_name,
        args.evidence_role,
        args.group,
        args.title,
        args.timer_boundary,
        args.gate,
        args.asm_boundary,
        args.notes,
    )
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text(summary, encoding="utf-8")
    args.manifest.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
