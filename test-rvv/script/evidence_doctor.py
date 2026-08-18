#!/usr/bin/env python3
"""
RVV Evidence Doctor（证据体检）

这个脚本检查 RVV benchmark / board summary / checksum / asm attribution 等证据摘要中
是否存在需要 worker 停下来解释的信号。它不替代 reviewer，也不自动证明实现有 bug；它把
可疑数据模式输出为 Errors / Warnings / Suggestions，供 summary、evaluation 和 Handoff
Packet 引用。

推荐输入是 JSON manifest（证据清单），因为只有 manifest 才能表达 A/B 边界、row source、
checksum、asm boundary 和环境信息。也可以用 --summary-md 对现有 markdown 摘要做轻量检查；
该模式只能发现数值层面的异常，并会提示 metadata 不足。
"""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1

STRICT_ROLES = {
    "strict_ab",
    "strict-a-b",
    "strict_a_b",
    "production_direct",
    "production-direct",
    "production_ready",
    "production-ready",
}
DIAGNOSTIC_ROLES = {
    "diagnostic",
    "production_shaped_diagnostic",
    "production-shaped-diagnostic",
    "component_ablation",
    "component-ablation",
    "mixed_boundary_cross_check",
    "mixed-boundary-cross-check",
    "summary_only_unknown",
}
DEFAULT_EQUAL_FIELDS = [
    "boundary",
    "wrapper",
    "row_source",
    "solve",
    "checksum_policy",
    "timer_boundary",
    "gate",
    "mask",
    "reduction",
]
DEFAULT_REQUIRED_SUMMARY_FIELDS = [
    "iterations",
    "warmup_iterations",
    "run_count",
]
DEFAULT_ENV_FIELDS = [
    "device",
    "taskset",
    "governor",
    "freq",
    "temperature",
]
# Evidence manifest 的规范字段仍以 evidence-doctor.zh.md 为准。这里的 alias 只为
# 兼容旧 summary / topic-local wrapper 的常见命名漂移；使用 alias 时脚本会给出
# noncanonical_field_alias suggestion，提醒后续把字段归一化为规范名。
FIELD_ALIASES = {
    "boundary": ["entry_boundary", "implementation_boundary", "boundary_kind"],
    "wrapper": ["bench_wrapper", "wrapper_name", "entry_wrapper"],
    "row_source": ["rowSource", "row_source_policy", "rowSourcePolicy", "row_policy"],
    "formula_mode": ["formula", "formulaMode", "formula_policy"],
    "solve": ["include_solve", "solve_included", "has_solve"],
    "checksum_policy": ["checksumPolicy", "checksum_scope", "checksum_source"],
    "timer_boundary": ["timing_boundary", "timerBoundary", "timing_scope"],
    "gate": ["rvv_gate", "gate_policy"],
    "mask": ["finite_mask", "mask_policy"],
    "reduction": ["reduction_mode", "reduction_policy"],
    "asm_boundary": ["asmBoundary", "asm_symbol", "hot_symbol", "asm_hot_symbol"],
    "rvv_instr_count": ["rvv_instruction_count", "rvv_instr_lines", "rvv_line_count"],
    "std_ms": ["std_time_ms", "baseline_std_ms"],
    "rvv_ms": ["rvv_time_ms", "candidate_rvv_ms"],
    "ba_values": ["b_over_a_values", "speedup_values", "values"],
    "iterations": ["iters", "iteration_count", "iter_count"],
    "warmup_iterations": ["warm_up_iterations", "warmup_iters", "warmup"],
    "run_count": ["runs", "n_runs", "repeat_count"],
    "binary_hash": ["binary_sha256", "binary_digest", "elf_hash"],
}
DEFAULT_THRESHOLDS = {
    "min_repeated_runs": 5,
    "stable_runs": 20,
    "ba_degrade_threshold": 1.0,
    "high_degrade_fraction": 0.20,
    "long_tail_ratio": 1.15,
    "point_outlier_fraction": 0.20,
    "direction_epsilon_fraction": 0.02,
    "near_threshold_margin": 0.05,
    "component_full_epsilon_fraction": 0.05,
    "solve_delta_outlier_ratio": 3.0,
    "solve_delta_outlier_ms": 2.0,
}


@dataclass
class Issue:
    severity: str
    signal: str
    case: str
    observed_pattern: str
    why_suspicious: str
    possible_non_bug_explanations: str
    possible_bug_or_evidence_issues: str
    recommended_checks: str
    conclusion_policy: str


def _clean_cell(value: str) -> str:
    value = value.strip()
    value = value.strip("`")
    return value.strip()


def _norm_role(value: Any) -> str:
    return str(value or "").strip().lower().replace(" ", "_")


def _is_strict_role(role: str) -> bool:
    return _norm_role(role) in STRICT_ROLES


def _is_diagnostic_role(role: str) -> bool:
    return _norm_role(role) in DIAGNOSTIC_ROLES


def _get_path(mapping: dict[str, Any], dotted: str, default: Any = None) -> Any:
    current: Any = mapping
    for part in dotted.split("."):
        if not isinstance(current, dict) or part not in current:
            return default
        current = current[part]
    return current


def _first_present(*values: Any) -> Any:
    for value in values:
        if value is not None and value != "":
            return value
    return None


def _as_float(value: Any) -> float | None:
    if value is None or value == "":
        return None
    if isinstance(value, (int, float)):
        if math.isfinite(float(value)):
            return float(value)
        return None
    text = str(value).strip().rstrip("x")
    try:
        result = float(text)
    except ValueError:
        return None
    return result if math.isfinite(result) else None


def _as_int(value: Any) -> int | None:
    if value is None or value == "":
        return None
    if isinstance(value, int):
        return value
    try:
        return int(str(value).strip())
    except ValueError:
        return None


def _as_boolish(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value).strip().lower()


def _bool_value(value: Any) -> bool | None:
    if isinstance(value, bool):
        return value
    if value is None or value == "":
        return None
    text = str(value).strip().lower()
    if text in {"1", "true", "yes", "y", "on"}:
        return True
    if text in {"0", "false", "no", "n", "off"}:
        return False
    return None


def _as_values(value: Any) -> list[float]:
    if value is None:
        return []
    if isinstance(value, list):
        return [v for v in (_as_float(x) for x in value) if v is not None]
    if isinstance(value, str):
        return [float(x) for x in re.findall(r"([0-9]+(?:\.[0-9]+)?)\s*x", value)]
    return []


def _median(values: list[float]) -> float | None:
    return statistics.median(values) if values else None


def _pct(value: float) -> str:
    return f"{value * 100:.1f}%"


def _fmt_values(values: list[float]) -> str:
    return ", ".join(f"{v:.3g}x" for v in values)


def _issue(
    severity: str,
    signal: str,
    case: str,
    observed_pattern: str,
    why_suspicious: str,
    possible_non_bug_explanations: str,
    possible_bug_or_evidence_issues: str,
    recommended_checks: str,
    conclusion_policy: str,
) -> Issue:
    return Issue(
        severity=severity,
        signal=signal,
        case=case or "<summary>",
        observed_pattern=observed_pattern,
        why_suspicious=why_suspicious,
        possible_non_bug_explanations=possible_non_bug_explanations,
        possible_bug_or_evidence_issues=possible_bug_or_evidence_issues,
        recommended_checks=recommended_checks,
        conclusion_policy=conclusion_policy,
    )


