# color_modality Benchmark And Evidence

## Bench case 字典

| case | 入口 | 数据 | 计时边界 | 证明点 |
| --- | --- | --- | --- | --- |
| `color_preprocess_320x240` | test-support candidate | synthetic `PointXYZRGB` 320x240 | quantize + filter + spread helper | production-shaped diagnostic，保留作候选对拍 |
| `color_preprocess_641x481_tail` | test-support candidate | synthetic `PointXYZRGB` 641x481 | 同上，覆盖 RVV tail lanes | production-shaped diagnostic tail 行为 |
| `production_process_320x240` | `ColorModality<PointXYZRGB>::processInputData()` | synthetic organized `PointXYZRGB` 320x240 | public entry 内 quantize、filter、spread | 常规尺寸 production direct 性能 |
| `production_process_641x481_tail` | 同上 | synthetic organized `PointXYZRGB` 641x481 | 同上，覆盖非整齐宽度 | RVV tail lanes 下 production direct 性能 |

## 当前正式板卡证据

- summary: `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/summary.md`
- manifest: `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/evidence_manifest.json`
- Evidence Doctor: `test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/evidence_doctor.md`
- registry: `test-rvv/recognition/color_modality/log/evidence_registry.json`
- run label: `cm_phase040_quantize_filter_rvv_repeated`

| case | median speedup | min | max | B/A < 1 |
| --- | ---: | ---: | ---: | ---: |
| `production_process_320x240` | `2.490x` | `2.430x` | `2.510x` | `0/5` |
| `production_process_641x481_tail` | `2.410x` | `2.380x` | `2.430x` | `0/5` |

## Evidence Doctor

Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=0, Suggestions=4`。Suggestions 指向环境 metadata 和 binary hash 缺失；当前 checksum 稳定、无退化 run，因此不阻塞采纳。若后续出现方向反转或长尾，应先补这些 metadata 再重判。

## 提交边界

默认只提交 summary / manifest / doctor / registry 等摘要证据。raw board run logs、`build/`、远端路径和本机配置不进入默认提交边界。
