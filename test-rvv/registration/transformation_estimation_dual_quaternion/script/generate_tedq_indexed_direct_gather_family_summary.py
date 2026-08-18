#!/usr/bin/env python3
"""生成三类 indexed row-source staged/direct family 摘要和 manifest。

该脚本只比较同一 RVV 二进制中的 staged ordered reuse 与 direct indexed gather。
source-indexed、dual-indexed、correspondence 分别生成 comparison，不能互相继承
decision bucket。QEMU 输出只用于 smoke，性能结论必须来自目标板 repeated run。
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

POLICIES = (
    ("source-indexed", "source-indexed-cloud-pair"),
    ("dual-indexed", "dual-indexed-cloud-pair"),
    ("correspondence", "correspondence-pair"),
)


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
        p for p in root.iterdir() if p.is_dir() and p.name.startswith("run-")
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


def build_metrics(
    runs: list[dict[str, Any]], policy: str, row_source: str
) -> dict[str, dict[str, Any]]:
    staged_prefix = f"staged accum candidate {row_source}"
    direct_prefix = f"direct gather candidate {row_source}"
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
        direct_label = f"{direct_prefix} {suffix}"
        values: list[float] = []
        rows: list[dict[str, Any]] = []
        staged_checksums: set[str] = set()
        direct_checksums: set[str] = set()
        for run in runs:
            cases = run["rvv"]["cases"]
            if direct_label not in cases:
                raise SystemExit(f"missing {direct_label} in {run['name']}")
            staged = cases[staged_label]
            direct = cases[direct_label]
            values.append(float(staged["ms"]) / float(direct["ms"]))
            staged_checksums.add(str(staged.get("checksum")))
            direct_checksums.add(str(direct.get("checksum")))
            rows.append(
                {
                    "run": run["name"],
                    "staged_ms": float(staged["ms"]),
                    "direct_ms": float(direct["ms"]),
                    "ba": values[-1],
                }
            )
        metrics[suffix] = {
            "policy": policy,
            "row_source": row_source,
            "size": size_value(suffix),
            "values": values,
            "median": statistics.median(values),
            "min": min(values),
            "max": max(values),
            "bucket": bucket_for(values),
            "runs": rows,
            "staged_checksums": sorted(staged_checksums),
            "direct_checksums": sorted(direct_checksums),
        }
    return metrics


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


def policy_gate(row_source: str) -> str:
    return f"dense_{row_source.replace('-', '_')}_pointxyz_float_xyz_aos"


def side(
    row_source: str,
    boundary: str,
    asm_boundary: str,
    value_key: str,
    value: float,
    checksum: list[str],
) -> dict[str, Any]:
    return {
        "boundary": boundary,
        "wrapper": "bench_tedq",
        "row_source": row_source,
        "solve": True,
        "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
        "checksum": checksum,
        "timer_boundary": "indexed_row_ingress_plus_c1_c2_accumulation_plus_eigen_4x4_solve",
        "gate": policy_gate(row_source),
        "mask": "not_applicable_dense_only",
        "reduction": "rvv_f64_tree",
        "asm_boundary": asm_boundary,
        value_key: value,
    }


def build_manifest(
    runs: list[dict[str, Any]],
    policy_metrics: dict[str, dict[str, dict[str, Any]]],
    summary_path: Path,
    args: argparse.Namespace,
) -> dict[str, Any]:
    first_meta = runs[0]["rvv"]["metadata"]
    comparisons: list[dict[str, Any]] = []
    policy_decisions: dict[str, str] = {}
    for policy, metrics in policy_metrics.items():
        policy_decisions[policy] = overall_decision(metrics)
        row_source = next(row_source for name, row_source in POLICIES if name == policy)
        for suffix, info in metrics.items():
            staged_median = statistics.median(row["staged_ms"] for row in info["runs"])
            direct_median = statistics.median(row["direct_ms"] for row in info["runs"])
            comparisons.append(
                {
                    "name": f"{policy} staged-vs-direct RVV family {suffix}",
                    "group": f"tedq_{policy}_implementation_family",
                    "case_kind": "implementation_family_comparison",
                    "evidence_role": "strict_ab",
                    "row_source": row_source,
                    "point_type": "PointXYZ",
                    "scalar": "float",
                    "size": info["size"],
                    "run_count": len(runs),
                    "ba_values": info["values"],
                    "decision_bucket": info["bucket"],
                    "std_ms": staged_median,
                    "rvv_ms": direct_median,
                    "allowed_contract_mismatches": ["boundary", "reduction", "asm_boundary"],
                    "notes": (
                        "B/A = staged ordered reuse RVV ms / direct indexed gather RVV ms. "
                        "The two sides use the same RVV binary, row source, formula, solve and "
                        "checksum policy; each indexed policy remains independently bounded."
                    ),
                    "baseline": side(
                        row_source,
                        "test_support_staged_ordered_reuse",
                        "bench_binary_staged_ordered_reuse",
                        "std_ms",
                        staged_median,
                        info["staged_checksums"],
                    ),
                    "candidate": side(
                        row_source,
                        f"test_support_{policy}_direct_gather",
                        f"bench_binary_{policy}_direct_gather",
                        "rvv_ms",
                        direct_median,
                        info["direct_checksums"],
                    ),
                }
            )
    overall = "positive" if all(value == "positive" for value in policy_decisions.values()) else "per_policy"
    return {
        "schema_version": 1,
        "summary": {
            "title": "transformation_estimation_dual_quaternion indexed direct-gather family comparison",
            "evidence_role": "diagnostic",
            "summary_path": str(summary_path),
            "label": "indexed staged ordered reuse vs direct indexed gather; per-policy, not production direct",
            "analysis_script": "test-rvv/registration/transformation_estimation_dual_quaternion/script/generate_tedq_indexed_direct_gather_family_summary.py",
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
                "decision_bucket": overall,
                "policy_decisions": policy_decisions,
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


def checksum_note(info: dict[str, Any]) -> str:
    if info["staged_checksums"] == info["direct_checksums"]:
        return "same"
    return f"staged `{', '.join(info['staged_checksums'])}` / direct `{', '.join(info['direct_checksums'])}`"


def write_summary(
    path: Path,
    runs: list[dict[str, Any]],
    policy_metrics: dict[str, dict[str, dict[str, Any]]],
    manifest_path: Path,
    doctor_path: Path,
    args: argparse.Namespace,
) -> None:
    lines = [
        "# transformation_estimation_dual_quaternion indexed direct-gather family summary",
        "",
        "## 运行合同",
        "",
        f"- run_label：`{args.run_label}`",
        f"- runs：`{', '.join(run['name'] for run in runs)}`",
        f"- device：`{args.device_label}`",
        f"- case-filter：`{args.case_filter}`",
        "- evidence_role：`diagnostic`；三类 indexed policy 独立比较，不是 production direct。",
        f"- iterations：`{runs[0]['rvv']['metadata'].get('iterations')}`",
        f"- warm-up iterations：`{runs[0]['rvv']['metadata'].get('warmup_iterations')}`",
        "- B/A：`staged ordered reuse RVV ms / direct indexed gather RVV ms`，大于 1 表示 direct gather 更快。",
        f"- manifest：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
    ]
    for policy, metrics in policy_metrics.items():
        row_source = next(row_source for name, row_source in POLICIES if name == policy)
        lines.extend(
            [
                "",
                f"## {row_source} staged-vs-direct RVV family",
                "",
                "两侧使用同一 RVV binary、同一 row source、同一 C1/C2 公式、同一 Eigen 4x4 solve "
                "和同一 checksum policy；唯一目标差异是 indexed row ingress。",
                "",
                "| size | values | median | min | max | bucket | checksum |",
                "| --- | --- | ---: | ---: | ---: | --- | --- |",
            ]
        )
        for suffix, info in metrics.items():
            lines.append(
                f"| {suffix} | `{', '.join(f'{value:.3f}' for value in info['values'])}` | "
                f"{info['median']:.3f} | {info['min']:.3f} | {info['max']:.3f} | "
                f"`{info['bucket']}` | {checksum_note(info)} |"
            )
        lines.append(
            f"- policy_decision_bucket：`{overall_decision(metrics)}`（默认按 64K / 256K 判断，4K 保留为小规模诊断）。"
        )
    lines.extend(
        [
            "",
            "## 决策边界",
            "",
            "- 每个 indexed policy 独立判断；不能把 source-indexed、dual-indexed 或 correspondence 的结果互相外推。",
            "- positive / weak_positive 只表示 test-rvv implementation-family comparison，不能升级为 production-ready。",
            "- 若 direct gather 的收益方向不稳定，必须保留 staged candidate，并把对应 policy 降级为 unstable 或 neutral。",
            "",
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
    policy_metrics = {
        policy: build_metrics(runs, policy, row_source)
        for policy, row_source in POLICIES
    }
    manifest = build_manifest(runs, policy_metrics, summary_path, args)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_summary(summary_path, runs, policy_metrics, manifest_path, doctor_path, args)
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
