#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for point_coding board logs.

本脚本只解析 point_coding topic 的 board bench 日志，把 Std/RVV
日志里的 case label、平均耗时、checksum 和 repeated speedup（重复加速比）
转成 Evidence Doctor（证据体检）manifest。它不读取 raw board 环境，也不把
component ablation（组件消融）升级成 production evidence（生产证据）。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"Iterations:\s*(?P<iterations>[0-9]+)")
WARMUP_RE = re.compile(r"Warmup Iterations:\s*(?P<warmup>[0-9]+)")
DATASET_RE = re.compile(r"Dataset:\s*(?P<dataset>.+)")


CASE_METADATA = {
    "encode_indexed_16": {
        "group": "encode-indexed-gather",
        "size": 16,
        "asm_boundary": "encodePointsRVV / inlined bench path",
        "candidate_label": "indexed_gather_scalar_quantize_v0",
        "timer_boundary": "component_encode_only_checksum_after_timing",
    },
    "encode_indexed_64": {
        "group": "encode-indexed-gather",
        "size": 64,
        "asm_boundary": "encodePointsRVV / inlined bench path",
        "candidate_label": "indexed_gather_scalar_quantize_v0",
        "timer_boundary": "component_encode_only_checksum_after_timing",
    },
    "encode_indexed_256": {
        "group": "encode-indexed-gather",
        "size": 256,
        "asm_boundary": "encodePointsRVV / inlined bench path",
        "candidate_label": "indexed_gather_scalar_quantize_v0",
        "timer_boundary": "component_encode_only_checksum_after_timing",
    },
    "encode_indexed_1024": {
        "group": "encode-indexed-gather",
        "size": 1024,
        "asm_boundary": "encodePointsRVV / inlined bench path",
        "candidate_label": "indexed_gather_scalar_quantize_v0",
        "timer_boundary": "component_encode_only_checksum_after_timing",
    },
    "encode_indexed_4096": {
        "group": "encode-indexed-gather",
        "size": 4096,
        "asm_boundary": "encodePointsRVV / inlined bench path",
        "candidate_label": "indexed_gather_scalar_quantize_v0",
        "timer_boundary": "component_encode_only_checksum_after_timing",
    },
    "encode_indexed_16384": {
        "group": "encode-indexed-gather",
        "size": 16384,
        "asm_boundary": "encodePointsRVV / inlined bench path",
        "candidate_label": "indexed_gather_scalar_quantize_v0",
        "timer_boundary": "component_encode_only_checksum_after_timing",
    },
    "decode_contiguous_16": {
        "group": "decode-contiguous",
        "size": 16,
        "asm_boundary": "decodePointsRVV / inlined bench path",
        "candidate_label": "contiguous_decode_v0",
        "timer_boundary": "component_decode_only_checksum_after_timing",
    },
    "decode_contiguous_64": {
        "group": "decode-contiguous",
        "size": 64,
        "asm_boundary": "decodePointsRVV / inlined bench path",
        "candidate_label": "contiguous_decode_v0",
        "timer_boundary": "component_decode_only_checksum_after_timing",
    },
    "decode_contiguous_256": {
        "group": "decode-contiguous",
        "size": 256,
        "asm_boundary": "decodePointsRVV / inlined bench path",
        "candidate_label": "contiguous_decode_v0",
        "timer_boundary": "component_decode_only_checksum_after_timing",
    },
    "decode_contiguous_1024": {
        "group": "decode-contiguous",
        "size": 1024,
        "asm_boundary": "decodePointsRVV / inlined bench path",
        "candidate_label": "contiguous_decode_v0",
        "timer_boundary": "component_decode_only_checksum_after_timing",
    },
    "decode_contiguous_4096": {
        "group": "decode-contiguous",
        "size": 4096,
        "asm_boundary": "decodePointsRVV / inlined bench path",
        "candidate_label": "contiguous_decode_v0",
        "timer_boundary": "component_decode_only_checksum_after_timing",
    },
    "decode_contiguous_16384": {
        "group": "decode-contiguous",
        "size": 16384,
        "asm_boundary": "decodePointsRVV / inlined bench path",
        "candidate_label": "contiguous_decode_v0",
        "timer_boundary": "component_decode_only_checksum_after_timing",
    },
    "decode_context_16": {
        "group": "decode-context",
        "size": 16,
        "asm_boundary": "decodePointsRVVToCloud / inlined bench path",
        "candidate_label": "decode_context_v0",
        "timer_boundary": "production_shaped_decode_context_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_object_state_decode_context",
    },
    "decode_context_64": {
        "group": "decode-context",
        "size": 64,
        "asm_boundary": "decodePointsRVVToCloud / inlined bench path",
        "candidate_label": "decode_context_v0",
        "timer_boundary": "production_shaped_decode_context_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_object_state_decode_context",
    },
    "decode_context_256": {
        "group": "decode-context",
        "size": 256,
        "asm_boundary": "decodePointsRVVToCloud / inlined bench path",
        "candidate_label": "decode_context_v0",
        "timer_boundary": "production_shaped_decode_context_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_object_state_decode_context",
    },
    "decode_context_1024": {
        "group": "decode-context",
        "size": 1024,
        "asm_boundary": "decodePointsRVVToCloud / inlined bench path",
        "candidate_label": "decode_context_v0",
        "timer_boundary": "production_shaped_decode_context_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_object_state_decode_context",
    },
    "decode_context_4096": {
        "group": "decode-context",
        "size": 4096,
        "asm_boundary": "decodePointsRVVToCloud / inlined bench path",
        "candidate_label": "decode_context_v0",
        "timer_boundary": "production_shaped_decode_context_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_object_state_decode_context",
    },
    "decode_context_16384": {
        "group": "decode-context",
        "size": 16384,
        "asm_boundary": "decodePointsRVVToCloud / inlined bench path",
        "candidate_label": "decode_context_v0",
        "timer_boundary": "production_shaped_decode_context_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_object_state_decode_context",
    },
}

