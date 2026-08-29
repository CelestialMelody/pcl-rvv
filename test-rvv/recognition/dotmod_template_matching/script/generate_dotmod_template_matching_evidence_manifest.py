#!/usr/bin/env python3
"""Generate DOTMOD template matching summary and Evidence Doctor manifest.

本脚本解析当前 topic 的 repeated board（重复板卡测试）日志，把每个
run-* 目录里的 Std/RVV benchmark 输出转成 `summary.md` 和
`evidence_manifest.json`，供全局 Evidence Doctor（证据体检）脚本检查。
Phase 000/010 输出是 production-shaped diagnostic（生产形态诊断）证据；
Phase 020 输出是 production-public（真实公开入口）证据，用来判断当前
production patch（生产补丁）是否值得采纳。
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
TIMING_RE = re.compile(
    r"^(?P<label>DOTMOD (?:submap baseline window score|direct window score|full detectTemplates-shaped total|production detectTemplates total)):\s+"
    r"(?P<us>[0-9.eE+-]+)\s+us\s+/\s+iter"
)
TEMPLATES_RE = re.compile(r"^Templates:\s+(?P<templates>\d+)")
MODALITIES_RE = re.compile(r"^Modalities:\s+(?P<modalities>\d+)")
THRESHOLD_RE = re.compile(r"^Threshold:\s+(?P<threshold>[0-9.]+)")
CHECKSUM_RE = re.compile(r"^checksum:\s+(?P<checksum>[0-9]+)")

CASE_LABELS = {
    "DOTMOD submap baseline window score": {
        "case": "dotmod_submap_baseline_window_score",
        "group": "dotmod-phase000-control",
        "timer_boundary": "scoreWindowViaSubMapStd over every synthetic sliding-window position",
        "reduction": "scalar byte bitwise-and count after per-window submap copy",
        "notes": "Control row: both Std/RVV builds execute the same submap baseline helper, so it only checks build/log drift and is not the optimized candidate.",
    },
    "DOTMOD direct window score": {
        "case": "dotmod_direct_window_score",
        "group": "dotmod-phase000-direct-window-candidate",
        "timer_boundary": "scoreWindowDirectStd in Std build versus scoreWindowDirectRVV in RVV build over every synthetic sliding-window position",
        "reduction": "row-wise RVV byte load, bitwise-and, nonzero mask and vcpop count",
        "notes": "Candidate row: this is the Phase 000 decision item. It avoids per-window submap allocation/copy and uses RVV for byte hit counting.",
    },
    "DOTMOD full detectTemplates-shaped total": {
        "case": "dotmod_full_detecttemplates_shaped_total",
        "group": "dotmod-phase010-full-detecttemplates-shaped-candidate",
        "timer_boundary": "detectTemplatesViaSubMapStd full-shaped loop in Std build versus detectTemplatesDirectRVV full-shaped loop in RVV build",
        "reduction": "full row/col/template/modality loop with per-window responses vector; RVV only replaces byte hit counting inside each window",
        "boundary": "test_helper",
        "wrapper": "test-rvv/recognition/dotmod_template_matching/src/bench_dotmod_template_matching.cpp",
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "notes": "Candidate row: this is the Phase 010 decision item. It preserves full detectTemplates-shaped ordering, threshold and response accumulation while replacing submap score counting.",
    },
    "DOTMOD production detectTemplates total": {
        "case": "dotmod_production_detecttemplates_total",
        "group": "dotmod-phase020-production-public-candidate",
        "timer_boundary": "real pcl::DOTMOD::detectTemplates public entry compiled from current production source; Std build disables __RVV10__, RVV build enables the production RVV dispatch",
        "reduction": "production row/col/template/modality loop; RVV replaces byte hit counting for each original map window when __RVV10__ is enabled",
        "boundary": "public_overload",
        "wrapper": "test-rvv/recognition/dotmod_template_matching/src/bench_dotmod_template_matching_production_direct.cpp",
        "case_kind": "production_direct",
        "evidence_role": "production-public",
        "notes": "Candidate row: this is the Phase 020 production-public decision item. It calls real createAndAddTemplate() and detectTemplates() compiled from current production sources.",
    },
}


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
        elif match := TEMPLATES_RE.match(line):
            data["templates"] = int(match.group("templates"))
        elif match := MODALITIES_RE.match(line):
            data["modalities"] = int(match.group("modalities"))
        elif match := THRESHOLD_RE.match(line):
            data["threshold"] = float(match.group("threshold"))
        elif match := TIMING_RE.match(line):
            data["timings_us"][match.group("label")] = float(match.group("us"))
        elif match := CHECKSUM_RE.match(line):
            data["checksum"] = match.group("checksum")
    missing = [
        key
        for key in ("dataset", "iterations", "warmup_iterations", "checksum")
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


def common_metadata(case_info: dict[str, str], dataset: str) -> dict[str, str]:
    return {
        "boundary": case_info.get("boundary", "test_helper"),
        "wrapper": case_info.get(
            "wrapper",
            "test-rvv/recognition/dotmod_template_matching/src/bench_dotmod_template_matching.cpp",
        ),
        "row_source": "organized_quantized_byte_map_sliding_window",
        "solve": "not_applicable",
        "checksum_policy": "FNV-style checksum over DOTMOD detections or combined diagnostic local sums; Std/RVV mismatch is fatal for the selected manifest case",
        "timer_boundary": case_info["timer_boundary"],
        "gate": "synthetic row-major uint8_t image/template, image dimensions larger than window, tail lanes included",
        "mask": "byte bitwise-and nonzero hit mask",
        "reduction": case_info["reduction"],
        "asm_boundary": "pcl::test::dotmod_rvv::scoreWindowDirectRVV / inlined helper in bench_dotmod_template_matching_rvv",
        "point_type": "not_applicable_quantized_byte_map",
        "size": dataset,
    }


def build_outputs(
    input_dir: Path,
    run_label: str,
    manifest_case: str,
    evidence_role: str | None,
) -> tuple[str, dict[str, Any]]:
    runs = sorted(path for path in input_dir.glob("run-*") if path.is_dir())
    if not runs:
        raise ValueError(f"no run-* directories under {input_dir}")

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
        parsed.append({"run_dir": run_dir, "std": std_log, "rvv": rvv_log})

    context = parsed[0]["std"]
    available_labels = [
        label
        for label, info in CASE_LABELS.items()
        if all(label in item["std"]["timings_us"] and label in item["rvv"]["timings_us"] for item in parsed)
    ]
    if not any(info["case"] == manifest_case for label, info in CASE_LABELS.items() if label in available_labels):
        raise ValueError(f"manifest case {manifest_case!r} is not present in every run")

    summary_rows: list[str] = []
    comparisons: list[dict[str, Any]] = []
    for label in available_labels:
        info = CASE_LABELS[label]
        speedups: list[float] = []
        std_values: list[float] = []
        rvv_values: list[float] = []
        checksum_pairs: list[tuple[str, str]] = []
        for item in parsed:
            std_us = item["std"]["timings_us"][label]
            rvv_us = item["rvv"]["timings_us"][label]
            speedups.append(std_us / rvv_us)
            std_values.append(std_us)
            rvv_values.append(rvv_us)
            checksum_pairs.append((item["std"]["checksum"], item["rvv"]["checksum"]))

        if any(lhs != rhs for lhs, rhs in checksum_pairs):
            raise ValueError(f"{label} has checksum mismatch between Std and RVV logs")
        metadata = common_metadata(info, context["dataset"])
        # 控制项或非本阶段候选只帮助 reviewer 发现构建 / 日志漂移，不参与
        # 本次 Evidence Doctor 候选采纳决策。
        if info["case"] == manifest_case:
            selected_evidence_role = evidence_role or info.get("evidence_role", "production_shaped_diagnostic")
            comparison = {
                "name": info["case"],
                "case_kind": info.get("case_kind", "production_shaped_diagnostic"),
                "evidence_role": selected_evidence_role,
                "group": info["group"],
                "point_type": metadata["point_type"],
                "size": metadata["size"],
                "run_count": len(parsed),
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "ba_values": [round(value, 6) for value in speedups],
                "std_ms": statistics.median(std_values) / 1000.0,
                "rvv_ms": statistics.median(rvv_values) / 1000.0,
                "baseline": {**metadata, "checksum": checksum_pairs[0][0]},
                "candidate": {**metadata, "checksum": checksum_pairs[0][1]},
                "requires_board_context": True,
                "notes": info["notes"],
            }
            comparisons.append(comparison)

        summary_rows.append(
            "| `{case}` | {runs} | {median:.3f}x | {min_v:.3f}x | {max_v:.3f}x | "
            "{p10:.3f}x | {p90:.3f}x | {below}/{runs} | {bucket} | {values} |".format(
                case=info["case"],
                runs=len(speedups),
                median=statistics.median(speedups),
                min_v=min(speedups),
                max_v=max(speedups),
                p10=percentile(speedups, 0.10),
                p90=percentile(speedups, 0.90),
                below=sum(1 for value in speedups if value < 1.0),
                bucket=decision_bucket(speedups),
                values=", ".join(f"{value:.3f}x" for value in speedups),
            )
        )

    summary = "\n".join(
        [
            "# DOTMOD template matching repeated board summary",
            "",
            f"- run_label: `{run_label}`",
            f"- expected_runs: `{len(parsed)}`",
            f"- collected_runs: `{len(parsed)}`",
            f"- evidence_role: `{evidence_role or 'from selected case metadata'}`",
            f"- A/B boundary: `{CASE_LABELS[next(label for label, info in CASE_LABELS.items() if info['case'] == manifest_case)].get('boundary', 'test_helper')}`",
            f"- dataset: `{context['dataset']}`",
            f"- templates: `{context.get('templates', 'not_recorded')}`",
            f"- modalities: `{context.get('modalities', 'not_recorded')}`",
            f"- threshold: `{context.get('threshold', 'not_recorded')}`",
            f"- iterations: `{context['iterations']}`",
            f"- warmup_iterations: `{context['warmup_iterations']}`",
            "",
            "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | decision bucket | values |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |",
            *summary_rows,
            "",
            "## Evidence boundary",
            "",
            "manifest 只把当前阶段的候选行交给 Evidence Doctor；其它行作为控制项"
            "或辅助解释，用于观察两侧构建和日志是否漂移。Phase 000/010 的"
            " production-shaped diagnostic（生产形态诊断）证据不能替代 Phase 020 的"
            " production direct（真实生产路径）证据；Phase 020 的 production-public"
            "（真实公开入口）证据只覆盖本 summary 记录的 `DOTMOD::detectTemplates()` 输入边界。",
            "",
        ]
    )

    summary_evidence_role = evidence_role or (
        comparisons[0]["evidence_role"] if comparisons else "production_shaped_diagnostic"
    )
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "DOTMOD template matching repeated board summary",
            "evidence_role": summary_evidence_role,
            "summary_path": repo_relative(input_dir / "summary.md"),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": context["iterations"],
                "warmup_iterations": context["warmup_iterations"],
                "run_count": len(parsed),
                "vlen": "see SoC / ELF (board)",
                "templates": context.get("templates", "not_recorded"),
                "modalities": context.get("modalities", "not_recorded"),
                "threshold": context.get("threshold", "not_recorded"),
            },
        },
        "checks": {"strict_ab": False},
        "thresholds": {"long_tail_ratio": 1.25},
        "comparisons": comparisons,
    }
    return summary, manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--run-label", default="dotmod_phase000_submap_direct_window_repeated")
    parser.add_argument("--manifest-case", default="dotmod_direct_window_score")
    parser.add_argument("--evidence-role", default=None)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()

    summary, manifest = build_outputs(
        args.input_dir, args.run_label, args.manifest_case, args.evidence_role
    )
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text(summary, encoding="utf-8")
    args.manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
