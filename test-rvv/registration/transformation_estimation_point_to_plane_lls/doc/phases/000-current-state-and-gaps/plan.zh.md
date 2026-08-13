# Phase 000 Plan: Current State And Gaps

## 阶段意图和边界

本阶段恢复 phase loop（阶段循环）并收口当前 dirty 变更。当前 dirty 主线是把
full-cloud 标量 normal-equation reference（法方程参考链路）从 production detail
移回 `test-rvv/test_support/`，让 production header 只保留真实 runtime helper
和 RVV dispatch / fallback。阶段目标是证明这次边界清理不改变 production hot path、
bench case 或 dispatch gate，并让下一轮 worker 可以从 phase 文档恢复。

本阶段不做：

- 不新增 RVV 算法候选。
- 不扩大到 source-indexed、dual-indices、correspondences、weighted 或 `Scalar=double`。
- 不用 QEMU timing（QEMU 计时）做性能结论。
- 不重跑板卡，除非后续验证发现 hot path、bench case 或 dispatch gate 被改动。

## S0 和 dirty isolation

| 项 | 当前状态 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 要求恢复 phase loop、检查 git status 和 dirty isolation。 |
| 注释策略 | test-rvv / diagnostic / prototype 详细中文；production 注释只解释维护边界、fallback、dispatch、数值风险和数据布局。 |
| 文档策略 | closeout 当前状态优先；长期文档不写对话流程；英文术语首次出现带中文解释。 |
| 证据策略 | summary-only；raw logs 不默认提交；QEMU 只用于 correctness、路径和日志形状。 |
| dirty isolation | 进入本阶段前已有 5 个 dirty 文件，均属于本 topic：production header、topic doc、test_support、test cpp、evaluation。创建本 phase 文档属于本轮新增 topic test asset；不回退既有 dirty。 |

进入本阶段前的 dirty 文件：

```text
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
test-rvv/registration/transformation_estimation_point_to_plane_lls/test_support/transformation_estimation_point_to_plane_lls_common.hpp
test-rvv/registration/transformation_estimation_point_to_plane_lls/test_transformation_estimation_point_to_plane_lls.cpp
test-rvv/registration/transformation_estimation_point_to_plane_lls/transformation_estimation_point_to_plane_lls-evaluation.zh.md
```

## 当前状态清单

| 领域 | 当前事实 | 路径 |
| --- | --- | --- |
| production candidate | full-cloud f32 AoS layout-gated `Scalar=float` fused-formula block dispatch，代表点型板卡 5-run 正向。 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |
| production fallback | gate 失败回 `estimateRigidTransformationFullCloudStd`，该 helper 仍通过 `ConstCloudIterator` 进入原标量路径。 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` |
| reference cleanup | `buildPointToPlaneLLSFullCloudStd` 已从 production header 移除；tests 使用 `support::accumulate_std_full` 作为 expected。 | 当前 dirty diff |
| test support | `finite_point_and_normal` 和 `accumulate_std_full` 已模板化，支持 `PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` 等代表点型。 | `test_support/transformation_estimation_point_to_plane_lls_common.hpp` |
| evidence registry | topic 尚无 `log/evidence_registry.json`。 | 人工检查 summary / logs |
| phase loop | 本阶段之前没有 `doc/phases`。 | 本阶段新增 |

## 假设与候选族

| 假设 | 本阶段验证方式 |
| --- | --- |
| 移除 production detail 标量 reference 不改变 production RVV hot path。 | 代码审计确认 RVV helper、dispatch gate 和 bench case 未改；必要时 `git diff --check` 和 std/RVV correctness。 |
| test-only scalar reference 足够覆盖 production-facing normal-equation expected。 | std/RVV `run_test_*` 均通过，且 production RVV helper 输出转换为 test-support equation 后对拍。 |
| 现有板卡 summary 仍可作为 production candidate 性能主证据。 | 本阶段不改 hot path / bench / gate，因此不刷新板卡；在 result 中标明 freshness。 |

## 本阶段优化矩阵

见 `../optimization-matrix.zh.md`。本阶段只新增 / 更新 `reference / production-detail boundary cleanup`
行；其它候选族保持历史状态。

## 实现和测试动作

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| P0 | 建立 phase loop 资产。 | `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、本 plan。 | 路径存在，矩阵能记录当前 candidate 和本阶段动作。 |
| P1 | 审计 dirty diff 和遗留引用。 | `rg "buildPointToPlaneLLSFullCloudStd"`、production header / tests 读取。 | production header 不再暴露 test-only scalar normal-equation helper；文档不再把它列为 production helper。 |
| P2 | 补或修当前源码 / 测试小问题。 | 只限本 topic dirty 文件和 phase 文档。 | 不改变 production hot path、bench case 或 dispatch gate。 |
| P3 | 跑 correctness 和格式检查。 | `git diff --check`、`make -C ... run_test_std`、`make -C ... run_test_rvv`。 | std/RVV 测试通过；若失败，先修复或标 blocked。 |
| P4 | 回填 phase result、matrix 和长期文档。 | `result.zh.md`、evaluation / topic doc 必要小更新。 | 逐项列出 done / partial / deferred / blocked；Evidence registry 状态和 freshness 写清。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 benchmark（性能测试）或 board summary（板卡摘要），因此不运行
JSON manifest 形式 Evidence Doctor。对既有 summary 做人工 doctor：

- `production_dispatch_generic_representative_5run_summary.md`：summary-only，metadata incomplete，但已记录 run count、case filter、values、analyzer hash 和 raw archive 边界。
- `block_fused_formula_5run_summary.md`：diagnostic direct summary，不替代 production-dispatch。

`evidence_registry_status=not_available`。本阶段人工检查路径写入 result；若后续 official
target 覆盖 output summary，应接入 `test-rvv/script/evidence_registry.py` 或在 result 中标
`stale_doc_pending_refresh`。

## 板卡复跑预算和决策桶

本阶段预算为 `0` 次板卡复跑，因为预计改动只影响 test reference / production detail 边界，不改变
RVV hot path、bench case 或 dispatch gate。若 P2/P3 发现这些边界被改变，立即暂停当前预算，
新建后续 phase 并定义 board rerun budget。

当前沿用既有板卡决策桶：`positive` within current production candidate boundary。

## 继续 / 停止条件

继续条件：

- P3 correctness 失败但可在本 topic 内修复。
- 发现 production header 仍残留 test-only reference helper 或文档证据冲突。

停止条件：

- P0-P4 全部闭合，且没有本阶段授权内的未阻塞动作。
- 继续需要改 hot path、bench case、dispatch gate、板卡复跑或扩大到其它 row source policy。

默认下一阶段：

- 若 correctness 通过且文档同步完成，建议停在 `ready_for_review`。
- 若 reviewer 要求进一步压缩 RVV helper size，再另建 `010-helper-shape-review` 或等价 phase。

## 文档更新清单

| 文档 | 本阶段动作 |
| --- | --- |
| `doc/phases/README.zh.md` | 新建阶段索引。 |
| `doc/phases/optimization-matrix.zh.md` | 新建候选矩阵。 |
| `doc/phases/000-current-state-and-gaps/result.zh.md` | 阶段结束时新建并回填事实。 |
| `transformation_estimation_point_to_plane_lls-evaluation.zh.md` | 如测试结果或 registry 状态变化，补充当前 evidence freshness。 |
| `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` | 如 production helper 边界说明需同步，做窄更新。 |