for _size in (256, 1024, 4096, 16384):
    CASE_METADATA[f"decode_multileaf_{_size}"] = {
        "group": "decode-multileaf-context",
        "size": _size,
        "asm_boundary": "decodePointsRVVToCloud / multi-leaf inlined bench path",
        "candidate_label": "decode_multileaf_context_v0",
        "timer_boundary": "production_shaped_decode_multileaf_checksum_after_timing",
        "evidence_role": "production_shaped_diagnostic",
        "case_kind": "production_shaped_helper",
        "row_source": "point_coding_multileaf_decode_context",
    }

for _size in (256, 1024, 4096, 16384):
    CASE_METADATA[f"decode_production_direct_{_size}"] = {
        "group": "decode-production-direct",
        "size": _size,
        "asm_boundary": "PointCoding<PointXYZ>::decodePoints production RVV path",
        "candidate_label": "production_decode_pointxyz_v0",
        "timer_boundary": "production_direct_decode_checksum_after_timing",
        "evidence_role": "production_direct",
        "case_kind": "production_entry",
        "row_source": "point_coding_production_decode_stream",
    }

for _point_suffix, _point_type in (("xyzi", "PointXYZI"), ("xyzrgb", "PointXYZRGB")):
    for _size in (1024, 4096):
        CASE_METADATA[f"decode_production_direct_traits_{_point_suffix}_{_size}"] = {
            "group": "decode-production-direct-traits",
            "size": _size,
            "asm_boundary": f"PointCoding<{_point_type}>::decodePoints production RVV path",
            "candidate_label": f"production_decode_traits_{_point_suffix}_v0",
            "timer_boundary": "production_direct_decode_traits_checksum_after_timing",
            "evidence_role": "production_direct",
            "case_kind": "production_entry",
            "row_source": "point_coding_production_decode_stream",
            "point_type": _point_type,
            "wrapper": f"PointCoding<{_point_type}>::decodePoints",
        }

for _size in (256, 1024):
    CASE_METADATA[f"octree_roundtrip_public_{_size}"] = {
        "group": "octree-roundtrip-public",
        "size": _size,
        "asm_boundary": "OctreePointCloudCompression<PointXYZ> public encode/decode with PointCoding decode RVV dispatch",
        "candidate_label": "public_octree_roundtrip_pointxyz_v0",
        "timer_boundary": "production_public_encode_decode_roundtrip_checksum_after_timing",
        "evidence_role": "production_public",
        "case_kind": "public_entry",
        "row_source": "octree_public_encode_decode_stream",
        "point_type": "PointXYZ",
        "wrapper": "OctreePointCloudCompression<PointXYZ>::encodePointCloud/decodePointCloud",
    }
    CASE_METADATA[f"octree_decode_public_{_size}"] = {
        "group": "octree-decode-public",
        "size": _size,
        "asm_boundary": "OctreePointCloudCompression<PointXYZ>::decodePointCloud with PointCoding decode RVV dispatch",
        "candidate_label": "public_octree_decode_pointxyz_v0",
        "timer_boundary": "production_public_decode_only_prebuilt_stream_checksum_after_timing",
        "evidence_role": "production_public",
        "case_kind": "public_entry",
        "row_source": "octree_public_compressed_stream_decode",
        "point_type": "PointXYZ",
        "wrapper": "OctreePointCloudCompression<PointXYZ>::decodePointCloud",
    }


