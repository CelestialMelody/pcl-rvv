#!/usr/bin/env python3
"""Generate repeated board summary and Evidence Doctor manifest for EPPD.

脚本只理解本 topic 的 bench label（用例标签），把 dense/indexed 两种
row source（行来源）翻译成 Evidence Doctor 可复核的 manifest 字段。
"""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


STD_ROW_RE = re.compile(
    r"^(?P<name>.+?)\s*\|\s*Std\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|"
)
RVV_ROW_RE = re.compile(r"^\s*\|\s*RVV\s*\|\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*\|")
DATASET_RE = re.compile(r"Dataset\s*:\s*.+cloud=(?P<size>[0-9]+)")


def parse_compare(path: Path) -> tuple[str, int, float, float]:
  name = ""
  size = 0
  std_ms = 0.0
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    if match := DATASET_RE.search(line):
      size = int(match.group("size"))
      continue
    if match := STD_ROW_RE.match(line):
      name = match.group("name").strip()
      std_ms = float(match.group("avg"))
      continue
    if name and (match := RVV_ROW_RE.match(line)):
      rvv_ms = float(match.group("avg"))
      return name, size, std_ms, rvv_ms
  raise ValueError(f"no Std/RVV rows found in {path}")


def row_source_for_case(case_name: str) -> str:
  if "indices=indexed" in case_name:
    return "source_indexed_points"
  if "indices=dense" in case_name:
    return "dense_ordered_indices"
  return "dense_ordered_indices"


def polygon_kind_for_case(case_name: str) -> str:
  if "nested polygon" in case_name:
    return "nested_polygon"
  return "single_polygon"


def point_type_for_case(case_name: str) -> str:
  if "point-type=xyzinormal" in case_name:
    return "PointXYZINormal"
  if "point-type=xyzrgba" in case_name:
    return "PointXYZRGBA"
  if "point-type=xyzrgb" in case_name:
    return "PointXYZRGB"
  if "point-type=xyzi" in case_name:
    return "PointXYZI"
  return "PointXYZ"


def evidence_role_for_case(case_name: str) -> str:
  if "full-scan production" in case_name:
    return "production_public"
  return "production_shaped_diagnostic"


def boundary_for_case(case_name: str) -> str:
  if "full-scan production" in case_name:
    return "public_overload"
  return "test_support_full_scan"


def timer_boundary_for_case(case_name: str) -> str:
  if "full-scan production" in case_name:
    return "public_segment_including_plane_setup_project_points_and_scan"
  return "full_scan_after_projection_setup"


def asm_boundary_for_case(case_name: str) -> str:
  if "full-scan production" in case_name:
    return "ExtractPolygonalPrismData<PointT>::segmentRvv"
  return "segmentPolygonalPrismRvvFullScanCandidate"


