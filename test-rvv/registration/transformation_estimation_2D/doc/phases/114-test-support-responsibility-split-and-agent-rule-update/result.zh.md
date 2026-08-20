# TE2D 测试支撑职责拆分与 Agent 规则更新结果

## 阶段结论

Phase 114 是 test support（测试支撑）结构阶段，不是新的 RVV 性能优化阶段。本阶段不修改 production source（生产源码），不改变算法语义、gtest / bench case 名称、Make target 合同、CLI case-filter、checksum 字段或计时边界。

Commit A 已先单独完成 `.agents` workflow rule update：

```text
e22032462 workflow: make test support splits responsibility-first
```

该提交把规则改为 responsibility-first（职责优先）拆分：只要测试支撑文件混合多个稳定职责，就应先按职责拆；800/1000 行阈值只用于拆分后单一职责文件继续增长时的二次拆分判断。若不拆，后续 phase result / Handoff 必须写 `rejected with evidence` 或 `turn_stop_deferred with stop_condition_hit`，不能只因为“还能看”跳过。

## 实际执行范围

| 计划动作 | 状态 | 事实 / 证据 |
| --- | --- | --- |
| P1 职责和符号归属扫描 | done | 原测试支撑单头同时承载 core types、fixtures、layout helpers、row source、public wrappers、family A/B、ordered / source-indexed / dual-indexed / correspondence candidates 和 checksums，命中职责优先拆分条件。 |
| P2 创建新内部头并迁移实现 | done | `include/te2d.h` 保持稳定聚合入口；内部职责拆到 `include/impl/te2d_*.hpp`。 |
| P3 清理旧路径引用 | done | 旧内部入口文件已删除，不保留 compatibility aggregator（兼容聚合入口）或 legacy alias（旧别名）；限定扫描无旧文件名引用。 |
| P4 保持测试合同 | done | `src/test_te2d.cpp`、`src/bench_te2d.cpp`、Make target、case-filter 和输出字段未改名。 |
| P5 correctness 验证 | done | `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` 已重新编译 Std/RVV，两侧均 `84/84` pass。 |
| P6 QEMU smoke 验证 | done | `make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state` 通过，Std/RVV 均 `84/84`。QEMU 只证明 correctness / path / log-shape，不作为性能证据。 |
| P7 文档和状态同步 | done | README、test-support code map、correctness/evaluation/optimization docs、phase README、roadmap、matrix 已同步新结构；Handoff 待最终验证后回填。 |
| P8 提交前检查 | done | `evidence_status` fresh；YAML parse 输出 `ok`；`git diff --check` 无输出；staged set 审计只包含 TE2D topic 文件，未包含 `.agents`、tmp handoff、logs、production 或无关 dirty 文件。 |

## 最终文件职责

| 文件 | 职责 | 二次拆分状态 |
| --- | --- | --- |
| `include/te2d.h` | 稳定聚合入口，测试和 bench 只 include 它。 | 单纯聚合入口，无需二次拆分。 |
| `include/impl/te2d_core_types.hpp` | 公共依赖、`CandidateStats`、`Fused2DAccumulation`。 | 单一职责。 |
| `include/impl/te2d_fixtures.hpp` | 点云 fixtures、代表性点型样本和二维刚体变换构造。 | 单一职责。 |
| `include/impl/te2d_layout_helpers.hpp` | layout gate、finite/dense 检查和 stats 填充。 | 单一职责。 |
| `include/impl/te2d_row_sources.hpp` | source-indexed、dual-indexed、correspondence 的索引统计、合法性检查和物化。 | 单一职责。 |
| `include/impl/te2d_public_wrappers.hpp` | 调用真实 `TransformationEstimation2D` public overload 的 test-only wrapper。 | 单一职责。 |
| `include/impl/te2d_family_ab.hpp` | materialize-to-ordered 和 staged-dual public wrapper。 | 单一职责。 |
| `include/impl/te2d_ordered_candidates.hpp` | ordered-cloud-pair 标量 / RVV accumulation、求解和 candidate。 | 单一职责；低于二次拆分阈值。 |
| `include/impl/te2d_source_indexed_candidates.hpp` | source-indexed materialize 与 direct-gather candidates。 | 单一 row-source 职责。 |
| `include/impl/te2d_dual_indexed_candidates.hpp` | dual-indexed materialize 与 direct-gather candidates。 | 单一 row-source 职责。 |
| `include/impl/te2d_correspondence_candidates.hpp` | correspondence direct-gather、chunked staging 和 materialize candidates。 | 同属 correspondence row source；当前低于二次拆分阈值。 |
| `include/impl/te2d_checksums.hpp` | matrix diff 和 checksum helper。 | 单一职责。 |

每个新内部头都有中文文件级说明，并写清它只属于 RVV 测试支撑，不能证明 production dispatch。

## 偏差和修复

拆分过程中曾在机械移动时丢失部分 RVV-only helper 的 `__RVV10__` 条件编译边界，导致非 RVV 或 Std 构建会看到 RVV intrinsic（内建函数）相关实现。修复方式是只恢复原有条件编译保护，未改变公式、fallback、输入 gate、统计字段或 case 合同。受影响文件为 ordered、source-indexed、dual-indexed 和 correspondence candidates；修复后 `run_test_compare` 已通过。

## 证据边界

- correctness：`run_test_compare` 证明拆分后 Std/RVV correctness 没有语义回归。
- QEMU：`record_qemu_correctness_state` 已通过，Std/RVV 均 `84/84`；只用于正确性、路径和日志形状。
- board performance（板卡性能）：本阶段不运行，不生成新的性能结论。
- production：无 production source diff；Phase 103/104 仍是 guarded probe，Phase 106 source-indexed generic public variance 仍为 negative，Phase 111 Normal 类不接入且用户不继续推进 Normal 优化。

## 当前验证状态

| 命令 | 结果 |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_2D run_test_compare` | pass：Std `84/84`，RVV `84/84`。 |
| `make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state` | pass：Std `84/84`，RVV `84/84`。 |
| `make -C test-rvv/registration/transformation_estimation_2D evidence_status` | pass：`evidence registry check: fresh`；Phase 114 plan/result 已纳入 doc input list。 |
| YAML parse for current Handoff | pass：输出 `ok`。 |
| `git diff --check` | pass：无输出。 |
| old-entry scan | pass：旧内部入口文件名在 topic、`doc-rvv`、Handoff 和 `.agents` 中零命中。 |

## Artifact Tracking 和提交边界

Commit B 只应提交 TE2D test support 职责拆分、topic-local docs、Phase 114 plan/result、phase README、roadmap 和 matrix 同步。不提交 `.agents`、production source、`tmp/rvv-work-logs/**`、`log/**`、`build/**`、raw/generated logs、SVD scale、surface 或 library-screening dirty files。

## Continue / Stop Decision

当前仍需创建 Commit B。Commit B 通过后，Phase 114 默认恢复动作是 reviewer 检查 Commit B；不自动创建 Normal、correspondence、generic widening 或其它新 RVV 性能优化 phase。
