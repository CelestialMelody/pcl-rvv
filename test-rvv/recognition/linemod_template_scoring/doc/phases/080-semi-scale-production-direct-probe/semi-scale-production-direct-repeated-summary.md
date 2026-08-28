# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production-public`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `positive`
- binary_hash: `sha256:a11ed5192759f85aacab784a2af65b0326152e6974a53fcd453a8f1fd6cf2f36`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD production semi-scale detectTemplates total | 57919.800000 | 51021.800000 | 1.136x | 1.130x | 1.141x | 0/5 | `positive` |

## Repeated Values

### LINEMOD production semi-scale detectTemplates total

- timer_boundary: `LINEMOD::detectTemplatesSemiScaleInvariant_public_total_current_source`
- row_source: `production_quantized_modality_public_entry`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `production_semiscale_detections_fnv1a`
- boundary_note: Phase 080 production direct：真实调用当前源码编译出的 `LINEMOD::detectTemplatesSemiScaleInvariant` 公开入口；计时包含默认 energy maps、linearized copy、每个 scale 的 score accumulation、threshold scan 和 detection 构造，不覆盖 separate-energy 编译分支、NMS 或 averaged detection。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 57596.500000 | 50699.500000 | 1.136x | `5083644600596343113` | 4096 | `14884697105373567807` | `not_applicable` | `not_exposed` |
| run-02 | 57919.800000 | 51262.500000 | 1.130x | `5083644600596343113` | 4096 | `14884697105373567807` | `not_applicable` | `not_exposed` |
| run-03 | 58004.900000 | 50822.800000 | 1.141x | `5083644600596343113` | 4096 | `14884697105373567807` | `not_applicable` | `not_exposed` |
| run-04 | 57999.900000 | 51021.800000 | 1.137x | `5083644600596343113` | 4096 | `14884697105373567807` | `not_applicable` | `not_exposed` |
| run-05 | 57862.000000 | 51034.100000 | 1.134x | `5083644600596343113` | 4096 | `14884697105373567807` | `not_applicable` | `not_exposed` |


## Boundary

本 summary 只证明上方 `Repeated Values` 中列出的 production public（真实公开入口）timing label：`LINEMOD production semi-scale detectTemplates total` 在当前源码默认宏配置下命中 `EnergyMaps -> LinearizedMaps` RVV copy helper 后的整入口耗时。每个 label 的 `boundary_note` 是该项的主证据边界。它不证明 NMS、averaged detection、separate-energy 编译分支或新的 RVV family；若后续结果为弱、负、中性或不稳定，只能降级对应 production-public 边界。
