#!/usr/bin/env python3
"""
Sanitize RVV evidence logs before committing them.

The script replaces machine-local paths, board paths, runtime library values,
SSH-style targets, and IP addresses with stable placeholders. By default it only
reports what would change. Use --in-place to rewrite files, or --check to fail
when any input still needs sanitizing.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


RULES: list[tuple[str, re.Pattern[str], str]] = [
    (
        "LD_LIBRARY_PATH",
        re.compile(r"LD_LIBRARY_PATH=(?:\\\n|[^\s])+"),
        "LD_LIBRARY_PATH=<runtime-libs>",
    ),
    (
        "pcl source root",
        re.compile(r"/home/[^ \t\n'\"，。；:]+/(?:codes/RISCV/workspace|codes|workspace)/pcl"),
        "<pcl-src>",
    ),
    (
        "riscv dependency root",
        re.compile(r"/home/[^ \t\n'\"，。；:]+/(?:codes/RISCV/workspace|codes|workspace)/riscv"),
        "<rv-install>",
    ),
    (
        "x86 dependency root",
        re.compile(r"/home/[^ \t\n'\"，。；:]+/(?:codes/RISCV/workspace|codes|workspace)/x86"),
        "<x86-install>",
    ),
    (
        "riscv build root",
        re.compile(r"/home/[^ \t\n'\"，。；:]+/(?:codes/)?riscv-build"),
        "<riscv-build>",
    ),
    (
        "third-party source root",
        re.compile(r"/home/[^ \t\n'\"，。；:]+/(?:codes/RISCV/workspace|codes|workspace)/pkgs"),
        "<third-party-src>",
    ),
    (
        "board pcl-test root",
        re.compile(r"/root/pcl-test"),
        "<board-root>",
    ),
    (
        "riscv toolchain root",
        re.compile(r"/opt/riscv64"),
        "<riscv-toolchain>",
    ),
    (
        "generic home path",
        re.compile(r"/home/[^ \t\n'\"，。；]+"),
        "<home>",
    ),
    (
        "ssh target",
        re.compile(r"\b[A-Za-z0-9_.+-]+@[A-Za-z0-9_.-]+\b"),
        "<ssh-target>",
    ),
    (
        "ipv4 address",
        re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b"),
        "<ip-address>",
    ),
]


def sanitize_text(text: str) -> tuple[str, dict[str, int]]:
    counts: dict[str, int] = {}
    out = text
    for name, pattern, replacement in RULES:
        out, count = pattern.subn(replacement, out)
        if count:
            counts[name] = counts.get(name, 0) + count
    return out, counts


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        print(f"skip binary/non-utf8: {path}", file=sys.stderr)
        return None


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Sanitize machine-local details from RVV evidence logs."
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="rewrite files in place; without this, only report planned changes",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="return non-zero if any file would change",
    )
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args(argv)

    changed = 0
    for path in args.files:
        text = read_text(path)
        if text is None:
            continue
        sanitized, counts = sanitize_text(text)
        if sanitized == text:
            print(f"clean: {path}")
            continue

        changed += 1
        summary = ", ".join(f"{name}={count}" for name, count in sorted(counts.items()))
        print(f"sanitize: {path} ({summary})")
        if args.in_place:
            path.write_text(sanitized, encoding="utf-8")

    if args.check and changed:
        print(f"{changed} file(s) still need sanitizing.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
