#!/usr/bin/env python3
"""Generate a topic-local Evidence Doctor manifest for VFH board compare logs."""

from __future__ import annotations

import argparse
import json
import re
import statistics
from pathlib import Path


CASE_RE = re.compile(
    r"^(?P<name>[A-Za-z0-9_]+)\s+\|\s+Std\s+\|\s+(?P<std_ms>[0-9.]+)\s+ms\s+\|.*?\n"
    r"\s+\|\s+RVV\s+\|\s+(?P<rvv_ms>[0-9.]+)\s+ms\s+\|.*?\[\s*(?P<speedup>[0-9.]+)x\s*\]",
    re.MULTILINE,
)
CHECKSUM_RE = re.compile(r"^(?P<name>[A-Za-z0-9_]+) checksum:\s+(?P<checksum>[-+0-9.eE]+)\s*$", re.MULTILINE)


def comparison_metadata(name: str) -> dict[str, object]:
  if name == "candidate_vfh_centroid_spfh_rvv":
    return {
        "boundary": "test helper",
        "case_kind": "component-ablation",
        "evidence_role": "diagnostic",
        "group": "vfh-centroid-spfh-rvv",
        "point_type": "PointNormal -> VFHSignature308",
        "gate": "dense finite PointNormal input, default VFH bins",
        "timer_boundary": "computeVFHSignatureCentroidSPFHRVV including scalar centroid and histogram scatter",
        "row_source": "sequential full-cloud indices",
        "reduction": "RVV centroid-to-point tuple math with scalar ordered histogram scatter",
        "asm_boundary": "computeVFHSignatureCentroidSPFHRVV or inlined accumulateSPFHCentroidPairMathRVV",
        "notes": "Test-only VFH candidate; it does not prove production dispatch.",
    }
  if name == "candidate_vfh_spfh_viewpoint_rvv":
    return {
        "boundary": "test helper",
        "case_kind": "component-ablation",
        "evidence_role": "diagnostic",
        "group": "vfh-spfh-viewpoint-rvv",
        "point_type": "PointNormal -> VFHSignature308",
        "gate": "dense finite PointNormal input, default VFH bins",
        "timer_boundary": "computeVFHSignatureSPFHAndViewpointRVV including scalar centroid and ordered histogram scatter",
        "row_source": "sequential full-cloud indices",
        "reduction": "RVV centroid-to-point tuple math plus RVV viewpoint normal-dot preparation; scalar ordered histogram scatter",
        "asm_boundary": "computeVFHSignatureSPFHAndViewpointRVV or inlined accumulateViewpointRVV",
        "notes": "Test-only combined VFH candidate; it narrows the production probe scope but does not prove production dispatch.",
    }
  if name == "candidate_vfh_centroids_spfh_viewpoint_rvv":
    return {
        "boundary": "test helper",
        "case_kind": "component-ablation",
        "evidence_role": "diagnostic",
        "group": "vfh-centroids-spfh-viewpoint-rvv",
        "point_type": "PointNormal -> VFHSignature308",
        "gate": "dense finite PointNormal input, default VFH bins",
        "timer_boundary": "computeVFHSignatureCentroidsSPFHAndViewpointRVV including RVV normal centroid reduction and scalar histogram scatter",
        "row_source": "sequential full-cloud indices",
        "reduction": "common compute3DCentroid plus RVV normal centroid reduction, RVV tuple/viewpoint preparation, scalar ordered histogram scatter",
        "asm_boundary": "computeVFHSignatureCentroidsSPFHAndViewpointRVV or inlined computeNormalCentroidRVV",
        "notes": "Test-only reduction-combined VFH candidate; xyz centroid may already use common RVV in RVV builds, so new evidence mainly isolates normal centroid reduction.",
    }
  if name == "public_vfh_compute_baseline":
    return {
        "boundary": "public overload",
        "case_kind": "production-public-baseline",
        "evidence_role": "production_public_baseline",
        "group": "vfh-public-compute",
        "point_type": "PointNormal -> VFHSignature308",
        "gate": "dense finite PointNormal input, default VFH bins",
        "timer_boundary": "VFHEstimation::compute full public path",
        "row_source": "sequential full-cloud indices",
        "reduction": "centroid, normal centroid, SPFH histogram and viewpoint histogram",
        "asm_boundary": "VFHEstimation::compute public path",
        "notes": "Public dilution baseline before any production RVV dispatch exists.",
    }
  if name == "production_vfh_compute_default":
    return {
        "boundary": "public overload",
        "case_kind": "production-public",
        "evidence_role": "production-public",
        "group": "vfh-production-compute",
        "point_type": "PointNormal -> VFHSignature308",
        "gate": "dense finite PointNormal input, default VFH bins, full-cloud sequential indices",
        "timer_boundary": "VFHEstimation::compute full public path after bounded RVV dispatch",
        "row_source": "sequential full-cloud indices",
        "reduction": "production RVV helper for centroid/normal reductions, tuple preparation and viewpoint preparation; scalar ordered histogram scatter",
        "asm_boundary": "pcl::detail::computeVFHSignatureRVV or inlined production helper inside VFHEstimation::computeFeature",
        "notes": "Production-public VFH evidence after PI2 patch; this is the case used for adoption decisions.",
    }
  return {
      "boundary": "test helper",
      "case_kind": "component-ablation",
      "evidence_role": "diagnostic_baseline",
      "group": "vfh-reference",
      "point_type": "PointNormal -> VFHSignature308",
      "gate": "dense finite PointNormal input, default VFH bins",
      "timer_boundary": "computeVFHSignatureReference",
      "row_source": "sequential full-cloud indices",
      "reduction": "scalar reference",
      "asm_boundary": "computeVFHSignatureReference",
      "notes": "Scalar reference baseline for same-chain comparison.",
  }


