#!/usr/bin/env python3
"""Generate repeated board summary and Evidence Doctor manifest for APMF components."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import Any


ITEM_RE = re.compile(r"^(apmf .+?)\s+\|\s+Std\s+\|\s+([0-9.]+) ms")
RVV_RE = re.compile(r"^\s*\|\s+RVV\s+\|\s+([0-9.]+) ms.*\[\s*([0-9.]+)x\s*\]")


def parse_summary(path: Path) -> dict[str, dict[str, float]]:
  rows: dict[str, dict[str, float]] = {}
  current = ""
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    item = ITEM_RE.match(line)
    if item:
      current = item.group(1).strip()
      rows[current] = {"std_ms": float(item.group(2))}
      continue
    rvv = RVV_RE.match(line)
    if rvv and current:
      rows[current]["rvv_ms"] = float(rvv.group(1))
      rows[current]["speedup"] = float(rvv.group(2))
      current = ""
  return rows


def make_side(build: str, item: str, ms_values: list[float]) -> dict[str, Any]:
  row_source = "full_cloud" if "grid z-min" in item else "synthetic_grid_or_ground"
  reduction = "window_min_max" if "window open" in item else "none"
  mask = "finite_xyz_mask_only_for_non_dense_case" if "non-dense" in item else "none_or_component_local"
  boundary = "test_helper_component"
  wrapper = "bench_apmf"
  if "full pipeline" in item:
    row_source = "full_cloud_plus_current_ground"
    reduction = "window_min_max_and_threshold_tail"
    mask = "finite_xyz_mask_for_initial_ground_and_grid_z_min"
    boundary = "test_helper_production_shaped"
  if "production public" in item:
    row_source = "full_cloud_public_extract"
    reduction = "window_min_max_scalar_with_rvv_grid_and_tail_when_available"
    mask = "finite_xyz_mask_for_grid_z_min_and_initial_ground"
    boundary = "public_overload"
    wrapper = "bench_apmf_production"
  return {
      "label": build,
      "boundary": boundary,
      "wrapper": wrapper,
      "row_source": row_source,
      "solve": False,
      "checksum_policy": "bench_printed_checksum_verified_by_matching_std_rvv_logs",
      "timer_boundary": item,
      "gate": "rvv_macro_or_std_build",
      "mask": mask,
      "reduction": reduction,
      "build": build,
      "ms_median": statistics.median(ms_values),
      "ms_min": min(ms_values),
      "ms_max": max(ms_values),
  }


def decision_for(values: list[float]) -> str:
  median = statistics.median(values)
  if min(values) >= 1.2:
    return "positive"
  if median >= 1.05:
    return "weak-positive"
  if 0.95 <= median < 1.05:
    return "neutral"
  return "negative"


def point_type_for(item: str) -> str:
  if "PointXYZRGBA" in item:
    return "PointXYZRGBA"
  if "PointXYZRGB" in item:
    return "PointXYZRGB"
  if "PointXYZI" in item:
    return "PointXYZI"
  if (
      "grid" in item
      or "tail" in item
      or "full pipeline" in item
      or "production public" in item
  ):
    return "PointXYZ"
  return "not_applicable"


def notes_for(evidence_role: str) -> str:
  if evidence_role == "production-public":
    return (
        "APMF production public evidence. Std and RVV builds are compared through "
        "ApproximateProgressiveMorphologicalFilter::extract, so this evidence supports the "
        "PI5 production retention decision for the covered public overload and point type."
    )
  if evidence_role == "production-shaped diagnostic":
    return (
        "APMF production-shaped diagnostic evidence. This can justify a bounded production "
        "probe, but final retention requires production public evidence."
    )
  return (
      "APMF diagnostic evidence. This is not production direct evidence; it can only "
      "support a bounded production probe if production scope and fallback are planned "
      "separately."
  )


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
  run_logs = sorted(Path(args.repeated_dir).glob("run*/analyze_bench_compare.log"))
  include_re = re.compile(args.include_regex) if args.include_regex else None
  by_item: dict[str, dict[str, list[float]]] = {}
  for log in run_logs:
    for item, values in parse_summary(log).items():
      if include_re and not include_re.search(item):
        continue
      by_item.setdefault(item, {"std_ms": [], "rvv_ms": [], "speedup": []})
      by_item[item]["std_ms"].append(values["std_ms"])
      by_item[item]["rvv_ms"].append(values["rvv_ms"])
      by_item[item]["speedup"].append(values["speedup"])

  comparisons = []
  for item, values in sorted(by_item.items()):
    case_kind = "production_shaped_diagnostic" if "full pipeline" in item else "component_diagnostic"
    evidence_role = "production-shaped diagnostic" if "full pipeline" in item else "diagnostic"
    if "production public" in item:
      case_kind = "production_public"
      evidence_role = "production-public"
    comparisons.append(
        {
            "name": item,
            "group": "apmf_production_public_repeated_board"
            if "production public" in item
            else "apmf_full_pipeline_repeated_board"
            if "full pipeline" in item
            else "apmf_component_repeated_board",
            "case_kind": case_kind,
            "evidence_role": evidence_role,
            "point_type": point_type_for(item),
            "size": args.size,
            "run_count": len(values["speedup"]),
            "iterations": args.iterations,
            "warmup_iterations": args.warmup_iterations,
            "ba_values": values["speedup"],
            "baseline": make_side("std_no_rvv_macro", item, values["std_ms"]),
            "candidate": make_side("rvv_macro", item, values["rvv_ms"]),
            "allowed_contract_mismatches": [],
            "decision_bucket": decision_for(values["speedup"]),
            "notes": notes_for(evidence_role),
        }
    )
  summary_role = "diagnostic"
  summary_title = "APMF component repeated board diagnostic"
  if comparisons and all(comp["evidence_role"] == "production-public" for comp in comparisons):
    summary_role = "production-public"
    summary_title = "APMF production public repeated board"
  if comparisons and all(comp["evidence_role"] == "production-shaped diagnostic" for comp in comparisons):
    summary_role = "production-shaped diagnostic"
    summary_title = "APMF full-pipeline repeated board diagnostic"

  return {
      "schema_version": 1,
      "summary": {
          "title": summary_title,
          "evidence_role": summary_role,
          "summary_path": args.summary,
          "metadata": {
              "device": args.device,
              "iterations": args.iterations,
              "warmup_iterations": args.warmup_iterations,
              "run_count": len(run_logs),
              "taskset": args.taskset,
              "governor": args.governor,
              "freq": args.freq,
              "temperature": args.temperature,
              "vlen": args.vlen,
              "binary_hash": args.binary_hash,
          },
      },
      "checks": {"strict_ab": False},
      "comparisons": comparisons,
  }


def write_summary(manifest: dict[str, Any], output: Path) -> None:
  output.parent.mkdir(parents=True, exist_ok=True)
  lines = [
      f"# {manifest['summary']['title']}",
      "",
      "## Context",
      "",
  ]
  metadata = manifest["summary"]["metadata"]
  for key in ["device", "run_count", "iterations", "warmup_iterations", "governor", "freq", "temperature", "vlen"]:
    lines.append(f"- {key}: `{metadata.get(key, '')}`")
  lines.extend(
      [
          "",
          "## Results",
          "",
          "| Benchmark Item | runs | median speedup | min | max | values | decision bucket |",
          "| --- | ---: | ---: | ---: | ---: | --- | --- |",
      ]
  )
  for comparison in manifest["comparisons"]:
    values = comparison["ba_values"]
    values_text = ", ".join(f"{value:.2f}x" for value in values)
    lines.append(
        f"| {comparison['name']} | {comparison['run_count']} | "
        f"{statistics.median(values):.2f}x | {min(values):.2f}x | {max(values):.2f}x | "
        f"{values_text} | {comparison['decision_bucket']} |"
    )
  lines.append("")
  output.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--repeated-dir", required=True)
  parser.add_argument("--summary", required=True)
  parser.add_argument("--output", required=True)
  parser.add_argument("--device", default="Milkv-Jupiter")
  parser.add_argument("--size", type=int, default=262144)
  parser.add_argument("--iterations", type=int, default=8)
  parser.add_argument("--warmup-iterations", type=int, default=2)
  parser.add_argument("--taskset", default="not_pinned")
  parser.add_argument("--governor", default="not_recorded")
  parser.add_argument("--freq", default="not_recorded")
  parser.add_argument("--temperature", default="not_recorded")
  parser.add_argument("--vlen", default="not_recorded")
  parser.add_argument("--binary-hash", default="")
  parser.add_argument(
      "--include-regex",
      default="",
      help="Only include benchmark items whose label matches this regex.",
  )
  args = parser.parse_args()

  manifest = build_manifest(args)
  write_summary(manifest, Path(args.summary))
  output = Path(args.output)
  output.parent.mkdir(parents=True, exist_ok=True)
  output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
  print(f"wrote summary: {args.summary}")
  print(f"wrote manifest: {args.output}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
