# Phase 020 Result: Roadmap And Evaluation Recovery

## 执行范围

本阶段按 plan 补齐 phase loop（阶段循环）的恢复资产和文档主归属。实际执行范围只覆盖
topic-local 文档与导航：新增 `doc/optimization-roadmap.zh.md`，将 evaluation（函数级评估）
主文档迁到 `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`，根目录旧路径保留为
legacy pointer（旧路径指针），并更新 phase README、optimization matrix 和主题长期文档引用。

production header、RVV hot path、dispatch gate、fallback gate、测试逻辑、bench case 和 case filter
均未在本阶段修改。

## 计划动作完成矩阵

| id | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| R0 写 phase plan | done | `doc/phases/020-roadmap-and-evaluation-recovery/plan.zh.md` | plan 先于 Phase 020 文档迁移存在，范围锁定为文档恢复。 |
| R1 新增 roadmap | done | `doc/optimization-roadmap.zh.md` | 记录当前 production candidate 边界、候选搜索空间、阶段反思新增路线和暂缓 / 拒绝路线。 |
| R2 迁移 evaluation 主路径并保留旧入口 | done | `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`；根目录 `transformation_estimation_point_to_plane_lls-evaluation.zh.md`。 | 新路径承载完整评估和 Traceability Map；旧路径只作为 legacy pointer。 |
| R3 同步文档导航和引用 | done | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`。 | 文档主归属和恢复入口指向新 evaluation / roadmap。 |
| R4 验证文档迁移 | done | `git diff --check`；`rg` 引用检查；`run_test_std`；`run_test_rvv`。 | diff check 干净；新主路径可被引用；std/RVV QEMU correctness 均 40/40 通过。 |
| R5 回填 phase result | done | 本文件。 | 阶段闭合，当前默认进入 review。 |

## 文档归属决策

| 审计项 | 决策 | 事实和理由 |
| --- | --- | --- |
| optimization roadmap | adopted | 新增 `doc/optimization-roadmap.zh.md`。roadmap 现在是 helper shape review、evidence registry adoption、test source split、更多点型、`Scalar=double` 和 indexed / correspondences follow-up 的主恢复入口。 |
| evaluation 主路径 | adopted | 按 `artifact_layout.evaluation_doc_template` 迁到 `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`。 |
| 根目录旧 evaluation | adopted as legacy pointer | 旧文件只说明新主路径，避免旧引用立刻失效；不再复制完整评估正文。 |
| Traceability Map | adopted | 新 evaluation 主文档新增 Traceability Map（可追踪性地图），覆盖 production public entry、dispatch / fallback、RVV helper、test support aggregator、reference、production direct tests、bench rows、board summary、matrix 和 roadmap。 |
| topic-local doc suite | deferred | 当前已具备 `doc/phases/`、roadmap 和 evaluation 主路径。README / testing-overview / correctness-tests / benchmark-and-evidence 等完整 doc suite 可在 reviewer 要求或新增测试前另开 phase。 |

## 验证结果

| 命令 | 结果 | 证据路径 / 边界 |
| --- | --- | --- |
| `git diff --check` | pass | 命令无输出。 |
| `rg -n "主归属\|为主归属\|函数级决策审计\|Traceability Map\|optimization-roadmap\|legacy pointer\|旧路径指针" ...` | pass | 新主题文档、phase README、evaluation 主文档和旧 pointer 均指向 `doc/` 主路径；历史 phase plan/result 中的旧路径保留为当时事实。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std` | pass，40/40 tests | `log/qemu/run_test_std.log`，本机 generated log，默认不提交。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv` | pass，40/40 tests | `log/qemu/run_test_rvv.log`，本机 generated log，默认不提交。 |

QEMU correctness（QEMU 正确性验证）只证明构建、路径和功能，不证明目标硬件性能。本阶段没有运行
QEMU bench、反汇编或板卡，因为改动不触碰 RVV hot path 指令逻辑、bench case 或 dispatch gate。

## Evidence Doctor、Registry 和 Freshness

本阶段没有生成新的 benchmark、board summary（板卡摘要）、checksum summary（校验和摘要）或
asm attribution（反汇编归属），因此没有运行 JSON manifest 形式 Evidence Doctor（证据体检）。
人工检查结论：

| 输入 | 结果 | 处理 |
| --- | --- | --- |
| `output/board/production_dispatch_generic_representative_5run_summary.md` | 当前 production-dispatch 性能主证据；本阶段未刷新。 | 继续作为 summary-only evidence。 |
| `output/board/block_fused_formula_5run_summary.md` | fused-formula direct diagnostic 历史证据；本阶段未刷新。 | 继续作为辅助诊断证据。 |
| `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | 本轮 correctness generated logs，被 Make target 覆盖。 | 本机验证证据，不进入默认提交边界。 |

`evidence_registry_status=not_available`。本 topic 仍没有 `log/evidence_registry.json`。roadmap 已把
`030-evidence-registry-adoption` 列为可恢复后续项。

## Optimization Matrix 更新

`optimization-matrix.zh.md` 中新增 `roadmap / evaluation recovery` 行，并在本阶段结束时更新为
`done / ready_for_review`。其它候选状态不变：

- fused-formula block-reduction full-cloud production dispatch 仍是当前 production candidate。
- source-indexed、dual-indices、correspondences 仍保持 historical diagnostic / no-production。
- reference / production-detail boundary cleanup 和 test harness layout migration 保持 done。

## 板卡复跑预算与决策桶

本阶段计划预算为 0 次板卡复跑，实际复跑 0 次。理由是文档恢复不改变 RVV hot path、bench case、
case filter、dispatch gate 或 fallback gate。既有 production-dispatch 5-run decision bucket 保持
`positive` within current production candidate boundary。

## Dirty Isolation

当前可审查 topic diff 包含：

```text
doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

`.agents/` 下已有 agent asset patch 属于 separate review，不纳入本 phase 的 topic 产物。`build/`、
`log/` 和 raw output 仍是本机生成物 / 忽略边界，默认不提交。

## Continue / Stop Decision

当前 phase 完成，`unblocked_next_actions=none` within this phase。停止条件命中：

- R0-R5 已闭合。
- roadmap 和 evaluation 主归属已可恢复。
- 继续推进需要进入新的范围：拆分大型 test/bench 源文件、迁移 `test_support/` 到 `include/impl/`、
  接入 evidence registry、压缩 production RVV helper、扩大 row source 或重跑板卡。

`next_phase_default=ready_for_review`。若 reviewer 要求继续当前 topic，推荐从 roadmap 选择一个窄 phase：

- `030-evidence-registry-adoption`：接入 topic-local `log/evidence_registry.json` 或等价 freshness check。
- `030-helper-shape-review`：只审查 / 压缩 production RVV block helper size，并重跑 correctness。
- `030-test-source-split`：拆分大型 test / bench 源文件，保留 target 名和测试语义。
