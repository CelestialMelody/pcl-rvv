#!/usr/bin/env python3
"""生成 transformation_estimation_2D correspondence component ablation 摘要。"""

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
COMPONENT_RE = re.compile(
  r"^(?P<label>component scan-correspondence-structs|component extract-indices|"
  r"component gather-xyz-no-solve|component materialize-rows-no-solve|"
  r"prematerialized ordered public full|direct correspondence public full|"
  r"staged-dual-indexed public full|materialize-ordered public full) "
  r"2D correspondence-pair (?P<profile>[A-Za-z0-9_-]+) (?P<size>\d+K)$"
)

LABEL_TO_KEY = {
  "component scan-correspondence-structs": "scan_structs",
  "component extract-indices": "extract_indices",
  "component gather-xyz-no-solve": "gather_xyz_no_solve",
  "component materialize-rows-no-solve": "materialize_rows_no_solve",
  "prematerialized ordered public full": "prematerialized_ordered_full",
  "direct correspondence public full": "direct_full",
  "staged-dual-indexed public full": "staged_full",
  "materialize-ordered public full": "materialize_ordered_full",
}

COMPONENT_KEYS = [
  "scan_structs",
  "extract_indices",
  "gather_xyz_no_solve",
  "materialize_rows_no_solve",
]
FULL_KEYS = [
  "prematerialized_ordered_full",
  "direct_full",
  "staged_full",
  "materialize_ordered_full",
]
ALL_KEYS = COMPONENT_KEYS + FULL_KEYS


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


def parse_case_name(name: str) -> tuple[str, str, str] | None:
  match = COMPONENT_RE.match(name)
  if not match:
    return None
  return LABEL_TO_KEY[match.group("label")], match.group("profile"), match.group("size")


def label_for(key: str, profile: str, size: str) -> str:
  for label, mapped in LABEL_TO_KEY.items():
    if mapped == key:
      return f"{label} 2D correspondence-pair {profile} {size}"
  raise KeyError(key)


def checksum_set(rows: list[dict[str, Any]], key: str) -> list[str]:
  return sorted({str(row.get(f"{key}_checksum", "missing")) for row in rows})


def med(rows: list[dict[str, Any]], key: str) -> float:
  return statistics.median(row[key] for row in rows)


def ratio(numerator: float, denominator: float) -> float:
  return numerator / denominator if denominator else 0.0


def build_metrics(runs: list[dict[str, Any]]) -> dict[tuple[str, str], dict[str, Any]]:
  first_cases = runs[0]["rvv"]["cases"]
  expected: set[tuple[str, str]] = set()
  for name in first_cases:
    parsed = parse_case_name(name)
    if parsed:
      _, profile, size = parsed
      expected.add((profile, size))
  if not expected:
    raise SystemExit("no correspondence component ablation cases found")

  metrics: dict[tuple[str, str], dict[str, Any]] = {}
  for profile, size in sorted(expected, key=lambda item: (item[0], size_value(item[1]))):
    rows: list[dict[str, Any]] = []
    direct_over_staged: list[float] = []
    materialize_over_direct: list[float] = []
    prematerialized_over_direct: list[float] = []
    for run in runs:
      cases = run["rvv"]["cases"]
      row: dict[str, Any] = {"run": run["name"]}
      for key in ALL_KEYS:
        label = label_for(key, profile, size)
        if label not in cases:
          raise SystemExit(f"missing {label} in {run['name']}")
        row[f"{key}_ms"] = float(cases[label]["ms"])
        row[f"{key}_checksum"] = cases[label].get("checksum")
      direct_over_staged.append(
        ratio(row["direct_full_ms"], row["staged_full_ms"])
      )
      materialize_over_direct.append(
        ratio(row["materialize_ordered_full_ms"], row["direct_full_ms"])
      )
      prematerialized_over_direct.append(
        ratio(row["prematerialized_ordered_full_ms"], row["direct_full_ms"])
      )
      row["direct_over_staged"] = direct_over_staged[-1]
      row["materialize_over_direct"] = materialize_over_direct[-1]
      row["prematerialized_over_direct"] = prematerialized_over_direct[-1]
      rows.append(row)
    metrics[(profile, size)] = {
      "profile": profile,
      "size_suffix": size,
      "size": size_value(size),
      "rows": rows,
      "direct_over_staged": direct_over_staged,
      "materialize_over_direct": materialize_over_direct,
      "prematerialized_over_direct": prematerialized_over_direct,
      "bucket": bucket_for(direct_over_staged),
    }
  return metrics


