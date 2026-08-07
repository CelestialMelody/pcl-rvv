#!/usr/bin/env python3
"""
Collect repeated TEPTPLW RVV-vs-RVV board evidence.

The script deploys the topic RVV benchmark once, runs it repeatedly on the
configured board, stores raw logs under log/board, validates checksum
stability, generates the B/A reports, and optionally regenerates asm attribution.

It intentionally does not compare Std/RVV speedup. Candidate selection uses
baseline and candidate rows from the same RVV log.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
TOPIC_DIR = SCRIPT_PATH.parents[1]
REPO_ROOT = SCRIPT_PATH.parents[4]
ANALYZE_SCRIPT = SCRIPT_PATH.with_name("analyze_teptplw_rvv_ba.py")
ASM_SCRIPT = SCRIPT_PATH.with_name("generate_teptplw_asm_attribution.py")
TRACE_SUMMARY_SCRIPT = SCRIPT_PATH.with_name("summarize_teptplw_trace.py")
ASM_PATH = (
    TOPIC_DIR
    / "build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_weighted_rvv.full.asm"
)
CHECKSUM_RE = re.compile(r"^\s*Checksum:\s*(?P<value>\S+)\s*$")


def run(
    command: list[str],
    *,
    cwd: Path = REPO_ROOT,
    capture: bool = False,
    dry_run: bool = False,
) -> subprocess.CompletedProcess[str]:
  print("+ " + shlex.join(command))
  if dry_run:
    return subprocess.CompletedProcess(command, 0, "", "")
  return subprocess.run(
      command,
      cwd=cwd,
      check=True,
      text=True,
      stdout=subprocess.PIPE if capture else None,
      stderr=subprocess.STDOUT if capture else None,
  )


def read_make_variables(names: list[str], dry_run: bool) -> dict[str, str]:
  makefile = ["include Makefile", ".PHONY: print-teptplw-vars", "print-teptplw-vars:"]
  for name in names:
    makefile.append(f"\t@printf '%s=%s\\n' '{name}' '$({name})'")
  command = [
      "make",
      "--no-print-directory",
      "-s",
      "-C",
      str(TOPIC_DIR),
      "-f",
      "-",
      "print-teptplw-vars",
  ]
  print("+ " + shlex.join(command))
  if dry_run:
    return {
        "REMOTE_USER": "<REMOTE_USER>",
        "REMOTE_IP": "<REMOTE_IP>",
        "REMOTE_DIR": "/root/pcl-test/registration/"
        "transformation_estimation_point_to_plane_lls_weighted",
        "TARGET_BENCH_RVV": "bench_transformation_estimation_point_to_plane_lls_weighted_rvv",
        "SSH_CMD": "ssh",
    }
  completed = subprocess.run(
      command,
      cwd=REPO_ROOT,
      input="\n".join(makefile) + "\n",
      check=True,
      text=True,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )
  values: dict[str, str] = {}
  for line in completed.stdout.splitlines():
    name, separator, value = line.partition("=")
    if separator:
      values[name] = value
  missing = [name for name in names if not values.get(name)]
  if missing:
    raise SystemExit("missing board configuration: " + ", ".join(missing))
  return values


def remote_command(
    ssh_command: str,
    remote: str,
    shell_command: str,
    *,
    output: Path | None,
    dry_run: bool,
) -> None:
  command = [*shlex.split(ssh_command), remote, shell_command]
  print("+ " + shlex.join(command))
  if output is not None:
    print(f"  -> {output}")
  if dry_run:
    return
  if output is None:
    subprocess.run(command, cwd=REPO_ROOT, check=True, text=True)
    return
  output.parent.mkdir(parents=True, exist_ok=True)
  with output.open("w", encoding="utf-8") as stream:
    subprocess.run(
        command,
        cwd=REPO_ROOT,
        check=True,
        text=True,
        stdout=stream,
        stderr=subprocess.STDOUT,
    )


def environment_probe() -> str:
  governor = (
      "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
  )
  frequency = (
      "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"
  )
  cpuinfo_frequency = (
      "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq"
  )
  return (
      "date; uname -a; "
      f"if test -r {shlex.quote(governor)}; then cat {shlex.quote(governor)}; fi; "
      f"if test -r {shlex.quote(frequency)}; then cat {shlex.quote(frequency)}; fi"
      f"; if test -r {shlex.quote(cpuinfo_frequency)}; then "
      f"cat {shlex.quote(cpuinfo_frequency)}; fi; "
      "for t in /sys/class/thermal/thermal_zone*/temp; do "
      "if test -r \"$t\"; then "
      "zone=$(basename \"$(dirname \"$t\")\"); "
      "echo \"${zone}=$(cat \"$t\")\"; "
      "fi; "
      "done"
  )


def checksum_values(path: Path) -> list[str]:
  values: list[str] = []
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    if match := CHECKSUM_RE.match(line):
      values.append(match.group("value"))
  return values


def write_checksum_report(logs: list[Path], output: Path) -> None:
  sequences = [checksum_values(path) for path in logs]
  lines = ["# checksum validation", ""]
  for path, values in zip(logs, sequences):
    final = values[-1] if values else "<missing>"
    lines.append(
        f"- `{path.name}`: checksum_rows={len(values)}, final_checksum={final}"
    )
  all_present = all(sequences)
  identical = all_present and all(sequence == sequences[0] for sequence in sequences[1:])
  lines.extend(
      [
          "",
          f"- all runs contain checksum rows: `{'yes' if all_present else 'no'}`",
          f"- checksum sequences identical: `{'yes' if identical else 'no'}`",
          "",
      ]
  )
  output.write_text("\n".join(lines), encoding="utf-8")
  if not all_present or not identical:
    raise SystemExit(f"checksum validation failed; see {output}")


def default_output_dir(case_filter: str, runs: int) -> Path:
  label = re.sub(r"[^a-zA-Z0-9]+", "_", case_filter).strip("_")
  return TOPIC_DIR / "log/board" / label


def resolve_asm_profile(case_filter: str, requested: str) -> str:
  if requested != "auto":
    return requested
  if case_filter == "production-default-fused-abcd-ilp":
    return "production-default-fused-abcd-ilp"
  if case_filter in ("generic-fused-abc", "generic-fused-formula"):
    return "generic-fused-formula"
  return "none"


def default_analysis_filter(case_filter: str) -> str:
  if case_filter.startswith("production-symbol"):
    return "production-symbol"
  if case_filter.startswith("production-shaped"):
    return "production-shaped"
  return ""


def git_context() -> dict[str, object]:
  commit = subprocess.run(
      ["git", "rev-parse", "HEAD"],
      cwd=REPO_ROOT,
      check=True,
      text=True,
      stdout=subprocess.PIPE,
  ).stdout.strip()
  status = subprocess.run(
      ["git", "status", "--short"],
      cwd=REPO_ROOT,
      check=True,
      text=True,
      stdout=subprocess.PIPE,
  ).stdout.splitlines()
  return {"commit": commit, "dirty": bool(status), "status": status}


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Collect repeated board RVV-vs-RVV evidence for weighted TEPTPLW."
  )
  parser.add_argument("--runs", type=int, default=5)
  parser.add_argument(
      "--case-filter",
      default="production-default-fused-abcd-ilp",
  )
  parser.add_argument("--size", default="262144")
  parser.add_argument("--iterations", type=int, default=20)
  parser.add_argument("--warmup-iterations", type=int, default=5)
  parser.add_argument("--cpu", type=int, default=0)
  parser.add_argument("--output-dir", type=Path, default=None)
  parser.add_argument("--analysis-filter", default=None)
  parser.add_argument("--below-threshold", type=float, default=1.0)
  parser.add_argument("--skip-deploy", action="store_true")
  parser.add_argument("--skip-asm", action="store_true")
  parser.add_argument(
      "--asm-profile",
      choices=(
          "auto",
          "none",
          "production-default-fused-abcd-ilp",
          "generic-fused-formula",
      ),
      default="auto",
  )
  parser.add_argument("--remote-lib-dir", default="/root/pcl-test/lib")
  parser.add_argument("--dry-run", action="store_true")
  args = parser.parse_args()

  if args.runs <= 0:
    parser.error("--runs must be positive")
  if args.iterations <= 0:
    parser.error("--iterations must be positive")
  if args.warmup_iterations < 0:
    parser.error("--warmup-iterations must be non-negative")
  if args.cpu < 0:
    parser.error("--cpu must be non-negative")

  output_dir = (
      args.output_dir.resolve()
      if args.output_dir is not None
      else default_output_dir(args.case_filter, args.runs)
  )
  if output_dir.exists() and any(output_dir.iterdir()):
    raise SystemExit(f"output directory is not empty: {output_dir}")
  if not args.dry_run:
    output_dir.mkdir(parents=True, exist_ok=True)

  variables = read_make_variables(
      ["REMOTE_USER", "REMOTE_IP", "REMOTE_DIR", "TARGET_BENCH_RVV", "SSH_CMD"],
      args.dry_run,
  )
  remote = f"{variables['REMOTE_USER']}@{variables['REMOTE_IP']}"
  bench_args = [
      "--case-filter",
      args.case_filter,
      "--size",
      args.size,
      "--iterations",
      str(args.iterations),
      "--warmup-iterations",
      str(args.warmup_iterations),
  ]
  remote_bench = (
      f"cd {shlex.quote(variables['REMOTE_DIR'])} && "
      f"LD_LIBRARY_PATH={shlex.quote(args.remote_lib_dir)}:$LD_LIBRARY_PATH "
      f"taskset -c {args.cpu} "
      f"./{shlex.quote(variables['TARGET_BENCH_RVV'])} "
      + shlex.join(bench_args)
  )

  manifest: dict[str, object] = {
      "created_at": dt.datetime.now(dt.timezone.utc).astimezone().isoformat(),
      "topic": str(TOPIC_DIR.relative_to(REPO_ROOT)),
      "output_dir": str(output_dir.relative_to(REPO_ROOT)),
      "runs": args.runs,
      "case_filter": args.case_filter,
      "size": args.size,
      "iterations": args.iterations,
      "warmup_iterations": args.warmup_iterations,
      "cpu": args.cpu,
      "board": {
          "remote": remote,
          "remote_dir": variables["REMOTE_DIR"],
          "bench": variables["TARGET_BENCH_RVV"],
      },
      "git": None if args.dry_run else git_context(),
  }
  if not args.dry_run:
    (output_dir / "collection_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

  if not args.skip_deploy:
    run(
        ["make", "-C", str(TOPIC_DIR), "deploy_bench_rvv"],
        dry_run=args.dry_run,
    )

  remote_command(
      variables["SSH_CMD"],
      remote,
      environment_probe(),
      output=None if args.dry_run else output_dir / "board_env_before.log",
      dry_run=args.dry_run,
  )

  logs: list[Path] = []
  for index in range(1, args.runs + 1):
    log = output_dir / f"run{index:02d}_rvv.log"
    logs.append(log)
    remote_command(
        variables["SSH_CMD"],
        remote,
        remote_bench,
        output=None if args.dry_run else log,
        dry_run=args.dry_run,
    )

  remote_command(
      variables["SSH_CMD"],
      remote,
      environment_probe(),
      output=None if args.dry_run else output_dir / "board_env_after.log",
      dry_run=args.dry_run,
  )

  if args.dry_run:
    print(f"[dry-run] evidence directory: {output_dir}")
    return 0

  write_checksum_report(logs, output_dir / "checksum_validation.md")
  analysis_filter = (
      args.analysis_filter
      if args.analysis_filter is not None
      else default_analysis_filter(args.case_filter)
  )
  analyze_command = [
      sys.executable,
      str(ANALYZE_SCRIPT),
      *[str(path) for path in logs],
      "--output",
      str(output_dir / "analyze_rvv_ba.md"),
      "--below-threshold-report",
      str(output_dir / "ba_below_1_frequency.txt"),
      "--below-threshold",
      str(args.below_threshold),
  ]
  if analysis_filter:
    analyze_command.extend(["--case-filter", analysis_filter])
  run(analyze_command)

  run(
      [
          sys.executable,
          str(TRACE_SUMMARY_SCRIPT),
          *[str(path) for path in logs],
          "--output",
          str(output_dir / "trace_summary.md"),
      ]
  )

  asm_profile = resolve_asm_profile(args.case_filter, args.asm_profile)
  if not args.skip_asm and asm_profile != "none":
    run(["make", "-C", str(TOPIC_DIR), "dump_bench_rvv"])
    asm_output = (
        "asm_production_symbol_attribution.md"
        if asm_profile == "production-default-fused-abcd-ilp"
        else "asm_generic_fused_formula_attribution.md"
    )
    run(
        [
            sys.executable,
            str(ASM_SCRIPT),
            "--profile",
            asm_profile,
            "--asm",
            str(ASM_PATH),
            "--output",
            str(output_dir / asm_output),
        ]
    )

  print(f"[done] evidence directory: {output_dir}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
