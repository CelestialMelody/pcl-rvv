# Phase 003 Plan: spfh-pair-feature-batch audit

## 阶段意图和边界

本阶段只审计 `computePointSPFHSignature` 的 pair-feature batch（点对特征批处理）是否值得进入 RVV candidate。
目标是判断可行性、风险和下一步，不直接修改 production（生产源码），不改 `features/src/pfh.cpp` 共享 helper，
不接 `fpfh_omp.hpp`。

本阶段允许读取并评估现有 `common/include/pcl/common/impl/rvv_math.hpp` 中的 `acos_RVV_f32m2` 和
`atan2_RVV_f32m2`，但只有在输入域、fallback（回退路径）、histogram scatter（直方图离散累加）和 same-chain
correctness（同构链路正确性）计划闭合后，才进入实现。

## 当前状态清单

| area | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| weighted FPFH | Phase 002 已 adopted with bounded scope。 | `002-weighted-spfh-33-production-probe/result.zh.md` |
| SPFH component | `component_spfh_signature` 在 Phase 002 repeated board 中 avg 1.010x，Doctor near-threshold suggestion。 | `log/board/pi1-production-weighted/repeated/evidence_manifest.json` |
| math helper | common RVV math 中已有 `acos_RVV_f32m2` 和 `atan2_RVV_f32m2`，但需要审计语义和调用方输入域。 | `common/include/pcl/common/impl/rvv_math.hpp`, `common/include/pcl/common/common.h` |
| production source | `computePointSPFHSignature` 逐邻居调用 `computePairFeatures`，然后标量 bin clamp 和 histogram 累加。 | `features/include/pcl/features/impl/fpfh.hpp`, `features/src/pfh.cpp` |

## 假设与候选族

| candidate family | 假设 | 风险 / 未知 | 本阶段判据 |
| --- | --- | --- | --- |
| `spfh-pair-feature-rvv-math-batch` | 批量计算 Darboux frame 中的 dot/cross/norm/`atan2`，减少 pair feature 热点。 | 现有 helper 是 finite-domain fast approximation（有限域快速近似）还是 strict libm replacement（严格 libm 替换）需要确认；SPFH bin 边界对角度误差敏感。 | 能写出输入域、误差预算、same-chain reference、fallback 和 board bench 计划。 |
| `spfh-histogram-scatter-batch` | 用 RVV 计算 bin index，再标量或分组 scatter 累加 histogram。 | 11-bin histogram 存在冲突累加，直接 scatter 会改变顺序或丢失冲突；可能需要 staging，额外内存流量可能抵消收益。 | 必须先证明 scatter 方案保持标量语义或明确保留标量 tail。 |
| `component-only-no-production` | 若 math/scatter 风险高或预计收益不足，保留 Phase 002 weighted helper，pair-feature 暂缓。 | 可能错过 SPFH 主成本。 | 审计给出明确 blocker 或下一 phase 条件，而不是泛泛放弃。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段是 `diagnostic` / `production-shaped diagnostic`，不作为 production adoption evidence。 |
| A/B boundary | `computePointSPFHSignature` component helper 和 test-rvv bench case。 |
| 当前决策问题 | `implementation-shape`：是否值得实现 pair-feature RVV candidate。 |
| diagnostic 是否可外推到 production | 不能直接外推；它只证明 SPFH component 中的候选形态。 |
| comparison-boundary / baseline mismatch 风险 | 高。`computePointSPFHSignature` 在 public path 中被 search、SPFH lookup 和 weighted helper 稀释。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许。本阶段若没有强 correctness 和 board 证据，只能暂缓。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 若未来接 production，需要先有 production direct tests；当前阶段不采纳。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 source audit | 阅读 `features/src/pfh.cpp`、`fpfh.hpp` 中 pair feature 与 bin scatter 流程。 | 写清标量公式、输入域和哪些部分可批处理。 |
| A2 math helper audit | 阅读 `rvv_math.hpp` 和 `common.h` 中 `acos` / `atan2` RVV helper。 | 明确 helper 合同是否匹配 `computePairFeatures` 的输入域。 |
| A3 candidate decision | 决定本阶段是否进入 test-only candidate 实现。 | 若实现，先补 plan 修订；若暂缓，写 result 并给恢复条件。 |
| A4 文档同步 | 更新 phase result、matrix、roadmap、evaluation。 | 不影响 Phase 002 adopted production patch。 |

## 板卡复跑预算和决策桶

本阶段如果只做审计，不跑板卡。若后续实现 candidate，必须先更新 plan 并采用 5-run repeated board budget：

- `strong_positive`：component SPFH candidate 明显高于 scalar，0/5 degradation，correctness 和 Doctor 无 Error。
- `neutral_or_negative`：接近 1.0 或退化，不能进入 production probe。
- `unstable`：方向摇摆或 Doctor 指出 A/B 边界异常，降级为 diagnostic only。

## 继续 / 停止条件

合法停止条件：

- 数学 helper 合同不匹配，且本阶段实现会改变 bin 边界语义。
- histogram scatter 需要复杂 staging，当前没有足够证据说明收益能覆盖维护成本。
- candidate 需要修改 shared `features/src/pfh.cpp` 或 public API，超出本阶段授权。

否则，默认继续到 test-only component candidate plan。

## 文档更新清单

- 本阶段 result。
- `doc/phases/optimization-matrix.zh.md`。
- `doc/optimization-roadmap.zh.md`。
- 必要时更新 `doc/fpfh-evaluation.zh.md` 的后续方向。
