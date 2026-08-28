# Phase 010: dominant filter RVV

## 阶段意图和边界

本阶段只覆盖 production helper `filterQuantizedColorGradients()` 的核心语义：
输入是 organized `uint8_t` quantized map（量化方向图，取值 0..8），输出是同尺寸
filtered map（过滤后方向图），内区每个像素统计 3x3 邻域中 1..8 号方向的出现次数；
若最大方向出现次数 `>=5`，输出 `1 << direction_index`，否则输出 0。边框保持 0。

本阶段不修改 production 源码，不处理 Sobel、angle quantize、spread 或
feature extraction，也不证明 `processInputData()` production dispatch。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 上一阶段 | Sobel+quantize diagnostic board repeated 为 positive，production 未接入 | `doc/phases/000-current-state-and-cgm-scaffold/result.zh.md` |
| production 语义 | `filterQuantizedColorGradients()` 对每个内区像素构建 9-bin histogram，按 1..8 顺序用严格 `<` 选最大值，阈值为 `>=5` | `recognition/include/pcl/recognition/color_gradient_modality.h` |
| 测试资产 | 当前只有 Sobel+quantize helper 和 tests | `include/impl/cgm_sobel_quantize.hpp`, `src/test_cgm.cpp` |
| 证据边界 | Phase 010 继续 production-shaped diagnostic，仍不接 production | topic-local test-rvv |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `dominant-3x3-filter-rvv` | 可用 RVV load 周边 3 行、对 8 个方向做 lane-wise equality count，再用 mask threshold 写 one-hot 输出 | 8-bin histogram 逐方向比较较多；小图或缓存主导时可能不如标量；tie-break 必须匹配 production 的严格 `<` | planned |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper scalar reference vs RVV candidate |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 dominant filter 是否值得进入 full-chain |
| diagnostic 是否可外推到 production | unknown；它复刻 filter 子链路，但不包含前置 Sobel/quantize、后置 spread 和 public entry |
| comparison-boundary / baseline mismatch 风险 | yes；bench 只测 isolated filter helper |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许继续做 full-chain diagnostic；不能仅凭 filter 子链路负向拒绝 production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dominant-3x3-filter-rvv` | organized quantized map | `uint8_t` contiguous map, width/height | `computeDominantFilterCandidate` test helper | planned RED/GREEN in `run_test_compare` | planned `bench_cgm` filter case | planned 5-run board repeated if local gates pass | planned `check_cgm_rvv_asm` extension | planned | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED gtest | 新增 filter case，先期望 RVV build 命中 filter path | RVV build 在实现前失败 |
| GREEN helper | `include/impl/cgm_sobel_quantize.hpp` 或后续拆分为 filter header | scalar/RVV helper 输出逐字节一致 |
| bench | `src/bench_cgm.cpp` 新增 filter case | QEMU smoke 只验证日志形状 |
| asm | `check_cgm_rvv_asm` 覆盖 filter 相关 byte load / compare / merge / store | filtered asm 能看到 RVV byte/compare 指令 |
| board repeated | `board_repeated` 或 case-filter 针对 filter | 5-run decision bucket |
| Evidence Doctor / registry | `record_evidence_state_repeated` 或新增 phase010 run label | Errors/Warn/Suggestion 被记录 |

## 板卡复跑预算和决策桶

默认 5 runs，20 iterations，3 warmup。decision bucket：
`positive` 为 median >= 1.20 且 `B/A < 1` 为 0；
`weak_positive` 为 median `[1.05, 1.20)` 且方向稳定；
`neutral` 为 `[0.95, 1.05)`；`negative` 为 `<0.95`；预算内跨桶摇摆为 `unstable`。

## 继续 / 停止条件

若 correctness、asm、board repeated 和 doctor 闭合，继续更新 Phase 010 result、
roadmap、matrix、evaluation 和 Handoff，再进入 full-chain 或 full Sobel RGB stencil
下一 phase。只有工具/板卡不可用、证据矛盾、dirty isolation 不安全或需要 production
授权时允许停止。

## 文档更新清单

更新 Phase 010 result、README phase index、optimization matrix、roadmap、evaluation
和 current handoff。`doc-rvv` 仍不适用，除非后续进入 production integration 并通过 PI5。
