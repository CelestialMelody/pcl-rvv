#!/usr/bin/env python3
"""Check production RVV helper attribution for sac_model_line.

The gate intentionally looks for production helper names and their key RVV
instructions in the RVV bench assembly. Before production integration this
should fail because the helpers do not exist.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REQUIRED = {
    "countWithinDistanceRVV": ["vlux", "vfmacc", "vmflt", "vcpop"],
    "selectWithinDistanceRVV": ["vlux", "vfmacc", "vmflt", "vcompress", "vse32", "vfwcvt", "vse64"],
    "getDistancesToModelRVV": ["vlux", "vfmacc", "vfsqrt", "vfwcvt", "vse64"],
}

FIRST_VFSQRT_SEQUENCE = {
    "getDistancesToModelRVV": ["vfwcvt", "vse64"],
}


def helper_body(asm: str, helper: str) -> str:
    match = re.search(rf"^[0-9a-f]+ <.*{re.escape(helper)}.*>:\n", asm, re.M)
    if not match:
        return ""
    start = match.end()
    next_symbol = re.search(r"^[0-9a-f]+ <[^>]+>:\n", asm[start:], re.M)
    end = start + next_symbol.start() if next_symbol else len(asm)
    return asm[start:end]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--asm", required=True, type=Path)
    args = parser.parse_args()

    asm = args.asm.read_text(encoding="utf-8", errors="replace")
    failures: list[str] = []

    for helper, instructions in REQUIRED.items():
        body = helper_body(asm, helper)
        if not body:
            failures.append(f"missing helper symbol: {helper}")
            continue
        for instruction in instructions:
            if instruction not in body:
                failures.append(f"{helper}: missing instruction containing {instruction!r}")
        sequence = FIRST_VFSQRT_SEQUENCE.get(helper)
        if sequence:
            lines = body.splitlines()
            first_vfsqrt = next((idx for idx, line in enumerate(lines) if "vfsqrt" in line), None)
            if first_vfsqrt is None:
                failures.append(f"{helper}: missing first vfsqrt sequence")
            else:
                window = "\n".join(lines[first_vfsqrt:first_vfsqrt + 8])
                if "vse32" in window:
                    failures.append(f"{helper}: first vfsqrt sequence stores through vse32 scratch")
                for instruction in sequence:
                    if instruction not in window:
                        failures.append(
                            f"{helper}: first vfsqrt sequence missing instruction containing {instruction!r}"
                        )

    if failures:
        print("line production asm gate: FAIL")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("line production asm gate: PASS")
    for helper in REQUIRED:
        print(f"- {helper}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
