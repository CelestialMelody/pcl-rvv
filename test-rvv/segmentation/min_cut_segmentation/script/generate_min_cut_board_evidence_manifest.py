#!/usr/bin/env python3
"""Generate repeated board summary and Evidence Doctor manifest for min-cut diagnostics."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path
from typing import Any


STD_ROW_RE = re.compile(r"^(?P<name>.+?)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|")
RVV_ROW_RE = re.compile(r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|.*\[\s*(?P<speedup>[0-9]+(?:\.[0-9]+)?)x\s*\]")


def parse_compare(path: Path) -> dict[str, dict[str, float]]:
  rows: dict[str, dict[str, float]] = {}
  current = ""
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    if match := STD_ROW_RE.match(line):
      current = match.group("name").strip()
      rows[current] = {"std_ms": float(match.group("avg"))}
      continue
    if current and (match := RVV_ROW_RE.match(line)):
      rvv_ms = float(match.group("avg"))
      rows[current]["rvv_ms"] = rvv_ms
      rows[current]["speedup"] = float(match.group("speedup"))
      current = ""
  return rows


def decision_for(values: list[float]) -> str:
  median = statistics.median(values)
  if median >= 1.10 and min(values) >= 1.0:
    return "positive"
  if median >= 1.03:
    return "weak-positive"
  if median >= 0.97:
    return "neutral"
  return "negative"


def metadata_for_item(item: str) -> dict[str, str]:
  if "unary_min_distance" in item:
    return {
        "case_kind": "component_diagnostic",
        "row_source": "input-indexed point rows",
        "timer_boundary": "unary potential helper only; excludes KNN, Boost graph mutation and max-flow",
        "mask": "none_dense_pointxyz",
        "reduction": "foreground_min_distance",
        "asm_boundary": "computeUnaryPotentialsRVV",
    }
  if "buildgraph_potential_batch" in item:
    return {
        "case_kind": "production_shaped_diagnostic",
        "row_source": "full input rows plus KNN edge rows",
        "timer_boundary": "buildGraph-shaped helper; includes KNN, Boost graph mutation and edge marker; excludes max-flow and public extract",
        "mask": "none_dense_pointxyz",
        "reduction": "foreground_min_distance",
        "asm_boundary": "computeBuildGraphPotentialBatchRVV, computeUnaryPotentialsRVV and computeBinaryPotentialsRVV",
    }
  return {
      "case_kind": "component_diagnostic",
      "row_source": "edge list source/target index pairs",
      "timer_boundary": "binary potential helper only; excludes KNN, Boost graph mutation and max-flow",
      "mask": "none_dense_pointxyz",
      "reduction": "none",
      "asm_boundary": "computeBinaryPotentialsRVV plus expf_RVV_f32m2",
  }


def make_side(build: str, item: str, values: list[float]) -> dict[str, Any]:
  meta = metadata_for_item(item)
  is_buildgraph = "buildgraph_potential_batch" in item
  side: dict[str, Any] = {
      "label": build,
      "boundary": "test_helper_buildgraph" if is_buildgraph else "test_helper_component",
      "wrapper": "bench_min_cut_segmentation",
      "row_source": meta["row_source"],
      "solve": False,
      "checksum_policy": "bench printed weighted floating checksum; correctness covered by run_test_compare",
      "timer_boundary": meta["timer_boundary"],
      "gate": "rvv_macro_or_std_build",
      "mask": meta["mask"],
      "reduction": meta["reduction"],
      "build": build,
      "ms_median": statistics.median(values),
      "ms_min": min(values),
      "ms_max": max(values),
  }
  if build == "rvv_macro":
    side["asm_boundary"] = meta["asm_boundary"]
    side["rvv_instr_count"] = "see build/asm/riscv/bench_min_cut_segmentation_rvv.asm"
  return side


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
  run_logs = sorted(Path(args.repeated_dir).glob("run*/analyze_bench_compare.log"))
  if not run_logs:
    raise SystemExit(f"no repeated compare logs under {args.repeated_dir}")

  by_item: dict[str, dict[str, list[float]]] = {}
  for log in run_logs:
    for item, values in parse_compare(log).items():
      by_item.setdefault(item, {"std_ms": [], "rvv_ms": [], "speedup": []})
      by_item[item]["std_ms"].append(values["std_ms"])
      by_item[item]["rvv_ms"].append(values["rvv_ms"])
      by_item[item]["speedup"].append(values["speedup"])

  comparisons = []
  for item, values in sorted(by_item.items()):
    meta = metadata_for_item(item)
    is_buildgraph = "buildgraph_potential_batch" in item
    comparisons.append(
        {
            "name": item,
            "group": "min_cut_buildgraph_repeated_board"
            if is_buildgraph
            else "min_cut_component_repeated_board",
            "case_kind": meta["case_kind"],
            "evidence_role": "production_shaped_diagnostic"
            if is_buildgraph
            else "diagnostic",
            "point_type": "PointXYZ",
            "size": args.size,
            "foreground_points": args.foreground,
            "neighbours": args.neighbours,
            "run_count": len(values["speedup"]),
            "iterations": args.iterations,
            "warmup_iterations": args.warmup_iterations,
            "ba_values": values["speedup"],
            "baseline": make_side("std_no_rvv_macro", item, values["std_ms"]),
            "candidate": make_side("rvv_macro", item, values["rvv_ms"]),
            "allowed_contract_mismatches": [
                f"{item} RVV uses float expf_RVV_f32m2 while production scalar uses double std::exp; evidence role stays diagnostic"
            ]
            if ("binary_exp_weight" in item or "buildgraph_potential_batch" in item)
            else [],
            "decision_bucket": decision_for(values["speedup"]),
            "notes": (
                "Production-shaped diagnostic evidence. The timer includes nearestKSearch, Boost graph mutation "
                "and edge_marker duplicate checks, but excludes public extract, object lifecycle and "
                "boykov_kolmogorov_max_flow."
                if is_buildgraph
                else (
                    "Diagnostic component evidence only. The timer excludes nearestKSearch, Boost graph mutation, "
                    "edge_marker duplicate checks and boykov_kolmogorov_max_flow, so it can justify only a bounded "
                    "production-shaped probe."
                )
            ),
        }
    )

  has_buildgraph = any("buildgraph_potential_batch" in item for item in by_item)
  return {
      "schema_version": 1,
      "summary": {
          "title": (
              "min_cut_segmentation buildGraph-shaped repeated board diagnostic"
              if has_buildgraph
              else "min_cut_segmentation component repeated board diagnostic"
          ),
          "evidence_role": "production_shaped_diagnostic" if has_buildgraph else "diagnostic",
          "summary_path": args.summary,
          "metadata": {
              "device": args.device,
              "iterations": args.iterations,
              "warmup_iterations": args.warmup_iterations,
              "run_count": len(run_logs),
              "neighbours": args.neighbours,
              "taskset": args.taskset,
              "governor": args.governor,
              "freq": args.freq,
              "temperature": args.temperature,
              "vlen": args.vlen,
              "binary_hash": args.binary_hash or "not_recorded",
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
  parser.add_argument("--size", type=int, default=65536)
  parser.add_argument("--foreground", type=int, default=19)
  parser.add_argument("--neighbours", type=int, default=14)
  parser.add_argument("--iterations", type=int, default=8)
  parser.add_argument("--warmup-iterations", type=int, default=2)
  parser.add_argument("--taskset", default="not_pinned")
  parser.add_argument("--governor", default="not_recorded")
  parser.add_argument("--freq", default="not_recorded")
  parser.add_argument("--temperature", default="not_recorded")
  parser.add_argument("--vlen", default="not_recorded")
  parser.add_argument("--binary-hash", default="")
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
