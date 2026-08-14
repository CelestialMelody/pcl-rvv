# Phase 040 Result: production-integration-plan

> Phase 040 是历史 PI1 计划结果；当前 production 结论见 Phase 050 result。

## 计划版本和实际范围

本阶段完成 PI1 production integration plan（生产接入计划）。实际范围与计划一致：
只更新 topic-local 文档、phase 状态和 Handoff Packet。Production 源码没有修改。

## 执行动作回填

| id | 状态 | 产物 / 命令 | 结论 |
| --- | --- | --- | --- |
| A1 恢复 Phase 020 状态 | done | 读取 Handoff、phase README、roadmap、matrix、evaluation 和 Phase 020 result。 | 默认恢复入口确认为 `040-production-integration-plan`。 |
| A2 读取 production gate 规则 | done | `rvv-implementation/SKILL.md`、`point-load-store.md`、`fallback-and-dispatch.md`、`RVV Generic Point Type Strategy.zh.md`。 | PI1 选择 exact `PointXYZ -> PointXYZ` 初始 gate，泛型点类型保持 deferred。 |
| A3 冻结候选范围 | done | `plan.zh.md` 的“PI1 候选范围”。 | 只允许 ordered-cloud-pair public overload、`PointXYZ -> PointXYZ`、`Scalar=float`、dense finite、`n >= 16`。 |
| A4 冻结 fallback matrix | done | `plan.zh.md` 的“Fallback Matrix”。 | non-RVV、double、非 `PointXYZ`、indices、correspondences、小规模、非 dense / 非有限输入均回退标量。 |
| A5 规划 PI2-PI5 | done | `plan.zh.md` 的实现、测试、Evidence Doctor、board 和 PI5 决策章节。 | PI2 需要用户授权；PI4 必须重跑 production direct evidence。 |
| A6 同步文档和 Handoff | done | README、roadmap、matrix、evaluation、optimization evidence、phase README、当前 Handoff。 | `next_phase_default` 更新为 `PI2-production-patch-if-user-authorizes`。 |
| A7 验证 | done | `make evidence_status`、`git diff --check`、YAML parse、path-scoped status。 | 见本文件“Validation”。 |

## PI1 决策

| 决策项 | 结果 | 理由 |
| --- | --- | --- |
| production patch | not_started | PI2 会修改 production 源码，需要用户明确授权。 |
| candidate scope | frozen narrow | Phase 020 证据只覆盖 test-only `PointXYZ -> PointXYZ`、`Scalar=float`、ordered-cloud-pair。 |
| generic point type | deferred | 泛型 traits / offset / representative board evidence 未闭合。 |
| `Scalar=double` | fallback | double reduction 和最终矩阵误差预算未验证。 |
| indices / correspondences | fallback | row source gather、query / match 展开和 public semantics 未闭合。 |
| `doc-rvv` 长期主题文档 | not_applicable | 本阶段完成时尚无 adopted production behavior、production patch 或 PI5 证据闭环；后续阶段已另行闭合。 |

## Production Integration Plan 摘要

PI2 若被授权，只能尝试 ordered-cloud-pair public overload。实现形态应保持 public entry（公开入口）简短：
先执行现有数量检查，再在 `__RVV10__` 下调用窄范围 RVV helper；helper 成功时写回矩阵并返回，
失败时自然落回现有 `ConstCloudIterator` 标量 helper。

PI3 必须补 production direct correctness 和 fallback tests。PI4 必须生成 production public bench、
production asm attribution、board repeated summary、manifest、Evidence Doctor 和 registry 记录。
PI5 才能再次给 EvidenceDecision。

## Evidence Doctor

本阶段没有新增 benchmark、board summary、checksum summary 或 asm attribution 输出，因此不运行新的
Evidence Doctor 脚本。PI1 计划引用的 Phase 020 证据仍为：

| input | Errors | Warnings | Suggestions | 当前角色 |
| --- | ---: | ---: | ---: | --- |
| `log/qemu/evidence_manifest.json` | 0 | 0 | 0 | QEMU smoke contract；不证明性能。 |
| `log/board/ordered_cloud_pair_repeated/evidence_manifest.json` | 0 | 0 | 0 | pre-production diagnostic；支持 PI1，不替代 production evidence。 |

PI4 必须为 production direct evidence 生成新的 manifest / doctor，不能复用 Phase 020 的诊断 manifest。

## Registry 状态

Phase 040 没有生成新 evidence output（证据输出）。本阶段只把新 phase 文档加入 `evidence_status`
的文档扫描输入，确保现有 registry 能继续检查 Phase 020 证据是否仍被当前文档引用。

## Optimization Matrix 更新

