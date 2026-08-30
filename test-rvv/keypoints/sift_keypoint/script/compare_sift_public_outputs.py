#!/usr/bin/env python3
"""对比 SIFT public benchmark 打印的 PointWithScale trace。"""

from __future__ import annotations

import argparse
import json
import math
import re
from dataclasses import dataclass
from pathlib import Path


TRACE_RE = re.compile(
    r"^public_trace_(?P<label>public_sift_keypoint_[A-Za-z0-9_]+)\s+"
    r"index=(?P<index>\d+)\s+"
    r"x=(?P<x>[-+0-9.eE]+)\s+"
    r"y=(?P<y>[-+0-9.eE]+)\s+"
    r"z=(?P<z>[-+0-9.eE]+)\s+"
    r"scale=(?P<scale>[-+0-9.eE]+)$",
    re.MULTILINE,
)
COUNT_RE = re.compile(
    r"^public_trace_(?P<label>public_sift_keypoint_[A-Za-z0-9_]+)_count=(?P<count>\d+)\s+emitted=(?P<emitted>\d+)$",
    re.MULTILINE,
)


@dataclass(frozen=True)
class Point:
    index: int
    x: float
    y: float
    z: float
    scale: float

    def fields(self) -> tuple[float, float, float, float]:
        return (self.x, self.y, self.z, self.scale)


def parse_trace(path: Path, label: str) -> tuple[int | None, int | None, list[Point]]:
    text = path.read_text(encoding="utf-8", errors="replace")
    expected_count = None
    emitted_count = None
    for match in COUNT_RE.finditer(text):
        if match.group("label") == label:
            expected_count = int(match.group("count"))
            emitted_count = int(match.group("emitted"))

    points: list[Point] = []
    for match in TRACE_RE.finditer(text):
        if match.group("label") != label:
            continue
        points.append(
            Point(
                index=int(match.group("index")),
                x=float(match.group("x")),
                y=float(match.group("y")),
                z=float(match.group("z")),
                scale=float(match.group("scale")),
            )
        )
    points.sort(key=lambda point: point.index)
    return expected_count, emitted_count, points


def quantized_key(point: Point, tolerance: float) -> tuple[int, int, int, int]:
    scale = 1.0 / tolerance
    return tuple(int(round(value * scale)) for value in point.fields())


def compare(std_points: list[Point], rvv_points: list[Point], tolerance: float) -> dict[str, object]:
    compared = min(len(std_points), len(rvv_points))
    max_abs = {name: 0.0 for name in ("x", "y", "z", "scale")}
    mismatches: list[dict[str, object]] = []
    field_names = ("x", "y", "z", "scale")
    for i in range(compared):
        std = std_points[i]
        rvv = rvv_points[i]
        diffs = {name: abs(lhs - rhs) for name, lhs, rhs in zip(field_names, std.fields(), rvv.fields())}
        for name, diff in diffs.items():
            max_abs[name] = max(max_abs[name], diff)
        if std.index != rvv.index or any(diff > tolerance for diff in diffs.values()):
            if len(mismatches) < 20:
                mismatches.append(
                    {
                        "position": i,
                        "std": std.__dict__,
                        "rvv": rvv.__dict__,
                        "abs_diff": diffs,
                    }
                )

    std_keys = [quantized_key(point, tolerance) for point in std_points]
    rvv_keys = [quantized_key(point, tolerance) for point in rvv_points]
    std_multiset: dict[tuple[int, int, int, int], int] = {}
    rvv_multiset: dict[tuple[int, int, int, int], int] = {}
    for key in std_keys:
        std_multiset[key] = std_multiset.get(key, 0) + 1
    for key in rvv_keys:
        rvv_multiset[key] = rvv_multiset.get(key, 0) + 1
    common = 0
    remaining_rvv = dict(rvv_multiset)
    std_only: list[Point] = []
    for key, count in std_multiset.items():
        matched = min(count, remaining_rvv.get(key, 0))
        common += matched
        remaining_rvv[key] = remaining_rvv.get(key, 0) - matched
    for point in std_points:
        key = quantized_key(point, tolerance)
        if rvv_multiset.get(key, 0) <= 0:
            std_only.append(point)
        else:
            rvv_multiset[key] -= 1

    remaining_std = dict(std_multiset)
    rvv_only: list[Point] = []
    for point in rvv_points:
        key = quantized_key(point, tolerance)
        if remaining_std.get(key, 0) <= 0:
            rvv_only.append(point)
        else:
            remaining_std[key] -= 1

    return {
        "compared_points": compared,
        "std_emitted_points": len(std_points),
        "rvv_emitted_points": len(rvv_points),
        "max_abs_diff": max_abs,
        "in_order_mismatch_count": sum(
            1
            for i in range(compared)
            if std_points[i].index != rvv_points[i].index
            or any(
                not math.isclose(a, b, rel_tol=0.0, abs_tol=tolerance)
                for a, b in zip(std_points[i].fields(), rvv_points[i].fields())
            )
        ),
        "quantized_multiset_common": common,
        "quantized_multiset_std_only": len(std_points) - common,
        "quantized_multiset_rvv_only": len(rvv_points) - common,
        "first_std_only_points": [point.__dict__ for point in std_only[:20]],
        "first_rvv_only_points": [point.__dict__ for point in rvv_only[:20]],
        "first_mismatches": mismatches,
    }


