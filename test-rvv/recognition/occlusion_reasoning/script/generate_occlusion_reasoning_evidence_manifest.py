#!/usr/bin/env python3
"""
把 occlusion_reasoning board repeated 输出整理成 Evidence Doctor 可读的 manifest。
脚本只解析当前 topic 的 case label、Std/RVV 日志和 analyze summary，不提交 raw log。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
from pathlib import Path
from typing import Any


ANALYZE_CASE_RE = re.compile(
  r"^(?P<name>.+?)\s+\|\s+Std\s+\|\s+(?P<std_value>[0-9.]+)\s+(?P<std_unit>[um]s)\s+\|.*?\n"
  r"\s+\|\s+RVV\s+\|\s+(?P<rvv_value>[0-9.]+)\s+(?P<rvv_unit>[um]s)\s+\|.*?\[\s*(?P<speedup>[0-9.]+)x\s*\]",
  re.M,
)
RAW_ROW_RE = re.compile(
  r"^\s*(?P<name>.+?)\s*:\s*(?P<value>[0-9.]+)\s*(?P<unit>[um]s)\s*/\s*iter",
  re.M,
)
CHECKSUM_RE = re.compile(r"checksum\s*[:=]\s*([0-9]+)")
ITER_RE = re.compile(r"^\s*Iterations\s*:\s*(\d+)", re.M)
WARMUP_RE = re.compile(r"^\s*Warmup(?: Iterations)?\s*:\s*(\d+)", re.M)
DATASET_RE = re.compile(r"^\s*Dataset\s*:\s*(.+?)\s*$", re.M)
DEVICE_RE = re.compile(r"^\s*Device\s*:\s*(.+?)\s*$", re.M)
VLEN_RE = re.compile(r"^\s*VLEN\s*:\s*(.+?)\s*$", re.M)


def sha256_file(path: Path) -> str:
  h = hashlib.sha256()
  with path.open("rb") as f:
    for chunk in iter(lambda: f.read(1024 * 1024), b""):
      h.update(chunk)
  return "sha256:" + h.hexdigest()


def to_ms(value: float, unit: str) -> float:
  return value * 0.001 if unit == "us" else value


def parse_raw_bench(path: Path, accepted_names: set[str]) -> tuple[float | None, str | None, dict[str, Any]]:
  if not path.exists():
    return None, None, {}
  text = path.read_text(errors="replace")
  m_time = None
  for candidate in RAW_ROW_RE.finditer(text):
    if candidate.group("name").strip() in accepted_names:
      m_time = candidate
      break
  m_checksum = CHECKSUM_RE.search(text)
  context: dict[str, Any] = {}
  if m_iter := ITER_RE.search(text):
    context["iterations"] = int(m_iter.group(1))
  if m_warmup := WARMUP_RE.search(text):
    context["warmup_iterations"] = int(m_warmup.group(1))
  if m_dataset := DATASET_RE.search(text):
    context["dataset"] = m_dataset.group(1).strip()
  if not m_time:
    return None, (m_checksum.group(1) if m_checksum else None), context
  return (to_ms(float(m_time.group("value")), m_time.group("unit")),
          m_checksum.group(1) if m_checksum else None,
          context)


def parse_analyze_summary(path: Path, accepted_names: set[str]) -> tuple[float | None, float | None, float | None, dict[str, Any]]:
  if not path.exists():
    return None, None, None, {}
  text = path.read_text(encoding="utf-8", errors="replace")
  context: dict[str, Any] = {}
  if m_iter := ITER_RE.search(text):
    context["iterations"] = int(m_iter.group(1))
  if m_warmup := WARMUP_RE.search(text):
    context["warmup_iterations"] = int(m_warmup.group(1))
  if m_dataset := DATASET_RE.search(text):
    context["dataset"] = m_dataset.group(1).strip()
  if m_device := DEVICE_RE.search(text):
    context["device"] = m_device.group(1).strip()
  if m_vlen := VLEN_RE.search(text):
    context["vlen"] = m_vlen.group(1).strip()
  for match in ANALYZE_CASE_RE.finditer(text):
    if match.group("name").strip() in accepted_names:
      std_ms = to_ms(float(match.group("std_value")), match.group("std_unit"))
      rvv_ms = to_ms(float(match.group("rvv_value")), match.group("rvv_unit"))
      return std_ms, rvv_ms, float(match.group("speedup")), context
  return None, None, None, context


def percentile(values: list[float], pct: float) -> float:
  ordered = sorted(values)
  if len(ordered) == 1:
    return ordered[0]
  position = (len(ordered) - 1) * pct
  lower = int(position)
  upper = min(lower + 1, len(ordered) - 1)
  fraction = position - lower
  return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def unique_or_mixed(values: list[Any], default: Any = None) -> Any:
  present = [value for value in values if value not in (None, "")]
  if not present:
    return default
  first = present[0]
  return first if all(value == first for value in present) else "mixed"


def decision_bucket(values: list[float]) -> str:
  median = statistics.median(values)
  below_one = sum(1 for value in values if value < 1.0)
  if median >= 1.20 and below_one == 0:
    return "positive"
  if 1.05 <= median < 1.20 and below_one <= 1:
    return "weak_positive"
  if 0.95 <= median < 1.05:
    return "neutral"
  if median < 0.95:
    return "negative"
  return "unstable"


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--summary-dir", required=True)
  parser.add_argument("--output", required=True)
  parser.add_argument("--summary-output", required=True)
  parser.add_argument("--title", required=True)
  parser.add_argument("--run-label", required=True)
  parser.add_argument("--case-name", default="phase000_filter_indices_projection_mask_compress")
  parser.add_argument("--evidence-role", default="production_shaped_diagnostic")
  parser.add_argument("--expected-runs", type=int, required=True)
  parser.add_argument("--asm", default="")
  parser.add_argument("--wrapper", default="")
  parser.add_argument("--timer-boundary", default="")
  parser.add_argument("--row-source", default="")
  parser.add_argument("--group", default="")
  parser.add_argument("--implementation", default="")
  parser.add_argument("--baseline-implementation", default="")
  parser.add_argument("--candidate-implementation", default="")
  parser.add_argument("--point-type", default="")
  parser.add_argument("--checksum-policy", default="kept_indices_fnv1a")
  args = parser.parse_args()
  production_public = args.evidence_role in {"production-public", "production_direct", "production-direct"}
  boundary = "public_entry" if production_public else "test_support_helper"
  case_kind = "production_direct" if production_public else "production_shaped_diagnostic"
  wrapper = args.wrapper or (
    "ZBuffering::filter(model, indices, thres)" if production_public else "bench_occlusion_reasoning filter-index callsite"
  )
  timer_boundary = args.timer_boundary or (
    "production_public_filter_indices_after_computeDepthMap_setup_excludes_copyPointCloud"
    if production_public else
    "filter_indices_without_computeDepthMap_or_copyPointCloud"
  )
  row_source = args.row_source or (
    "model_point_stream_plus_member_depth_buffer"
    if production_public else
    "model_point_stream_plus_raw_depth_buffer"
  )
  group = args.group or ("phase010_production_filter" if production_public else "phase000_projection_filter")
  implementation = args.baseline_implementation or (
    "zBufferingFilterStd" if production_public else "filterIndicesScalarReference"
  )
  candidate_implementation = args.candidate_implementation or args.implementation or (
    "zBufferingFilterRVV" if production_public else "filterIndicesCandidate"
  )
  point_type = args.point_type or (
    "PointXYZ / float / AoS" if production_public else "ProjectionPoint / PointXYZ-like xyz fields"
  )

  summary_dir = Path(args.summary_dir)
  runs = []
  for run_dir in sorted(summary_dir.glob("run_*")):
    analyze_log = run_dir / "analyze_bench_compare.log"
    std_log = run_dir / "run_bench_std.log"
    rvv_log = run_dir / "run_bench_rvv.log"
    accepted_names = {
      args.case_name,
      "ZBuffering filter diagnostic",
      "phase000_filter_indices_projection_mask_compress",
      "production_filter_indices_projection_mask_compress",
      "public_inline_filter_visible_points",
      "public_inline_filter_occluded_points",
      "public_inline_filter_projection_mask_compress",
    }
    std_ms, rvv_ms, speedup, context = parse_analyze_summary(analyze_log, accepted_names)
    std_checksum = rvv_checksum = None
    std_context: dict[str, Any] = {}
    rvv_context: dict[str, Any] = {}
    raw_std_ms, std_checksum, std_context = parse_raw_bench(std_log, accepted_names)
    raw_rvv_ms, rvv_checksum, rvv_context = parse_raw_bench(rvv_log, accepted_names)
    if std_ms is None:
      std_ms = raw_std_ms
    if rvv_ms is None:
      rvv_ms = raw_rvv_ms
    if std_ms is None or rvv_ms is None:
      continue
    context = {**std_context, **rvv_context, **context}
    runs.append({
      "run": run_dir.name,
      "std_ms": std_ms,
      "rvv_ms": rvv_ms,
      "speedup": speedup if speedup is not None else (std_ms / rvv_ms if rvv_ms else 0.0),
      "std_checksum": std_checksum,
      "rvv_checksum": rvv_checksum,
      "context": context,
      "std_log": std_log,
      "rvv_log": rvv_log,
      "analyze_log": analyze_log,
    })

  if not runs:
    raise SystemExit(f"no repeated board runs parsed under {summary_dir}")

  speedups = [float(run["speedup"]) for run in runs]
  summary_lines = [
    f"# {args.title}",
    "",
    f"- run_label: `{args.run_label}`",
    f"- expected_runs: `{args.expected_runs}`",
    f"- collected_runs: `{len(runs)}`",
    f"- evidence_role: `{args.evidence_role}`",
    f"- A/B boundary: `{boundary}`",
    f"- timer_boundary: `{timer_boundary}`",
    f"- decision_bucket: `{decision_bucket(speedups)}`",
    f"- median_speedup: `{statistics.median(speedups):.3f}x`",
    f"- min_speedup: `{min(speedups):.3f}x`",
    f"- max_speedup: `{max(speedups):.3f}x`",
    f"- p10_speedup: `{percentile(speedups, 0.10):.3f}x`",
    f"- p90_speedup: `{percentile(speedups, 0.90):.3f}x`",
    f"- B/A < 1: `{sum(1 for value in speedups if value < 1.0)}/{len(speedups)}`",
    "",
    "| run | std ms | rvv ms | B/A | checksum |",
    "| --- | ---: | ---: | ---: | --- |",
  ]
  for run in runs:
    checksum_pair = f"std={run['std_checksum']};rvv={run['rvv_checksum']}"
    summary_lines.append(
      f"| {run['run']} | {run['std_ms']:.6f} | {run['rvv_ms']:.6f} | {run['speedup']:.6f} | {checksum_pair} |"
    )

  output = Path(args.output)
  output.parent.mkdir(parents=True, exist_ok=True)
  summary_output = Path(args.summary_output)
  summary_output.parent.mkdir(parents=True, exist_ok=True)
  summary_output.write_text("\n".join(summary_lines) + "\n")

  contexts = [run["context"] for run in runs]
  iterations = unique_or_mixed([context.get("iterations") for context in contexts])
  warmup_iterations = unique_or_mixed([context.get("warmup_iterations") for context in contexts])
  device = unique_or_mixed([context.get("device") for context in contexts], "RISC-V board / target hardware")
  vlen = unique_or_mixed([context.get("vlen") for context in contexts], "see SoC / ELF (board)")
  dataset = unique_or_mixed([context.get("dataset") for context in contexts])
  asm_path = Path(args.asm) if args.asm else None
  asm_hash = sha256_file(asm_path) if asm_path and asm_path.exists() else ""

  manifest = {
    "schema_version": 1,
    "summary": {
      "title": args.title,
      "evidence_role": args.evidence_role,
      "summary_path": str(summary_output),
      "metadata": {
        "run_count": len(runs),
        "expected_runs": args.expected_runs,
        "iterations": iterations,
        "warmup_iterations": warmup_iterations,
        "device": device,
        "vlen": vlen,
        "binary_hash": asm_hash,
        "dataset": dataset,
      },
    },
    "checks": {
      "strict_ab": False,
    },
    "comparisons": [{
      "name": args.case_name,
      "case_kind": case_kind,
      "evidence_role": args.evidence_role,
      "point_type": point_type,
      "size": dataset,
      "group": group,
      "run_count": len(runs),
      "iterations": iterations,
      "warmup_iterations": warmup_iterations,
      "ba_values": speedups,
      "decision_bucket": decision_bucket(speedups),
      "baseline": {
        "boundary": boundary,
        "wrapper": wrapper,
        "implementation": implementation,
        "row_source": row_source,
        "solve": "none",
        "checksum_policy": args.checksum_policy,
        "timer_boundary": timer_boundary,
        "gate": "same projection bounds and depth predicate",
        "mask": "projection bounds plus finite depth predicate",
        "reduction": "ordered indices append",
        "checksum": unique_or_mixed([run["std_checksum"] for run in runs]),
        "asm_boundary": "scalar reference build",
        "std_ms": statistics.median([float(run["std_ms"]) for run in runs]),
        "rvv_ms": statistics.median([float(run["rvv_ms"]) for run in runs]),
        "log_path": str(summary_dir),
      },
      "candidate": {
        "boundary": boundary,
        "wrapper": wrapper,
        "implementation": candidate_implementation,
        "row_source": row_source,
        "solve": "none",
        "checksum_policy": args.checksum_policy,
        "timer_boundary": timer_boundary,
        "gate": "same projection bounds and depth predicate",
        "mask": "projection bounds plus finite depth predicate",
        "reduction": "ordered indices append",
        "checksum": unique_or_mixed([run["rvv_checksum"] for run in runs]),
        "asm_boundary": str(args.asm) if args.asm else "",
        "rvv_ms": statistics.median([float(run["rvv_ms"]) for run in runs]),
        "std_ms": statistics.median([float(run["std_ms"]) for run in runs]),
        "log_path": str(summary_dir),
        "notes": "__RVV10__ build vectorizes xyz projection and integer pixel computation; lane-order scalar depth append preserves filter output order.",
      },
    }],
  }
  output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
