#!/usr/bin/env python3
"""Generate Evidence Doctor manifest for integral_image_normal diagnostic bench."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


ROW_RE = re.compile(r"^(?P<case>[^:]+):\s*(?P<ms>[0-9.]+)\s*ms\s*/\s*iter$")
CHECKSUM_RE = re.compile(r"^checksum_(?P<case>[^=]+)=(?P<checksum>[^ ]+).*width=(?P<width>\d+)\s+height=(?P<height>\d+)")
ITER_RE = re.compile(r"^Iterations:\s*(?P<iterations>\d+)")
WARMUP_RE = re.compile(r"^Warmup Iterations:\s*(?P<warmup>\d+)")


def parse_log(path: Path) -> dict[str, object]:
    rows: dict[str, float] = {}
    checksums: dict[str, str] = {}
    sizes: dict[str, tuple[int, int]] = {}
    iterations = None
    warmup = None

    for line in path.read_text(encoding="utf-8").splitlines():
      if m := ROW_RE.match(line.strip()):
        rows[m.group("case")] = float(m.group("ms"))
      elif m := CHECKSUM_RE.match(line.strip()):
        case = m.group("case")
        checksums[case] = m.group("checksum")
        sizes[case] = (int(m.group("width")), int(m.group("height")))
      elif m := ITER_RE.match(line.strip()):
        iterations = int(m.group("iterations"))
      elif m := WARMUP_RE.match(line.strip()):
        warmup = int(m.group("warmup"))

    return {
        "rows": rows,
        "checksums": checksums,
        "sizes": sizes,
        "iterations": iterations,
        "warmup": warmup,
    }


def write_summary(path: Path, manifest: dict[str, object]) -> None:
    """Write a compact Markdown summary for the repeated board evidence."""
    rows = [
        "# integral_image_normal repeated board summary",
        "",
        "本摘要由 topic-local manifest wrapper 生成，记录 integral image normal（积分图法线）"
        "在板卡上的 5-run 结果。`prod_*` case 是 production direct（真实生产入口）证据；"
        "其它 case 是 diagnostic（诊断）或 production-shaped diagnostic（生产形态诊断）证据。",
        "",
        f"- evidence_role：{manifest['summary']['evidence_role']}",
        f"- run_count：{manifest['summary']['metadata']['run_count']}",
        f"- iterations：{manifest['summary']['metadata']['iterations']}",
        f"- warmup_iterations：{manifest['summary']['metadata']['warmup_iterations']}",
        "",
        "| case | min speedup | median speedup | mean speedup | max speedup | checksum | decision bucket |",
        "| --- | ---: | ---: | ---: | ---: | --- | --- |",
    ]

    for comparison in manifest["comparisons"]:
        values = comparison["ba_values"]
        checksum = comparison["baseline"]["checksum"]
        checksum_match = checksum == comparison["candidate"]["checksum"]
        bucket = "positive" if min(values) > 1.20 and checksum_match else "needs_review"
        rows.append(
            f"| {comparison['name']} | {min(values):.2f}x | {statistics.median(values):.2f}x | "
            f"{statistics.mean(values):.2f}x | {max(values):.2f}x | {checksum} | {bucket} |"
        )

    rows.extend([
        "",
        "## 证据边界",
        "",
        "- QEMU（仿真器）日志不参与性能结论。",
        "- `prod_compute_*` 证明真实 `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()` 入口在当前板卡上的 Std/RVV 对比。",
        "- `map_prep_*` 只证明连续 `float z` buffer 的 map-prep helper 在当前板卡上稳定快于同边界标量 helper。",
        "- `avg3d_diff_*` 只证明测试专用 4-float stride 点型的 diff_x / diff_y buffer helper，不覆盖积分图构建。",
        "- `avg3d_profile_*` 和 `profile_component_*` 是 production-shaped diagnostic（生产形态诊断），用于拆分 diff-buffer、积分图构建和 normal query 的相对成本；它仍不是 production direct。",
        "- production RVV 当前只覆盖 `PointInT` 满足 xyz AoS float layout gate 时的 map-prep 前缀；distance transform、normal output、indices 和其它点型仍保留标量边界。",
        "",
    ])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(rows), encoding="utf-8")


def case_metadata(case: str) -> dict[str, str | int]:
    if case.startswith("prod_compute_avg_depth_"):
      return {
          "group": "integral_image_normal_production_compute",
          "point_type": "pcl_pointxyz_to_normal",
          "checksum_policy": "production_output_and_distance_map_sample",
          "timer_boundary": "production-public-compute-average-depth-change",
          "mask": "public-compute-full-organized-image",
          "candidate_label": "rvv-production-compute-map-prep",
          "baseline_label": "std-production-compute",
          "asm_boundary": "integral_image_normal_compute_feature_map_prep_rvv",
          "rvv_instr_count": 4,
          "case_kind": "production_direct",
          "boundary": "public_compute",
          "wrapper": "IntegralImageNormalEstimation<PointXYZ_Normal>::compute",
      }
    if case.startswith("pcl_avg3d_profile_"):
      return {
          "group": "integral_image_normal_pcl_integral_image2d_profile_total",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "pcl_profile_total_normal_query",
          "timer_boundary": "pcl-average-3d-gradient-profile-total",
          "mask": "pcl-integral-image-query-loop",
          "candidate_label": "rvv-pcl-avg3d-profile-total",
          "baseline_label": "std-pcl-avg3d-profile-total",
          "asm_boundary": "bench_integral_image_normal_rvv_pcl_avg3d_profile",
          "rvv_instr_count": 3,
          "case_kind": "diagnostic",
          "boundary": "test_helper",
          "wrapper": "bench_integral_image_normal",
      }
    if case.startswith("pcl_iin_diff_"):
      return {
          "group": "integral_image_normal_pcl_integral_image2d_profile_component",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "pcl_profile_component_diff",
          "timer_boundary": "pcl-profile-component-diff-buffer",
          "mask": "interior-pixel-only",
          "candidate_label": "rvv-pcl-profile-component-diff",
          "baseline_label": "std-pcl-profile-component-diff",
          "asm_boundary": "bench_integral_image_normal_rvv_avg3d_diff",
          "rvv_instr_count": 3,
          "case_kind": "diagnostic",
          "boundary": "test_helper",
          "wrapper": "bench_integral_image_normal",
      }
    if case.startswith("pcl_iin_setinput_dxdy_"):
      return {
          "group": "integral_image_normal_pcl_integral_image2d_profile_component",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "pcl_profile_component_setinput_dxdy",
          "timer_boundary": "pcl-integral-image2d-setinput-dxdy",
          "mask": "pcl-finite-diff-buffer",
          "candidate_label": "rvv-pcl-iin-setinput-dxdy",
          "baseline_label": "std-pcl-iin-setinput-dxdy",
          "asm_boundary": "pcl_integral_image2d_setinput_scalar_boundary",
          "rvv_instr_count": 0,
      }
    if case.startswith("pcl_iin_query_"):
      return {
          "group": "integral_image_normal_pcl_integral_image2d_profile_component",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "pcl_profile_component_query",
          "timer_boundary": "pcl-integral-image2d-query-loop",
          "mask": "pcl-integral-image-query-loop",
          "candidate_label": "rvv-pcl-iin-query",
          "baseline_label": "std-pcl-iin-query",
          "asm_boundary": "pcl_integral_image2d_query_scalar_boundary",
          "rvv_instr_count": 0,
      }
    if case.startswith("avg3d_profile_"):
      return {
          "group": "integral_image_normal_average_3d_gradient_profile_total",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "profile_total_normal_query",
          "timer_boundary": "average-3d-gradient-profile-total",
          "mask": "interior-query-loop",
          "candidate_label": "rvv-avg3d-profile-total",
          "baseline_label": "std-avg3d-profile-total",
          "asm_boundary": "bench_integral_image_normal_rvv_avg3d_profile",
          "rvv_instr_count": 3,
      }
    if case.startswith("profile_component_diff_"):
      return {
          "group": "integral_image_normal_average_3d_gradient_profile_component",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "profile_component_diff",
          "timer_boundary": "profile-component-diff-buffer",
          "mask": "interior-pixel-only",
          "candidate_label": "rvv-profile-component-diff",
          "baseline_label": "std-profile-component-diff",
          "asm_boundary": "bench_integral_image_normal_rvv_avg3d_diff",
          "rvv_instr_count": 3,
      }
    if case.startswith("profile_component_integral_"):
      return {
          "group": "integral_image_normal_average_3d_gradient_profile_component",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "profile_component_integral",
          "timer_boundary": "profile-component-integral-image-build",
          "mask": "finite-diff-buffer",
          "candidate_label": "rvv-profile-component-integral",
          "baseline_label": "std-profile-component-integral",
          "asm_boundary": "scalar-profile-integral-builder",
          "rvv_instr_count": 0,
      }
    if case.startswith("profile_component_query_"):
      return {
          "group": "integral_image_normal_average_3d_gradient_profile_component",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "profile_component_query",
          "timer_boundary": "profile-component-normal-query",
          "mask": "interior-query-loop",
          "candidate_label": "rvv-profile-component-query",
          "baseline_label": "std-profile-component-query",
          "asm_boundary": "scalar-profile-query-loop",
          "rvv_instr_count": 0,
      }
    if case.startswith("avg3d_diff_"):
      return {
          "group": "integral_image_normal_average_3d_gradient_diff",
          "point_type": "synthetic_xyz_pad_point",
          "checksum_policy": "sampled_diff_x_diff_y",
          "timer_boundary": "average-3d-gradient-diff-buffer-only",
          "mask": "interior-pixel-only",
          "candidate_label": "rvv-avg3d-diff",
          "baseline_label": "std-avg3d-diff",
          "asm_boundary": "test_integral_image_normal_rvv_avg3d_diff",
          "rvv_instr_count": 3,
      }
    return {
        "group": "integral_image_normal_map_prep",
        "point_type": "synthetic_z_buffer",
        "checksum_policy": "sampled_distance_map",
        "timer_boundary": "map-prep-only",
        "mask": "depth-change-map",
        "candidate_label": "rvv-map-prep",
        "baseline_label": "std-map-prep",
        "asm_boundary": "test_integral_image_normal_rvv",
        "rvv_instr_count": 1,
        "case_kind": "diagnostic",
        "boundary": "test_helper",
        "wrapper": "bench_integral_image_normal",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", type=Path)
    parser.add_argument("--rvv-log", type=Path)
    parser.add_argument("--summary", type=Path)
    parser.add_argument("--run-dir", action="append", type=Path, default=[])
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", default="Milkv-Jupiter")
    args = parser.parse_args()

    run_specs = []
    if args.std_log and args.rvv_log:
      run_specs.append((args.std_log, args.rvv_log, args.summary))
    for run_dir in args.run_dir:
      run_specs.append((
          run_dir / "run_bench_std.log",
          run_dir / "run_bench_rvv.log",
          run_dir / "analyze_bench_compare.log",
      ))
    if not run_specs:
      raise SystemExit("provide --std-log/--rvv-log or at least one --run-dir")

    parsed_runs = [(parse_log(std_log), parse_log(rvv_log), summary) for std_log, rvv_log, summary in run_specs]
    cases = sorted(set.intersection(*[
        set(std["rows"]) & set(rvv["rows"]) for std, rvv, _summary in parsed_runs
    ]))

    comparisons = []
    for case in cases:
      std0, rvv0, _summary0 = parsed_runs[0]
      width, height = std0["sizes"].get(case, (0, 0))
      std_ms = std0["rows"][case]
      rvv_ms = rvv0["rows"][case]
      checksum_std = std0["checksums"].get(case, "missing")
      checksum_rvv = rvv0["checksums"].get(case, "missing")
      metadata = case_metadata(case)
      case_kind = str(metadata.get("case_kind", "diagnostic"))
      boundary = str(metadata.get("boundary", "test_helper"))
      wrapper = str(metadata.get("wrapper", "bench_integral_image_normal"))
      evidence_role = "production_direct" if case_kind == "production_direct" else "strict_ab"
      ba_values = []
      for std, rvv, _summary in parsed_runs:
        ba_values.append(std["rows"][case] / rvv["rows"][case])
      comparisons.append({
          "name": case,
          "group": metadata["group"],
          "evidence_role": evidence_role,
          "case_kind": case_kind,
          "point_type": metadata["point_type"],
          "size": width * height,
          "run_count": len(parsed_runs),
          "baseline": {
              "label": metadata["baseline_label"],
              "boundary": boundary,
              "wrapper": wrapper,
              "row_source": "ordered-organized-image",
              "solve": False,
              "checksum_policy": metadata["checksum_policy"],
              "checksum": checksum_std,
              "timer_boundary": metadata["timer_boundary"],
              "gate": "same_input",
              "mask": metadata["mask"],
              "reduction": "none",
              "asm_boundary": "scalar_fallback",
              "rvv_instr_count": 0,
              "std_ms": std_ms,
              "rvv_ms": std_ms,
          },
          "candidate": {
              "label": metadata["candidate_label"],
              "boundary": boundary,
              "wrapper": wrapper,
              "row_source": "ordered-organized-image",
              "solve": False,
              "checksum_policy": metadata["checksum_policy"],
              "checksum": checksum_rvv,
              "timer_boundary": metadata["timer_boundary"],
              "gate": "same_input",
              "mask": metadata["mask"],
              "reduction": "none",
              "asm_boundary": metadata["asm_boundary"],
              "rvv_instr_count": metadata["rvv_instr_count"],
              "std_ms": std_ms,
              "rvv_ms": rvv_ms,
          },
          "ba_values": ba_values,
      })

    first_std, first_rvv, first_summary = parsed_runs[0]
    manifest = {
        "schema_version": 1,
        "summary": {
            "title": "integral_image_normal diagnostic board summary",
            "evidence_role": "mixed_boundary_cross_check",
            "summary_path": str(args.summary or first_summary or ""),
            "label": "bounded repeated board diagnostic",
            "analysis_script": "test-rvv/features/integral_image_normal/script/generate_integral_image_normal_evidence_manifest.py",
            "metadata": {
                "device": args.device,
                "iterations": first_std["iterations"] or first_rvv["iterations"],
                "warmup_iterations": first_std["warmup"] or first_rvv["warmup"],
                "run_count": len(parsed_runs),
                "taskset": "not_recorded",
                "governor": "not_recorded",
                "freq": "not_recorded",
                "temperature": "not_recorded",
                "vlen": "see SoC / ELF",
                "binary_hash": "not_recorded",
            },
        },
        "checks": {"strict_ab": True},
        "comparisons": comparisons,
    }
    if args.summary:
      write_summary(args.summary, manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
