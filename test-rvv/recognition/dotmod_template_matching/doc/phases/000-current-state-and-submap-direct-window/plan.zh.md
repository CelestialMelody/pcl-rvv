# Phase 000 Plan: current-state-and-submap-direct-window

## 阶段意图和边界

本阶段启动 `recognition/src/dotmod.cpp` 的 DOTMOD template matching（模板匹配）
RVV（RISC-V Vector，可变长度向量扩展）函数级评估。阶段目标是先回答
`detectTemplates()` 中每个滑窗位置都调用 `QuantizedMap::getSubMap()` 是否是主要可优化边界：
建立 `getSubMap` baseline（基线）与 direct window load（直接窗口读取）candidate（候选）的
same-chain correctness（同构链路正确性）和后续 bench 入口。

本阶段不修改 production（生产源码），不接入 `recognition/src/dotmod.cpp`，不证明完整
`DOTMOD::detectTemplates()` public entry（公开入口）收益，也不覆盖 DOTMOD color gradient
modality（颜色梯度模态）预处理。

## 当前状态清单

| item | current state |
| --- | --- |
| queue source | 执行清单第 4 项，状态为未启动；建议先做 `getSubMap` vs direct window load ablation（消融对照） |
| production source | `detectTemplates()` 外层按 row/col 滑窗，内层按 modality/template/data_index 对 `image_data & template_data` 计数 |
| allocation boundary | 当前每个 modality、每个窗口构造一个 `QuantizedMap` submap，存在分配和二维拷贝成本 |
| response boundary | 每个窗口都会分配 `std::vector<float> responses`，threshold（阈值）输出 detection 顺序必须保持 |
| topic assets | 新建 `test-rvv/recognition/dotmod_template_matching` |
| production state | 本阶段不修改 production |

## 假设与候选族

| candidate family | hypothesis | risk / unknown | status |
| --- | --- | --- | --- |
| `submap-direct-window-score-rvv` | 直接从原 map 的窗口行加载 image byte，与 template byte 做 bitwise AND 后用 mask popcount 计数，可避免 per-window submap 分配/拷贝 | 只覆盖单模板/单 modality 的窗口计分；完整入口还包含 template loop、responses 分配和 detection push 顺序 | planned |
| `response-buffer-reuse` | 将每个窗口的 `responses` 分配移出内层，可能比 RVV 算术更有效 | 改 production 前需 public-entry correctness 和 board evidence | roadmap |
| `full-detectTemplates-production-shaped` | 把 direct-window helper 放回完整多模板/多模态检测链路 | 需要保持 detection 顺序和阈值浮点缩放 | roadmap |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `submap-direct-window-score-rvv` | organized quantized byte map | `uint8_t` image/template row-major layout | test helper for one window and one template | `run_test_compare` | planned Phase 000 bench | planned 5-run board repeated | RVV binary should show byte load/and/mask popcount | planned | planned | implement helper after RED |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| RED gtest | `make -C test-rvv/recognition/dotmod_template_matching run_test_rvv` | helper 缺失导致编译失败 |
| GREEN helper | `include/impl/dotmod_template_matching_candidates.hpp` | Std/RVV correctness 通过 |
| bench scaffold | `src/bench_dotmod_template_matching.cpp` | 输出 baseline 与 direct window timing、checksum |
| asm | `make -C ... dump_bench_rvv` | filtered asm 中能看到 RVV byte/mask/reduction 指令，或解释降级 |
| board repeated | 5-run budget | 板卡可用时采集 decision bucket |
| docs | phase result、matrix、roadmap、evaluation、Handoff | 能从短 prompt 恢复下一阶段 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | RVV-vs-scalar and implementation-shape |
| diagnostic 是否可外推到 production | unknown；本阶段只覆盖单窗口计分，不覆盖完整 template/modalities loops、responses 分配和 detection 输出 |
| comparison-boundary / baseline mismatch 风险 | yes；baseline 显式模拟 `getSubMap` 后计分，candidate 直接窗口读取，仍不是 `DOTMOD::detectTemplates()` 公开入口 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；必须先做 full detectTemplates production-shaped diagnostic |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有 adopted RVV family；若后续同时尝试 response reuse 或多模板 fused loop，需要同边界 A/B |

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.20` 为 `positive`，`1.05-1.20` 为
`weak_positive`，`0.95-1.05` 为 `neutral`，`<0.95` 为 `negative`；预算内跨桶摇摆或
checksum / Evidence Doctor 异常无法解释时标为 `unstable`。

## 继续 / 停止条件

本阶段如果 correctness、asm 和 board evidence 正向，下一阶段默认进入
`010-full-detecttemplates-shaped-diagnostic`，把 direct-window helper 放回多 template /
多 modality / threshold 输出链路。若 Phase 000 弱或负，只拒绝当前单窗口 RVV 候选，仍应评估
`response-buffer-reuse` 或 full-chain profile，不能直接把 topic 写成 no-production。
