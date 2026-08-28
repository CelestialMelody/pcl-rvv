#!/usr/bin/env python3
"""Generate CGM repeated board summary and Evidence Doctor manifest."""

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
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+).*?checksum:\s*(?P<checksum>[0-9]+)", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations\s*:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup(?: Iterations)?\s*:\s*(?P<warmup>\d+)", re.MULTILINE)
DATASET_RE = re.compile(r"^\s*Dataset\s*:\s*(?P<dataset>.+?)\s*$", re.MULTILINE)
DEVICE_RE = re.compile(r"^\s*Device\s*:\s*(?P<device>.+?)\s*$", re.MULTILINE)
VLEN_RE = re.compile(r"^\s*VLEN\s*:\s*(?P<vlen>.+?)\s*$", re.MULTILINE)


def parse_context(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    if match := DATASET_RE.search(text):
        context["dataset"] = match.group("dataset").strip()
    if match := DEVICE_RE.search(text):
        context["device"] = match.group("device").strip()
    if match := VLEN_RE.search(text):
        context["vlen"] = match.group("vlen").strip()
    return context


def parse_summary(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context = parse_context(path)
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
    if 1.05 <= median < 1.20 and below_one == 0:
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def metadata_for_case(name: str, run_label: str = "") -> dict[str, str]:
    common = {
        "group": "cgm-sobel-quantize-production-shaped-diagnostic",
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "point_type": "pcl::RGB",
        "row_source": "organized_rgb_image_inner_pixels",
        "boundary": "test_helper",
        "wrapper": "test-rvv/recognition/color_gradient_modality/src/bench_cgm.cpp",
        "timer_boundary": "computeSobelQuantizedCandidate, Sobel max-channel selection plus magnitude/angle quantize for inner pixels",
        "gate": "organized pcl::RGB image, width>=3, height>=3, borders are zero-filled",
        "mask": "magnitude_threshold predicate; border pixels excluded from timed inner-pixel rows",
        "reduction": "no cross-lane reduction; scalar Sobel staging and RVV per-lane sqrt/atan2/angle quantize",
        "checksum_policy": "FNV-style checksum over quantized bin and magnitude scaled by 1000",
        "asm_boundary": "computeSobelQuantizedCandidate / inlined RVV math in bench_cgm_rvv",
        "notes": "Phase 000 production-shaped diagnostic. It does not call ColorGradientModality::processInputData and does not include Gaussian smoothing, dominant filter, spread or feature extraction.",
    }
    sizes = {
        "sobel_quantize_320x240": "320x240 organized RGB image",
        "sobel_quantize_641x481_tail": "641x481 organized RGB image with RVV tail lanes",
        "filter_dominant_320x240": "320x240 quantized map",
        "filter_dominant_641x481_tail": "641x481 quantized map with RVV tail lanes",
        "full_chain_320x240": "320x240 organized RGB image through Sobel+quantize+filter",
        "full_chain_641x481_tail": "641x481 organized RGB image through Sobel+quantize+filter with RVV tail lanes",
        "full_chain_stencil_320x240": "320x240 organized RGB image through RGB Sobel-stencil RVV, quantize and filter",
        "full_chain_stencil_641x481_tail": "641x481 organized RGB image through RGB Sobel-stencil RVV, quantize and filter with RVV tail lanes",
        "production_process_320x240": "320x240 organized PointXYZRGB cloud through ColorGradientModality::processInputData",
        "production_process_641x481_tail": "641x481 organized PointXYZRGB cloud through ColorGradientModality::processInputData with RVV tail lanes",
    }
    if name not in sizes:
        raise ValueError(f"unknown CGM case label: {name}")
    if name.startswith("filter_dominant"):
        common = {
            **common,
            "group": "cgm-dominant-filter-production-shaped-diagnostic",
            "row_source": "organized_quantized_gradient_map_inner_pixels",
            "timer_boundary": "filterQuantizedGradientsCandidate, 3x3 dominant-bin count plus >=5 threshold for inner pixels",
            "gate": "organized uint8_t quantized map values 0..8, width>=3, height>=3, borders are zero-filled",
            "mask": "3x3 bin equality masks for directions 1..8; zero bin participates only as non-direction background",
            "reduction": "lane-wise 9-neighbor equality count per direction; strict greater-than preserves production tie-break",
            "checksum_policy": "FNV-style checksum over filtered one-hot uint8_t map",
            "asm_boundary": "filterQuantizedGradientsCandidate / inlined RVV byte compare and store in bench_cgm_rvv",
            "notes": "Phase 010 production-shaped diagnostic. It does not call ColorGradientModality::processInputData and does not include Sobel, quantize, spread or feature extraction.",
        }
    if name.startswith("full_chain_stencil"):
        common = {
            **common,
            "group": "cgm-full-rgb-stencil-quantize-filter-production-shaped-diagnostic",
            "row_source": "organized_rgb_image_inner_pixels_to_quantized_filter_map",
            "timer_boundary": "computeSobelQuantizedStencilCandidate, RGB byte stride-load Sobel stencil, max-channel selection, magnitude/angle quantize and 3x3 dominant-bin filter",
            "gate": "organized pcl::RGB image, B/G/R/A 4-byte point stride, width>=3, height>=3, borders are zero-filled at each subchain boundary",
            "mask": "magnitude_threshold predicate followed by 3x3 bin equality masks for directions 1..8",
            "reduction": "RVV stride byte loads, widened integer Sobel arithmetic, lane-wise max-channel selection, per-lane sqrt/atan2/angle quantize, then lane-wise dominant filter equality count",
            "checksum_policy": "FNV-style checksum over filtered one-hot uint8_t map",
            "asm_boundary": "computeSobelQuantizedStencilCandidate / inlined RVV stride-load Sobel math and byte filter in bench_cgm_rvv",
            "notes": "Phase 040 production-shaped diagnostic. It does not call ColorGradientModality::processInputData and is a candidate-family A/B against the current full-chain diagnostic, not an adopted production path.",
        }
    elif name.startswith("full_chain"):
        common = {
            **common,
            "group": "cgm-full-sobel-quantize-filter-production-shaped-diagnostic",
            "row_source": "organized_rgb_image_inner_pixels_to_quantized_filter_map",
            "timer_boundary": "computeSobelQuantizedFilteredCandidate, Sobel max-channel selection, magnitude/angle quantize, quantized-map extraction and 3x3 dominant-bin filter",
            "gate": "organized pcl::RGB image, width>=3, height>=3, borders are zero-filled at each subchain boundary",
            "mask": "magnitude_threshold predicate followed by 3x3 bin equality masks for directions 1..8",
            "reduction": "scalar Sobel staging, RVV per-lane sqrt/atan2/angle quantize, then lane-wise dominant filter equality count",
            "checksum_policy": "FNV-style checksum over filtered one-hot uint8_t map",
            "asm_boundary": "computeSobelQuantizedFilteredCandidate / inlined RVV math and byte filter in bench_cgm_rvv",
            "notes": "Phase 020 production-shaped diagnostic. It does not call ColorGradientModality::processInputData and does not include Gaussian smoothing, spread or feature extraction.",
        }
    if name.startswith("production_process"):
        phase = "050" if "phase050" in run_label else "030"
        reduction = (
            "scalar Gaussian and spread around an RVV post-Gaussian RGB byte stride-load Sobel stencil, "
            "max-channel selection, per-lane sqrt/atan2/angle quantize plus lane-wise dominant filter"
            if phase == "050"
            else "scalar Gaussian and spread around an RVV post-Gaussian per-lane sqrt/atan2/angle quantize plus lane-wise dominant filter"
        )
        common = {
            **common,
            "group": "cgm-production-public-color-gradient-process",
            "case_kind": "production_direct",
            "evidence_role": "production_direct",
            "point_type": "pcl::PointXYZRGB",
            "row_source": "organized_pointxyzrgb_public_entry",
            "boundary": "public_overload",
            "wrapper": "ColorGradientModality<PointXYZRGB>::processInputData via src/bench_cgm.cpp",
            "timer_boundary": "processInputData includes RGB copy, Gaussian convolution, post-Gaussian Std/RVV color-gradient chain and spread; excludes feature extraction",
            "gate": "organized PointXYZRGB cloud, width>=3, height>=3, __RVV10__ candidate build dispatches after Gaussian output to pcl::RGB",
            "mask": "production gradient_magnitude_threshold predicate followed by production 3x3 dominant-bin filter and spread mask behavior",
            "reduction": reduction,
            "checksum_policy": "FNV-style checksum over production quantized map and spreaded quantized map; GradientXY floating angles are covered by tolerant correctness tests",
            "asm_boundary": "ColorGradientModality<PointXYZRGB>::computeColorGradientPipelineRVV / inlined production helper in bench_cgm_rvv",
            "notes": f"Phase {phase} production direct evidence. It calls the public ColorGradientModality<PointXYZRGB>::processInputData entry; feature extraction remains outside this timer boundary.",
        }
    return {**common, "size": sizes[name]}


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle", "vlse", "vse", "vsetvli", "vfsqrt", "vfcvt", "vf", "vm")
    return sum(
        1
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines()
        if any(needle in line for needle in needles)
    )


def evidence_boundary_text(case_names: list[str], run_label: str) -> str:
    if case_names and all(name.startswith("production_process") for name in case_names):
        phase = "050" if "phase050" in run_label else "030"
        final_action = (
            "本轮用户已授权接入后板卡有收益即可采纳，因此该 summary 可作为 Phase 050 adopted production behavior 的生产性能依据。"
            if phase == "050"
            else "PI5 仍需用户确认是否采纳或回滚 production patch。"
        )
        return (
            f"本 summary 是 Phase {phase} production direct（真实生产路径）证据。Std/RVV 两侧"
            "都通过 `ColorGradientModality<PointXYZRGB>::processInputData()` 公开入口计时；"
            "计时边界包含 RGB copy、Gaussian convolution、Gaussian 后 color-gradient Std/RVV "
            "链路和 spread，不包含 feature extraction。该证据只证明当前 public RVV path "
            f"是否快于当前 public scalar path；{final_action}"
        )
    if case_names and all(name.startswith("filter_dominant") for name in case_names):
        return (
            "本 summary 是 Phase 010 未接 production 的 production-shaped diagnostic 证据。"
            "Std/RVV 两侧共享同一个 bench wrapper，计时边界只覆盖 manifest 中的 "
            "dominant filter helper，不能替代 `ColorGradientModality::processInputData()` "
            "的 production evidence。"
        )
    if case_names and all(name.startswith("sobel_quantize") for name in case_names):
        return (
            "本 summary 是 Phase 000 未接 production 的 production-shaped diagnostic 证据。"
            "Std/RVV 两侧共享同一个 bench wrapper，计时边界只覆盖 manifest 中的 "
            "Sobel+quantize helper，不能替代 `ColorGradientModality::processInputData()` "
            "的 production evidence。"
        )
    if case_names and any(name.startswith("full_chain_stencil") for name in case_names):
        return (
            "本 summary 是 Phase 040 未接 production 的 production-shaped diagnostic 证据。"
            "Std/RVV 两侧共享同一个 bench wrapper，计时边界覆盖当前 full-chain helper 和 "
            "RGB Sobel stencil RVV helper。它用于判断新 candidate family 是否值得进入后续 "
            "production integration，不能替代已采纳 production direct evidence。"
        )
    if case_names and all(name.startswith("full_chain") for name in case_names):
        return (
            "本 summary 是 Phase 020 未接 production 的 production-shaped diagnostic 证据。"
            "Std/RVV 两侧共享同一个 bench wrapper，计时边界覆盖 manifest 中的 "
            "Sobel+quantize+dominant filter 串接 helper，不能替代 "
            "`ColorGradientModality::processInputData()` 的 production evidence。"
        )
    return (
        "本 summary 是未接 production 的 production-shaped diagnostic 证据。Std/RVV "
        "两侧共享同一个 bench wrapper，计时边界只覆盖 manifest 中列出的 topic-local "
        "helper，不能替代 `ColorGradientModality::processInputData()` 的 production evidence。"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--title", default="CGM Sobel+quantize repeated board summary")
    parser.add_argument("--run-label", default="cgm_phase000_sobel_quantize_repeated")
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    args = parser.parse_args()

    summary_paths = sorted(args.summary_dir.glob("run_*/analyze_bench_compare.log"))
    if not summary_paths:
        raise SystemExit(f"no run_*/analyze_bench_compare.log files found under {args.summary_dir}")

    contexts: list[dict[str, Any]] = []
    grouped: dict[str, dict[str, list[Any]]] = {}
    for path in summary_paths:
        context, comparisons = parse_summary(path)
        contexts.append(context)
        for item in comparisons:
            name = str(item["name"])
            if args.case_name and name != args.case_name:
                continue
            metadata_for_case(name, args.run_label)
            bucket = grouped.setdefault(name, {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(float(item["std_ms"]))
            bucket["rvv"].append(float(item["rvv_ms"]))
            bucket["speedup"].append(float(item["speedup"]))
            bucket["paths"].append(str(item["summary_path"]))

    if not grouped:
        raise SystemExit("no benchmark comparisons parsed")

    case_roles = sorted({metadata_for_case(name, args.run_label)["evidence_role"] for name in grouped})
    summary_role = case_roles[0] if len(case_roles) == 1 else "mixed_boundary_cross_check"
    case_boundaries = sorted({metadata_for_case(name, args.run_label)["boundary"] for name in grouped})
    summary_boundary = case_boundaries[0] if len(case_boundaries) == 1 else "mixed"
    asm_lines = count_rvv_asm_lines(args.asm)
    args.summary_output.parent.mkdir(parents=True, exist_ok=True)
    summary_lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(summary_paths)}`",
        f"- evidence_role: `{summary_role}`",
        f"- A/B boundary: `{summary_boundary}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    comparisons_manifest: list[dict[str, Any]] = []
    for name, values in sorted(grouped.items()):
        speedups = [float(value) for value in values["speedup"]]
        std_values = [float(value) for value in values["std"]]
        rvv_values = [float(value) for value in values["rvv"]]
        paths = [str(value) for value in values["paths"]]
        meta = metadata_for_case(name, args.run_label)
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
        }
        std_checksums = [parse_checksum(Path(path).with_name("run_bench_std.log"), name) for path in paths]
        rvv_checksums = [parse_checksum(Path(path).with_name("run_bench_rvv.log"), name) for path in paths]
        std_ms = statistics.mean(std_values)
        rvv_ms = statistics.mean(rvv_values)
        comparisons_manifest.append(
            {
                "name": name,
                "group": meta["group"],
                "evidence_role": meta["evidence_role"],
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": meta["size"],
                "run_count": len(speedups),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "build_comparison": True,
                "std_ms": std_ms,
                "rvv_ms": rvv_ms,
                "ba_values": speedups,
                "std_ms_values": std_values,
                "rvv_ms_values": rvv_values,
                "run_paths": paths,
                "baseline": {
                    "label": "Std build",
                    "checksum": unique_or_mixed(std_checksums),
                    "std_ms": std_ms,
                    "rvv_ms": std_ms,
                    **common,
                },
                "candidate": {
                    "label": "RVV build",
                    "checksum": unique_or_mixed(rvv_checksums),
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    **common,
                },
                "notes": meta["notes"],
            }
        )

    decision_values = list(comparisons_manifest[0]["ba_values"])
    summary_lines += [
        "",
        "## Evidence boundary",
        "",
        evidence_boundary_text(sorted(grouped), args.run_label),
    ]
    args.summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    first_context = contexts[0] if contexts else {}
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": summary_role,
            "summary_path": str(args.summary_output),
            "summary_paths": [str(path) for path in summary_paths],
            "label": args.run_label,
            "analysis_script": "test-rvv/recognition/color_gradient_modality/script/generate_cgm_evidence_manifest.py",
            "metadata": {
                "device": first_context.get("device", "board via Makefile config"),
                "vlen": first_context.get("vlen", "see SoC / ELF (board)"),
                "dataset": first_context.get("dataset", "synthetic organized RGB image"),
                "iterations": unique_or_mixed([ctx.get("iterations") for ctx in contexts]),
                "warmup_iterations": unique_or_mixed([ctx.get("warmup_iterations") for ctx in contexts]),
                "run_count": len(summary_paths),
                "expected_runs": args.expected_runs,
                "decision_bucket": decision_bucket(decision_values),
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
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.summary_output}")
    print(f"wrote manifest: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
