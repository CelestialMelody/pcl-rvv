# Phase 020: full-chain production-shaped diagnostic

## 阶段意图和边界

本阶段串接已有 `sobel-selected-angle-quantize-rvv` 和
`dominant-3x3-filter-rvv`，形成一个 test helper 边界内的 full-chain
production-shaped diagnostic（生产形态诊断）：输入 organized RGB，先生成 quantized map，
再执行 dominant filter，输出 filtered one-hot map。

本阶段不修改 production 源码，不调用真实 `ColorGradientModality::processInputData()`，
不包含 Gaussian smoothing、`QuantizedMap::spreadQuantizedMap()` 或 `extractFeatures()`。
本阶段只判断两个已正向子链路组合后是否仍有板卡正向信号，以及是否值得后续进入 bounded
production integration plan（有界生产接入计划）。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| Phase 000 | Sobel+quantize diagnostic repeated board 为 positive，两个规模 checksum 一致 | `doc/phases/000-current-state-and-cgm-scaffold/result.zh.md` |
| Phase 010 | dominant filter diagnostic repeated board 为 positive，`filter_dominant_320x240` checksum 一致 | `doc/phases/010-dominant-filter-rvv/result.zh.md` |
| 生产状态 | production 未修改；`doc-rvv` 仍不适用 | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| 证据登记 | Phase 000 / 010 summary、manifest、doctor 已登记 | `log/evidence_registry.json` |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `full-sobel-quantize-filter-rvv` | Phase 000 的 RVV angle/quantize 与 Phase 010 的 RVV filter 串接后仍能在 board 上正向 | 中间 `GradientCell` vector 和 quantized map 提取会增加内存流量；没有 Gaussian / spread，不能外推完整 production | planned |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper scalar full subchain vs RVV candidate full subchain |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断组合子链路是否支持进入 PI1 production integration plan |
| diagnostic 是否可外推到 production | unknown；它更接近 `processInputData()` 中 Sobel+quantize+filter 的连续片段，但仍缺 Gaussian、spread、对象状态和 public entry |
| comparison-boundary / baseline mismatch 风险 | yes；Std/RVV 两侧共享 test helper wrapper，但 wrapper 不等于 production public overload |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许；弱/负结果只说明当前 helper 组合不支持直接升级，需要先做 mismatch audit 或 component ablation |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；若进入 production，PI5 后还需用户确认采纳 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `full-sobel-quantize-filter-rvv` | organized RGB image -> quantized map | `pcl::RGB`, `GradientCell` staging, `uint8_t` filtered map | planned `computeSobelQuantizedFilteredCandidate` test helper | planned RED/GREEN in `run_test_compare` | planned `full_chain_*` bench cases | planned 5-run board repeated if local gates pass | planned `check_cgm_full_chain_rvv_asm` | planned | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED gtest | 新增 full-chain same-chain test，先引用 planned helper | RVV build 在 helper 未实现前失败 |
| GREEN helper | `include/impl/cgm_sobel_quantize.hpp` 增加 scalar/candidate 串接 helper | scalar/RVV 输出逐字节一致，RVV build 命中 full-chain path |
| bench | `src/bench_cgm.cpp` 新增 `full_chain_320x240` 与 `full_chain_641x481_tail` | QEMU smoke 只验证日志形状 |
| asm | 新增 `check_cgm_full_chain_rvv_asm` | 同一个 RVV bench asm 中同时命中 float math 和 byte filter 指令 |
| board repeated | `board_repeated` case-filter `full_chain_320x240,full_chain_641x481_tail` | 5-run decision bucket |
| Evidence Doctor / registry | `record_evidence_state_repeated` 使用 phase020 run label | Errors/Warn/Suggestion 被记录 |

## 板卡复跑预算和决策桶

默认 5 runs，20 iterations，3 warmup。decision bucket：
`positive` 为 median >= 1.20 且 `B/A < 1` 为 0；
`weak_positive` 为 median `[1.05, 1.20)` 且方向稳定；
`neutral` 为 `[0.95, 1.05)`；`negative` 为 `<0.95`；预算内跨桶摇摆为
`unstable`。若 Phase 020 方向与 Phase 000/010 单链路明显冲突，不无限复跑，先记录为
component interaction（组件交互）风险并转入消融计划。

## 继续 / 停止条件

若 correctness、asm、board repeated 和 Evidence Doctor 闭合，更新 result、matrix、roadmap、
evaluation 和 Handoff。若 full-chain 为 positive，可把下一默认阶段设为 PI1 production
integration plan；若 weak/negative/unstable，则下一阶段优先 component ablation 或 full Sobel
RGB stencil，不直接下 no-production 结论。

只有工具/板卡不可用、证据矛盾、dirty isolation 不安全、或继续需要实际修改 production 源码且未到
PI1 gate 时允许停止。

## 文档更新清单

更新 Phase 020 result、phase index、optimization matrix、roadmap、evaluation 和 current handoff。
`doc-rvv` 仍不适用，除非后续进入 production integration 并通过 PI5 且用户确认采纳。
