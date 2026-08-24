# Phase 030：encode 输入域与边界 fallback 结果

## 实际范围

本阶段只读 production 源码和上游调用位置，没有修改 production，也没有改默认 RVV candidate。审计对象是 `io/include/pcl/compression/point_coding.h` 和 `io/include/pcl/compression/impl/octree_pointcloud_compression.hpp` 中传入 `PointCoding::encodePoints` 的 `referencePoint_arg` 与 `pointCompressionResolution_`。

## 审计结果

| 问题 | 源码事实 | 结论 |
| --- | --- | --- |
| `referencePoint_arg` 范围 | `PointCoding::encodePoints` 接收 `const double*`，在函数内没有范围检查。上游调用传入 `lowerVoxelCorner`。 | `PointCoding` 自身没有提供可用于 f32 fast path 的 reference 范围合同。 |
| `pointCompressionResolution_` 范围 | 通过 `setPrecision(float)` 写入，默认值为 `0.001f`；当前函数内没有非零、正数或最小值 gate。 | 无法仅从 `PointCoding` 证明除法误差上界。 |
| f32 fast path 安全性 | 标量表达式先按 double 计算，再 `static_cast<int>` 向零截断。 | 只要 f32 近似跨过整数截断边界，就会差 1；没有输入域合同就不能安全跳过 scalar same-chain helper。 |
| boundary fallback threshold | 需要知道 `(value - reference) / resolution` 的最大 f32-vs-double 误差。 | 当前源码不足以给出全局阈值；实现 fast path 会把未证明假设写进 candidate。 |

## Decision

`boundary lane fallback` 当前判为 `blocked / rejected for implementation without input-domain contract`。

这不是性能负向，而是语义边界不成立：如果没有 production 输入域、reference / resolution 范围和误差预算，就不能证明“远离整数边界的 lane 一定可用 f32 量化”。为了避免把未证明的近似写成 production-shaped diagnostic，本阶段不实现 f32 fast path。

## Matrix 更新

| candidate family | status | evidence | next action |
| --- | --- | --- | --- |
| f32 fast path + boundary fallback | blocked / rejected for now | production source read-only audit。 | 只有拿到输入域合同、profile 或用户接受的误差合同后再重开。 |
| indexed gather + scalar same-chain quantize | diagnostic-positive | Phase 020 默认路径 board summary。 | 继续作为当前默认 encode diagnostic。 |
| decode stability profile | phase_deferred + unblocked | 当前 decode 5-run weak-positive。 | 下一默认 phase。 |

## Continue / Stop Decision

`continue_stop_decision`：encode 量化主线当前命中语义停止条件，不进入 production，也不实现 boundary fallback。当前 topic 仍有未阻塞的 decode stability profile（解码稳定性复核）动作，下一默认 phase 是 `040-decode-stability-profile`；该 phase 应使用单独 output dir，避免覆盖当前 12-case default summary。
