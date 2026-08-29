# color_gradient_dot_modality benchmark 与证据

本文件做什么：
这里记录 bench case、summary、manifest、Evidence Doctor 和 registry 的关系。性能结论只来自 board
repeated（板卡重复采集），QEMU 只做 correctness / log-shape smoke。

## Bench case 字典

| case | 入口 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| `dominant_map_320x240` | `computeDominantMapCandidate()` | 只看 dominant-map helper 的 helper 级形态。 | 不能替代 production direct。 |
| `dominant_map_641x481_tail` | `computeDominantMapCandidate()` | 观察 tail lane 与较大尺寸的 helper 行为。 | 不能替代 production direct。 |
| `process_input_320x240` | `ColorGradientDOTModality<PointXYZRGB>::processInputData()` | 真实 public entry 的 production direct 计时。 | 不能外推到未验证点型。 |
| `process_input_641x481_tail` | `ColorGradientDOTModality<PointXYZRGB>::processInputData()` | 真实 public entry 的 tail case 计时。 | 不能外推到其它 layout / scalar。 |

## 当前证据路径

| 入口 | 路径 | 角色 |
| --- | --- | --- |
| summary | `test-rvv/recognition/color_gradient_dot_modality/log/board/repeated_phase010_production_direct/summary.md` | repeated board summary |
| manifest | `test-rvv/recognition/color_gradient_dot_modality/log/board/repeated_phase010_production_direct/evidence_manifest.json` | machine-readable evidence manifest |
| doctor | `test-rvv/recognition/color_gradient_dot_modality/log/board/repeated_phase010_production_direct/evidence_doctor.md` | Evidence Doctor report |
| registry | `test-rvv/recognition/color_gradient_dot_modality/log/evidence_registry.json` | freshness / recovery registry |

## 计时边界

- `run_bench_rvv` 的计时边界是 `bench_cgdm` 内的 case body。
- `run_qemu_smoke` 只证明能跑通和日志形状，不写成速度结论。
- board repeated 结果包含 warmup、重复采样和 checksum 一致性检查。

## 证据角色

- helper bench 只说明 candidate 形态。
- production direct board summary 才能证明当前 public entry 在板卡上的收益。
- Evidence Doctor 的 Errors / Warnings / Suggestions 不能被跳过；它们决定是继续、降级还是收口。

## 提交边界

默认 `topic-only` 提交保留源码、测试支撑、bench wrapper、脚本和文档，不强行提交 `log/` 下的本机证据文件。
如果需要 `topic-plus-sanitized-logs`，只加入被文档引用的 summary / manifest / doctor / registry；raw board
logs 和 QEMU run logs 继续排除。
