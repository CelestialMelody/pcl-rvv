#!/usr/bin/env python3
"""生成 correspondence direct index stream 点型/layout 扩展的板卡摘要。

该脚本比较 Std/RVV 两个 bench binary 中的同名
`correspondence-point-type-layout` case。它只覆盖 test-rvv diagnostic（诊断）
边界：row_source_policy 为 correspondence-pair，index_pattern 为 strided，
point type 为 PointXYZI 或 PointXYZRGB。summary 和 manifest 不能替代
production direct evidence（真实生产路径证据）。
"""

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
POINT_TYPE_CASE_RE = re.compile(
    r"^direct index stream candidate correspondence-pair "
    r"(?P<point_type>PointXYZI|PointXYZRGB) strided (?P<size>\d+K)$"
)


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
    elif match := WARMUP_RE.match(line):
      metadata["warmup_iterations"] = int(match.group("value"))
    elif match := DATASET_RE.match(line):
      metadata["dataset"] = match.group("value")
    elif match := CASE_RE.match(line):
      current_case = match.group("name").strip()
      cases[current_case] = {"ms": float(match.group("ms"))}
    elif current_case and (match := CHECKSUM_RE.search(line)):
      cases[current_case]["checksum"] = match.group("checksum")
      current_case = None
  return {"metadata": metadata, "cases": cases}


def collect_runs(root: Path, expected_runs: int) -> list[dict[str, Any]]:
  runs: list[dict[str, Any]] = []
  for directory in sorted(path for path in root.iterdir() if path.is_dir() and path.name.startswith("run-")):
    std_path = directory / "run_bench_std.log"
    rvv_path = directory / "run_bench_rvv.log"
    if not std_path.exists() or not rvv_path.exists():
      raise SystemExit(f"missing Std/RVV logs under {directory}")
    std = parse_log(std_path)
    rvv = parse_log(rvv_path)
    missing = sorted(set(std["cases"]) ^ set(rvv["cases"]))
    if missing:
      raise SystemExit(f"case set mismatch under {directory}: {', '.join(missing)}")
    runs.append({"name": directory.name, "std": std, "rvv": rvv})
  if len(runs) != expected_runs:
    raise SystemExit(f"expected {expected_runs} run-* dirs under {root}, got {len(runs)}")
  return runs


def size_value(size: str) -> int:
  return int(size.rstrip("K")) * 1024


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


def metric(values: list[float]) -> dict[str, Any]:
  return {
      "values": values,
      "median": statistics.median(values),
      "min": min(values),
      "max": max(values),
      "bucket": bucket_for(values),
  }


def fmt(values: list[float]) -> str:
  return ", ".join(f"{value:.3f}" for value in values)


def collect_labels(runs: list[dict[str, Any]]) -> list[tuple[str, str, str]]:
  labels: list[tuple[str, str, str]] = []
  for name in sorted(runs[0]["std"]["cases"]):
    match = POINT_TYPE_CASE_RE.match(name)
    if match:
      labels.append((name, match.group("point_type"), match.group("size")))
  if not labels:
    raise SystemExit("no correspondence point-type layout cases found")
  return labels


def build_metrics(runs: list[dict[str, Any]]) -> list[dict[str, Any]]:
  result: list[dict[str, Any]] = []
  for label, point_type, size in collect_labels(runs):
    values: list[float] = []
    rows: list[dict[str, Any]] = []
    std_checksums: set[str] = set()
    rvv_checksums: set[str] = set()
    for run in runs:
      std_case = run["std"]["cases"][label]
      rvv_case = run["rvv"]["cases"][label]
      std_ms = float(std_case["ms"])
      rvv_ms = float(rvv_case["ms"])
      values.append(std_ms / rvv_ms)
      std_checksums.add(str(std_case.get("checksum")))
      rvv_checksums.add(str(rvv_case.get("checksum")))
      rows.append({"run": run["name"], "std_ms": std_ms, "rvv_ms": rvv_ms, "ba": values[-1]})
    result.append(
        {
            "label": label,
            "point_type": point_type,
            "size_label": size,
            "size": size_value(size),
            "same_boundary": metric(values),
            "rows": rows,
            "checksums": {"std": sorted(std_checksums), "rvv": sorted(rvv_checksums)},
        }
    )
  return sorted(result, key=lambda item: (item["point_type"], item["size"]))