def _comparison_name(comp: dict[str, Any], index: int) -> str:
    return str(comp.get("name") or comp.get("case_name") or f"comparison[{index}]")


def _comparison_role(comp: dict[str, Any], manifest: dict[str, Any]) -> str:
    return _norm_role(
        _first_present(
            comp.get("evidence_role"),
            comp.get("role"),
            _get_path(manifest, "summary.evidence_role"),
            manifest.get("evidence_role"),
            "summary_only_unknown",
        )
    )


def _thresholds(manifest: dict[str, Any]) -> dict[str, float]:
    result = dict(DEFAULT_THRESHOLDS)
    raw = manifest.get("thresholds") or _get_path(manifest, "checks.thresholds") or {}
    if isinstance(raw, dict):
        for key, value in raw.items():
            number = _as_float(value)
            if number is not None:
                result[key] = number
    return result


def _equal_fields(manifest: dict[str, Any], comp: dict[str, Any]) -> list[str]:
    fields = comp.get("equal_fields")
    if fields is None:
        fields = _get_path(manifest, "checks.equal_fields")
    if fields is None:
        fields = DEFAULT_EQUAL_FIELDS
    if not isinstance(fields, list):
        return list(DEFAULT_EQUAL_FIELDS)
    return [str(x) for x in fields]


def _aliases_for(field: str) -> list[str]:
    return FIELD_ALIASES.get(field, [])


def _mapping_value(mapping: dict[str, Any], field: str) -> tuple[Any, str | None]:
    if field in mapping:
        return mapping[field], None
    for alias in _aliases_for(field):
        if alias in mapping:
            return mapping[alias], alias
    return None, None


def _side_value_with_source(comp: dict[str, Any], side: str, field: str) -> tuple[Any, str | None]:
    side_map = comp.get(side)
    if isinstance(side_map, dict):
        value, alias = _mapping_value(side_map, field)
        if alias is not None or field in side_map:
            return value, f"{side}.{alias or field}"
    flat_key = f"{side}_{field}"
    if flat_key in comp:
        return comp[flat_key], None
    for alias in _aliases_for(field):
        for flat_alias in (f"{side}_{alias}", f"{side}_{alias.lower()}"):
            if flat_alias in comp:
                return comp[flat_alias], flat_alias
    return None, None


def _side_value(comp: dict[str, Any], side: str, field: str) -> Any:
    value, _alias = _side_value_with_source(comp, side, field)
    return value


def _side_metric(comp: dict[str, Any], side: str, field: str) -> float | None:
    return _as_float(_side_value(comp, side, field))


def _combined_value(manifest: dict[str, Any], comp: dict[str, Any], key: str) -> Any:
    comp_direct, _ = _mapping_value(comp, key)
    comp_meta = comp.get("metadata")
    comp_meta_value = None
    if isinstance(comp_meta, dict):
        comp_meta_value, _ = _mapping_value(comp_meta, key)
    summary = manifest.get("summary")
    summary_direct = None
    summary_meta_value = None
    if isinstance(summary, dict):
        summary_direct, _ = _mapping_value(summary, key)
        summary_meta = summary.get("metadata")
        if isinstance(summary_meta, dict):
            summary_meta_value, _ = _mapping_value(summary_meta, key)
    env = manifest.get("environment")
    env_value = None
    if isinstance(env, dict):
        env_value, _ = _mapping_value(env, key)
    return _first_present(
        comp_direct,
        comp_meta_value,
        summary_direct,
        summary_meta_value,
        env_value,
    )


def _comparison_timing_ms(comp: dict[str, Any], build: str) -> float | None:
    """返回 comparison 中某个 build 的平均耗时。

    Std/RVV compare manifest 常把 std_ms 放在 comparison 顶层，或放在 baseline /
    candidate 侧。这里兼容两种形态，供跨 case 的 component/full 关系检查使用。
    """
    if build == "std":
        direct = _as_float(comp.get("std_ms"))
        if direct is not None:
            return direct
        baseline = _as_float(_side_value(comp, "baseline", "std_ms"))
        if baseline is not None:
            return baseline
        return _as_float(_side_value(comp, "candidate", "std_ms"))
    if build == "rvv":
        direct = _as_float(comp.get("rvv_ms"))
        if direct is not None:
            return direct
        candidate = _as_float(_side_value(comp, "candidate", "rvv_ms"))
        if candidate is not None:
            return candidate
        return _as_float(_side_value(comp, "baseline", "rvv_ms"))
    return None


def _comparison_solve_included(comp: dict[str, Any]) -> bool | None:
    baseline = _bool_value(_side_value(comp, "baseline", "solve"))
    candidate = _bool_value(_side_value(comp, "candidate", "solve"))
    if baseline is not None and candidate is not None and baseline == candidate:
        return baseline
    if baseline is not None:
        return baseline
    return candidate


def _comparison_family_key(comp: dict[str, Any], *, include_formula: bool) -> tuple[Any, ...] | None:
    baseline_boundary = _side_value(comp, "baseline", "boundary")
    candidate_boundary = _side_value(comp, "candidate", "boundary")
    baseline_wrapper = _side_value(comp, "baseline", "wrapper")
    candidate_wrapper = _side_value(comp, "candidate", "wrapper")
    baseline_row_source = _side_value(comp, "baseline", "row_source")
    candidate_row_source = _side_value(comp, "candidate", "row_source")
    baseline_formula = _side_value(comp, "baseline", "formula_mode")
    candidate_formula = _side_value(comp, "candidate", "formula_mode")

    row_source = _first_present(comp.get("row_source"), baseline_row_source, candidate_row_source)
    formula = _first_present(comp.get("formula_mode"), baseline_formula, candidate_formula)
    boundary = _first_present(comp.get("boundary"), baseline_boundary, candidate_boundary)
    wrapper = _first_present(comp.get("wrapper"), baseline_wrapper, candidate_wrapper)
    point_type = comp.get("point_type")
    size = comp.get("size")
    if not all(value not in (None, "") for value in (row_source, boundary, wrapper, point_type, size)):
        return None
    parts: list[Any] = [str(boundary), str(wrapper), str(row_source), str(point_type), str(size)]
    if include_formula:
        if formula in (None, ""):
            return None
        parts.append(str(formula))
    return tuple(parts)


def parse_summary_markdown_context(text: str) -> dict[str, Any]:
    """解析常见 markdown summary 顶部的运行上下文字段。"""
    context: dict[str, Any] = {}
    key_map = {
        "runs": "run_count",
        "run_count": "run_count",
        "iterations": "iterations",
        "warm-up iterations": "warmup_iterations",
        "warmup iterations": "warmup_iterations",
        "warmup_iterations": "warmup_iterations",
        "device": "device",
        "taskset": "taskset",
        "governor": "governor",
        "freq": "freq",
        "frequency": "freq",
        "temperature": "temperature",
        "vlen": "vlen",
        "binary hash": "binary_hash",
        "binary_hash": "binary_hash",
    }
    for raw_line in text.splitlines():
        match = re.match(r"^\s*[-*]\s*([^:：]+)\s*[:：]\s*(.+?)\s*$", raw_line)
        if not match:
            continue
        raw_key, raw_value = match.group(1).strip(), match.group(2).strip()
        key = key_map.get(raw_key.lower())
        if not key:
            continue
        number = _as_int(raw_value)
        context[key] = number if number is not None and key in {"run_count", "iterations", "warmup_iterations"} else raw_value.strip("`")
    return context


