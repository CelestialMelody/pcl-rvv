# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production_shaped_diagnostic`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `negative`
- binary_hash: `sha256:bcc0d88f6649e08bfba24b71f494027206f7ed5b7b92791bead68b721938a5ae`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD energy map generation | 25.657300 | 26.902700 | 0.954x | 0.947x | 0.963x | 5/5 | `negative` |

## Repeated Values

### LINEMOD energy map generation

- timer_boundary: `quantized_byte_stream_to_default_merged_energy_maps`
- row_source: `quantized_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `energy_maps_fnv1a`
- boundary_note: 只覆盖 synthetic QuantizedMap 字节流到默认合并 energy maps；不覆盖 separate-energy 编译分支、LinearizedMaps 拷贝或真实 production dispatch。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 25.892900 | 26.880600 | 0.963x | `12986699280141894531` | not_recorded | `not_recorded` | `12569223794094713363` | `not_recorded` |
| run-02 | 25.947100 | 26.979400 | 0.962x | `12986699280141894531` | not_recorded | `not_recorded` | `12569223794094713363` | `not_recorded` |
| run-03 | 25.513800 | 26.933300 | 0.947x | `12986699280141894531` | not_recorded | `not_recorded` | `12569223794094713363` | `not_recorded` |
| run-04 | 25.657300 | 26.902700 | 0.954x | `12986699280141894531` | not_recorded | `not_recorded` | `12569223794094713363` | `not_recorded` |
| run-05 | 25.449800 | 26.741000 | 0.952x | `12986699280141894531` | not_recorded | `not_recorded` | `12569223794094713363` | `not_recorded` |

## Boundary

本 summary 只证明上方 `Repeated Values` 中列出的 timing label 对应的 test helper 边界；每个 label 的 `boundary_note` 是该项的主证据边界。它不证明未列出的 LINEMOD 子阶段、NMS、averaged detection 或真实 production dispatch。若某项结果为弱、负、中性或不稳定，只能降级该 diagnostic boundary，不能直接外推为完整 production no-go。
