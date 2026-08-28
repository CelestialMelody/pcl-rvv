# Phase 000 Plan: current-state-and-score-accumulation

## 阶段意图和边界

本阶段建立 LINEMOD score accumulation（分数累加）production-shaped diagnostic（生产形态诊断）。它只证明一组已线性化 `u8` score maps 累加到 `u16 score_sums` 的 correctness（正确性）和候选性能，不证明 `EnergyMaps` 构造、`LinearizedMaps` 拷贝、threshold scan（阈值扫描）、non-max suppression（非极大值抑制）、averaged detections（邻域加权检测）或真实 production dispatch（生产分流）。

## 当前状态清单

| item | current state |
| --- | --- |
| source | `recognition/src/linemod.cpp` 包含 `matchTemplates`、`detectTemplates` 和 `detectTemplatesSemiScaleInvariant` 三条相关路径 |
| existing SIMD | 源码有 `__SSE2__` 分支，但当前 RISC-V production 没有 RVV 分支 |
| topic assets | 新建 `test-rvv/recognition/linemod_template_scoring` |
| board availability | 用户已说明板卡可用；本阶段计划 correctness 通过后继续 board bench |
| production state | 本阶段不修改 production |

## 假设与候选族

第一候选族是 `u8` score map 拓宽为 `u16` 后向量累加。预期 RVV 用 `vle8`、zero-extend（零扩展）和 `vadd` 减少逐元素循环成本；风险是每个 feature/bin map 都完整扫一遍 `mem_size`，实际收益可能被内存带宽、`vsetvli` 和缓存行为稀释。

## 优化矩阵

| candidate | scope | correctness | bench | asm | board | decision |
| --- | --- | --- | --- | --- | --- | --- |
| score accumulation RVV | already-linearized maps, `mem_size` tail included | std/RVV helper 对拍 | planned | planned | planned | planned |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| 写 RED 测试 | `src/test_linemod_template_scoring.cpp` | helper 缺失导致编译失败 | failure observed |
| 实现标量和 RVV helper | `include/impl/linemod_template_scoring_candidates.hpp` | std/RVV correctness 通过 | tests pass |
| 增加 bench harness | `src/bench_linemod_template_scoring.cpp` | board 可运行的 Std/RVV 对比输出 | bench logs |
| 反汇编检查 | `make dump_linemod_score_bench_rvv` | RVV 指令可归属到 helper | asm summary |
| 板卡复跑 | 5-run budget | decision bucket stable / unstable | Evidence Doctor |

## Evidence Doctor 和 Registry

本阶段涉及 board summary 和 EvidenceDecision 前必须运行 Evidence Doctor（证据体检）。若脚本化 manifest 尚未完成，先按 `evidence-doctor.zh.md` 人工记录 Errors / Warnings / Suggestions，并把 `evidence_registry_status` 标成 partial。

## 阶段完成条件

Correctness 必须同时覆盖 `mem_size` 非整向量尾段、多 map 累加、阈值扫描所需最大值和 checksum。若只完成 helper 或单次测试，阶段不关闭。

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.05` 为 positive，`1.00-1.05` 为 weak-positive，`0.97-1.00` 为 neutral，`< 0.97` 为 negative；方向在预算内摇摆则为 unstable。

## 继续 / 停止条件

板卡可用时，本阶段不以“需要板卡验证”为停止理由。若 correctness、构建工具、ssh/rsync、Evidence Doctor Error 或 dirty isolation 阻塞，停止在 Handoff，并写清解除条件。

## 文档更新清单

更新 `README.zh.md`、evaluation、phase result、optimization matrix、optimization roadmap 和 Handoff。没有 adopted production behavior，因此不创建 `doc-rvv` production 长期主题文档。

## Roadmap 同步动作

Phase result 必须回填 score accumulation RVV 的 adopted / attempted / rejected / deferred 状态，并决定下一 phase 是 threshold scan、energy map generation、linearized map copy，还是 production integration plan。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar |
| diagnostic 是否可外推到 production | unknown；只覆盖已线性化 score map 的累加核 |
| comparison-boundary / baseline mismatch 风险 | yes；不包含 map 构建、模板 feature 遍历和 detection 输出 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，条件是完整入口 profile 或 direct test 证明 score accumulation 仍值得接入 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 RVV family；若后续出现多个 RVV family，需要补同边界 A/B |
