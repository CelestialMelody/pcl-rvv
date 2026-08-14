#!/usr/bin/env python3
"""Collect repeated Std/RVV board compare evidence for ICP transformCloud."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
TOPIC_DIR = SCRIPT_PATH.parents[1]
REPO_ROOT = SCRIPT_PATH.parents[4]
REPEATED_ANALYZER = REPO_ROOT / "test-rvv/script/analyze_bench_repeated.py"


def run(command: list[str], *, cwd: Path = REPO_ROOT, capture: bool = False) -> str:
  print("+ " + shlex.join(command))
  result = subprocess.run(
      command,
      cwd=cwd,
      check=True,
      text=True,
      stdout=subprocess.PIPE if capture else None,
      stderr=subprocess.STDOUT if capture else None,
  )
  return result.stdout if capture and result.stdout is not None else ""


def make_var(name: str) -> str:
  value = run(["make", "--no-print-directory", "-C", str(TOPIC_DIR), f"print-{name}"], capture=True)
  return value.strip()


def extract_table(analyzer_output: str) -> str:
  marker = "| Benchmark Item |"
  start = analyzer_output.find(marker)
  if start < 0:
    raise RuntimeError("repeated analyzer did not produce a benchmark table")
  return analyzer_output[start:].strip() + "\n"


def sha256_file(path: Path) -> str:
  digest = hashlib.sha256()
  with path.open("rb") as handle:
    for chunk in iter(lambda: handle.read(1024 * 1024), b""):
      digest.update(chunk)
  return "sha256:" + digest.hexdigest()


def local_binary_hashes() -> str:
  entries: list[str] = []
  for label, rel in (
      ("std_local_unstripped", "build/riscv/bench_icp_std"),
      ("rvv_local_unstripped", "build/riscv/bench_icp_rvv"),
  ):
    path = TOPIC_DIR / rel
    if path.is_file():
      entries.append(f"{label}={sha256_file(path)}")
  return "; ".join(entries) if entries else "not_recorded"


def optional_remote_capture(command: str) -> str:
  try:
    ssh_cmd = shlex.split(make_var("SSH_CMD"))
    remote = f"{make_var('REMOTE_USER')}@{make_var('REMOTE_IP')}"
    return run([*ssh_cmd, remote, command], capture=True).strip()
  except (subprocess.CalledProcessError, ValueError):
    return ""


def remote_binary_hashes() -> str:
  remote_dir = make_var("REMOTE_DIR")
  command = (
      "sha256sum "
      f"{shlex.quote(remote_dir + '/bench_icp_std')} "
      f"{shlex.quote(remote_dir + '/bench_icp_rvv')} 2>/dev/null || true"
  )
  output = optional_remote_capture(command)
  entries: list[str] = []
  for line in output.splitlines():
    parts = line.split()
    if len(parts) >= 2 and re.fullmatch(r"[0-9a-fA-F]{64}", parts[0]):
      label = Path(parts[1]).name
      entries.append(f"{label}=sha256:{parts[0].lower()}")
  return "; ".join(entries)


def board_metadata() -> dict[str, str]:
  output = optional_remote_capture(
      "printf 'uname='; uname -a; "
      "printf 'governor='; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true; "
      "printf 'freq='; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || true; "
      "printf 'temperature='; cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || true"
  )
  result: dict[str, str] = {}
  for line in output.splitlines():
    if "=" in line:
      key, value = line.split("=", 1)
      result[key.strip()] = value.strip() or "not_recorded"
  return result


def rerun_command_lines(runs: int) -> list[str]:
  return [
      "make -C test-rvv/registration/icp collect_board_transform_cloud_repeated \\",
      f"  ICP_BOARD_RUNS={runs}",
  ]


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--runs", type=int, default=5)
  parser.add_argument("--iterations", type=int, default=20)
  parser.add_argument("--warmup-iterations", type=int, default=3)
  parser.add_argument(
      "--output-dir",
      type=Path,
      default=TOPIC_DIR / "log/board/transform_cloud_repeated",
  )
  parser.add_argument(
      "--raw-dir",
      type=Path,
      default=None,
      help="Keep per-run board logs here; default uses a temporary local-only directory.",
  )
  parser.add_argument("--dry-run", action="store_true")
  args = parser.parse_args()

  if args.runs <= 0 or args.iterations <= 0 or args.warmup_iterations < 0:
    parser.error("runs and iterations must be positive; warmup-iterations cannot be negative")

  output_dir = args.output_dir.resolve()
  summary_path = output_dir / "summary.md"
  output_dir.mkdir(parents=True, exist_ok=True)

  temporary_raw: tempfile.TemporaryDirectory[str] | None = None
  if args.raw_dir is None:
    temporary_raw = tempfile.TemporaryDirectory(prefix="icp_board_compare_")
    raw_dir = Path(temporary_raw.name)
  else:
    raw_dir = args.raw_dir.resolve()
    raw_dir.mkdir(parents=True, exist_ok=True)

  try:
    raw_logs: list[Path] = []
    for run_index in range(1, args.runs + 1):
      run_dir = raw_dir / f"run{run_index:02d}"
      command = [
          "make",
          "-C",
          str(TOPIC_DIR),
          "run_board_bench_compare",
          "fetch_board_logs",
          f"OUTPUT_DIR_BOARD={run_dir}",
      ]
      if args.dry_run:
        print("+ " + shlex.join(command))
        continue
      run(command)
      source_log = run_dir / "analyze_bench_compare.log"
      if not source_log.is_file():
        raise RuntimeError(f"missing compare log after run {run_index}: {source_log}")
      raw_logs.append(source_log)

    if args.dry_run:
      return 0

    analyzed = run(
        [sys.executable, str(REPEATED_ANALYZER), *map(str, raw_logs)],
        capture=True,
    )
    table = extract_table(analyzed)
    metadata = board_metadata()
    remote_hashes = remote_binary_hashes()
    binary_hash = remote_hashes or local_binary_hashes()
    raw_boundary = (
        f"- raw compare logs：`{raw_dir}`；local-only diagnostic inputs，默认不进入提交边界。"
        if args.raw_dir is not None
        else "- raw compare logs：默认使用临时目录，脚本结束后清理，不进入提交边界。"
    )
    summary = "\n".join(
        [
            "# ICP transformCloud 重复板卡 Benchmark 摘要",
            "",
            "运行上下文：",
            f"- device：`{make_var('BOARD_LABEL') or 'board'}`",
            "- evidence role：`production_direct`",
            "- case filter：`transform-cloud`",
            f"- runs：`{args.runs}`",
            f"- iterations：`{args.iterations}`",
            f"- warm-up iterations：`{args.warmup_iterations}`",
            "- taskset：`not_pinned`",
            f"- governor：`{metadata.get('governor', 'not_recorded')}`",
            f"- freq：`{metadata.get('freq', 'not_recorded')}`",
            f"- temperature：`{metadata.get('temperature', 'not_recorded')}`",
            f"- binary hash：`{binary_hash}`",
            raw_boundary,
            "",
            "使用 topic-local target 重新生成：",
            "",
            "```bash",
            *rerun_command_lines(args.runs),
            "```",
            "",
            table.rstrip(),
            "",
        ]
    )
    summary_path.write_text(summary, encoding="utf-8")
    print(f"[done] summary: {summary_path}")
    return 0
  finally:
    if temporary_raw is not None:
      temporary_raw.cleanup()


if __name__ == "__main__":
  raise SystemExit(main())
