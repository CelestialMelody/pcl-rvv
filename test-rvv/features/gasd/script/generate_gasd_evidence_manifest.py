#!/usr/bin/env python3
"""Generate GASD repeated board summary and Evidence Doctor manifest."""

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
    if name == "public_gasd_shape_compute":
        return {
            "group": "gasd-public-shape-compute-profile",
            "evidence_role": "production-public profile",
            "case_kind": "public-compute-profile",
            "point_type": "PointXYZ -> GASDSignature512",
            "timer_boundary": "public GASDEstimation::computeFeature via estimator.compute",
            "row_source": "synthetic PointXYZ input cloud",
            "gate": "default public shape estimator configuration; production source is unchanged",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "production scalar descriptor accumulation; no hand-written RVV production patch",
            "checksum_policy": "descriptor size checksum only; benchmark checks run-to-run consistency, not descriptor value parity",
            "asm_boundary": "not_applicable for Phase 080; public build profile is not attributed to a hand-written RVV candidate",
            "ab_boundary": "public compute benchmark",
            "notes": "Public shape compute profile. Std/RVV builds use the same production GASD source; the comparison is a build-level profile signal, not production patch evidence.",
        }
    if name == "public_gasd_color_compute":
        return {
            "group": "gasd-public-color-compute-profile",
            "evidence_role": "production-public profile",
            "case_kind": "public-compute-profile",
            "point_type": "PointXYZRGBA -> GASDSignature984",
            "timer_boundary": "public GASDColorEstimation::computeFeature via estimator.compute",
            "row_source": "synthetic PointXYZRGBA input cloud",
            "gate": "default public color estimator configuration with INTERP_NONE color interpolation; production source is unchanged",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "production scalar descriptor accumulation; no hand-written RVV production patch",
            "checksum_policy": "descriptor size checksum only; benchmark checks run-to-run consistency, not descriptor value parity",
            "asm_boundary": "not_applicable for Phase 080; public build profile is not attributed to a hand-written RVV candidate",
            "ab_boundary": "public compute benchmark",
            "notes": "Public color compute profile. Std/RVV builds use the same production GASD source; the comparison is a build-level profile signal, not production patch evidence.",
        }
    if name == "candidate_shape_combined_rvv":
        return {
            "group": "gasd-shape-combined-production-shaped-diagnostic",
            "evidence_role": "production-shaped diagnostic",
            "case_kind": "production-shaped-diagnostic",
            "point_type": "PointXYZ -> Eigen shape histogram grid -> descriptor buffer",
            "timer_boundary": "shape projection plus trilinear Eigen histogram accumulation plus shape descriptor copy",
            "row_source": "synthetic transformed-like shape samples",
            "gate": "INTERP_TRILINEAR shape combined helper; alignment transform, indices and public compute dispatch excluded",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "scalar += accumulation into Eigen histogram bins; RVV is used for projection, trilinear staging and descriptor copy",
            "checksum_policy": "tolerance-aligned quantized shape descriptor checksum with 1e3 scale",
            "asm_boundary": "computeShapeDescriptorTrilinearRVVStaged / projectShapeSamplesRVVToBuffers / computeTrilinearInterpolationRVVToBuffers / copyShapeHistogramsRVVToBuffer in bench_gasd_rvv",
            "notes": "Production-shaped shape combined diagnostic. It combines projection staging, trilinear Eigen histogram accumulation and descriptor copy in a test helper, but does not prove public compute dispatch.",
        }
    if name == "candidate_trilinear_eigen_histogram_write_rvv":
        return {
            "group": "gasd-trilinear-eigen-histogram-write-probe",
            "case_kind": "component-ablation",
            "point_type": "PointXYZ projection staging -> trilinear index/weight buffers -> Eigen histogram grid",
            "timer_boundary": "trilinear interpolation staging plus scalar Eigen-backed histogram accumulation",
            "row_source": "synthetic transformed shape samples with precomputed projection staging",
            "gate": "INTERP_TRILINEAR shape write probe; Eigen::VectorXf histogram writes included, public compute dispatch excluded",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "scalar += accumulation into Eigen histogram bins after RVV staging",
            "checksum_policy": "tolerance-aligned quantized Eigen histogram checksum",
            "asm_boundary": "accumulateTrilinearHistogramEigenRVVStaged / computeTrilinearInterpolationRVVToBuffers in bench_gasd_rvv",
            "notes": "Eigen-backed histogram write probe diagnostic. It keeps scalar Eigen accumulation for correctness and measures whether RVV trilinear staging still helps once the production-like histogram container layout is included.",
        }
    if name == "candidate_trilinear_histogram_write_rvv":
        return {
            "group": "gasd-trilinear-histogram-write-probe",
            "case_kind": "component-ablation",
            "point_type": "PointXYZ projection staging -> trilinear index/weight buffers -> flat histogram",
            "timer_boundary": "trilinear interpolation staging plus scalar flat-histogram accumulation",
            "row_source": "synthetic transformed shape samples with precomputed projection staging",
            "gate": "INTERP_TRILINEAR shape write probe; flat histogram writes included, Eigen::VectorXf production layout excluded",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "scalar += accumulation into flat histogram bins after RVV staging",
            "checksum_policy": "tolerance-aligned quantized flat histogram checksum",
            "asm_boundary": "accumulateTrilinearHistogramRVVStaged / computeTrilinearInterpolationRVVToBuffers in bench_gasd_rvv",
            "notes": "Histogram write probe diagnostic. It keeps scalar accumulation for correctness and measures whether RVV trilinear staging still helps once flat histogram writes are included.",
        }
    if name == "candidate_trilinear_interpolation_rvv":
        return {
            "group": "gasd-trilinear-interpolation-staging",
            "case_kind": "component-ablation",
            "point_type": "PointXYZ projection staging -> trilinear index/weight buffers",
            "timer_boundary": "trilinear interpolation arithmetic/index staging only",
            "row_source": "synthetic transformed shape samples with precomputed projection staging",
            "gate": "INTERP_TRILINEAR arithmetic only; histogram writes and scatter are excluded from the timer boundary",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "not_applicable; per-sample index/weight staging only",
            "checksum_policy": "tolerance-aligned quantized trilinear interpolation staging checksum",
            "asm_boundary": "computeTrilinearInterpolationRVVToBuffers in bench_gasd_rvv",
            "notes": "Trilinear interpolation diagnostic. It computes grid/histogram indices and eight spatial weights but does not update Eigen histogram bins.",
        }
    if name == "candidate_color_hue_rvv":
        return {
            "group": "gasd-color-hue-projection",
            "case_kind": "component-ablation",
            "point_type": "PointXYZRGBA -> hue/hbin staging buffers",
            "timer_boundary": "color hue and hbin staging only",
            "row_source": "synthetic transformed color samples",
            "gate": "dense PointXYZRGBA samples with production RGB hue semantics, including grayscale diff==0 lanes",
            "mask": "RGB max-channel masks and diff==0 grayscale mask",
            "reduction": "not_applicable; per-sample hue/hbin staging only",
            "checksum_policy": "tolerance-aligned quantized color hue staging buffer checksum",
            "asm_boundary": "projectColorHueRVVToBuffers in bench_gasd_rvv",
            "notes": "Color hue diagnostic. It computes production-style hue and color histogram bin staging but does not write color histograms or run interpolation.",
        }
    if name == "candidate_shape_projection_rvv":
        return {
            "group": "gasd-shape-sample-projection",
            "case_kind": "component-ablation",
            "point_type": "PointXYZ -> projection staging buffers",
            "timer_boundary": "shape sample projection staging only",
            "row_source": "synthetic transformed shape samples",
            "gate": "finite PointXYZ samples, production-like max_coord and distance normalization factor derived from the synthetic cloud",
            "mask": "not_applicable; dense finite synthetic samples",
            "reduction": "not_applicable; per-sample staging only",
            "checksum_policy": "tolerance-aligned quantized shape projection staging buffer checksum",
            "asm_boundary": "projectShapeSamplesRVVToBuffers in bench_gasd_rvv",
            "notes": "Shape projection diagnostic. It computes normalized grid coordinates and distance histogram bins but does not write histograms or run interpolation.",
        }
    if name == "candidate_color_copy_rvv":
        return {
            "group": "gasd-color-histogram-copy",
            "case_kind": "component-ablation",
            "point_type": "PointXYZRGBA -> GASDSignature984",
            "timer_boundary": "color fixed-grid histogram boundary merge plus contiguous copy",
            "row_source": "synthetic color histogram grid",
            "gate": "fixed color grid, finite float bins, scalar circular boundary merge retained",
            "mask": "not_applicable; dense finite synthetic histogram",
            "reduction": "not_applicable; contiguous copy after boundary merge",
            "checksum_policy": "quantized descriptor-copy checksum",
            "asm_boundary": "copyColorHistogramsRVVToBuffer in bench_gasd_rvv",
            "notes": "Color copy diagnostic. It includes color histogram boundary merge but does not include hue projection or sample interpolation.",
        }
    return {
        "group": "gasd-shape-histogram-copy",
        "case_kind": "component-ablation",
        "point_type": "PointXYZ -> GASDSignature512",
        "timer_boundary": "shape fixed-grid contiguous histogram copy",
        "row_source": "synthetic shape histogram grid",
        "gate": "fixed shape grid, finite float bins, no interpolation state",
        "mask": "not_applicable; dense finite synthetic histogram",
        "reduction": "not_applicable; contiguous copy",
        "checksum_policy": "quantized descriptor-copy checksum",
        "asm_boundary": "copyShapeHistogramsRVVToBuffer in bench_gasd_rvv",
        "notes": "Shape copy diagnostic. It screens the shared copy tail but does not include alignment, projection, interpolation or public compute dispatch.",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--title", default="GASD fixed-grid copy diagnostic repeated board summary")
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
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

    asm_lines = 0
    if args.asm.exists():
        asm_lines = sum(
            1
            for line in args.asm.read_text(encoding="utf-8", errors="ignore").splitlines()
            if any(op in line for op in ("vle32", "vlse32", "vse32", "vsetvli", "vfmacc", "vfsqrt", "vfcvt"))
        )

    args.summary_output.parent.mkdir(parents=True, exist_ok=True)
    summary_evidence_role = "diagnostic"
    summary_ab_boundary = "test helper"
    if args.case_name:
        summary_meta = metadata_for_case(args.case_name)
        summary_evidence_role = summary_meta.get("evidence_role", "diagnostic")
        summary_ab_boundary = summary_meta.get("ab_boundary", "test helper")
    summary_lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(summary_paths)}`",
        f"- evidence_role: `{summary_evidence_role}`",
        f"- A/B boundary: `{summary_ab_boundary}`",
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
        evidence_role = meta.get("evidence_role", "diagnostic")
        ab_boundary = meta.get("ab_boundary", "test helper")
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
            "boundary": ab_boundary.replace(" ", "_"),
            "wrapper": "test-rvv/features/gasd/src/bench_gasd.cpp",
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
        std_checksums = [
            parse_checksum(Path(path).with_name("run_bench_std.log"), name)
            for path in paths
        ]
        rvv_checksums = [
            parse_checksum(Path(path).with_name("run_bench_rvv.log"), name)
            for path in paths
        ]
        comparisons_manifest.append(
            {
                "name": name,
                "group": meta["group"],
                "evidence_role": evidence_role,
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": "half-grid and hists-size from dataset line",
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
                "baseline": {"label": "Std build", "checksum": unique_or_mixed(std_checksums), **common},
                "candidate": {"label": "RVV build", "checksum": unique_or_mixed(rvv_checksums), **common},
                "notes": meta["notes"],
            }
        )

    summary_lines += [
        "",
        "## Evidence boundary",
        "",
        "这是未接 production（生产源码）的性能证据。Std/RVV 两侧共享同一个 bench wrapper；若 case 是 public compute profile，生产源码相同，结果只能作为 build-level profile signal（构建层级性能信号）。计时只覆盖 manifest 中的 timer boundary。",
    ]
    args.summary_output.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    if not comparisons_manifest:
        raise SystemExit("no benchmark comparisons matched the requested case")

    first_context = contexts[0] if contexts else {}
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": comparisons_manifest[0]["evidence_role"],
            "summary_path": str(args.summary_output),
            "summary_paths": [str(path) for path in summary_paths],
            "label": args.run_label,
            "analysis_script": "test-rvv/features/gasd/script/generate_gasd_evidence_manifest.py",
            "metadata": {
                "device": "board via Makefile config",
                "dataset": first_context.get("dataset", "synthetic gasd grid"),
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
