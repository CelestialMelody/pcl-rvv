# Phase 030：encode 输入域与边界 fallback 计划

## 阶段意图和边界

Phase 020 拒绝 f64 exact quantize（精确双精度量化）作为默认路线后，本阶段只做 production input-domain（生产输入域）和 boundary fallback（边界回退）语义审计。目标是判断能否安全实现 f32 fast path（单精度快速路径）+ boundary lane fallback（边界向量通道回退）。

本阶段不修改 production（生产源码），不实现新的 RVV fast path，不运行板卡性能测试。只有当源码能给出可证明 error bound（误差界）时，才允许下一阶段实现 fast path。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| f32 full quantize | correctness 边界差 1，Phase 000 拒绝。 |
| f64 exact quantize | correctness 通过，但 Phase 020 板卡退化，默认拒绝。 |
| 默认 candidate | indexed gather + scalar same-chain quantize，当前 diagnostic-positive。 |
| production source | `PointCoding::encodePoints` 直接使用 `(idxPoint.coord - referencePoint_arg[k]) / pointCompressionResolution_` 后 `static_cast<int>`。 |

## 审计动作

1. 读取 `io/include/pcl/compression/point_coding.h`，确认 `referencePoint_arg`、`pointCompressionResolution_` 和量化表达式。
2. 读取上游 `octree_pointcloud_compression` 调用处，判断 `referencePoint_arg` 是否有可用于误差界的范围合同。
3. 如果没有合同，记录为什么不能安全实现 f32 fast path + boundary fallback。
4. 更新 result、roadmap 和 matrix，把下一默认动作转向 decode stability profile（解码稳定性复核）或 production-shaped context。

## 完成条件

| 条件 | 判定方式 |
| --- | --- |
| 可以继续实现 boundary fallback | 能从源码或配置证明 reference / resolution / coordinate 范围，并给出保守 fallback threshold（阈值）。 |
| 拒绝或阻塞 boundary fallback | 源码只给任意 double reference 和 float resolution，无法证明 f32 近似不会越过 `static_cast<int>` 截断边界。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | semantic audit（语义审计），不作为性能证据。 |
| A/B boundary | production source read-only。 |
| 当前决策问题 | implementation-shape：是否允许构造 f32 fast path + scalar fallback。 |
| diagnostic 是否可外推到 production | 本阶段直接读 production source，但不产生 production performance。 |
| comparison-boundary / baseline mismatch 风险 | 不比较性能；风险是把未证明的输入域当成生产合同。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许；必须先有语义安全边界。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段不触发。 |
