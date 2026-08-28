# LINEMOD Template Scoring Repeated Board Summary

## Context

- evidence_role: `production_shaped_diagnostic`
- device: `Milkv-Jupiter`
- iterations: `200`
- warmup_iterations: `5`
- run_count: `5`
- decision_bucket: `positive`
- binary_hash: `sha256:29d2c916ed4667515079f68b81a2b9f2c1e0b496f3f7a1c23b181df1ef4b3012`

## Statistics

| benchmark | Std median us/iter | RVV median us/iter | speedup median | speedup min | speedup max | degrade frequency | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| LINEMOD full-chain total | 23995.100000 | 15330.100000 | 1.565x | 1.500x | 1.624x | 0/5 | `positive` |
| LINEMOD full-chain linearized map copy | 20662.000000 | 11555.500000 | 1.790x | 1.769x | 1.840x | 0/5 | `positive` |

## Repeated Values

### LINEMOD full-chain total

- timer_boundary: `full_chain_split_energy_linearize_accumulate_scan_no_nms`
- row_source: `synthetic_quantized_map_to_linearized_score_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `full_chain_scores_energy_linearized_and_scan_fnv1a`
- boundary_note: Phase 050 的主判定项：包含 energy generation、8-bin linearization、score accumulation 和 threshold scan；只允许 linearized copy 在 RVV build 中变化，不覆盖 NMS、averaged detection、semi-scale 或真实 production dispatch。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 23991.200000 | 15330.100000 | 1.565x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-02 | 23995.100000 | 15995.700000 | 1.500x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-03 | 24004.400000 | 14905.600000 | 1.610x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-04 | 24009.800000 | 14781.400000 | 1.624x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-05 | 23991.400000 | 15817.700000 | 1.517x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |

### LINEMOD full-chain linearized map copy

- timer_boundary: `full_chain_split_8_bin_energy_maps_to_linearized_maps`
- row_source: `energy_map_byte_stream`
- asm_boundary: `linemod template scoring bench helper or inlined candidate; required RVV mnemonics present`
- checksum_policy: `full_chain_linearized_maps_fnv1a`
- boundary_note: Phase 050 的核心候选：8 个 bin-major energy maps 到 8*64 个 linearized maps；只替换拷贝核，不覆盖 LinearizedMaps 对象分配、getOffsetMap、NMS 或真实 production dispatch。

| run | Std us/iter | RVV us/iter | std/RVV speedup | checksum | count_above_threshold | index_checksum | energy_checksum | linearized_checksum |
| --- | ---: | ---: | ---: | --- | ---: | --- | --- | --- |
| run-01 | 20682.100000 | 11555.500000 | 1.790x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-02 | 20662.000000 | 11667.800000 | 1.771x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-03 | 20707.400000 | 11373.100000 | 1.821x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-04 | 20601.400000 | 11195.700000 | 1.840x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |
| run-05 | 20544.900000 | 11614.700000 | 1.769x | `4830927854896129411` | 4096 | `17982530146836431747` | `11595828835197036419` | `13774286969167833219` |

## Boundary

本 summary 只证明上方 `Repeated Values` 中列出的 timing label 对应的 test helper 边界；每个 label 的 `boundary_note` 是该项的主证据边界。它不证明未列出的 LINEMOD 子阶段、NMS、averaged detection 或真实 production dispatch。若某项结果为弱、负、中性或不稳定，只能降级该 diagnostic boundary，不能直接外推为完整 production no-go。
