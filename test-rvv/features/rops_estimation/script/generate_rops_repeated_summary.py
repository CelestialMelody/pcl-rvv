#!/usr/bin/env python3
"""Generate ROPS repeated board summary and Evidence Doctor manifest."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


DEFAULT_CASE_LABEL = "rops_distribution_matrix_binning"
COMPARE_RVV_RE = re.compile(
    r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9.]+)\s*us\s*\|.*?\[\s*(?P<speedup>[0-9.]+)x\s*\]"
)
CHECKSUM_RE = re.compile(r"^\s*Checksum:\s*(?P<checksum>\d+)\s*$", re.MULTILINE)
ITER_RE = re.compile(r"^\s*Iterations:\s*(?P<iterations>\d+)\s*$", re.MULTILINE)
WARMUP_RE = re.compile(r"^\s*Warmup Iterations:\s*(?P<warmup>\d+)\s*$", re.MULTILINE)


def compile_compare_std_re(case_label: str, value_key: str) -> re.Pattern[str]:
    return re.compile(
        rf"^{case_label},points=(?P<points>\d+),{value_key}=(?P<value>\d+)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9.]+)\s*us\s*\|"
    )


def compile_raw_row_re(case_label: str, value_key: str) -> re.Pattern[str]:
    return re.compile(
        rf"^{case_label},points=(?P<points>\d+),{value_key}=(?P<value>\d+)\s*:\s*(?P<avg>[0-9.]+)\s+us\s*/\s*iter\s*$",
        re.MULTILINE,
    )


def parse_compare(path: Path, case_label: str, value_key: str) -> tuple[int, int, float, float, float]:
    compare_std_re = compile_compare_std_re(case_label, value_key)
    points: int | None = None
    value: int | None = None
    std_us: float | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if match := compare_std_re.match(line):
            points = int(match.group("points"))
            value = int(match.group("value"))
            std_us = float(match.group("avg"))
            continue
        if points is not None and value is not None and std_us is not None and (
            match := COMPARE_RVV_RE.match(line)
        ):
            rvv_us = float(match.group("avg"))
            return points, value, std_us, rvv_us, float(match.group("speedup"))
    raise SystemExit(f"cannot parse compare log: {path}")


def parse_raw(path: Path, case_label: str, value_key: str) -> dict[str, int | float | str | None]:
    text = path.read_text(encoding="utf-8", errors="replace")
    row = compile_raw_row_re(case_label, value_key).search(text)
    checksum = CHECKSUM_RE.search(text[row.end() :]) if row else None
    iterations = ITER_RE.search(text)
    warmup = WARMUP_RE.search(text)
    return {
        "avg_us": float(row.group("avg")) if row else None,
        "checksum": checksum.group("checksum") if checksum else None,
        "iterations": int(iterations.group("iterations")) if iterations else None,
        "warmup_iterations": int(warmup.group("warmup")) if warmup else None,
    }


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * pct
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def decision_bucket(values: list[float]) -> str:
    median = statistics.median(values)
    below_one = sum(1 for value in values if value < 1.0)
    if median >= 1.15 and below_one == 0:
        return "positive"
    if median >= 1.05 and below_one <= max(1, len(values) // 5):
        return "weak_positive"
    if 0.95 <= median < 1.05:
        return "neutral"
    if median < 0.95:
        return "negative"
    return "unstable"


def unique_or_mixed(values: list[object | None]) -> object | str | None:
    present = [value for value in values if value is not None]
    if not present:
        return None
    first = present[0]
    return first if all(value == first for value in present) else "mixed"


def case_metadata(case_label: str) -> dict[str, str]:
    typed_production_cases = {
        "rops_production_pointxyzi_rotate_distribution_pipeline": "PointXYZI",
        "rops_production_pointnormal_rotate_distribution_pipeline": "PointNormal",
    }
    if case_label in typed_production_cases:
        point_type = typed_production_cases[case_label]
        return {
            "title": f"ROPS {point_type} production rotate + distribution detail repeated board summary",
            "group": f"rops-production-{point_type.lower()}-rotate-distribution-detail",
            "evidence_role": "production_detail",
            "case_kind": "production-detail",
            "point_type": point_type,
            "ab_boundary": "production private helper",
            "value_key": "repeat",
            "timer_boundary": "production_rotate_cloud_aabb_then_three_distribution_matrices",
            "row_source": f"synthetic_{point_type}_transformed_local_cloud_to_production_rotated_cloud_to_bins",
            "checksum_policy": "production_rotated_cloud_aabb_and_distribution_matrix_aggregate_checksum",
            "gate": (
                f"{point_type} finite dense synthetic cloud, RVVXYZAoSFloatLayout traits gate, "
                "production private helper dispatch, X/Y/Z and tilted axis rotations, XY/XZ/YZ projections"
            ),
            "mask": "no finite mask; dense synthetic input and production gate fallback outside this boundary",
            "reduction": "production RVV vector min/max reduction for rotated xyz AABB; scalar scatter after vector row/col staging",
            "asm_boundary": f"pcl::ROPSEstimation<{point_type}, Histogram<135>>::rotateCloud/getDistributionMatrix via bench_rops_estimation_rvv",
            "asm_ops": "vlse32,vsse32,vfmacc,vfmin,vfmax,vfredmin,vfredmax,vfcvt.rtz,vminu,vmerge,vse32",
            "boundary_text": (
                f"这是 {point_type} production-detail（生产私有 helper 直连）性能证据。Std/RVV 两侧共享同一个 "
                "bench wrapper；Std build 因未定义 `__RVV10__` 走 production scalar body，RVV build "
                "通过真实 `ROPSEstimation::rotateCloud()` 和 `getDistributionMatrix()` 的 production "
                "dispatch 命中 RVV helper。计时覆盖 production private helper 的 rotateCloud + AABB、"
                "rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 "
                "scatter；不包含 LRF、central moments、mesh local surface、descriptor normalization 或完整 "
                "public `computeFeature()`。额外字段不参与当前 RVV stage，本结果不证明这些字段的输出语义。"
            ),
        }
    if case_label == "rops_production_rotate_distribution_pipeline":
        return {
            "title": "ROPS production rotate + distribution detail repeated board summary",
            "group": "rops-production-rotate-distribution-detail",
            "evidence_role": "production_detail",
            "case_kind": "production-detail",
            "point_type": "PointXYZ",
            "ab_boundary": "production private helper",
            "value_key": "repeat",
            "timer_boundary": "production_rotate_cloud_aabb_then_three_distribution_matrices",
            "row_source": "synthetic_transformed_local_cloud_to_production_rotated_cloud_to_bins",
            "checksum_policy": "production_rotated_cloud_aabb_and_distribution_matrix_aggregate_checksum",
            "gate": "PointXYZ finite dense synthetic cloud, production private helper dispatch, X/Y/Z and tilted axis rotations, XY/XZ/YZ projections",
            "mask": "no finite mask; dense synthetic input and production gate fallback outside this boundary",
            "reduction": "production RVV vector min/max reduction for rotated xyz AABB; scalar scatter after vector row/col staging",
            "asm_boundary": "pcl::ROPSEstimation<PointXYZ, Histogram<135>>::rotateCloud/getDistributionMatrix via bench_rops_estimation_rvv",
            "asm_ops": "vlse32,vsse32,vfmacc,vfmin,vfmax,vfredmin,vfredmax,vfcvt.rtz,vminu,vmerge,vse32",
            "boundary_text": (
                "这是 production-detail（生产私有 helper 直连）性能证据。Std/RVV 两侧共享同一个 "
                "bench wrapper；Std build 因未定义 `__RVV10__` 走 production scalar body，RVV build "
                "通过真实 `ROPSEstimation::rotateCloud()` 和 `getDistributionMatrix()` 的 production "
                "dispatch 命中 RVV helper。计时覆盖 production private helper 的 rotateCloud + AABB、"
                "rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 "
                "scatter；不包含 LRF、central moments、mesh local surface、descriptor normalization 或完整 "
                "public `computeFeature()`。"
            ),
        }
    if case_label == "rops_rotate_distribution_pipeline":
        return {
            "title": "ROPS combined rotate + distribution diagnostic repeated board summary",
            "group": "rops-combined-rotate-distribution-diagnostic",
            "evidence_role": "production_shaped_diagnostic",
            "case_kind": "production-shaped-diagnostic",
            "point_type": "PointXYZ",
            "ab_boundary": "test helper",
            "value_key": "repeat",
            "timer_boundary": "rotate_cloud_aabb_then_three_distribution_matrices",
            "row_source": "synthetic_transformed_local_cloud_to_rotated_cloud_to_bins",
            "checksum_policy": "rotated_cloud_aabb_and_distribution_matrix_aggregate_checksum",
            "gate": "PointXYZ finite dense synthetic cloud, X/Y/Z and tilted axis rotations, XY/XZ/YZ projections",
            "mask": "no finite mask; synthetic finite input",
            "reduction": "vector min/max reduction for rotated xyz AABB; scalar scatter after vector row/col staging",
            "asm_boundary": "rotateCloudAndDistributionMatricesRVV in bench_rops_estimation_rvv",
            "asm_ops": "vlse32,vsse32,vfmacc,vfmin,vfmax,vfredmin,vfredmax,vfcvt.rtz,vminu,vmerge,vse32",
            "boundary_text": (
                "这是未接 production 的 production-shaped diagnostic（生产形态诊断）性能证据。Std/RVV "
                "两侧共享同一个 bench wrapper；Std build 走 scalar fallback，RVV build 走 "
                "`rotateCloudAndDistributionMatricesRVV()` 组合候选。计时覆盖 rotateCloud + AABB、"
                "rotated cloud buffer 写回、3 个 projection 的 distribution matrix row/col staging 和标量 scatter；"
                "不包含 LRF、central moments、mesh local surface、descriptor normalization 或 public dispatch。"
            ),
        }
    if case_label == "rops_rotate_cloud_aabb":
        return {
            "title": "ROPS rotateCloud + AABB diagnostic repeated board summary",
            "group": "rops-rotate-cloud-aabb-diagnostic",
            "evidence_role": "diagnostic",
            "case_kind": "component-ablation",
            "point_type": "PointXYZ",
            "ab_boundary": "test helper",
            "value_key": "repeat",
            "timer_boundary": "rotate_cloud_aos_3x3_store_and_aabb_reduction",
            "row_source": "synthetic_transformed_local_cloud",
            "checksum_policy": "rotated_cloud_and_aabb_quantized_checksum",
            "gate": "PointXYZ finite dense synthetic cloud, X/Y/Z and tilted axis rotations",
            "mask": "no finite mask; synthetic finite input",
            "reduction": "vector min/max reduction for rotated xyz AABB",
            "asm_boundary": "rotateCloudRVV in bench_rops_estimation_rvv",
            "asm_ops": "vlse32,vsse32,vfmacc,vfmin,vfmax,vfredmin,vfredmax",
            "boundary_text": (
                "这是未接 production 的 component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 "
                "bench wrapper；Std build 走 scalar fallback，RVV build 走 `rotateCloudRVV()` 候选。计时覆盖 "
                "AoS stride load/store、3x3 rotation 和 AABB min/max reduction，不包含 LRF、distribution matrix、"
                "central moments、mesh local surface 或完整 descriptor normalization。"
            ),
        }
    return {
        "title": "ROPS distribution matrix diagnostic repeated board summary",
        "group": "rops-distribution-matrix-diagnostic",
        "evidence_role": "diagnostic",
        "case_kind": "component-ablation",
        "point_type": "PointXYZ",
        "ab_boundary": "test helper",
        "value_key": "bins",
        "timer_boundary": "distribution_matrix_binning_with_row_col_staging_and_scalar_scatter",
        "row_source": "rotated_local_cloud_synthetic",
        "checksum_policy": "distribution_matrix_quantized_checksum",
        "gate": "PointXYZ finite dense synthetic cloud, XY/XZ/YZ projections, fixed bins",
        "mask": "no finite mask; synthetic finite input",
        "reduction": "not_applicable; scalar scatter accumulation after vector row/col staging",
        "asm_boundary": "getDistributionMatrixRVV in bench_rops_estimation_rvv",
        "asm_ops": "vlse32,vfsub,vfmul,vfcvt.rtz,vminu,vmerge,vse32",
        "boundary_text": (
            "这是未接 production 的 component diagnostic（组件诊断）性能证据。Std/RVV 两侧共享同一个 "
            "bench wrapper；Std build 走 scalar fallback，RVV build 走 `getDistributionMatrixRVV()` 候选。"
            "计时包含 row/col staging 和标量 scatter，不包含 LRF、rotateCloud、mesh local surface 或完整 "
            "descriptor normalization。"
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--run-label", required=True)
    parser.add_argument("--expected-runs", required=True, type=int)
    parser.add_argument("--asm", required=True, type=Path)
    parser.add_argument("--case-label", default=DEFAULT_CASE_LABEL)
    args = parser.parse_args()
    meta = case_metadata(args.case_label)

    run_dirs = sorted(path for path in args.input_dir.glob("run-*") if path.is_dir())
    if not run_dirs:
        raise SystemExit(f"no run-* directories under {args.input_dir}")

    asm_lines = 0
    if args.asm.exists():
        ops = tuple(meta["asm_ops"].split(","))
        asm_lines = sum(
            1
            for line in args.asm.read_text(encoding="utf-8", errors="ignore").splitlines()
            if any(op in line for op in ops)
        )

    rows: list[dict[str, object]] = []
    for run_dir in run_dirs:
        points, value, std_us, rvv_us, speedup = parse_compare(
            run_dir / "analyze_bench_compare.log", args.case_label, meta["value_key"]
        )
        std_raw = parse_raw(run_dir / "run_bench_std.log", args.case_label, meta["value_key"])
        rvv_raw = parse_raw(run_dir / "run_bench_rvv.log", args.case_label, meta["value_key"])
        rows.append(
            {
                "run": run_dir.name,
                "points": points,
                "value": value,
                "std_us": std_us,
                "rvv_us": rvv_us,
                "speedup": speedup,
                "std_checksum": std_raw.get("checksum"),
                "rvv_checksum": rvv_raw.get("checksum"),
                "iterations": std_raw.get("iterations") or rvv_raw.get("iterations"),
                "warmup_iterations": std_raw.get("warmup_iterations") or rvv_raw.get("warmup_iterations"),
            }
        )

    values = [float(row["speedup"]) for row in rows]
    bucket = decision_bucket(values)
    below_one = sum(1 for value in values if value < 1.0)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"# {meta['title']}",
        "",
        f"- run_label: `{args.run_label}`",
        f"- expected_runs: `{args.expected_runs}`",
        f"- collected_runs: `{len(rows)}`",
        f"- evidence_role: `{meta['evidence_role']}`",
        f"- A/B boundary: `{meta['ab_boundary']}`",
        f"- timer_boundary: `{meta['timer_boundary']}`",
        f"- decision_bucket: `{bucket}`",
        "",
        "| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | values |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        "| `{},points={},{}={}` | {} | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {:.3f}x | {}/{} | {} |".format(
            args.case_label,
            rows[0]["points"],
            meta["value_key"],
            rows[0]["value"],
            len(rows),
            statistics.median(values),
            min(values),
            max(values),
            percentile(values, 0.10),
            percentile(values, 0.90),
            below_one,
            len(rows),
            ", ".join(f"{value:.3f}x" for value in values),
        ),
        "",
        "## 每轮明细",
        "",
        "| run | Std us/iter | RVV us/iter | speedup | checksum match |",
        "| --- | ---: | ---: | ---: | --- |",
    ]
    for row in rows:
        lines.append(
            "| `{run}` | {std:.4f} | {rvv:.4f} | {speed:.3f}x | {match} |".format(
                run=row["run"],
                std=float(row["std_us"]),
                rvv=float(row["rvv_us"]),
                speed=float(row["speedup"]),
                match="yes" if row["std_checksum"] == row["rvv_checksum"] else "no",
            )
        )
    lines += [
        "",
        "## Evidence boundary",
        "",
        meta["boundary_text"],
    ]
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    iterations = unique_or_mixed([row["iterations"] for row in rows])
    warmup = unique_or_mixed([row["warmup_iterations"] for row in rows])
    common = {
        "boundary": meta["ab_boundary"].replace(" ", "_"),
        "wrapper": "test-rvv/features/rops_estimation/src/bench_rops_estimation.cpp",
        "row_source": meta["row_source"],
        "solve": False,
        "checksum_policy": meta["checksum_policy"],
        "checksum": rows[0]["std_checksum"],
        "timer_boundary": meta["timer_boundary"],
        "gate": meta["gate"],
        "mask": meta["mask"],
        "reduction": meta["reduction"],
        "asm_boundary": meta["asm_boundary"],
        "rvv_instr_count": asm_lines,
    }
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": meta["title"],
            "evidence_role": meta["evidence_role"],
            "summary_path": str(args.output),
            "label": args.run_label,
            "analysis_script": "test-rvv/features/rops_estimation/script/generate_rops_repeated_summary.py",
            "metadata": {
                "device": "board via Makefile config",
                "iterations": iterations,
                "warmup_iterations": warmup,
                "run_count": len(rows),
                "decision_bucket": bucket,
                "expected_runs": args.expected_runs,
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
        "comparisons": [
            {
                "name": f"{args.case_label},points={rows[0]['points']},{meta['value_key']}={rows[0]['value']}",
                "group": meta["group"],
                "evidence_role": meta["evidence_role"],
                "case_kind": meta["case_kind"],
                "point_type": meta["point_type"],
                "size": rows[0]["points"],
                "run_count": len(rows),
                "iterations": iterations,
                "warmup_iterations": warmup,
                "build_comparison": True,
                "std_ms": statistics.mean(float(row["std_us"]) for row in rows) / 1000.0,
                "rvv_ms": statistics.mean(float(row["rvv_us"]) for row in rows) / 1000.0,
                "ba_values": values,
                "baseline": {"label": "Std build", **common},
                "candidate": {"label": "RVV build", **common},
            }
        ],
    }
    args.manifest.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote summary: {args.output}")
    print(f"wrote manifest: {args.manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
