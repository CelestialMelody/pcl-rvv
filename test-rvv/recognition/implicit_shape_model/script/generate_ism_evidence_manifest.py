#!/usr/bin/env python3
"""Generate ISM diagnostic repeated board summary and Evidence Doctor manifest."""

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
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s+checksum:\s*(?P<checksum>[0-9]+)", re.MULTILINE)
BEST_INDEX_RE = re.compile(r"best_index:\s*(?P<value>[0-9]+)")
SIGMA_RE = re.compile(r"sigma:\s*(?P<value>[0-9.]+)")
DENSITY_RE = re.compile(r"(?:^|,\s*)density:\s*(?P<value>[0-9.]+)")
ASSIGNED_RE = re.compile(r"assigned:\s*(?P<value>[0-9]+)")
VOTES_RE = re.compile(r"votes:\s*(?P<value>[0-9]+)")
STRONGEST_PEAK_DENSITY_RE = re.compile(r"strongest_peak_density:\s*(?P<value>[0-9.]+)")
PEAK_FINGERPRINT_RE = re.compile(r"peak_fingerprint:\s*(?P<value>[0-9]+)")
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup(?:\s+Iterations)?\s*:\s*(?P<warmup>\d+)", re.MULTILINE)
DATASET_RE = re.compile(r"^\s*Dataset:\s*(?P<dataset>.+?)\s*$", re.MULTILINE)
BUILD_RE = re.compile(r"^\s*Build Path:\s*(?P<build>.+?)\s*$", re.MULTILINE)


