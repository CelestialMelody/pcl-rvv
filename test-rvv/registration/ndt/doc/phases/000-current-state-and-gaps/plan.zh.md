# Phase 000 Plan: Current State And Gaps

## Phase 目标和授权边界

目标是为 `registration/include/pcl/registration/impl/ndt.hpp` 建立函数级 RVV 评估和 test-only diagnostic（测试专用诊断）证据。授权边界只覆盖 `test-rvv/registration/ndt` 的测试资产、topic-local 文档和证据摘要；不修改 production 源码。

## 当前源码判断

`computeDerivatives` 的逐点、逐邻域循环是首轮候选；`computeAngleDerivatives` 是每轮一次的小矩阵预计算，`JacobiSVD`、line search（线搜索）和 voxel neighbor search（体素邻域搜索）不作为首轮手写 RVV 目标。

## 本轮假设

如果把 `updateDerivatives` 所需输入 staged（暂存）成 SoA（数组结构），RVV 可以跨 sample 批量计算 gradient / hessian reduction（规约）。风险是 staged buffer、多遍 reduction 和标量 `exp` 成本可能超过收益。

## 执行步骤

1. 创建 `src/`、`include/impl/`、`script/`、`doc/` topic scaffold。
2. 实现标量 reference 与 RVV candidate，并保持 `__RVV10__` 关闭时 fallback 到标量。
3. 跑 `run_test_compare`、QEMU smoke、filtered asm 和 Evidence Doctor。
4. 跑 board correctness smoke 和 5 次 repeated board bench。
5. 更新 summary、manifest、Evidence Doctor、registry、evaluation、roadmap 和 matrix。

## 板卡复跑预算和决策桶

预算为 5 次 repeated run。`median speedup >= 1.20` 且 0 次低于 1.0 为 positive；`1.05-1.20` 且少量低于 1.0 为 weak-positive；`0.95-1.05` 为 neutral；`median < 0.95` 为 negative；预算耗尽且方向摇摆时标为 unstable。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar implementation-shape |
| diagnostic 是否可外推到 production | no；它排除邻域搜索、point derivative、line search、solver 和 dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；标量和 RVV reduction 形态不同，只能诊断候选形态 |
| negative 时是否允许 bounded production probe | 只有用户明确要求，并先补 public-entry profile 与 fallback / dispatch 计划 |
| clean adoption 是否需要 production boundary A/B | yes；当前不满足 |
