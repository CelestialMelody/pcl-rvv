#!/usr/bin/env python3
"""Generate the GrabCut board Evidence Doctor manifest.

这个脚本只理解当前 topic 的 bench log 格式，把 `BENCH grabcut_component`
行翻译成全局 Evidence Doctor（证据体检）能读取的规范字段。它不尝试从
case 名猜生产语义；新增 case 必须先进入 CASE_LABELS，否则脚本失败退出。
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BOARD_LOG_DIR = TOPIC_ROOT / "log" / "board"
DEFAULT_MANIFEST = DEFAULT_BOARD_LOG_DIR / "evidence_manifest.json"

BENCH_RE = re.compile(r"^BENCH\s+(?P<kind>\S+)\s+(?P<fields>.+)$")

CASE_LABELS: dict[str, dict[str, Any]] = {
    "organized_nlinks": {
        "case_kind": "component_ablation",
        "evidence_role": "diagnostic",
        "point_type": "organized_rgb_pixel",
        "row_source": "organized_image_grid",
        "size": "320x240",
        "boundary": "test_support_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "component_loop_only",
        "gate": "organized_width_height_nonzero",
        "mask": "not_applicable_no_finite_mask",
        "reduction": "local_neighbor_formula_no_reduction",
        "group": "grabcut_component",
        "decision_hint": "weak_or_negative_diagnostic",
        "notes": (
            "该 case 只覆盖测试支撑里的 organized n-link 组件计算，"
            "不覆盖真实 public entry、完整图构建或 max-flow/min-cut 求解。"
        ),
    },
    "gmm_probability": {
        "case_kind": "component_ablation",
        "evidence_role": "diagnostic",
        "point_type": "bgr_float_sample",
        "row_source": "synthetic_pixel_samples",
        "size": "320x240",
        "boundary": "test_support_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "component_loop_only",
        "gate": "finite_bgr_and_positive_covariance_inverse",
        "mask": "finite_domain_exp_input",
        "reduction": "per_pixel_min_probability_scalar_tail",
        "group": "grabcut_component",
        "decision_hint": "positive_diagnostic",
        "notes": (
            "该 case 覆盖 GrabCut GMM probability density（高斯混合模型概率密度）"
            "批量计算的诊断 helper；checksum 使用输入指纹加误差预算，"
            "同构正确性由 gtest 的绝对 / 相对误差阈值承担。"
        ),
    },
    "terminal_weights": {
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "diagnostic",
        "point_type": "bgr_float_sample",
        "row_source": "synthetic_pixel_samples",
        "size": "320x240",
        "boundary": "test_support_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "terminal_weight_loop_only_no_graph_mutation",
        "gate": "finite_bgr_positive_covariance_inverse_and_positive_mixture_probability",
        "mask": "finite_domain_exp_input",
        "reduction": "k5_pi_weighted_mixture_scalar_sum_plus_scalar_log",
        "group": "grabcut_component",
        "decision_hint": "public_shaped_probe",
        "notes": (
            "该 case 复刻 GrabCut initGraph 中 unknown trimap 的 terminal weight（端点权重）"
            "公式：K=5 GMM probabilityDensity(c) 加权求和后执行 -log；"
            "不包含 setTerminalWeights、n-link graph edge mutation 或 max-flow 求解。"
        ),
    },
    "initgraph_no_solve": {
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "diagnostic",
        "point_type": "bgr_float_sample",
        "row_source": "synthetic_pixel_samples",
        "size": "320x240",
        "boundary": "test_support_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "terminal_weight_loop_plus_terminal_write_sink_no_graph_solve",
        "gate": "finite_bgr_positive_covariance_inverse_positive_mixture_probability_and_nonempty_sink",
        "mask": "finite_domain_exp_input",
        "reduction": "k5_pi_weighted_mixture_scalar_sum_plus_scalar_log_plus_terminal_write_sink",
        "group": "grabcut_component",
        "decision_hint": "initgraph_no_solve_probe",
        "notes": (
            "该 case 在 terminal_weights 的 K=5 GMM mixture（混合概率）和 terminal -log 之后，"
            "追加轻量 terminal write sink（端点权重写入消耗点），用于观察收益在更接近 "
            "initGraph no-solve（不含求解）边界时是否被稀释；它仍不包含真实 graph edge "
            "mutation、trimap 固定标签分支或 max-flow 求解。"
        ),
    },
    "production_initgraph_terminal": {
        "case_kind": "production_direct",
        "evidence_role": "production-detail",
        "point_type": "PointXYZRGB_via_Image_Color",
        "row_source": "indices_ordered_unknown_trimap",
        "size": "320x240",
        "boundary": "production_detail_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "real_initGraph_terminal_weights_no_nlink_edges_no_solve",
        "gate": "rvv_build_unknown_trimap_count_at_least_two",
        "mask": "unknown_trimap_batch_collected_before_terminal_write",
        "reduction": "k5_pi_weighted_mixture_scalar_log_real_setTerminalWeights",
        "group": "grabcut_component",
        "decision_hint": "production_terminal_probe",
        "notes": (
            "该 case 通过测试专用子类调用 production `GrabCut<PointXYZRGB>::initGraph()`，"
            "真实写入 Boykov-Kolmogorov graph terminal capacities（图端点容量），"
            "但 n-link 数量为 0，仍不包含 max-flow 求解或完整 `extract/refineOnce` wall time。"
        ),
    },
    "public_extract": {
        "case_kind": "production_direct",
        "evidence_role": "production-public",
        "point_type": "PointXYZRGB",
        "row_source": "organized_image_grid_with_central_unknown_trimap",
        "size": "320x240",
        "boundary": "production_public_extract",
        "wrapper": "grabcut_component_bench",
        "solve": True,
        "timer_boundary": "setBackgroundPointsIndices_plus_extract_full_refine_loop",
        "gate": "organized_rgb_cloud_with_nonempty_background_and_unknown_sets",
        "mask": "production_trimap_and_graphcut_state",
        "reduction": "full_pipeline_terminal_helper_plus_scalar_nlinks_gmm_learn_and_maxflow",
        "group": "grabcut_component",
        "decision_hint": "production_public_wall_time",
        "notes": (
            "该 case 构造 organized `PointXYZRGB` 点云，调用真实 "
            "`GrabCut<PointXYZRGB>::setBackgroundPointsIndices()` 和 `extract()`。"
            "它用于观察已采纳 terminal weight RVV helper 在完整 public wall-time（公开入口总耗时）"
            "中是否仍有可见收益；结果不能单独归因到 n-link、GMM learn 或 max-flow。"
        ),
    },
    "public_extract_profile": {
        "case_kind": "component_profile",
        "evidence_role": "diagnostic-profile",
        "point_type": "PointXYZRGB",
        "row_source": "organized_image_grid_with_central_unknown_trimap",
        "size": "96x72",
        "boundary": "test_only_public_shaped_profile_wrapper",
        "wrapper": "GrabCutBenchAccess::runPublicExtractProfile",
        "solve": True,
        "timer_boundary": "split_setBackgroundPointsIndices_plus_extract_profile_components",
        "gate": "organized_rgb_cloud_with_nonempty_background_and_unknown_sets",
        "mask": "production_trimap_and_graphcut_state",
        "reduction": "profile_only_component_timing_not_a_new_rvv_family",
        "group": "grabcut_profile",
        "decision_hint": "profile_before_new_production_family",
        "notes": (
            "该 case 使用测试专用子类拆分 public `setBackgroundPointsIndices()` + `extract()` "
            "中的 initCompute、GMM build/learn、initGraph、graph solve 和 output cluster 组件。"
            "它只用于定位下一候选，不能替代 production-public 证据，也不能直接采纳新 RVV family。"
        ),
    },
    "learn_gmm_assignment": {
        "case_kind": "component_ablation",
        "evidence_role": "diagnostic",
        "point_type": "PointXYZRGB_like_float_color",
        "row_source": "organized_image_grid_with_foreground_background_mask",
        "size": "96x72",
        "boundary": "test_support_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "learnGMMs_component_assignment_only_no_gaussian_fitter_relearn",
        "gate": "finite_bgr_positive_covariance_inverse_and_nonempty_foreground_background_groups",
        "mask": "foreground_background_hard_segmentation_mask",
        "reduction": "k5_per_pixel_max_component_selection_with_test_only_staging",
        "group": "grabcut_component",
        "decision_hint": "learn_gmm_assignment_probe",
        "notes": (
            "该 case 只覆盖 `learnGMMs()` 第一段 component assignment（分量归属选择）："
            "每个像素根据当前 hard segmentation（硬分割状态）选择 foreground 或 background GMM，"
            "再从 K=5 Gaussian 中选概率最大的 component。它不包含 GaussianFitter 重新累加、"
            "GMM 参数更新、initGraph 或 max-flow 求解。"
        ),
    },
    "learn_gmms_full": {
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production-shaped diagnostic",
        "point_type": "PointXYZRGB_like_float_color",
        "row_source": "organized_image_grid_with_foreground_background_mask",
        "size": "96x72",
        "boundary": "test_support_helper",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "learnGMMs_assignment_plus_gaussian_fitter_relearn_no_initgraph_no_solve",
        "gate": "finite_bgr_positive_covariance_inverse_and_nonempty_foreground_background_groups",
        "mask": "foreground_background_hard_segmentation_mask",
        "reduction": "k5_per_pixel_max_component_selection_then_scalar_bucket_accumulation",
        "group": "grabcut_component",
        "decision_hint": "full_learn_gmms_probe",
        "notes": (
            "该 case 覆盖完整 `learnGMMs()` 局部函数形态：先执行 component assignment（分量归属选择），"
            "再按 component vector 重新累加 foreground/background GaussianFitter 并更新 GMM 参数。"
            "它不包含 initGraph、max-flow 或 public `extract()` wall time；若结果为 positive，"
            "仍只能作为后续 production integration loop 的前置证据。"
        ),
    },
    "production_learn_gmms": {
        "case_kind": "production_direct",
        "evidence_role": "production-detail",
        "point_type": "PointXYZRGB_via_Image_Color",
        "row_source": "ordered_indices_over_Image_Color",
        "size": "96x72",
        "boundary": "production_free_function",
        "wrapper": "grabcut_component_bench",
        "solve": False,
        "timer_boundary": "real_learnGMMs_assignment_plus_gaussian_fitter_relearn_no_initgraph_no_solve",
        "gate": "rvv_build_indices_size_at_least_two",
        "mask": "foreground_background_hard_segmentation_mask",
        "reduction": "k5_per_pixel_max_component_selection_then_scalar_bucket_accumulation",
        "group": "grabcut_component",
        "decision_hint": "production_learn_gmms_probe",
        "notes": (
            "该 case 直接调用 production `pcl::segmentation::grabcut::learnGMMs()`，"
            "覆盖真实 production dispatch（生产分流）和小规模以外的 RVV assignment path。"
            "它不包含 initGraph、max-flow 或 public `extract()` wall time；若结果为 positive，"
            "仍需 PI5 用户确认后才能视为 adopted production behavior（已采用生产行为）。"
        ),
    },
}


def _parse_key_values(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for match in re.finditer(r"(\w+)=([^\s]+)", text):
        fields[match.group(1)] = match.group(2)
    return fields


def _parse_log(path: Path) -> dict[str, dict[str, Any]]:
    if not path.exists():
        raise FileNotFoundError(path)

    rows: dict[str, dict[str, Any]] = {}
    with path.open("r", encoding="utf-8", errors="replace") as fh:
        for raw_line in fh:
            match = BENCH_RE.match(raw_line.strip())
            if not match:
                continue
            if match.group("kind") != "grabcut_component":
                continue
            fields = _parse_key_values(match.group("fields"))
            case = fields.get("case")
            bench_path = fields.get("path")
            if not case or not bench_path:
                continue
            if case not in CASE_LABELS:
                raise ValueError(f"未知 case label：{case!r}；请先补 CASE_LABELS")
            if bench_path not in {"reference", "rvv_candidate"}:
                raise ValueError(f"未知 bench path：{bench_path!r} in {path}")
            rows[case] = {
                **fields,
                "bench_kind": match.group("kind"),
                "log_path": _repo_relative(path),
            }
    return rows


def _repo_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(TOPIC_ROOT.parents[2].resolve()))
    except ValueError:
        return str(path)


def _float_field(row: dict[str, Any], key: str) -> float | None:
    value = row.get(key)
    if value is None:
        return None
    return float(value)


def _int_field(row: dict[str, Any], key: str) -> int | None:
    value = row.get(key)
    if value is None:
        return None
    return int(value)


def _median(values: list[float]) -> float:
    sorted_values = sorted(values)
    mid = len(sorted_values) // 2
    if len(sorted_values) % 2:
        return sorted_values[mid]
    return (sorted_values[mid - 1] + sorted_values[mid]) / 2.0


def _side(label: str, row: dict[str, Any], meta: dict[str, Any]) -> dict[str, Any]:
    return {
        "label": label,
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": meta["row_source"],
        "solve": meta["solve"],
        "checksum_policy": row.get("checksum_policy", "not_recorded"),
        "checksum": row.get("checksum", "not_recorded"),
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": meta["mask"],
        "reduction": meta["reduction"],
        "asm_boundary": (
            "grabcut_diag::computeGMMProbabilityCandidate"
            if meta.get("decision_hint") == "positive_diagnostic" and label == "rvv_candidate"
            else "grabcut_diag::computeTerminalWeightsCandidate"
            if meta.get("decision_hint") == "public_shaped_probe" and label == "rvv_candidate"
            else "inlined grabcut_diag::computeInitGraphNoSolveCandidate via computeTerminalWeightsCandidate/computeGMMProbabilityCandidate"
            if meta.get("decision_hint") == "initgraph_no_solve_probe" and label == "rvv_candidate"
            else "pcl::GrabCut<PointXYZRGB>::initGraphTerminalWeightsRVV"
            if meta.get("decision_hint") == "production_terminal_probe" and label == "rvv_candidate"
            else "pcl::GrabCut<PointXYZRGB>::extract via initGraphTerminalWeightsRVV"
            if meta.get("decision_hint") == "production_public_wall_time" and label == "rvv_candidate"
            else "test-only public-shaped profile wrapper via initGraphTerminalWeightsRVV"
            if meta.get("decision_hint") == "profile_before_new_production_family" and label == "rvv_candidate"
            else "test_support_helper_or_scalar_reference"
        ),
        "std_ms": _float_field(row, "avg_ms") if label == "reference" else None,
        "rvv_ms": _float_field(row, "avg_ms") if label == "rvv_candidate" else None,
        "source_log": row.get("log_path"),
    }


def build_manifest(std_log: Path, rvv_log: Path, output: Path, case_filter: str) -> dict[str, Any]:
    std_rows = _parse_log(std_log)
    rvv_rows = _parse_log(rvv_log)
    missing = sorted(set(std_rows) ^ set(rvv_rows))
    if missing:
        raise ValueError(f"Std/RVV log case 不一致：{', '.join(missing)}")

    selected_cases = sorted(std_rows)
    if case_filter != "all":
        if case_filter not in std_rows:
            raise ValueError(f"case 不存在于 Std/RVV 日志：{case_filter!r}")
        selected_cases = [case_filter]

    comparisons: list[dict[str, Any]] = []
    for case in selected_cases:
        meta = CASE_LABELS[case]
        std_row = std_rows[case]
        rvv_row = rvv_rows[case]
        std_ms = _float_field(std_row, "avg_ms")
        rvv_ms = _float_field(rvv_row, "avg_ms")
        if std_ms is None or rvv_ms is None or rvv_ms <= 0:
            raise ValueError(f"{case}: avg_ms 缺失或非法")

        iterations = _int_field(std_row, "iterations")
        warmup = _int_field(std_row, "warmup")
        if iterations != _int_field(rvv_row, "iterations") or warmup != _int_field(rvv_row, "warmup"):
            raise ValueError(f"{case}: Std/RVV iterations 或 warmup 不一致")

        comparison: dict[str, Any] = {
            "name": case,
            "case_kind": meta["case_kind"],
            "evidence_role": meta["evidence_role"],
            "point_type": meta["point_type"],
            "row_source": meta["row_source"],
            "size": f"{std_row.get('width', 'unknown')}x{std_row.get('height', 'unknown')}",
            "group": meta["group"],
            "run_count": 1,
            "iterations": iterations,
            "warmup_iterations": warmup,
            "std_ms": std_ms,
            "rvv_ms": rvv_ms,
            "ba_values": [std_ms / rvv_ms],
            "baseline": _side("reference", std_row, meta),
            "candidate": _side("rvv_candidate", rvv_row, meta),
            "metadata": {
                "width": _int_field(std_row, "width"),
                "height": _int_field(std_row, "height"),
                "decision_hint": meta["decision_hint"],
                "notes": meta["notes"],
            },
            "requires_board_context": True,
        }

        for key in (
            "max_abs_error",
            "max_rel_error",
            "error_budget_abs",
            "error_budget_rel",
            "max_budget_ratio",
        ):
            if key in rvv_row:
                comparison["metadata"][key] = float(rvv_row[key])
        comparisons.append(comparison)

    summary_evidence_role = "mixed" if len(comparisons) != 1 else comparisons[0]["evidence_role"]
    metadata = {
        "device": "Milkv-Jupiter",
        "iterations": _int_field(next(iter(std_rows.values())), "iterations"),
        "warmup_iterations": _int_field(next(iter(std_rows.values())), "warmup"),
        "run_count": 1,
        "taskset": "not_recorded",
        "governor": "not_recorded",
        "freq": "not_recorded",
        "temperature": "not_recorded",
        "vlen": "see SoC / ELF (board)",
        "binary_hash": "not_recorded",
    }
    return {
        "schema_version": 1,
        "summary": {
            "title": "GrabCut board evidence manifest",
            "evidence_role": summary_evidence_role,
            "summary_path": _repo_relative(output),
            "label": f"segmentation/grabcut_segmentation board evidence case={case_filter}",
            "metadata": metadata,
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
            "thresholds": {
                "min_repeated_runs": 5,
                "high_degrade_fraction": 0.20,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": comparisons,
    }


def build_repeated_manifest(repeated_dir: Path, output: Path, case_filter: str) -> dict[str, Any]:
    if case_filter == "all":
        raise ValueError("--repeated-dir requires a specific --case")
    if case_filter not in CASE_LABELS:
        raise ValueError(f"未知 case label：{case_filter!r}")

    std_logs = sorted(repeated_dir.glob("run*_std.log"))
    if not std_logs:
        raise ValueError(f"未找到 repeated std logs: {repeated_dir}/run*_std.log")

    rows: list[tuple[dict[str, Any], dict[str, Any]]] = []
    for std_log in std_logs:
        rvv_log = std_log.with_name(std_log.name.replace("_std.log", "_rvv.log"))
        if not rvv_log.exists():
            raise ValueError(f"缺少匹配 RVV log：{rvv_log}")
        std_rows = _parse_log(std_log)
        rvv_rows = _parse_log(rvv_log)
        if case_filter not in std_rows or case_filter not in rvv_rows:
            raise ValueError(f"{std_log.name}/{rvv_log.name} 中缺少 case={case_filter}")
        rows.append((std_rows[case_filter], rvv_rows[case_filter]))

    meta = CASE_LABELS[case_filter]
    std_values: list[float] = []
    rvv_values: list[float] = []
    ba_values: list[float] = []
    checksums_std: set[str] = set()
    checksums_rvv: set[str] = set()
    source_logs: list[dict[str, str]] = []
    iterations = _int_field(rows[0][0], "iterations")
    warmup = _int_field(rows[0][0], "warmup")
    width = _int_field(rows[0][0], "width")
    height = _int_field(rows[0][0], "height")
    max_abs_error = 0.0
    max_rel_error = 0.0
    max_budget_ratio = 0.0

    for std_row, rvv_row in rows:
        if _int_field(std_row, "iterations") != iterations or _int_field(rvv_row, "iterations") != iterations:
            raise ValueError("repeated logs 的 iterations 不一致")
        if _int_field(std_row, "warmup") != warmup or _int_field(rvv_row, "warmup") != warmup:
            raise ValueError("repeated logs 的 warmup 不一致")
        if _int_field(std_row, "width") != width or _int_field(rvv_row, "width") != width:
            raise ValueError("repeated logs 的 width 不一致")
        if _int_field(std_row, "height") != height or _int_field(rvv_row, "height") != height:
            raise ValueError("repeated logs 的 height 不一致")
        std_ms = _float_field(std_row, "avg_ms")
        rvv_ms = _float_field(rvv_row, "avg_ms")
        if std_ms is None or rvv_ms is None or rvv_ms <= 0.0:
            raise ValueError("repeated logs 缺少合法 avg_ms")
        std_values.append(std_ms)
        rvv_values.append(rvv_ms)
        ba_values.append(std_ms / rvv_ms)
        checksums_std.add(str(std_row.get("checksum", "not_recorded")))
        checksums_rvv.add(str(rvv_row.get("checksum", "not_recorded")))
        source_logs.append({"std": std_row["log_path"], "rvv": rvv_row["log_path"]})
        max_abs_error = max(max_abs_error, _float_field(rvv_row, "max_abs_error") or 0.0)
        max_rel_error = max(max_rel_error, _float_field(rvv_row, "max_rel_error") or 0.0)
        max_budget_ratio = max(max_budget_ratio, _float_field(rvv_row, "max_budget_ratio") or 0.0)

    comparison = {
        "name": case_filter,
        "case_kind": meta["case_kind"],
        "evidence_role": meta["evidence_role"],
        "point_type": meta["point_type"],
        "row_source": meta["row_source"],
        "size": f"{width}x{height}",
        "group": meta["group"],
        "run_count": len(rows),
        "iterations": iterations,
        "warmup_iterations": warmup,
        "std_ms": _median(std_values),
        "rvv_ms": _median(rvv_values),
        "ba_values": ba_values,
        "baseline": {
            **_side("reference", rows[-1][0], meta),
            "checksum": sorted(checksums_std),
            "source_log": [item["std"] for item in source_logs],
        },
        "candidate": {
            **_side("rvv_candidate", rows[-1][1], meta),
            "checksum": sorted(checksums_rvv),
            "source_log": [item["rvv"] for item in source_logs],
        },
        "metadata": {
            "width": width,
            "height": height,
            "decision_hint": meta["decision_hint"],
            "notes": meta["notes"],
            "std_values_ms": std_values,
            "rvv_values_ms": rvv_values,
            "max_abs_error": max_abs_error,
            "max_rel_error": max_rel_error,
            "max_budget_ratio": max_budget_ratio,
        },
        "requires_board_context": True,
    }
    summary_evidence_role = comparison["evidence_role"]
    metadata = {
        "device": "Milkv-Jupiter",
        "iterations": iterations,
        "warmup_iterations": warmup,
        "run_count": len(rows),
        "taskset": "not_recorded",
        "governor": "not_recorded",
        "freq": "not_recorded",
        "temperature": "not_recorded",
        "vlen": "see SoC / ELF (board)",
        "binary_hash": "not_recorded",
        "repeated_dir": _repo_relative(repeated_dir),
    }
    return {
        "schema_version": 1,
        "summary": {
            "title": "GrabCut repeated board evidence manifest",
            "evidence_role": summary_evidence_role,
            "summary_path": _repo_relative(output),
            "label": f"segmentation/grabcut_segmentation repeated board evidence case={case_filter}",
            "metadata": metadata,
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
            "thresholds": {
                "min_repeated_runs": 5,
                "high_degrade_fraction": 0.20,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": [comparison],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--std-log", type=Path, default=DEFAULT_BOARD_LOG_DIR / "run_bench_std.log")
    parser.add_argument("--rvv-log", type=Path, default=DEFAULT_BOARD_LOG_DIR / "run_bench_rvv.log")
    parser.add_argument("--repeated-dir", type=Path, help="读取 runNN_std.log/runNN_rvv.log 聚合 repeated manifest")
    parser.add_argument("--output", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--case",
        choices=sorted(["all", *CASE_LABELS.keys()]),
        default="all",
        help="只生成指定 bench case 的 manifest；默认包含全部已知 case。",
    )
    args = parser.parse_args()

    if args.repeated_dir:
        manifest = build_repeated_manifest(args.repeated_dir, args.output, args.case)
    else:
        manifest = build_manifest(args.std_log, args.rvv_log, args.output, args.case)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote manifest: {_repo_relative(args.output)}")
    print(f"comparisons: {len(manifest['comparisons'])}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"错误: {exc}", file=sys.stderr)
        raise SystemExit(1)
