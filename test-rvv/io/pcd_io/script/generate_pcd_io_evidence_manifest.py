#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for pcd_io component evidence."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path

CASE_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+)\s*:\s*(?P<avg>[0-9.]+)\s+ms/iter")
TOTAL_RE = re.compile(
    r"Total Time:\s*(?P<total>[0-9.]+)\s+ms,\s*checksum:\s*(?P<checksum>[0-9]+),\s*"
    r"points:\s*(?P<points>[0-9]+),\s*point_step:\s*(?P<point_step>[0-9]+)"
)


def parse_log(path: Path) -> dict:
    lines = path.read_text(encoding="utf-8").splitlines()
    cases: dict[str, dict] = {}
    index = 0
    while index < len(lines):
        line = lines[index]
        match = CASE_RE.match(line)
        if match and index + 1 < len(lines):
            total = TOTAL_RE.search(lines[index + 1])
            if total:
                cases[match.group("name")] = {
                    "avg_ms": float(match.group("avg")),
                    "total_ms": float(total.group("total")),
                    "checksum": total.group("checksum"),
                    "points": int(total.group("points")),
                    "point_step": int(total.group("point_step")),
                }
                index += 1
        index += 1
    if not cases:
        raise ValueError(f"no benchmark cases parsed from {path}")
    return {"path": str(path), "cases": cases}


def field_layout_for(name: str) -> str:
    if "padded" in name:
        return "xyzi_4x_u32_with_tail_padding"
    return "xyzi_4x_u32_contiguous"


def direction_for(name: str) -> str:
    if name.startswith("production_writer"):
        return "production_writer_public_ostream"
    if name.startswith("writer_payload"):
        return "writer_payload_aos_to_lzf"
    if name.startswith("reader_payload"):
        return "reader_payload_lzf_to_aos_and_finite_scan"
    if name.startswith("unpack"):
        return "field_major_to_aos"
    return "aos_to_field_major"


