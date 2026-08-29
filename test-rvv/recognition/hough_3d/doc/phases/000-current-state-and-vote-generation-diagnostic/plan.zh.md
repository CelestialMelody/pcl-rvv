# Phase 000 Plan: current-state-and-vote-generation-diagnostic

## 阶段意图和边界

本阶段先把 `Hough3DGrouping::houghVoting()` 的 vote generation 子段拆成可审查 helper，
验证 scene vote 生成、min/max reduction 和保序候选索引的语义是否能被 RVV candidate
保持。

本阶段不接入 production，不碰 `HoughSpace3D::vote()` / `voteInt()` / `findMaxima()`，
也不把 accumulator scatter 写成当前阶段的主目标。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 生产源码 | `houghVoting()` 先算 scene vote，再写 HoughSpace | `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` |
| topic 骨架 | 已创建 | `test-rvv/recognition/hough_3d` |
| 队列来源 | 来自 recognition 复筛清单第 4 项 | `doc-rvv/library-screening/recognition/recognition-retained-candidate-rescreen.zh.md` |

## Phase Scope 与扩展队列

- `validated_scope`：`PointXYZ` / `float` / AoS，scene vote generation，min/max reduction。
- `unvalidated_scope`：accumulator scatter、`findMaxima()`、泛型点类型、production direct。
- `point_type_expansion_queue`：后续再补 `PointModelT` / `PointSceneT` 泛型 gate。
- `phase_closeout_boundary`：只关闭 vote generation diagnostic。

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `vote-generation-rvv` | 3x3 线性组合和 reduction 可先被 vectorize | accumulator scatter 仍可能主导 | planned |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `vote-generation-rvv` | correspondence-pair | `PointXYZ` / `float` / AoS | diagnostic helper | `run_test_compare` | `run_bench_compare` | 待补 | 待补 | 待补 | planned | 写 RED gtest |

## 实现和测试动作

| action | 产物 / 命令 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| RED gtest | `make -C test-rvv/recognition/hough_3d run_test_compare` | RVV build 先失败，证明 test 能抓住未接上的路径 | failure observed |
| GREEN helper | `include/impl/hough_3d_candidates.hpp` | std / rvv correctness 通过 | `run_test_compare` pass |
| bench | `run_bench_compare` | synthetic vote generation timing | board repeated |
| asm | `dump_bench_rvv` | RVV 指令归属到 helper | pass 或降级 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar diagnostic |
| diagnostic 是否可外推到 production | unknown；它只覆盖 vote generation，不含 HoughSpace scatter |
| comparison-boundary / baseline mismatch 风险 | yes；accumulator write 和 maxima scan 还未纳入 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但要先把 accumulator 分层 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 目前还没有已采纳 family |

## 继续 / 停止条件

板卡可用时优先补 correctness 和 bench；若构建失败、测试 helper 语义不清、或
accumulator scatter 已经主导到看不清 vote generation，则暂缓并另起结构 phase。

