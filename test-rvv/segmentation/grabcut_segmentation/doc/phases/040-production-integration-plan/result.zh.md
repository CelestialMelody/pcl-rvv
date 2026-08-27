# Phase 040: production integration plan result

## 阶段结论

PI1 production integration plan（生产接入计划）已完成。本文件记录的是 PI1 完成时的历史状态：
当时 EvidenceDecision（证据决策）为 `partial-production-candidate / PI2-blocked-on-user-authorization`，
且本阶段没有修改 production（生产源码）。当前 truth（当前事实）已由 Phase 060 取代：production patch
已存在并通过 production-detail evidence（生产细节证据），随后由用户确认保留 / 采纳，并由 Phase 070 补充 production-public evidence（真实公开入口证据）。

PI1 当时的停止条件是授权边界：PI2 production patch（生产补丁）会触碰
`segmentation/include/pcl/segmentation/grabcut_segmentation.h` 和
`segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp`。Phase 060 已完成 PI2-PI5 证据采集，
本文件保留 PI1 历史授权边界；当前状态以 Phase 060 / Phase 070 result 和长期 `doc-rvv` 为准。

## 计划动作回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 冻结 PI2 范围 | done | `040-production-integration-plan/plan.zh.md#pi2_scope` | 只允许尝试 `initGraph()` unknown trimap terminal branch 的 GMM probability + terminal cost batch。 |
| 冻结 forbidden expansion | done | `040-production-integration-plan/plan.zh.md#forbidden_expansion` | 不碰 public API、color staging、organized n-link、non-organized KNN、max-flow、公共 RVV math API 或其它 topic。 |
| fallback / dispatch 计划 | done | `040-production-integration-plan/plan.zh.md#dispatch--fallback-策略` | 非 RVV、小规模、非 unknown trimap、无效概率 / determinant、n-link 和 solver 均保持标量或原语义。 |
| production direct test 计划 | done | `040-production-integration-plan/plan.zh.md#production-direct-test-计划` | PI3 需要 public-shaped smoke、RVV hit instrumentation、fixed-label fallback 和 non-RVV build fallback。 |
| PI4 证据计划 | done | `040-production-integration-plan/plan.zh.md#PI4-证据计划` | PI4 需要 correctness、QEMU log-shape、production asm、board repeated 和 Evidence Doctor。 |
| diagnostic-to-production mismatch audit | done | `040-production-integration-plan/plan.zh.md#diagnostic-to-production-mismatch-audit` | Phase 030 只能支撑 bounded production probe（有界生产探针），不能支撑 clean adoption（干净采纳）。 |
| production patch | historical blocked / superseded | user authorization boundary；当前见 `060-production-initgraph-terminal-evidence/result.zh.md` | PI1 当时未修改 production；Phase 060 已实现并验证窄范围补丁。 |

## doc_suite_role_inventory

| role | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | README 已列 evaluation、roadmap、phase index、Phase 050/060 result。 | 当前恢复入口已改为 PI5 用户确认。 |
| testing_overview | `merged:doc/grabcut_segmentation-evaluation.zh.md#测试计划和-bench-计划` | 当前测试 / bench 规模可由 evaluation 和 README 恢复。 | 若用户要求更完整 public wall-time，再补 standalone overview 或新 phase result。 |
| correctness_tests | `merged:doc/grabcut_segmentation-evaluation.zh.md#测试计划和-bench-计划` | 当前 gtest 角色已在 evaluation 表中列出。 | Phase 060 已补 production direct test 说明。 |
| benchmark_and_evidence | `merged:doc/phases/*/result.zh.md` | Phase 020/030/060 result 和 repeated manifest / doctor 是当前性能主归属。 | 用户确认采纳后只在长期 `doc-rvv` 写生产摘要。 |
| optimization_evidence | `standalone:doc/phases/optimization-matrix.zh.md` | matrix 已列 production terminal helper、bench、board、asm、doctor 和 PI5 decision。 | none before user confirmation。 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | roadmap 默认恢复队列已停在 PI5 用户确认。 | 用户确认后进入 production closeout 或回滚。 |
| test_support_code_map | `merged:doc/grabcut_segmentation-evaluation.zh.md#Traceability Map` | Traceability Map 已覆盖 production helper、test helper、bench、script 和 evidence output。 | 用户确认采纳后同步长期文档。 |
| phase_index | `standalone:doc/phases/README.zh.md` | phase index 已加入 Phase 050 和 Phase 060。 | none before user confirmation。 |
| evaluation_diagnostic | `standalone:doc/grabcut_segmentation-evaluation.zh.md` | evaluation 已写当前 EvidenceDecision、诊断证据链和 production 接入判断。 | PI5 后转入 production evaluation closeout。 |
| production_topic_doc | `standalone:doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` | 用户已确认保留 / 采纳 production patch，Phase 070 production-public evidence positive。 | 当前轮创建 / 更新长期生产文档。 |