def metadata_for(name: str) -> dict:
    if name.startswith("production_writer"):
        return {
            "group": "pcd_io_production_writer",
            "evidence_role": "production_public",
            "case_kind": "production_public",
            "ab_boundary": "public_overload",
            "wrapper": "PCDWriter::writeBinaryCompressed(std::ostream&)",
            "row_source": "PCLPointCloud2_public_writer_aos_to_binary_compressed",
            "timer_boundary": "public_ostream_header_plus_pack_plus_lzf_plus_stream_checksum_after_timing",
            "candidate_label": "production-rvv-stride-u32-pack-plus-lzf",
            "asm_boundary": "writeBinaryCompressed production helper",
        }
    if name.startswith("writer_payload"):
        return {
            "group": "pcd_io_writer_payload",
            "evidence_role": "production_shaped_diagnostic",
            "case_kind": "production_shaped",
            "ab_boundary": "test_helper",
            "wrapper": "pcd_io_compressed_writer_payload_shaped",
            "row_source": "PCLPointCloud2_writer_payload_aos_to_lzf",
            "timer_boundary": "pack_to_field_major_plus_lzf_checksum_after_timing",
            "candidate_label": "rvv-stride-u32-pack-plus-lzf",
            "asm_boundary": "makeCompressedWriterPayloadCandidate inlined bench path",
        }
    if name.startswith("reader_payload"):
        return {
            "group": "pcd_io_reader_payload",
            "evidence_role": "production_shaped_diagnostic",
            "case_kind": "production_shaped",
            "ab_boundary": "test_helper",
            "wrapper": "pcd_io_compressed_reader_payload_shaped",
            "row_source": "PCLPointCloud2_reader_payload_lzf_to_aos_and_finite_scan",
            "timer_boundary": "lzf_decompress_plus_unpack_plus_finite_scan_checksum_after_timing",
            "candidate_label": "rvv-stride-u32-unpack-plus-finite-scan",
            "asm_boundary": "readCompressedBodyCandidate inlined bench path",
        }
    return {
        "group": "pcd_io_layout_conversion",
        "evidence_role": "component_ablation",
        "case_kind": "test_helper",
        "ab_boundary": "test_helper",
        "wrapper": "pcd_io_layout_conversion_component",
        "row_source": f"PCLPointCloud2_{direction_for(name)}",
        "timer_boundary": "layout_conversion_only_checksum_after_timing",
        "candidate_label": "rvv-stride-u32",
        "asm_boundary": "packFieldsCandidate/unpackFieldsCandidate inlined bench path",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repeat-dir", required=True)
    parser.add_argument("--summary-log", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    repeat_dir = Path(args.repeat_dir)
    summary_log = Path(args.summary_log)
    run_pairs = []
    for run_dir in sorted(repeat_dir.glob("run*")):
        std_log = run_dir / "run_bench_std.log"
        rvv_log = run_dir / "run_bench_rvv.log"
        if std_log.exists() and rvv_log.exists():
            run_pairs.append((run_dir.name, parse_log(std_log), parse_log(rvv_log)))
    if not run_pairs:
        raise ValueError(f"no run*/run_bench_std.log + run_bench_rvv.log pairs found in {repeat_dir}")

    case_names = set(run_pairs[0][1]["cases"]) & set(run_pairs[0][2]["cases"])
    for _run_id, std, rvv in run_pairs[1:]:
        case_names &= set(std["cases"]) & set(rvv["cases"])

    comparisons = []
    for name in sorted(case_names):
        metadata = metadata_for(name)
        std_cases = [std["cases"][name] for _run_id, std, _rvv in run_pairs]
        rvv_cases = [rvv["cases"][name] for _run_id, _std, rvv in run_pairs]
        std_checksums = {case["checksum"] for case in std_cases}
        rvv_checksums = {case["checksum"] for case in rvv_cases}
        if len(std_checksums) != 1 or len(rvv_checksums) != 1:
            raise ValueError(f"unstable checksum across repeated logs for {name}")

        std_ms_values = [case["avg_ms"] for case in std_cases]
        rvv_ms_values = [case["avg_ms"] for case in rvv_cases]
        ba_values = [std_ms / rvv_ms for std_ms, rvv_ms in zip(std_ms_values, rvv_ms_values)]
        std_case = std_cases[-1]
        rvv_case = rvv_cases[-1]
        comparisons.append(
            {
                "name": name,
                "group": metadata["group"],
                "evidence_role": metadata["evidence_role"],
                "case_kind": metadata["case_kind"],
                "point_type": "PCLPointCloud2 synthetic fields",
                "size": std_case["points"],
                "run_count": len(run_pairs),
                "iterations": 20,
                "warmup_iterations": 3,
                "baseline": {
                    "label": "scalar-reference",
                    "boundary": metadata["ab_boundary"],
                    "wrapper": metadata["wrapper"],
                    "row_source": metadata["row_source"],
                    "solve": False,
                    "checksum_policy": "bytes_fnv1a",
                    "checksum": std_case["checksum"],
                    "timer_boundary": metadata["timer_boundary"],
                    "gate": field_layout_for(name),
                    "mask": "not_applicable",
                    "reduction": "not_applicable",
                    "asm_boundary": "scalar_reference",
                    "rvv_instr_count": 0,
                    "std_ms": statistics.fmean(std_ms_values),
                    "rvv_ms": statistics.fmean(std_ms_values),
                },
                "candidate": {
                    "label": metadata["candidate_label"],
                    "boundary": metadata["ab_boundary"],
                    "wrapper": metadata["wrapper"],
                    "row_source": metadata["row_source"],
                    "solve": False,
                    "checksum_policy": "bytes_fnv1a",
                    "checksum": rvv_case["checksum"],
                    "timer_boundary": metadata["timer_boundary"],
                    "gate": field_layout_for(name),
                    "mask": "not_applicable",
                    "reduction": "not_applicable",
                    "asm_boundary": metadata["asm_boundary"],
                    "rvv_instr_count": None,
                    "std_ms": statistics.fmean(std_ms_values),
                    "rvv_ms": statistics.fmean(rvv_ms_values),
                },
                "ba_values": ba_values,
            }
        )

    evidence_roles = {comparison["evidence_role"] for comparison in comparisons}
    all_production_writer = all(comparison["name"].startswith("production_writer") for comparison in comparisons)
    all_writer = all(comparison["name"].startswith("writer_payload") for comparison in comparisons)
    all_reader = all(comparison["name"].startswith("reader_payload") for comparison in comparisons)
    if evidence_roles == {"production_public"} and all_production_writer:
        summary_title = "pcd_io compressed writer production public repeated board summary"
        summary_role = "production_public"
        summary_label = "pcd_io board repeated phase050 production writer"
        boundary_note = (
            "Bench measures public PCDWriter::writeBinaryCompressed(std::ostream&): "
            "binary_compressed header, field layout conversion, LZF compression and ostream payload write."
        )
    elif evidence_roles == {"production_shaped_diagnostic"} and all_writer:
        summary_title = "pcd_io compressed writer payload production-shaped diagnostic repeated board summary"
        summary_role = "production_shaped_diagnostic"
        summary_label = "pcd_io board repeated phase010 writer payload"
        boundary_note = (
            "Bench measures compressed writer payload: field layout conversion plus LZF compression; "
            "text header, ostream flush, mmap / file I/O and reader finite scan are out of scope."
        )
    elif evidence_roles == {"production_shaped_diagnostic"} and all_reader:
        summary_title = "pcd_io compressed reader payload production-shaped diagnostic repeated board summary"
        summary_role = "production_shaped_diagnostic"
        summary_label = "pcd_io board repeated phase040 reader payload"
        boundary_note = (
            "Bench measures compressed reader payload: LZF decompression, field-major unpack and finite scan; "
            "text header, ostream flush, mmap / file I/O and writer payload generation are out of scope."
        )
    elif evidence_roles == {"component_ablation"}:
        summary_title = "pcd_io component ablation repeated board summary"
        summary_role = "component_ablation"
        summary_label = "pcd_io board repeated phase000"
        boundary_note = (
            "Bench measures field layout conversion only; LZF, mmap, stream I/O and finite scan are out of scope."
        )
    else:
        summary_title = "pcd_io mixed-boundary repeated board summary"
        summary_role = "mixed_boundary_diagnostic"
        summary_label = "pcd_io board repeated mixed boundary"
        boundary_note = (
            "Bench contains mixed evidence roles; per-comparison metadata defines each usable evidence boundary."
        )

    manifest = {
        "schema_version": 1,
        "topic": "pcd_io",
        "module": "io",
        "source": "io/src/pcd_io.cpp",
        "summary": {
            "title": summary_title,
            "evidence_role": summary_role,
            "summary_path": str(summary_log),
            "label": summary_label,
            "analysis_script": "test-rvv/io/pcd_io/script/generate_pcd_io_evidence_manifest.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "dataset": "synthetic PCLPointCloud2 compressed layout conversion component",
                "iterations": 20,
                "warmup_iterations": 3,
                "run_count": len(run_pairs),
            },
        },
        "environment": {"device": "Milkv-Jupiter"},
        "checks": {"strict_ab": False},
        "comparisons": comparisons,
        "notes": [
            boundary_note,
            "QEMU timing is not used for performance conclusions.",
        ],
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