def side(point_type: str, build: str, ms_key: str, ms: float, checksums: list[str]) -> dict[str, Any]:
  return {
      "label": build,
      "boundary": "test_support_correspondence_direct_index_stream_point_type_layout",
      "wrapper": "bench_tedq",
      "row_source": "correspondence-pair",
      "point_type": point_type,
      "solve": True,
      "checksum_policy": "path_fingerprint_matrix_correctness_checked_by_gtest_budget",
      "checksum": ["matrix_budget_passed_by_run_test_compare"],
      "path_fingerprint": checksums,
      "timer_boundary": "correspondence_index_stream_plus_gather_plus_c1_c2_accumulation_plus_eigen_4x4_solve",
      "gate": f"dense_correspondence_pair_{point_type}_float_xyz_aos",
      "mask": "not_applicable_dense_only",
      "reduction": "scalar_same_chain" if build == "Std build" else "rvv_f64_tree",
      "asm_boundary": "not_applicable_std_build" if build == "Std build" else "bench_binary_correspondence_direct_index_stream",
      ms_key: ms,
  }


def overall(metrics: list[dict[str, Any]]) -> str:
  buckets = {item["same_boundary"]["bucket"] for item in metrics if item["size_label"] != "4K"}
  if not buckets:
    buckets = {item["same_boundary"]["bucket"] for item in metrics}
  if buckets <= {"positive"}:
    return "positive"
  if buckets <= {"positive", "weak_positive"}:
    return "weak_positive"
  if "negative" in buckets:
    return "negative"
  if "unstable" in buckets:
    return "unstable"
  return "neutral"


def build_manifest(runs: list[dict[str, Any]], metrics: list[dict[str, Any]], args: argparse.Namespace, summary_path: Path) -> dict[str, Any]:
  first_meta = runs[0]["std"]["metadata"]
  comparisons: list[dict[str, Any]] = []
  for item in metrics:
    std_med = statistics.median(row["std_ms"] for row in item["rows"])
    rvv_med = statistics.median(row["rvv_ms"] for row in item["rows"])
    comparisons.append(
        {
            "name": f"correspondence point-type layout {item['point_type']} {item['size_label']}",
            "group": "tedq_correspondence_point_type_layout",
            "case_kind": "point_type_layout_diagnostic",
            "evidence_role": "diagnostic",
            "row_source": "correspondence-pair",
            "point_type": item["point_type"],
            "scalar": "float",
            "size": item["size"],
            "run_count": len(runs),
            "ba_values": item["same_boundary"]["values"],
            "decision_bucket": item["same_boundary"]["bucket"],
            "std_ms": std_med,
            "rvv_ms": rvv_med,
            "allowed_contract_mismatches": ["reduction", "asm_boundary"],
            "notes": "B/A = Std direct index stream ms / RVV direct index stream ms for the same point type and correspondence index pattern. This is diagnostic evidence, not public-entry evidence.",
            "baseline": side(item["point_type"], "Std build", "std_ms", std_med, item["checksums"]["std"]),
            "candidate": side(item["point_type"], "RVV build", "rvv_ms", rvv_med, item["checksums"]["rvv"]),
        }
    )
  return {
      "schema_version": 1,
      "summary": {
          "title": "transformation_estimation_dual_quaternion correspondence point-type layout repeated diagnostic",
          "evidence_role": "diagnostic",
          "summary_path": str(summary_path),
          "label": "correspondence direct index stream PointXYZI/PointXYZRGB layout diagnostic; test-rvv only",
          "analysis_script": "test-rvv/registration/transformation_estimation_dual_quaternion/script/generate_tedq_correspondence_point_type_layout_summary.py",
          "metadata": {
              "device": args.device_label,
              "iterations": first_meta.get("iterations"),
              "warmup_iterations": first_meta.get("warmup_iterations"),
              "run_count": len(runs),
              "taskset": "not_pinned",
              "governor": "not_recorded",
              "freq": "not_recorded",
              "temperature": "not_recorded",
              "vlen": args.vlen_desc,
              "binary_hash": f"std={sha256_file(Path(args.std_bin))}; rvv={sha256_file(Path(args.rvv_bin))}",
              "dataset": first_meta.get("dataset"),
              "decision_bucket": overall(metrics),
          },
      },
      "checks": {
          "strict_ab": False,
          "equal_fields": ["boundary", "wrapper", "row_source", "point_type", "solve", "checksum_policy", "timer_boundary", "gate", "mask"],
          "thresholds": {"min_repeated_runs": args.expected_runs},
      },
      "runs": [run["name"] for run in runs],
      "artifacts": {"input_dir": args.input_dir, "summary_path": str(summary_path), "std_bin": args.std_bin, "rvv_bin": args.rvv_bin},
      "comparisons": comparisons,
  }


