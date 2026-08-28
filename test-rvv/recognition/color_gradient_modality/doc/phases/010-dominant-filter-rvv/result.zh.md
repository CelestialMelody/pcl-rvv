# Phase 010 Result: dominant filter RVV

## 执行范围

本阶段完成 `dominant-3x3-filter-rvv` 的 production-shaped diagnostic
（生产形态诊断）。实现和证据只覆盖 `filterQuantizedColorGradients()` 的
quantized map -> one-hot filtered map 子链路，不修改 production（生产源码），不证明
`ColorGradientModality::processInputData()` 的真实分流。

## 动作回填

| action | status | evidence |
| --- | --- | --- |
| RED gtest | done | `DominantFilterMatchesProductionTieBreakAndThreshold` 在 candidate API / path 未实现时失败，随后修正测试数据覆盖 5:4 majority 和 4-count below-threshold |
| GREEN helper | done | `filterQuantizedGradientsScalar` 复刻 production histogram / strict greater-than tie-break / `>=5` 阈值；`filterQuantizedGradientsCandidate` 在 RVV build 下用 9 邻域 byte load、8-bin equality count 和 mask store |
| correctness | done | `make -C test-rvv/recognition/color_gradient_modality run_test_compare`，Std/RVV 均 3/3 |
| bench | done | `bench_cgm` 新增 `filter_dominant_320x240` 与 `filter_dominant_641x481_tail` case；本阶段板卡 evidence 使用 320x240 dedicated case |
| asm | done | `make -C test-rvv/recognition/color_gradient_modality check_cgm_filter_rvv_asm` 通过，filtered asm 命中 `vle8` / `vse8` / compare / merge 类指令 |
| board repeated | done | `cgm_phase010_dominant_filter_repeated`，5 runs，20 iterations，3 warmup |
| Evidence Doctor / registry | done | `record_evidence_state_repeated` 生成 summary / manifest / doctor 并登记 registry |

## 板卡结果

证据路径：

- summary: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/summary.md`
- manifest: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/evidence_manifest.json`
- doctor: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase010_dominant_filter/evidence_doctor.md`
- registry: `test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`

| case | runs | median speedup | min | max | B/A < 1 | checksum |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `filter_dominant_320x240` | 5 | `3.480x` | `3.470x` | `3.510x` | `0/5` | Std/RVV 一致：`5318654772487904455` |

Decision bucket（决策桶）为 `positive`。远端 make 每轮出现 clock skew warning
（板卡文件时间戳偏移警告）；本阶段未观察到 checksum、方向或长尾异常，因此记录为
环境风险，不降级当前 diagnostic 结论。若后续 full-chain 出现方向反转，应优先修正
板卡时间、补 binary hash（编译产物指纹）或记录 governor / freq / temperature 后重跑。

## Evidence Doctor

`log/board/repeated_phase010_dominant_filter/evidence_doctor.md` 结果：

- Errors: 0
- Warnings: 0
- Suggestions: 2

Suggestions 为缺少 `taskset / governor / freq / temperature` 环境字段，以及缺少
`binary_hash`。处理策略：不阻塞 Phase 010，因为 repeated direction 稳定、checksum
一致、run_count 达到计划预算；后续 full-chain 或 production direct 阶段应补环境摘要和
binary identity。

## 诊断到 production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper `filterQuantizedGradientsCandidate` vs scalar reference |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 dominant filter 是否值得纳入 full-chain |
| diagnostic 是否可外推到 production | 不能直接外推；它覆盖 filter 子链路，但不包含前置 Sobel/quantize、Gaussian smoothing、spread、feature extraction 或 public entry |
| comparison-boundary / baseline mismatch 风险 | 有；bench wrapper 不是 `ColorGradientModality::processInputData()` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果为 positive；若 full-chain 后续变弱或退化，仍不能直接 no-production，必须按 full-chain 边界审计 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；当前没有 production patch，也没有 production direct evidence |

## 阶段决策

`dominant-3x3-filter-rvv` 标为 `attempted-positive diagnostic`。当前 helper 在板卡上
对 dedicated 320x240 filter case 有稳定正向信号，值得作为 full-chain candidate
的组成部分保留；但本阶段只证明 isolated filter helper，不进入 production integration loop
（生产接入闭环）。

`continue_stop_decision`: continue。

`stop_condition_hit`: none。

`next_phase_default`: `020-full-chain-production-shaped-diagnostic`。

理由：roadmap 和 matrix 仍有当前授权范围内的 high-priority unblocked action，需要把
Sobel+quantize 与 dominant filter 串到同一个 production-shaped helper，观察子链路组合后
是否仍保持正向，以及是否暴露 staging / memory traffic（暂存 / 内存流量）瓶颈。
