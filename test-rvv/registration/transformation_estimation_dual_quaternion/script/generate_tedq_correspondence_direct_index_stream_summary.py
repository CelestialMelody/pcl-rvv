#!/usr/bin/env python3
"""生成 correspondence direct index-stream 的板卡摘要和 Evidence manifest。

该脚本只比较同一个 RVV bench binary 中的三种 correspondence row ingress：

* staged ordered reuse：先把 query/match 展开成两个临时 index vector；
* direct indexed gather：复用已经验证过的双侧 index-vector gather；
* direct index stream：从 Correspondence AoS 记录按固定 stride 直接读取 query/match。

三侧共享 C1/C2 公式、RVV reduction、Eigen 4x4 solve 和 checksum policy。摘要中的
性能结论只代表 correspondence-pair 的 test-rvv implementation-family comparison，
不能替代 production direct 证据。
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
        elif match := WARMUP_RE.match(line):
            metadata["warmup_iterations"] = int(match.group("value"))
        elif match := DATASET_RE.match(line):
            metadata["dataset"] = match.group("value")
        elif match := CASE_RE.match(line):
            current_case = match.group("name").strip()
            cases[current_case] = {"ms": float(match.group("ms"))}
        elif current_case and (match := CHECKSUM_RE.search(line)):
            cases[current_case]["checksum"] = match.group("checksum")
            current_case = None
    return {"metadata": metadata, "cases": cases}


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
    staged_prefix = "staged accum candidate correspondence-pair"
    labels = sorted(
        {
            name
            for name in runs[0]["rvv"]["cases"]
            if name.startswith(staged_prefix)
        }
    )
    if not labels:
        raise SystemExit(f"no {staged_prefix} cases found")

    metrics: dict[str, dict[str, Any]] = {}
    for staged_label in labels:
        suffix = size_suffix(staged_label)
        direct_label = f"direct gather candidate correspondence-pair {suffix}"
        stream_label = f"direct index stream candidate correspondence-pair {suffix}"
        values_staged_direct: list[float] = []
        values_staged_stream: list[float] = []
        values_direct_stream: list[float] = []
        rows: list[dict[str, Any]] = []
        checksums = {"staged": set(), "direct": set(), "stream": set()}
        for run in runs:
            cases = run["rvv"]["cases"]
            for label in (direct_label, stream_label):
                if label not in cases:
                    raise SystemExit(f"missing {label} in {run['name']}")
            staged = cases[staged_label]
            direct = cases[direct_label]
            stream = cases[stream_label]
            staged_ms = float(staged["ms"])
            direct_ms = float(direct["ms"])
            stream_ms = float(stream["ms"])
            values_staged_direct.append(staged_ms / direct_ms)
            values_staged_stream.append(staged_ms / stream_ms)
            values_direct_stream.append(direct_ms / stream_ms)
            rows.append(
                {
                    "run": run["name"],
                    "staged_ms": staged_ms,
                    "direct_ms": direct_ms,
                    "stream_ms": stream_ms,
                    "staged_vs_direct": values_staged_direct[-1],
                    "staged_vs_stream": values_staged_stream[-1],
                    "direct_vs_stream": values_direct_stream[-1],
                }
            )
            checksums["staged"].add(str(staged.get("checksum")))
            checksums["direct"].add(str(direct.get("checksum")))
            checksums["stream"].add(str(stream.get("checksum")))

        metrics[suffix] = {
            "size": size_value(suffix),
            "rows": rows,
            "staged_vs_direct": values_staged_direct,
            "staged_vs_stream": values_staged_stream,
            "direct_vs_stream": values_direct_stream,
            "buckets": {
                "staged_vs_direct": bucket_for(values_staged_direct),
                "staged_vs_stream": bucket_for(values_staged_stream),
                "direct_vs_stream": bucket_for(values_direct_stream),
            },
            "checksums": {key: sorted(value) for key, value in checksums.items()},
        }
    return metrics


def overall_decision(metrics: dict[str, dict[str, Any]], key: str) -> str:
    target = [
        info["buckets"][key]
        for suffix, info in metrics.items()
        if suffix != "4K"
    ] or [info["buckets"][key] for info in metrics.values()]
    if set(target) <= {"positive"}:
        return "positive"
    if set(target) <= {"positive", "weak_positive"}:
        return "weak_positive"
    if "negative" in target:
        return "negative"
    if "unstable" in target:
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
        "wrapper": "bench_tedq",
        "row_source": "correspondence-pair",
        "solve": True,
        "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
        "checksum": checksum,
        "timer_boundary": "correspondence_row_ingress_plus_c1_c2_accumulation_plus_eigen_4x4_solve",
        "gate": "dense_correspondence_pair_pointxyz_float_xyz_aos",
        "mask": "not_applicable_dense_only",
        "reduction": "rvv_f64_tree",
        "asm_boundary": asm_boundary,
        value_key: value,
    }


def comparison(
    name: str,
    group: str,
    baseline_label: str,
    candidate_label: str,
    metric_key: str,
    info: dict[str, Any],
    baseline_ms: float,
    candidate_ms: float,
    baseline_boundary: str,
    candidate_boundary: str,
    baseline_asm: str,
    candidate_asm: str,
    baseline_checksum: list[str],
    candidate_checksum: list[str],
) -> dict[str, Any]:
    return {
        "name": name,
        "group": group,
        "case_kind": "implementation_family_comparison",
        "evidence_role": "strict_ab",
        "row_source": "correspondence-pair",
        "point_type": "PointXYZ",
        "scalar": "float",
        "size": info["size"],
        "run_count": len(info["rows"]),
        "ba_values": info[metric_key],
        "decision_bucket": info["buckets"][metric_key],
        "std_ms": baseline_ms,
        "rvv_ms": candidate_ms,
        "allowed_contract_mismatches": ["boundary", "asm_boundary"],
        "notes": (
            f"B/A = {baseline_label} ms / {candidate_label} ms. The two sides use the "
            "same RVV binary, correspondence rows, C1/C2 formula, Eigen 4x4 solve and "
            "checksum policy; only row ingress differs."
        ),
        "baseline": side(
            baseline_boundary,
            baseline_asm,
            "std_ms",
            baseline_ms,
            baseline_checksum,
        ),
        "candidate": side(
            candidate_boundary,
            candidate_asm,
            "rvv_ms",
            candidate_ms,
            candidate_checksum,
        ),
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
        staged_ms = statistics.median(row["staged_ms"] for row in info["rows"])
        direct_ms = statistics.median(row["direct_ms"] for row in info["rows"])
        stream_ms = statistics.median(row["stream_ms"] for row in info["rows"])
        checksums = info["checksums"]
        comparisons.append(
            comparison(
                f"correspondence staged-vs-direct gather {suffix}",
                "tedq_correspondence_staged_vs_direct_gather",
                "staged ordered reuse RVV",
                "direct indexed gather RVV",
                "staged_vs_direct",
                info,
                staged_ms,
                direct_ms,
                "test_support_staged_ordered_reuse",
                "test_support_correspondence_direct_gather",
                "bench_binary_staged_ordered_reuse",
                "bench_binary_correspondence_direct_gather",
                checksums["staged"],
                checksums["direct"],
            )
        )
        comparisons.append(
            comparison(
                f"correspondence staged-vs-direct index stream {suffix}",
                "tedq_correspondence_staged_vs_direct_index_stream",
                "staged ordered reuse RVV",
                "direct index stream RVV",
                "staged_vs_stream",
                info,
                staged_ms,
                stream_ms,
                "test_support_staged_ordered_reuse",
                "test_support_correspondence_direct_index_stream",
                "bench_binary_staged_ordered_reuse",
                "bench_binary_correspondence_direct_index_stream",
                checksums["staged"],
                checksums["stream"],
            )
        )
        comparisons.append(
            comparison(
                f"correspondence direct gather-vs-direct index stream {suffix}",
                "tedq_correspondence_direct_gather_vs_index_stream",
                "direct indexed gather RVV",
                "direct index stream RVV",
                "direct_vs_stream",
                info,
                direct_ms,
                stream_ms,
                "test_support_correspondence_direct_gather",
                "test_support_correspondence_direct_index_stream",
                "bench_binary_correspondence_direct_gather",
                "bench_binary_correspondence_direct_index_stream",
                checksums["direct"],
                checksums["stream"],
            )
        )
    decisions = {
        "staged_vs_direct": overall_decision(metrics, "staged_vs_direct"),
        "staged_vs_stream": overall_decision(metrics, "staged_vs_stream"),
        "direct_vs_stream": overall_decision(metrics, "direct_vs_stream"),
    }
    return {
        "schema_version": 1,
        "summary": {
            "title": "transformation_estimation_dual_quaternion correspondence direct index-stream comparison",
            "evidence_role": "diagnostic",
            "summary_path": str(summary_path),
            "label": (
                "correspondence staged vs direct gather vs direct index stream; "
                "test-rvv only, not production direct"
            ),
            "analysis_script": (
                "test-rvv/registration/transformation_estimation_dual_quaternion/"
                "script/generate_tedq_correspondence_direct_index_stream_summary.py"
            ),
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
                "binary_hash": f"rvv={sha256_file(Path(args.rvv_bin))}",
                "dataset": first_meta.get("dataset"),
                "decision_bucket": decisions,
            },
        },
        "checks": {
            "strict_ab": False,
            "equal_fields": [
                "wrapper",
                "row_source",
                "solve",
                "checksum_policy",
                "timer_boundary",
                "gate",
                "mask",
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


def checksum_note(checksums: dict[str, list[str]]) -> str:
    if checksums["staged"] == checksums["direct"] == checksums["stream"]:
        return "same"
    return "; ".join(
        f"{key} `{', '.join(value)}`" for key, value in checksums.items()
    )


def values_text(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}" for value in values)


def write_summary(
    path: Path,
    runs: list[dict[str, Any]],
    metrics: dict[str, dict[str, Any]],
    manifest_path: Path,
    doctor_path: Path,
    args: argparse.Namespace,
) -> None:
    lines = [
        "# transformation_estimation_dual_quaternion correspondence direct index-stream summary",
        "",
        "## 运行合同",
        "",
        f"- run_label：`{args.run_label}`",
        f"- runs：`{', '.join(run['name'] for run in runs)}`",
        f"- device：`{args.device_label}`",
        f"- case-filter：`{args.case_filter}`",
        "- evidence_role：`diagnostic`；只覆盖 `correspondence-pair`，不是 production direct。",
        f"- iterations：`{runs[0]['rvv']['metadata'].get('iterations')}`",
        f"- warm-up iterations：`{runs[0]['rvv']['metadata'].get('warmup_iterations')}`",
        "- B/A：各比较均为 baseline ms / candidate ms，大于 1 表示 candidate 更快。",
        "- 三侧共享 RVV binary、C1/C2 公式、Eigen 4x4 solve 和 checksum policy；差异只在 correspondence row ingress。",
        f"- manifest：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
    ]
    headings = (
        ("staged_vs_direct", "staged ordered reuse / direct indexed gather"),
        ("staged_vs_stream", "staged ordered reuse / direct index stream"),
        ("direct_vs_stream", "direct indexed gather / direct index stream"),
    )
    for suffix, info in metrics.items():
        lines.extend(
            [
                "",
                f"## correspondence-pair {suffix}",
                "",
                "| comparison | values | median | min | max | bucket |",
                "| --- | --- | ---: | ---: | ---: | --- |",
            ]
        )
        for key, title in headings:
            values = info[key]
            lines.append(
                f"| {title} | `{values_text(values)}` | "
                f"{statistics.median(values):.3f} | {min(values):.3f} | "
                f"{max(values):.3f} | `{info['buckets'][key]}` |"
            )
        lines.append(f"- checksum：{checksum_note(info['checksums'])}")

    lines.extend(
        [
            "",
            "## 决策边界",
            "",
            f"- staged-vs-direct overall：`{overall_decision(metrics, 'staged_vs_direct')}`。",
            f"- staged-vs-index-stream overall：`{overall_decision(metrics, 'staged_vs_stream')}`。",
            f"- direct-gather-vs-index-stream overall：`{overall_decision(metrics, 'direct_vs_stream')}`。",
            "- 64K 若继续偏离 4K / 256K，只按 correspondence 的 size 独立解释；不能单因归因于 gather 或 AoS 展开。",
            "- direct index stream 若收益不稳定，保留 Phase 007 direct gather 作为明确 baseline，不升级为默认路径。",
            "- 所有正向结果仍是 test-rvv 诊断证据，不能写成 production-ready。",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--case-filter", required=True)
    parser.add_argument("--expected-runs", type=int, default=5)
    parser.add_argument("--rvv-bin", required=True)
    parser.add_argument("--device-label", default="RVV board")
    parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    summary_path = output_dir / "summary.md"
    manifest_path = output_dir / "evidence_manifest.json"
    doctor_path = output_dir / "evidence_doctor.md"
    runs = collect_runs(Path(args.input_dir), args.expected_runs)
    metrics = build_metrics(runs)
    manifest = build_manifest(runs, metrics, summary_path, args)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_summary(summary_path, runs, metrics, manifest_path, doctor_path, args)
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
