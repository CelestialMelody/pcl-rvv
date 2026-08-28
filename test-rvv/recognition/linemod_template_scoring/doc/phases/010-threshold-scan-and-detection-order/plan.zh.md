# Phase 010 Plan: threshold-scan-and-detection-order

## 阶段意图和边界

本阶段建立 threshold / max scan（阈值和最大值扫描）production-shaped diagnostic。输入是 Phase 000 已生成的 `u16 score_sums`，输出是最大值、最大值最早位置、超过 raw threshold 的候选计数，以及可供后续保序 detection 写入使用的候选 index 列表。它不修改 production，不覆盖 non-max suppression（非极大值抑制）和 averaged detection（邻域加权检测）内部逻辑，也不证明真实 `detectTemplates` public entry。

## 当前状态清单

| item | current state |
| --- | --- |
| score accumulation | Phase 000 completed，board repeated positive，证据角色为 production-shaped diagnostic |
| existing summary helper | `summarizeScores` 是标量实现，只输出 max、tie-break 和 count |
| source semantics | `matchTemplates` 最大值扫描使用 strict `>`；`detectTemplates` 使用 `raw_score > raw_threshold` 并按 mem_index 顺序 push detection |
| production state | 不修改 `recognition/src/linemod.cpp` |
| board availability | 当前会话板卡可用；本阶段 correctness 和 asm 后继续 board repeated |

## 假设与候选族

候选族是 `u16` score scan RVV：用 vector compare 找到 threshold mask，再在同一线性顺序中保留候选 index；max / earliest tie-break 可以先采用 chunk-local RVV reduce 加标量 tie-break，或保持 max 标量只向量化 threshold count。第一实现优先选择“向量化 threshold count + 保序候选收集的标量 tail”，因为 detection 输出顺序比单纯最大值更重要。

## 优化矩阵

| candidate | scope | correctness | bench | asm | board | decision |
| --- | --- | --- | --- | --- | --- | --- |
| threshold count RVV + max tie-break helper | `u16 score_sums`, `raw_threshold`, mem_size tail included | planned RED/GREEN | planned | planned | planned if bench exists | planned |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| 写 RED 测试 | `src/test_linemod_template_scoring.cpp` | `scanScoresRVV` 或等价 helper 缺失导致编译失败 | failure observed |
| 实现标量 / RVV scan helper | `include/impl/linemod_template_scoring_candidates.hpp` | count、max、earliest tie、候选 index 顺序与标量一致 | tests pass |
| 扩展 bench harness | `src/bench_linemod_template_scoring.cpp` | 可单独计时 accumulation、scan 或 combined path | parseable bench logs |
| 反汇编检查 | `make dump_linemod_score_bench_rvv` 或新增 target | compare / mask / vector load 指令可归属 | asm summary |
| 板卡 repeated | 5-run budget | decision bucket stable / unstable | Evidence Doctor |

## Evidence Doctor 和 Registry

本阶段若生成 board summary，必须复用 topic-local manifest wrapper 或新增 scan-specific manifest 字段，并登记到 `log/evidence_registry.json`。若只完成 correctness 而未完成 bench，不做性能结论，matrix 标成 `phase_deferred + unblocked` 并继续补 bench。

## 阶段完成条件

Correctness 必须覆盖：空候选、所有候选、非整向量 tail、最大值并列时保留最早 index、threshold 使用 strict `>`、候选 index 按 mem_index 递增。性能结论必须来自带 warm-up 的 board repeated，QEMU timing 只可作为 smoke。

## 板卡复跑预算和决策桶

默认 5-run，`median speedup >= 1.05` 为 positive，`1.00-1.05` 为 weak-positive，`0.97-1.00` 为 neutral，`<0.97` 为 negative；方向在预算内摇摆则为 unstable。若 Evidence Doctor 发现长尾但 5/5 正向，可继续但降级为 diagnostic positive。

## 继续 / 停止条件

板卡可用时不因“需要板卡验证”停止。若 helper 无法保持 detection 顺序、candidate index 写回需要未授权 production buffer 设计，或 doctor 出现 Error，则停在 Handoff；否则继续到本阶段 EvidenceDecision，再决定进入 energy map / linearized map phase 或 production integration plan。

## 文档更新清单

更新本 phase result、optimization matrix、optimization roadmap、evaluation 和 Handoff。没有 adopted production behavior 时仍不创建 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar for threshold / max scan helper |
| diagnostic 是否可外推到 production | unknown；真实入口还要处理 NMS、averaging 和 detection push |
| comparison-boundary / baseline mismatch 风险 | yes；候选 index sink 可能不同于 production `detections.push_back` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，条件是同边界 scan helper correctness 完整且 production wrapper 能保持输出顺序 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若进入 production，需要 production direct Std/RVV repeated；多实现族时需要同边界 RVV-vs-RVV A/B |
