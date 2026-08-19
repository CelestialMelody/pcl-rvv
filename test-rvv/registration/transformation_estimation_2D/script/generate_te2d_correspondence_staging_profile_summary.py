#!/usr/bin/env python3
"""生成 transformation_estimation_2D correspondence staging profile 摘要。"""

from __future__ import annotations

import argparse
import hashlib
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
BUILD_RE = re.compile(r"^Build:\s*(?P<value>.+?)\s*$")

DIRECT_PREFIX = "direct correspondence public 2D correspondence-pair"
STAGED_PREFIX = "staged-dual-indexed public 2D correspondence-pair"
MATERIALIZE_PREFIX = "materialize-ordered public 2D correspondence-pair"


def sha256_file(path: Path) -> str:
  if not path.is_file():
    return "not_recorded"
  digest = hashlib.sha256()
  with path.open("rb") as handle:
    for chunk in iter(lambda: handle.read(1024 * 1024), b""):
      digest.update(chunk)
  return "sha256:" + digest.hexdigest()


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
    if match := BUILD_RE.match(line):
      metadata["build"] = match.group("value")
      continue
    if match := CASE_RE.match(line):
      current_case = match.group("name").strip()
      cases[current_case] = {"ms": float(match.group("ms"))}
      continue
    if current_case and (match := CHECKSUM_RE.search(line)):
      cases[current_case]["checksum"] = match.group("checksum")
      current_case = None
  return {"path": str(path), "metadata": metadata, "cases": cases}


def collect_runs(root: Path, expected_runs: int) -> list[dict[str, Any]]:
  runs: list[dict[str, Any]] = []
  for directory in sorted(path for path in root.iterdir() if path.is_dir() and path.name.startswith("run-")):
    rvv_path = directory / "run_bench_rvv.log"
    if not rvv_path.exists():
      raise SystemExit(f"missing RVV log under {directory}")
    runs.append({"name": directory.name, "rvv": parse_log(rvv_path)})
  if len(runs) != expected_runs:
    raise SystemExit(f"expected {expected_runs} run-* dirs under {root}, got {len(runs)}")
  return runs


def size_suffix(label: str) -> str:
  match = re.search(r"(\d+K)", label)
  if not match:
    raise SystemExit(f"cannot infer size suffix from case label: {label}")
  return match.group(1)


def size_value(suffix: str) -> int:
  return int(suffix.rstrip("K")) * 1024


def bucket_for(values: list[float]) -> str:
  if not values:
    return "blocked"
  median = statistics.median(values)
  if all(value > 1.15 for value in values):
    return "positive"
  if median >= 1.03 and min(values) >= 0.97:
    return "weak_positive"
  if all(0.97 <= value <= 1.03 for value in values):
    return "neutral"
  if median < 0.97 or min(values) < 0.97:
    return "negative"
  return "unstable"


def checksum_set(rows: list[dict[str, Any]], key: str) -> list[str]:
  return sorted({str(row.get(f"{key}_checksum", "missing")) for row in rows})


