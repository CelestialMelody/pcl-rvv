# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production-public`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `positive`
- binary_hash: `sha256:9bada57bcdc513e15cc52536f39091363b8678918fa763754093f7516bebaf59`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD production matchTemplates total | 50190.000000 | 44114.300000 | 1.138x | 1.133x | 1.173x | 0/5 | `positive` |
| LINEMOD production detectTemplates total | 57707.700000 | 51155.400000 | 1.129x | 1.111x | 1.141x | 0/5 | `positive` |

## Repeated Values

### LINEMOD production matchTemplates total

- timer_boundary: `LINEMOD::matchTemplates_public_total_current_source`
- row_source: `production_quantized_modality_public_entry`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `production_match_detections_fnv1a`
- boundary_note: Phase 070 production direct：真实调用当前源码编译出的 `LINEMOD::matchTemplates` 公开入口；计时包含 energy maps、linearized copy、score accumulation 和单个最佳 detection 构造，不覆盖 semi-scale 或 separate-energy 编译分支。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 50190.000000 | 44114.300000 | 1.138x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-02 | 50044.400000 | 43982.100000 | 1.138x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-03 | 51799.900000 | 45728.800000 | 1.133x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-04 | 50122.700000 | 43800.400000 | 1.144x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-05 | 52020.300000 | 44365.600000 | 1.173x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |

### LINEMOD production detectTemplates total

- timer_boundary: `LINEMOD::detectTemplates_public_total_current_source`
- row_source: `production_quantized_modality_public_entry`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `production_detect_detections_fnv1a`
- boundary_note: Phase 070 production direct：真实调用当前源码编译出的 `LINEMOD::detectTemplates` 公开入口；计时包含默认 energy maps、linearized copy、score accumulation、threshold scan 和 detection 构造，不覆盖 NMS、averaged detection、semi-scale 或 separate-energy 编译分支。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 57707.700000 | 51199.800000 | 1.127x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-02 | 57647.600000 | 50694.800000 | 1.137x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-03 | 58331.900000 | 51102.400000 | 1.141x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-04 | 57768.400000 | 51155.400000 | 1.129x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |
| run-05 | 57625.700000 | 51886.200000 | 1.111x | `5083644600596343113` | 4096 | `4025643396097835907` | `not_applicable` | `not_exposed` |


## Boundary

本 summary 只证明上方 `Repeated Values` 中列出的 production public（真实公开入口）timing label：当前源码编译出的 `LINEMOD::matchTemplates` 与 `LINEMOD::detectTemplates` 在默认宏配置下命中 `EnergyMaps -> LinearizedMaps` RVV copy helper 后的整入口耗时。每个 label 的 `boundary_note` 是该项的主证据边界。它不证明 NMS、averaged detection、semi-scale、separate-energy 编译分支或新的 RVV family；若后续结果为弱、负、中性或不稳定，只能降级对应 production-public 边界。
