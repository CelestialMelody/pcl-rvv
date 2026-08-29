# color_modality Optimization Evidence

| candidate | production code | test / bench | board evidence | asm | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| RGB extrema quantize RVV | `recognition/include/pcl/recognition/color_modality.h::quantizeColorsRVV()` | `run_test_compare`、`production_process_*` | `repeated_phase040_quantize_filter_rvv/summary.md` | `vlse8` byte field loads and integer arithmetic in RVV build | adopted | exact `PointXYZRGB` only |
| 3x3 dominant filter RVV | `filterQuantizedColorsRVV()` | `run_test_compare`、`production_process_*` | 同 Phase 040 summary | `vle8/vse8/vmseq/vmsgtu/vmerge` | adopted | uses quantized byte map |
| Spread | `QuantizedMap::spreadQuantizedMap()` | production_process timer includes call | covered indirectly by production_process checksum/timing | owned by quantizable_modality topic | adopted external dependency | 本 topic 不修改 shared spread helper |
| `extractFeatures()` | unchanged | no dedicated test in this topic | none | none | deferred with evidence | 需要 profile / component ablation |
| `computeDistanceMap()` | unchanged | no dedicated test in this topic | none | none | deferred with evidence | 递推状态复杂，需单独算法审计 |

## 采纳说明

Phase 040 是 production direct（真实生产路径）证据，不再只是 diagnostic（诊断）。用户授权“板卡有收益即可采纳”，而当前 repeated board 显示两组 public entry case 都远高于 positive 门槛且无退化 run，所以 quantize+filter family 作为当前 production adopted path 保留。