def parse_summary_markdown(path: Path) -> dict[str, Any]:
    """从 markdown summary 表格中提取轻量 comparison。只解析数值异常，不假装有边界元数据。"""
    text = path.read_text(encoding="utf-8", errors="replace")
    context = parse_summary_markdown_context(text)
    comparisons: list[dict[str, Any]] = []
    header: list[str] | None = None
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line.startswith("|") or "|" not in line[1:]:
            continue
        parts = [_clean_cell(p) for p in line.strip("|").split("|")]
        lower = [p.lower() for p in parts]
        if any("benchmark" in p for p in lower) and any("run" in p for p in lower):
            header = lower
            continue
        if all(set(p) <= {"-", ":", " "} for p in parts):
            continue
        if not header or len(parts) < 2:
            continue
        case_name = parts[0]
        if not case_name:
            continue
        values: list[float] = []
        if "values" in header and header.index("values") < len(parts):
            values = _as_values(parts[header.index("values")])
        if not values:
            # fallback：只在没有 values 列时解析整行，避免把 median/min/max 重复当成 values。
            values = _as_values(" | ".join(parts[1:]))
        if not values:
            continue
        run_count = None
        for idx, name in enumerate(header):
            if name.startswith("runs") and idx < len(parts):
                run_count = _as_int(parts[idx])
                break
        inherited_context = dict(context)
        comparisons.append(
            {
                "name": case_name,
                "evidence_role": "summary_only_unknown",
                "source_summary": str(path),
                **inherited_context,
                "run_count": run_count or inherited_context.get("run_count") or len(values),
                "ba_values": values,
                "metadata_incomplete": True,
            }
        )
    return {
        "schema_version": SCHEMA_VERSION,
        "summary": {
            "title": f"summary-md:{path}",
            "evidence_role": "summary_only_unknown",
            "summary_path": str(path),
            "metadata": context,
        },
        "comparisons": comparisons,
        "checks": {"strict_ab": False},
    }


def load_manifest(path: Path | None, summary_md: Path | None) -> dict[str, Any]:
    if path and summary_md:
        raise ValueError("--manifest 与 --summary-md 只能二选一")
    if summary_md:
        return parse_summary_markdown(summary_md)
    if not path:
        raise ValueError("需要传入 --manifest 或 --summary-md；可用 --write-template 生成模板")
    with path.open("r", encoding="utf-8") as fh:
        manifest = json.load(fh)
    if not isinstance(manifest, dict):
        raise ValueError("manifest 顶层必须是 JSON object")
    return manifest


def check_manifest_shape(manifest: dict[str, Any]) -> list[Issue]:
    issues: list[Issue] = []
    version = manifest.get("schema_version")
    if version not in (None, SCHEMA_VERSION):
        issues.append(
            _issue(
                "WARNING",
                "schema_version_mismatch",
                "<manifest>",
                f"schema_version={version}, 当前脚本版本={SCHEMA_VERSION}",
                "schema 版本不一致时，字段语义可能已经漂移。",
                "可能只是旧 summary 尚未迁移。",
                "也可能是脚本和 manifest 模板来自不同版本，导致检查遗漏。",
                "确认生成 manifest 的脚本版本；必要时用 --write-template 重新生成模板并迁移字段。",
                "版本未确认前，不要把 doctor 结果写成完整通过；至少在 Handoff 中说明版本差异。",
            )
        )
    comparisons = manifest.get("comparisons")
    if not isinstance(comparisons, list) or not comparisons:
        issues.append(
            _issue(
                "ERROR",
                "missing_comparisons",
                "<manifest>",
                "manifest 没有非空 comparisons 列表。",
                "没有 comparison 时，脚本无法检查 A/B 边界、数值分布或异常频率。",
                "可能是当前工作只生成了 raw log，还没有生成 summary manifest。",
                "也可能是 analysis script 输出路径错误或旧日志未被解析。",
                "先生成 summary manifest，至少列出 case name、证据角色、run count、baseline/candidate 或 ba_values。",
                "不能把空 doctor report 当作证据通过。",
            )
        )
    return issues


def _find_alias_usages_in_mapping(prefix: str, mapping: dict[str, Any], canonical_fields: list[str]) -> list[str]:
    usages: list[str] = []
    for field in canonical_fields:
        for alias in _aliases_for(field):
            if alias in mapping:
                usages.append(f"{prefix}{alias} -> {field}")
    return usages


def _find_alias_usages(manifest: dict[str, Any], comp: dict[str, Any]) -> list[str]:
    fields = sorted(set(DEFAULT_EQUAL_FIELDS + DEFAULT_REQUIRED_SUMMARY_FIELDS + DEFAULT_ENV_FIELDS + list(FIELD_ALIASES)))
    usages: list[str] = []
    usages.extend(_find_alias_usages_in_mapping("comparison.", comp, fields))
    for side in ("baseline", "candidate"):
        side_map = comp.get(side)
        if isinstance(side_map, dict):
            usages.extend(_find_alias_usages_in_mapping(f"{side}.", side_map, fields))
    metadata = comp.get("metadata")
    if isinstance(metadata, dict):
        usages.extend(_find_alias_usages_in_mapping("comparison.metadata.", metadata, fields))
    summary = manifest.get("summary")
    if isinstance(summary, dict):
        usages.extend(_find_alias_usages_in_mapping("summary.", summary, fields))
        summary_meta = summary.get("metadata")
        if isinstance(summary_meta, dict):
            usages.extend(_find_alias_usages_in_mapping("summary.metadata.", summary_meta, fields))
    env = manifest.get("environment")
    if isinstance(env, dict):
        usages.extend(_find_alias_usages_in_mapping("environment.", env, fields))
    return sorted(set(usages))