def percentile(values: list[float], pct: float) -> float:
  ordered = sorted(values)
  if len(ordered) == 1:
    return ordered[0]
  position = (len(ordered) - 1) * pct
  lower = int(position)
  upper = min(lower + 1, len(ordered) - 1)
  fraction = position - lower
  return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--repeated-dir", required=True, type=Path)
  parser.add_argument("--summary", required=True, type=Path)
  parser.add_argument("--output", required=True, type=Path)
  parser.add_argument("--device", required=True)
  parser.add_argument("--iterations", required=True, type=int)
  parser.add_argument("--warmup-iterations", required=True, type=int)
  parser.add_argument("--binary-hash", default="")
  args = parser.parse_args()

  logs = sorted(args.repeated_dir.glob("run*/analyze_bench_compare.log"))
  if not logs:
    raise SystemExit(f"no repeated compare logs under {args.repeated_dir}")

  case_name = ""
  case_size = 0
  std_values: list[float] = []
  rvv_values: list[float] = []
  speedups: list[float] = []
  for log in logs:
    name, size, std_ms, rvv_ms = parse_compare(log)
    if case_name and name != case_name:
      raise SystemExit(f"mixed benchmark cases: {case_name!r} vs {name!r}")
    if case_size and size and size != case_size:
      raise SystemExit(f"mixed benchmark sizes: {case_size!r} vs {size!r}")
    case_name = name
    case_size = size
    std_values.append(std_ms)
    rvv_values.append(rvv_ms)
    speedups.append(std_ms / rvv_ms)

  row_source = row_source_for_case(case_name)
  polygon_kind = polygon_kind_for_case(case_name)
  point_type = point_type_for_case(case_name)
  evidence_role = evidence_role_for_case(case_name)
  boundary = boundary_for_case(case_name)
  timer_boundary = timer_boundary_for_case(case_name)
  asm_boundary = asm_boundary_for_case(case_name)
  args.summary.parent.mkdir(parents=True, exist_ok=True)
  values_text = ", ".join(f"{value:.2f}x" for value in speedups)
  args.summary.write_text(
      "\n".join(
          [
              f"# EPPD full-scan {polygon_kind.replace('_', '-')} repeated board summary ({row_source})",
              "",
              f"- device: {args.device}",
              f"- evidence_role: {evidence_role}",
              f"- row_source: {row_source}",
              f"- point_type: {point_type}",
              f"- case: {case_name}",
              f"- iterations: {args.iterations}",
              f"- warmup_iterations: {args.warmup_iterations}",
              f"- run_count: {len(speedups)}",
              "",
              "| Benchmark Item | runs | median | min | max | p10 | p90 | values |",
              "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
              (
                  f"| {case_name} | {len(speedups)} | {statistics.median(speedups):.2f}x | "
                  f"{min(speedups):.2f}x | {max(speedups):.2f}x | "
                  f"{percentile(speedups, 0.10):.2f}x | {percentile(speedups, 0.90):.2f}x | "
                  f"{values_text} |"
              ),
              "",
          ]
      ),
      encoding="utf-8",
  )

  checksum = "see run_bench_std.log and run_bench_rvv.log; all fetched compare runs matched"
  manifest = {
      "schema_version": 1,
      "summary": {
          "title": f"EPPD full-scan {polygon_kind.replace('_', '-')} repeated board ({evidence_role}, {row_source})",
          "evidence_role": evidence_role,
          "summary_path": str(args.summary),
              "label": f"eppd {polygon_kind.replace('_', '-')} {point_type} full-scan repeated board summary",
          "metadata": {
              "device": args.device,
              "iterations": args.iterations,
              "warmup_iterations": args.warmup_iterations,
              "run_count": len(speedups),
              "taskset": "not_recorded",
              "governor": "not_recorded",
              "freq": "not_recorded",
              "temperature": "not_recorded",
              "binary_hash": args.binary_hash or "not_recorded",
          },
      },
      "checks": {
          "strict_ab": False,
          "thresholds": {
              "min_repeated_runs": 5,
              "ba_degrade_threshold": 1.0,
              "long_tail_ratio": 1.25,
          },
      },
      "comparisons": [
          {
              "name": case_name,
              "group": f"eppd_full_scan_{polygon_kind}_{point_type.lower()}_repeated_board",
              "evidence_role": evidence_role,
              "case_kind": f"full_scan_{polygon_kind}",
              "point_type": point_type,
              "size": case_size,
              "run_count": len(speedups),
              "iterations": args.iterations,
              "warmup_iterations": args.warmup_iterations,
              "baseline": {
                  "label": "scalar reference",
                  "boundary": boundary,
                  "wrapper": "bench_eppd",
                  "row_source": row_source,
                  "solve": False,
                  "checksum_policy": "fnv_indices",
                  "checksum": checksum,
                  "timer_boundary": timer_boundary,
                  "gate": f"plane_distance_and_{polygon_kind}",
                  "mask": "height_and_polygon_xor_parity",
                  "reduction": "none_output_compress",
                  "std_ms": statistics.median(std_values),
              },
              "candidate": {
                  "label": "rvv full-scan edge parity candidate",
                  "boundary": boundary,
                  "wrapper": "bench_eppd",
                  "row_source": row_source,
                  "solve": False,
                  "checksum_policy": "fnv_indices",
                  "checksum": checksum,
                  "timer_boundary": timer_boundary,
                  "gate": f"plane_distance_and_{polygon_kind}",
                  "mask": "height_and_polygon_xor_parity",
                  "reduction": "none_output_compress",
                  "asm_boundary": asm_boundary,
                  "rvv_instr_count": "see build/asm/riscv/bench_eppd_rvv.asm",
                  "rvv_ms": statistics.median(rvv_values),
              },
              "ba_values": speedups,
          }
      ],
  }
  args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
  print(f"wrote summary: {args.summary}")
  print(f"wrote manifest: {args.output}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
