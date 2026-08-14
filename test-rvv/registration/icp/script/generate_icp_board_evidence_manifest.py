#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for ICP production repeated board evidence."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


CONTEXT_RE = re.compile(r"^\s*-\s*([^:：]+)\s*[:：]\s*`?(.+?)`?\s*$")


def parse_context(text: str) -> dict[str, str]:
  context: dict[str, str] = {}
  key_map = {
      "device": "device",
      "runs": "run_count",
      "iterations": "iterations",
      "warm-up iterations": "warmup_iterations",
      "warmup iterations": "warmup_iterations",
      "taskset": "taskset",
      "governor": "governor",
      "freq": "freq",
      "temperature": "temperature",
      "binary hash": "binary_hash",
  }
  for line in text.splitlines():
    match = CONTEXT_RE.match(line)
    if not match:
      continue
    key = key_map.get(match.group(1).strip().lower())
    if key:
      context[key] = match.group(2).strip().strip("`")
  return context


def parse_int(context: dict[str, str], key: str, default: int) -> int:
  try:
    return int(context.get(key, ""))
  except ValueError:
    return default


def parse_repeated_table(text: str) -> list[dict[str, Any]]:
  rows: list[dict[str, Any]] = []
  for line in text.splitlines():
    if not line.startswith("|"):
      continue
    parts = [part.strip() for part in line.strip().strip("|").split("|")]
    if len(parts) != 8 or parts[0] in {"Benchmark Item", "---"}:
      continue
    if not parts[0].startswith("icp transform-cloud"):
      continue
    values = [float(value) for value in re.findall(r"([0-9.]+)x", parts[7])]
    rows.append(
        {
            "name": parts[0],
            "runs": int(parts[1]),
            "median": parts[2],
            "min": parts[3],
            "max": parts[4],
            "p10": parts[5],
            "p90": parts[6],
            "values": values,
        }
    )
  return rows


def point_type_for_case(name: str) -> str:
  return "PointNormal" if "xyz-normal" in name else "PointXYZ"


def size_for_case(name: str) -> int:
  if "256K" in name:
    return 256 * 1024
  if "64K" in name:
    return 64 * 1024
  return 0


def make_side(build: str) -> dict[str, Any]:
  side = {
      "boundary": "production_transform_cloud",
      "wrapper": "bench_icp",
      "row_source": "full_cloud",
      "solve": False,
      "checksum_policy": "coarse_transform_output_fingerprint_correctness_covered_by_gtest",
      "timer_boundary": "transform_cloud_only",
      "gate": "production_scalar_or_rvv_dispatch",
      "mask": "production_finite_xyz_then_optional_normal_mask",
      "reduction": "none",
      "build": build,
  }
  if build == "rvv_macro":
    side["asm_boundary"] = "production IterativeClosestPoint::transformCloud symbol"
    side["asm_notes"] = (
        "bench_icp_rvv.full.asm contains PointXYZ and PointNormal transformCloud "
        "symbols with vsetvli e32,m2, vlsseg3e32.v, vfmacc.vf, vmand.mm, and "
        "masked vsse32.v; fallback branches jal into transformCloudStandard "
        "symbols, and bench lambdas jal into the production transformCloud symbols."
    )
  else:
    side["asm_boundary"] = "std production IterativeClosestPoint::transformCloud symbol"
  return side


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
  summary_path = Path(args.summary)
  text = summary_path.read_text(encoding="utf-8", errors="replace")
  context = parse_context(text)
  comparisons: list[dict[str, Any]] = []
  for row in parse_repeated_table(text):
    comparisons.append(
        {
            "name": row["name"],
            "case_kind": "board_repeated_bench",
            "evidence_role": "production_direct",
            "point_type": point_type_for_case(row["name"]),
            "size": size_for_case(row["name"]),
            "group": "icp_transform_cloud_board_repeated",
            "build_comparison": True,
            "run_count": row["runs"],
            "iterations": parse_int(context, "iterations", args.iterations),
            "warmup_iterations": parse_int(
                context, "warmup_iterations", args.warmup_iterations
            ),
            "ba_values": row["values"],
            "baseline": make_side("std_no_rvv_macro"),
            "candidate": make_side("rvv_macro"),
            "allowed_contract_mismatches": [],
            "notes": (
                "Repeated board evidence for production IterativeClosestPoint::transformCloud "
                "dispatch; correctness is covered by production-direct gtest."
            ),
        }
    )
  return {
      "schema_version": 1,
      "summary": {
          "title": "ICP transformCloud repeated board production direct",
          "evidence_role": "production_direct",
          "summary_path": str(summary_path),
          "metadata": {
              "device": context.get("device", args.device),
              "iterations": parse_int(context, "iterations", args.iterations),
              "warmup_iterations": parse_int(
                  context, "warmup_iterations", args.warmup_iterations
              ),
              "run_count": parse_int(context, "run_count", args.runs),
              "taskset": context.get("taskset", "not_pinned"),
              "governor": context.get("governor", "not_recorded"),
              "freq": context.get("freq", "not_recorded"),
              "temperature": context.get("temperature", "not_recorded"),
              "binary_hash": context.get("binary_hash", ""),
          },
      },
      "checks": {"strict_ab": False},
      "comparisons": comparisons,
  }


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--summary", required=True)
  parser.add_argument("--output", required=True)
  parser.add_argument("--device", default="board")
  parser.add_argument("--runs", type=int, default=5)
  parser.add_argument("--iterations", type=int, default=20)
  parser.add_argument("--warmup-iterations", type=int, default=3)
  args = parser.parse_args()

  manifest = build_manifest(args)
  output = Path(args.output)
  output.parent.mkdir(parents=True, exist_ok=True)
  output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
  print(f"wrote manifest: {output}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