def check_contract(manifest: dict[str, Any], comp: dict[str, Any], index: int) -> list[Issue]:
    issues: list[Issue] = []
    case = _comparison_name(comp, index)
    role = _comparison_role(comp, manifest)
    strict = bool(comp.get("strict_ab")) or bool(_get_path(manifest, "checks.strict_ab")) or _is_strict_role(role)
    label = str(
        _first_present(
            comp.get("label"),
            comp.get("summary_label"),
            _get_path(manifest, "summary.label"),
            _get_path(manifest, "summary.title"),
            case,
        )
    )

    lower_label = label.lower()
    alias_usages = _find_alias_usages(manifest, comp)
    if alias_usages:
        issues.append(
            _issue(
                "SUGGESTION",
                "noncanonical_field_alias",
                case,
                "检测到非规范字段名：" + ", ".join(alias_usages[:12]) + (" ..." if len(alias_usages) > 12 else ""),
                "alias 可以让旧 summary 继续被检查，但字段名漂移会降低跨 topic 稳定性和 reviewer 可读性。",
                "可能是 topic-local wrapper 沿用了旧命名或外部工具字段名。",
                "也可能是拼写错误；如果 alias 表没有覆盖，严格检查会把字段视为缺失并阻塞。",
                "后续把 manifest 归一化为 evidence-doctor.zh.md 中的规范字段；只把 alias 作为迁移兼容层。",
                "本次可继续检查，但 Handoff 应说明使用了非规范字段名，避免把 alias 当作长期合同。",
            )
        )
    if ("strict" in lower_label or "a/b" in lower_label or "a-b" in lower_label) and _is_diagnostic_role(role):
        issues.append(
            _issue(
                "WARNING",
                "role_name_mismatch",
                case,
                f"summary/case 名称暗示 strict A/B，但 evidence_role={role}。",
                "名称和证据角色不一致会诱导 worker 把 cross-check 或 diagnostic 当成严格性能结论。",
                "可能是旧命名没有迁移，或者当前表只用于诊断但名称沿用了 A/B。",
                "也可能是 manifest 角色填错，导致 hard guard 没有开启。",
                "修正 evidence_role 或重命名 summary；若确实是 strict A/B，补齐同边界 metadata 并开启 strict_ab。",
                "未修正前，结论必须按较弱证据角色书写，不能写成严格 candidate-vs-baseline B/A。",
            )
        )
    if "production" in lower_label and role in {"diagnostic", "summary_only_unknown"}:
        issues.append(
            _issue(
                "WARNING",
                "production_name_without_role",
                case,
                f"名称包含 production，但 evidence_role={role}。",
                "production-shaped、production direct 和 production-ready 是不同证据角色。名称含 production 但角色不清会扩大结论边界。",
                "可能只是 case 模拟生产输入形态，并未命中真实 production dispatch。",
                "也可能是 production direct 证据缺失或 manifest 未记录。",
                "明确写成 production-shaped diagnostic、production direct 或 mixed-boundary cross-check，并列出 public entry 是否真实命中。",
                "角色未澄清前，不能把该表单独作为 production evidence。",
            )
        )

    baseline = comp.get("baseline")
    candidate = comp.get("candidate")
    has_sides = isinstance(baseline, dict) or isinstance(candidate, dict)
    if strict and not (isinstance(baseline, dict) and isinstance(candidate, dict)):
        issues.append(
            _issue(
                "ERROR",
                "strict_ab_missing_sides",
                case,
                "strict A/B 没有同时提供 baseline 和 candidate metadata。",
                "严格 A/B 的核心是证明两侧只有目标变量不同；缺少任一侧 metadata 就无法检查可比性。",
                "可能 summary 仍是旧格式，只保存了最终 speedup。",
                "也可能 analysis script 没有把 wrapper、boundary、checksum 等字段传给 doctor。",
                "补 baseline/candidate 字段，或将该证据降级为 summary-only warning / diagnostic。",
                "缺少两侧 metadata 时，doctor 必须阻止严格性能结论。",
            )
        )

    if has_sides:
        equal_fields = _equal_fields(manifest, comp)
        allowed = set(str(x) for x in comp.get("allowed_contract_mismatches", []))
        for field in equal_fields:
            if field in allowed:
                continue
            lhs = _side_value(comp, "baseline", field)
            rhs = _side_value(comp, "candidate", field)
            if lhs is None or rhs is None:
                severity = "ERROR" if strict else "WARNING"
                issues.append(
                    _issue(
                        severity,
                        "missing_contract_field",
                        case,
                        f"字段 {field!r} 缺失：baseline={lhs!r}, candidate={rhs!r}。",
                        "缺少契约字段时，无法证明两侧是否真的在同一证据边界内比较。",
                        "可能该字段对当前 topic 不适用，或旧脚本尚未输出。",
                        "也可能是 summary 混入旧日志、case label 无法解析，或 worker 忽略了关键边界。",
                        f"若字段适用，补齐 baseline/candidate.{field}；若不适用，在 allowed_contract_mismatches 或 notes 中解释。",
                        "strict A/B 下缺字段视为阻塞；diagnostic 下必须在结论中说明 metadata 不完整。",
                    )
                )
            elif _as_boolish(lhs) != _as_boolish(rhs):
                severity = "ERROR" if strict else "WARNING"
                issues.append(
                    _issue(
                        severity,
                        "contract_mismatch",
                        case,
                        f"字段 {field!r} 不一致：baseline={lhs!r}, candidate={rhs!r}。",
                        "A/B 两侧边界不一致时，数值差异可能来自 wrapper、row source、solve 或计时口径，而不是候选本身。",
                        "可能这是有意设计的 cross-check，用于了解真实 public overload 与 helper 的差异。",
                        "也可能是表名或 summary 角色错误，把 mixed-boundary 数据当成 strict A/B。",
                        "若有意混合边界，重命名为 mixed-boundary cross-check 并降级结论；若不是，按同边界重跑。",
                        "strict A/B 下该项阻塞严格性能结论；diagnostic 下必须显式解释并避免外推到 production。",
                    )
                )

        checksum_l = _side_value(comp, "baseline", "checksum")
        checksum_r = _side_value(comp, "candidate", "checksum")
        if checksum_l is not None and checksum_r is not None and str(checksum_l) != str(checksum_r):
            issues.append(
                _issue(
                    "ERROR",
                    "checksum_mismatch",
                    case,
                    f"checksum 不一致：baseline={checksum_l}, candidate={checksum_r}。",
                    "正确性未闭合时，性能数字不能说明候选优化是否语义等价。",
                    "可能 checksum 口径不同、包含 warm-up、或 raw log 解析到了不同输出对象。",
                    "也可能是实现公式、mask、reduction、索引或 fallback 语义存在错误。",
                    "先修 correctness：确认 checksum_policy、输入对象、warm-up 是否一致，必要时运行 numerical consistency / QEMU / board correctness。",
                    "禁止性能结论；只能报告 correctness blocked。",
                )
            )

        asm_l = _side_value(comp, "baseline", "asm_boundary")
        asm_r = _side_value(comp, "candidate", "asm_boundary")
        if strict and (not asm_l or not asm_r):
            issues.append(
                _issue(
                    "WARNING",
                    "asm_boundary_missing",
                    case,
                    f"strict/performance comparison 缺少 asm_boundary：baseline={asm_l!r}, candidate={asm_r!r}。",
                    "没有反汇编归属时，无法证明性能数字来自目标 hot path，而不是 bench harness、Eigen/libm 或无关代码。",
                    "可能当前阶段只做 board summary，asm 将在后续补齐。",
                    "也可能 RVV 指令存在但没有归属到 production 符号或目标 helper。",
                    "补 objdump / attribution summary，记录 hot symbol、inline/clone 情况和关键 RVV 指令。",
                    "若生产接入结论依赖性能，该 warning 必须在 EvidenceDecision 前闭合或作为风险写明。",
                )
            )

    if comp.get("metadata_incomplete"):
        issues.append(
            _issue(
                "WARNING",
                "summary_only_metadata_missing",
                case,
                "当前输入来自 markdown summary，缺少 baseline/candidate、boundary、checksum、asm 和环境 metadata。",
                "只看表格数值只能发现部分异常，不能证明证据边界可比较。",
                "可能是历史 summary 尚未接入 manifest。",
                "也可能当前 topic 的 analyzer 还没有输出 EvidenceMeta。",
                "为正式 evidence 生成 JSON manifest，至少补 case_kind、boundary、row_source、solve、checksum_policy、checksum、asm_boundary 和环境字段。",
                "summary-only doctor 结果只能作为 reviewer aid，不能写成完整 Evidence Doctor 通过。",
            )
        )

    return issues


