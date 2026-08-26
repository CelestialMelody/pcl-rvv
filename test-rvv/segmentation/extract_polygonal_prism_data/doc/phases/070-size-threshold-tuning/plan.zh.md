# Phase 070 Plan: size-threshold-tuning

## 阶段意图和边界

本阶段只评估 `segmentRvv` 当前 `indices_->size() >= 64` 的规模阈值是否仍合适。范围限定在真实 public entry（公开入口）`ExtractPolygonalPrismData<PointT>::segment(PointIndices&)`、`PointXYZ`、dense ordered indices、single polygon、`--path production`。不修改 public API，不改 `projectPoints`，不扩大到新 row source、点型或 `Scalar=double`。

本阶段先做 board sweep（板卡规模扫描）观察 32 / 48 / 64 / 96 / 128 / 256 的 Std/RVV speedup（标量 / RVV 加速比）。若 32 或 48 稳定正向，再考虑把 production threshold 降到对应值并做 post-change board confirmation（改后板卡确认）；若低于 64 不是稳定 positive bucket，则保留当前阈值并关闭本阶段。

## 当前状态清单

| area | 当前状态 | 证据 |
| --- | --- | --- |
| production threshold | `indices_->size() < 64` fallback | `SegmentRvvDeclinesSmallInputs` |
| adopted production ranges | single / nested polygon、dense / indexed、`PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` | Phase 045 / 050 / 060 result |
| board availability | available | 当前会话已完成 Phase 060 post-gate board runs |

## 优化矩阵

| candidate family | row source | point type / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| threshold keep 64 | dense ordered | `PointXYZ` / current production | existing fallback test | size sweep shows <64 neutral / negative or noisy | existing production asm | size-sweep doctor | pending |
| threshold lower to 48 | dense ordered | `PointXYZ` / current production | update fallback tests if adopted | 48 positive and 32 not required | same helper | post-change doctor | pending |
| threshold lower to 32 | dense ordered | `PointXYZ` / current production | update fallback tests if adopted | 32 and above positive | same helper | post-change doctor | pending |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| size sweep board evidence | `log/board/repeated-production-size-sweep/summary.md` 或等价 summary | 每个规模有 Std/RVV 结果和 checksum 一致 |
| Evidence Doctor | sweep manifest / doctor | Errors 必须为 0；Warnings 要解释为保留阈值、改阈值或降级证据 |
| optional production patch | `extract_polygonal_prism_data.hpp` | 只有低于 64 的规模稳定 positive 时才改 threshold |
| post-change verification | tests + board confirmation | 若改 threshold，重跑 `make run_test_compare`、`make run_board_test` 和同边界 board confirmation |

## 板卡复跑预算和决策桶

先做 1 组 sweep，规模为 32 / 48 / 64 / 96 / 128 / 256，iterations=16，warmup=3。若 32 或 48 接近阈值但可能改变 gate，最多对候选规模做 5-run repeated confirmation；若 bucket 不稳定，保留当前 64。

- `positive`：median >= 1.10x 且 min > 1.00x。
- `neutral`：median 在 0.95x 到 1.10x，或低于 64 的规模方向摇摆。
- `negative`：median < 0.95x 或多次低于 1。
- `unstable`：不同 run 方向反转，Evidence Doctor 有 Error，或 checksum / metadata 不闭合。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | public overload；Std / RVV build 都调用真实 `segment` |
| 当前决策问题 | 当前 size gate 是否应从 64 调整 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；只用 production path sweep |
| comparison-boundary / baseline mismatch 风险 | sweep 必须固定 `PointXYZ`、dense、single polygon 和 `--path production` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；但只限 threshold 常量，失败则保留当前 gate |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策是 threshold gate，不是实现族替换 |

## 继续 / 停止条件

如果低于 64 的候选没有稳定 positive bucket，停止并保留当前阈值。如果 32 或 48 稳定 positive，则同轮修改 threshold 并完成 post-change correctness、board confirmation、Evidence Doctor、registry 和文档刷新。完成后再检查 roadmap 是否还有当前授权范围内的未阻塞方向。
