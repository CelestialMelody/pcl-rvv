# Phase 011 Plan：row-source taxonomy and pattern boundary

## 阶段意图和边界

本阶段做整理后重启，不做新的性能 candidate。目标是把当前 topic 的命名坐标系固定为：

- 第一层：`row_source_policy`，只允许四类 production 入口形态：
  `ordered-cloud-pair`、`source-indexed-cloud-pair`、`dual-indexed-cloud-pair`、
  `correspondence-pair`。
- 第二层：`index_pattern`，只在对应 row source 内部出现。Phase 010 的
  `contiguous`、`local-window`、`strided` 属于 `correspondence-pair` 内部
  synthetic pattern，不是新的 production 数据流。
- 第三层：`candidate_family`，描述实现族，例如 staged ordered reuse、direct indexed
  gather、direct index stream、segment stream。
- 第四层：`evidence_role`，描述证据角色，例如 correctness、qemu_smoke_only、
  diagnostic、production_direct。

本阶段只修改 `test-rvv/registration/transformation_estimation_dual_quaternion`
下的文档、test-support 注释和 bench 输出说明；不修改 production TEDQ header、不新增
production dispatch、不创建 `doc-rvv`。

## 当前状态

Phase 010 已完成并拒绝 locality-aware production candidate：contiguous 相对 strided
baseline 为 weak-positive / positive，local-window 三个规模均 negative，Evidence
Doctor 为 `Errors=3 / Warnings=4 / Suggestions=0`。这些数据只说明
`correspondence-pair` 内部 index pattern 对 direct index stream 敏感。

当前活动文档仍有少量旧 label 说明和旧 smoke 入口文案，会让读者把历史 bench filter、
production row-source policy 和 synthetic index pattern 混在一起。本阶段先清理这些
说明，再恢复到下一轮真正优化。

## 优化矩阵

| candidate family | row_source_policy | index_pattern | point type / Scalar / layout | correctness | bench / evidence | decision |
| --- | --- | --- | --- | --- | --- | --- |
| taxonomy cleanup | all four policies | correspondence pattern only when row source is `correspondence-pair` | not_applicable | not_required | `make evidence_status` fresh；不新增 board run | planned |
| correspondence direct index stream | `correspondence-pair` | strided baseline；contiguous/local-window 已作为消融 | `PointXYZ` / `float` / xyz AoS | Phase 010 Std/RVV `22/22`；当前回归为 `24/24` | Phase 008 retained；Phase 010 locality rejected | baseline retained |
| point type / layout expansion | `correspondence-pair` first | not yet selected | `PointXYZI`、`PointXYZRGB` / `float` / xyz AoS | planned next phase | planned next phase | phase_deferred + unblocked |

## 实现和测试动作

| action | 产物 | 验证 | 完成判据 |
| --- | --- | --- | --- |
| A1 taxonomy docs | README、evaluation、testing overview、benchmark/evidence、optimization evidence、roadmap、matrix | 文档 scan | 活动文档不再把 pattern 写成 row source。 |
| A2 test-support wording | `include/impl/tedq_adapters.hpp`、`src/test_tedq.cpp`、`src/bench_tedq.cpp` | 编译 / QEMU smoke | 注释和 bench banner 明确四类 row-source policy。 |
| A3 evidence freshness | registry / doctor | `make evidence_status` | 没有 `unregistered_change` 或 doc-ref 问题。 |
| A4 next phase handoff | phase README / roadmap | 文档 scan | 默认恢复入口指向 Phase 012 point type / layout expansion。 |

## Evidence Doctor 和 registry

本阶段不新增 board 性能数据。若改动 bench banner 导致 QEMU smoke log 变化，只运行窄
QEMU smoke 并刷新 registry；QEMU 仍不用于性能结论。Phase 010 board doctor 的
`Errors=3 / Warnings=4 / Suggestions=0` 保留为负向证据，不尝试抹平。

## 继续 / 停止条件

阶段完成后进入 `012-point-type-layout-expansion`。该阶段应优先验证
`correspondence-pair` direct index stream 在 `PointXYZI`、`PointXYZRGB` 上的 correctness、
QEMU smoke、asm 和 bounded board repeated。production 仍需单独授权 PI1。