def check_environment(manifest: dict[str, Any], comp: dict[str, Any], index: int) -> list[Issue]:
    issues: list[Issue] = []
    case = _comparison_name(comp, index)
    role = _comparison_role(comp, manifest)
    strict_or_perf = _is_strict_role(role) or "production" in role or "board" in role or comp.get("requires_board_context")
    if not strict_or_perf and role == "summary_only_unknown":
        strict_or_perf = True
    if not strict_or_perf and (
        comp.get("build_comparison")
        or _comparison_timing_ms(comp, "std") is not None
        or _comparison_timing_ms(comp, "rvv") is not None
    ):
        strict_or_perf = True

    if not strict_or_perf:
        return issues

    missing = [key for key in DEFAULT_REQUIRED_SUMMARY_FIELDS if _combined_value(manifest, comp, key) in (None, "")]
    if missing:
        issues.append(
            _issue(
                "WARNING",
                "missing_run_contract",
                case,
                "缺少运行合同字段：" + ", ".join(missing),
                "缺少 iterations、warm-up 或 run count 会让性能分布和异常频率无法复核。",
                "可能当前证据只是 smoke 或单次诊断，不是 repeated board。",
                "也可能 summary 漏写运行参数，导致后续 reviewer 无法重跑同一条件。",
                "在 summary manifest 中补齐 iterations、warmup_iterations、run_count；单次运行必须说明不能支撑稳定性能结论。",
                "这些字段缺失时，不应写强 production performance 结论。",
            )
        )

    warmup_iterations = _as_int(_combined_value(manifest, comp, "warmup_iterations"))
    if warmup_iterations == 0:
        issues.append(
            _issue(
                "WARNING",
                "zero_warmup_iterations",
                case,
                "manifest 记录 warmup_iterations=0。",
                "带 Std/RVV 数字的 bench 若没有 warm-up，容易把冷启动、缓存状态、频率爬升或首次路径成本混进性能表。",
                "可能当前 target 原本只是 quick board smoke，用于证明可运行和日志形状。",
                "也可能是 Makefile / collect script 漏传 warmup，导致 worker 把 smoke 数值当成性能分析输入。",
                "正式 bench 或任何给用户展示的 Std/RVV timing 默认加入 warm-up；若必须无 warm-up，target / run label 应显式写 smoke_no_warmup，并禁止性能排序或 production 结论。",
                "该数据只能作为 no-warmup diagnostic / historical evidence；进入 EvidenceDecision 前应按相同输入重跑带 warm-up 的板卡数据。",
            )
        )

    env_missing = [key for key in DEFAULT_ENV_FIELDS if _combined_value(manifest, comp, key) in (None, "")]
    if env_missing:
        issues.append(
            _issue(
                "SUGGESTION",
                "environment_metadata_missing",
                case,
                "缺少环境字段：" + ", ".join(env_missing),
                "环境字段缺失不会必然推翻结果，但会削弱对长尾、run-to-run 反转和异常值的解释能力。",
                "可能当前板卡环境固定，worker 没有重复记录。",
                "也可能温度、governor、频率或绑核变化影响了异常点。",
                "下次采集记录 taskset、governor、freq、temperature、device / VLEN；必要时把环境摘要写入 board summary。",
                "若没有其它异常，可继续，但 Handoff 应说明环境 metadata 边界。",
            )
        )

    binary_hash = _combined_value(manifest, comp, "binary_hash")
    if not binary_hash:
        issues.append(
            _issue(
                "SUGGESTION",
                "binary_identity_missing",
                case,
                "缺少 binary_hash 或等价二进制身份字段。",
                "没有二进制身份时，难以排除旧 binary、旧日志或不同编译选项混入。",
                "可能当前 summary 的命令已经足够复现。",
                "也可能 raw log 来自不同 build，尤其是多轮远端板卡采集。",
                "在 repeated board summary 或 manifest 中记录 binary hash / git tree / build label；发现冲突时清理旧日志后重跑。",
                "不是单独阻塞项，但遇到方向反转或长尾时应优先补齐。",
            )
        )
    return issues


def check_values(manifest: dict[str, Any], comp: dict[str, Any], index: int) -> list[Issue]:
    issues: list[Issue] = []
    case = _comparison_name(comp, index)
    thresholds = _thresholds(manifest)
    values = _as_values(_combined_value(manifest, comp, "ba_values"))
    run_count = _as_int(_combined_value(manifest, comp, "run_count"))

    if values:
        med = statistics.median(values)
        min_v = min(values)
        max_v = max(values)
        degrade_threshold = thresholds["ba_degrade_threshold"]
        degraded = [v for v in values if v < degrade_threshold]
        degrade_fraction = len(degraded) / len(values)
        if degraded:
            severity = "WARNING" if degrade_fraction <= thresholds["high_degrade_fraction"] else "ERROR"
            issues.append(
                _issue(
                    severity,
                    "ba_degradation_frequency",
                    case,
                    f"B/A values={_fmt_values(values)}；{len(degraded)}/{len(values)} 低于 {degrade_threshold:.3g}。",
                    "平均或 median 正向不能掩盖退化频率；高频退化说明候选不稳定或 case 分布被少数值主导。",
                    "可能是测量噪声、温度 / 频率波动、输入局部性变化或 run 数不足。",
                    "也可能是候选在某些点型、规模、row source、mask 或 reduction 路径上真实退化。",
                    "报告退化频率；必要时扩大 runs，按 point type / size / row source 分组，检查 trace 和 asm attribution。",
                    "退化频率未解释前，不能只写 median / mean 正向；若高于阈值，production 结论应降级或阻塞。",
                )
            )
        if min_v > 0 and max_v / min_v >= thresholds["long_tail_ratio"]:
            issues.append(
                _issue(
                    "WARNING",
                    "long_tail_or_variance",
                    case,
                    f"min={min_v:.3g}x, median={med:.3g}x, max={max_v:.3g}x，max/min={max_v / min_v:.2f}。",
                    "长尾说明单一均值不足以描述稳定性，异常值可能改变接入判断。",
                    "可能是板卡温度、频率、调度、缓存状态或 run 顺序导致。",
                    "也可能是候选存在数据相关分支、fallback、spill/reload 或内存带宽拐点。",
                    "查看 per-iteration trace、温度 / governor / freq / taskset；必要时扩大到 20-run 或 50-run。",
                    "可以继续分析，但 summary 必须保留 min/median/max 和长尾解释，不能先验剔除异常。",
                )
            )
        if 1 <= len(values) < thresholds["min_repeated_runs"]:
            issues.append(
                _issue(
                    "WARNING",
                    "low_run_count",
                    case,
                    f"只解析到 {len(values)} 个 repeated value。",
                    "run 数过低时，无法判断异常频率和稳定性。",
                    "可能当前只是 quick board check。",
                    "也可能 raw logs 缺失或 summary parser 只读到部分行。",
                    "正式 board summary 至少使用 5-run；接近阈值或方向不稳时扩大到 20-run / 50-run。",
                    "低 run count 只能支撑初筛，不能支撑强 production performance 结论。",
                )
            )
        if run_count is not None and run_count != len(values):
            if run_count > 0 and len(values) > run_count and len(values) % run_count == 0:
                issues.append(
                    _issue(
                        "SUGGESTION",
                        "combined_repeated_groups",
                        case,
                        f"run_count={run_count}，ba_values 解析到 {len(values)} 个值，像是 {len(values) // run_count} 个规模或子组被合并在一行。",
                        "一行混合多个子组时，doctor 可以粗略检查总体异常，但无法判断每个 size / point type 子组的独立稳定性。",
                        "可能 summary 为了节省篇幅把 64K / 256K 或多个 batch 的 values 合在一行。",
                        "也可能 case label 没有拆分规模，导致某个子组退化被其它子组掩盖。",
                        "正式 manifest 应把每个 size / point type / row source 拆成独立 comparison，或在字段中显式记录 subgroup。",
                        "该项不阻塞；但涉及 production 结论时，应优先按子组报告稳定性。",
                    )
                )
            else:
                issues.append(
                    _issue(
                        "WARNING",
                        "run_count_values_mismatch",
                        case,
                        f"run_count={run_count}，但 ba_values 解析到 {len(values)} 个值。",
                        "run count 和 values 数量不一致时，可能有日志丢失或 summary 拼接错误。",
                        "可能 values 只列出筛选后的子集。",
                        "也可能 collection/fetch 阶段漏了某些 run，或旧日志混入。",
                        "核对 raw log 列表和 repeated summary；明确 values 是全量还是过滤后子集。",
                        "未解释前，不要用该 frequency 作为稳定性证据。",
                    )
                )
        if thresholds["ba_degrade_threshold"] <= med < thresholds["ba_degrade_threshold"] + thresholds["near_threshold_margin"]:
            issues.append(
                _issue(
                    "SUGGESTION",
                    "near_threshold_ba",
                    case,
                    f"median={med:.3g}x，距离 1.0 阈值不足 {thresholds['near_threshold_margin']:.2f}。",
                    "接近阈值的收益容易被测量波动、输入分布或二进制差异反转。",
                    "可能仍是可接受的弱收益，尤其当实现简单、fallback 清楚。",
                    "也可能真实收益不足，接入后维护成本高于收益。",
                    "扩大 runs，补静态实现质量分析；文档写清为什么弱收益仍可接受或为什么暂缓。",
                    "不能只因 median 略大于 1 就写成稳定加速。",
                )
            )
    elif _is_strict_role(_comparison_role(comp, manifest)):
        issues.append(
            _issue(
                "WARNING",
                "missing_repeated_values",
                case,
                "未提供 ba_values / speedup_values。",
                "没有 repeated values 时，doctor 无法检查退化频率、长尾和稳定性。",
                "可能当前 manifest 只用于数据契约检查。",
                "也可能 analysis script 只输出了单个 mean/median，丢失了分布信息。",
                "保留每轮 B/A 或 speedup values；至少输出 median/min/max 和 B/A<1 频率。",
                "缺少分布信息时，性能结论必须说明稳定性边界。",
            )
        )
    return issues