def overall_decision(metrics: dict[tuple[str, str], dict[str, Any]]) -> str:
  target = [info for (_, size), info in metrics.items() if size in ("64K", "256K")]
  target = target or list(metrics.values())
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


def component_comparison(
  key: str,
  profile: str,
  size: str,
  info: dict[str, Any],
) -> dict[str, Any]:
  solve = key in FULL_KEYS
  return {
    "name": f"correspondence component ablation {key} {profile} {size}",
    "group": "te2d_correspondence_component_ablation",
    "case_kind": "component_ablation_full_anchor" if solve else "component_ablation_no_solve",
    "evidence_role": "component_ablation",
    "row_source": "correspondence_pair",
    "point_type": "PointXYZ",
    "source_point_type": "PointXYZ",
    "target_point_type": "PointXYZ",
    "scalar": "float",
    "profile": profile,
    "size": info["size"],
    "run_count": len(info["rows"]),
    "rvv_ms": med(info["rows"], f"{key}_ms"),
    "rvv_ms_values": [row[f"{key}_ms"] for row in info["rows"]],
    "checksum": checksum_set(info["rows"], key),
    "checksum_policy": (
      "matrix_checksum_same_input_same_output_correctness_checked_by_gtest"
      if solve
      else "component_checksum_mixed_sink_not_matrix_correctness"
    ),
    "boundary": key,
    "wrapper": "bench_te2d",
    "solve": solve,
    "timer_boundary": (
      "correspondence_component_ablation_full_estimate_anchor"
      if solve
      else "correspondence_component_ablation_no_solve_component_only"
    ),
    "gate": "pointxyz_float_valid_correspondences_dense_selected_finite_size_ge_16",
    "mask": "dense_finite_selected_rows_prechecked",
    "reduction": "rvv_or_scalar_public_path_for_full_anchor" if solve else "no_solve_component_checksum",
    "notes": (
      "component no-solve labels are mixed-sink diagnostics and must not be used as "
      "strict production A/B. Full anchors use matrix checksum and existing public helpers."
    ),
  }


def full_anchor_direct_staged_comparison(
  profile: str,
  size: str,
  info: dict[str, Any],
) -> dict[str, Any]:
  return {
    "name": f"correspondence component ablation full-anchor direct-vs-staged {profile} {size}",
    "group": "te2d_correspondence_component_ablation_full_anchor",
    "case_kind": "component_ablation_full_anchor_profile",
    "evidence_role": "component_ablation",
    "row_source": "correspondence_pair",
    "point_type": "PointXYZ",
    "source_point_type": "PointXYZ",
    "target_point_type": "PointXYZ",
    "scalar": "float",
    "profile": profile,
    "size": info["size"],
    "run_count": len(info["rows"]),
    "ba_values": info["direct_over_staged"],
    "decision_bucket": info["bucket"],
    "std_ms": med(info["rows"], "direct_full_ms"),
    "rvv_ms": med(info["rows"], "staged_full_ms"),
    "notes": (
      "B/A = direct correspondence public full RVV ms / staged-dual-indexed public full RVV ms; "
      "大于 1 表示 staged-dual full anchor 更快。本 comparison 仍是 component-ablation profile。"
    ),
  }


