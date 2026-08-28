# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production_shaped_diagnostic`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `positive`
- binary_hash: `sha256:c9513a6a03c8b4ae3fc79f7b8c1056be74c4fc125a94bd370750020a8f9200b5`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD linearized map copy | 2248.440000 | 1023.700000 | 2.160x | 2.033x | 2.266x | 0/5 | `positive` |

## Repeated Values

### LINEMOD linearized map copy

- timer_boundary: `single_energy_map_to_64_linearized_offset_maps`
- row_source: `energy_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `linearized_maps_fnv1a`
- boundary_note: 只覆盖 synthetic single-bin energy map 到 64 个 step=8 linearized offset maps；不覆盖 EnergyMaps / LinearizedMaps 对象分配、getOffsetMap 后续读取、score accumulation、NMS 或真实 production dispatch。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 2271.910000 | 1051.790000 | 2.160x | `12986699280141894531` | not_recorded | `not_recorded` | `2092149027815752579` | `18079035698387387267` |
| run-02 | 2201.520000 | 1023.700000 | 2.151x | `12986699280141894531` | not_recorded | `not_recorded` | `2092149027815752579` | `18079035698387387267` |
| run-03 | 2253.120000 | 994.280000 | 2.266x | `12986699280141894531` | not_recorded | `not_recorded` | `2092149027815752579` | `18079035698387387267` |
| run-04 | 2248.440000 | 1012.070000 | 2.222x | `12986699280141894531` | not_recorded | `not_recorded` | `2092149027815752579` | `18079035698387387267` |
| run-05 | 2222.100000 | 1093.140000 | 2.033x | `12986699280141894531` | not_recorded | `not_recorded` | `2092149027815752579` | `18079035698387387267` |

## Boundary

本 summary 只证明上方 `Repeated Values` 中列出的 timing label 对应的 test helper 边界；每个 label 的 `boundary_note` 是该项的主证据边界。它不证明未列出的 LINEMOD 子阶段、NMS、averaged detection 或真实 production dispatch。若某项结果为弱、负、中性或不稳定，只能降级该 diagnostic boundary，不能直接外推为完整 production no-go。