def check_direction_conflicts(manifest: dict[str, Any], comp: dict[str, Any], index: int) -> list[Issue]:
    issues: list[Issue] = []
    case = _comparison_name(comp, index)
    thresholds = _thresholds(manifest)
    eps = thresholds["direction_epsilon_fraction"]

    b_std = _side_metric(comp, "baseline", "std_ms")
    c_std = _side_metric(comp, "candidate", "std_ms")
    b_rvv = _side_metric(comp, "baseline", "rvv_ms")
    c_rvv = _side_metric(comp, "candidate", "rvv_ms")
    values = _as_values(_combined_value(manifest, comp, "ba_values"))
    med = _median(values)

    if None not in (b_std, c_std, b_rvv, c_rvv) and b_std and b_rvv:
        assert b_std is not None and c_std is not None and b_rvv is not None and c_rvv is not None
        std_delta = (b_std - c_std) / b_std
        rvv_delta = (b_rvv - c_rvv) / b_rvv
        if abs(std_delta) > eps and abs(rvv_delta) > eps and (std_delta > 0) != (rvv_delta > 0):
            issues.append(
                _issue(
                    "WARNING",
                    "std_rvv_direction_conflict",
                    case,
                    f"Std 侧 candidate 相对 baseline 变化 {std_delta:+.1%}，RVV 侧变化 {rvv_delta:+.1%}。",
                    "Std 与 RVV 方向相反不必然是 bug，但它说明性能差异可能来自边界、编译、测量或实现形态，而不是单一候选因素。",
                    "可能是 std 和 RVV build 的编译器优化不同、缓存/频率波动，或 candidate 对 RVV 资源压力更高。",
                    "也可能是 baseline/candidate wrapper、row source、solve、asm attribution 或 checksum policy 不一致。",
                    "先确认同边界 metadata；再检查 RVV-vs-RVV B/A、asm attribution、trace 和 run-to-run 稳定性。",
                    "该 warning 必须进入 summary 或 Handoff；未解释前不要直接写候选整体更优。",
                )
            )
        baseline_speedup = b_std / b_rvv if b_rvv > 0 else None
        candidate_speedup = c_std / c_rvv if c_rvv > 0 else None
        if baseline_speedup and candidate_speedup and med is not None:
            speedup_delta = (candidate_speedup - baseline_speedup) / baseline_speedup
            if speedup_delta > eps and med < 1.0:
                issues.append(
                    _issue(
                        "WARNING",
                        "speedup_metric_conflict",
                        case,
                        f"candidate std/RVV speedup 比 baseline 高 {speedup_delta:+.1%}，但 RVV-vs-RVV B/A median={med:.3g}x。",
                        "std/RVV speedup 只能说明同一 case 的 build 差异，不能替代同一 RVV binary 内的候选 A/B。",
                        "可能 candidate 的 std 侧更慢或 RVV 侧编译差异让 speedup 看起来更高。",
                        "也可能 worker 把两个不同指标混用，导致结论方向错误。",
                        "生产候选判断优先使用同一 RVV binary / 同一 log 内 RVV-vs-RVV B/A；std/RVV speedup 只作为辅助说明。",
                        "必须修正文档口径，不能用 std/RVV speedup 覆盖 RVV-vs-RVV 退化。",
                    )
                )

    instr_b = _side_metric(comp, "baseline", "rvv_instr_count")
    instr_c = _side_metric(comp, "candidate", "rvv_instr_count")
    if instr_b is not None and instr_c is not None and med is not None:
        if instr_c < instr_b and med < 1.0:
            issues.append(
                _issue(
                    "WARNING",
                    "fewer_instructions_but_slower",
                    case,
                    f"candidate RVV 指令数 {instr_c:g} 少于 baseline {instr_b:g}，但 B/A median={med:.3g}x。",
                    "指令数更少但更慢说明瓶颈可能不在总指令数，或者归因边界没有闭合。",
                    "可能是寄存器压力、spill/reload、访存、vsetvl、分支、cache 或频率波动。",
                    "也可能 asm attribution 统计到了不同符号、inline/clone 或无关代码。",
                    "检查 hot symbol、mnemonic histogram、spill/reload、load/store、vsetvli 和 trace；必要时做更窄 component ablation。",
                    "不能只用指令数减少支撑接入；必须解释为什么性能没有同步改善。",
                )
            )
    return issues


def check_group_outliers(manifest: dict[str, Any]) -> list[Issue]:
    issues: list[Issue] = []
    thresholds = _thresholds(manifest)
    grouped: dict[str, list[tuple[int, dict[str, Any], float]]] = {}
    for idx, comp in enumerate(manifest.get("comparisons") or []):
        if not isinstance(comp, dict):
            continue
        group = str(comp.get("group") or comp.get("case_family") or "").strip()
        if not group:
            continue
        values = _as_values(_combined_value(manifest, comp, "ba_values"))
        med = _median(values)
        if med is not None:
            grouped.setdefault(group, []).append((idx, comp, med))

    for group, rows in grouped.items():
        if len(rows) < 3:
            continue
        group_median = statistics.median([m for _, _, m in rows])
        if group_median <= 0:
            continue
        for idx, comp, med in rows:
            rel = abs(med - group_median) / group_median
            direction_differs = (med < 1.0) != (group_median < 1.0)
            if rel >= thresholds["point_outlier_fraction"] or direction_differs:
                case = _comparison_name(comp, idx)
                issues.append(
                    _issue(
                        "WARNING",
                        "group_outlier",
                        case,
                        f"group={group} 的组内 median={group_median:.3g}x；当前 case median={med:.3g}x，偏离 {rel:.1%}。",
                        "同组 point type / size / row source 中单个 case 明显偏离，说明结论可能不应按组整体外推。",
                        "可能是点型 layout、AoS stride、字段 offset、缓存局部性或测量噪声不同。",
                        "也可能是该 case 的 gate、mask、fallback、row source 或 asm boundary 与其它 case 不同。",
                        "按 point type / size / row source 分开报告；检查 metadata、trace 和 asm attribution，必要时收窄 production gate。",
                        "未解释前，不能把其它 case 的收益直接继承到该 case。",
                    )
                )
    return issues


