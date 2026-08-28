# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production_shaped_diagnostic`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `negative`
- binary_hash: `sha256:3a35e0182e61b2ef707a6d5b558b928294d21147269b90aeb0612accda15832a`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD score accumulation | 194.960000 | 227.175000 | 0.862x | 0.825x | 0.889x | 5/5 | `negative` |

## Repeated Values

### LINEMOD score accumulation

- timer_boundary: `score_accumulation_helper_only_no_energy_or_detection`
- row_source: `linearized_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `score_sums_fnv1a`
- boundary_note: 只覆盖 u8 linearized score maps 到 u16 score_sums 的累加核。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| run-01 | 189.350000 | 229.422000 | 0.825x | `9840709576022462159` | not_recorded | `not_recorded` |
| run-02 | 197.333000 | 223.635000 | 0.882x | `9840709576022462159` | not_recorded | `not_recorded` |
| run-03 | 202.322000 | 227.662000 | 0.889x | `9840709576022462159` | not_recorded | `not_recorded` |
| run-04 | 194.960000 | 227.175000 | 0.858x | `9840709576022462159` | not_recorded | `not_recorded` |
| run-05 | 194.162000 | 225.359000 | 0.862x | `9840709576022462159` | not_recorded | `not_recorded` |

## Boundary

本 summary 只证明 already-linearized score maps 的 test helper 边界；不证明 energy map 构建、linearized map 拷贝、NMS、averaged detection 或真实 production dispatch。Phase 010 中 `score scan` 的 RVV 候选还包含一次标量保序 index append，因此负向结果不能直接外推为完整 production no-go。
