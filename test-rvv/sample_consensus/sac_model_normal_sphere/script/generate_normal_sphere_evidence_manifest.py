#!/usr/bin/env python3
"""Generate Evidence Doctor input for sac_model_normal_sphere.

本脚本解析当前 topic 的 board bench 日志。Phase 000-030 仍输出
production-shaped diagnostic（生产形态诊断）或 component ablation（组件消融）
证据；Phase 060 则把真实 public entry（公开入口）Std/RVV 对比翻译成
production-public 证据，供生产接入后的 result、evaluation、doc-rvv 和
Handoff 引用。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


TOPIC_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = next(path for path in (TOPIC_ROOT, *TOPIC_ROOT.parents) if (path / ".git").exists())

DATASET_RE = re.compile(
    r"^Dataset:\s+synthetic sac_model_normal_sphere (?P<point_type>\S+) "
    r"direct indexed normal-sphere cloud \(points=(?P<size>\d+),"
)
ITERATIONS_RE = re.compile(r"^Iterations:\s+(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s+(?P<warmup>\d+)")
BUILD_RE = re.compile(r"^Build:\s+(?P<build>Std|RVV)")
CHECKSUM_RE = re.compile(r"^Checksum:\s+(?P<checksum>\S+)")
TIMING_RE = re.compile(r"^(?P<label>.+?)\s*:\s+(?P<ms>[0-9.]+)\s+ms/iter$")
RVV_INSTRUCTION_RE = re.compile(r"\s+v[a-z0-9]+(?:\.[a-z0-9]+)*\s+")
KEY_MNEMONIC_RE = re.compile(
    r"\b(vle32\.v|vluxei32\.v|vluxei64\.v|vse32\.v|vse64\.v|"
    r"vfsqrt\.v|vfdiv\.vv|vfmacc\.vv|vfmacc\.vf|vmflt\.vf|vcpop\.m|vcompress\.vm)\b"
)

CASE_METADATA = {
    "public selectWithinDistance": {
        "case_kind": "mixed_boundary_cross_check",
        "evidence_role": "mixed_boundary_cross_check",
        "group": "normal-sphere-public-overload-cross-check",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelNormalSphere::selectWithinDistance",
        "timer_boundary": "public_select_function_call_no_input_setup",
        "gate": "production_scalar_entry_no_test_candidate_dispatch",
        "mask": "production_threshold_predicate",
        "reduction": "scalar_inlier_append",
        "notes": "公开入口尚未接入 RVV；此行只说明 Std/RVV build 下现有 public overload 的交叉检查，不作为 production direct RVV 证据。",
    },
    "public countWithinDistance": {
        "case_kind": "mixed_boundary_cross_check",
        "evidence_role": "mixed_boundary_cross_check",
        "group": "normal-sphere-public-overload-cross-check",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelNormalSphere::countWithinDistance",
        "timer_boundary": "public_count_function_call_no_input_setup",
        "gate": "production_scalar_entry_no_test_candidate_dispatch",
        "mask": "production_threshold_predicate",
        "reduction": "scalar_count",
        "notes": "公开入口尚未接入 RVV；此行只说明 Std/RVV build 下现有 public overload 的交叉检查，不作为 production direct RVV 证据。",
    },
    "public getDistancesToModel": {
        "case_kind": "mixed_boundary_cross_check",
        "evidence_role": "mixed_boundary_cross_check",
        "group": "normal-sphere-public-overload-cross-check",
        "boundary": "public_overload",
        "wrapper": "SampleConsensusModelNormalSphere::getDistancesToModel",
        "timer_boundary": "public_get_distances_function_call_no_input_setup",
        "gate": "production_scalar_entry_no_test_candidate_dispatch",
        "mask": "not_applicable_dense_output",
        "reduction": "scalar_dense_double_output",
        "notes": "公开入口尚未接入 RVV；此行只说明 Std/RVV build 下现有 public overload 的交叉检查，不作为 production direct RVV 证据。",
    },
    "diagnostic candidate selectWithinDistance": {
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "group": "normal-sphere-count-select-diagnostic",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelNormalSphereAccess::selectWithinDistanceCandidate",
        "timer_boundary": "candidate_select_function_call_no_input_setup",
        "gate": "RVV layout gate for PointT xyz fields and PointNT Normal fields",
        "mask": "distance_less_than_threshold_mask_then_scalar_lane_writeback",
        "reduction": "no_cross_lane_reduction_scalar_inlier_append",
        "notes": "测试专用 production-shaped diagnostic；证明公式、gather 和阈值输出可行，不证明真实 production dispatch 已接入。",
    },
    "diagnostic candidate vcompress selectWithinDistance": {
        "case_kind": "component_ablation",
        "evidence_role": "component_ablation",
        "group": "normal-sphere-select-vcompress-ablation",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelNormalSphereAccess::selectWithinDistanceVCompressCandidate",
        "timer_boundary": "candidate_vcompress_select_function_call_no_input_setup",
        "gate": "RVV layout gate for PointT xyz fields and PointNT Normal fields",
        "mask": "distance_less_than_threshold_mask_then_vcompress_writeback",
        "reduction": "vcompress_index_and_distance_writeback",
        "notes": "测试专用 select 写回实现族消融；只比较写回组织形态，不证明 production dispatch 已接入。",
    },
    "diagnostic candidate countWithinDistance": {
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "group": "normal-sphere-count-select-diagnostic",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelNormalSphereAccess::countWithinDistanceCandidate",
        "timer_boundary": "candidate_count_function_call_no_input_setup",
        "gate": "RVV layout gate for PointT xyz fields and PointNT Normal fields",
        "mask": "distance_less_than_threshold_mask",
        "reduction": "vcpop_mask_count",
        "notes": "测试专用 production-shaped diagnostic；证明公式、gather 和 mask count 可行，不证明真实 production dispatch 已接入。",
    },
    "diagnostic candidate getDistancesToModel": {
        "case_kind": "production_shaped_diagnostic",
        "evidence_role": "production_shaped_diagnostic",
        "group": "normal-sphere-getdistances-diagnostic",
        "boundary": "test_helper",
        "wrapper": "SampleConsensusModelNormalSphereAccess::getDistancesToModelCandidate",
        "timer_boundary": "candidate_get_distances_function_call_no_input_setup",
        "gate": "RVV layout gate for PointT xyz fields and PointNT Normal fields",
        "mask": "not_applicable_dense_output",
        "reduction": "no_cross_lane_reduction_vfwcvt_and_vse64_dense_output",
        "notes": "Phase 000 只把 getDistancesToModel 当 same-chain 正确性支撑；生产候选需要另起 dense-store audit。",
    },
}

PHASE_LABELS = {
    "000": tuple(label for label in CASE_METADATA if label != "diagnostic candidate vcompress selectWithinDistance"),
    "010": tuple(CASE_METADATA),
    "020": (
        "public getDistancesToModel",
        "diagnostic candidate getDistancesToModel",
    ),
    "030": tuple(CASE_METADATA),
    "060": (
        "public selectWithinDistance",
        "public countWithinDistance",
        "public getDistancesToModel",
    ),
}

PRODUCTION_PUBLIC_CASE_METADATA = {
    "public selectWithinDistance": {
        **CASE_METADATA["public selectWithinDistance"],
        "case_kind": "production_public",
        "evidence_role": "production_public",
        "group": "normal-sphere-production-public",
        "gate": "production dispatch: RVV build may enter normal-sphere RVV helper; Std build has no __RVV10__ dispatch",
        "reduction": "vcompress_index_and_distance_writeback_in_rvv_build",
        "notes": "Phase 060 production public（真实公开入口）证据；Std build 走标量 helper，RVV build 在 gate 命中时走 production RVV helper。",
    },
    "public countWithinDistance": {
        **CASE_METADATA["public countWithinDistance"],
        "case_kind": "production_public",
        "evidence_role": "production_public",
        "group": "normal-sphere-production-public",
        "gate": "production dispatch: RVV build may enter normal-sphere RVV helper; Std build has no __RVV10__ dispatch",
        "reduction": "vcpop_mask_count_in_rvv_build",
        "notes": "Phase 060 production public（真实公开入口）证据；Std build 走标量 helper，RVV build 在 gate 命中时走 production RVV helper。",
    },
    "public getDistancesToModel": {
        **CASE_METADATA["public getDistancesToModel"],
        "case_kind": "production_public",
        "evidence_role": "production_public",
        "group": "normal-sphere-production-public",
        "gate": "production dispatch: RVV build may enter normal-sphere RVV helper; Std build has no __RVV10__ dispatch",
        "reduction": "vfwcvt_f32_to_f64_then_dense_vse64_in_rvv_build",
        "notes": "Phase 060 production public（真实公开入口）证据；Std build 走标量 helper，RVV build 在 gate 命中时走 production RVV helper。",
    },
}


def repo_relative(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def sha256(path: Path) -> str | None:
    if not path.exists():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def parse_bench_log(path: Path, expected_build: str, labels: tuple[str, ...]) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(path)

    context: dict[str, Any] = {"path": repo_relative(path), "timings_ms": {}}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        text = line.strip()
        if match := DATASET_RE.match(text):
            context["point_type"] = match.group("point_type")
            context["size"] = int(match.group("size"))
            context["dataset"] = text.removeprefix("Dataset: ")
        elif match := ITERATIONS_RE.match(text):
            context["iterations"] = int(match.group("iterations"))
        elif match := WARMUP_RE.match(text):
            context["warmup_iterations"] = int(match.group("warmup"))
        elif match := BUILD_RE.match(text):
            context["build"] = match.group("build")
        elif match := CHECKSUM_RE.match(text):
            context["checksum"] = match.group("checksum")
        elif match := TIMING_RE.match(text):
            label = match.group("label").strip()
            if label in labels:
                context["timings_ms"][label] = float(match.group("ms"))

    required = ["point_type", "size", "iterations", "warmup_iterations", "build", "checksum"]
    missing = [key for key in required if key not in context]
    if missing:
        raise ValueError(f"{path} missing required field(s): {', '.join(missing)}")
    if context["build"] != expected_build:
        raise ValueError(f"{path} build={context['build']!r}, expected {expected_build!r}")
    if set(labels) - set(context["timings_ms"]):
        missing_labels = sorted(set(labels) - set(context["timings_ms"]))
        raise ValueError(f"{path} missing timing label(s): {', '.join(missing_labels)}")
    return context


def parse_run_dir(run_dir: Path, point_type: str, labels: tuple[str, ...]) -> dict[str, Any]:
    std = parse_bench_log(run_dir / "run_bench_std.log", "Std", labels)
    rvv = parse_bench_log(run_dir / "run_bench_rvv.log", "RVV", labels)
    for key in ("point_type", "size", "iterations", "warmup_iterations", "checksum"):
        if std[key] != rvv[key]:
            raise ValueError(f"{run_dir}: std/rvv {key} mismatch: {std[key]!r} vs {rvv[key]!r}")
    if std["point_type"] != point_type:
        raise ValueError(f"{run_dir}: point_type={std['point_type']!r}, expected {point_type!r}")
    return {
        "point_type": point_type,
        "run_dir": repo_relative(run_dir),
        "std": std,
        "rvv": rvv,
        "speedups": {
            label: std["timings_ms"][label] / rvv["timings_ms"][label]
            for label in labels
        },
    }


def parse_run_dirs(root: Path, point_type: str, labels: tuple[str, ...]) -> list[dict[str, Any]]:
    if (root / "run_bench_std.log").exists() and (root / "run_bench_rvv.log").exists():
        return [parse_run_dir(root, point_type, labels)]

    runs = [
        parse_run_dir(path.parent, point_type, labels)
        for path in sorted(root.glob("*/run_bench_std.log"))
        if (path.parent / "run_bench_rvv.log").exists()
    ]
    if not runs:
        raise FileNotFoundError(f"{root} has no run_bench_std.log/run_bench_rvv.log pair")
    return runs


def count_rvv_instructions(path: Path) -> tuple[int, dict[str, int]]:
    if not path.exists():
        return 0, {}
    total = 0
    mnemonics: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if RVV_INSTRUCTION_RE.search(line):
            total += 1
        if match := KEY_MNEMONIC_RE.search(line):
            mnemonic = match.group(1)
            mnemonics[mnemonic] = mnemonics.get(mnemonic, 0) + 1
    return total, mnemonics


def classify_bucket(speedup: float) -> str:
    if speedup >= 1.20:
        return "positive"
    if speedup >= 1.05:
        return "weak_positive"
    if speedup >= 0.95:
        return "neutral"
    return "negative"


def side_metadata(
    *,
    label: str,
    meta: dict[str, str],
    log: dict[str, Any],
    asm_boundary: str,
    rvv_instr_count: int,
    binary_hash: str | None,
) -> dict[str, Any]:
    return {
        "label": label,
        "boundary": meta["boundary"],
        "wrapper": meta["wrapper"],
        "row_source": "direct_indexed_source_and_normal_cloud",
        "solve": False,
        "checksum_policy": "aggregate_bench_checksum",
        "checksum": log["checksum"],
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": meta["mask"],
        "reduction": meta["reduction"],
        "asm_boundary": asm_boundary,
        "rvv_instr_count": rvv_instr_count,
        "binary_hash": binary_hash or "not_recorded",
    }


def make_comparisons(
    runs: list[dict[str, Any]],
    labels: tuple[str, ...],
    case_metadata: dict[str, dict[str, str]],
    asm_boundary: str,
    rvv_instr_count: int,
    rvv_mnemonics: dict[str, int],
    std_binary_hash: str | None,
    rvv_binary_hash: str | None,
) -> list[dict[str, Any]]:
    comparisons: list[dict[str, Any]] = []
    run = runs[0]
    for label in labels:
        meta = case_metadata[label]
        if label.startswith("public ") and meta["evidence_role"] != "production_public":
            continue
        std_values = [item["std"]["timings_ms"][label] for item in runs]
        rvv_values = [item["rvv"]["timings_ms"][label] for item in runs]
        speedup_values = [item["speedups"][label] for item in runs]
        std_ms = statistics.median(std_values)
        rvv_ms = statistics.median(rvv_values)
        speedup = statistics.median(speedup_values)
        role = meta["evidence_role"]
        if role == "production_public":
            baseline_label = "production-public-std"
            candidate_label = "production-public-rvv"
        else:
            baseline_label = "diagnostic-candidate-std-fallback"
            candidate_label = "diagnostic-candidate-rvv"
        comparisons.append(
            {
                "name": f"{run['point_type']} {label}",
                "group": f"{meta['group']}:{label}",
                "case_kind": meta["case_kind"],
                "evidence_role": role,
                "point_type": f"{run['point_type']} + Normal",
                "size": run["std"]["size"],
                "run_count": len(runs),
                "iterations": run["std"]["iterations"],
                "warmup_iterations": run["std"]["warmup_iterations"],
                "ba_values": speedup_values,
                "std_ms": std_ms,
                "rvv_ms": rvv_ms,
                "baseline_ms": std_ms,
                "candidate_ms": rvv_ms,
                "decision_bucket": classify_bucket(speedup),
                "source_logs": [
                    path
                    for item in runs
                    for path in (item["std"]["path"], item["rvv"]["path"])
                ],
                "notes": meta["notes"],
                "rvv_mnemonics": rvv_mnemonics,
                "baseline": {
                    **side_metadata(
                        label=baseline_label,
                        meta=meta,
                        log=run["std"],
                        asm_boundary=asm_boundary,
                        rvv_instr_count=0,
                        binary_hash=std_binary_hash,
                    ),
                    "std_ms": std_ms,
                },
                "candidate": {
                    **side_metadata(
                        label=candidate_label,
                        meta=meta,
                        log=run["rvv"],
                        asm_boundary=asm_boundary,
                        rvv_instr_count=rvv_instr_count,
                        binary_hash=rvv_binary_hash,
                    ),
                    "rvv_ms": rvv_ms,
                },
            }
        )
    return comparisons


def make_select_family_comparisons(
    run: dict[str, Any],
    asm_boundary: str,
    rvv_instr_count: int,
    rvv_mnemonics: dict[str, int],
    rvv_binary_hash: str | None,
) -> list[dict[str, Any]]:
    baseline_label = "diagnostic candidate selectWithinDistance"
    candidate_label = "diagnostic candidate vcompress selectWithinDistance"
    if candidate_label not in run["rvv"]["timings_ms"]:
        return []

    baseline_meta = CASE_METADATA[baseline_label]
    candidate_meta = CASE_METADATA[candidate_label]
    baseline_ms = run["rvv"]["timings_ms"][baseline_label]
    candidate_ms = run["rvv"]["timings_ms"][candidate_label]
    speedup = baseline_ms / candidate_ms
    return [
        {
            "name": f"{run['point_type']} vcompress select vs scalar-writeback select",
            "group": "normal-sphere-select-vcompress-ablation",
            "case_kind": "component_ablation",
            "evidence_role": "component_ablation",
            "point_type": f"{run['point_type']} + Normal",
            "size": run["rvv"]["size"],
            "run_count": 1,
            "iterations": run["rvv"]["iterations"],
            "warmup_iterations": run["rvv"]["warmup_iterations"],
            "ba_values": [speedup],
            "baseline_ms": baseline_ms,
            "candidate_ms": candidate_ms,
            "decision_bucket": classify_bucket(speedup),
            "source_logs": [run["rvv"]["path"]],
            "notes": "同一 RVV build 内的 select 写回实现族消融；B/A = scalar-writeback RVV ms / vcompress RVV ms，>1 表示 vcompress 更快。",
            "rvv_mnemonics": rvv_mnemonics,
            "allowed_contract_mismatches": ["wrapper", "timer_boundary", "mask", "reduction"],
            "baseline": {
                **side_metadata(
                    label="rvv-scalar-writeback-select",
                    meta=baseline_meta,
                    log=run["rvv"],
                    asm_boundary=asm_boundary,
                    rvv_instr_count=rvv_instr_count,
                    binary_hash=rvv_binary_hash,
                ),
                "rvv_ms": baseline_ms,
            },
            "candidate": {
                **side_metadata(
                    label="rvv-vcompress-select",
                    meta=candidate_meta,
                    log=run["rvv"],
                    asm_boundary=asm_boundary,
                    rvv_instr_count=rvv_instr_count,
                    binary_hash=rvv_binary_hash,
                ),
                "rvv_ms": candidate_ms,
            },
        }
    ]


def percentile(values: list[float], pct: float) -> float:
    if len(values) == 1:
        return values[0]
    ordered = sorted(values)
    pos = (len(ordered) - 1) * pct
    lo = int(pos)
    hi = min(lo + 1, len(ordered) - 1)
    frac = pos - lo
    return ordered[lo] * (1.0 - frac) + ordered[hi] * frac


def write_summary(path: Path, manifest: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path = manifest["summary"].get("manifest_path", path.with_name("board-evidence-manifest.json"))
    manifest_path_text = manifest_path if isinstance(manifest_path, str) else repo_relative(Path(manifest_path))
    public_context = manifest.get("public_context", {})
    run_count = int(manifest["summary"].get("metadata", {}).get("run_count", 1))
    if run_count >= 5:
        rerun_budget = f"`{run_count}/5 used`; repeated board buckets are stable, so no extra rerun was required."
        boundary_note = (
            f"这份 summary 来自 {run_count}-run repeated board（重复板卡测试）中位数，"
            "用于支撑当前 production-public（真实公开入口）性能结论。"
        )
    else:
        rerun_budget = "`1/5 used`; current buckets are far from threshold for candidate rows, so no automatic rerun was required."
        boundary_note = "这份 summary 来自单次 board smoke（板卡小型验证），不是 repeated board（重复板卡测试）稳定性结论。"
    lines = [
        f"# {manifest['summary']['title'].replace(' manifest', ' summary')}",
        "",
        f"- run_label: `{manifest['summary']['label']}`",
        f"- evidence_role: `{manifest['summary']['evidence_role']}`.",
        f"- A/B boundary: {manifest['summary']['ab_boundary']}",
        f"- rerun_budget: {rerun_budget}",
        f"- manifest: `{manifest_path_text}`",
        "",
        "| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |",
        "| --- | --- | ---: | ---: | ---: | --- | --- |",
    ]
    for comp in manifest["comparisons"]:
        values = comp["ba_values"]
        lines.append(
            "| `{}` | `{}` | {:.4f} | {:.4f} | {:.3f}x | `{}` | `{}` |".format(
                comp["name"],
                comp["point_type"],
                comp["baseline_ms"],
                comp["candidate_ms"],
                statistics.median(values),
                comp["decision_bucket"],
                comp["evidence_role"],
            )
        )
    if any(public_context.values()):
        lines.extend(
            [
                "",
                "## Public 入口上下文",
                "",
                "公开入口上下文只记录未进入当前 candidate comparison 的 public 行。Phase 060 的 public 行已经作为 production-public comparison 进入上表。",
                "",
                "| case | " + " | ".join(f"{point_type} speedup" for point_type in public_context) + " | role |",
                "| --- | " + " | ".join("---:" for _ in public_context) + " | --- |",
            ]
        )
        for label in ("public selectWithinDistance", "public countWithinDistance", "public getDistancesToModel"):
            speedup_cells = []
            for point_type in public_context:
                speedup = public_context.get(point_type, {}).get(label, "n/a")
                speedup_cells.append(f"{speedup:.3f}x" if isinstance(speedup, (int, float)) else str(speedup))
            lines.append(
                f"| `{label}` | {' | '.join(speedup_cells)} | `mixed_boundary_cross_check` |"
            )
    lines.extend(
        [
            "",
            "## Evidence Doctor 输入边界",
            "",
            boundary_note,
            manifest["summary"]["conclusion_policy"],
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pointxyz-dir", default=TOPIC_ROOT / "log/board", type=Path)
    parser.add_argument("--pointxyz-type", default="PointXYZ")
    parser.add_argument("--pointxyzi-dir", default=TOPIC_ROOT / "log/board-pointxyzi", type=Path)
    parser.add_argument("--pointxyzi-type", default="PointXYZI")
    parser.add_argument("--pointxyzrgb-dir", type=Path)
    parser.add_argument("--pointxyzrgba-dir", type=Path)
    parser.add_argument("--asm", default=TOPIC_ROOT / "build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm", type=Path)
    parser.add_argument("--std-binary", default=TOPIC_ROOT / "build/riscv/bench_sac_model_normal_sphere_std", type=Path)
    parser.add_argument("--rvv-binary", default=TOPIC_ROOT / "build/riscv/bench_sac_model_normal_sphere_rvv", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--run-label", default="normal-sphere-phase000-board-smoke")
    parser.add_argument("--phase", choices=sorted(PHASE_LABELS), default="000")
    args = parser.parse_args()

    labels = PHASE_LABELS[args.phase]
    case_metadata = PRODUCTION_PUBLIC_CASE_METADATA if args.phase == "060" else CASE_METADATA
    run_groups = [
        parse_run_dirs(args.pointxyz_dir, args.pointxyz_type, labels),
        parse_run_dirs(args.pointxyzi_dir, args.pointxyzi_type, labels),
    ]
    if args.phase == "060":
        if args.pointxyzrgb_dir is None or args.pointxyzrgba_dir is None:
            raise ValueError("Phase 060 requires --pointxyzrgb-dir and --pointxyzrgba-dir")
        run_groups.extend(
            [
                parse_run_dirs(args.pointxyzrgb_dir, "PointXYZRGB", labels),
                parse_run_dirs(args.pointxyzrgba_dir, "PointXYZRGBA", labels),
            ]
        )
    rvv_instr_count, rvv_mnemonics = count_rvv_instructions(args.asm)
    asm_boundary = (
        "SampleConsensusModelNormalSphereAccess::distancesRVV and candidate RVV helpers in bench_sac_model_normal_sphere_rvv"
        if rvv_instr_count
        else "missing_asm_attribution"
    )
    comparisons: list[dict[str, Any]] = []
    for runs in run_groups:
        comparisons.extend(
            make_comparisons(
                runs,
                labels,
                case_metadata,
                asm_boundary,
                rvv_instr_count,
                rvv_mnemonics,
                sha256(args.std_binary),
                sha256(args.rvv_binary),
            )
        )
        if args.phase == "010":
            comparisons.extend(
                make_select_family_comparisons(
                    runs[0],
                    asm_boundary,
                    rvv_instr_count,
                    rvv_mnemonics,
                    sha256(args.rvv_binary),
                )
            )

    representative_runs = [runs[0] for runs in run_groups]
    first = representative_runs[0]["std"]
    run_counts = [len(runs) for runs in run_groups]
    phase_name_by_id = {
        "000": "Phase 000 board smoke",
        "010": "Phase 010 vcompress select ablation",
        "020": "Phase 020 getDistances dense-store audit",
        "030": "Phase 030 RGB/RGBA point-type expansion",
        "060": "Phase 060 production public integration",
    }
    evidence_role_by_id = {
        "000": "production_shaped_diagnostic",
        "010": "component_ablation",
        "020": "production_shaped_diagnostic",
        "030": "production_shaped_diagnostic",
        "060": "production_public",
    }
    ab_boundary_by_id = {
        "000": "board Std build fallback vs board RVV build candidate for the same test-only helper.",
        "010": "board RVV scalar-writeback select helper vs board RVV vcompress select helper for Phase 010 family comparison.",
        "020": "board Std build fallback vs board RVV build getDistances test-only helper for dense double output.",
        "030": "board Std build fallback vs board RVV build test-only candidates for RGB/RGBA source layouts.",
        "060": "board Std build public overload vs board RVV build public overload after production dispatch integration.",
    }
    conclusion_policy_by_id = {
        "000": "候选行的速度信号足够远离 1.0 阈值，可用于 Phase 000 是否进入下一阶段的诊断判断；production 接入仍需要 PI1-PI5。",
        "010": "Phase 010 的写回实现族 B/A 可用于判断 `vcompress` 是否进入 PI1 审计；production 接入仍需要 PI1-PI5。",
        "020": "Phase 020 只判断 dense double store 诊断候选是否仍值得保留；即使正向，也不能替代 production direct（真实生产路径证据）。",
        "030": "Phase 030 只判断 RGB/RGBA source layout 是否能加入测试专用候选范围；即使正向，也不能替代 production direct（真实生产路径证据）。",
        "060": "Phase 060 使用接入后的真实 public entry Std/RVV 板卡对比；若 correctness、asm 和 Evidence Doctor 无 Error 且速度桶为正向，可支撑本阶段生产采纳判断。",
    }
    phase_name = phase_name_by_id[args.phase]
    evidence_role = evidence_role_by_id[args.phase]
    ab_boundary = ab_boundary_by_id[args.phase]
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": f"sac_model_normal_sphere {phase_name} manifest",
            "label": args.run_label,
            "evidence_role": evidence_role,
            "ab_boundary": ab_boundary,
            "conclusion_policy": conclusion_policy_by_id[args.phase],
            "summary_path": repo_relative(args.summary_output),
            "manifest_path": repo_relative(args.output),
            "metadata": {
                "device": "Milkv-Jupiter",
                "iterations": first["iterations"],
                "warmup_iterations": first["warmup_iterations"],
                "run_count": min(run_counts),
                "run_count_by_point_type": {
                    runs[0]["point_type"]: len(runs)
                    for runs in run_groups
                },
                "taskset": "not_recorded_by_board_smoke",
                "governor": "not_recorded_by_board_smoke",
                "freq": "not_recorded_by_board_smoke",
                "temperature": "not_recorded_by_board_smoke",
                "vlen": "see SoC / ELF (board)",
                "binary_hash": sha256(args.rvv_binary) or "not_recorded",
            },
        },
        "public_context": {
            run["point_type"]: {
                label: run["speedups"][label]
                for label in labels
                if label.startswith("public ") and case_metadata[label]["evidence_role"] != "production_public"
            }
            for run in representative_runs
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
                "ba_degrade_threshold": 1.0,
                "near_threshold_margin": 0.05,
                "long_tail_ratio": 1.15,
            },
        },
        "comparisons": comparisons,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_summary(args.summary_output, manifest)
    print(f"wrote manifest: {repo_relative(args.output)}")
    print(f"wrote summary: {repo_relative(args.summary_output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