def check_component_full_relationships(manifest: dict[str, Any]) -> list[Issue]:
    issues: list[Issue] = []
    thresholds = _thresholds(manifest)
    eps = thresholds["component_full_epsilon_fraction"]
    comparisons = manifest.get("comparisons") or []
    by_formula: dict[tuple[Any, ...], dict[bool, tuple[int, dict[str, Any]]]] = {}
    solve_deltas: dict[tuple[str, tuple[Any, ...]], list[tuple[int, dict[str, Any], float, str]]] = {}

    if not isinstance(comparisons, list):
        return issues

    for idx, comp in enumerate(comparisons):
        if not isinstance(comp, dict):
            continue
        solve = _comparison_solve_included(comp)
        if solve is None:
            continue
        key = _comparison_family_key(comp, include_formula=True)
        if key is None:
            continue
        by_formula.setdefault(key, {})[solve] = (idx, comp)

    for key, pair in by_formula.items():
        if True not in pair or False not in pair:
            continue
        full_idx, full = pair[True]
        component_idx, component = pair[False]
        full_name = _comparison_name(full, full_idx)
        component_name = _comparison_name(component, component_idx)
        full_checksum = _side_value(full, "baseline", "checksum_policy") or _side_value(full, "candidate", "checksum_policy")
        component_checksum = _side_value(component, "baseline", "checksum_policy") or _side_value(component, "candidate", "checksum_policy")
        for build, label in (("std", "Std"), ("rvv", "RVV")):
            full_ms = _comparison_timing_ms(full, build)
            component_ms = _comparison_timing_ms(component, build)
            if full_ms is None or component_ms is None or full_ms <= 0:
                continue
            delta = full_ms - component_ms
            if component_ms > full_ms * (1.0 + eps):
                ratio = component_ms / full_ms
                issues.append(
                    _issue(
                        "WARNING",
                        "component_no_solve_slower_than_full_estimate",
                        f"{component_name} [{label}]",
                        f"component no-solve {component_ms:.4g} ms > full estimate {full_ms:.4g} ms，ratio={ratio:.3g}x。",
                        "component no-solve 理论上少了 solve / matrix 构造；明显更慢说明这个 pair 不能被直接解释为“只少做 solve”。",
                        "可能是 cold cache、频率 / 中断、branch predictor、日志顺序、编译差异、checksum sink 或计时边界造成。",
                        "也可能是 component wrapper、accumulate helper、fallback gate、sink policy 或 manifest 解析有误。",
                        "用带 warm-up 的板卡 repeated run 复核；必要时增加 sink-aligned pair、per-iteration trace、asm attribution 或 profile。",
                        "该 warning 未解释前，component/full 对照只能作为异常诊断，不能支撑 solve 成本或候选取舍结论。",
                    )
                )
            if delta > 0:
                group_key = _comparison_family_key(full, include_formula=False)
                if group_key is not None:
                    solve_deltas.setdefault((build, group_key), []).append(
                        (full_idx, full, delta, component_name)
                    )
        if full_checksum and component_checksum and str(full_checksum) != str(component_checksum):
            issues.append(
                _issue(
                    "SUGGESTION",
                    "component_full_checksum_sink_mixed",
                    f"{component_name} vs {full_name}",
                    f"full checksum_policy={full_checksum!r}，component checksum_policy={component_checksum!r}。",
                    "full estimate 和 component no-solve 的输出对象不同；这不是 correctness 错误，但会削弱二者耗时差的归因。",
                    "可能当前设计只是为了防止编译器消除两条路径，而不是为了严格测 solve overhead。",
                    "也可能文档把 component no-solve 误写成与 full estimate 只差 solve / matrix 构造。",
                    "后续若要比较二者，应让 full 路径也保留 normal-equation sink，或把该 pair 明确标为 mixed-sink component diagnostic。",
                    "checksum sink 未对齐时，不要把 full-component 差值写成单独的 solve / matrix 构造成本。",
                )
            )

    for (build, group_key), rows in solve_deltas.items():
        if len(rows) < 2:
            continue
        deltas = [delta for _, _, delta, _ in rows]
        median_delta = statistics.median(deltas)
        if median_delta <= 0:
            continue
        for idx, comp, delta, component_name in rows:
            excess = delta - median_delta
            ratio = delta / median_delta if median_delta > 0 else math.inf
            if ratio < thresholds["solve_delta_outlier_ratio"] and excess < thresholds["solve_delta_outlier_ms"]:
                continue
            full_name = _comparison_name(comp, idx)
            issues.append(
                _issue(
                    "WARNING",
                    "full_component_solve_delta_outlier",
                    f"{full_name} [{build.upper()}]",
                    f"full-component delta={delta:.4g} ms，同组 median delta={median_delta:.4g} ms，ratio={ratio:.3g}x。",
                    "同一 row source / 点型 / size 下，某个 full estimate 比对应 component 多出的时间明显偏离其它候选，说明异常可能在 solve 之外。",
                    "可能是测量波动、cache / 频率状态、case 顺序、matrix checksum、branch 或 wrapper 差异。",
                    "也可能是 full estimate helper、solve 输入、fallback gate、寄存器压力、spill/reload 或 asm hot path 归属存在真实问题。",
                    f"对 `{full_name}` 和 `{component_name}` 做带 warm-up repeated board；补 per-iteration trace、asm attribution，并确认 full 和 component sink 口径。",
                    "未解释前，不能把该 full estimate 退化直接归因到 fused formula 本身，也不能永久拒绝生产候选。",
                )
            )
    return issues


def run_checks(manifest: dict[str, Any]) -> list[Issue]:
    issues = check_manifest_shape(manifest)
    comparisons = manifest.get("comparisons") or []
    if isinstance(comparisons, list):
        for index, comp in enumerate(comparisons):
            if not isinstance(comp, dict):
                issues.append(
                    _issue(
                        "ERROR",
                        "invalid_comparison_entry",
                        f"comparison[{index}]",
                        "comparison entry 不是 JSON object。",
                        "脚本无法解析该 comparison 的 case、metadata 或数值。",
                        "可能是 manifest 生成脚本输出了错误结构。",
                        "也可能是手写 manifest 时缩进或列表层级错误。",
                        "修正 manifest，使每个 comparison 都是 object。",
                        "该 entry 不能作为证据。",
                    )
                )
                continue
            issues.extend(check_contract(manifest, comp, index))
            issues.extend(check_environment(manifest, comp, index))
            issues.extend(check_values(manifest, comp, index))
            issues.extend(check_direction_conflicts(manifest, comp, index))
    issues.extend(check_group_outliers(manifest))
    issues.extend(check_component_full_relationships(manifest))
    return issues


