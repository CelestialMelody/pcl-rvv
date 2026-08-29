#!/usr/bin/env python3
"""把 BRISK 2D repeated board 输出整理成 Evidence Doctor 可读的 manifest。"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_TIME_RE = re.compile(r"^(?P<name>brisk_[A-Za-z0-9_]+):\s*(?P<ms>[0-9.]+)\s+ms\s*/\s*iter$", re.MULTILINE)
CHECKSUM_RE = re.compile(
    r"^checksum_(?P<name>brisk_[A-Za-z0-9_]+)=(?P<checksum>[0-9]+)"
    r"\s+iterations=(?P<iterations>\d+)\s+warmup=(?P<warmup>\d+)$",
    re.MULTILINE,
)


CASE_LABELS: dict[str, dict[str, str]] = {
    "brisk_halfsample_640x480": {
        "group": "brisk_2d_downsample_helper",
        "case_kind": "production_direct",
        "point_type": "uint8_t image bytes",
        "row_source": "contiguous_organized_image_no_indices",
        "size": "640x480 source image, HALFSAMPLE derived layer",
        "timer_boundary": "construct one brisk::Layer from source Layer with HALFSAMPLE; includes output allocation and checksum outside timer only through wrapper return",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper for width/height vector chunks; non-RVV build uses portable scalar fallback",
        "reduction": "2x2 byte average with integer floor division by 4",
        "notes": "真实 production helper 证据；不覆盖 AGAST/OAST detector 和完整 BriskKeypoint2D::compute() 端到端耗时。",
    },
    "brisk_halfsample_641x481_tail": {
        "group": "brisk_2d_downsample_helper",
        "case_kind": "production_direct",
        "point_type": "uint8_t image bytes",
        "row_source": "contiguous_organized_image_no_indices",
        "size": "641x481 source image, HALFSAMPLE tail coverage",
        "timer_boundary": "construct one brisk::Layer from source Layer with HALFSAMPLE; odd source dimensions exercise tail-derived output width/height",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper for full destination width; non-RVV build uses portable scalar fallback",
        "reduction": "2x2 byte average with integer floor division by 4",
        "notes": "tail 规模证据；不能外推到非 contiguous 图像或 descriptor path。",
    },
    "brisk_twothirdsample_640x480": {
        "group": "brisk_2d_downsample_helper",
        "case_kind": "production_direct",
        "point_type": "uint8_t image bytes",
        "row_source": "contiguous_organized_image_no_indices",
        "size": "640x480 source image, TWOTHIRDSAMPLE derived layer",
        "timer_boundary": "construct one brisk::Layer from source Layer with TWOTHIRDSAMPLE; includes 3x3 source blocks mapped to 2x2 output blocks",
        "gate": "__RVV10__ and __riscv_vector build uses RVV helper for block groups; non-RVV build uses portable scalar fallback",
        "reduction": "3x3 to 2x2 weighted byte interpolation with integer floor division by 9",
        "notes": "真实 production helper 证据；当前 RISC-V / 非 SSSE3 语义按 portable scalar floor division 冻结。",
    },
    "brisk_construct_pyramid_640x480": {
        "group": "brisk_2d_construct_pyramid",
        "case_kind": "production_direct",
        "point_type": "uint8_t image bytes",
        "row_source": "contiguous_organized_image_no_indices",
        "size": "640x480 source image, ScaleSpace(octaves=4)::constructPyramid",
        "timer_boundary": "construct BRISK ScaleSpace pyramid from image; includes one TWOTHIRDSAMPLE layer and later HALFSAMPLE layers",
        "gate": "__RVV10__ and __riscv_vector build dispatches downsample helpers inside production ScaleSpace; non-RVV build uses portable scalar fallback",
        "reduction": "combined downsample helpers across pyramid construction",
        "notes": "production public-ish helper chain evidence; still does not include AGAST/OAST keypoint detection.",
    },
    "brisk_public_compute_320x240": {
        "group": "brisk_2d_public_compute",
        "case_kind": "production_public",
        "point_type": "PointXYZRGBA synthetic organized cloud",
        "row_source": "organized_point_cloud_converted_to_image_data_no_indices",
        "size": "320x240 PointXYZRGBA cloud, BriskKeypoint2D threshold=60 octaves=4",
        "timer_boundary": "run BriskKeypoint2D<PointXYZRGBA>::compute on a synthetic organized cloud; includes image_data conversion, ScaleSpace construction, AGAST/OAST detection, scale refinement, and output copy",
        "gate": "__RVV10__ and __riscv_vector build only accelerates downsample helpers inside ScaleSpace; public compute entry and detector stages remain otherwise unchanged",
        "reduction": "not a pure reduction; checksum samples public keypoint output after complete compute",
        "notes": "真实 public entry 证据；用于判断 downsample helper 收益能否穿透完整 keypoint pipeline，不证明 AGAST/OAST 已经加速。",
    },
}


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


def parse_bench_log(path: Path) -> dict[str, dict[str, Any]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    by_case: dict[str, dict[str, Any]] = {}
    for match in CASE_TIME_RE.finditer(text):
        by_case.setdefault(match.group("name"), {})["ms"] = float(match.group("ms"))
    for match in CHECKSUM_RE.finditer(text):
        entry = by_case.setdefault(match.group("name"), {})
        entry["checksum"] = match.group("checksum")
        entry["iterations"] = int(match.group("iterations"))
        entry["warmup_iterations"] = int(match.group("warmup"))
    return by_case


def binary_hash(path: Path) -> str | None:
    if not path.exists() or not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = (
        "vsetvli",
        "vsetivli",
        "vlse8",
        "vsse8",
        "vle8",
        "vse8",
        "vzext",
        "vadd",
        "vmul",
        "vdivu",
        "vncvt",
        "vnsrl",
    )
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def side_metadata(case_name: str, *, build: str, ms_values: list[float], checksums: list[str | None]) -> dict[str, Any]:
    meta = CASE_LABELS[case_name]
    side = {
        "boundary": "public BriskKeypoint2D::compute" if meta["case_kind"] == "production_public" else "production detail helper / ScaleSpace helper chain",
        "wrapper": "test-rvv/keypoints/brisk_2d/src/bench_brisk_2d.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": "sampledChecksum over derived image bytes, warm-up excluded from final timed average",
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": "no finite mask; uint8_t organized image bytes are always consumed",
        "reduction": meta["reduction"],
        "asm_boundary": "keypoints/src/brisk_2d.cpp halfsample/twothirdsample RVV helpers, possibly inlined into bench binary",
        "checksum": unique_or_mixed(checksums),
    }
    side[f"{build}_ms"] = statistics.mean(ms_values)
    return side


def collect_runs(repeat_root: Path) -> list[tuple[Path, dict[str, dict[str, Any]], dict[str, dict[str, Any]]]]:
    run_dirs = sorted(path for path in repeat_root.glob("run-*") if path.is_dir())
    if not run_dirs:
        run_dirs = [repeat_root]
    runs = []
    for run_dir in run_dirs:
        std_path = run_dir / "run_bench_std.log"
        rvv_path = run_dir / "run_bench_rvv.log"
        analyze_path = run_dir / "analyze_bench_compare.log"
        if not std_path.exists() or not rvv_path.exists():
            continue
        if not analyze_path.exists():
            raise SystemExit(f"missing analyze_bench_compare.log in {run_dir}")
        runs.append((run_dir, parse_bench_log(std_path), parse_bench_log(rvv_path)))
    if not runs:
        raise SystemExit(f"no run_bench_std.log/run_bench_rvv.log pairs found under {repeat_root}")
    return runs


def selected_case_names(summary_role: str) -> list[str]:
    if summary_role == "production_direct":
        return [
            name
            for name, meta in CASE_LABELS.items()
            if meta["case_kind"] != "production_public"
        ]
    if summary_role == "production_public":
        return [
            name
            for name, meta in CASE_LABELS.items()
            if meta["case_kind"] == "production_public"
        ]
    return list(CASE_LABELS)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeat-root", required=True, type=Path)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", type=Path)
    parser.add_argument("--summary-title", required=True)
    parser.add_argument("--summary-role", default="production_direct")
    args = parser.parse_args()

    runs = collect_runs(args.repeat_root)
    summary_output = args.summary_output or args.output.with_name("repeated-evidence-summary.md")
    asm_lines = count_rvv_asm_lines(args.asm)
    rvv_binary_hash = binary_hash(Path("build/riscv/bench_brisk_2d_rvv"))
    std_binary_hash = binary_hash(Path("build/riscv/bench_brisk_2d_std"))
    boundary_by_role = {
        "production_mixed_detail_and_public": "production detail helper / ScaleSpace helper chain / public BriskKeypoint2D::compute",
        "production_public": "public BriskKeypoint2D::compute",
        "production_direct": "production detail helper / ScaleSpace helper chain",
    }
    summary_boundary = boundary_by_role.get(args.summary_role, args.summary_role)

    comparisons: list[dict[str, Any]] = []
    summary_lines = [
        f"# {args.summary_title}",
        "",
        f"- repeat_root: `{args.repeat_root}`",
        f"- evidence_role: `{args.summary_role}`",
        f"- A/B boundary: `{summary_boundary}`",
        f"- collected_runs: `{len(runs)}`",
        f"- asm_rvv_line_count: `{asm_lines}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    all_speedups: list[float] = []
    all_iterations: list[int | None] = []
    all_warmups: list[int | None] = []
    for case_name in selected_case_names(args.summary_role):
        std_values: list[float] = []
        rvv_values: list[float] = []
        speedups: list[float] = []
        std_checksums: list[str | None] = []
        rvv_checksums: list[str | None] = []
        source_logs: list[str] = []
        for run_dir, std_cases, rvv_cases in runs:
            if case_name not in std_cases or case_name not in rvv_cases:
                raise SystemExit(f"case {case_name} missing in {run_dir}")
            std_entry = std_cases[case_name]
            rvv_entry = rvv_cases[case_name]
            if "ms" not in std_entry or "ms" not in rvv_entry:
                raise SystemExit(f"case {case_name} missing timing in {run_dir}")
            std_ms = float(std_entry["ms"])
            rvv_ms = float(rvv_entry["ms"])
            std_values.append(std_ms)
            rvv_values.append(rvv_ms)
            speedups.append(std_ms / rvv_ms if rvv_ms else 0.0)
            std_checksums.append(std_entry.get("checksum"))
            rvv_checksums.append(rvv_entry.get("checksum"))
            all_iterations.extend([std_entry.get("iterations"), rvv_entry.get("iterations")])
            all_warmups.extend([std_entry.get("warmup_iterations"), rvv_entry.get("warmup_iterations")])
            source_logs.extend(
                [
                    str(run_dir / "run_bench_std.log"),
                    str(run_dir / "run_bench_rvv.log"),
                    str(run_dir / "analyze_bench_compare.log"),
                ]
            )

        meta = CASE_LABELS[case_name]
        below_one = sum(1 for value in speedups if value < 1.0)
        all_speedups.extend(speedups)
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
                "case_kind": meta["case_kind"],
                "evidence_role": "production_public"
                if meta["case_kind"] == "production_public"
                else args.summary_role,
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": len(speedups),
                "iterations": unique_or_mixed(all_iterations),
                "warmup_iterations": unique_or_mixed(all_warmups),
                "std_ms": statistics.mean(std_values),
                "rvv_ms": statistics.mean(rvv_values),
                "ba_values": speedups,
                "checksum_equal": unique_or_mixed(std_checksums) == unique_or_mixed(rvv_checksums),
                "rvv_instr_count": asm_lines,
                "source_logs": source_logs,
                "notes": meta["notes"],
                "baseline": side_metadata(case_name, build="std", ms_values=std_values, checksums=std_checksums),
                "candidate": side_metadata(case_name, build="rvv", ms_values=rvv_values, checksums=rvv_checksums),
            }
        )

    summary_output.parent.mkdir(parents=True, exist_ok=True)
    summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.summary_title,
            "evidence_role": args.summary_role,
            "summary_path": str(summary_output),
            "repeat_root": str(args.repeat_root),
            "decision_bucket": decision_bucket(all_speedups),
            "metadata": {
                "device": "Milkv-Jupiter",
                "vlen": "see SoC / ELF (board)",
                "run_count": len(runs),
                "iterations": unique_or_mixed(all_iterations),
                "warmup_iterations": unique_or_mixed(all_warmups),
                "taskset": "not recorded by current board harness",
                "governor": "not recorded by current board harness",
                "freq": "not recorded by current board harness",
                "temperature": "not recorded by current board harness",
                "binary_hash": unique_or_mixed([std_binary_hash, rvv_binary_hash]),
            },
        },
        "checks": {"strict_ab": False},
        "comparisons": comparisons,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
