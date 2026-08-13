# Phase 010 Plan: Test Harness Layout Audit

## 阶段意图和边界

本阶段补齐 topic maturity audit（主题成熟度审计）中遗漏的 test harness layout
audit（测试框架布局审计）。当前 topic 已有 production candidate 和 Phase 000 的
reference / production-detail 边界清理证据，但测试源码仍位于 topic 根目录，且使用超长
`test_transformation_estimation_point_to_plane_lls.cpp` /
`bench_transformation_estimation_point_to_plane_lls.cpp` 文件名。根据当前 agent 资产，
长 topic 需要审计是否采用配置解析出的 `src/`、`include/`、`include/impl` 布局和长 topic
缩写 topic token。

本阶段目标是做最小结构迁移：让 Makefile 使用 `src/` 下的测试 / bench 源码，让外部测试和
bench include `include/teptpl.h` 聚合入口，并保留旧
`test_support_transformation_estimation_point_to_plane_lls.hpp` 作为 compatibility alias
（兼容别名）。现有 `test_support/` 内部分层先不改名、不移动到 `include/impl/`；它已经按
common、RVV math、row sources、reductions 和 candidates 拆分，继续作为内部实现目录。这样
可以降低 churn（无效改动）和审查风险，同时把最影响 reviewer 定位的根目录源码布局先收口。

本阶段不做：

- 不修改 production header、RVV hot path、dispatch gate、fallback gate 或 bench case 逻辑。
- 不拆分 1567 行测试文件和 1081 行 bench 文件的函数职责；若 reviewer 要求，再新建后续 phase。
- 不把 `test_support/` 文件机械改名为相邻 weighted topic 的 `include/impl/teptplw_*` 形态。
- 不新增 row source、FMA、ILP、LMUL 或 production candidate。
- 不运行 QEMU timing 或板卡 bench；性能结论沿用既有 summary-only 证据。

## S0 和 dirty isolation

| 项 | 当前状态 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`、`rvv-workflow`、`rvv-test` 和 phase loop 规则；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 要求恢复 phase loop、检查 git status 和 dirty isolation。 |
| 注释策略 | test-rvv / diagnostic / prototype 详细中文；production 注释只解释维护边界、fallback、dispatch、数值风险和数据布局。 |
| 文档策略 | closeout 当前状态优先；长期文档不写对话流程；英文术语首次出现带中文解释。 |
| 证据策略 | summary-only；raw logs 不默认提交；QEMU 只用于 correctness、路径和日志形状。 |
| dirty isolation | 当前 dirty 分为两类：topic 文件和 agent asset patch。Phase 010 只允许修改本 topic 的测试资产、phase 文档和必要 topic 文档；`.agents/` dirty 作为单独 agent asset review 边界，当前阶段不继续修改。 |

当前允许路径：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
```

本阶段预计不再修改 production header；它只因 Phase 000 的 reference cleanup 仍处于 dirty 状态。
`.agents/skills/...` 的 dirty 改动不纳入本阶段产物，提交或审查时应 separate review。

## 当前状态清单