def parse_context(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context: dict[str, Any] = {}
    if match := ITER_RE.search(text):
        context["iterations"] = int(match.group("iterations"))
    if match := WARMUP_RE.search(text):
        context["warmup_iterations"] = int(match.group("warmup"))
    if match := DATASET_RE.search(text):
        context["dataset"] = match.group("dataset").strip()
    if match := BUILD_RE.search(text):
        context["build_path"] = match.group("build").strip()
    return context


def parse_summary(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    context = parse_context(path)
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


def parse_case_detail(path: Path, case_name: str) -> dict[str, Any]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    case_line = ""
    for index, line in enumerate(lines):
        if line.lstrip().startswith(case_name) and index + 1 < len(lines):
            case_line = lines[index + 1]
            break
    result: dict[str, Any] = {}
    if match := BEST_INDEX_RE.search(case_line):
        result["best_index"] = int(match.group("value"))
    if match := SIGMA_RE.search(case_line):
        result["sigma"] = float(match.group("value"))
    if match := DENSITY_RE.search(case_line):
        result["density"] = float(match.group("value"))
    if match := ASSIGNED_RE.search(case_line):
        result["assigned"] = int(match.group("value"))
    if match := VOTES_RE.search(case_line):
        result["votes"] = int(match.group("value"))
    if match := STRONGEST_PEAK_DENSITY_RE.search(case_line):
        result["strongest_peak_density"] = float(match.group("value"))
    if match := PEAK_FINGERPRINT_RE.search(case_line):
        result["peak_fingerprint"] = int(match.group("value"))
    return result


def semantic_checksum(case_name: str, baseline: dict[str, Any], candidate: dict[str, Any]) -> str:
    if case_name == "descriptor_cluster_distance":
        same_index = baseline.get("best_index") == candidate.get("best_index")
        lhs = baseline.get("best_index", "missing")
        return f"semantic:best_index={lhs}:match={same_index}"
    if case_name == "sigma_pairwise_max_dot":
        lhs = float(baseline.get("sigma", float("nan")))
        rhs = float(candidate.get("sigma", float("nan")))
        return f"semantic:sigma:abs_tol=1e-4:pass={abs(lhs - rhs) <= 1e-4}"
    if case_name == "vote_density_gaussian_sum":
        lhs = float(baseline.get("density", float("nan")))
        rhs = float(candidate.get("density", float("nan")))
        tolerance = max(1e-3, abs(lhs) * 3e-5)
        return f"semantic:density:tol={tolerance:.6g}:pass={abs(lhs - rhs) <= tolerance}"
    if case_name == "descriptor_batch_assignment":
        same_assigned = baseline.get("assigned") == candidate.get("assigned")
        lhs = baseline.get("assigned", "missing")
        return f"semantic:assigned_count={lhs}:match={same_assigned}"
    if case_name == "public_find_objects_descriptor_assignment":
        same_votes = baseline.get("votes") == candidate.get("votes")
        same_peak_density = baseline.get("strongest_peak_density") == candidate.get("strongest_peak_density")
        same_peak_fingerprint = baseline.get("peak_fingerprint") == candidate.get("peak_fingerprint")
        return (
            "semantic:public_votes={}:votes_match={}:peak_density_match={}:peak_fingerprint_match={}".format(
                baseline.get("votes", "missing"),
                same_votes,
                same_peak_density,
                same_peak_fingerprint,
            )
        )
    raise ValueError(f"unknown ISM case label: {case_name}")


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


def metadata_for_case(name: str) -> dict[str, str]:
    common = {
        "group": "implicit-shape-model-local-formula-diagnostic",
        "case_kind": "diagnostic",
        "evidence_role": "diagnostic",
        "point_type": "pcl::PointXYZ-like synthetic float data",
        "boundary": "test_helper",
        "wrapper": "test-rvv/recognition/implicit_shape_model/src/bench_ism.cpp",
        "checksum_policy": "semantic fingerprint aligned with QEMU tolerance tests; raw_bit_checksum is retained for drift review",
        "notes": "Phase 000 local formula diagnostic; correctness is established by run_test_compare tolerances, not bit-identical floating-point reductions.",
    }
    per_case = {
        "descriptor_cluster_distance": {
            "row_source": "single_descriptor_against_cluster_centers",
            "size": "FeatureSize=153 descriptor against configurable cluster centers",
            "timer_boundary": "nearest cluster squared L2 distance loop; excludes FPFH estimation and vote creation",
            "gate": "contiguous float descriptor and centers; no Eigen dynamic allocation in timed loop",
            "mask": "no finite mask; synthetic finite positive descriptor values",
            "reduction": "per-center vector sum of squared differences",
            "asm_boundary": "nearestClusterDistanceRVV in bench_ism_rvv",
        },
        "sigma_pairwise_max_dot": {
            "row_source": "single_training_cloud_pairwise_points",
            "size": "configurable synthetic PointXYZ-like AoS cloud",
            "timer_boundary": "calculateSigmas local pairwise max-dot formula; excludes class grouping and average sigma reduction",
            "gate": "positive finite x/y/z fields and count >= 2",
            "mask": "triangular i<j pair mask expressed by scalar outer loop and RVV inner j loop",
            "reduction": "per-row vector max reduction plus scalar max across rows",
            "asm_boundary": "maxPairwiseDotSigmaRVV in bench_ism_rvv",
        },
        "vote_density_gaussian_sum": {
            "row_source": "radius_search_result_distances_and_vote_strengths",
            "size": "configurable synthetic squared distances and vote strengths",
            "timer_boundary": "ISMVoteList density Gaussian weighted sum after radiusSearch; excludes tree lookup and vote list traversal",
            "gate": "finite squared distances and positive sigma",
            "mask": "no branch mask; every synthetic neighbor contributes",
            "reduction": "RVV expf helper plus vector sum of strength * exp(-dist/sigma^2)",
            "asm_boundary": "densityWeightedSumRVV / pcl::expf_RVV_f32m2 in bench_ism_rvv",
        },
        "descriptor_batch_assignment": {
            "row_source": "descriptor_batch_against_cluster_centers",
            "size": "synthetic DescriptorBatch=512 with FeatureSize=153",
            "timer_boundary": "multi-keypoint descriptor assignment with descriptor_sum gate; excludes feature estimation and vote generation",
            "gate": "descriptor_sum epsilon gate and contiguous float batch/centers",
            "mask": "zero descriptor rows skipped by scalar gate",
            "reduction": "per-descriptor nearest cluster assignment",
            "asm_boundary": "descriptorBatchAssignmentRVV in bench_ism_rvv",
        },
        "public_find_objects_descriptor_assignment": {
            "row_source": "public_find_objects_deterministic_fixture",
            "size": "deterministic public-entry model, FeatureSize=153, descriptors=512, clusters=184",
            "timer_boundary": "public findObjects() entry with deterministic test-only feature estimator; excludes training and file I/O",
            "gate": "deterministic model and public entry fixture",
            "mask": "zero descriptor rows skipped by scalar gate",
            "reduction": "per-keypoint nearest cluster assignment inside real findObjects()",
            "asm_boundary": "findNearestClusterIndexRVV inside findObjects()",
            "group": "implicit-shape-model-public-entry-production-direct",
            "case_kind": "production_direct",
            "evidence_role": "production_direct",
            "point_type": "pcl::PointXYZ / pcl::Normal",
            "boundary": "public_overload",
            "wrapper": "test-rvv/recognition/implicit_shape_model/src/bench_ism.cpp public-entry bench helper",
            "checksum_policy": "semantic fingerprint aligned with the public-entry vote count plus strongest peak summary; raw_bit_checksum is retained for drift review",
            "notes": "Phase 020 production-direct repeated board evidence for real trainISM/findObjects public entry; correctness is checked against the upstream test fixture, and checksum fingerprints the public-entry vote count plus strongest peak summary.",
        },
    }
    if name not in per_case:
        raise ValueError(f"unknown ISM case label: {name}")
    return {**common, **per_case[name]}


def count_rvv_asm_lines(path: Path) -> int:
    if not path.exists():
        return 0
    needles = ("vle32", "vlse32", "vfmul", "vfmacc", "vfred", "vsetvli", "vf")
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
    parser.add_argument("--title", default="ISM local formula diagnostic repeated board summary")
    parser.add_argument("--run-label", default="ism_phase000_local_formula_diagnostic_repeated")
    parser.add_argument("--case-name")
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--evidence-role", default="diagnostic")
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
            metadata_for_case(name)
            bucket = grouped.setdefault(name, {"std": [], "rvv": [], "speedup": [], "paths": []})
            bucket["std"].append(float(item["std_ms"]))
            bucket["rvv"].append(float(item["rvv_ms"]))
            bucket["speedup"].append(float(item["speedup"]))
            bucket["paths"].append(str(item["summary_path"]))

    if not grouped:
        raise SystemExit("no benchmark comparisons parsed")

    asm_lines = count_rvv_asm_lines(args.asm)
    args.summary_output.parent.mkdir(parents=True, exist_ok=True)

    grouped_metadata = [metadata_for_case(case_name) for case_name in sorted(grouped)]
    summary_boundary = unique_or_mixed([meta["boundary"] for meta in grouped_metadata])
    summary_kind = unique_or_mixed([meta["case_kind"] for meta in grouped_metadata])
    summary_role = unique_or_mixed([meta["evidence_role"] for meta in grouped_metadata])

    summary_lines = [
        f"# {args.title}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(summary_paths)}`",
        f"- iterations: `{unique_or_mixed([ctx.get('iterations') for ctx in contexts])}`",
        f"- warmup_iterations: `{unique_or_mixed([ctx.get('warmup_iterations') for ctx in contexts])}`",
        f"- evidence_role: `{args.evidence_role}`",
        f"- A/B boundary: `{summary_boundary}`",
        f"- asm_rvv_line_count: `{asm_lines}`",
        "- checksum_policy: `semantic fingerprint; raw bit checksums are preserved in manifest`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]

    comparisons_manifest: list[dict[str, Any]] = []
    all_speedups: list[float] = []
    for case_name, stats in sorted(grouped.items()):
        speedups = [float(value) for value in stats["speedup"]]
        std_values = [float(value) for value in stats["std"]]
        rvv_values = [float(value) for value in stats["rvv"]]
        paths = [str(value) for value in stats["paths"]]
        all_speedups.extend(speedups)
        meta = metadata_for_case(case_name)
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
        std_checksums = [parse_checksum(Path(path).with_name("run_bench_std.log"), case_name) for path in paths]
        rvv_checksums = [parse_checksum(Path(path).with_name("run_bench_rvv.log"), case_name) for path in paths]
        std_details = [parse_case_detail(Path(path).with_name("run_bench_std.log"), case_name) for path in paths]
        rvv_details = [parse_case_detail(Path(path).with_name("run_bench_rvv.log"), case_name) for path in paths]
        semantic_checksums = [
            semantic_checksum(case_name, std_detail, rvv_detail)
            for std_detail, rvv_detail in zip(std_details, rvv_details)
        ]
        semantic_value = unique_or_mixed(semantic_checksums)
        side_contract = {
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
        }
        comparisons_manifest.append(
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
                "build_comparison": True,
                "std_ms": statistics.mean(std_values),
                "rvv_ms": statistics.mean(rvv_values),
                "ba_values": speedups,
                "std_ms_values": std_values,
                "rvv_ms_values": rvv_values,
                "run_paths": paths,
                "baseline": {
                    "label": "Std build",
                    "checksum": semantic_value,
                    "raw_bit_checksum": unique_or_mixed(std_checksums),
                    "result_details": unique_or_mixed(std_details),
                    **side_contract,
                },
                "candidate": {
                    "label": "RVV build",
                    "checksum": semantic_value,
                    "raw_bit_checksum": unique_or_mixed(rvv_checksums),
                    "result_details": unique_or_mixed(rvv_details),
                    **side_contract,
                },
                "same_checksum": unique_or_mixed(std_checksums) == unique_or_mixed(rvv_checksums),
                "same_semantic_checksum": all(
                    value.endswith("pass=True")
                    or value.endswith(":match=True")
                    or (
                        "votes_match=True" in value
                        and "peak_density_match=True" in value
                        and "peak_fingerprint_match=True" in value
                    )
                    for value in semantic_checksums
                ),
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
                "decision_bucket": decision_bucket(speedups),
                "notes": meta["notes"],
            }
        )

    decision = decision_bucket(all_speedups)
    summary_lines.extend(
        [
            "",
            f"- overall_decision_bucket: `{decision}`",
            "",
            "## 证据边界",
            "",
            "本 summary 的 evidence role（证据角色）来自 manifest 中的 case metadata"
            f"（用例元数据）：case_kind=`{summary_kind}`，evidence_role=`{summary_role}`，"
            f"A/B boundary=`{summary_boundary}`。Std/RVV 两侧共享 manifest 记录的 wrapper、"
            "计时边界和 checksum policy；是否能支撑 production direct（真实生产路径直连）结论，"
            "以对应 case 的 metadata、Evidence Doctor 和 phase result 为准。",
            "",
        ]
    )
    args.summary_output.write_text("\n".join(summary_lines), encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "title": args.title,
        "run_label": args.run_label,
        "evidence_role": args.evidence_role,
        "expected_runs": args.expected_runs,
        "collected_runs": len(summary_paths),
        "decision_bucket": decision,
        "comparisons": comparisons_manifest,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote {args.summary_output}")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
