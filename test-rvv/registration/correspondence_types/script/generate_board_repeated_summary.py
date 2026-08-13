#!/usr/bin/env python3
"""
生成 correspondence_types 的 board repeated summary（板卡重复测试摘要）。

输入是一组 run-* 目录，每个目录包含 run_bench_std.log 和 run_bench_rvv.log。
脚本生成可读 summary、case、Std/RVV 平均耗时、checksum、B/A values
（这里 B/A = Std ms / RVV ms，>1 表示 RVV 更快）和 Evidence Doctor manifest
（证据体检清单）。默认提交边界只纳入 summary / Evidence Doctor，可再生成的
manifest 与 raw log（原始日志）仍按 .gitignore 和 evidence policy 处理。
"""

from __future__ import annotations

import argparse
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


class SummaryOptions:
    def __init__(
        self,
        case_filter: str,
        group: str,
        evidence_role: str,
        case_kind: str,
        boundary: str,
        decision_summary: str,
    ) -> None:
        self.case_filter = case_filter
        self.group = group
        self.evidence_role = evidence_role
        self.case_kind = case_kind
        self.boundary = boundary
        self.decision_summary = decision_summary


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
    dirs = [p for p in root.iterdir() if p.is_dir() and p.name.startswith("run-")]
    return sorted(dirs, key=lambda p: p.name)


def bucket_for(values: list[float]) -> str:
    if not values:
        return "blocked"
    if all(v > 1.15 for v in values):
        return "positive"
    if statistics.median(values) >= 1.03 and min(values) >= 0.97:
        return "weak_positive"
    if all(0.97 <= v <= 1.03 for v in values):
        return "neutral"
    if statistics.median(values) < 0.97 or min(values) < 0.97:
        return "negative"
    return "unstable"


def case_size(name: str) -> int | None:
    match = re.search(r"(\d+)K", name)
    if not match:
        return None
    return int(match.group(1)) * 1024


def case_checksum_policy(name: str) -> str:
    return "fnv1a_indices" if "index" in name or "query+match" in name else "scaled_mean_stddev"


def case_timer_boundary(name: str) -> str:
    return "two_candidate_calls_and_checksum" if "query+match" in name else "one_candidate_call_and_checksum"


def case_reduction(name: str) -> str:
    return "float_square_same_chain_scalar_order" if "distance-stats" in name else "not_applicable_index_extract"


def fmt_values(values: list[float]) -> str:
    return ", ".join(f"{value:.3f}" for value in values)


def collect(root: Path) -> dict[str, Any]:
    runs: list[dict[str, Any]] = []
    for directory in run_dirs(root):
        std_path = directory / "run_bench_std.log"
        rvv_path = directory / "run_bench_rvv.log"
        if not std_path.exists() or not rvv_path.exists():
            raise SystemExit(f"missing logs under {directory}")
        std = parse_log(std_path)
        rvv = parse_log(rvv_path)
        missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
        if missing:
            raise SystemExit(f"case set mismatch under {directory}: {', '.join(missing)}")
        runs.append({"name": directory.name, "std": std, "rvv": rvv})
    if not runs:
        raise SystemExit(f"no run-* directories under {root}")
    return {"root": root, "runs": runs}