| 领域 | 当前事实 | 路径 |
| --- | --- | --- |
| production candidate | full-cloud f32 AoS layout-gated `Scalar=float` fused-formula block dispatch，三类代表点型 5-run 板卡正向。 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |
| Phase 000 | reference / production-detail boundary cleanup 已完成；std/RVV QEMU correctness 40/40 通过。 | `doc/phases/000-current-state-and-gaps/result.zh.md` |
| test source layout | `test_*.cpp` 和 `bench_*.cpp` 仍在 topic 根目录，文件名过长。 | topic 根目录 |
| test support architecture | 已有聚合头和 `test_support/` 内部分层，但聚合头不在配置解析出的 `include/` 目录。 | `test_support_transformation_estimation_point_to_plane_lls.hpp`、`test_support/` |
| sibling structure audit | weighted sibling 使用 `src/`、`include/`、`include/impl/` 和 `teptplw` 缩写。当前 topic 只采纳结构质量 bar，不迁移算法和 source-indexed production 方案。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/` |
| evidence registry | topic 尚无 `log/evidence_registry.json`。 | Phase 000 人工检查 |

## 假设与候选族

| 假设 | 本阶段验证方式 |
| --- | --- |
| 只迁移源码路径和聚合入口不会改变测试、bench 或 production 行为。 | 使用 `git diff --check`、`run_test_std`、`run_test_rvv` 验证；必要时只编译 bench 或运行窄 QEMU bench smoke。 |
| 旧聚合头作为 compatibility alias 可以降低文档和外部引用迁移风险。 | 旧头只 include 新 `include/teptpl.h`，文件级注释说明 alias 角色；Makefile 和新源码使用新入口。 |
| 不立即移动 `test_support/` 到 `include/impl/` 更适合当前 phase。 | 在 result 中记录 `deferred` 理由：减少 churn，避免同时改 include graph 和大型 helper 路径；若后续拆分测试源文件，再考虑内部目录迁移。 |

## 本阶段优化矩阵

本阶段新增 `test harness layout migration` 行，见 `../optimization-matrix.zh.md`。生产候选状态不变。

## 实现和测试动作

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| L0 | 写本 phase plan 并冻结范围。 | 本文件。 | plan 先于 Phase 010 实现 / 测试资产修改存在。 |
| L1 | 迁移源码入口。 | `src/test_teptpl.cpp`、`src/bench_teptpl.cpp`；Makefile `SRCS_TEST/SRCS_BENCH` 更新。 | std/RVV 构建仍使用同一 target 名；旧根目录大 cpp 不再作为 Makefile source。 |
| L2 | 迁移聚合入口并保留兼容别名。 | `include/teptpl.h`、根目录 alias 头。 | 新 test/bench include 新聚合头；旧头不含实现，只转发。 |
| L3 | 文档同步 layout audit。 | Phase README / matrix、evaluation 或 topic doc 的窄更新。 | 写清 `src/include` adopted、`include/impl` rename deferred、compat alias 原因。 |
| L4 | 验证 layout 迁移。 | `git diff --check`、`make -C ... run_test_std`、`make -C ... run_test_rvv`。 | std/RVV QEMU correctness 通过；若失败，先修或标 blocked。 |
| L5 | 回填 phase result。 | `result.zh.md`。 | 每项动作有 `done / partial / deferred / blocked`、证据路径和 continue / stop decision。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 benchmark、board summary、checksum summary 或 asm attribution，因此不运行
JSON manifest 形式 Evidence Doctor。若运行 QEMU bench smoke，只写成 log-shape，不写性能结论。

`evidence_registry_status=not_available`。本阶段若仅覆盖 `log/qemu/run_test_std.log` 和
`log/qemu/run_test_rvv.log`，继续按人工检查路径记录；不把 generated logs 纳入默认提交边界。

## 板卡复跑预算和决策桶

本阶段板卡复跑预算为 `0`。理由是 layout 迁移不改变 RVV hot path、bench case、case filter、
dispatch gate 或目标硬件执行逻辑。若 L1-L2 以外发现必须改 bench case 或 RVV helper，新建后续
phase 并定义 board rerun budget。

既有 production-dispatch decision bucket 保持 `positive` within current production candidate boundary。

## 继续 / 停止条件

继续条件：

- L4 correctness 失败且可以在本 topic 测试资产内修复。
- Makefile/source path 迁移导致 bench binary 无法编译，需要补 include path 或 alias。

停止条件：

- L0-L5 闭合，且剩余动作只属于后续可选范围：拆分大型 test/bench 源文件、迁移 `test_support/`
  到 `include/impl/`、接入 evidence registry、压缩 production RVV helper 或扩大 row source。
- 继续需要修改 production hot path、bench case、dispatch gate 或运行板卡。

默认下一阶段：

- 若本阶段通过，`next_phase_default=ready_for_review`。
- 若 reviewer 要求进一步整理，建议新建 `020-test-source-split`，按 public semantics、candidate、
  row source、production direct 和 bench harness 拆分大型源文件。

## 文档更新清单

| 文档 | 本阶段动作 |
| --- | --- |
| `doc/phases/README.zh.md` | 增加 Phase 010 索引和默认恢复入口。 |
| `doc/phases/optimization-matrix.zh.md` | 增加 test harness layout migration 行。 |
| `doc/phases/010-test-harness-layout-audit/result.zh.md` | 阶段结束时新建并回填事实。 |
| `transformation_estimation_point_to_plane_lls-evaluation.zh.md` | 补充测试框架布局审计和验证结果。 |
| `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` | 如需，仅窄更新 Test-RVV Diagnostic 保留策略中的路径说明。 |
