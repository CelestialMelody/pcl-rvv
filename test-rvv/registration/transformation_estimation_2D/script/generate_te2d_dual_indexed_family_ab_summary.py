#!/usr/bin/env python3
"""生成 transformation_estimation_2D indexed / correspondence family A/B 摘要。

本脚本比较同一 RVV bench binary 中的两个 row-source family：

- materialize selected source / target rows，再调用真实 ordered-cloud-pair public overload；
- 当前有界 production probe 的 row-source public direct gather overload。

QEMU 日志只用于 smoke；真实性能结论必须来自板卡 repeated run。这里的
`B/A = materialize_ordered_public_rvv_ms / direct_row_source_public_rvv_ms`，
大于 1 表示当前 direct gather family 更快。
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
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")

FAMILY_CONFIGS: dict[str, dict[str, str]] = {
    "dual-indexed-family-ab": {
        "display_name": "dual-indexed",
        "title_name": "Dual-indexed",
        "materialize_prefix": "materialize-ordered public 2D dual-indexed-cloud-pair",
        "direct_prefix": "direct dual-indexed public 2D dual-indexed-cloud-pair",
        "row_source": "dual_indexed_cloud_pair",
        "group": "te2d_dual_indexed_family_ab",
        "title": "transformation_estimation_2D dual-indexed production-detail family A/B",
        "label": "dual-indexed materialize ordered public vs direct public; RVV-vs-RVV family selection",
        "materialize_boundary": "dual_indexed_materialize_source_rows_plus_production_public_ordered_cloud_pair",
        "direct_boundary": "production_public_dual_indexed_direct_gather",
        "materialize_asm_boundary": "production_public_ordered_cloud_pair_or_family_ab_materialize_boundary",
        "direct_asm_boundary": "production_public_dual_indexed_boundary_or_family_ab_direct_boundary",
        "timer_boundary": "dual_indexed_family_ingress_plus_two_pass_centered_correlation_plus_2d_solve",
        "gate": "pointxyz_float_valid_source_target_indices_dense_selected_finite_size_ge_16",
        "row_semantics": "dual-indexed row semantics",
        "direct_overload": "Phase 093 dual-indexed public overload",
        "not_extend": "generic point type、source-indexed 或 correspondence",
    },
    "correspondence-family-ab": {
        "display_name": "correspondence",
        "title_name": "Correspondence",
        "materialize_prefix": "materialize-ordered public 2D correspondence-pair",
        "direct_prefix": "direct correspondence public 2D correspondence-pair",
        "row_source": "correspondence_pair",
        "group": "te2d_correspondence_family_ab",
        "title": "transformation_estimation_2D correspondence production-detail family A/B",
        "label": "correspondence materialize ordered public vs direct public; RVV-vs-RVV family selection",
        "materialize_boundary": "correspondence_materialize_query_match_rows_plus_production_public_ordered_cloud_pair",
        "direct_boundary": "production_public_correspondence_direct_gather",
        "materialize_asm_boundary": "production_public_ordered_cloud_pair_or_correspondence_family_ab_materialize_boundary",
        "direct_asm_boundary": "production_public_correspondence_boundary_or_family_ab_direct_boundary",
        "timer_boundary": "correspondence_family_ingress_plus_two_pass_centered_correlation_plus_2d_solve",
        "gate": "pointxyz_float_valid_correspondences_dense_selected_finite_size_ge_16",
        "row_semantics": "correspondence query/match row semantics",
        "direct_overload": "Phase 094 correspondence public overload",
        "not_extend": "generic point type、source-indexed 或 dual-indexed",
    },
}

FAMILY_CONFIG = FAMILY_CONFIGS["dual-indexed-family-ab"]


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
        if match := BUILD_RE.match(line):
            metadata["build"] = match.group("value")
            continue
        if match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
            continue
        if current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None
    return {"path": str(path), "metadata": metadata, "cases": cases}


def collect_runs(root: Path, expected_runs: int) -> list[dict[str, Any]]:
    runs: list[dict[str, Any]] = []
    for directory in sorted(
        path for path in root.iterdir() if path.is_dir() and path.name.startswith("run-")
    ):
        rvv_path = directory / "run_bench_rvv.log"
        if not rvv_path.exists():
            raise SystemExit(f"missing RVV log under {directory}")
        runs.append({"name": directory.name, "rvv": parse_log(rvv_path)})
    if len(runs) != expected_runs:
        raise SystemExit(f"expected {expected_runs} run-* dirs under {root}, got {len(runs)}")
    return runs


def size_suffix(label: str) -> str:
    match = re.search(r"(\d+K)", label)
    if not match:
        raise SystemExit(f"cannot infer size suffix from case label: {label}")
    return match.group(1)


def size_value(suffix: str) -> int:
    return int(suffix.rstrip("K")) * 1024


def bucket_for(values: list[float]) -> str:
    if not values:
        return "blocked"
    median = statistics.median(values)
    if all(value > 1.15 for value in values):
        return "positive"
    if median >= 1.03 and min(values) >= 0.97:
        return "weak_positive"
    if all(0.97 <= value <= 1.03 for value in values):
        return "neutral"
    if median < 0.97 or min(values) < 0.97:
        return "negative"
    return "unstable"


def build_metrics(runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    labels = sorted(
        name
        for name in runs[0]["rvv"]["cases"]
        if name.startswith(FAMILY_CONFIG["materialize_prefix"])
    )
    if not labels:
        raise SystemExit(f"no {FAMILY_CONFIG['materialize_prefix']} cases found")

    metrics: dict[str, dict[str, Any]] = {}
    for materialize_label in labels:
        suffix = size_suffix(materialize_label)
        direct_label = f"{FAMILY_CONFIG['direct_prefix']} {suffix}"
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        materialize_checksums: set[str] = set()
        direct_checksums: set[str] = set()
        for run in runs:
            cases = run["rvv"]["cases"]
            if materialize_label not in cases:
                raise SystemExit(f"missing {materialize_label} in {run['name']}")
            if direct_label not in cases:
                raise SystemExit(f"missing {direct_label} in {run['name']}")
            materialize = cases[materialize_label]
            direct = cases[direct_label]
            materialize_ms = float(materialize["ms"])
            direct_ms = float(direct["ms"])
            values.append(materialize_ms / direct_ms)
            materialize_checksums.add(str(materialize.get("checksum")))
            direct_checksums.add(str(direct.get("checksum")))
            rows.append(
                {
                    "run": run["name"],
                    "materialize_ms": materialize_ms,
                    "direct_ms": direct_ms,
                    "ba": values[-1],
                }
            )
        metrics[suffix] = {
            "size": size_value(suffix),
            "values": values,
            "median": statistics.median(values),
            "min": min(values),
            "max": max(values),
            "bucket": bucket_for(values),
            "runs": rows,
            "materialize_checksums": sorted(materialize_checksums),
            "direct_checksums": sorted(direct_checksums),
        }
    return dict(sorted(metrics.items(), key=lambda item: size_value(item[0])))


def overall_decision(metrics: dict[str, dict[str, Any]]) -> str:
    target = [info for suffix, info in metrics.items() if suffix != "4K"] or list(metrics.values())
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


def side(
    boundary: str,
    asm_boundary: str,
    value_key: str,
    value: float,
    checksum: list[str],
) -> dict[str, Any]:
    return {
        "boundary": boundary,
        "wrapper": "bench_te2d",
        "row_source": FAMILY_CONFIG["row_source"],
        "solve": True,
        "checksum_policy": "matrix_checksum_same_input_same_output_correctness_checked_by_gtest",
        "checksum": checksum,
        "timer_boundary": FAMILY_CONFIG["timer_boundary"],
        "gate": FAMILY_CONFIG["gate"],
        "mask": "dense_finite_selected_rows_prechecked",
        "reduction": "rvv_two_pass_centered_sum_tree",
        "asm_boundary": asm_boundary,
        value_key: value,
    }


def build_manifest(
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    summary_path: Path,
    args: argparse.Namespace,
) -> dict[str, Any]:
    first_meta = runs[0]["rvv"]["metadata"]
    comparisons: list[dict[str, Any]] = []
    for suffix, info in metrics.items():
        materialize_median = statistics.median(
            row["materialize_ms"] for row in info["runs"]
        )
        direct_median = statistics.median(row["direct_ms"] for row in info["runs"])
        comparisons.append(
            {
                "name": (
                    f"{FAMILY_CONFIG['display_name']} materialize-vs-direct "
                    f"production-detail family {suffix}"
                ),
                "group": FAMILY_CONFIG["group"],
                "case_kind": "production_detail_family_selection",
                "evidence_role": "production_detail",
                "row_source": FAMILY_CONFIG["row_source"],
                "point_type": "PointXYZ",
                "source_point_type": "PointXYZ",
                "target_point_type": "PointXYZ",
                "scalar": "float",
                "size": info["size"],
                "run_count": len(runs),
                "ba_values": info["values"],
                "decision_bucket": info["bucket"],
                "std_ms": materialize_median,
                "rvv_ms": direct_median,
                "allowed_contract_mismatches": ["boundary", "asm_boundary"],
                "notes": (
                    "B/A = materialize ordered public RVV ms / direct "
                    f"{FAMILY_CONFIG['display_name']} public RVV ms. "
                    "The two sides use the same RVV binary, "
                    f"{FAMILY_CONFIG['row_semantics']}, point type, scalar, "
                    "solve and checksum policy. "
                    "This is production-detail family selection evidence, not public "
                    "Std/RVV speedup and not a production dispatch change."
                ),
                "baseline": side(
                    FAMILY_CONFIG["materialize_boundary"],
                    FAMILY_CONFIG["materialize_asm_boundary"],
                    "std_ms",
                    materialize_median,
                    info["materialize_checksums"],
                ),
                "candidate": side(
                    FAMILY_CONFIG["direct_boundary"],
                    FAMILY_CONFIG["direct_asm_boundary"],
                    "rvv_ms",
                    direct_median,
                    info["direct_checksums"],
                ),
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": FAMILY_CONFIG["title"],
            "evidence_role": "production_detail",
            "summary_path": str(summary_path),
            "label": FAMILY_CONFIG["label"],
            "analysis_script": "test-rvv/registration/transformation_estimation_2D/script/generate_te2d_dual_indexed_family_ab_summary.py",
            "metadata": {
                "device": args.device_label,
                "iterations": first_meta.get("iterations"),
                "warmup_iterations": first_meta.get("warmup_iterations"),
                "run_count": len(runs),
                "taskset": args.taskset,
                "governor": args.governor,
                "freq": args.freq,
                "temperature": args.temperature,
                "vlen": args.vlen_desc,
                "binary_hash": f"rvv={sha256_file(Path(args.rvv_bin))}",
                "dataset": first_meta.get("dataset"),
                "build": first_meta.get("build"),
                "case_filter": args.case_filter,
                "point_type": "PointXYZ",
                "source_point_type": "PointXYZ",
                "target_point_type": "PointXYZ",
                "row_source": FAMILY_CONFIG["row_source"],
                "decision_bucket": overall_decision(metrics),
            },
        },
        "checks": {
            "strict_ab": True,
            "equal_fields": [
                "wrapper",
                "row_source",
                "solve",
                "checksum_policy",
                "timer_boundary",
                "gate",
                "mask",
                "reduction",
            ],
            "thresholds": {"min_repeated_runs": args.expected_runs},
        },
        "runs": [run["name"] for run in runs],
        "artifacts": {
            "input_dir": str(Path(args.input_dir)),
            "summary_path": str(summary_path),
            "rvv_bin": args.rvv_bin,
        },
        "comparisons": comparisons,
    }


def values_text(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}x" for value in values)


def checksum_note(info: dict[str, Any]) -> str:
    if info["materialize_checksums"] == info["direct_checksums"]:
        return "same"
    return (
        f"materialize `{', '.join(info['materialize_checksums'])}` / "
        f"direct `{', '.join(info['direct_checksums'])}`"
    )


def render_summary(
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    manifest_path: Path,
    doctor_path: Path,
    args: argparse.Namespace,
) -> str:
    first_meta = runs[0]["rvv"]["metadata"]
    lines: list[str] = []
    lines.append(
        f"# transformation_estimation_2D {FAMILY_CONFIG['title_name']} Family A/B Summary"
    )
    lines.append("")
    lines.append("## 运行合同")
    lines.append("")
    lines.append(f"- run_label：`{args.run_label}`")
    lines.append(f"- runs：`{', '.join(run['name'] for run in runs)}`")
    lines.append(f"- device：`{args.device_label}`")
    lines.append(f"- case-filter：`{args.case_filter}`")
    lines.append("- evidence_role：`production_detail`；这是同一 RVV binary 内的实现族选择证据。")
    lines.append(f"- iterations：`{first_meta.get('iterations')}`")
    lines.append(f"- warm-up iterations：`{first_meta.get('warmup_iterations')}`")
    lines.append(
        "- B/A：`materialize ordered public RVV ms / direct "
        f"{FAMILY_CONFIG['display_name']} public RVV ms`，大于 1 表示 direct gather 更快。"
    )
    lines.append(f"- manifest：`{manifest_path}`")
    lines.append(f"- Evidence Doctor：`{doctor_path}`")
    lines.append("")
    lines.append("## 结果")
    lines.append("")
    lines.append(
        "两侧使用同一 RVV bench binary、同一 "
        f"{FAMILY_CONFIG['row_semantics']}、同一 `PointXYZ -> PointXYZ` / "
        "`Scalar=float` 输入和同一 2D solve。"
    )
    lines.append(
        "materialize 侧在每次计时内复制 selected source/target rows，再调用真实 "
        "ordered-cloud-pair public overload；direct 侧调用 "
        f"{FAMILY_CONFIG['direct_overload']}。"
    )
    lines.append("")
    lines.append("| size | values | median | min | max | bucket | materialize_ms | direct_ms | checksum |")
    lines.append("| --- | --- | ---: | ---: | ---: | --- | ---: | ---: | --- |")
    for suffix, info in metrics.items():
        materialize_median = statistics.median(
            row["materialize_ms"] for row in info["runs"]
        )
        direct_median = statistics.median(row["direct_ms"] for row in info["runs"])
        lines.append(
            f"| {suffix} | {values_text(info['values'])} | {info['median']:.3f}x | "
            f"{info['min']:.3f}x | {info['max']:.3f}x | `{info['bucket']}` | "
            f"{materialize_median:.4f} | {direct_median:.4f} | {checksum_note(info)} |"
        )
    lines.append("")
    lines.append("## 决策桶")
    lines.append("")
    lines.append(f"- overall_decision_bucket：`{overall_decision(metrics)}`（默认按 64K / 256K 判断，4K 保留为小规模参考）。")
    lines.append(
        "- positive / weak_positive 支持在当前 exact `PointXYZ -> PointXYZ` "
        f"{FAMILY_CONFIG['display_name']} production probe 范围内继续保留 direct "
        f"gather family；不扩大到 {FAMILY_CONFIG['not_extend']}。"
    )
    lines.append(
        "- negative / unstable 只触发用户判断或后续 staging/profile；不自动回滚已采纳的 "
        "ordered-cloud-pair / source-indexed patch，也不自动回滚当前 bounded probe。"
    )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-filter", default="dual-indexed-family-ab")
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--rvv-bin", required=True)
    parser.add_argument("--device-label", default="RVV board")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    parser.add_argument("--taskset", default="not_recorded_board_target")
    parser.add_argument("--governor", default="not_recorded_board_target")
    parser.add_argument("--freq", default="not_recorded_board_target")
    parser.add_argument("--temperature", default="not_recorded_board_target")
    args = parser.parse_args()
    if args.case_filter not in FAMILY_CONFIGS:
        expected = ", ".join(sorted(FAMILY_CONFIGS))
        raise SystemExit(f"unsupported case-filter for family A/B summary: {args.case_filter}; expected one of {expected}")
    global FAMILY_CONFIG
    FAMILY_CONFIG = FAMILY_CONFIGS[args.case_filter]

    runs = collect_runs(args.input_dir, args.expected_runs)
    metrics = build_metrics(runs)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    summary_path = args.output_dir / "summary.md"
    manifest_path = args.output_dir / "evidence_manifest.json"
    doctor_path = args.output_dir / "evidence_doctor.md"
    manifest = build_manifest(runs, metrics, summary_path, args)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    summary_path.write_text(
        render_summary(runs, metrics, manifest_path, doctor_path, args),
        encoding="utf-8",
    )
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
