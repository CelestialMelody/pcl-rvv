#!/usr/bin/env python3
"""
Generate asm attribution reports for weighted TEPTPLW RVV symbols.

The report is derived from objdump output produced by:

  make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted dump_bench_rvv

The production profile is the production-integration evidence gate. The generic
profile is a test_support code-generation diagnostic and must not be treated as
production-dispatch evidence.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]


SYMBOL_RE = re.compile(r"^\s*(?P<addr>[0-9a-fA-F]+)\s+<(?P<name>.+)>:\s*$")
INSTR_RE = re.compile(r"^\s*(?P<addr>[0-9a-fA-F]+):\s+(?P<rest>.+)$")
HEX_TOKEN_RE = re.compile(r"^[0-9a-fA-F]+$")
PRODUCTION_SYMBOL_RE = re.compile(
    r"buildPointToPlaneLLSWeightedFullCloudBlockRVV"
    r"(?P<fused>FusedAbcdIlp)?<(?P<src>pcl::Point[^,>]+), (?P<tgt>pcl::Point[^>]+)>"
)
PRODUCTION_ESTIMATE_RVV_SYMBOL_RE = re.compile(
    r"estimatePointToPlaneLLSWeightedFullCloudRVV<"
    r"(?P<src>pcl::Point[^,>]+), (?P<tgt>pcl::Point[^,>]+), float>"
)
PRODUCTION_ITERATOR_SYMBOL_RE = re.compile(
    r"TransformationEstimationPointToPlaneLLSWeighted<"
    r"(?P<src>pcl::Point[^,>]+), (?P<tgt>pcl::Point[^,>]+), float>"
    r"::estimateRigidTransformation\(pcl::ConstCloudIterator<"
)
PRODUCTION_PUBLIC_FULL_CLOUD_SYMBOL_RE = re.compile(
    r"TransformationEstimationPointToPlaneLLSWeighted<"
    r"(?P<src>pcl::Point[^,>]+), (?P<tgt>pcl::Point[^,>]+), float>"
    r"::estimateRigidTransformation\(pcl::PointCloud<(?P=src)> const&, "
    r"pcl::PointCloud<(?P=tgt)> const&, Eigen::Matrix<"
)
GENERIC_FUSED_SYMBOL_RE = re.compile(
    r"accumulate_candidate_full_block_fused_formula_layout_gated<"
    r"\(pcl::registration::rvv_te_pt2plane_lls_weighted_diag::WeightedFusedFormulaMode\)"
    r"(?P<mode>[0-7]), pcl::(?P<src>Point[^,>]+), pcl::(?P<tgt>Point[^>]+)>"
)

POINT_ORDER = (
    "pointnormal",
    "pointxyz-to-pointnormal",
    "pointxyz-to-pointxyzinormal",
)
VARIANT_ORDER = ("baseline", "fused")
GENERIC_MODE_LABELS = {
    0: "abc",
    1: "abc-ilp",
    2: "d-six-term",
    3: "d-six-term-ilp",
    4: "d-displacement",
    5: "d-displacement-ilp",
    6: "abcd",
    7: "abcd-ilp",
}
POINT_LABELS = {
    ("PointNormal", "PointNormal"): "pointnormal",
    ("PointXYZ", "PointNormal"): "pointxyz-to-pointnormal",
    ("PointXYZ", "PointXYZINormal"): "pointxyz-to-pointxyzinormal",
}


def display_path(path: Path | None) -> str:
  if path is None:
    return "<stdout>"
  resolved = path.resolve()
  try:
    return str(resolved.relative_to(REPO_ROOT))
  except ValueError:
    return str(path)


@dataclass
class SymbolBlock:
  address: int
  name: str
  lines: list[str]
  end_address: int | None = None


@dataclass(frozen=True)
class AsmCounts:
  address: int
  bytes_: int
  instr: int
  rvv_total: int
  vset: int
  vlse32: int
  vle32: int
  fmul: int
  fadd: int
  fsub: int
  fmacc: int
  fmsac: int
  merge: int
  reduce: int
  vcpop: int
  spill_reload: int
  jal: int


@dataclass(frozen=True)
class SymbolAttribution:
  counts: AsmCounts
  boundary: str
  symbol_name: str


def parse_symbols(path: Path) -> list[SymbolBlock]:
  symbols: list[SymbolBlock] = []
  current: SymbolBlock | None = None
  for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
    if match := SYMBOL_RE.match(line):
      current = SymbolBlock(
          address=int(match.group("addr"), 16),
          name=match.group("name"),
          lines=[],
      )
      symbols.append(current)
      continue
    if current is not None:
      current.lines.append(line)

  addresses = [symbol.address for symbol in symbols]
  for index, symbol in enumerate(symbols):
    end_address = None
    for candidate in addresses[index + 1 :]:
      if candidate > symbol.address:
        end_address = candidate
        break
    symbol.end_address = end_address
  return symbols


def instruction_address(line: str) -> int | None:
  if match := INSTR_RE.match(line):
    return int(match.group("addr"), 16)
  return None


def mnemonic_from_line(line: str) -> str | None:
  match = INSTR_RE.match(line)
  if not match:
    return None
  tokens = match.group("rest").split()
  while tokens and HEX_TOKEN_RE.fullmatch(tokens[0]):
    tokens.pop(0)
  if not tokens:
    return None
  return tokens[0]


def short_point_type(type_name: str) -> str:
  return type_name.rsplit("::", 1)[-1].strip()


def production_symbol_key(symbol_name: str) -> tuple[str, str] | None:
  match = PRODUCTION_SYMBOL_RE.search(symbol_name)
  if not match:
    return None
  src = short_point_type(match.group("src"))
  tgt = short_point_type(match.group("tgt"))
  point_label = POINT_LABELS.get((src, tgt))
  if point_label is None:
    return None
  variant = "fused" if match.group("fused") else "baseline"
  return point_label, variant


def point_label_from_src_tgt(src_name: str, tgt_name: str) -> str | None:
  src = short_point_type(src_name)
  tgt = short_point_type(tgt_name)
  return POINT_LABELS.get((src, tgt))


def production_default_boundary_key(symbol_name: str) -> tuple[str, int, str] | None:
  if match := PRODUCTION_SYMBOL_RE.search(symbol_name):
    point_label = point_label_from_src_tgt(match.group("src"), match.group("tgt"))
    if point_label is not None:
      return point_label, 0, "detail"

  if match := PRODUCTION_ESTIMATE_RVV_SYMBOL_RE.search(symbol_name):
    point_label = point_label_from_src_tgt(match.group("src"), match.group("tgt"))
    if point_label is not None:
      return point_label, 1, "estimate-rvv-helper"

  if match := PRODUCTION_PUBLIC_FULL_CLOUD_SYMBOL_RE.search(symbol_name):
    point_label = point_label_from_src_tgt(match.group("src"), match.group("tgt"))
    if point_label is not None:
      return point_label, 2, "public-full-cloud"

  if match := PRODUCTION_ITERATOR_SYMBOL_RE.search(symbol_name):
    point_label = point_label_from_src_tgt(match.group("src"), match.group("tgt"))
    if point_label is not None:
      return point_label, 3, "iterator-clone"

  return None


def generic_fused_symbol_key(symbol_name: str) -> tuple[str, str] | None:
  match = GENERIC_FUSED_SYMBOL_RE.search(symbol_name)
  if not match:
    return None
  src = short_point_type(match.group("src"))
  tgt = short_point_type(match.group("tgt"))
  point_label = POINT_LABELS.get((src, tgt))
  if point_label is None:
    return None
  mode = int(match.group("mode"))
  return point_label, GENERIC_MODE_LABELS[mode]


def count_symbol(symbol: SymbolBlock) -> AsmCounts:
  mnemonics: list[str] = []
  last_instruction_address = symbol.address
  for line in symbol.lines:
    if address := instruction_address(line):
      last_instruction_address = address
    if mnemonic := mnemonic_from_line(line):
      mnemonics.append(mnemonic)

  counts = Counter(mnemonics)
  end_address = symbol.end_address
  if end_address is None:
    end_address = last_instruction_address + 4
  return AsmCounts(
      address=symbol.address,
      bytes_=max(0, end_address - symbol.address),
      instr=len(mnemonics),
      rvv_total=sum(1 for mnemonic in mnemonics if mnemonic.startswith("v")),
      vset=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vset")),
      vlse32=counts["vlse32.v"],
      vle32=counts["vle32.v"],
      fmul=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vfmul")),
      fadd=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vfadd")),
      fsub=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vfsub")),
      fmacc=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vfmacc")),
      fmsac=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vfmsac")),
      merge=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vmerge")),
      reduce=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vfred")),
      vcpop=sum(1 for mnemonic in mnemonics if mnemonic.startswith("vcpop")),
      spill_reload=counts["vs1r.v"] + counts["vl1re32.v"],
      jal=counts["jal"],
  )


def collect_production_symbol_counts(path: Path) -> dict[tuple[str, str], AsmCounts]:
  results: dict[tuple[str, str], AsmCounts] = {}
  for symbol in parse_symbols(path):
    key = production_symbol_key(symbol.name)
    if key is None:
      continue
    # Objdump may contain weak aliases in other topics. Keep the first concrete
    # definition for this profile and fail later if an expected symbol is absent.
    results.setdefault(key, count_symbol(symbol))
  return results


def collect_production_default_attributions(path: Path) -> dict[str, SymbolAttribution]:
  ranked: dict[str, tuple[int, SymbolAttribution]] = {}
  for symbol in parse_symbols(path):
    key = production_default_boundary_key(symbol.name)
    if key is None:
      continue
    point_label, priority, boundary = key
    attribution = SymbolAttribution(
        counts=count_symbol(symbol),
        boundary=boundary,
        symbol_name=symbol.name,
    )
    previous = ranked.get(point_label)
    if previous is None or priority < previous[0]:
      ranked[point_label] = (priority, attribution)
  return {point_type: attribution for point_type, (_, attribution) in ranked.items()}


def collect_generic_fused_formula_counts(path: Path) -> dict[tuple[str, str], AsmCounts]:
  results: dict[tuple[str, str], AsmCounts] = {}
  for symbol in parse_symbols(path):
    key = generic_fused_symbol_key(symbol.name)
    if key is None:
      continue
    results.setdefault(key, count_symbol(symbol))
  return results


def format_table_row(point_type: str, variant: str, counts: AsmCounts) -> str:
  return (
      f"| `{point_type}` | `{variant}` | `0x{counts.address:x}` | {counts.bytes_} | "
      f"{counts.instr} | {counts.rvv_total} | {counts.vset} | {counts.vlse32} | "
      f"{counts.vle32} | {counts.fmul} | {counts.fadd} | {counts.fsub} | "
      f"{counts.fmacc} | {counts.fmsac} | {counts.merge} | {counts.reduce} | "
      f"{counts.vcpop} | {counts.spill_reload} | {counts.jal} |"
  )


def format_attributed_table_row(point_type: str, variant: str, attribution: SymbolAttribution) -> str:
  counts = attribution.counts
  return (
      f"| `{point_type}` | `{variant}` | `{attribution.boundary}` | "
      f"`0x{counts.address:x}` | {counts.bytes_} | "
      f"{counts.instr} | {counts.rvv_total} | {counts.vset} | {counts.vlse32} | "
      f"{counts.vle32} | {counts.fmul} | {counts.fadd} | {counts.fsub} | "
      f"{counts.fmacc} | {counts.fmsac} | {counts.merge} | {counts.reduce} | "
      f"{counts.vcpop} | {counts.spill_reload} | {counts.jal} |"
  )


def markdown_table(headers: list[str], aligns: list[str], rows: list[list[str]]) -> list[str]:
  if len(headers) != len(aligns):
    raise ValueError("headers and aligns must have the same length")
  for row in rows:
    if len(row) != len(headers):
      raise ValueError("table row has a different length from headers")

  separators = []
  for align in aligns:
    if align == "right":
      separators.append("---:")
    else:
      separators.append("---")

  lines = [
      "| " + " | ".join(headers) + " |",
      "| " + " | ".join(separators) + " |",
  ]
  lines.extend("| " + " | ".join(row) + " |" for row in rows)
  return lines


def delta_text(label: str, baseline: AsmCounts, fused: AsmCounts) -> str:
  return (
      f"- `{label}`: fused RVV 指令数 delta = {fused.rvv_total - baseline.rvv_total:+d}；"
      f"bytes {baseline.bytes_}->{fused.bytes_}；"
      f"`vfmsac` {baseline.fmsac}->{fused.fmsac}，"
      f"`vfmacc` {baseline.fmacc}->{fused.fmacc}，"
      f"`vfadd` {baseline.fadd}->{fused.fadd}，"
      f"`vfsub` {baseline.fsub}->{fused.fsub}，"
      f"spill/reload {baseline.spill_reload}->{fused.spill_reload}，"
      f"jal {baseline.jal}->{fused.jal}。"
  )


def same_instruction_profile(left: AsmCounts, right: AsmCounts) -> bool:
  return (
      left.rvv_total == right.rvv_total
      and left.vset == right.vset
      and left.vlse32 == right.vlse32
      and left.vle32 == right.vle32
      and left.fmul == right.fmul
      and left.fadd == right.fadd
      and left.fsub == right.fsub
      and left.fmacc == right.fmacc
      and left.fmsac == right.fmsac
      and left.merge == right.merge
      and left.reduce == right.reduce
      and left.vcpop == right.vcpop
      and left.spill_reload == right.spill_reload
      and left.jal == right.jal
  )


def format_report(asm_path: Path, output_path: Path | None, counts: dict[tuple[str, str], AsmCounts]) -> str:
  missing = [
      (point_type, variant)
      for point_type in POINT_ORDER
      for variant in VARIANT_ORDER
      if (point_type, variant) not in counts
  ]
  if missing:
    missing_text = ", ".join(f"{point_type}/{variant}" for point_type, variant in missing)
    raise SystemExit(f"missing expected production symbols: {missing_text}")

  asm_note = display_path(asm_path)
  output_note = display_path(output_path)
  lines = [
      "# production-symbol fused-abcd-ilp asm attribution",
      "",
      f"- asm: `{asm_note}`",
      "- generated by: "
      "`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/script/generate_teptplw_asm_attribution.py "
      f"--profile production-symbol-fused-abcd-ilp --asm {asm_note} --output {output_note}`",
      "- 口径：只统计 `buildPointToPlaneLLSWeightedFullCloudBlockRVV*` production "
      "detail 符号内的指令，不统计调用者 checksum、bench harness 或 test_support helper。",
      "- `rvv total` 包含所有以 `v` 开头的 RVV 指令；`spill/reload` 统计 "
      "`vs1r.v` 与 `vl1re32.v` 的合计。",
      "",
  ]
  table_rows = []
  for point_type in POINT_ORDER:
    for variant in VARIANT_ORDER:
      item = counts[(point_type, variant)]
      table_rows.append([
          f"`{point_type}`",
          f"`{variant}`",
          f"`0x{item.address:x}`",
          str(item.bytes_),
          str(item.instr),
          str(item.rvv_total),
          str(item.vset),
          str(item.vlse32),
          str(item.vle32),
          str(item.fmul),
          str(item.fadd),
          str(item.fsub),
          str(item.fmacc),
          str(item.fmsac),
          str(item.merge),
          str(item.reduce),
          str(item.vcpop),
          str(item.spill_reload),
          str(item.jal),
      ])
  lines.extend(markdown_table(
      [
          "point type",
          "variant",
          "addr",
          "bytes",
          "instr",
          "rvv total",
          "vset",
          "vlse32",
          "vle32",
          "fmul",
          "fadd",
          "fsub",
          "fmacc",
          "fmsac",
          "merge",
          "reduce",
          "vcpop",
          "spill/reload",
          "jal",
      ],
      ["left", "left", *["right"] * 17],
      table_rows,
  ))

  lines.extend(["", "## 归因摘记"])
  for point_type in POINT_ORDER:
    baseline = counts[(point_type, "baseline")]
    fused = counts[(point_type, "fused")]
    lines.append(delta_text(point_type, baseline, fused))
  lines.append("")
  return "\n".join(lines)


def format_production_default_report(
    asm_path: Path, output_path: Path | None, attributions: dict[str, SymbolAttribution]
) -> str:
  missing = [point_type for point_type in POINT_ORDER if point_type not in attributions]
  if missing:
    raise SystemExit("missing expected production symbols: " + ", ".join(missing))

  asm_note = display_path(asm_path)
  output_note = display_path(output_path)
  lines = [
      "# production-default fused-abcd-ilp asm attribution",
      "",
      f"- asm: `{asm_note}`",
      "- generated by: "
      "`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/script/generate_teptplw_asm_attribution.py "
      f"--profile production-default-fused-abcd-ilp --asm {asm_note} --output {output_note}`",
      "- 口径：当前默认 production 已经采用 fused-abcd-ilp 公式形态；"
      "归因优先统计 `buildPointToPlaneLLSWeightedFullCloudBlockRVV` detail 符号。"
      "如果某个点型的 detail 符号被编译器内联/合并，则回退到 "
      "`estimatePointToPlaneLLSWeightedFullCloudRVV` 包装 helper，再回退到 "
      "public full-cloud overload，最后才回退到 iterator clone。每一行的 "
      "`boundary` 列记录实际采用的归因边界。",
      "- `rvv total` 包含所有以 `v` 开头的 RVV 指令；`spill/reload` 统计 "
      "`vs1r.v` 与 `vl1re32.v` 的合计。",
      "",
      "## 符号边界",
      "",
  ]

  summary_rows = []
  memory_rows = []
  arithmetic_rows = []
  for point_type in POINT_ORDER:
    attribution = attributions[point_type]
    counts = attribution.counts
    summary_rows.append([
        f"`{point_type}`",
        "`default-fused-abcd-ilp`",
        f"`{attribution.boundary}`",
        f"`0x{counts.address:x}`",
        str(counts.bytes_),
        str(counts.instr),
        str(counts.rvv_total),
        str(counts.jal),
    ])
    memory_rows.append([
        f"`{point_type}`",
        str(counts.vset),
        str(counts.vlse32),
        str(counts.vle32),
        str(counts.vcpop),
        str(counts.spill_reload),
    ])
    arithmetic_rows.append([
        f"`{point_type}`",
        str(counts.fmul),
        str(counts.fadd),
        str(counts.fsub),
        str(counts.fmacc),
        str(counts.fmsac),
        str(counts.merge),
        str(counts.reduce),
    ])

  lines.extend(markdown_table(
      ["point type", "variant", "boundary", "addr", "bytes", "instr", "rvv total", "jal"],
      ["left", "left", "left", "right", "right", "right", "right", "right"],
      summary_rows,
  ))
  lines.extend([
      "",
      "## RVV 访存与控制",
      "",
  ])
  lines.extend(markdown_table(
      ["point type", "vset", "vlse32", "vle32", "vcpop", "spill/reload"],
      ["left", "right", "right", "right", "right", "right"],
      memory_rows,
  ))
  lines.extend([
      "",
      "## RVV 算术与归约",
      "",
  ])
  lines.extend(markdown_table(
      ["point type", "fmul", "fadd", "fsub", "fmacc", "fmsac", "merge", "reduce"],
      ["left", "right", "right", "right", "right", "right", "right", "right"],
      arithmetic_rows,
  ))
  lines.extend(
      [
          "",
          "## 归因摘记",
          "- 接入后 production 不再保留旧 block-baseline 与 fused helper 两个符号；"
          "历史 fused-vs-block A/B 见旧 `production_symbol_fused_abcd_ilp_*` 输出目录。",
          "- 当前 profile 用于确认默认 production 符号本身的 RVV 指令形态，"
          "不能再输出同一二进制内的旧 block/fused delta。",
          "- 不同点型可能落在不同归因边界：这是编译器内联/codegen 的结果，"
          "不是 benchmark 失败。比较表时必须同时看 `boundary` 列，避免把 "
          "`detail`、`estimate-rvv-helper`、`public-full-cloud` 或 `iterator-clone` "
          "的总指令数直接当成同一粒度比较。",
          "",
      ]
  )
  return "\n".join(lines)


def format_generic_report(
    asm_path: Path,
    output_path: Path | None,
    baselines: dict[tuple[str, str], AsmCounts],
    candidates: dict[tuple[str, str], AsmCounts],
) -> str:
  missing_baselines = [
      point_type for point_type in POINT_ORDER if (point_type, "baseline") not in baselines
  ]
  if missing_baselines:
    raise SystemExit(
        "missing expected production baseline symbols: " + ", ".join(missing_baselines)
    )
  missing_candidates = [
      (point_type, candidate)
      for point_type in POINT_ORDER
      for candidate in GENERIC_MODE_LABELS.values()
      if (point_type, candidate) not in candidates
  ]
  if missing_candidates:
    missing_text = ", ".join(f"{point_type}/{candidate}" for point_type, candidate in missing_candidates)
    raise SystemExit(f"missing expected generic fused symbols: {missing_text}")

  asm_note = display_path(asm_path)
  output_note = display_path(output_path)
  lines = [
      "# generic fused formula asm attribution",
      "",
      f"- asm: `{asm_note}`",
      "- generated by: "
      "`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/script/generate_teptplw_asm_attribution.py "
      f"--profile generic-fused-formula --asm {asm_note} --output {output_note}`",
      "- 口径：baseline 取 production `buildPointToPlaneLLSWeightedFullCloudBlockRVV` "
      "detail 符号；candidate 取 test_support "
      "`accumulate_candidate_full_block_fused_formula_layout_gated<Mode,...>` 符号。",
      "- 表中计数覆盖完整符号，包含参数/gate/fallback 分支；它不是只截取某个热循环。"
      "`jal` 因此也可能来自冷路径的 scalar fallback 或对象初始化，必须结合调用目标审计。",
      "- 该 profile 用于 codegen 归因，不代表 candidate 已接入 production dispatch。",
      "",
  ]
  table_rows = []
  for point_type in POINT_ORDER:
    baseline = baselines[(point_type, "baseline")]
    for candidate in GENERIC_MODE_LABELS.values():
      counts = candidates[(point_type, candidate)]
      table_rows.append([
          f"`{point_type}`",
          f"`{candidate}`",
          f"`0x{counts.address:x}`",
          str(counts.rvv_total),
          str(baseline.rvv_total),
          f"{counts.rvv_total - baseline.rvv_total:+d}",
          str(counts.fmul),
          str(counts.fadd),
          str(counts.fsub),
          str(counts.fmacc),
          str(counts.fmsac),
          str(counts.reduce),
          str(counts.spill_reload),
          str(counts.jal),
      ])
  lines.extend(markdown_table(
      [
          "point type",
          "candidate",
          "addr",
          "rvv total",
          "baseline rvv",
          "delta",
          "vfmul",
          "vfadd",
          "vfsub",
          "vfmacc",
          "vfmsac",
          "vfred",
          "spill/reload",
          "jal",
      ],
      ["left", "left", *["right"] * 12],
      table_rows,
  ))
  lines.extend(["", "## 归因摘记"])
  for point_type in POINT_ORDER:
    baseline = baselines[(point_type, "baseline")]
    abcd = candidates[(point_type, "abcd")]
    abcd_ilp = candidates[(point_type, "abcd-ilp")]
    lines.append(
        f"- `{point_type}`: `abcd` delta {abcd.rvv_total - baseline.rvv_total:+d}，"
        f"`abcd-ilp` delta {abcd_ilp.rvv_total - baseline.rvv_total:+d}；"
        f"`abcd` 与 `abcd-ilp` RVV 指令统计"
        f"{'相同' if same_instruction_profile(abcd, abcd_ilp) else '不同'}。"
    )
  lines.append("")
  return "\n".join(lines)


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Generate weighted TEPTPLW production-symbol asm attribution."
  )
  parser.add_argument(
      "--profile",
      choices=(
          "production-default-fused-abcd-ilp",
          "production-symbol-fused-abcd-ilp",
          "generic-fused-formula",
      ),
      default="production-default-fused-abcd-ilp",
  )
  parser.add_argument("--asm", required=True, type=Path, help="objdump -d -C full asm file")
  parser.add_argument("--output", type=Path, default=None, help="Markdown output path")
  args = parser.parse_args()

  if args.profile == "production-default-fused-abcd-ilp":
    production_default_attributions = collect_production_default_attributions(args.asm)
    report = format_production_default_report(
        args.asm, args.output, production_default_attributions
    )
  elif args.profile == "production-symbol-fused-abcd-ilp":
    production_counts = collect_production_symbol_counts(args.asm)
    report = format_report(args.asm, args.output, production_counts)
  else:
    production_counts = collect_production_symbol_counts(args.asm)
    generic_counts = collect_generic_fused_formula_counts(args.asm)
    report = format_generic_report(args.asm, args.output, production_counts, generic_counts)
  if args.output is None:
    sys.stdout.write(report)
  else:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(report, encoding="utf-8")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
