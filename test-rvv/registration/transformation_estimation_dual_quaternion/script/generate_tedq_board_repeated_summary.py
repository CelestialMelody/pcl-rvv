#!/usr/bin/env python3
"""
生成 transformation_estimation_dual_quaternion 的 board repeated summary（板卡重复测试摘要）。

输入是一组 `run-*` 目录，每个目录包含 Std/RVV bench log 和可选 analyze log。脚本输出：

- `summary.md`：给 reviewer 阅读的板卡重复摘要。
- `evidence_manifest.json`：给通用 Evidence Doctor（证据体检）读取的 manifest。

默认模式比较同一个 test-only RVV accumulation helper 的 Std/RVV 构建。它可以判断
ordered-cloud-pair C1/C2 accumulation candidate 在板端是否值得继续进入 PI1，但不能
单独证明 production dispatch（生产分发）已经可采纳。

`--production-public` 模式比较当前 production public entry 的 Std/RVV 构建。
可配合 `--row-source-policy` 分别抽取 ordered-cloud-pair、source-indexed-cloud-pair、
dual-indexed-cloud-pair 或 correspondence-pair。若 RVV 构建中接入了 production
dispatch，它可作为该 row-source policy 的 production direct（真实生产路径）证据。

`--component-ablation` 模式把 `component accum only` 写入 manifest，作为严格的 C1/C2
accumulation-only（只测累加前端）A/B；solve-only 和 public/helper 边界只写入 Markdown
summary，作为解释 Phase 001 / Phase 002 差异的 context（背景线索）。

`--row-source-policy` 模式只抽取指定 row source policy（行来源策略）的 staged ordered
candidate，用于在同一个 row-source bench filter 中分别登记 source-indexed、dual-indexed
或 correspondence 的板卡诊断证据。它不把其它 policy 的日志行混入当前 summary。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


CASE_RE = re.compile(r"^(?P<name>.+?)\s*:\s*(?P<ms>[0-9]+(?:\.[0-9]+)?)\s+ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*(?P<checksum>[0-9]+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<value>[0-9]+)\s*$")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<value>[0-9]+)\s*$")
DATASET_RE = re.compile(r"^Dataset:\s*(?P<value>.+?)\s*$")


def sha256_file(path: Path) -> str:
    if not path.is_file():
        return "not_recorded"
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def parse_log(path: Path) -> dict[str, Any]:
    cases: dict[str, dict[str, Any]] = {}
    metadata: dict[str, Any] = {}
    current_case: str | None = None

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
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None

    return {"path": str(path), "metadata": metadata, "cases": cases}


def run_dirs(root: Path) -> list[Path]:
    return sorted(p for p in root.iterdir() if p.is_dir() and p.name.startswith("run-"))


def collect_runs(root: Path, expected_runs: int) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    for directory in run_dirs(root):
        std_path = directory / "run_bench_std.log"
        rvv_path = directory / "run_bench_rvv.log"
        analyze_path = directory / "analyze_bench_compare.log"
        if not std_path.exists() or not rvv_path.exists():
            raise SystemExit(f"missing Std/RVV logs under {directory}")
        std = parse_log(std_path)
        rvv = parse_log(rvv_path)
        missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
        if missing:
            raise SystemExit(f"case set mismatch under {directory}: {', '.join(missing)}")
        runs.append(
            {
                "name": directory.name,
                "std": std,
                "rvv": rvv,
                "analyze": str(analyze_path) if analyze_path.exists() else "missing",
            }
        )
    if len(runs) != expected_runs:
        raise SystemExit(f"expected {expected_runs} run-* dirs under {root}, got {len(runs)}")
    return runs


def size_suffix(label: str) -> str:
    match = re.search(r"(\d+K)", label)
    if not match:
        raise ValueError(f"cannot infer size suffix from case label: {label}")
    return match.group(1)


def size_value(suffix: str) -> int:
    return int(suffix.rstrip("K")) * 1024


def mode_config(production_public: bool,
                component_ablation: bool,
                row_source_policy: str | None) -> dict[str, Any]:
    if production_public:
        row_source = row_source_policy or "ordered-cloud-pair"
        if row_source not in {
            "ordered-cloud-pair",
            "source-indexed-cloud-pair",
            "dual-indexed-cloud-pair",
            "correspondence-pair",
        }:
            raise SystemExit(f"unsupported production row-source policy: {row_source}")
        return {
            "case_prefix": "public dual quaternion",
            "case_middle": row_source,
            "comparison_prefix": "production public same-boundary",
            "comparison_middle": row_source,
            "group": "tedq_production_public_same_boundary",
            "case_kind": "production_direct",
            "comparison_role": "production_direct",
            "summary_role": "production_direct",
            "summary_label": f"production public {row_source}; production direct",
            "boundary": f"production_public_{row_source.replace('-', '_')}_current_entry",
            "row_source": row_source,
            "heading": f"production public {row_source} Std/RVV（同边界）",
            "description": (
                f"这一表比较同一 production {row_source} public entry 的 Std 与 RVV "
                "构建。RVV 构建命中当前 production dispatch 时，它用于判断该 "
                "row-source policy 是否满足生产证据门槛。"
            ),
            "notes": (
                f"B/A = Std public entry ms / RVV public entry ms for the same "
                f"production {row_source} overload. Interpret it with production "
                "path-hit, asm and correctness evidence from the same source state."
            ),
            "summary_boundary": (
                f"evidence_role：`production_direct`；当前摘要只覆盖 `{row_source}` "
                "public entry，其它 row-source policy 由同一 run 的独立 manifest "
                "分别登记。"
            ),
            "ba_definition": (
                f"`Std public {row_source} ms / RVV public {row_source} ms`，"
                "大于 1 表示 RVV public entry 更快。"
            ),
            "candidate_reduction": "rvv_build_public_entry",
            "candidate_asm_boundary": "production_public_entry_current_build",
            "gate": f"dense_{row_source.replace('-', '_')}_float_xyz_aos",
            "solve": True,
            "timer_boundary": "row_source_ingress_plus_c1_c2_accumulation_plus_eigen_4x4_solve",
            "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
            "decision_policy": (
                "- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 "
                "row-source policy 纳入 production gate。"
            ),
        }
    if row_source_policy:
        if row_source_policy not in {
            "source-indexed-cloud-pair",
            "dual-indexed-cloud-pair",
            "correspondence-pair",
        }:
            raise SystemExit(f"unsupported row-source policy: {row_source_policy}")
        return {
            "case_prefix": f"staged accum candidate {row_source_policy}",
            "case_middle": "",
            "comparison_prefix": f"staged accum {row_source_policy} same-boundary",
            "comparison_middle": "",
            "group": "tedq_row_source_staged_ordered_reuse",
            "case_kind": "row_source_diagnostic",
            "comparison_role": "strict_ab",
            "summary_role": "diagnostic",
            "summary_label": (
                f"{row_source_policy} staged ordered reuse; not production direct"
            ),
            "boundary": "test_support_staged_ordered_reuse",
            "row_source": row_source_policy,
            "heading": f"{row_source_policy} staged candidate Std/RVV（同边界）",
            "description": (
                f"这一表只比较 {row_source_policy} 将 row 展开并暂存为 ordered cloud "
                "后复用 C1/C2 RVV candidate 的 Std/RVV 构建。index / correspondence "
                "展开和 staging 成本包含在 bench 计时边界内；它仍是 test-rvv 诊断证据。"
            ),
            "notes": (
                f"B/A = Std staged {row_source_policy} ms / RVV staged "
                f"{row_source_policy} ms for the same test-support row-source adapter. "
                "This is row-source diagnostic evidence, not production direct evidence."
            ),
            "summary_boundary": (
                f"evidence_role：`diagnostic`；当前摘要只覆盖 `{row_source_policy}`，"
                "同边界 Std/RVV 对比不是 production direct（真实生产路径证据）。"
            ),
            "ba_definition": (
                f"`Std staged {row_source_policy} ms / RVV staged "
                f"{row_source_policy} ms`，大于 1 表示 RVV candidate 更快。"
            ),
            "candidate_reduction": "rvv_f64_tree",
            "candidate_asm_boundary": "bench_binary_staged_ordered_reuse",
            "gate": "dense_staged_ordered_cloud_float_xyz_aos",
            "solve": True,
            "timer_boundary": (
                "row_source_index_or_correspondence_expansion_plus_staging_plus_"
                "c1_c2_accumulation_plus_eigen_4x4_solve"
            ),
            "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
            "decision_policy": (
                "- 当前 positive / weak_positive 只批准该 row source 的诊断候选；"
                "其它 policy 不继承本摘要结论，也不能直接进入 production。"
            ),
        }
    if component_ablation:
        return {
            "case_prefix": "component accum only",
            "comparison_prefix": "component accum-only same-boundary",
            "group": "tedq_component_accum_only_same_boundary",
            "case_kind": "component_ablation",
            "comparison_role": "component_ablation",
            "summary_role": "component_ablation",
            "summary_label": "component accumulation-only ablation; not production direct",
            "boundary": "test_support_accumulation_only_component",
            "heading": "component accum-only Std/RVV（严格同边界）",
            "description": (
                "这一表只比较 C1/C2 accumulation-only component（只测累加前端组件）"
                "的 Std 与 RVV 构建，不包含 Eigen 4x4 solve 或矩阵构造。"
            ),
            "notes": (
                "B/A = Std accumulation-only ms / RVV accumulation-only ms for the same "
                "test-support component. This is component ablation evidence, not production direct evidence."
            ),
            "summary_boundary": (
                "evidence_role：`component_ablation`；严格表只覆盖 ordered-cloud-pair "
                "C1/C2 accumulation-only component，不覆盖 production public entry。"
            ),
            "ba_definition": "`Std component ms / RVV component ms`，大于 1 表示 RVV component 更快。",
            "candidate_reduction": "rvv_f64_tree",
            "candidate_asm_boundary": "bench_binary_component_accumulation",
            "solve": False,
            "timer_boundary": "c1_c2_accumulation_only",
            "checksum_policy": "accumulation_checksum_correctness_checked_by_gtest_budget",
            "decision_policy": (
                "- component strict A/B 的 `positive` / `weak_positive` 只能说明 C1/C2 前端仍有局部收益，"
                "不能替代 production direct。"
            ),
        }
    return {
        "case_prefix": "rvv accum candidate",
        "comparison_prefix": "rvv accum same-boundary",
        "group": "tedq_rvv_accum_same_boundary",
        "case_kind": "board_diagnostic_same_boundary",
        "comparison_role": "strict_ab",
        "summary_role": "diagnostic",
        "summary_label": "board repeated diagnostic; not production direct",
        "boundary": "test_support_rvv_accumulation_candidate",
        "heading": "rvv accum Std/RVV（同边界）",
        "description": (
            "这一表只比较同一 test-support ordered-cloud-pair helper 的 Std 与 RVV "
            "构建，用于判断 TEDQ C1/C2 accumulation candidate 是否值得进入 PI1。"
        ),
        "notes": (
            "B/A = Std helper ms / RVV helper ms for the same test-support ordered-cloud-pair "
            "candidate. This is board diagnostic evidence for PI1 readiness, not production direct evidence."
        ),
        "summary_boundary": (
            "evidence_role：`diagnostic`；同边界 Std/RVV helper 对比为 `strict_ab`，"
            "但不是 production direct（真实生产路径证据）。"
        ),
        "ba_definition": "`Std helper ms / RVV helper ms`，大于 1 表示 RVV helper 更快。",
        "candidate_reduction": "rvv_f64_tree",
        "candidate_asm_boundary": "bench_binary_test_support_helper",
        "solve": True,
        "timer_boundary": "c1_c2_accumulation_plus_eigen_4x4_solve",
        "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
        "decision_policy": (
            "- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback matrix 和 production bench。"
        ),
    }


def candidate_case(size: str, config: dict[str, str]) -> str:
    middle = config.get("case_middle", "ordered-cloud-pair")
    parts = [config["case_prefix"]]
    if middle:
        parts.append(middle)
    parts.append(size)
    return " ".join(parts)


def bucket_for(values: list[float]) -> str:
    if not values:
        return "blocked"
    med = statistics.median(values)
    if all(v > 1.15 for v in values):
        return "positive"
    if med >= 1.03 and min(values) >= 0.97:
        return "weak_positive"
    if all(0.97 <= v <= 1.03 for v in values):
        return "neutral"
    if med < 0.97 or min(values) < 0.97:
        return "negative"
    return "unstable"


def fmt_values(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}" for value in values)


def metric(values: list[float]) -> dict[str, Any]:
    return {
        "values": values,
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
        "bucket": bucket_for(values),
    }


def build_case_metrics(runs: list[dict[str, Any]], config: dict[str, str]) -> dict[str, dict[str, Any]]:
    sizes = sorted(
        {
            size_suffix(name)
            for name in runs[0]["std"]["cases"]
            if name.startswith(config["case_prefix"])
        },
        key=size_value,
    )
    if not sizes:
        raise SystemExit(f"no {config['case_prefix']} cases found")

    result: dict[str, dict[str, Any]] = {}
    for suffix in sizes:
        label = candidate_case(suffix, config)
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        std_checksums: set[str] = set()
        rvv_checksums: set[str] = set()
        for run in runs:
            std_case = run["std"]["cases"][label]
            rvv_case = run["rvv"]["cases"][label]
            std_ms = float(std_case["ms"])
            rvv_ms = float(rvv_case["ms"])
            ba = std_ms / rvv_ms
            values.append(ba)
            std_checksums.add(str(std_case.get("checksum")))
            rvv_checksums.add(str(rvv_case.get("checksum")))
            rows.append(
                {
                    "run": run["name"],
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    "ba": ba,
                }
            )
        result[suffix] = {
            "size": size_value(suffix),
            "same_boundary": metric(values),
            "runs": rows,
            "checksums": {
                "std": sorted(std_checksums),
                "rvv": sorted(rvv_checksums),
            },
        }
    return result


def overall_decision(case_metrics: dict[str, dict[str, Any]]) -> str:
    target = [info["same_boundary"] for suffix, info in case_metrics.items() if suffix != "4K"]
    if not target:
        target = [info["same_boundary"] for info in case_metrics.values()]
    buckets = {info["bucket"] for info in target}
    if buckets <= {"positive"}:
        return "positive"
    if buckets <= {"positive", "weak_positive"}:
        return "weak_positive"
    if "negative" in buckets:
        return "negative"
    if "unstable" in buckets:
        return "unstable"
    return "neutral"


def side(config: dict[str, str],
         reduction: str,
         asm_boundary: str,
         ms_key: str,
         ms_value: float,
         checksums: list[str]) -> dict[str, Any]:
    return {
        "boundary": config["boundary"],
        "wrapper": "bench_tedq",
        "row_source": config.get("row_source", "ordered-cloud-pair"),
        "solve": config.get("solve", True),
        "checksum_policy": config.get(
            "checksum_policy", "path_fingerprint_matrix_correctness_checked_by_gtest_budget"
        ),
        "checksum": checksums,
        "timer_boundary": config.get(
            "timer_boundary", "c1_c2_accumulation_plus_eigen_4x4_solve"
        ),
        "gate": config.get("gate", "dense_ordered_cloud_pair_float_xyz_aos"),
        "mask": "not_applicable_dense_only",
        "reduction": reduction,
        "asm_boundary": asm_boundary,
        ms_key: ms_value,
    }


def build_manifest(runs: list[dict[str, Any]],
                   case_metrics: dict[str, dict[str, Any]],
                   summary_path: Path,
                   args: argparse.Namespace,
                   config: dict[str, str]) -> dict[str, Any]:
    first_meta = runs[0]["std"]["metadata"]
    binary_hash = (
        f"std={sha256_file(Path(args.std_bin))}; rvv={sha256_file(Path(args.rvv_bin))}"
    )
    comparisons: list[dict[str, Any]] = []
    for suffix, info in case_metrics.items():
        metric_info = info["same_boundary"]
        std_med = statistics.median(row["std_ms"] for row in info["runs"])
        rvv_med = statistics.median(row["rvv_ms"] for row in info["runs"])
        comparisons.append(
            {
                "name": " ".join(
                    part
                    for part in (
                        config["comparison_prefix"],
                        config.get("comparison_middle", "ordered-cloud-pair"),
                        suffix,
                    )
                    if part
                ),
                "group": config["group"],
                "case_kind": config["case_kind"],
                "evidence_role": config["comparison_role"],
                "row_source": config.get("row_source", "ordered-cloud-pair"),
                "point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "run_count": len(runs),
                "ba_values": metric_info["values"],
                "decision_bucket": metric_info["bucket"],
                "std_ms": std_med,
                "rvv_ms": rvv_med,
                "allowed_contract_mismatches": [
                    "reduction",
                    "asm_boundary",
                ],
                "notes": config["notes"],
                "baseline": side(
                    config,
                    "scalar_same_chain",
                    "not_applicable_std_build",
                    "std_ms",
                    std_med,
                    info["checksums"]["std"],
                ),
                "candidate": side(
                    config,
                    config["candidate_reduction"],
                    config["candidate_asm_boundary"],
                    "rvv_ms",
                    rvv_med,
                    info["checksums"]["rvv"],
                ),
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": "transformation_estimation_dual_quaternion board repeated diagnostic",
            "evidence_role": config["summary_role"],
            "summary_path": str(summary_path),
            "label": config["summary_label"],
            "analysis_script": "test-rvv/registration/transformation_estimation_dual_quaternion/script/generate_tedq_board_repeated_summary.py",
            "metadata": {
                "device": args.device_label,
                "iterations": first_meta.get("iterations"),
                "warmup_iterations": first_meta.get("warmup_iterations"),
                "run_count": len(runs),
                "taskset": "not_pinned",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": args.vlen_desc,
                "binary_hash": binary_hash,
                "dataset": first_meta.get("dataset"),
                "decision_bucket": overall_decision(case_metrics),
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
            "thresholds": {
                "min_repeated_runs": args.expected_runs,
            },
        },
        "runs": [run["name"] for run in runs],
        "artifacts": {
            "input_dir": str(Path(args.input_dir)),
            "summary_path": str(summary_path),
            "std_bin": args.std_bin,
            "rvv_bin": args.rvv_bin,
        },
        "comparisons": comparisons,
    }


def checksum_cell(values: list[str]) -> str:
    if len(values) == 1:
        return values[0]
    return ", ".join(values)


def write_summary(path: Path,
                  runs: list[dict[str, Any]],
                  case_metrics: dict[str, dict[str, Any]],
                  manifest_path: Path,
                  doctor_path: Path,
                  args: argparse.Namespace,
                  config: dict[str, str]) -> None:
    lines = [
        "# transformation_estimation_dual_quaternion board repeated summary",
        "",
        "## 运行合同",
        "",
        f"- run_label：`{args.run_label}`",
        f"- runs：`{', '.join(run['name'] for run in runs)}`",
        f"- device：`{args.device_label}`",
        f"- case-filter：`{args.case_filter}`",
        f"- {config['summary_boundary']}",
        f"- iterations：`{runs[0]['std']['metadata'].get('iterations')}`",
        f"- warm-up iterations：`{runs[0]['std']['metadata'].get('warmup_iterations')}`",
        f"- B/A：{config['ba_definition']}",
        f"- manifest（默认本地证据清单）：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
        "",
        f"## {config['heading']}",
        "",
        config["description"],
        "",
        "| size | values | median | min | max | bucket | checksum |",
        "| --- | --- | ---: | ---: | ---: | --- | --- |",
    ]
    for suffix, info in case_metrics.items():
        m = info["same_boundary"]
        checksum_note = (
            "same"
            if info["checksums"]["std"] == info["checksums"]["rvv"]
            else f"Std `{checksum_cell(info['checksums']['std'])}` / RVV `{checksum_cell(info['checksums']['rvv'])}`"
        )
        lines.append(
            f"| {suffix} | `{fmt_values(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` | {checksum_note} |"
        )

    lines.extend(
        [
            "",
            "## 决策桶",
            "",
            f"- overall_decision_bucket：`{overall_decision(case_metrics)}`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。",
            "- `positive` / `weak_positive` 只能按当前 mode 的证据边界解释；indexed / correspondences 不从 ordered-cloud-pair 自动继承。",
            config.get(
                "decision_policy",
                "- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。",
            ),
            "",
        ]
    )

    if getattr(args, "component_ablation", False):
        append_component_context(lines, runs)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def append_context_table(lines: list[str],
                         title: str,
                         description: str,
                         case_metrics: dict[str, dict[str, Any]],
                         ba_definition: str) -> None:
    lines.extend(
        [
            "",
            f"## {title}",
            "",
            description,
            "",
            f"B/A：{ba_definition}",
            "",
            "| size | values | median | min | max | bucket | checksum |",
            "| --- | --- | ---: | ---: | ---: | --- | --- |",
        ]
    )
    for suffix, info in case_metrics.items():
        m = info["same_boundary"]
        checksum_note = (
            "same"
            if info["checksums"]["std"] == info["checksums"]["rvv"]
            else f"Std `{checksum_cell(info['checksums']['std'])}` / RVV `{checksum_cell(info['checksums']['rvv'])}`"
        )
        lines.append(
            f"| {suffix} | `{fmt_values(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` | {checksum_note} |"
        )


def build_context_metrics(runs: list[dict[str, Any]], case_prefix: str) -> dict[str, dict[str, Any]]:
    return build_case_metrics(
        runs,
        {
            "case_prefix": case_prefix,
        },
    )


def cross_boundary_rows(runs: list[dict[str, Any]]) -> list[dict[str, Any]]:
    sizes = sorted(
        {
            size_suffix(name)
            for name in runs[0]["std"]["cases"]
            if name.startswith("public dual quaternion")
        },
        key=size_value,
    )
    rows: list[dict[str, Any]] = []
    for suffix in sizes:
        public_label = f"public dual quaternion ordered-cloud-pair {suffix}"
        helper_label = f"rvv accum candidate ordered-cloud-pair {suffix}"
        std_values: list[float] = []
        rvv_values: list[float] = []
        for run in runs:
            std_public = float(run["std"]["cases"][public_label]["ms"])
            std_helper = float(run["std"]["cases"][helper_label]["ms"])
            rvv_public = float(run["rvv"]["cases"][public_label]["ms"])
            rvv_helper = float(run["rvv"]["cases"][helper_label]["ms"])
            std_values.append(std_public / std_helper)
            rvv_values.append(rvv_public / rvv_helper)
        rows.append(
            {
                "size": suffix,
                "std": metric(std_values),
                "rvv": metric(rvv_values),
            }
        )
    return rows


def append_component_context(lines: list[str], runs: list[dict[str, Any]]) -> None:
    append_context_table(
        lines,
        "helper full-estimate context（同边界）",
        "这一表复查 test-support helper full-estimate（C1/C2 累加 + Eigen solve）的 Std/RVV 构建差异，作为 Phase 001 的同 run context。",
        build_context_metrics(runs, "rvv accum candidate"),
        "`Std helper full-estimate ms / RVV helper full-estimate ms`，大于 1 表示 RVV helper 更快。",
    )
    append_context_table(
        lines,
        "public entry context（同入口构建对比）",
        "这一表只比较当前源码状态下 public ordered-cloud-pair entry 的 Std/RVV 构建。当前没有 retained production dispatch，因此它不是 production speedup 证据。",
        build_context_metrics(runs, "public dual quaternion"),
        "`Std public entry ms / RVV public entry ms`，大于 1 表示 RVV 构建下 public entry 更快。",
    )
    append_context_table(
        lines,
        "solve-only context（预计算累加后段）",
        "这一表只测用预计算 C1/C2 accumulation 输入 Eigen solve 和矩阵构造的后段成本；预期不含 RVV 前端。",
        build_context_metrics(runs, "component solve only"),
        "`Std solve-only ms / RVV solve-only ms`，接近 1 表示后段不是 RVV 受益点。",
    )

    lines.extend(
        [
            "",
            "## public/helper mixed-boundary cross-check",
            "",
            "这一表是 mixed-boundary cross-check（混合边界交叉检查）：`public / helper` 大于 1 表示 public entry 比 test-support helper 更慢。它只帮助解释 wrapper / baseline 差异，不能作为严格 A/B。",
            "",
            "| size | Std public/helper median | Std min | Std max | RVV public/helper median | RVV min | RVV max |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in cross_boundary_rows(runs):
        std = row["std"]
        rvv = row["rvv"]
        lines.append(
            f"| {row['size']} | {std['median']:.3f} | {std['min']:.3f} | {std['max']:.3f} | {rvv['median']:.3f} | {rvv['min']:.3f} | {rvv['max']:.3f} |"
        )
    lines.append("")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", default="log/board/rvv_accum_full_cloud_repeated")
    parser.add_argument("--output-dir", default="log/board/rvv_accum_full_cloud_repeated")
    parser.add_argument("--run-label", default="rvv_accum_full_cloud_repeated")
    parser.add_argument("--case-filter", default="ordered-cloud-pair")
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--std-bin", default="build/riscv/bench_transformation_estimation_dual_quaternion_std")
    parser.add_argument("--rvv-bin", default="build/riscv/bench_transformation_estimation_dual_quaternion_rvv")
    parser.add_argument("--device-label", default="RVV board")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    parser.add_argument("--production-public", action="store_true")
    parser.add_argument("--component-ablation", action="store_true")
    parser.add_argument(
        "--row-source-policy",
        choices=[
            "ordered-cloud-pair",
            "source-indexed-cloud-pair",
            "dual-indexed-cloud-pair",
            "correspondence-pair",
        ],
    )
    args = parser.parse_args()

    if args.component_ablation and (args.production_public or args.row_source_policy):
        raise SystemExit(
            "--component-ablation cannot be combined with --production-public or --row-source-policy"
        )

    config = mode_config(
        args.production_public,
        args.component_ablation,
        args.row_source_policy,
    )
    output_dir = Path(args.output_dir)
    summary_path = output_dir / "summary.md"
    manifest_path = output_dir / "evidence_manifest.json"
    doctor_path = output_dir / "evidence_doctor.md"
    runs = collect_runs(Path(args.input_dir), args.expected_runs)
    case_metrics = build_case_metrics(runs, config)
    manifest = build_manifest(runs, case_metrics, summary_path, args, config)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_summary(summary_path, runs, case_metrics, manifest_path, doctor_path, args, config)
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
