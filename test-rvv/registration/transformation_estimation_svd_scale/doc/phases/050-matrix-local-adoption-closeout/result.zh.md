# Phase 050 结果：matrix-local adoption closeout

## EvidenceDecision

`adopted-by-user / production-helper-simplification`。

用户已确认当前有收益的实现可以接入。本阶段据此把 Phase 042 的 `matrix-local-scale-simplification` 从 `positive_pending_user_judgment / smaller_patch_probe` 收口为已采纳的 production helper simplification：`getTransformationFromCorrelation` 使用 `trace(R * H)` 直接计算 scale 分子，不再构造 `R4 * cloud_src_demean` 临时矩阵和逐列点积。

## 实际状态

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| production source | adopted | 源码已使用 `trace(R * H)`；没有扩大 RVV dispatch、row-source gate、点型 gate 或 `Scalar` 范围。 |
| correctness | phase-local 15 tests | Phase 050 收口时 Std/RVV correctness 为 15 tests；Phase 051 后当前 correctness 已更新为 16 tests，matrix-local 等价测试仍在 aggregate 中。 |
| board evidence | weak-positive but accepted as helper simplification | Phase 020 / 042 board median B/A 为 `1.683x` / `1.173x` / `1.173x`，Doctor `Errors=0`、`Warnings=2`。 |
| production behavior | adopted helper simplification | 这是标量后段公式简化，不是新的 RVV intrinsic family，也不替代 direct-fused 主线。 |

## 结论边界

- 已采纳的是 `getTransformationFromCorrelation` 内部的局部公式简化。
- 该结论不扩大到 `Scalar=double` 的 RVV 路径；非 RVV / fallback 语义仍由父类和现有 gate 保持。
- 该结论不恢复 Phase 048 staged-selected-cloud 或 Phase 049 target-sorted，这两条路线仍为负向诊断。
- 后续若继续优化，应另开新 candidate family 或更窄边界，而不是把 matrix-local 的 weak-positive 外推成新的 RVV mainline。

## 文档同步

已将 matrix-local 状态同步为 `adopted-by-user / production-helper-simplification`。Phase 050 收口时 correctness 为 15 tests；Phase 051 后当前 correctness 已更新为 16 tests。若本阶段后继续优化，默认从 roadmap 的新候选空间恢复。
