#!/usr/bin/env python3
"""Check RVV instructions are attributable to the moment_invariants production path."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


SYMBOL_RE = re.compile(r"^[0-9a-fA-F]+ <(.+)>:$")
LOAD_RE = re.compile(r"\bvlux(?:seg3)?ei32\b")
REDUCE_RE = re.compile(r"\bvfred[ou]sum\b")

DEFAULT_SYMBOL_FRAGMENTS = (
    "pcl::detail::momentInvariantsIndexedMomentsRVV<pcl::PointXYZ>",
    "pcl::MomentInvariantsEstimation<pcl::PointXYZ, pcl::MomentInvariants>::computePointMomentInvariants",
    "pcl::MomentInvariantsEstimation<pcl::PointXYZ, pcl::MomentInvariants>::computeFeature",
)


def parse_sections(lines: list[str]) -> list[tuple[str, int, int, list[str]]]:
    sections: list[tuple[str, int, int, list[str]]] = []
    current_name: str | None = None
    current_start = 0
    current_lines: list[str] = []

    for lineno, line in enumerate(lines, start=1):
        match = SYMBOL_RE.match(line)
        if match:
            if current_name is not None:
                sections.append((current_name, current_start, lineno - 1, current_lines))
            current_name = match.group(1)
            current_start = lineno
            current_lines = [line]
        elif current_name is not None:
            current_lines.append(line)

    if current_name is not None:
        sections.append((current_name, current_start, len(lines), current_lines))
    return sections


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("asm", type=Path)
    parser.add_argument(
        "--symbol-fragment",
        action="append",
        dest="symbol_fragments",
        help="Production symbol fragment to inspect. Defaults to PointXYZ moment_invariants symbols.",
    )
    args = parser.parse_args()
    symbol_fragments = tuple(args.symbol_fragments or DEFAULT_SYMBOL_FRAGMENTS)

    text = args.asm.read_text(encoding="utf-8", errors="replace")
    sections = parse_sections(text.splitlines())
    matching_sections = [
        section
        for section in sections
        if any(fragment in section[0] for fragment in symbol_fragments)
    ]

    found_load = False
    found_reduce = False
    evidence: list[str] = []
    for name, start, end, section_lines in matching_sections:
        body = "\n".join(section_lines)
        section_has_load = LOAD_RE.search(body) is not None
        section_has_reduce = REDUCE_RE.search(body) is not None
        if section_has_load or section_has_reduce:
            evidence.append(
                f"{name} lines={start}-{end} load={section_has_load} reduce={section_has_reduce}"
            )
        found_load = found_load or section_has_load
        found_reduce = found_reduce or section_has_reduce

    if found_load and found_reduce:
        print("[OK] production RVV asm attribution found")
        for item in evidence:
            print(f"  {item}")
        return 0

    print("[FAIL] production RVV asm attribution missing for moment_invariants public path", file=sys.stderr)
    if not matching_sections:
        print("  no matching production symbols found", file=sys.stderr)
    else:
        print("  checked symbols:", file=sys.stderr)
        for name, start, end, _ in matching_sections:
            print(f"    {name} lines={start}-{end}", file=sys.stderr)
    print(f"  load_found={found_load} reduce_found={found_reduce}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
