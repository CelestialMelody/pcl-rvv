#!/usr/bin/env python3
"""
生成 transformation_estimation_2D 的 QEMU smoke Evidence Doctor manifest（证据清单）。

本脚本只解析当前 topic 的 RVV smoke 日志和 asm attribution（反汇编归属）摘要。
它记录 case label、checksum（校验和）、运行参数和诊断边界 metadata，让通用
Evidence Doctor（证据体检）可以检查证据形状。

QEMU timing（QEMU 计时）只保存在 `observed_ms` 字段，Evidence Doctor 不把它当作
repeated performance（重复性能）或 Std/RVV A/B 数据。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")
GENERIC_CASE_RE = re.compile(
    r"^(?:public )?generic 2D (?P<source>[A-Za-z0-9_]+)->(?P<target>[A-Za-z0-9_]+) (?P<size>\d+K)\s*$"
)
SOURCE_INDEXED_GENERIC_CASE_RE = re.compile(
    r"^(?:public )?source-indexed generic 2D (?P<source>[A-Za-z0-9_]+)->(?P<target>[A-Za-z0-9_]+) (?P<size>\d+K)\s*$"
)
DUAL_INDEXED_GENERIC_CASE_RE = re.compile(
    r"^dual-indexed generic 2D (?P<source>[A-Za-z0-9_]+)->(?P<target>[A-Za-z0-9_]+) (?P<size>\d+K)\s*$"
)
CORRESPONDENCE_GENERIC_CASE_RE = re.compile(
    r"^correspondence generic 2D (?P<source>[A-Za-z0-9_]+)->(?P<target>[A-Za-z0-9_]+) (?P<size>\d+K)\s*$"
)


def parse_log(path: Path) -> dict[str, Any]:
    cases: dict[str, dict[str, Any]] = {}
    current_case: str | None = None
    metadata: dict[str, Any] = {}

    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if match := ITER_RE.match(line):
            metadata["iterations"] = int(match.group("value"))
            continue
        if match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
            continue
        if match := DATASET_RE.match(line):
            metadata["dataset"] = match.group("value")
            continue
        if match := BUILD_RE.match(line):
            metadata["build"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"observed_ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["log_checksum"] = match.group("checksum")
            current_case = None

    return {"path": str(path), "metadata": metadata, "cases": cases}


def size_for_case(name: str) -> int:
    if "256K" in name:
        return 256 * 1024
    if "64K" in name:
        return 64 * 1024
    if "4K" in name:
        return 4 * 1024
    return 0


def row_source_for_case(name: str, requested: str) -> str:
    if requested != "auto":
        return requested
    if "source-indexed-cloud-pair" in name:
        return "source_indexed_cloud_pair"
    if "source-indexed generic" in name:
        return "source_indexed_cloud_pair"
    if "dual-indexed-cloud-pair" in name:
        return "dual_indexed_cloud_pair"
    if "dual-indexed generic" in name:
        return "dual_indexed_cloud_pair"
    if "correspondence generic" in name:
        return "correspondence_pair"
    if "correspondence-pair" in name:
        return "correspondence_pair"
    return "ordered_cloud_pair"


def point_types_for_case(name: str, case_filter: str) -> dict[str, str]:
    if case_filter in (
        "generic-xyz-point-types",
        "generic-xyz-point-types-public",
        "source-indexed-generic-xyz-point-types",
        "source-indexed-generic-xyz-point-types-public",
        "source-indexed-generic-xyz-point-types-public-variance",
        "source-indexed-generic-pointnormal-256k-public",
        "dual-indexed-generic-xyz-point-types",
        "correspondence-generic-xyz-point-types",
    ):
        if case_filter in (
            "source-indexed-generic-xyz-point-types",
            "source-indexed-generic-xyz-point-types-public",
            "source-indexed-generic-xyz-point-types-public-variance",
            "source-indexed-generic-pointnormal-256k-public",
        ):
            match = SOURCE_INDEXED_GENERIC_CASE_RE.match(name)
        elif case_filter == "dual-indexed-generic-xyz-point-types":
            match = DUAL_INDEXED_GENERIC_CASE_RE.match(name)
        elif case_filter == "correspondence-generic-xyz-point-types":
            match = CORRESPONDENCE_GENERIC_CASE_RE.match(name)
        else:
            match = GENERIC_CASE_RE.match(name)
        if not match:
            raise SystemExit(f"cannot parse generic point-type case label: {name}")
        source = match.group("source")
        target = match.group("target")
        return {
            "source_point_type": source,
            "target_point_type": target,
            "point_type": f"{source}->{target}",
        }
    return {
        "source_point_type": "PointXYZ",
        "target_point_type": "PointXYZ",
        "point_type": "PointXYZ",
    }


def load_asm_summary(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {
            "attribution_decision": "not_available",
            "candidate_key_mnemonics": {},
            "totals": {},
        }
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise SystemExit(f"asm summary is not a JSON object: {path}")
    return data


def rvv_instr_count_for_category(asm_summary: dict[str, Any], category: str) -> int:
    categories = asm_summary.get("categories", {})
    if category == "production_public_combined":
        return sum(
            categories.get(name, {}).get("total", 0)
            for name in ("production_public_lambda_boundary", "production_public_boundary")
        )
    if category == "production_public_source_indexed_combined":
        return sum(
            categories.get(name, {}).get("total", 0)
            for name in (
                "production_public_source_indexed_lambda_boundary",
                "production_public_source_indexed_boundary",
            )
        )
    if category == "production_public_source_indexed_generic_combined":
        return sum(
            categories.get(name, {}).get("total", 0)
            for name in (
                "production_public_source_indexed_generic_lambda_boundary",
                "production_public_source_indexed_generic_boundary",
                "production_public_source_indexed_boundary",
            )
        )
    if category == "production_public_correspondence_combined":
        return sum(
            categories.get(name, {}).get("total", 0)
            for name in (
                "production_public_correspondence_lambda_boundary",
                "production_public_correspondence_boundary",
            )
        )
    if category == "production_public_generic_combined":
        return sum(
            categories.get(name, {}).get("total", 0)
            for name in (
                "production_public_generic_lambda_boundary",
                "production_public_generic_boundary",
                "production_public_boundary",
            )
        )
    return categories.get(category, {}).get("total", 0)


def make_comparison(
    name: str,
    case: dict[str, Any],
    asm_summary: dict[str, Any],
    args: argparse.Namespace,
) -> dict[str, Any]:
    point_types = point_types_for_case(name, args.case_filter)
    return {
        "name": name,
        "group": args.group,
        "evidence_role": args.comparison_evidence_role,
        "case_kind": args.case_kind,
        "row_source": row_source_for_case(name, args.row_source),
        "point_type": point_types["point_type"],
        "source_point_type": point_types["source_point_type"],
        "target_point_type": point_types["target_point_type"],
        "scalar": "float",
        "size": size_for_case(name),
        "run_count": 1,
        "observed_ms": case.get("observed_ms"),
        "log_checksum": case.get("log_checksum"),
        "checksum_policy": "matrix_path_fingerprint_plus_rvv_gate_flag_correctness_checked_by_gtest",
        "asm_boundary": asm_summary.get("attribution_decision", "not_available"),
        "rvv_instr_count": rvv_instr_count_for_category(
            asm_summary, args.asm_category
        ),
        "notes": (
            "This comparison records one RVV QEMU smoke case. It intentionally omits "
            "ba_values/std_ms/rvv_ms so Evidence Doctor will not treat QEMU timing as "
            "target-hardware performance evidence."
        ),
    }


def build_manifest(rvv_log: Path,
                   asm_summary_path: Path,
                   args: argparse.Namespace) -> dict[str, Any]:
    rvv = parse_log(rvv_log)
    asm_summary = load_asm_summary(asm_summary_path)
    if not rvv["cases"]:
        raise SystemExit(f"no benchmark cases parsed from {rvv_log}")

    metadata = {
        "device": "QEMU rv64gcv smoke; not target hardware",
        "iterations": rvv["metadata"].get("iterations"),
        "warmup_iterations": rvv["metadata"].get("warmup_iterations"),
        "run_count": 1,
        "taskset": "not_applicable_qemu",
        "governor": "not_applicable_qemu",
        "freq": "not_applicable_qemu",
        "temperature": "not_recorded_qemu_smoke",
        "vlen": "256-bit (qemu -cpu rv64,v=true,vlen=256,elen=64)",
        "binary_hash": "not_recorded_qemu_smoke",
        "dataset": rvv["metadata"].get("dataset"),
        "build": rvv["metadata"].get("build"),
    }

    return {
        "schema_version": 1,
        "summary": {
            "title": args.title,
            "evidence_role": args.summary_evidence_role,
            "summary_path": str(rvv_log),
            "label": args.label,
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": metadata,
        },
        "checks": {
            "strict_ab": False,
        },
        "artifacts": {
            "rvv_log": str(rvv_log),
            "asm_summary": str(asm_summary_path),
            "asm_attribution_decision": asm_summary.get("attribution_decision", "not_available"),
            "candidate_key_mnemonics": asm_summary.get("candidate_key_mnemonics", {}),
            "production_public_key_mnemonics": asm_summary.get(
                "production_public_key_mnemonics", {}
            ),
        },
        "comparisons": [
            make_comparison(name, case, asm_summary, args)
            for name, case in sorted(rvv["cases"].items())
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rvv-log", default="log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log")
    parser.add_argument("--asm-summary", default="log/qemu/asm_attribution.json")
    parser.add_argument("--output", default="log/qemu/evidence_manifest.json")
    parser.add_argument("--title", default="transformation_estimation_2D QEMU smoke diagnostic")
    parser.add_argument("--label", default="QEMU smoke diagnostic; no performance conclusion")
    parser.add_argument("--summary-evidence-role", default="diagnostic")
    parser.add_argument("--comparison-evidence-role", default="diagnostic")
    parser.add_argument("--case-kind", default="qemu_smoke_only")
    parser.add_argument("--case-filter", default="ordered-cloud-pair-fused")
    parser.add_argument("--row-source", default="ordered_cloud_pair")
    parser.add_argument("--group", default="te2d_qemu_ordered_cloud_pair_smoke")
    parser.add_argument("--asm-category", default="candidate_lambda_boundary")
    args = parser.parse_args()

    manifest = build_manifest(Path(args.rvv_log), Path(args.asm_summary), args)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
