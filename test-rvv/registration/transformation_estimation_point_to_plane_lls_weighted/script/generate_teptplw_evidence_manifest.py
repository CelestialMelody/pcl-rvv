#!/usr/bin/env python3
"""
Generate an Evidence Doctor manifest for weighted TEPTPLW repeated board evidence.

This topic-local wrapper understands the weighted TEPTPLW repeated summary
formats for production-dispatch and source-indexed board evidence. It translates
those human-readable artifacts into the canonical evidence_manifest.json
consumed by `test-rvv/script/evidence_doctor.py`.

The wrapper is intentionally conservative: checksum and asm inputs currently
come from the production-default trace directory, while the repeated Std/RVV
summary may live under the production-dispatch or production-source-indices
directories. The manifest records those source paths instead of pretending all
evidence came from one raw run.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

SCRIPT_PATH = Path(__file__).resolve()
TOPIC_DIR = SCRIPT_PATH.parents[1]
REPO_ROOT = SCRIPT_PATH.parents[4]
DEFAULT_DISPATCH_DIR = TOPIC_DIR / "log/board/production_dispatch_fused_abcd_ilp"
DEFAULT_SOURCE_INDEXED_DIR = TOPIC_DIR / "log/board/production_source_indices_staged_gather"
DEFAULT_SOURCE_INDEXED_PROBE_DIR = (
    TOPIC_DIR / "log/board/production_source_indices_block_fused_abcd_ilp_probe"
)
DEFAULT_ROW_SOURCES_DIR = TOPIC_DIR / "log/board/run_board_bench_row_sources"
DEFAULT_SOURCE_INDEXED_FAMILY_DIR = TOPIC_DIR / "log/board/run_board_bench_source_indexed_family"
DEFAULT_SOURCE_INDEXED_FAMILY_REPEATED_DIR = TOPIC_DIR / "log/board/source_indexed_family_repeated"
DEFAULT_DUAL_CORRESPONDENCE_FAMILY_DIR = TOPIC_DIR / "log/board/run_board_bench_dual_correspondence_family"
DEFAULT_DEFAULT_DIR = TOPIC_DIR / "log/board/production_default_fused_abcd_ilp"
DIAGNOSTIC_KINDS = {
    "row-sources-diagnostic",
    "source-indexed-family-diagnostic",
    "source-indexed-family-repeated",
    "dual-correspondence-family-diagnostic",
}

SUMMARY_ROW_RE = re.compile(r"^\|\s*(weighted lls .+?)\s*\|")
VALUE_RE = re.compile(r"([0-9]+(?:\.[0-9]+)?)\s*x")
PRODUCTION_CASE_RE = re.compile(
    r"^weighted lls (?P<layer>production-dispatch) "
    r"(?P<row_source>full-cloud|source-indices) (?P<point_type>.+) (?P<size>\d+)$"
)
ROW_SOURCE_STD_RE = re.compile(
    r"^(?P<name>weighted lls row-sources "
    r"(?P<row_source>full-cloud|source-indices|dual-indices|correspondences) "
    r"(?P<point_type>.+?) (?P<size>\d+))\s*\|\s*Std\s*\|\s*"
    r"(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*(?P<total>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*"
    r"\[\s*(?P<speedup>[0-9]+(?:\.[0-9]+)?)x\s*\]"
)
ROW_SOURCE_RVV_RE = re.compile(
    r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*"
    r"(?P<total>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*\[\s*(?P<speedup>[0-9]+(?:\.[0-9]+)?)x\s*\]"
)
SOURCE_INDEXED_FAMILY_STD_RE = re.compile(
    r"^(?P<name>weighted lls (?:(?P<component>component)\s+)?source-indexed-family "
    r"(?P<variant>staged-gather|block-baseline|block-fused-abcd-ilp) "
    r"(?P<point_type>.+?)\s+(?:(?P<no_solve>no-solve)\s+)?(?P<size>\d+))\s*\|\s*Std\s*\|\s*"
    r"(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*(?P<total>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*"
    r"\[\s*(?P<speedup>[0-9]+(?:\.[0-9]+)?)x\s*\]"
)
SOURCE_INDEXED_FAMILY_SUMMARY_NAME_RE = re.compile(
    r"^(?P<name>weighted lls (?:(?P<component>component)\s+)?source-indexed-family "
    r"(?P<variant>staged-gather|block-baseline|block-fused-abcd-ilp) "
    r"(?P<point_type>.+?)\s+(?:(?P<no_solve>no-solve)\s+)?(?P<size>\d+))$"
)
DUAL_CORRESPONDENCE_FAMILY_STD_RE = re.compile(
    r"^(?P<name>weighted lls (?:(?P<component>component)\s+)?dual-correspondence-family "
    r"(?P<row_source>dual-indices|correspondences) "
    r"(?P<variant>staged-gather|block-baseline|block-fused-abcd-ilp) "
    r"(?P<point_type>.+?)\s+(?:(?P<no_solve>no-solve)\s+)?(?P<size>\d+))\s*\|\s*Std\s*\|\s*"
    r"(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*(?P<total>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|\s*"
    r"\[\s*(?P<speedup>[0-9]+(?:\.[0-9]+)?)x\s*\]"
)
ROW_SOURCE_CONTEXT_RE = re.compile(r"^\s*(Device|VLEN|Dataset|Iterations|Warmup)\s*:\s*(.+?)\s*$")
BENCH_HEADER_CONTEXT_RE = re.compile(
    r"^\s*(Dataset|Iterations|Warmup Iterations|Case filter|Build)\s*:\s*(.+?)\s*$"
)
BULLET_KV_RE = re.compile(r"^\s*-\s*([^:：]+)\s*[:：]\s*(.+?)\s*$")
ASM_ROW_RE = re.compile(r"^\|\s*`(?P<point>[^`]+)`\s*\|")
CHECKSUM_RE = re.compile(r"final_checksum=([0-9]+(?:\.[0-9]+)?)")
CHECKSUM_IDENTICAL_RE = re.compile(r"checksum sequences identical:\s*`?(yes|no)`?", re.IGNORECASE)
CHECKSUM_ROWS_RE = re.compile(r"all runs contain checksum rows:\s*`?(yes|no)`?", re.IGNORECASE)
ROW_SOURCE_POLICY_MAP = {
    "full-cloud": "full_cloud",
    "source-indices": "source_indexed",
    "dual-indices": "dual_indices",
    "correspondences": "correspondences",
}


def repo_path(path: Path) -> str:
    resolved = path.resolve()
    try:
        return str(resolved.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def int_or_none(value: Any) -> int | None:
    if value is None:
        return None
    if isinstance(value, int):
        return value
    try:
        return int(value.strip().strip("`"))
    except (AttributeError, ValueError):
        return None


def first_int_or_none(*values: Any) -> int | None:
    for value in values:
        parsed = int_or_none(value)
        if parsed is not None:
            return parsed
    return None


def clean_cell(value: str) -> str:
    return value.strip().strip("`").strip()


def parse_summary(path: Path) -> tuple[dict[str, str], list[dict[str, Any]]]:
    if not path.is_file():
        raise FileNotFoundError(f"summary not found: {path}")

    context: dict[str, str] = {}
    rows: list[dict[str, Any]] = []
    header: list[str] | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := BULLET_KV_RE.match(raw_line):
            context[match.group(1).strip().lower()] = match.group(2).strip().strip("`")
            continue
        line = raw_line.strip()
        if not line.startswith("|"):
            continue
        cells = [clean_cell(cell) for cell in line.strip("|").split("|")]
        lower = [cell.lower() for cell in cells]
        if lower and lower[0] == "benchmark item":
            header = lower
            continue
        if not header or not cells or set(cells[0]) <= {"-", ":", " "}:
            continue
        if not SUMMARY_ROW_RE.match(line):
            continue
        item = cells[0]
        values: list[float] = []
        if "values" in header and header.index("values") < len(cells):
            values = [float(value) for value in VALUE_RE.findall(cells[header.index("values")])]
        run_count = None
        if "runs" in header and header.index("runs") < len(cells):
            run_count = int_or_none(cells[header.index("runs")])
        rows.append({"name": item, "run_count": run_count, "ba_values": values})
    if not rows:
        raise ValueError(f"no benchmark rows parsed from {path}")
    return context, rows


def parse_row_source_diagnostic(path: Path) -> tuple[dict[str, str], list[dict[str, Any]]]:
    if not path.is_file():
        raise FileNotFoundError(f"row-source diagnostic log not found: {path}")

    context: dict[str, str] = {}
    rows: list[dict[str, Any]] = []
    pending: dict[str, Any] | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := ROW_SOURCE_CONTEXT_RE.match(raw_line):
            key = match.group(1).strip().lower().replace(" ", "_")
            value = match.group(2).strip()
            if key == "iterations":
                first_number = re.search(r"\d+", value)
                context[key] = first_number.group(0) if first_number else value
            else:
                context[key] = value
            continue
        if match := ROW_SOURCE_STD_RE.match(raw_line):
            pending = {
                "name": match.group("name"),
                "row_source": match.group("row_source"),
                "point_type": match.group("point_type"),
                "size": int(match.group("size")),
                "std_avg_ms": float(match.group("avg")),
                "std_total_ms": float(match.group("total")),
                "std_speedup": float(match.group("speedup")),
            }
            continue
        if pending and (match := ROW_SOURCE_RVV_RE.match(raw_line)):
            row = dict(pending)
            row.update(
                {
                    "rvv_avg_ms": float(match.group("avg")),
                    "rvv_total_ms": float(match.group("total")),
                    "rvv_speedup": float(match.group("speedup")),
                    "ba_values": [float(pending["std_avg_ms"]) / float(match.group("avg"))],
                }
            )
            rows.append(row)
            pending = None
    if not rows:
        raise ValueError(f"no row-source rows parsed from {path}")
    return context, rows


def parse_source_indexed_family_diagnostic(path: Path) -> tuple[dict[str, str], list[dict[str, Any]]]:
    if not path.is_file():
        raise FileNotFoundError(f"source-indexed family diagnostic log not found: {path}")

    context: dict[str, str] = {}
    rows: list[dict[str, Any]] = []
    pending: dict[str, Any] | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := ROW_SOURCE_CONTEXT_RE.match(raw_line):
            key = match.group(1).strip().lower().replace(" ", "_")
            value = match.group(2).strip()
            if key == "iterations":
                first_number = re.search(r"\d+", value)
                context[key] = first_number.group(0) if first_number else value
            else:
                context[key] = value
            continue
        if match := SOURCE_INDEXED_FAMILY_STD_RE.match(raw_line):
            pending = {
                "name": match.group("name"),
                "variant": match.group("variant"),
                "point_type": match.group("point_type"),
                "size": int(match.group("size")),
                "component": bool(match.group("component")),
                "no_solve": bool(match.group("no_solve")),
                "std_avg_ms": float(match.group("avg")),
                "std_total_ms": float(match.group("total")),
                "std_speedup": float(match.group("speedup")),
            }
            continue
        if pending and (match := ROW_SOURCE_RVV_RE.match(raw_line)):
            row = dict(pending)
            row.update(
                {
                    "rvv_avg_ms": float(match.group("avg")),
                    "rvv_total_ms": float(match.group("total")),
                    "rvv_speedup": float(match.group("speedup")),
                    "ba_values": [float(pending["std_avg_ms"]) / float(match.group("avg"))],
                }
            )
            rows.append(row)
            pending = None
    if not rows:
        raise ValueError(f"no source-indexed family rows parsed from {path}")
    return context, rows


def parse_source_indexed_family_repeated_summary(path: Path) -> tuple[dict[str, str], list[dict[str, Any]]]:
    context, rows = parse_summary(path)
    parsed: list[dict[str, Any]] = []
    for row in rows:
        match = SOURCE_INDEXED_FAMILY_SUMMARY_NAME_RE.match(str(row["name"]))
        if not match:
            continue
        parsed.append(
            {
                **row,
                "name": match.group("name"),
                "variant": match.group("variant"),
                "point_type": match.group("point_type"),
                "size": int(match.group("size")),
                "component": bool(match.group("component")),
                "no_solve": bool(match.group("no_solve")),
            }
        )
    if not parsed:
        raise ValueError(f"no source-indexed family repeated rows parsed from {path}")
    return context, parsed


def parse_dual_correspondence_family_diagnostic(path: Path) -> tuple[dict[str, str], list[dict[str, Any]]]:
    if not path.is_file():
        raise FileNotFoundError(f"dual/correspondence family diagnostic log not found: {path}")

    context: dict[str, str] = {}
    rows: list[dict[str, Any]] = []
    pending: dict[str, Any] | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := ROW_SOURCE_CONTEXT_RE.match(raw_line):
            key = match.group(1).strip().lower().replace(" ", "_")
            value = match.group(2).strip()
            if key == "iterations":
                first_number = re.search(r"\d+", value)
                context[key] = first_number.group(0) if first_number else value
            else:
                context[key] = value
            continue
        if match := DUAL_CORRESPONDENCE_FAMILY_STD_RE.match(raw_line):
            pending = {
                "name": match.group("name"),
                "row_source": match.group("row_source"),
                "variant": match.group("variant"),
                "point_type": match.group("point_type"),
                "size": int(match.group("size")),
                "component": bool(match.group("component")),
                "no_solve": bool(match.group("no_solve")),
                "std_avg_ms": float(match.group("avg")),
                "std_total_ms": float(match.group("total")),
                "std_speedup": float(match.group("speedup")),
            }
            continue
        if pending and (match := ROW_SOURCE_RVV_RE.match(raw_line)):
            row = dict(pending)
            row.update(
                {
                    "rvv_avg_ms": float(match.group("avg")),
                    "rvv_total_ms": float(match.group("total")),
                    "rvv_speedup": float(match.group("speedup")),
                    "ba_values": [float(pending["std_avg_ms"]) / float(match.group("avg"))],
                }
            )
            rows.append(row)
            pending = None
    if not rows:
        raise ValueError(f"no dual/correspondence family rows parsed from {path}")
    return context, rows


def parse_asm_attribution(path: Path) -> dict[str, dict[str, Any]]:
    if not path.is_file():
        return {}
    parsed: dict[str, dict[str, Any]] = {}
    in_symbol_table = False
    header: list[str] | None = None
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if line == "## 符号边界":
            in_symbol_table = True
            continue
        if in_symbol_table and line.startswith("## "):
            break
        if not in_symbol_table or not line.startswith("|"):
            continue
        cells = [clean_cell(cell) for cell in line.strip("|").split("|")]
        if cells and cells[0] == "point type":
            header = cells
            continue
        if not header or not cells or set(cells[0]) <= {"-", ":", " "}:
            continue
        row = dict(zip(header, cells))
        point = row.get("point type", "")
        if not point:
            continue
        parsed[point] = {
            "asm_boundary": row.get("boundary") or None,
            "rvv_instr_count": int_or_none(row.get("rvv total", "")),
            "asm_symbol_addr": row.get("addr") or None,
        }
    return parsed


def parse_checksum_validation(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {}
    text = path.read_text(encoding="utf-8", errors="replace")
    final_values = CHECKSUM_RE.findall(text)
    final_checksum = final_values[-1] if final_values else None
    identical = None
    if match := CHECKSUM_IDENTICAL_RE.search(text):
        identical = match.group(1).lower() == "yes"
    rows_present = None
    if match := CHECKSUM_ROWS_RE.search(text):
        rows_present = match.group(1).lower() == "yes"
    return {
        "final_checksum": final_checksum,
        "checksum_sequences_identical": identical,
        "all_runs_contain_checksum_rows": rows_present,
    }


def parse_board_env(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    lines = [line.strip() for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip()]
    result: dict[str, str] = {}
    if len(lines) >= 2:
        result["device"] = "Milkv-Jupiter" if "milkv-jupiter" in lines[1].lower() else lines[1]
    if len(lines) >= 3:
        result["governor"] = lines[2]
    freqs = [line for line in lines[3:] if line.isdigit()]
    if freqs:
        result["freq"] = ",".join(freqs)
    temps = [line for line in lines[3:] if line.startswith("thermal_zone")]
    if temps:
        result["temperature"] = ", ".join(temps)
    return result


def parse_collection_manifest(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {}
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    result: dict[str, Any] = {}
    for key in ("runs", "iterations", "warmup_iterations", "size", "case_filter", "cpu"):
        if key in data:
            result[key] = data[key]
    if isinstance(data.get("git"), dict):
        result["git_commit"] = data["git"].get("commit")
        result["git_dirty"] = data["git"].get("dirty")
    return result


def parse_bench_header_context(log_dir: Path) -> dict[str, str]:
    """从 run_bench_std.log / run_bench_rvv.log 兜底读取 bench header。

    历史 analyze_bench_compare.log 没有把 Warmup Iterations 写进 context。Evidence
    Doctor 需要这个字段来区分 no-warmup smoke 和正式 bench，所以 wrapper 在同目录
    的原始 Std/RVV 日志里补读一次。
    """
    result: dict[str, str] = {}
    for name in ("run_bench_std.log", "run_bench_rvv.log"):
        path = log_dir / name
        if not path.is_file():
            continue
        for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            match = BENCH_HEADER_CONTEXT_RE.match(raw_line)
            if not match:
                continue
            key = match.group(1).strip().lower().replace(" ", "_")
            value = match.group(2).strip()
            result.setdefault(key, value)
    return result


def comparison_from_row(
    row: dict[str, Any],
    asm: dict[str, dict[str, Any]],
    checksum: dict[str, Any],
    *,
    profile: str,
) -> dict[str, Any]:
    match = PRODUCTION_CASE_RE.match(row["name"])
    if not match:
        return {
            "name": row["name"],
            "evidence_role": "summary_only_unknown",
            "ba_values": row.get("ba_values", []),
            "metadata_incomplete": True,
        }

    point_type = match.group("point_type")
    size = int(match.group("size"))
    row_source = ROW_SOURCE_POLICY_MAP.get(match.group("row_source"), match.group("row_source").replace("-", "_"))
    asm_info = asm.get(point_type, {})
    if profile in {
        "production-source-indices",
        "production-source-indices-block-fused-probe",
    }:
        boundary = "production_dispatch_public_source_indexed"
        wrapper = "public_source_indexed_overload"
        gate = "production_source_indexed_dispatch"
        group = "production_dispatch_source_indexed"
        if profile == "production-source-indices-block-fused-probe":
            title_tag = "production-source-indices-block-fused-abcd-ilp-probe"
            implementation_family = "block_fused_abcd_ilp"
        else:
            title_tag = "production-source-indices"
            implementation_family = "staged_gather_compressed_tail"
        checksum_policy = "source_indexed_board_speedup_summary"
    else:
        boundary = "production_dispatch_public_full_cloud"
        wrapper = "public_full_cloud_overload"
        gate = "production_full_cloud_dispatch"
        group = "production_dispatch_full_cloud"
        title_tag = "production-dispatch"
        implementation_family = "block_fused_abcd_ilp"
        checksum_policy = "rvv_trace_final_checksum_supplemental"
    candidate_asm_boundary = asm_info.get("asm_boundary")
    candidate_rvv_instr_count = asm_info.get("rvv_instr_count")
    candidate_checksum = checksum.get("final_checksum")
    notes = [
        "ba_values are Std/RVV repeated board speedups from the topic-local summary.",
        "checksum and asm attribution are supplemental production-default RVV evidence from separate summary artifacts.",
    ]
    if profile in {
        "production-source-indices",
        "production-source-indices-block-fused-probe",
    }:
        candidate_asm_boundary = None
        candidate_rvv_instr_count = None
        candidate_checksum = None
        notes = [
            "ba_values are Std/RVV repeated board speedups from the source-indexed summary.",
            "source-indexed-specific asm attribution is not recorded in this manifest; production-default checksum / asm paths remain supplemental source paths only.",
        ]
        if profile == "production-source-indices-block-fused-probe":
            notes.insert(
                1,
                "The RVV candidate is the Phase 031 source-indexed block-fused-abcd-ilp production probe.",
            )
    common = {
        "boundary": boundary,
        "wrapper": wrapper,
        "row_source": row_source,
        "formula_mode": "production_weighted_lls",
        "solve": True,
        "checksum_policy": checksum_policy,
        "timer_boundary": "estimate_call_includes_solve",
        "gate": gate,
        "mask": "production_finite_mask",
        "reduction": "production_weighted_lls_semantics",
    }
    baseline = {
        "label": f"std {title_tag}",
        "build": "std",
        **common,
        "implementation_family": "scalar_std",
        "asm_boundary": "std_non_rvv_path_not_attributed",
        "rvv_instr_count": 0,
    }
    candidate = {
        "label": f"rvv {title_tag}",
        "build": "rvv",
        **common,
        "implementation_family": implementation_family,
        "asm_boundary": candidate_asm_boundary,
        "rvv_instr_count": candidate_rvv_instr_count,
        "checksum": candidate_checksum,
    }
    return {
        "name": row["name"],
        "case_kind": "production_direct",
        "evidence_role": "production_direct",
        "build_comparison": "std_vs_rvv",
        "point_type": point_type,
        "size": size,
        "group": group,
        "run_count": row.get("run_count"),
        "ba_values": row.get("ba_values", []),
        "baseline": baseline,
        "candidate": candidate,
        "notes": notes,
    }


def row_source_diagnostic_from_row(row: dict[str, Any]) -> dict[str, Any]:
    row_source = ROW_SOURCE_POLICY_MAP.get(row["row_source"], str(row["row_source"]).replace("-", "_"))
    observed_speedup = float(row["std_avg_ms"]) / float(row["rvv_avg_ms"])
    common = {
        "boundary": "row_source_diagnostic_harness",
        "wrapper": "run_bench_row_sources",
        "row_source": row_source,
        "formula_mode": "diagnostic_weighted_lls",
        "solve": True,
        "checksum_policy": "not_recorded_for_single_run_diagnostic",
        "timer_boundary": "bench_compare_avg_includes_solve",
        "gate": "row_source_diagnostic_candidate",
        "mask": "production_finite_mask",
        "reduction": "diagnostic_row_source_semantics",
    }
    return {
        "name": row["name"],
        "case_kind": "component_ablation",
        "evidence_role": "diagnostic",
        "build_comparison": "std_vs_rvv",
        "point_type": row["point_type"],
        "size": row["size"],
        "group": f"row_source_diagnostic_{row_source}",
        "run_count": 1,
        "observed_speedup": observed_speedup,
        "std_ms": row["std_avg_ms"],
        "rvv_ms": row["rvv_avg_ms"],
        "baseline": {
            "label": "std row-source diagnostic",
            "build": "std",
            **common,
            "asm_boundary": "std_non_rvv_path_not_attributed",
            "rvv_instr_count": 0,
            "std_ms": row["std_avg_ms"],
        },
        "candidate": {
            "label": "rvv row-source diagnostic",
            "build": "rvv",
            **common,
            "asm_boundary": None,
            "rvv_instr_count": None,
            "rvv_ms": row["rvv_avg_ms"],
        },
        "notes": [
            "Single-run board diagnostic comparison used to decide whether a row-source family should enter the production integration loop.",
            f"Observed speedup={observed_speedup:.3f}x from the compare log.",
            "Negative dual-indices / correspondences values are diagnostic evidence and should not be relabeled as production direct.",
        ],
    }


def source_indexed_family_diagnostic_from_row(
    row: dict[str, Any], *, full_checksum_policy: str
) -> dict[str, Any]:
    variant = str(row["variant"]).replace("-", "_")
    has_solve = not bool(row.get("no_solve"))
    observed_speedup = float(row["std_avg_ms"]) / float(row["rvv_avg_ms"])
    reduction = {
        "staged_gather": "staged_gather_compressed_tail",
        "block_baseline": "block_reduction_abcn",
        "block_fused_abcd_ilp": "block_reduction_abcn_fused_abcd_ilp",
    }.get(variant, variant)
    common = {
        "boundary": "source_indexed_family_diagnostic_harness",
        "wrapper": "run_bench_source_indexed_family",
        "row_source": "source_indexed",
        "formula_mode": variant,
        "solve": has_solve,
        "checksum_policy": full_checksum_policy if has_solve else "normal_equation_checksum",
        "timer_boundary": "diagnostic_estimate_includes_solve" if has_solve else "component_no_solve",
        "gate": "test_rvv_source_indexed_family_candidate",
        "mask": "production_finite_mask_weight_not_masked",
        "reduction": reduction,
    }
    return {
        "name": row["name"],
        "case_kind": "implementation_family_comparison" if has_solve else "component_ablation",
        "evidence_role": "diagnostic",
        "build_comparison": "std_vs_rvv",
        "point_type": row["point_type"],
        "size": row["size"],
        "group": f"source_indexed_family_{variant}",
        "run_count": 1,
        "observed_speedup": observed_speedup,
        "std_ms": row["std_avg_ms"],
        "rvv_ms": row["rvv_avg_ms"],
        "ba_values": row.get("ba_values", []),
        "baseline": {
            "label": "std source-indexed-family diagnostic",
            "build": "std",
            **common,
            "asm_boundary": "std_non_rvv_path_not_attributed",
            "rvv_instr_count": 0,
            "std_ms": row["std_avg_ms"],
        },
        "candidate": {
            "label": "rvv source-indexed-family diagnostic",
            "build": "rvv",
            **common,
            "asm_boundary": None,
            "rvv_instr_count": None,
            "rvv_ms": row["rvv_avg_ms"],
        },
        "notes": [
            "Single-run board diagnostic comparison for source-indexed implementation-family carry-over.",
            f"Observed speedup={observed_speedup:.3f}x from the compare log.",
            "This is pre-production diagnostic evidence; it does not replace production source-indexed repeated-board evidence.",
        ],
    }


def source_indexed_family_repeated_from_row(
    row: dict[str, Any], *, full_checksum_policy: str
) -> dict[str, Any]:
    variant = str(row["variant"]).replace("-", "_")
    has_solve = not bool(row.get("no_solve"))
    reduction = {
        "staged_gather": "staged_gather_compressed_tail",
        "block_baseline": "block_reduction_abcn",
        "block_fused_abcd_ilp": "block_reduction_abcn_fused_abcd_ilp",
    }.get(variant, variant)
    common = {
        "boundary": "source_indexed_family_repeated_board_diagnostic",
        "wrapper": "collect_board_source_indexed_family_repeated",
        "row_source": "source_indexed",
        "formula_mode": variant,
        "solve": has_solve,
        "checksum_policy": full_checksum_policy if has_solve else "normal_equation_checksum",
        "timer_boundary": "repeated_diagnostic_estimate_includes_solve" if has_solve else "repeated_component_no_solve",
        "gate": "test_rvv_source_indexed_family_candidate",
        "mask": "production_finite_mask_weight_not_masked",
        "reduction": reduction,
    }
    return {
        "name": row["name"],
        "case_kind": "implementation_family_comparison" if has_solve else "component_ablation",
        "evidence_role": "diagnostic",
        "build_comparison": "std_vs_rvv",
        "point_type": row["point_type"],
        "size": row["size"],
        "group": f"source_indexed_family_repeated_{variant}",
        "run_count": row.get("run_count"),
        "ba_values": row.get("ba_values", []),
        "baseline": {
            "label": "std source-indexed-family repeated diagnostic",
            "build": "std",
            **common,
            "asm_boundary": "std_non_rvv_path_not_attributed",
            "rvv_instr_count": 0,
        },
        "candidate": {
            "label": "rvv source-indexed-family repeated diagnostic",
            "build": "rvv",
            **common,
            "asm_boundary": None,
            "rvv_instr_count": None,
        },
        "notes": [
            "Repeated board summary for source-indexed implementation-family carry-over.",
            "This is still pre-production diagnostic evidence; it does not prove production dispatch.",
            "Summary ba_values preserve run-to-run direction, but per-run Std/RVV ms and component/full delta checks require retained raw compare logs.",
        ],
    }


def dual_correspondence_family_diagnostic_from_row(row: dict[str, Any]) -> dict[str, Any]:
    row_source = ROW_SOURCE_POLICY_MAP.get(row["row_source"], str(row["row_source"]).replace("-", "_"))
    variant = str(row["variant"]).replace("-", "_")
    has_solve = not bool(row.get("no_solve"))
    observed_speedup = float(row["std_avg_ms"]) / float(row["rvv_avg_ms"])
    reduction = {
        "staged_gather": "staged_gather_compressed_tail",
        "block_baseline": "block_reduction_abcn",
        "block_fused_abcd_ilp": "block_reduction_abcn_fused_abcd_ilp",
    }.get(variant, variant)
    common = {
        "boundary": "dual_correspondence_family_diagnostic_harness",
        "wrapper": "run_bench_dual_correspondence_family",
        "row_source": row_source,
        "formula_mode": variant,
        "solve": has_solve,
        "checksum_policy": "matrix_checksum_with_accepted_points" if has_solve else "normal_equation_checksum",
        "timer_boundary": "diagnostic_estimate_includes_solve" if has_solve else "component_no_solve",
        "gate": "test_rvv_dual_correspondence_family_candidate",
        "mask": "production_finite_mask_weight_not_masked",
        "reduction": reduction,
    }
    return {
        "name": row["name"],
        "case_kind": "implementation_family_comparison" if has_solve else "component_ablation",
        "evidence_role": "diagnostic",
        "build_comparison": "std_vs_rvv",
        "point_type": row["point_type"],
        "size": row["size"],
        "group": f"dual_correspondence_family_{row_source}_{variant}",
        "run_count": 1,
        "observed_speedup": observed_speedup,
        "std_ms": row["std_avg_ms"],
        "rvv_ms": row["rvv_avg_ms"],
        "ba_values": row.get("ba_values", []),
        "baseline": {
            "label": "std dual-correspondence-family diagnostic",
            "build": "std",
            **common,
            "asm_boundary": "std_non_rvv_path_not_attributed",
            "rvv_instr_count": 0,
            "std_ms": row["std_avg_ms"],
        },
        "candidate": {
            "label": "rvv dual-correspondence-family diagnostic",
            "build": "rvv",
            **common,
            "asm_boundary": None,
            "rvv_instr_count": None,
            "rvv_ms": row["rvv_avg_ms"],
        },
        "notes": [
            "Single-run board diagnostic comparison for dual-indices / correspondences implementation-family carry-over.",
            f"Observed speedup={observed_speedup:.3f}x from the compare log.",
            "This is pre-production diagnostic evidence; it does not prove production dispatch or production performance.",
            "Correspondences cases include query/match/weight expansion in the diagnostic boundary.",
        ],
    }


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
    if args.kind == "row-sources-diagnostic":
        summary_context, rows = parse_row_source_diagnostic(args.summary)
        summary_context = {**parse_bench_header_context(args.summary.parent), **summary_context}
        asm = {}
        checksum = {}
        board_env = {}
        collection = {}
        title = "TEPTPLW row-source diagnostic evidence manifest"
        summary_boundary = (
            "Single-run row-source compare log is converted into a diagnostic manifest. "
            "This boundary is intentionally pre-production; it is used to decide whether "
            "full-cloud / source-indexed / dual-indices / correspondences need further "
            "candidate, test, bench, or board work."
        )
    elif args.kind == "source-indexed-family-diagnostic":
        summary_context, rows = parse_source_indexed_family_diagnostic(args.summary)
        summary_context = {**parse_bench_header_context(args.summary.parent), **summary_context}
        asm = {}
        checksum = {}
        board_env = {}
        collection = {}
        title = "TEPTPLW source-indexed implementation-family diagnostic manifest"
        summary_boundary = (
            "Single-run source-indexed-family compare log is converted into a diagnostic "
            "manifest. It compares staged-gather, block-baseline and block-fused-abcd-ilp "
            "test-rvv candidates under the source-indexed row source. This boundary is "
            "pre-production and cannot be used as production direct evidence."
        )
    elif args.kind == "source-indexed-family-repeated":
        summary_context, rows = parse_source_indexed_family_repeated_summary(args.summary)
        asm = {}
        checksum = {}
        board_env = {}
        collection = {}
        title = "TEPTPLW source-indexed implementation-family repeated board diagnostic manifest"
        summary_boundary = (
            "Repeated source-indexed-family summary is converted into a diagnostic "
            "manifest. It compares staged-gather, block-baseline and block-fused-abcd-ilp "
            "test-rvv candidates under the source-indexed row source. It is pre-production "
            "diagnostic evidence and cannot be used as production direct evidence."
        )
    elif args.kind == "dual-correspondence-family-diagnostic":
        summary_context, rows = parse_dual_correspondence_family_diagnostic(args.summary)
        summary_context = {**parse_bench_header_context(args.summary.parent), **summary_context}
        asm = {}
        checksum = {}
        board_env = {}
        collection = {}
        title = "TEPTPLW dual-indices / correspondences implementation-family diagnostic manifest"
        summary_boundary = (
            "Single-run dual-correspondence-family compare log is converted into a diagnostic "
            "manifest. It compares staged-gather, block-baseline and block-fused-abcd-ilp "
            "test-rvv candidates under dual-indices and correspondences row sources. "
            "This boundary is pre-production and cannot be used as production direct evidence."
        )
    else:
        summary_context, rows = parse_summary(args.summary)
        asm = parse_asm_attribution(args.asm_attribution)
        checksum = parse_checksum_validation(args.checksum_validation)
        board_env = parse_board_env(args.board_env)
        collection = (
            {}
            if args.kind == "production-source-indices-block-fused-probe"
            else parse_collection_manifest(args.collection_manifest)
        )

        if args.kind == "production-source-indices":
            title = "TEPTPLW source-indexed repeated board evidence manifest"
            summary_boundary = (
                "Repeated source-indexed speedup summary is recorded as production direct, "
                "but source-indexed-specific asm attribution is still missing. The manifest "
                "keeps the supplemental production-default RVV checksum / asm source paths "
                "for traceability only and is a wrapper smoke for Evidence Doctor, not a "
                "replacement for raw logs."
            )
        elif args.kind == "production-source-indices-block-fused-probe":
            title = "TEPTPLW source-indexed block-fused-abcd-ilp production probe manifest"
            summary_boundary = (
                "Phase 031 records repeated Std/RVV timing from the real source-indexed "
                "public dispatch after routing the RVV path to the block-fused-abcd-ilp "
                "probe. This is production direct evidence for the bounded probe, not a "
                "generic source-indexed adoption claim. Source-indexed-specific asm "
                "attribution and binary identity remain incomplete."
            )
        else:
            title = "TEPTPLW production-dispatch repeated board evidence manifest"
            summary_boundary = (
                "Repeated production-dispatch speedup summary is combined with supplemental "
                "production-default RVV checksum and asm summaries; this manifest is a "
                "wrapper smoke for Evidence Doctor, not a replacement for raw logs."
            )

    iterations = first_int_or_none(summary_context.get("iterations", ""), collection.get("iterations"))
    warmup_iterations = first_int_or_none(
        summary_context.get("warm-up iterations", ""),
        summary_context.get("warmup_iterations", ""),
        summary_context.get("warmup", ""),
        collection.get("warmup_iterations"),
    )
    run_count = first_int_or_none(summary_context.get("runs", ""), collection.get("runs"))
    if args.kind in DIAGNOSTIC_KINDS and run_count is None:
        run_count = 1
    taskset = f"taskset -c {collection['cpu']}" if "cpu" in collection else None

    metadata = {
        "device": board_env.get("device") or summary_context.get("device") or "Milkv-Jupiter",
        "iterations": iterations,
        "warmup_iterations": warmup_iterations,
        "run_count": run_count,
        "taskset": taskset,
        "governor": board_env.get("governor"),
        "freq": board_env.get("freq"),
        "temperature": board_env.get("temperature"),
        "vlen": "see SoC / ELF (board)",
        "git_commit": collection.get("git_commit"),
        "git_dirty": collection.get("git_dirty"),
    }
    metadata = {key: value for key, value in metadata.items() if value is not None}

    return {
        "schema_version": 1,
        "summary": {
            "title": title,
            "evidence_role": "diagnostic" if args.kind in DIAGNOSTIC_KINDS else "production_direct",
            "summary_path": repo_path(args.summary),
            "metadata": metadata,
            **(
                {
                    "probe": {
                        "name": "source_indexed_block_fused_abcd_ilp",
                        "phase": "031-source-indexed-fused-production-probe",
                        "collection_profile": summary_context.get("collection profile"),
                    }
                }
                if args.kind == "production-source-indices-block-fused-probe"
                else {}
            ),
            "source_paths": {
                "summary": repo_path(args.summary),
                **(
                    {
                        "checksum_validation": repo_path(args.checksum_validation),
                        "asm_attribution": repo_path(args.asm_attribution),
                        "board_env": repo_path(args.board_env),
                    }
                    if args.kind == "production-source-indices-block-fused-probe"
                    else {}
                ),
                **(
                    {
                        "checksum_validation": repo_path(args.checksum_validation),
                        "asm_attribution": repo_path(args.asm_attribution),
                        "board_env": repo_path(args.board_env),
                        "collection_manifest": repo_path(args.collection_manifest),
                    }
                    if args.kind not in DIAGNOSTIC_KINDS
                    and args.kind != "production-source-indices-block-fused-probe"
                    else {}
                ),
                **(
                    {
                        "raw_board_compare_log": repo_path(args.summary),
                    }
                    if args.kind in DIAGNOSTIC_KINDS
                    else {}
                ),
            },
            "source_boundary_note": summary_boundary,
        },
        "checks": {
            "strict_ab": args.kind not in DIAGNOSTIC_KINDS,
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
        "comparisons": [
            (
                row_source_diagnostic_from_row(row)
                if args.kind == "row-sources-diagnostic"
                else source_indexed_family_diagnostic_from_row(
                    row,
                    full_checksum_policy=args.source_indexed_family_full_checksum_policy,
                )
                if args.kind == "source-indexed-family-diagnostic"
                else source_indexed_family_repeated_from_row(
                    row,
                    full_checksum_policy=args.source_indexed_family_full_checksum_policy,
                )
                if args.kind == "source-indexed-family-repeated"
                else dual_correspondence_family_diagnostic_from_row(row)
                if args.kind == "dual-correspondence-family-diagnostic"
                else comparison_from_row(row, asm, checksum, profile=args.kind)
            )
            for row in rows
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate TEPTPLW Evidence Doctor manifest.")
    parser.add_argument(
        "--kind",
        choices=(
            "production-dispatch",
            "production-source-indices",
            "production-source-indices-block-fused-probe",
            "row-sources-diagnostic",
            "source-indexed-family-diagnostic",
            "source-indexed-family-repeated",
            "dual-correspondence-family-diagnostic",
        ),
        default="production-dispatch",
    )
    parser.add_argument("--summary", type=Path, default=None)
    parser.add_argument("--asm-attribution", type=Path, default=None)
    parser.add_argument("--checksum-validation", type=Path, default=None)
    parser.add_argument("--board-env", type=Path, default=None)
    parser.add_argument("--collection-manifest", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--source-indexed-family-full-checksum-policy",
        choices=("normal_equation_plus_matrix_checksum", "matrix_checksum_with_accepted_points"),
        default="normal_equation_plus_matrix_checksum",
        help=(
            "source-indexed-family full-estimate sink policy. Use "
            "matrix_checksum_with_accepted_points only for historical logs produced "
            "before Phase 030 sink alignment."
        ),
    )
    args = parser.parse_args()

    if args.summary is None:
        if args.kind == "production-source-indices":
            args.summary = DEFAULT_SOURCE_INDEXED_DIR / "summary.md"
        elif args.kind == "production-source-indices-block-fused-probe":
            args.summary = DEFAULT_SOURCE_INDEXED_PROBE_DIR / "summary.md"
        elif args.kind == "row-sources-diagnostic":
            args.summary = DEFAULT_ROW_SOURCES_DIR / "analyze_bench_compare.log"
        elif args.kind == "source-indexed-family-diagnostic":
            args.summary = DEFAULT_SOURCE_INDEXED_FAMILY_DIR / "analyze_bench_compare.log"
        elif args.kind == "source-indexed-family-repeated":
            args.summary = DEFAULT_SOURCE_INDEXED_FAMILY_REPEATED_DIR / "summary.md"
        elif args.kind == "dual-correspondence-family-diagnostic":
            args.summary = DEFAULT_DUAL_CORRESPONDENCE_FAMILY_DIR / "analyze_bench_compare.log"
        else:
            args.summary = DEFAULT_DISPATCH_DIR / "summary.md"
    if args.kind not in DIAGNOSTIC_KINDS:
        if args.asm_attribution is None:
            args.asm_attribution = DEFAULT_DEFAULT_DIR / "asm_production_symbol_attribution.md"
        if args.checksum_validation is None:
            args.checksum_validation = DEFAULT_DEFAULT_DIR / "checksum_validation.md"
        if args.board_env is None:
            args.board_env = DEFAULT_DEFAULT_DIR / "board_env_before.log"
        if args.collection_manifest is None:
            args.collection_manifest = DEFAULT_DEFAULT_DIR / "collection_manifest.json"
    if args.output is None:
        if args.kind == "production-source-indices":
            args.output = DEFAULT_SOURCE_INDEXED_DIR / "evidence_manifest.json"
        elif args.kind == "production-source-indices-block-fused-probe":
            args.output = DEFAULT_SOURCE_INDEXED_PROBE_DIR / "evidence_manifest.json"
        elif args.kind == "row-sources-diagnostic":
            args.output = DEFAULT_ROW_SOURCES_DIR / "evidence_manifest.json"
        elif args.kind == "source-indexed-family-diagnostic":
            args.output = DEFAULT_SOURCE_INDEXED_FAMILY_DIR / "evidence_manifest.json"
        elif args.kind == "source-indexed-family-repeated":
            args.output = DEFAULT_SOURCE_INDEXED_FAMILY_REPEATED_DIR / "evidence_manifest.json"
        elif args.kind == "dual-correspondence-family-diagnostic":
            args.output = DEFAULT_DUAL_CORRESPONDENCE_FAMILY_DIR / "evidence_manifest.json"
        else:
            args.output = DEFAULT_DISPATCH_DIR / "evidence_manifest.json"

    manifest = build_manifest(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[done] evidence manifest: {repo_path(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