def build_metrics(runs: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
  first_cases = runs[0]["rvv"]["cases"]
  labels = sorted(name for name in first_cases if name.startswith(DIRECT_PREFIX))
  if not labels:
    raise SystemExit(f"no {DIRECT_PREFIX} cases found")

  metrics: dict[str, dict[str, Any]] = {}
  for direct_label in labels:
    suffix = size_suffix(direct_label)
    staged_label = f"{STAGED_PREFIX} {suffix}"
    materialize_label = f"{MATERIALIZE_PREFIX} {suffix}"
    rows: list[dict[str, Any]] = []
    direct_over_staged: list[float] = []
    materialize_over_staged: list[float] = []
    materialize_over_direct: list[float] = []
    for run in runs:
      cases = run["rvv"]["cases"]
      for label in (direct_label, staged_label, materialize_label):
        if label not in cases:
          raise SystemExit(f"missing {label} in {run['name']}")
      direct = cases[direct_label]
      staged = cases[staged_label]
      materialize = cases[materialize_label]
      direct_ms = float(direct["ms"])
      staged_ms = float(staged["ms"])
      materialize_ms = float(materialize["ms"])
      direct_over_staged.append(direct_ms / staged_ms)
      materialize_over_staged.append(materialize_ms / staged_ms)
      materialize_over_direct.append(materialize_ms / direct_ms)
      rows.append(
        {
          "run": run["name"],
          "direct_ms": direct_ms,
          "staged_ms": staged_ms,
          "materialize_ms": materialize_ms,
          "direct_over_staged": direct_over_staged[-1],
          "materialize_over_staged": materialize_over_staged[-1],
          "materialize_over_direct": materialize_over_direct[-1],
          "direct_checksum": direct.get("checksum"),
          "staged_checksum": staged.get("checksum"),
          "materialize_checksum": materialize.get("checksum"),
        }
      )
    metrics[suffix] = {
      "size": size_value(suffix),
      "rows": rows,
      "direct_over_staged": direct_over_staged,
      "materialize_over_staged": materialize_over_staged,
      "materialize_over_direct": materialize_over_direct,
      "bucket": bucket_for(direct_over_staged),
    }
  return dict(sorted(metrics.items(), key=lambda item: size_value(item[0])))


def overall_decision(metrics: dict[str, dict[str, Any]]) -> str:
  target = [info for suffix, info in metrics.items() if suffix != "4K"] or list(metrics.values())
  buckets = {info["bucket"] for info in target}
  if buckets <= {"positive"}:
    return "positive"
  if buckets <= {"positive", "weak_positive"}:
    return "weak_positive"
  if "negative" in buckets:
    return "negative"
  if "unstable" in buckets:
    return "unstable"
  return "neutral"


def side(boundary: str,
         asm_boundary: str,
         value_key: str,
         value: float,
         checksum: list[str]) -> dict[str, Any]:
  return {
    "boundary": boundary,
    "asm_boundary": asm_boundary,
    "wrapper": "bench_te2d",
    "row_source": "correspondence_pair",
    "solve": True,
    "checksum_policy": "matrix_checksum_same_input_same_output_correctness_checked_by_gtest",
    "checksum": checksum,
    "timer_boundary": "correspondence_staging_profile_ingress_plus_two_pass_centered_correlation_plus_2d_solve",
    "gate": "pointxyz_float_valid_correspondences_dense_selected_finite_size_ge_16",
    "mask": "dense_finite_selected_rows_prechecked",
    "reduction": "rvv_two_pass_centered_sum_tree",
    value_key: value,
  }


def build_manifest(
  runs: list[dict[str, Any]],
  metrics: dict[str, dict[str, Any]],
  summary_path: Path,
  args: argparse.Namespace,
) -> dict[str, Any]:
  first_meta = runs[0]["rvv"]["metadata"]
  comparisons: list[dict[str, Any]] = []
  for suffix, info in metrics.items():
    direct_median = statistics.median(row["direct_ms"] for row in info["rows"])
    staged_median = statistics.median(row["staged_ms"] for row in info["rows"])
    comparisons.append(
      {
        "name": f"correspondence direct-vs-staged-dual production-detail profile {suffix}",
        "group": "te2d_correspondence_staging_profile",
        "case_kind": "production_detail_staging_profile",
        "evidence_role": "production_detail",
        "row_source": "correspondence_pair",
        "point_type": "PointXYZ",
        "source_point_type": "PointXYZ",
        "target_point_type": "PointXYZ",
        "scalar": "float",
        "size": info["size"],
        "run_count": len(runs),
        "ba_values": info["direct_over_staged"],
        "decision_bucket": info["bucket"],
        "std_ms": direct_median,
        "rvv_ms": staged_median,
        "allowed_contract_mismatches": ["boundary", "timer_boundary", "asm_boundary"],
        "notes": (
          "B/A = direct correspondence public RVV ms / staged-dual-indexed public RVV ms; "
          "大于 1 表示 staged-dual 更快。staging copy cost is inside the timer boundary."
        ),
        "baseline": side(
          "production_public_correspondence_direct_gather",
          "production_public_correspondence_boundary",
          "std_ms",
          direct_median,
          checksum_set(info["rows"], "direct"),
        ),
        "candidate": side(
          "correspondence_staged_indices_plus_production_public_dual_indexed",
          "production_public_dual_indexed_boundary",
          "rvv_ms",
          staged_median,
          checksum_set(info["rows"], "staged"),
        ),
      }
    )
  return {
    "schema_version": 1,
    "summary": {
      "title": "transformation_estimation_2D correspondence staging profile",
      "evidence_role": "production_detail",
      "summary_path": str(summary_path),
      "label": "correspondence direct public vs staged indices + dual-indexed public",
      "analysis_script": "test-rvv/registration/transformation_estimation_2D/script/generate_te2d_correspondence_staging_profile_summary.py",
      "metadata": {
        "device": args.device_label,
        "iterations": first_meta.get("iterations"),
        "warmup_iterations": first_meta.get("warmup_iterations"),
        "run_count": len(runs),
        "taskset": args.taskset,
        "governor": args.governor,
        "freq": args.freq,
        "temperature": args.temperature,
        "vlen": args.vlen_desc,
        "binary_hash": f"rvv={sha256_file(Path(args.rvv_bin))}",
        "dataset": first_meta.get("dataset"),
        "build": first_meta.get("build"),
        "case_filter": args.case_filter,
        "point_type": "PointXYZ",
        "source_point_type": "PointXYZ",
        "target_point_type": "PointXYZ",
        "row_source": "correspondence_pair",
        "decision_bucket": overall_decision(metrics),
      },
    },
    "checks": {
      "strict_ab": True,
      "equal_fields": [
        "wrapper",
        "row_source",
        "solve",
        "checksum_policy",
        "gate",
        "mask",
        "reduction",
      ],
      "thresholds": {"min_repeated_runs": args.expected_runs},
    },
    "runs": [run["name"] for run in runs],
    "artifacts": {
      "input_dir": str(Path(args.input_dir)),
      "summary_path": str(summary_path),
      "rvv_bin": args.rvv_bin,
    },
    "comparisons": comparisons,
  }


def values_text(values: list[float]) -> str:
  return ", ".join(f"{value:.3f}x" for value in values)


def med(rows: list[dict[str, Any]], key: str) -> float:
  return statistics.median(row[key] for row in rows)


def checksum_note(info: dict[str, Any]) -> str:
  direct = checksum_set(info["rows"], "direct")
  staged = checksum_set(info["rows"], "staged")
  materialize = checksum_set(info["rows"], "materialize")
  if direct == staged == materialize:
    return "same"
  return (
    f"direct `{', '.join(direct)}` / staged `{', '.join(staged)}` / "
    f"materialize `{', '.join(materialize)}`"
  )


def render_summary(
  runs: list[dict[str, Any]],
  metrics: dict[str, dict[str, Any]],
  manifest_path: Path,
  doctor_path: Path,
  args: argparse.Namespace,
) -> str:
  first_meta = runs[0]["rvv"]["metadata"]
  lines: list[str] = [
    "# transformation_estimation_2D Correspondence Staging Profile Summary",
    "",
    "## 运行合同",
    "",
    f"- run_label：`{args.run_label}`",
    f"- runs：`{', '.join(run['name'] for run in runs)}`",
    f"- device：`{args.device_label}`",
    f"- case-filter：`{args.case_filter}`",
    "- evidence_role：`production_detail`；这是 staging/profile 证据，不是 production adoption。",
    f"- iterations：`{first_meta.get('iterations')}`",
    f"- warm-up iterations：`{first_meta.get('warmup_iterations')}`",
    "- D/S：`direct correspondence public RVV ms / staged-dual-indexed public RVV ms`，大于 1 表示 staged-dual 更快。",
    "- M/S：`materialize ordered public RVV ms / staged-dual-indexed public RVV ms`，大于 1 表示 staged-dual 快于完整物化。",
    "- M/D：沿用 Phase 094 的 `materialize / direct` 口径，大于 1 表示 direct 快于完整物化。",
    f"- manifest：`{manifest_path}`",
    f"- Evidence Doctor：`{doctor_path}`",
    "",
    "## 结果",
    "",
    "staged-dual 路径把 correspondence query/match 拷成连续 source/target indices，并把 staging copy 计入计时边界。",
    "",
    "| size | D/S values | D/S median | D/S min | D/S max | bucket | M/S median | M/D median | direct_ms | staged_ms | materialize_ms | checksum |",
    "| --- | --- | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | --- |",
  ]
  for suffix, info in metrics.items():
    lines.append(
      f"| {suffix} | {values_text(info['direct_over_staged'])} | "
      f"{statistics.median(info['direct_over_staged']):.3f}x | "
      f"{min(info['direct_over_staged']):.3f}x | {max(info['direct_over_staged']):.3f}x | "
      f"`{info['bucket']}` | {statistics.median(info['materialize_over_staged']):.3f}x | "
      f"{statistics.median(info['materialize_over_direct']):.3f}x | "
      f"{med(info['rows'], 'direct_ms'):.4f} | {med(info['rows'], 'staged_ms'):.4f} | "
      f"{med(info['rows'], 'materialize_ms'):.4f} | {checksum_note(info)} |"
    )
  lines.extend(
    [
      "",
      "## 决策桶",
      "",
      f"- overall_decision_bucket：`{overall_decision(metrics)}`（默认按 64K / 256K 判断，4K 保留为小规模参考）。",
      "- positive / weak_positive 只说明 staged-dual 可继续进入 production-dispatch A/B；不能自动采纳。",
      "- negative / unstable 说明 staging 没有解释或缓解当前长尾，应继续 profile 或回到用户判断。",
      "",
    ]
  )
  return "\n".join(lines)


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--input-dir", required=True, type=Path)
  parser.add_argument("--output-dir", required=True, type=Path)
  parser.add_argument("--run-label", required=True)
  parser.add_argument("--case-filter", default="correspondence-staging-profile")
  parser.add_argument("--expected-runs", type=int, default=20)
  parser.add_argument("--rvv-bin", required=True)
  parser.add_argument("--device-label", default="RVV board")
  parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
  parser.add_argument("--taskset", default="not_recorded_board_target")
  parser.add_argument("--governor", default="not_recorded_board_target")
  parser.add_argument("--freq", default="not_recorded_board_target")
  parser.add_argument("--temperature", default="not_recorded_board_target")
  args = parser.parse_args()

  runs = collect_runs(args.input_dir, args.expected_runs)
  metrics = build_metrics(runs)
  args.output_dir.mkdir(parents=True, exist_ok=True)
  summary_path = args.output_dir / "summary.md"
  manifest_path = args.output_dir / "evidence_manifest.json"
  doctor_path = args.output_dir / "evidence_doctor.md"
  manifest = build_manifest(runs, metrics, summary_path, args)
  manifest_path.write_text(
    json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
  )
  summary_path.write_text(
    render_summary(runs, metrics, manifest_path, doctor_path, args), encoding="utf-8"
  )
  print(f"wrote summary: {summary_path}")
  print(f"wrote manifest: {manifest_path}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