def build_cases(data: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    first_cases = sorted(data["runs"][0]["std"]["cases"])
    for name in first_cases:
        values: list[float] = []
        std_values: list[float] = []
        rvv_values: list[float] = []
        std_checksums: set[str] = set()
        rvv_checksums: set[str] = set()
        per_run: list[dict[str, Any]] = []
        for run in data["runs"]:
            std_case = run["std"]["cases"][name]
            rvv_case = run["rvv"]["cases"][name]
            std_ms = float(std_case["ms"])
            rvv_ms = float(rvv_case["ms"])
            speedup = std_ms / rvv_ms if rvv_ms else 0.0
            values.append(speedup)
            std_values.append(std_ms)
            rvv_values.append(rvv_ms)
            std_checksums.add(str(std_case.get("checksum")))
            rvv_checksums.add(str(rvv_case.get("checksum")))
            per_run.append(
                {
                    "run": run["name"],
                    "std_ms": std_ms,
                    "rvv_ms": rvv_ms,
                    "speedup": speedup,
                    "std_checksum": std_case.get("checksum"),
                    "rvv_checksum": rvv_case.get("checksum"),
                }
            )
        result[name] = {
            "values": values,
            "std_values": std_values,
            "rvv_values": rvv_values,
            "std_checksums": sorted(std_checksums),
            "rvv_checksums": sorted(rvv_checksums),
            "checksum_match": std_checksums == rvv_checksums and len(std_checksums) == 1,
            "median": statistics.median(values),
            "min": min(values),
            "max": max(values),
            "bucket": bucket_for(values),
            "per_run": per_run,
        }
    return result


def build_manifest(
    data: dict[str, Any],
    cases: dict[str, Any],
    summary_path: Path,
    options: SummaryOptions,
) -> dict[str, Any]:
    first = data["runs"][0]
    metadata = first["std"]["metadata"]
    comparisons = []
    for name, info in cases.items():
        wrapper = "bench_correspondence_types"
        row_source = "correspondences"
        timer_boundary = case_timer_boundary(name)
        checksum_policy = case_checksum_policy(name)
        reduction = case_reduction(name)
        gate = "correspondence_layout_supported_or_scalar_fallback"
        mask = "not_applicable"
        checksum = info["std_checksums"][0] if info["checksum_match"] else "mismatch_or_multi"
        comparisons.append(
            {
                "name": name,
                "group": options.group,
                "evidence_role": options.evidence_role,
                "case_kind": options.case_kind,
                "row_source": row_source,
                "size": case_size(name),
                "run_count": len(data["runs"]),
                "ba_values": info["values"],
                "std_ms": statistics.median(info["std_values"]),
                "rvv_ms": statistics.median(info["rvv_values"]),
                "decision_bucket": info["bucket"],
                "baseline": {
                    "label": "Std build",
                    "boundary": options.boundary,
                    "wrapper": wrapper,
                    "row_source": row_source,
                    "solve": False,
                    "checksum_policy": checksum_policy,
                    "checksum": checksum,
                    "timer_boundary": timer_boundary,
                    "gate": gate,
                    "mask": mask,
                    "reduction": reduction,
                    "asm_boundary": "not_applicable_std_build",
                    "std_ms": statistics.median(info["std_values"]),
                },
                "candidate": {
                    "label": "RVV build",
                    "boundary": options.boundary,
                    "wrapper": wrapper,
                    "row_source": row_source,
                    "solve": False,
                    "checksum_policy": checksum_policy,
                    "checksum": checksum,
                    "timer_boundary": timer_boundary,
                    "gate": gate,
                    "mask": mask,
                    "reduction": reduction,
                    "asm_boundary": "bench_binary_smoke_only",
                    "rvv_ms": statistics.median(info["rvv_values"]),
                },
            }
        )
    return {
        "schema_version": 1,
        "summary": {
            "title": f"correspondence_types board repeated {options.decision_summary}",
            "evidence_role": options.evidence_role,
            "summary_path": str(summary_path),
            "label": f"board repeated {options.decision_summary}",
            "analysis_script": "test-rvv/script/evidence_doctor.py",
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": metadata.get("iterations"),
                "warmup_iterations": metadata.get("warmup_iterations"),
                "run_count": len(data["runs"]),
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": "not_recorded",
                "dataset": metadata.get("dataset"),
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
        },
        "runs": [run["name"] for run in data["runs"]],
        "comparisons": comparisons,
    }


def write_summary(
    path: Path,
    data: dict[str, Any],
    cases: dict[str, Any],
    manifest_path: Path,
    doctor_path: Path,
    options: SummaryOptions,
) -> None:
    role_note = (
        "不是 production direct（真实生产路径证据）。"
        if options.evidence_role == "diagnostic"
        else "production direct probe（真实生产路径探针），需要 Evidence Doctor 和反汇编归属共同解释。"
    )
    lines = [
        "# correspondence_types board repeated summary",
        "",
        "## 运行合同",
        "",
        f"- run root：`{data['root']}`",
        f"- runs：{', '.join(run['name'] for run in data['runs'])}",
        "- device：Milkv-Jupiter",
        f"- case-filter：`{options.case_filter}`",
        f"- evidence_role：`{options.evidence_role}`，{role_note}",
        "- B/A：`Std ms / RVV ms`，大于 1 表示 RVV 更快。",
        f"- manifest（本地机器输入，默认不提交）：`{manifest_path}`",
        f"- Evidence Doctor：`{doctor_path}`",
        "",
        "## 汇总",
        "",
        "| case | runs | speedup values | median | min | max | checksum | bucket |",
        "| --- | ---: | --- | ---: | ---: | ---: | --- | --- |",
    ]
    for name, info in cases.items():
        checksum = "match" if info["checksum_match"] else "mismatch"
        lines.append(
            f"| {name} | {len(info['values'])} | `{fmt_values(info['values'])}` | "
            f"{info['median']:.3f} | {info['min']:.3f} | {info['max']:.3f} | "
            f"{checksum} | `{info['bucket']}` |"
        )
    lines.extend(
        [
            "",
            "## 决策",
            "",
            f"5 轮板卡 `{options.case_filter}` 中，index extraction（索引抽取）的结论按上表 bucket 判断。",
            "若主 case 没有达到 `weak_positive`，本摘要不支持保留 production probe（生产路径探针）。",
            "若达到 `weak_positive`，仍需结合 correctness、asm attribution 和 Evidence Doctor 再做生产接入判断。",
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-root", default="log/board/010-board-diagnostic/index-extract")
    parser.add_argument("--summary", default="log/board/010-board-diagnostic/index-extract/summary.md")
    parser.add_argument("--manifest", default="log/board/010-board-diagnostic/index-extract/evidence_manifest.json")
    parser.add_argument("--doctor", default="log/board/010-board-diagnostic/index-extract/evidence_doctor.md")
    parser.add_argument("--case-filter", default="index-extract")
    parser.add_argument("--group", default="board-index-extract")
    parser.add_argument("--evidence-role", default="diagnostic")
    parser.add_argument("--case-kind", default="board_diagnostic")
    parser.add_argument("--boundary", default="test_support_candidate_wrapper")
    parser.add_argument("--decision-summary", default="diagnostic")
    args = parser.parse_args()
    options = SummaryOptions(
        case_filter=args.case_filter,
        group=args.group,
        evidence_role=args.evidence_role,
        case_kind=args.case_kind,
        boundary=args.boundary,
        decision_summary=args.decision_summary,
    )

    data = collect(Path(args.run_root))
    cases = build_cases(data)
    summary_path = Path(args.summary)
    manifest_path = Path(args.manifest)
    doctor_path = Path(args.doctor)
    manifest = build_manifest(data, cases, summary_path, options)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_summary(summary_path, data, cases, manifest_path, doctor_path, options)
    print(f"wrote summary: {summary_path}")
    print(f"wrote manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
