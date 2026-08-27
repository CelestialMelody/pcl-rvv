# Phase 000: current state and score-update diagnostic

## 阶段意图和边界

本阶段要证明 `updatedScoresAccordingToNeighborValues` 对连续 float score image（分数图像）的 3x3 邻域传播是否适合 RVV（RISC-V Vector，可伸缩向量扩展）候选。阶段范围只覆盖 test-only helper（测试专用辅助函数）；`features/include/pcl/features/impl/range_image_border_extractor.hpp` 和 `features/src/range_image_border_extractor.cpp` 暂不修改。

已验证范围计划：organized-grid row-major float score image，`Scalar=float`，内部像素固定 8 邻域，边界像素标量参考，minimum_border_probability 为默认附近值。不验证范围：完整 `computeFeature()`、indices 输入、`RangeImage::get1dPointAverage`、LocalSurface 指针数组、shadow / veil 状态机、principal curvature 和 production dispatch。

## 当前状态清单

| 项目 | 状态 | 路径 / 说明 |
| --- | --- | --- |
| 目标源码 | present | `features/include/pcl/features/impl/range_image_border_extractor.hpp`、`features/src/range_image_border_extractor.cpp` |
| 上游专用测试 | missing | `rg` 未发现 range_image_border_extractor 专用 test |
| topic 测试资产 | creating | `test-rvv/features/range_image_border_extractor/` |
| production 状态 | unchanged | 本阶段不修改 production |
| 板卡状态 | available by prompt | 用户说明“目前板卡可用” |

## 假设与候选族

| candidate | 假设 | 风险 | 本阶段判据 |
| --- | --- | --- | --- |
| score-update 3x3 RVV | 内部像素可用 VL chunk（可变向量长度分块）并行处理，边界保留标量 | 局部收益可能被完整 pipeline 稀释；浮点加法树可能有微小差异 | correctness 误差 <= `1e-6`、RVV asm 有 load/store/arithmetic、板卡 repeated bucket 非负向 |

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段只把 `score-update 3x3 RVV` 从 `planned` 推进到 `attempted`、`rejected` 或 `partial-production-candidate`。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `src/test_range_image_border_extractor.cpp`，`make run_test_rvv` | RVV 构建因 candidate 未实现而出现期望 mismatch |
| GREEN candidate | `include/impl/range_image_border_extractor_score_update.hpp` | `make run_test_compare` 通过 |
| asm check | `make dump_test_rvv` | filtered asm 中出现 RVV load/store 或算术指令 |
| board smoke | `make run_board_test fetch_board_logs` | 板卡 gtest 通过 |
| result update | `result.zh.md`、matrix、roadmap、Handoff | 每个 action 有 done / partial / blocked 状态 |

## Evidence Doctor 和 registry 规则

本阶段如果只完成 correctness 和 asm，不声明性能结论，Evidence Doctor 人工记录为 `not_run_for_performance`。若加入 board bench repeated，必须生成 summary / manifest 并运行 `test-rvv/script/evidence_doctor.py` 或按规则人工列出 Errors / Warnings / Suggestions。当前 topic 尚未接入 registry，result 中先写人工 freshness 检查路径。

## 板卡复跑预算和决策桶

若本阶段补 bench：默认 repeated board run count 为 5；允许一次同边界确认复跑。decision bucket（决策桶）暂定为：median speedup >= 1.20 且 `B/A < 1` 为 0/5 记 positive，1.05-1.20 记 weak-positive，0.98-1.05 记 neutral，<0.98 记 negative，跨方向或复跑反转记 unstable。

## 继续 / 停止条件

默认继续到 correctness、asm 和板卡 smoke。停止条件只包括：工具链或板卡不可达、RVV candidate correctness 无法闭合、继续需要修改 production 或扩大到完整 public entry、dirty isolation 不安全，或 Evidence Doctor Error 无法降级。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar |
| diagnostic 是否可外推到 production | unknown；本阶段只覆盖 score-update helper |
| comparison-boundary / baseline mismatch 风险 | helper 内低，完整 public entry 高 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅当后续 full diagnostic 或 profile 证明 public 主成本仍在 score-update 时允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；进入 production 前必须另做 PI1-PI5 |

## 文档更新清单

更新 `doc/range_image_border_extractor-evaluation.zh.md`、`doc/phases/000-current-state-and-score-update/result.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` 和 current Handoff。没有 adopted production behavior 时不创建 `doc-rvv/features/range_image_border_extractor-RVV.zh.md`。