def parse_summary(path: Path) -> list[dict[str, object]]:
  text = path.read_text(encoding="utf-8")
  std_checksums = parse_checksums(path.with_name("run_bench_std.log"))
  rvv_checksums = parse_checksums(path.with_name("run_bench_rvv.log"))
  return [
      {
          "name": match.group("name"),
          "std_ms": float(match.group("std_ms")),
          "rvv_ms": float(match.group("rvv_ms")),
          "speedup": float(match.group("speedup")),
          "summary_path": str(path),
          "std_checksum": std_checksums.get(match.group("name")),
          "rvv_checksum": rvv_checksums.get(match.group("name")),
      }
      for match in CASE_RE.finditer(text)
  ]


def parse_checksums(path: Path) -> dict[str, str]:
  if not path.exists():
    return {}
  text = path.read_text(encoding="utf-8")
  return {match.group("name"): match.group("checksum") for match in CHECKSUM_RE.finditer(text)}


def find_summary_paths(summary: Path | None, summary_dir: Path | None) -> list[Path]:
  if summary is not None:
    return [summary]
  if summary_dir is None:
    raise ValueError("either --summary or --summary-dir is required")
  paths = sorted(summary_dir.glob("run_*/analyze_bench_compare.log"))
  if not paths:
    raise ValueError(f"no run_*/analyze_bench_compare.log files found under {summary_dir}")
  return paths


def build_comparison(name: str, rows: list[dict[str, object]]) -> dict[str, object]:
  meta = comparison_metadata(name)
  std_values = [float(row["std_ms"]) for row in rows]
  rvv_values = [float(row["rvv_ms"]) for row in rows]
  speedups = [float(row["speedup"]) for row in rows]
  std_checksums = [str(row["std_checksum"]) for row in rows if row.get("std_checksum") is not None]
  rvv_checksums = [str(row["rvv_checksum"]) for row in rows if row.get("rvv_checksum") is not None]
  side_common = {
      "boundary": meta["boundary"],
      "wrapper": "test-rvv/features/vfh/src/bench_vfh.cpp",
      "row_source": meta["row_source"],
      "solve": False,
      "checksum_policy": "VFH descriptor checksum line",
      "checksum": "see matching checksum lines in run_bench_std.log and run_bench_rvv.log",
      "timer_boundary": meta["timer_boundary"],
      "gate": meta["gate"],
      "mask": "dense finite synthetic input; finite fallback not covered in this phase",
      "reduction": meta["reduction"],
      "asm_boundary": meta["asm_boundary"],
  }
  return {
      "name": name,
      "label": name,
      "group": meta["group"],
      "evidence_role": meta["evidence_role"],
      "case_kind": meta["case_kind"],
      "point_type": meta["point_type"],
      "size": "side/points from dataset line",
      "run_count": len(speedups),
      "iterations": 8,
      "warmup_iterations": 2,
      "build_comparison": True,
      "checksum_values": {"std": std_checksums, "rvv": rvv_checksums},
      "std_ms": statistics.mean(std_values),
      "rvv_ms": statistics.mean(rvv_values),
      "ba_values": speedups,
      "std_ms_values": std_values,
      "rvv_ms_values": rvv_values,
      "run_paths": [str(row["summary_path"]) for row in rows],
      "baseline": {
          **side_common,
          "label": "Std build",
          "std_ms": statistics.mean(std_values),
          "rvv_ms": statistics.mean(std_values),
          "checksum": std_checksums,
      },
      "candidate": {
          **side_common,
          "label": "RVV build",
          "std_ms": statistics.mean(std_values),
          "rvv_ms": statistics.mean(rvv_values),
          "checksum": rvv_checksums,
      },
      "notes": meta["notes"],
  }


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("--summary", type=Path)
  parser.add_argument("--summary-dir", type=Path)
  parser.add_argument("--output", required=True, type=Path)
  args = parser.parse_args()

  grouped: dict[str, list[dict[str, object]]] = {}
  for path in find_summary_paths(args.summary, args.summary_dir):
    for row in parse_summary(path):
      grouped.setdefault(str(row["name"]), []).append(row)
  if not grouped:
    raise ValueError("no benchmark comparisons parsed")

  has_production_public = "production_vfh_compute_default" in grouped
  manifest = {
      "schema_version": 1,
      "summary": {
          "title": "VFH repeated board production probe" if has_production_public else "VFH repeated board diagnostic",
          "evidence_role": "production-public" if has_production_public else "diagnostic",
          "summary_path": str(args.summary_dir or args.summary),
      },
      "checks": {"strict_ab": False},
      "comparisons": [build_comparison(name, rows) for name, rows in sorted(grouped.items())],
  }
  args.output.parent.mkdir(parents=True, exist_ok=True)
  args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
  print(f"wrote manifest: {args.output}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
