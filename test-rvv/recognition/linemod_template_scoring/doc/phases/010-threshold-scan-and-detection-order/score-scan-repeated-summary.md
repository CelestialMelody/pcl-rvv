# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production_shaped_diagnostic`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `negative`
- binary_hash: `sha256:5e1c6206be90fc10a831a6af0fed04ed2a9cd5c7bc7cbfd185ca801a6d07891c`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD score accumulation | 194.544000 | 230.582000 | 0.842x | 0.839x | 0.859x | 5/5 | `negative` |
| LINEMOD score scan | 17.037300 | 28.362300 | 0.601x | 0.595x | 0.605x | 5/5 | `negative` |
| LINEMOD accumulate+scan | 210.102000 | 239.401000 | 0.879x | 0.872x | 0.963x | 5/5 | `negative` |

## Repeated Values

### LINEMOD score accumulation

- timer_boundary: `score_accumulation_helper_only_no_energy_or_detection`
- row_source: `linearized_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `score_sums_fnv1a`
- boundary_note: 只覆盖 u8 linearized score maps 到 u16 score_sums 的累加核。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| run-01 | 194.544000 | 231.699000 | 0.840x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-02 | 197.982000 | 230.582000 | 0.859x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-03 | 194.284000 | 230.119000 | 0.844x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-04 | 194.852000 | 232.203000 | 0.839x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-05 | 193.503000 | 229.779000 | 0.842x | `9840709576022462159` | 1638 | `3938728128246843911` |

### LINEMOD score scan

- timer_boundary: `score_scan_threshold_count_max_and_candidate_index_sink`
- row_source: `linearized_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `score_scan_summary_and_order_fnv1a`
- boundary_note: 覆盖 threshold count、max 最早位置和候选 index 保序写入，不覆盖 NMS 或 detection 对象构造。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| run-01 | 16.902900 | 28.410200 | 0.595x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-02 | 17.069200 | 28.319800 | 0.603x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-03 | 16.912900 | 28.362300 | 0.596x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-04 | 17.124600 | 28.317100 | 0.605x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-05 | 17.037300 | 28.364600 | 0.601x | `9840709576022462159` | 1638 | `3938728128246843911` |

### LINEMOD accumulate+scan

- timer_boundary: `score_accumulation_then_score_scan_no_energy_or_nms`
- row_source: `linearized_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `score_sums_and_scan_summary_fnv1a`
- boundary_note: 覆盖累加后立刻扫描的组合成本，不覆盖 energy map、linearized copy、NMS 或真实 production dispatch。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| run-01 | 208.777000 | 239.491000 | 0.872x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-02 | 210.102000 | 239.401000 | 0.878x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-03 | 210.856000 | 239.908000 | 0.879x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-04 | 229.446000 | 238.291000 | 0.963x | `9840709576022462159` | 1638 | `3938728128246843911` |
| run-05 | 210.055000 | 233.241000 | 0.901x | `9840709576022462159` | 1638 | `3938728128246843911` |

## Boundary

本 summary 只证明 already-linearized score maps 的 test helper 边界；不证明 energy map 构建、linearized map 拷贝、NMS、averaged detection 或真实 production dispatch。Phase 010 中 `score scan` 的 RVV 候选还包含一次标量保序 index append，因此负向结果不能直接外推为完整 production no-go。
