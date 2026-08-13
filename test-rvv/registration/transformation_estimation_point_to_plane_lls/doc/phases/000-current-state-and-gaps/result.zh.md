# Phase 000 Result: Current State And Gaps

## 执行范围

本阶段按计划恢复 phase loop（阶段循环），并验证当前 dirty 变更：full-cloud 标量
normal-equation reference（法方程参考链路）从 production detail 移回
`test-rvv/test_support/` 后，production RVV hot path、bench case 和 dispatch gate 没有改变。

## 计划动作完成矩阵

| id | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| P0 建立 phase loop 资产 | done | `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、本阶段 plan/result。 | 下一轮短 prompt 可从 phase 文档恢复。 |
| P1 审计 dirty diff 和遗留引用 | done | `rg "buildPointToPlaneLLSFullCloudStd"`；读取 production header、test support、test cpp、topic doc、evaluation。 | production header 不再暴露 test-only scalar normal-equation helper；剩余引用只在测试 / 文档中说明该 helper 已移除。 |
| P2 修当前源码 / 测试小问题 | done | 进入本阶段前的 dirty diff 已包含所需清理：tests 改用 `support::accumulate_std_full`，`test_support` reference 已模板化。 | 未新增算法候选，未改 RVV hot path、bench case 或 production dispatch gate。 |
| P3 correctness 和格式检查 | done | `git diff --check`；`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std`；`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv`。 | diff check 干净；std QEMU correctness 40/40 通过；RVV QEMU correctness 40/40 通过。 |
| P4 回填 result、matrix 和长期文档 | done | 本文件；`optimization-matrix.zh.md`；evaluation / topic doc 的当前 dirty 更新。 | 阶段闭合，当前默认进入 review。 |

## 当前 diff 结论

production header 中移除了不参与 runtime fallback 的 `buildPointToPlaneLLSFullCloudStd`
及其行级标量 reference helper。真实 fallback 仍是 `estimateRigidTransformationFullCloudStd`
构造 `ConstCloudIterator` 后进入原标量 helper；RVV path 仍由
`buildPointToPlaneLLSFullCloudBlockRVVFusedFormula` 和
`estimatePointToPlaneLLSFullCloudRVV` 承担。

测试侧用 `support::NormalEquation` 作为 expected normal-equation 类型，RVV build 下把
production RVV helper 的输出转换成 test-support equation 再对拍。`finite_point_and_normal`
和 `accumulate_std_full` 已模板化，因此 `PointXYZ -> PointNormal`、
`PointXYZ -> PointXYZINormal` 和 layout fallback case 仍能用同一 test-only scalar
reference。

## 验证结果

| 命令 | 结果 | 证据路径 |
| --- | --- | --- |
| `git diff --check` | pass | 命令无输出。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std` | pass，40/40 tests | `log/qemu/run_test_std.log`，本机 generated log，默认不提交。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv` | pass，40/40 tests | `log/qemu/run_test_rvv.log`，本机 generated log，默认不提交。 |

QEMU correctness（QEMU 正确性验证）只证明构建、路径和功能，不证明目标硬件性能。本阶段没有运行
QEMU bench、反汇编或板卡，因为改动没有触碰 RVV hot path 指令逻辑、bench case 或 dispatch gate。

## Evidence Doctor 与 freshness

本阶段没有生成新的 benchmark、board summary 或 asm attribution（反汇编归属），因此没有运行 JSON
manifest 形式的 Evidence Doctor（证据体检）。人工检查结果：

| 输入 | 结果 | 处理 |
| --- | --- | --- |
| `output/board/production_dispatch_generic_representative_5run_summary.md` | summary-only；metadata incomplete，但记录 run count、case filter、values、analyzer hash 和 raw archive 边界。 | 继续作为当前 production-dispatch 板卡性能主证据；本阶段不刷新数值。 |
| `output/board/block_fused_formula_5run_summary.md` | summary-only；diagnostic direct A/B，不替代 production-dispatch。 | 继续作为 fused-formula direct diagnostic 历史证据。 |
| `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | 本轮 correctness generated logs；未被 git 跟踪。 | 作为本轮本机验证证据，不进入默认提交边界。 |

`evidence_registry_status=not_available`。本 topic 仍没有 `log/evidence_registry.json`；本阶段已人工检查
tracked board summary 和本轮 generated QEMU correctness logs。若后续 official target 覆盖
`output/board` summary 或需要提交 generated logs，应先接入 registry 或在新 phase 中标出
`stale_doc_pending_refresh`。

## Optimization Matrix 更新

`optimization-matrix.zh.md` 中 `reference / production-detail boundary cleanup` 已从
`in_progress` 更新为 `done / ready_for_review`。其它候选状态不变：

- fused-formula block-reduction full-cloud production dispatch 仍是当前 production candidate。
- current block 仍是 historical baseline，不是 production selector。
- source-indexed、dual-indices、correspondences 仍保持 historical diagnostic / no-production，不继承 full-cloud 结论。

## 板卡复跑预算与决策桶

本阶段计划预算为 0 次板卡复跑，实际复跑 0 次。理由是当前变更只清理 reference / production-detail
边界，不改变 RVV hot path、bench case 或 dispatch gate。既有 production-dispatch 5-run 结论的
decision bucket 保持 `positive` within current boundary；若后续修改 hot path、case 或 gate，必须另建
phase 并定义新的 rerun budget。

## Continue / Stop Decision

当前 phase 完成，`unblocked_next_actions=none` within this phase。停止条件命中：

- P0-P4 已闭合。
- 继续优化需要进入新的审查范围，例如压缩 RVV block helper、接入 evidence registry、重跑板卡、或扩展到其它 row source policy。

`next_phase_default=ready_for_review`。若 reviewer 要求继续当前 topic，建议按具体范围新建 phase：

- `010-helper-shape-review`：只审查 / 压缩 RVV block helper size，完成后重跑 correctness。
- `010-evidence-registry-adoption`：把本 topic 的 generated evidence 接入 `log/evidence_registry.json`。
- indexed / correspondences profile 或消融应另开 follow-up，不应从 full-cloud production candidate 直接继承。
