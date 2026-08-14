#!/usr/bin/env python3
"""Generate an Evidence Doctor manifest for the ICP QEMU bench smoke.

本脚本只解析当前 topic 的 `analyze_bench_compare.log` 和配套 run logs，
把 case label、Std/RVV 平均耗时和 checksum 指纹翻译成 Evidence Doctor
（证据体检）可读取的 JSON manifest。它服务 `registration/icp`，不是跨 topic 通用脚本。
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


LOG_CASE_RE = re.compile(r"^(icp transform-cloud .+?)\s*:\s*([0-9.]+)\s*ms/iter\s*$")
CHECKSUM_RE = re.compile(r"checksum:\s*([0-9]+)")


def parse_compare(path: Path) -> dict[str, dict[str, float]]:
  rows: dict[str, dict[str, float]] = {}
  current_name = ""
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    parts = [part.strip() for part in line.split("|")]
    if len(parts) != 5:
      continue
    name, impl, avg_text, _total_text, speedup_text = parts
    if name.startswith("icp transform-cloud"):
      current_name = name
    elif current_name:
      name = current_name
    if impl not in {"Std", "RVV"}:
      continue
    avg_match = re.search(r"([0-9.]+)\s+ms", avg_text)
    speedup_match = re.search(r"\[\s*([0-9.]+)x\s*\]", speedup_text)
    if not avg_match or not speedup_match:
      continue
    entry = rows.setdefault(name, {})
    key = "std_ms" if impl == "Std" else "rvv_ms"
    entry[key] = float(avg_match.group(1))
    if impl == "RVV":
      entry["speedup"] = float(speedup_match.group(1))
  return rows


def parse_checksums(path: Path) -> dict[str, str]:
  result: dict[str, str] = {}
  current = ""
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    case_match = LOG_CASE_RE.match(line.strip())
    if case_match:
      current = case_match.group(1)
      continue
    checksum_match = CHECKSUM_RE.search(line)
    if current and checksum_match:
      result[current] = checksum_match.group(1)
      current = ""
  return result


def point_type_for_case(name: str) -> str:
  return "PointNormal" if "xyz-normal" in name else "PointXYZ"


def size_for_case(name: str) -> int:
  if "256K" in name:
    return 256 * 1024
  if "64K" in name:
    return 64 * 1024
  return 0


def make_side(checksum: str, build: str) -> dict[str, Any]:
  return {
      "boundary": "test_support_transform_cloud_candidate",
      "wrapper": "bench_icp",
      "row_source": "full_cloud",
      "solve": False,
      "checksum_policy": "coarse_transform_output_fingerprint_correctness_covered_by_gtest",
      "log_checksum": checksum,
      "timer_boundary": "transform_cloud_only",
      "gate": "build_selects_scalar_or_rvv",
      "mask": "production_finite_xyz_then_optional_normal_mask",
      "reduction": "none",
      "build": build,
  }


def build_manifest(args: argparse.Namespace) -> dict[str, Any]:
  rows = parse_compare(Path(args.summary))
  std_checksums = parse_checksums(Path(args.std_log))
  rvv_checksums = parse_checksums(Path(args.rvv_log))
  comparisons: list[dict[str, Any]] = []
  for name, values in sorted(rows.items()):
    std_ms = values.get("std_ms")
    rvv_ms = values.get("rvv_ms")
    if std_ms is None or rvv_ms is None or rvv_ms == 0:
      continue
    comparisons.append(
        {
            "name": name,
            "case_kind": "qemu_bench_smoke",
            "evidence_role": "diagnostic",
            "point_type": point_type_for_case(name),
            "size": size_for_case(name),
            "group": "icp_transform_cloud_qemu_smoke",
            "build_comparison": True,
            "run_count": 1,
            "iterations": args.iterations,
            "warmup_iterations": args.warmup_iterations,
            "ba_values": [std_ms / rvv_ms],
            "std_ms": std_ms,
            "rvv_ms": rvv_ms,
            "baseline": make_side(std_checksums.get(name, ""), "std_no_rvv_macro"),
            "candidate": make_side(rvv_checksums.get(name, ""), "rvv_macro"),
            "allowed_contract_mismatches": [],
            "notes": "QEMU bench smoke only; performance conclusion requires target hardware.",
        }
    )
  return {
      "schema_version": 1,
      "summary": {
          "title": "ICP transformCloud QEMU bench smoke",
          "evidence_role": "diagnostic",
          "summary_path": args.summary,
          "metadata": {
              "device": "qemu-riscv64 rv64gcv",
              "vlen": "256",
              "iterations": args.iterations,
              "warmup_iterations": args.warmup_iterations,
              "run_count": 1,
              "taskset": "not_applicable_qemu",
              "governor": "not_applicable_qemu",
              "freq": "not_applicable_qemu",
              "temperature": "not_applicable_qemu",
              "binary_hash": "not_recorded_qemu_smoke",
          },
      },
      "checks": {"strict_ab": False},
      "comparisons": comparisons,
  }


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--summary", required=True)
  parser.add_argument("--std-log", required=True)
  parser.add_argument("--rvv-log", required=True)
  parser.add_argument("--output", required=True)
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
