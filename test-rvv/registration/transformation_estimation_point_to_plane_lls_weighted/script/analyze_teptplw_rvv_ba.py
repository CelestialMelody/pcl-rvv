#!/usr/bin/env python3
"""
Report RVV-vs-RVV B/A for weighted TEPTPLW fused-formula logs.

The Std/RVV compare table answers a different question: how much a single case
speeds up when compiled with RVV. This script compares candidates inside the
same RVV log:

  B/A = block-baseline_rvv_ms / block-fused-candidate_rvv_ms

Values greater than 1.00x mean the fused candidate is faster than the matching
RVV block baseline.
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path


ROW_RE = re.compile(
    r"^(?P<name>.+?)\s*:\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*/\s*iter\s*$"
)
BUILD_RE = re.compile(r"^\s*Build\s*:\s*(?P<build>\S+)\s*$")
ITERATIONS_RE = re.compile(r"^\s*Iterations\s*:\s*(?P<iterations>\d+)\s*$")
WARMUP_RE = re.compile(
    r"^\s*Warmup\s+Iterations\s*:\s*(?P<warmup>\d+)\s*$", re.IGNORECASE
)
ITER_MIN_MED_MAX_RE = re.compile(
    r"^\s*Iteration Min/Median/Max:\s*"
    r"(?P<min>[0-9]+(?:\.[0-9]+)?)\s*/\s*"
    r"(?P<median>[0-9]+(?:\.[0-9]+)?)\s*/\s*"
    r"(?P<max>[0-9]+(?:\.[0-9]+)?)\s*ms\s*$"
)

CANDIDATES = (
    "d-displacement-ilp",
    "d-displacement",
    "d-six-term-ilp",
    "d-six-term",
    "abc-ilp",
    "abcd-ilp",
    "abcd",
    "abc",
)


@dataclass
class CaseTiming:
  avg_ms: float
  iter_min_ms: float | None = None
  iter_median_ms: float | None = None
  iter_max_ms: float | None = None


@dataclass(frozen=True)
class PairKey:
  layer: str
  point_type: str
  size: str
  candidate: str


@dataclass
class PairValue:
  ba_avg: float
  ba_iter_median: float | None
  baseline_ms: float
  candidate_ms: float
  baseline_iter_median_ms: float | None
  candidate_iter_median_ms: float | None
  log: Path


def percentile(values: list[float], pct: float) -> float:
  ordered = sorted(values)
  if not ordered:
    raise ValueError("percentile() requires at least one value")
  if len(ordered) == 1:
    return ordered[0]
  position = (len(ordered) - 1) * pct
  lower = int(position)
  upper = min(lower + 1, len(ordered) - 1)
  fraction = position - lower
  return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def parse_log(path: Path) -> tuple[dict[str, CaseTiming], dict[str, str]]:
  timings: dict[str, CaseTiming] = {}
  context: dict[str, str] = {}
  active_build: str | None = None
  current_name: str | None = None

  for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    line = raw_line.rstrip("\n")
    if match := ITERATIONS_RE.match(line):
      context["iterations"] = match.group("iterations")
      continue
    if match := WARMUP_RE.match(line):
      context["warmup_iterations"] = match.group("warmup")
      continue
    if match := BUILD_RE.match(line):
      active_build = match.group("build")
      current_name = None
      continue

    if match := ROW_RE.match(line):
      current_name = None
      if active_build is not None and active_build != "rvv":
        continue
      name = match.group("name").strip()
      timings[name] = CaseTiming(avg_ms=float(match.group("avg")))
      current_name = name
      continue

    if current_name and (match := ITER_MIN_MED_MAX_RE.match(line)):
      timing = timings[current_name]
      timing.iter_min_ms = float(match.group("min"))
      timing.iter_median_ms = float(match.group("median"))
      timing.iter_max_ms = float(match.group("max"))

  return timings, context


def split_candidate(name: str) -> tuple[str, str] | None:
  for candidate in CANDIDATES:
    token = f"block-fused-{candidate} "
    if token in name:
      prefix, rest = name.split(token, 1)
      return candidate, prefix + rest
  return None


def pair_for_candidate(name: str) -> tuple[PairKey, str] | None:
  split = split_candidate(name)
  if not split:
    return None
  candidate, unfused_name = split

  prefix = "weighted lls component full-cloud "
  suffix_token = " no-solve "
  if name.startswith(prefix) and suffix_token in unfused_name:
    rest = unfused_name[len(prefix) :]
    point_type, size = rest.rsplit(suffix_token, 1)
    baseline = f"{prefix}block-baseline {point_type} no-solve {size}"
    return PairKey("component no-solve", point_type, size, candidate), baseline

  prefix = "weighted lls production-source-indices-detail component "
  suffix_token = " no-solve "
  if name.startswith(prefix) and suffix_token in unfused_name:
    rest = unfused_name[len(prefix) :]
    point_type, size = rest.rsplit(suffix_token, 1)
    baseline = f"{prefix}staged-gather {point_type} no-solve {size}"
    return (
        PairKey("production-source-indices-detail component", point_type, size, candidate),
        baseline,
    )

  prefix = "weighted lls production-source-indices-detail "
  if name.startswith(prefix):
    rest = unfused_name[len(prefix) :]
    point_type, size = rest.rsplit(" ", 1)
    baseline = f"{prefix}staged-gather {point_type} {size}"
    return (
        PairKey("production-source-indices-detail full", point_type, size, candidate),
        baseline,
    )

  prefix = "weighted lls production-shaped full-cloud "
  if name.startswith(prefix):
    rest = unfused_name[len(prefix) :]
    point_type, size = rest.rsplit(" ", 1)
    baseline = f"{prefix}block-baseline {point_type} {size}"
    return PairKey("production-shaped", point_type, size, candidate), baseline

  prefix = "weighted lls production-symbol full-cloud "
  if name.startswith(prefix):
    rest = unfused_name[len(prefix) :]
    point_type, size = rest.rsplit(" ", 1)
    baseline = f"{prefix}block-baseline {point_type} {size}"
    return PairKey("production-symbol", point_type, size, candidate), baseline

  prefix = "weighted lls full-cloud "
  if name.startswith(prefix):
    rest = unfused_name[len(prefix) :]
    point_type, size = rest.rsplit(" ", 1)
    baseline = f"{prefix}block-reduction {point_type} {size}"
    return PairKey("full estimate diagnostic", point_type, size, candidate), baseline

  return None


def collect_pairs(path: Path) -> tuple[list[tuple[PairKey, PairValue]], dict[str, str]]:
  timings, context = parse_log(path)
  pairs: list[tuple[PairKey, PairValue]] = []
  for name, candidate_timing in timings.items():
    paired = pair_for_candidate(name)
    if not paired:
      continue
    key, baseline_name = paired
    baseline_timing = timings.get(baseline_name)
    if baseline_timing is None or candidate_timing.avg_ms <= 0.0:
      continue
    ba_iter_median = None
    if (
        baseline_timing.iter_median_ms is not None
        and candidate_timing.iter_median_ms is not None
        and candidate_timing.iter_median_ms > 0.0
    ):
      ba_iter_median = baseline_timing.iter_median_ms / candidate_timing.iter_median_ms
    pairs.append(
        (
            key,
            PairValue(
                ba_avg=baseline_timing.avg_ms / candidate_timing.avg_ms,
                ba_iter_median=ba_iter_median,
                baseline_ms=baseline_timing.avg_ms,
                candidate_ms=candidate_timing.avg_ms,
                baseline_iter_median_ms=baseline_timing.iter_median_ms,
                candidate_iter_median_ms=candidate_timing.iter_median_ms,
                log=path,
            ),
        )
    )
  return pairs, context


def format_below_threshold_report(
    grouped: dict[PairKey, list[PairValue]], threshold: float
) -> str:
  lines: list[str] = []
  for key in sorted(grouped, key=lambda item: (item.layer, item.point_type, int(item.size), item.candidate)):
    values = grouped[key]
    avg_below = [value for value in values if value.ba_avg < threshold]
    iter_values = [value for value in values if value.ba_iter_median is not None]
    iter_below = [
        value for value in values if value.ba_iter_median is not None and value.ba_iter_median < threshold
    ]
    lines.append(
        f"{key.point_type}: avg_below_{threshold:.1f}={len(avg_below)}/{len(values)} "
        f"median_below_{threshold:.1f}={len(iter_below)}/{len(iter_values)}"
    )
    lines.append(
        "avg_values="
        + ", ".join(f"{value.log.name}:{value.ba_avg:.3f}x" for value in values)
    )
    if iter_values:
      lines.append(
          "median_values="
          + ", ".join(
              f"{value.log.name}:{value.ba_iter_median:.3f}x"
              for value in values
              if value.ba_iter_median is not None
          )
      )
    if avg_below:
      lines.append(
          "avg_below_runs="
          + ", ".join(
              f"{value.log.name}:{value.ba_avg:.3f}x baseline={value.baseline_ms:.6f} "
              f"fused={value.candidate_ms:.6f}"
              for value in avg_below
          )
      )
    if iter_below:
      lines.append(
          "median_below_runs="
          + ", ".join(
              f"{value.log.name}:{value.ba_iter_median:.3f}x baseline_med={value.baseline_iter_median_ms:.6f} "
              f"fused_med={value.candidate_iter_median_ms:.6f}"
              for value in iter_below
          )
      )
    lines.append("")
  return "\n".join(lines).rstrip() + "\n"


def format_values(values: list[float]) -> str:
  return ", ".join(f"{value:.3f}x" for value in values)


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Analyze RVV-vs-RVV B/A for weighted TEPTPLW fused candidates."
  )
  parser.add_argument("logs", nargs="+", type=Path, help="RVV raw logs or combined stdout logs")
  parser.add_argument(
      "--case-filter",
      default="",
      help="Only include rows whose point type/layer/candidate contains this text.",
  )
  parser.add_argument(
      "--output",
      type=Path,
      default=None,
      help="Optional Markdown output path. Defaults to stdout.",
  )
  parser.add_argument(
      "--below-threshold-report",
      type=Path,
      default=None,
      help="Optional path for a compact frequency report of runs below the threshold.",
  )
  parser.add_argument(
      "--below-threshold",
      type=float,
      default=1.0,
      help="Threshold used by --below-threshold-report.",
  )
  args = parser.parse_args()

  grouped: dict[PairKey, list[PairValue]] = {}
  contexts: list[str] = []
  for path in args.logs:
    pairs, context = collect_pairs(path)
    context_bits = []
    if "iterations" in context:
      context_bits.append(f"iterations={context['iterations']}")
    if "warmup_iterations" in context:
      context_bits.append(f"warmup={context['warmup_iterations']}")
    suffix = f" ({', '.join(context_bits)})" if context_bits else ""
    contexts.append(f"{path}{suffix}")
    for key, value in pairs:
      haystack = f"{key.layer} {key.point_type} {key.size} {key.candidate}"
      if args.case_filter and args.case_filter not in haystack:
        continue
      grouped.setdefault(key, []).append(value)

  output_stream = sys.stdout
  should_close_output = False
  if args.output is not None:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    output_stream = args.output.open("w", encoding="utf-8")
    should_close_output = True

  try:
    print("# TEPTPLW RVV-vs-RVV B/A Summary", file=output_stream)
    print(file=output_stream)
    print("Input logs:", file=output_stream)
    for context in contexts:
      print(f"- {context}", file=output_stream)
    print(file=output_stream)
    print(
        "| layer | point type | size | candidate | runs | avg B/A median | "
        "avg B/A min | avg B/A max | avg B/A p10 | avg B/A p90 | values | "
        "iter-median B/A median |",
        file=output_stream,
    )
    print(
        "| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: |",
        file=output_stream,
    )
    for key in sorted(
        grouped,
        key=lambda item: (
            item.layer,
            item.point_type,
            int(item.size),
            item.candidate,
        ),
    ):
      values = grouped[key]
      ba_values = [value.ba_avg for value in values]
      iter_values = [
          value.ba_iter_median
          for value in values
          if value.ba_iter_median is not None
      ]
      iter_median = statistics.median(iter_values) if iter_values else None
      iter_median_text = (
          f"{iter_median:.3f}x" if iter_median is not None else "n/a"
      )
      print(
          f"| {key.layer} | `{key.point_type}` | {key.size} | `{key.candidate}` | "
          f"{len(values)} | {statistics.median(ba_values):.3f}x | "
          f"{min(ba_values):.3f}x | {max(ba_values):.3f}x | "
          f"{percentile(ba_values, 0.10):.3f}x | "
          f"{percentile(ba_values, 0.90):.3f}x | "
          f"{format_values(ba_values)} | {iter_median_text} |",
          file=output_stream,
      )
  finally:
    if should_close_output:
      output_stream.close()

  if args.below_threshold_report is not None:
    args.below_threshold_report.parent.mkdir(parents=True, exist_ok=True)
    args.below_threshold_report.write_text(
        format_below_threshold_report(grouped, args.below_threshold), encoding="utf-8"
    )
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