def write_markdown(path: Path, payload: dict[str, object]) -> None:
    lines = [
        "# SIFT public output trace compare",
        "",
        f"- case: `{payload['case']}`",
        f"- tolerance: `{payload['tolerance']}`",
        f"- std_count: `{payload['std_count']}`; std_emitted: `{payload['std_emitted']}`",
        f"- rvv_count: `{payload['rvv_count']}`; rvv_emitted: `{payload['rvv_emitted']}`",
        f"- compared_points: `{payload['compared_points']}`",
        f"- in_order_mismatch_count: `{payload['in_order_mismatch_count']}`",
        f"- quantized_multiset_std_only: `{payload['quantized_multiset_std_only']}`",
        f"- quantized_multiset_rvv_only: `{payload['quantized_multiset_rvv_only']}`",
        "",
        "## Max Abs Diff",
        "",
        "| field | max abs diff |",
        "| --- | ---: |",
    ]
    for field, value in payload["max_abs_diff"].items():  # type: ignore[union-attr]
        lines.append(f"| `{field}` | `{value:.9g}` |")
    lines.extend(["", "## First Mismatches", ""])
    mismatches = payload["first_mismatches"]  # type: ignore[assignment]
    if not mismatches:
        lines.append("none")
    else:
        lines.extend(
            [
                "| position | std index | rvv index | dx | dy | dz | dscale |",
                "| ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
            ]
        )
        for mismatch in mismatches:  # type: ignore[union-attr]
            diff = mismatch["abs_diff"]
            lines.append(
                f"| {mismatch['position']} | {mismatch['std']['index']} | {mismatch['rvv']['index']} | "
                f"{diff['x']:.9g} | {diff['y']:.9g} | {diff['z']:.9g} | {diff['scale']:.9g} |"
            )
    lines.extend(["", "## Quantized Set-Only Points", ""])
    for title, key in (("Std-only", "first_std_only_points"), ("RVV-only", "first_rvv_only_points")):
        points = payload[key]  # type: ignore[index]
        lines.extend([f"### {title}", ""])
        if not points:
            lines.append("none")
        else:
            lines.extend(
                [
                    "| index | x | y | z | scale |",
                    "| ---: | ---: | ---: | ---: | ---: |",
                ]
            )
            for point in points:  # type: ignore[union-attr]
                lines.append(
                    f"| {point['index']} | {point['x']:.9g} | {point['y']:.9g} | "
                    f"{point['z']:.9g} | {point['scale']:.9g} |"
                )
        lines.append("")
    while lines and lines[-1] == "":
        lines.pop()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-log", required=True, type=Path)
    parser.add_argument("--rvv-log", required=True, type=Path)
    parser.add_argument("--case", default="public_sift_keypoint_320x240")
    parser.add_argument("--tolerance", type=float, default=1e-5)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    std_count, std_emitted, std_points = parse_trace(args.std_log, args.case)
    rvv_count, rvv_emitted, rvv_points = parse_trace(args.rvv_log, args.case)
    if not std_points or not rvv_points:
        raise SystemExit("missing public trace lines; rerun bench with --emit-public-trace")

    payload = compare(std_points, rvv_points, args.tolerance)
    payload.update(
        {
            "case": args.case,
            "tolerance": args.tolerance,
            "std_count": std_count,
            "std_emitted": std_emitted,
            "rvv_count": rvv_count,
            "rvv_emitted": rvv_emitted,
        }
    )
    write_markdown(args.output, payload)
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if (
        std_count != rvv_count
        or std_emitted != rvv_emitted
        or payload["in_order_mismatch_count"] != 0
        or payload["quantized_multiset_std_only"] != 0
        or payload["quantized_multiset_rvv_only"] != 0
    ):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