def build_manifest(
  runs: list[dict[str, Any]],
  metrics: dict[tuple[str, str], dict[str, Any]],
  summary_path: Path,
  args: argparse.Namespace,
) -> dict[str, Any]:
  first_meta = runs[0]["rvv"]["metadata"]
  comparisons: list[dict[str, Any]] = []
  for (profile, size), info in metrics.items():
    for key in ALL_KEYS:
      comparisons.append(component_comparison(key, profile, size, info))
    comparisons.append(full_anchor_direct_staged_comparison(profile, size, info))
  return {
    "schema_version": 1,
    "summary": {
      "title": "transformation_estimation_2D correspondence component ablation",
      "evidence_role": "component_ablation",
      "summary_path": str(summary_path),
      "label": "correspondence no-solve component costs plus full public anchors",
      "analysis_script": "test-rvv/registration/transformation_estimation_2D/script/generate_te2d_correspondence_component_ablation_summary.py",
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
      "strict_ab": False,
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


def checksum_note(info: dict[str, Any], keys: list[str]) -> str:
  parts = [f"{key}=`{', '.join(checksum_set(info['rows'], key))}`" for key in keys]
  return " / ".join(parts)


def render_summary(
  runs: list[dict[str, Any]],
  metrics: dict[tuple[str, str], dict[str, Any]],
  manifest_path: Path,
  doctor_path: Path,
  args: argparse.Namespace,
) -> str:
  first_meta = runs[0]["rvv"]["metadata"]
  lines: list[str] = [
    "# transformation_estimation_2D Correspondence Component Ablation Summary",
    "",
    "## 运行合同",
    "",
    f"- run_label：`{args.run_label}`",
    f"- runs：`{', '.join(run['name'] for run in runs)}`",
    f"- device：`{args.device_label}`",
    f"- case-filter：`{args.case_filter}`",
    "- evidence_role：`component_ablation`；这是组件消融，不是 production adoption。",
    f"- iterations：`{first_meta.get('iterations')}`",
    f"- warm-up iterations：`{first_meta.get('warmup_iterations')}`",
    "- D/S：`direct correspondence public full RVV ms / staged-dual-indexed public full RVV ms`，大于 1 表示 staged-dual full anchor 更快。",
    "- no-solve 组件使用 component checksum，不能和 matrix checksum 直接等价。",
    f"- manifest：`{manifest_path}`",
    f"- Evidence Doctor：`{doctor_path}`",
    "",
    "## 组件耗时",
    "",
    "| profile | size | scan | extract | gather xyz | materialize rows | direct full | gather/direct | materialize/direct | checksum |",
    "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
  ]
  for (profile, size), info in metrics.items():
    direct_ms = med(info["rows"], "direct_full_ms")
    gather_ms = med(info["rows"], "gather_xyz_no_solve_ms")
    materialize_ms = med(info["rows"], "materialize_rows_no_solve_ms")
    lines.append(
      f"| {profile} | {size} | {med(info['rows'], 'scan_structs_ms'):.4f} | "
      f"{med(info['rows'], 'extract_indices_ms'):.4f} | {gather_ms:.4f} | "
      f"{materialize_ms:.4f} | {direct_ms:.4f} | "
      f"{ratio(gather_ms, direct_ms):.3f}x | {ratio(materialize_ms, direct_ms):.3f}x | "
      f"{checksum_note(info, COMPONENT_KEYS)} |"
    )

  lines.extend(
    [
      "",
      "## Full Anchors",
      "",
      "| profile | size | D/S values | D/S median | D/S<1 | bucket | premat/direct | materialize/direct | premat full | direct full | staged full | materialize full | checksum |",
      "| --- | --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
  )
  for (profile, size), info in metrics.items():
    ds_values = info["direct_over_staged"]
    below_one = sum(1 for value in ds_values if value < 1.0)
    lines.append(
      f"| {profile} | {size} | {values_text(ds_values)} | "
      f"{statistics.median(ds_values):.3f}x | {below_one}/{len(ds_values)} | "
      f"`{info['bucket']}` | {statistics.median(info['prematerialized_over_direct']):.3f}x | "
      f"{statistics.median(info['materialize_over_direct']):.3f}x | "
      f"{med(info['rows'], 'prematerialized_ordered_full_ms'):.4f} | "
      f"{med(info['rows'], 'direct_full_ms'):.4f} | "
      f"{med(info['rows'], 'staged_full_ms'):.4f} | "
      f"{med(info['rows'], 'materialize_ordered_full_ms'):.4f} | "
      f"{checksum_note(info, FULL_KEYS)} |"
    )

  lines.extend(
    [
      "",
      "## 决策桶",
      "",
      f"- overall_decision_bucket：`{overall_decision(metrics)}`（只看 full-anchor D/S 的 64K / 256K，组件 no-solve 不参与 production 取舍）。",
      "- 组件耗时用于解释 correspondence ingress / gather / materialize 成本，不单独支持 dispatch switch。",
      "- 如果 full-anchor 或组件信号正向，也必须另开 production PI2-PI5 并等待用户确认。",
      "",
    ]
  )
  return "\n".join(lines)


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--input-dir", required=True, type=Path)
  parser.add_argument("--output-dir", required=True, type=Path)
  parser.add_argument("--run-label", required=True)
  parser.add_argument("--case-filter", default="correspondence-component-ablation")
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
