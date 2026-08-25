# Phase 020 result: PI1 production integration plan

## 执行范围

本阶段只完成 PI1 production integration plan（生产接入计划）文档化，不修改 production（生产源码）。实际产物为 `plan.zh.md`，并同步 roadmap / matrix / evaluation 的恢复入口。

## PI1 结论

phase 000 和 phase 010 的 helper-only diagnostic（仅测试 helper 边界诊断）证据足以支持一个 bounded production probe（有界生产探针）计划，但不足以直接写成 production-ready（可接入生产）或 clean adoption（干净采纳）。

推荐下一步是 PI2 production patch，但必须先获得用户明确授权。候选范围应保持窄边界：

- public API 不变。
- 优先用 PointXYZ-like traits gate（类似 PointXYZ 的 xyz 单 float AoS 布局门控），不能把 `PointXYZ` 诊断样本外推成完整模板入口。
- 只覆盖 `float`、indexed AoS、当前 `compute()` 使用的 xyz 读取路径。
- 非覆盖点类型、非 RVV 构建、小输入或 32-bit byte offset 不安全时 fallback（回退）到现有标量路径。

## 证据缺口

| gap | why it matters | next action |
| --- | --- | --- |
| production direct correctness | 当前 gtest 只验证 diagnostic helper，不证明 `MomentOfInertiaEstimation<PointT>::compute()` 真实命中 RVV | PI2/PI3 新增 public-entry-shaped gtest |
| fallback isolation | 当前没有 production fallback gate 测试 | PI3 覆盖非 RVV build、非覆盖点型、小输入 / 空输入和 shuffled indices |
| production asm attribution | 当前 asm 归属是 bench helper 内联范围 | PI4 需要 production direct binary 的符号或内联范围归属 |
| production board repeated | 当前性能结论来自 helper-only boundary | PI4 需要 public-entry-shaped board repeated 和 Evidence Doctor |
| binary identity metadata | Evidence Doctor suggestion 指出缺二进制身份字段 | PI4 manifest 补 binary hash / build label |

## continue / stop decision

`continue_stop_decision`：本阶段计划完成；下一步进入 production 源码修改，命中授权边界。

`next_phase_default`：`PI2 production_patch pending explicit user authorization`。

`stop_condition_hit`：继续推进需要修改 `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` 及可能的 private helper 声明，当前 prompt 未显式授权修改 production 源码；按 `AGENTS.md` 停在 PI2 前。