def checksum_note(item: dict[str, Any]) -> str:
  if item["checksums"]["std"] == item["checksums"]["rvv"]:
    return "same"
  return f"Std `{', '.join(item['checksums']['std'])}` / RVV `{', '.join(item['checksums']['rvv'])}`"


def write_summary(path: Path, metrics: list[dict[str, Any]], args: argparse.Namespace, manifest_path: Path, doctor_path: Path) -> None:
  lines = [
      "# transformation_estimation_dual_quaternion correspondence point-type layout summary",
      "",
      "## 运行合同",
      "",
      f"- run_label：`{args.run_label}`",
      f"- device：`{args.device_label}`",
      f"- case-filter：`{args.case_filter}`",
      "- evidence_role：`diagnostic`；只覆盖 correspondence-pair direct index stream 的代表性点型，不是 public-entry direct。",
      "- B/A：`Std direct index stream ms / RVV direct index stream ms`，大于 1 表示 RVV 构建更快。",
      f"- manifest：`{manifest_path}`",
      f"- Evidence Doctor：`{doctor_path}`",
      "",
      "## Point Type / Layout",
      "",
      "| point type | size | values | median | min | max | bucket | path fingerprint |",
      "| --- | --- | --- | ---: | ---: | ---: | --- | --- |",
  ]
  for item in metrics:
    m = item["same_boundary"]
    lines.append(
        f"| {item['point_type']} | {item['size_label']} | `{fmt(m['values'])}` | {m['median']:.3f} | {m['min']:.3f} | {m['max']:.3f} | `{m['bucket']}` | {checksum_note(item)} |"
    )
  lines.extend(
      [
          "",
          "## 决策边界",
          "",
          f"- overall_decision_bucket：`{overall(metrics)}`。",
          "- path fingerprint 若不同，只表示 bench 输出哈希不同；correctness 仍以 gtest 矩阵误差预算为准。",
          "- 本摘要不能证明 public-entry dispatch、混合 source/target 点型或真实 workload 分布。",
      ]
  )
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--input-dir", default="log/board/correspondence_point_type_layout_repeated")
  parser.add_argument("--output-dir", default="log/board/correspondence_point_type_layout_repeated")
  parser.add_argument("--run-label", default="correspondence_point_type_layout_repeated")
  parser.add_argument("--case-filter", default="correspondence-point-type-layout")
  parser.add_argument("--expected-runs", type=int, default=5)
  parser.add_argument("--std-bin", default="build/riscv/bench_transformation_estimation_dual_quaternion_std")
  parser.add_argument("--rvv-bin", default="build/riscv/bench_transformation_estimation_dual_quaternion_rvv")
  parser.add_argument("--device-label", default="RVV board")
  parser.add_argument("--vlen-desc", default="see SoC / ELF (board)")
  args = parser.parse_args()

  output_dir = Path(args.output_dir)
  summary_path = output_dir / "summary.md"
  manifest_path = output_dir / "evidence_manifest.json"
  doctor_path = output_dir / "evidence_doctor.md"
  runs = collect_runs(Path(args.input_dir), args.expected_runs)
  metrics = build_metrics(runs)
  manifest = build_manifest(runs, metrics, args, summary_path)
  output_dir.mkdir(parents=True, exist_ok=True)
  manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
  write_summary(summary_path, metrics, args, manifest_path, doctor_path)
  print(f"wrote summary: {summary_path}")
  print(f"wrote manifest: {manifest_path}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