## test support shape scan 和 target 粒度审计

| area | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| test / bench source layout | 已在 `src/test_grabcut.cpp` 和 `src/bench_grabcut.cpp`，匹配 `artifact_layout.source_subdir=src`。 | adopted | `wc -l` 显示 226 / 505 行，未超过 hard split 线。 | PI3 新增 tests 时维持 `src/`。 |
| aggregator / internal helpers | 聚合头为 `include/grabcut_diagnostic.h`，内部 helper 为 `include/impl/grabcut_diagnostic_reference.hpp`。 | adopted | 匹配 `test_support.aggregator_directory=include` 和 `internal_directory=include/impl`。 | 若 helper 继续增长到多职责过重，再拆 fixtures / candidates。 |
| script and manifest wrapper | topic-local `script/generate_grabcut_board_evidence_manifest.py` 生成 manifest，Makefile 有 phase-specific doctor target。 | adopted | Phase 030 repeated doctor 为 `Errors=0, Warnings=0, Suggestions=0`。 | PI4 需要 production manifest 字段。 |
| target granularity | 有 `run_test_compare` aggregate、case-filter bench、QEMU smoke、board smoke、phase-specific doctor target 和 production repeated doctor target。 | adopted | Phase 060 已通过 `production_initgraph_terminal` case 验证 production detail。 | 完整 public `extract/refineOnce` wall time 可作为用户要求时的补证据 phase。 |
| evidence registry | 当前有 `log/evidence_registry.json`；Phase 050/060 summary artifact 已接入 `evidence_status`。 | adopted | `make evidence_status` 是恢复和提交前 freshness check。 | none |

## 诊断证据链边界

Phase 030 当前主证据仍是：

- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/030-initgraph-no-solve-diagnostic/result.zh.md`

这些证据证明：在 synthetic BGR samples（合成 BGR 样本）和 lightweight terminal write sink 边界内，GMM / terminal 路线没有被 no-solve 外壳稀释。它们不能证明：真实 `graph_.addSourceEdge` / `addTargetEdge` 成本、n-link `addEdge`、fixed-label trimap、public `extract/refineOnce` wall time、production fallback 或 PI5 采纳。

## worker_quality_gate_check

| gate（门禁项） | status（状态） | evidence（文件 / 章节） | missing_items（缺口） |
| --- | --- | --- | --- |
| preferences_loaded / work_preferences | pass | `AGENTS.md`、`.agents/config/defaults.yaml`、S0 恢复摘要 | local override 不存在；默认 no commit。 |
| phase_plan_written_before_edits | pass | `040-production-integration-plan/plan.zh.md` | none |
| diagnostic_to_production_mismatch_audit_ready | pass | `040-production-integration-plan/plan.zh.md#diagnostic-to-production-mismatch-audit` | none |
| evidence_role_and_ab_boundary_ready | pass | Phase 030 result 和 PI1 plan | production A/B 仍待 PI4。 |
| bench_backend_choice_ready | pass | Phase 030 result | 性能结论只来自板卡 repeated；QEMU smoke 不计性能。 |
| evidence_doctor_result_ready | pass | `030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md` | production doctor 待 PI4。 |
| rerun_budget_decision_ready | pass | Phase 030 result | PI4 需重新设 production rerun budget。 |
| production_topic_doc_applicability_ready | pass / pending user confirmation | 本 result 的 `doc_suite_role_inventory`、Phase 060 result | `doc-rvv` 用户确认采纳前不适用。 |
| pi1_production_scope_ready | pass | PI1 plan `pi2_scope`、Phase 060 result | PI2-PI5 已按该 scope 执行；PI5 等确认。 |
| generic_point_type_strategy_ready | pass | PI1 plan `point type / layout` 行 | terminal batch 不直接读 `PointT`；若 PI2 扩到 color staging 必须重新打开 traits gate。 |
| fallback_dispatch_strategy_ready | pass | PI1 plan `dispatch / fallback 策略` | fallback tests 待 PI3。 |
| production_direct_test_plan_ready | pass | PI1 plan `production direct test 计划` | tests 待 PI3。 |
| artifact_tracking_status_ready | partial | `git status --short --untracked-files=all -- test-rvv/segmentation/grabcut_segmentation` | topic 目录整体 untracked；提交前需精确选择 topic 产物并排除 build/log raw。 |
| continue_stop_decision | pass | 本 result 阶段结论 | 停止原因是生产源码授权边界。 |

## Continue / stop decision

`continue_stop_decision` 的当前解释已由 Phase 060 和 Phase 070 更新：用户已确认保留 / 采纳当前窄范围
production patch，接入后 `public_extract` production-public evidence 为 positive。当前不再停在 PI5，
而是进入 production closeout / final verification。若未来要回滚，仍必须先取得明确回滚授权。
