# Phase 010 Plan: production-direct-vote-generation-probe

## 阶段意图和边界

本阶段验证 `Hough3DGrouping::houghVoting()` 的真实 production direct 路径。
目标不是证明 vote generation helper 本身，而是看它放回公开入口后，是否仍然值得把
当前 patch 采纳进 production。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 生产源码 | 已接入 RVV vote generation 分支 | `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` |
| 诊断结果 | phase 000 已正向 | `test-rvv/recognition/hough_3d/log/board/repeated_phase000_vote_generation_diagnostic/summary.md` |
| production direct bench | 已有 repeated board | `test-rvv/recognition/hough_3d/log/board/repeated_phase010_production_direct/summary.md` |

## Phase Scope 与扩展队列

- `validated_scope`：`PointXYZ` / `float` / AoS + RF axes，`houghVoting()` 公开入口。
- `unvalidated_scope`：`HoughSpace3D::vote()`、`voteInt()`、`findMaxima()`、其它点型。
- `point_type_expansion_queue`：后续再补 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | production_direct |
| A/B boundary | production detail helper + public overload |
| 当前决策问题 | 是否采纳当前 production patch |
| diagnostic 是否可外推到 production | 不可直接外推；full 入口还包含 scatter / interpolation |
| comparison-boundary / baseline mismatch 风险 | 有；局部 vote generation 与 full `houghVoting()` 成本不一致 |
| weak / neutral 时是否允许继续 probe | yes；但只能转向 accumulator / scatter 方向 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要，当前还没有 |

## 继续 / 停止条件

若 production direct repeated 结果继续落在 `neutral` 或 `negative`，当前 patch 不采纳；
下一 phase 默认转向 `accumulator-scatter-audit`。
