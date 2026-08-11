#!/usr/bin/env python3
"""Collect and summarize repeated Std/RVV board comparisons for this topic."""

from __future__ import annotations

import argparse
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


def run(command: list[str]) -> None:
    print("+ " + shlex.join(command))
    subprocess.run(command, cwd=REPO_ROOT, check=True)


def extract_table(analyzer_output: str) -> str:
    marker = "| Benchmark Item |"
    start = analyzer_output.find(marker)
    if start < 0:
        raise RuntimeError("repeated analyzer did not produce a benchmark table")
    return analyzer_output[start:].strip() + "\n"


def rerun_command_lines(case_filter: str) -> list[str]:
    if case_filter == "dual-correspondence-family":
        return [
            "make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \\",
            "  collect_board_production_dispatch_repeated \\",
            "  TEPTPLW_COMPARE_CASE_FILTER=dual-correspondence-family \\",
            "  TEPTPLW_COMPARE_SIZE=<size> \\",
            "  TEPTPLW_COMPARE_RUNS=<runs> \\",
            "  TEPTPLW_COMPARE_ITERATIONS=<iterations> \\",
            "  TEPTPLW_COMPARE_WARMUP_ITERATIONS=<warmup-iterations>",
        ]
    if case_filter == "source-indexed-family":
        return [
            "make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \\",
            "  collect_board_source_indexed_family_repeated \\",
            "  TEPTPLW_SOURCE_INDEXED_FAMILY_SIZE=<size> \\",
            "  TEPTPLW_SOURCE_INDEXED_FAMILY_RUNS=<runs> \\",
            "  TEPTPLW_SOURCE_INDEXED_FAMILY_ITERATIONS=<iterations> \\",
            "  TEPTPLW_SOURCE_INDEXED_FAMILY_WARMUP_ITERATIONS=<warmup-iterations>",
        ]
    if case_filter == "production-source-indices":
        return [
            "make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \\",
            "  collect_board_production_source_indices_repeated \\",
            "  TEPTPLW_SOURCE_INDICES_SIZE=<size> \\",
            "  TEPTPLW_SOURCE_INDICES_RUNS=<runs> \\",
            "  TEPTPLW_SOURCE_INDICES_ITERATIONS=<iterations> \\",
            "  TEPTPLW_SOURCE_INDICES_WARMUP_ITERATIONS=<warmup-iterations>",
        ]
    if case_filter == "row-sources":
        return [
            "make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \\",
            "  collect_board_row_sources_repeated \\",
            "  TEPTPLW_ROW_SOURCE_SIZE=<size> \\",
            "  TEPTPLW_ROW_SOURCE_RUNS=<runs> \\",
            "  TEPTPLW_ROW_SOURCE_ITERATIONS=<iterations> \\",
            "  TEPTPLW_ROW_SOURCE_WARMUP_ITERATIONS=<warmup-iterations>",
        ]
    return [
        "make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted \\",
        "  collect_board_production_dispatch_repeated \\",
        "  TEPTPLW_COMPARE_CASE_FILTER=<case-filter> \\",
        "  TEPTPLW_COMPARE_SIZE=<size> \\",
        "  TEPTPLW_COMPARE_RUNS=<runs> \\",
        "  TEPTPLW_COMPARE_ITERATIONS=<iterations> \\",
        "  TEPTPLW_COMPARE_WARMUP_ITERATIONS=<warmup-iterations>",
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Collect repeated Std/RVV board comparisons for TEPTPLW."
    )
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--case-filter", default="production-dispatch")
    parser.add_argument("--size", default="262144")
    parser.add_argument("--iterations", type=int, default=20)
    parser.add_argument("--warmup-iterations", type=int, default=5)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=TOPIC_DIR / "log/board/production_dispatch_fused_abcd_ilp",
    )
    parser.add_argument(
        "--raw-dir",
        type=Path,
        default=None,
        help="Keep per-run compare logs here; default is a temporary directory.",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if args.runs <= 0 or args.iterations <= 0 or args.warmup_iterations < 0:
        parser.error("runs and iterations must be positive; warmup-iterations cannot be negative")

    output_dir = args.output_dir.resolve()
    summary_path = output_dir / "summary.md"
    output_dir.mkdir(parents=True, exist_ok=True)

    temporary_raw = None
    if args.raw_dir is None:
        temporary_raw = tempfile.TemporaryDirectory(
            prefix="teptplw_production_dispatch_compare_"
        )
        raw_dir = Path(temporary_raw.name)
    else:
        raw_dir = args.raw_dir.resolve()
        raw_dir.mkdir(parents=True, exist_ok=True)

    try:
        raw_logs: list[Path] = []
        for run_index in range(1, args.runs + 1):
            bench_args = (
                f"--case-filter {args.case_filter} "
                f"--size {args.size} "
                f"--iterations {args.iterations} "
                f"--warmup-iterations {args.warmup_iterations}"
            )
            command = [
                "make",
                "-C",
                str(TOPIC_DIR),
                "run_board_bench_compare",
                "fetch_board_logs",
                f"BENCH_ARGS={bench_args}",
            ]
            if args.dry_run:
                print("+ " + shlex.join(command))
                continue
            run(command)
            source_log = TOPIC_DIR / "log/board/analyze_bench_compare.log"
            if not source_log.is_file():
                raise RuntimeError(f"missing compare log after run {run_index}: {source_log}")
            destination = raw_dir / f"run{run_index:02d}_analyze_bench_compare.log"
            shutil.copy2(source_log, destination)
            raw_logs.append(destination)

        if args.dry_run:
            return 0

        analyzed = subprocess.run(
            [sys.executable, str(REPEATED_ANALYZER), *map(str, raw_logs)],
            cwd=REPO_ROOT,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        ).stdout
        table = extract_table(analyzed)
        raw_log_boundary = (
            f"- raw compare logs：`{raw_dir}`；local-only diagnostic inputs，默认不进入提交边界。"
            if args.raw_dir is not None
            else "- raw compare logs：默认使用临时目录，脚本结束后清理，不进入提交边界。"
        )
        summary = "\n".join(
            [
                "# 重复 Benchmark Speedup 摘要",
                "",
                "运行上下文：",
                "- device：记录在每轮 raw compare log 中",
                f"- case filter：`{args.case_filter}`",
                f"- size：`{args.size}`",
                f"- runs：`{args.runs}`",
                f"- iterations：`{args.iterations}`",
                f"- warm-up iterations：`{args.warmup_iterations}`",
                raw_log_boundary,
                "",
                "使用 topic-local target 重新生成：",
                "",
                "```bash",
                *rerun_command_lines(args.case_filter),
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
