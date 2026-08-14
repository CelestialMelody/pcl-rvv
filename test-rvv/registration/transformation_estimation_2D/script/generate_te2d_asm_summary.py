#!/usr/bin/env python3
"""
生成 transformation_estimation_2D 的 asm attribution（反汇编归属）摘要。

本脚本只解析当前 topic 的 bench binary objdump 输出。它把 RVV 指令按 symbol
（符号边界）和 mnemonic（指令助记符）汇总，帮助 reviewer 区分：

- test-support fused candidate 是否生成了 `vlsseg3e32`、`vfmacc`、`vfredosum` 等关键指令；
- production 标量 helper、Eigen 或 bench harness 中是否也存在 RVV 指令；
- 当前归属是否受 inline / clone 边界影响。

它不读取 raw board log，也不生成性能结论。
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


RVV_RE = re.compile(
    r"\b("
    r"vsetvli?|"
    r"vle\d+\.v|vlse\d+\.v|vluxei\d+\.v|"
    r"vse\d+\.v|vsse\d+\.v|vsuxei\d+\.v|"
    r"vlseg\d+e\d+\.v|vsseg\d+e\d+\.v|"
    r"vlsseg\d+e\d+\.v|vssseg\d+e\d+\.v|"
    r"vfadd|vfsub|vfmul|vfmacc|vfmsub|vfmsac|vfnmsac|vfnmacc|"
    r"vfredosum|vfredmax|vfredmin|vfredusum|"
    r"vmul\.vv|vadd\.vv|vsub\.vv|"
    r"vmv\.v\.|vmv\.s\.|vmv\.v\.x|vmv\.s\.x|"
    r"vmerge|vcompress|vcpop|"
    r"vreinterpret|vfncvt|vfwcvt|"
    r"vrgather|vslideup|vslidedown|"
    r"vmul\.vx|vadd\.vx|vsub\.vx|"
    r"vfmv\.f\.s|vfmv\.s\.f|vfmv\.v\.f|"
    r"vmfeq|vmflt|vmfgt|vmfne|"
    r"vfirst"
    r")\b",
    re.IGNORECASE,
)
ASM_LINE_RE = re.compile(r"^\s*[0-9a-f]+:\s+\S")
SYMBOL_RE = re.compile(r"^\s*[0-9a-f]+\s+<(.+)>:\s*$")

KEY_MNEMONICS = (
    "vlsseg3e32.v",
    "vfmacc",
    "vfredosum",
    "vfsub",
    "vfadd",
    "vsetvli",
)

CATEGORIES = {
    "production_public_lambda_boundary": (
        "runPublicCase",
        "_M_invoke",
    ),
    "production_public_boundary": (
        "TransformationEstimation2D",
        "estimateRigidTransformation(pcl::PointCloud<pcl::PointXYZ> const&, "
        "pcl::PointCloud<pcl::PointXYZ> const&",
    ),
    "candidate_lambda_boundary": (
        "runFusedCase",
        "_M_invoke",
    ),
    "test_support_fixture": (
        "rvv_te2d_support::",
    ),
    "production_scalar_boundary": (
        "TransformationEstimation2D",
        "ConstCloudIterator",
        "compute3DCentroid",
        "demeanPointCloud",
    ),
    "eigen_or_stdlib": (
        "Eigen::",
        "std::",
    ),
    "bench_entry": (
        "main",
        "runCase",
        "printBanner",
    ),
}


def _mnemonic(line: str) -> str | None:
    match = RVV_RE.search(line)
    if not match:
        return None
    return match.group(1).lower()


def parse_asm(path: Path) -> dict[str, Any]:
    current_symbol = "<no-symbol>"
    total = 0
    by_mnemonic: Counter[str] = Counter()
    by_symbol: dict[str, Counter[str]] = defaultdict(Counter)
    sample_lines: dict[str, list[str]] = defaultdict(list)

    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        symbol_match = SYMBOL_RE.match(raw_line)
        if symbol_match:
            current_symbol = symbol_match.group(1)
            continue
        if not ASM_LINE_RE.match(raw_line):
            continue
        mnemonic = _mnemonic(raw_line)
        if not mnemonic:
            continue
        total += 1
        by_mnemonic[mnemonic] += 1
        by_symbol[current_symbol][mnemonic] += 1
        if len(sample_lines[current_symbol]) < 8:
            sample_lines[current_symbol].append(raw_line.strip())

    return {
        "path": str(path),
        "total": total,
        "by_mnemonic": dict(by_mnemonic),
        "by_symbol": {symbol: dict(counts) for symbol, counts in by_symbol.items()},
        "sample_lines": dict(sample_lines),
    }


def category_for(symbol: str) -> str:
    if (
        "_M_invoke" in symbol
        and any(
            name in symbol
            for name in (
                "runRowSourceCase",
                "runSourceIndexedCase",
                "runDualIndexedCase",
                "runCorrespondenceCase",
            )
        )
    ):
        return "row_source_lambda_boundary"
    for category, needles in CATEGORIES.items():
        if all(needle in symbol for needle in needles):
            return category
    if "TransformationEstimation2D" in symbol or "ConstCloudIterator" in symbol:
        return "production_scalar_boundary"
    if "rvv_te2d_support::" in symbol:
        return "test_support_fixture"
    if "Eigen::" in symbol or "std::" in symbol:
        return "eigen_or_stdlib"
    if symbol == "main" or "main+" in symbol:
        return "bench_entry"
    return "other"


def summarize_categories(parsed: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for symbol, counts in parsed["by_symbol"].items():
        category = category_for(symbol)
        entry = result.setdefault(category, {"total": 0, "mnemonics": Counter(), "symbols": []})
        total = sum(counts.values())
        entry["total"] += total
        entry["mnemonics"].update(counts)
        entry["symbols"].append({"symbol": symbol, "total": total, "mnemonics": counts})
    for entry in result.values():
        entry["mnemonics"] = dict(entry["mnemonics"])
        entry["symbols"] = sorted(entry["symbols"], key=lambda row: -row["total"])[:12]
    return result


def key_counts(counts: dict[str, int]) -> dict[str, int]:
    result: dict[str, int] = {}
    for key in KEY_MNEMONICS:
        result[key] = sum(value for mnemonic, value in counts.items() if mnemonic.startswith(key))
    return result


def build_summary(std_full_asm: Path,
                  rvv_full_asm: Path,
                  rvv_filtered_asm: Path,
                  focus_category: str = "all") -> dict[str, Any]:
    std = parse_asm(std_full_asm)
    rvv = parse_asm(rvv_full_asm)
    categories = summarize_categories(rvv)
    production_public_entries = [
        categories.get("production_public_lambda_boundary", {"total": 0, "mnemonics": {}}),
        categories.get("production_public_boundary", {"total": 0, "mnemonics": {}}),
    ]
    candidate = categories.get("candidate_lambda_boundary", {"total": 0, "mnemonics": {}})
    row_source = categories.get("row_source_lambda_boundary", {"total": 0, "mnemonics": {}})
    production = categories.get("production_scalar_boundary", {"total": 0, "mnemonics": {}})
    production_public = {
        "total": sum(entry.get("total", 0) for entry in production_public_entries),
        "mnemonics": dict(
            sum(
                (Counter(entry.get("mnemonics", {})) for entry in production_public_entries),
                Counter(),
            )
        ),
    }
    production_public_key = key_counts(production_public["mnemonics"])
    candidate_key = key_counts(candidate.get("mnemonics", {}))
    row_source_key = key_counts(row_source.get("mnemonics", {}))
    production_key = key_counts(production.get("mnemonics", {}))
    focus_total = categories.get(focus_category, {}).get("total", 0)
    if focus_category != "all" and focus_total:
        decision = f"{focus_category}_present"
    elif production_public.get("total", 0):
        decision = "production_public_inline_boundary_present"
    elif candidate["total"]:
        decision = "candidate_inline_boundary_present"
    else:
        decision = "candidate_boundary_not_found"
    if candidate["total"] and production_public.get("total", 0):
        decision += "_with_candidate_boundary"
    if production.get("total", 0):
        decision += "_with_other_rvv_boundaries"

    return {
        "schema_version": 1,
        "topic": "registration/transformation_estimation_2D",
        "artifacts": {
            "std_full_asm": str(std_full_asm),
            "rvv_full_asm": str(rvv_full_asm),
            "rvv_filtered_asm": str(rvv_filtered_asm),
        },
        "totals": {
            "std_rvv_lines": std["total"],
            "rvv_rvv_lines": rvv["total"],
            "delta_rvv_minus_std": rvv["total"] - std["total"],
        },
        "rvv_top_mnemonics": dict(Counter(rvv["by_mnemonic"]).most_common(20)),
        "std_top_mnemonics": dict(Counter(std["by_mnemonic"]).most_common(20)),
        "categories": categories,
        "production_public_key_mnemonics": production_public_key,
        "candidate_key_mnemonics": candidate_key,
        "row_source_key_mnemonics": row_source_key,
        "production_key_mnemonics": production_key,
        "focus_category": focus_category,
        "attribution_decision": decision,
        "boundary_note": (
            "Production public RVV instructions are attributed to either the runPublicCase "
            "lambda boundary or the exact PointXYZ ordered-cloud-pair public overload when "
            "the wrapper is fully inlined. Candidate RVV instructions are attributed to the "
            "runFusedCase lambda symbol; row-source candidates are attributed to the "
            "runRowSourceCase / row-source wrapper lambda family. The header-only helper "
            "may therefore be visible through an inlined caller symbol rather than a "
            "standalone helper."
        ),
    }


def render_markdown(summary: dict[str, Any]) -> str:
    totals = summary["totals"]
    categories = summary["categories"]
    lines: list[str] = []
    lines.append("# transformation_estimation_2D ASM Attribution Summary")
    lines.append("")
    lines.append("## 结论")
    lines.append("")
    lines.append(
        "当前 RVV bench binary 中，production public case 的关键指令优先归属到 "
        "`runPublicCase` lambda 内联边界，或 exact `PointXYZ` ordered-cloud-pair "
        "公开 overload 的 production symbol；test-only fused candidate 仍归属到 "
        "`runFusedCase` lambda 内联边界；row-source candidate 归属到 "
        "`runRowSourceCase` 或具体 row-source wrapper lambda 内联边界。需要结合 case-filter 和 production direct tests "
        "判断证据角色。"
    )
    lines.append("")
    lines.append(f"- attribution_decision：`{summary['attribution_decision']}`")
    lines.append(f"- std RVV lines：{totals['std_rvv_lines']}")
    lines.append(f"- rvv RVV lines：{totals['rvv_rvv_lines']}")
    lines.append(f"- delta：{totals['delta_rvv_minus_std']:+d}")
    lines.append("")
    lines.append("## 关键指令归属")
    lines.append("")
    lines.append("| category | RVV lines | key mnemonics | boundary |")
    lines.append("| --- | ---: | --- | --- |")
    for category, entry in sorted(categories.items(), key=lambda item: -item[1]["total"]):
        key = key_counts(entry.get("mnemonics", {}))
        key_text = ", ".join(f"{name}={count}" for name, count in key.items() if count)
        symbol = entry["symbols"][0]["symbol"] if entry.get("symbols") else "none"
        lines.append(f"| `{category}` | {entry['total']} | {key_text or 'none'} | `{symbol}` |")
    lines.append("")
    lines.append("## RVV build top mnemonics")
    lines.append("")
    lines.append("| mnemonic | count |")
    lines.append("| --- | ---: |")
    for mnemonic, count in summary["rvv_top_mnemonics"].items():
        lines.append(f"| `{mnemonic}` | {count} |")
    lines.append("")
    lines.append("## Top symbols")
    lines.append("")
    lines.append("| category | symbol | RVV lines | sample |")
    lines.append("| --- | --- | ---: | --- |")
    for category, entry in sorted(categories.items(), key=lambda item: -item[1]["total"]):
        for row in entry.get("symbols", [])[:5]:
            sample = "; ".join(summary_sample(row["symbol"], summary))
            lines.append(
                f"| `{category}` | `{row['symbol']}` | {row['total']} | `{sample}` |"
            )
    lines.append("")
    lines.append("## 边界说明")
    lines.append("")
    lines.append("- `runPublicCase` lambda 或 exact `PointXYZ` ordered-cloud-pair 公开 overload 边界用于 production public smoke / board bench，PI2 后可作为 production helper 命中证据的一部分。")
    lines.append("- `runFusedCase` lambda 内联边界对应 test-only fused candidate，不能替代 production public evidence。")
    lines.append("- `runRowSourceCase` 或 source-indexed、dual-indexed、correspondence wrapper lambda 边界对应 materialize-to-ordered row-source candidate，不能替代 production public evidence。")
    lines.append("- production fallback、Eigen / iterator 路径也可能含 RVV 指令；这些指令不能归入当前 production helper 收益。")
    lines.append("- 本摘要只支持 asm attribution（反汇编归属），不支持目标硬件性能结论。")
    lines.append("")
    return "\n".join(lines)


def summary_sample(symbol: str, summary: dict[str, Any]) -> list[str]:
    # sample_lines 不写入最终 JSON 顶层，保持摘要稳定；需要样例时重新解析 RVV full asm。
    rvv_full_asm = Path(summary["artifacts"]["rvv_full_asm"])
    parsed = parse_asm(rvv_full_asm)
    return parsed.get("sample_lines", {}).get(symbol, [])[:2]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--std-full-asm", required=True, type=Path)
    parser.add_argument("--rvv-full-asm", required=True, type=Path)
    parser.add_argument("--rvv-filtered-asm", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--json-output", required=True, type=Path)
    parser.add_argument("--focus-category", default="all")
    args = parser.parse_args()

    summary = build_summary(
        args.std_full_asm,
        args.rvv_full_asm,
        args.rvv_filtered_asm,
        args.focus_category,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render_markdown(summary), encoding="utf-8")
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"wrote asm summary: {args.output}")
    print(f"wrote asm summary json: {args.json_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
