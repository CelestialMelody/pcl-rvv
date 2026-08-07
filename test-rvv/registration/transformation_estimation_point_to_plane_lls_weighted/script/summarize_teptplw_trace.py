#!/usr/bin/env python3
"""
Summarize TEPTPLW benchmark trace logs.

This script is intentionally different from analyze_teptplw_rvv_ba.py.  The B/A
analyzer needs paired baseline/candidate rows from the same RVV binary.  After a
candidate becomes the production default, that pair no longer exists, so this
script reports repeated-run timing stability for the remaining production rows.
"""

from __future__ import annotations

import argparse
import re
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]


ROW_RE = re.compile(
    r"^(?P<name>.+?)\s*:\s*(?P<avg>[0-9]+(?:\.[0-9]+)?)\s*ms\s*/\s*iter\s*$"
)
ITERATIONS_RE = re.compile(r"^\s*Iterations\s*:\s*(?P<iterations>\d+)\s*$")
WARMUP_RE = re.compile(
    r"^\s*Warmup\s+Iterations\s*:\s*(?P<warmup>\d+)\s*$", re.IGNORECASE
)
CHECKSUM_RE = re.compile(r"^\s*Checksum:\s*(?P<value>\S+)\s*$")
ITER_MIN_MED_MAX_RE = re.compile(
    r"^\s*Iteration Min/Median/Max:\s*"
    r"(?P<min>[0-9]+(?:\.[0-9]+)?)\s*/\s*"
    r"(?P<median>[0-9]+(?:\.[0-9]+)?)\s*/\s*"
    r"(?P<max>[0-9]+(?:\.[0-9]+)?)\s*ms\s*$"
)


@dataclass
class CaseTiming:
  avg_ms: float
  iter_min_ms: float | None = None
  iter_median_ms: float | None = None
  iter_max_ms: float | None = None


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


def parse_log(path: Path) -> tuple[dict[str, CaseTiming], dict[str, str], list[str]]:
  timings: dict[str, CaseTiming] = {}
  context: dict[str, str] = {}
  checksums: list[str] = []
  current_name: str | None = None

  for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    line = raw_line.rstrip("\n")
    if match := ITERATIONS_RE.match(line):
      context["iterations"] = match.group("iterations")
      continue
    if match := WARMUP_RE.match(line):
      context["warmup_iterations"] = match.group("warmup")
      continue
    if match := CHECKSUM_RE.match(line):
      checksums.append(match.group("value"))
      continue
    if match := ROW_RE.match(line):
      name = match.group("name").strip()
      timings[name] = CaseTiming(avg_ms=float(match.group("avg")))
      current_name = name
      continue
    if current_name and (match := ITER_MIN_MED_MAX_RE.match(line)):
      timing = timings[current_name]
      timing.iter_min_ms = float(match.group("min"))
      timing.iter_median_ms = float(match.group("median"))
      timing.iter_max_ms = float(match.group("max"))

  return timings, context, checksums


def format_values(values: list[float]) -> str:
  return ", ".join(f"{value:.6f}" for value in values)


def display_path(path: Path) -> str:
  resolved = path.resolve()
  try:
    return str(resolved.relative_to(REPO_ROOT))
  except ValueError:
    return str(path)


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Summarize repeated TEPTPLW benchmark trace logs."
  )
  parser.add_argument("logs", nargs="+", type=Path, help="Benchmark raw logs")
  parser.add_argument("--output", type=Path, default=None, help="Markdown output path")
  args = parser.parse_args()

  grouped: dict[str, list[CaseTiming]] = {}
  contexts: list[str] = []
  checksum_sequences: list[list[str]] = []
  for path in args.logs:
    timings, context, checksums = parse_log(path)
    context_bits = []
    if "iterations" in context:
      context_bits.append(f"iterations={context['iterations']}")
    if "warmup_iterations" in context:
      context_bits.append(f"warmup={context['warmup_iterations']}")
    suffix = f" ({', '.join(context_bits)})" if context_bits else ""
    contexts.append(f"{display_path(path)}{suffix}")
    checksum_sequences.append(checksums)
    for name, timing in timings.items():
      grouped.setdefault(name, []).append(timing)

  output_stream = sys.stdout
  should_close_output = False
  if args.output is not None:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    output_stream = args.output.open("w", encoding="utf-8")
    should_close_output = True

  try:
    print("# TEPTPLW Trace Summary", file=output_stream)
    print(file=output_stream)
    print("Input logs:", file=output_stream)
    for context in contexts:
      print(f"- {context}", file=output_stream)
    print(file=output_stream)

    all_present = all(checksum_sequences)
    identical = (
        all_present
        and all(sequence == checksum_sequences[0] for sequence in checksum_sequences[1:])
    )
    print("Checksum:", file=output_stream)
    print(f"- all runs contain checksum rows: `{'yes' if all_present else 'no'}`",
          file=output_stream)
    print(f"- checksum sequences identical: `{'yes' if identical else 'no'}`",
          file=output_stream)
    if checksum_sequences and checksum_sequences[0]:
      print(f"- final checksum: `{checksum_sequences[0][-1]}`", file=output_stream)
    print(file=output_stream)

    print(
        "| case | runs | avg median ms | avg min ms | avg max ms | avg p10 ms | "
        "avg p90 ms | iter-median median ms | iter max max ms | avg values ms |",
        file=output_stream,
    )
    print(
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
        file=output_stream,
    )
    for name in sorted(grouped):
      timings = grouped[name]
      averages = [timing.avg_ms for timing in timings]
      iter_medians = [
          timing.iter_median_ms
          for timing in timings
          if timing.iter_median_ms is not None
      ]
      iter_maxes = [
          timing.iter_max_ms for timing in timings if timing.iter_max_ms is not None
      ]
      iter_median_text = (
          f"{statistics.median(iter_medians):.6f}" if iter_medians else "n/a"
      )
      iter_max_text = f"{max(iter_maxes):.6f}" if iter_maxes else "n/a"
      print(
          f"| `{name}` | {len(timings)} | {statistics.median(averages):.6f} | "
          f"{min(averages):.6f} | {max(averages):.6f} | "
          f"{percentile(averages, 0.10):.6f} | {percentile(averages, 0.90):.6f} | "
          f"{iter_median_text} | {iter_max_text} | {format_values(averages)} |",
          file=output_stream,
      )
  finally:
    if should_close_output:
      output_stream.close()

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
