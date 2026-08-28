# LINEMOD Score Accumulation Repeated Board Summary

## Context

- evidence_role: `production_shaped_diagnostic`
- case_kind: `production-shaped`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `positive`
- timer_boundary: `score_accumulation_helper_only_no_energy_or_detection`
- row_source: `linearized_map_byte_stream`
- asm_boundary: `accumulateScoreMapsRVV or inlined bench helper; required RVV mnemonics present`
- binary_hash: `sha256:b2cd53055b12ad0a1390b27f73ee97e0568af3934a7dca4636e722ef2d2287ba`

## Repeated Values

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| run-01 | 389.759000 | 228.749000 | 1.704x | `9840709576022462159` |
| run-02 | 291.746000 | 226.768000 | 1.287x | `9840709576022462159` |
| run-03 | 444.949000 | 227.406000 | 1.957x | `9840709576022462159` |
| run-04 | 316.838000 | 225.652000 | 1.404x | `9840709576022462159` |
| run-05 | 292.119000 | 227.005000 | 1.287x | `9840709576022462159` |

## Statistics

- std_us_median: `316.838000`
- rvv_us_median: `227.005000`
- speedup_median: `1.404x`
- speedup_min: `1.287x`
- speedup_max: `1.957x`
- degrade_frequency: `0/5`

## Boundary

本 summary 只证明 already-linearized score maps 的 test helper 边界；不证明 energy map 构建、linearized map 拷贝、threshold scan、detection 输出顺序或真实 production dispatch。