| candidate family | row source policy | decision | unblocked next action |
| --- | --- | --- | --- |
| production dispatch | ordered-cloud-pair | `PI1-plan-complete / PI2-needs-user-authorization` | 用户授权后进入 `PI2-production-patch-if-user-authorizes`。 |
| production dispatch | generic point types | deferred | 需要 traits / offset / fallback tests 和 representative board evidence。 |
| production dispatch | indexed / correspondences | not_applicable in PI1 | 若不授权 PI2，恢复到 `030-row-source-family-carryover`。 |

## 诊断证据链和生产边界

当前证据链仍停在诊断层：

- correctness：Phase 010 / 020 的 `run_test_compare` 和 `run_test_candidates`。
- QEMU：`log/qemu/evidence_manifest.json` 和 `log/qemu/evidence_doctor.md`。
- asm：`log/qemu/asm_attribution.md` 归属到 bench lambda 内联边界。
- board：`log/board/ordered_cloud_pair_repeated/summary.md` 为 test-only fused candidate 的 `weak_positive`。
- production boundary：production 源码未改；没有 production direct test、production asm 或 production board evidence。

## Worker Quality Gate Check

| gate（门禁项） | status（状态） | evidence（证据） | missing_items（缺口） |
| --- | --- | --- | --- |
| preferences_loaded | pass | Handoff `preferences_loaded`。 | none |
| comment_policy_frozen | pass | defaults: test-rvv / diagnostic 详细中文，production 克制。 | none |
| evidence_policy_frozen | pass | defaults: `summary-only`，raw logs 不默认提交。 | none |
| documentation_policy_frozen | pass | defaults: closeout current-state-first，`doc-rvv` 只在 production 采用后适用。 | none |
| phase_plan_written_before_edits | pass | `doc/phases/040-production-integration-plan/plan.zh.md`。 | none |
| optimization_roadmap_ready | pass | `doc/optimization-roadmap.zh.md` 已更新。 | none |
| optimization_matrix_ready | pass | `doc/phases/optimization-matrix.zh.md` 已更新。 | none |
| evidence_doctor_result_ready | pass | Phase 020 doctor 0/0/0；本阶段没有新 evidence output。 | PI4 需要 production doctor。 |
| evidence_registry_status_ready | pass | `make evidence_status` 通过。 | none |
| pi1_production_scope_ready | pass | `plan.zh.md` 的候选范围和 fallback matrix。 | none |
| generic_point_type_strategy_ready | pass | 已读取 `RVV Generic Point Type Strategy.zh.md`；PI1 明确 deferred。 | 泛型扩展需后续证据。 |
| fallback_dispatch_strategy_ready | pass | `plan.zh.md` 的 fallback matrix。 | PI3 需要实现并测试。 |
| production_direct_test_plan_ready | pass | `plan.zh.md` 的 PI3 / PI4 动作表。 | PI2 授权后执行。 |
| bench_backend_choice_ready | pass | PI4 计划只用 board / target hardware 写性能结论。 | none |
| asm_attribution_ready | partial | Phase 020 只有 diagnostic asm；PI4 计划 production asm。 | 需要生产符号 / public boundary 归因。 |
| board_evidence_paths_ready | partial | Phase 020 board summary 是 pre-production diagnostic。 | PI4 需要 production public repeated summary。 |
| production_topic_doc_applicability_ready | pass | 当前 `doc-rvv` 为 not_applicable。 | PI5 通过后创建。 |
| continue_stop_decision | pass | 本 result 的 Continue / Stop Decision。 | none |
| dirty_isolation_ready | pass | Handoff `dirty_isolation` 和 path-scoped status。 | none |
| validation_summary_ready | pass | 本 result “Validation”。 | none |
| followup_options_ready | pass | Handoff `followup_options_for_user`。 | none |

## Validation

| 验证 | 状态 | 说明 |
| --- | --- | --- |
| evidence registry | pass | `make -C test-rvv/registration/transformation_estimation_2D evidence_status`。 |
| whitespace | pass | `git diff --check -- test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D`。 |
| handoff YAML | pass | `python3 - <<'PY' ... yaml.safe_load(...) ... PY`。 |
| path-scoped status | pass | 已扫描当前 topic、handoff 和 production target path。 |

## Continue / Stop Decision

`continue_stop_decision`：Phase 040 PI1 plan complete，停止在 production patch 授权边界。

`stop_condition_hit`：继续到 PI2 会修改 production 源码。

`next_phase_default` at Phase 040 completion：`PI2-production-patch-if-user-authorizes`。
当前默认恢复入口已经由 Phase 050 改为 `030-row-source-family-carryover` 或 no-production closeout review。

如果用户不授权 PI2，默认恢复到 `030-row-source-family-carryover`，继续在 test-rvv 范围内审计
source-indexed-cloud-pair、dual-indexed-cloud-pair 和 correspondence-pair。