def render_markdown(manifest: dict[str, Any], issues: list[Issue]) -> str:
    title = _first_present(_get_path(manifest, "summary.title"), manifest.get("title"), "Evidence Doctor Report")
    summary_path = _first_present(_get_path(manifest, "summary.summary_path"), manifest.get("summary_path"), "not_recorded")
    role = _first_present(_get_path(manifest, "summary.evidence_role"), manifest.get("evidence_role"), "not_recorded")
    comparisons = manifest.get("comparisons") or []
    counts = {sev: sum(1 for item in issues if item.severity == sev) for sev in ("ERROR", "WARNING", "SUGGESTION")}

    lines: list[str] = []
    lines.append("# Evidence Doctor Report（证据体检报告）")
    lines.append("")
    lines.append(f"- title：{title}")
    lines.append(f"- evidence_role：{role}")
    lines.append(f"- summary_path：{summary_path}")
    lines.append(f"- comparisons：{len(comparisons) if isinstance(comparisons, list) else 'invalid'}")
    lines.append(f"- result：Errors={counts['ERROR']}，Warnings={counts['WARNING']}，Suggestions={counts['SUGGESTION']}")
    lines.append("")
    lines.append("本报告不会自动证明实现有 bug。它用于提示 worker / reviewer：哪些数据模式需要解释、重跑、降级证据边界或补充 metadata。")
    lines.append("")

    if not issues:
        lines.append("## No findings")
        lines.append("")
        lines.append("未发现脚本规则覆盖范围内的 Errors / Warnings / Suggestions。仍需 reviewer 按 topic 文档、源码、测试和证据路径复核。")
        lines.append("")
        return "\n".join(lines)

    for severity, heading in (
        ("ERROR", "Errors（必须修正，否则不能作为 production evidence 或严格性能结论）"),
        ("WARNING", "Warnings（可以继续，但结论必须说明风险和处理方式）"),
        ("SUGGESTION", "Suggestions（不阻塞当前结论，但给出下一步可验证动作）"),
    ):
        items = [item for item in issues if item.severity == severity]
        lines.append(f"## {heading}")
        lines.append("")
        if not items:
            lines.append("无。")
            lines.append("")
            continue
        for number, item in enumerate(items, 1):
            lines.append(f"### {number}. {item.signal} — {item.case}")
            lines.append("")
            lines.append(f"- observed_pattern：{item.observed_pattern}")
            lines.append(f"- why_suspicious：{item.why_suspicious}")
            lines.append(f"- possible_non_bug_explanations：{item.possible_non_bug_explanations}")
            lines.append(f"- possible_bug_or_evidence_issues：{item.possible_bug_or_evidence_issues}")
            lines.append(f"- recommended_checks：{item.recommended_checks}")
            lines.append(f"- conclusion_policy：{item.conclusion_policy}")
            lines.append("")
    return "\n".join(lines)


def write_template(path: Path) -> None:
    template = {
        "schema_version": SCHEMA_VERSION,
        "summary": {
            "title": "example strict RVV-vs-RVV B/A summary",
            "evidence_role": "strict_ab",
            "summary_path": "test-rvv/<module>/<topic>/log/board/<run>/summary.md",
            "label": "strict A/B repeated board summary",
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": {
                "device": "<board-label>",
                "iterations": 20,
                "warmup_iterations": 5,
                "run_count": 5,
                "taskset": "taskset -c <cpu>",
                "governor": "performance",
                "freq": "fixed-or-recorded",
                "temperature": "recorded range",
                "vlen": "recorded VLEN",
                "binary_hash": "sha256:<binary>",
            },
        },
        "checks": {
            "strict_ab": True,
            "equal_fields": DEFAULT_EQUAL_FIELDS,
            "thresholds": DEFAULT_THRESHOLDS,
        },
        "comparisons": [
            {
                "name": "example full-cloud pointnormal 262144",
                "group": "example-full-cloud",
                "evidence_role": "strict_ab",
                "case_kind": "production-shaped",
                "point_type": "pointnormal",
                "size": 262144,
                "run_count": 5,
                "baseline": {
                    "label": "block-baseline",
                    "boundary": "test_support_full_estimate",
                    "wrapper": "same_outer_wrapper",
                    "row_source": "full_cloud",
                    "solve": True,
                    "checksum_policy": "matrix",
                    "checksum": "abc123",
                    "timer_boundary": "estimate_only",
                    "gate": "same_gate",
                    "mask": "same_mask",
                    "reduction": "same_reduction",
                    "asm_boundary": "target_hot_symbol",
                    "rvv_instr_count": 120,
                    "std_ms": 10.0,
                    "rvv_ms": 4.0,
                },
                "candidate": {
                    "label": "block-fused",
                    "boundary": "test_support_full_estimate",
                    "wrapper": "same_outer_wrapper",
                    "row_source": "full_cloud",
                    "solve": True,
                    "checksum_policy": "matrix",
                    "checksum": "abc123",
                    "timer_boundary": "estimate_only",
                    "gate": "same_gate",
                    "mask": "same_mask",
                    "reduction": "same_reduction",
                    "asm_boundary": "target_hot_symbol",
                    "rvv_instr_count": 110,
                    "std_ms": 9.8,
                    "rvv_ms": 3.6,
                },
                "ba_values": [1.10, 1.11, 1.09, 1.12, 1.10],
            }
        ],
    }
    path.write_text(json.dumps(template, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_json(path: Path, manifest: dict[str, Any], issues: list[Issue]) -> None:
    payload = {
        "schema_version": SCHEMA_VERSION,
        "summary": manifest.get("summary", {}),
        "result": {
            "errors": sum(1 for item in issues if item.severity == "ERROR"),
            "warnings": sum(1 for item in issues if item.severity == "WARNING"),
            "suggestions": sum(1 for item in issues if item.severity == "SUGGESTION"),
        },
        "issues": [asdict(item) for item in issues],
    }
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="RVV Evidence Doctor / Validator")
    parser.add_argument("--manifest", type=Path, help="JSON evidence manifest")
    parser.add_argument("--summary-md", type=Path, help="Markdown summary table for lightweight numeric checks")
    parser.add_argument("--output", type=Path, help="Write Markdown report to this path")
    parser.add_argument("--json-output", type=Path, help="Write structured JSON report to this path")
    parser.add_argument("--write-template", type=Path, help="Write an example JSON manifest and exit")
    parser.add_argument(
        "--fail-on",
        choices=("error", "warning", "never"),
        default="error",
        help="Exit non-zero on error or warning; default: error",
    )
    args = parser.parse_args()

    if args.write_template:
        write_template(args.write_template)
        print(f"wrote template: {args.write_template}")
        return 0

    manifest = load_manifest(args.manifest, args.summary_md)
    issues = run_checks(manifest)
    report = render_markdown(manifest, issues)

    if args.output:
        args.output.write_text(report.rstrip() + "\n", encoding="utf-8")
        print(f"wrote report: {args.output}")
    else:
        print(report)

    if args.json_output:
        write_json(args.json_output, manifest, issues)
        print(f"wrote json report: {args.json_output}")

    has_error = any(item.severity == "ERROR" for item in issues)
    has_warning = any(item.severity == "WARNING" for item in issues)
    if args.fail_on == "error" and has_error:
        return 2
    if args.fail_on == "warning" and (has_error or has_warning):
        return 2
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"错误: {exc}", file=sys.stderr)
        raise SystemExit(1)