def parse_log(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
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
        if warmup is None:
            match = WARMUP_RE.search(line)
            if match:
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


def collect_log_pairs(args: argparse.Namespace) -> list[tuple[dict, dict]]:
    if args.repeat_dir is None:
        if args.std_log is None or args.rvv_log is None:
            raise ValueError("--std-log and --rvv-log are required when --repeat-dir is not used")
        return [(parse_log(args.std_log), parse_log(args.rvv_log))]

    pairs: list[tuple[dict, dict]] = []
    for run_dir in sorted(args.repeat_dir.glob("run-*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            pairs.append((parse_log(std_log), parse_log(rvv_log)))
    if not pairs:
        raise ValueError(f"no run-*/run_bench_std.log + run_bench_rvv.log pairs found in {args.repeat_dir}")
    return pairs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--repeat-dir", type=Path)
    parser.add_argument("--summary-log", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    log_pairs = collect_log_pairs(args)
    first_std, first_rvv = log_pairs[0]
    case_names = set(first_std["cases"]) & set(first_rvv["cases"])
    for std, rvv in log_pairs[1:]:
        case_names &= set(std["cases"]) & set(rvv["cases"])

    comparisons = []
    for name in sorted(case_names):
        metadata = CASE_METADATA.get(name, {})
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
        timer_boundary = metadata.get("timer_boundary", "component_only_checksum_after_timing")
        case_kind = metadata.get("case_kind", "test_helper")
        wrapper = metadata.get("wrapper", "point_coding_support")
        row_source = metadata.get(
            "row_source",
            "source_indexed_leaf" if name.startswith("encode") else "contiguous_output_segment",
        )
        comparisons.append({
            "name": name,
            "group": metadata.get("group", "point_coding"),
            "evidence_role": metadata.get("evidence_role", "component_ablation"),
            "case_kind": case_kind,
            "point_type": metadata.get("point_type", "PointXYZ"),
            "size": metadata.get("size", 0),
            "run_count": len(log_pairs),
            "baseline": {
                "label": "scalar-reference",
                "boundary": case_kind,
                "wrapper": wrapper,
                "row_source": row_source,
                "solve": False,
                "checksum_policy": "fnv1a_component_output",
                "checksum": std_case["checksum"],
                "timer_boundary": timer_boundary,
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": "scalar_reference",
                "rvv_instr_count": 0,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(std_ms_values),
            },
            "candidate": {
                "label": metadata.get("candidate_label", "rvv-candidate"),
                "boundary": case_kind,
                "wrapper": wrapper,
                "row_source": row_source,
                "solve": False,
                "checksum_policy": "fnv1a_component_output",
                "checksum": rvv_case["checksum"],
                "timer_boundary": timer_boundary,
                "gate": "same_case_filter",
                "mask": "not_applicable",
                "reduction": "not_applicable",
                "asm_boundary": metadata.get("asm_boundary", "bench_point_coding_rvv"),
                "rvv_instr_count": None,
                "std_ms": statistics.fmean(std_ms_values),
                "rvv_ms": statistics.fmean(rvv_ms_values),
            },
            "ba_values": ba_values,
        })

    summary_roles = {comparison["evidence_role"] for comparison in comparisons}
    summary_evidence_role = (
        next(iter(summary_roles)) if len(summary_roles) == 1 else "mixed_diagnostic"
    )
    summary_title = (
        "point_coding production-direct board summary"
        if summary_evidence_role == "production_direct"
        else "point_coding component ablation board summary"
    )

    manifest = {
        "schema_version": 1,
        "summary": {
            "title": summary_title,
            "evidence_role": summary_evidence_role,
            "summary_path": str(args.summary_log),
            "label": "point_coding component ablation",
            "analysis_script": "test-rvv/io/point_coding/script/generate_point_coding_evidence_manifest.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "dataset": first_rvv["dataset"],
                "iterations": first_rvv["iterations"],
                "warmup_iterations": first_rvv["warmup_iterations"],
                "run_count": len(log_pairs),
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
