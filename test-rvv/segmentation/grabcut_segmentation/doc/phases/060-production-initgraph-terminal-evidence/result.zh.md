# Phase 060: production initGraph terminal evidence result

## 阶段结论

当前 production patch（生产补丁）通过 PI3/PI4 证据闭环，阶段完成时进入 PI5 用户检查点。
该阶段当时的 `pending_user_confirmation_adopt_production` 已被用户确认保留 / 采纳取代；Phase 070
随后补充 production-public（真实公开入口）wall-time 证据。本文件保留 Phase 060 的 production-detail
（生产细节）证据事实，当前 adopted truth（已采纳事实）见 Phase 070 result 和
`doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md`。

补丁范围仍严格保持 Phase 040 的 `pi2_scope`：只在 `initGraph()` 中批量计算 `TrimapUnknown` terminal
weight（端点权重）并调用真实 `setTerminalWeights()`，不覆盖 color staging（颜色暂存）、organized n-link
（规则图像邻接边）、non-organized KNN、max-flow（最大流）、public API（公开接口）、公共 RVV API 或
`Scalar=double`。

## 计划动作回填

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| 生产范围审计 | done | `segmentation/include/pcl/segmentation/grabcut_segmentation.h`、`segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` | 只新增私有 RVV helper 声明和 `initGraph()` 内 unknown terminal batch 分流。 |
| production correctness | done | `make run_test_compare` | Std 4/4、RVV 6/6 通过；`GrabCutProductionDirect.RvvTerminalWeightsMatchScalarAndFixedLabelsFallback` 通过。 |
| QEMU smoke | done | `make run_bench_std BENCH_ARGS="--width 64 --height 48 --iterations 1 --warmup 1 --case production_initgraph_terminal"`；`make run_bench_rvv BENCH_ARGS="--width 64 --height 48 --iterations 1 --warmup 1 --case production_initgraph_terminal"` | case 可运行，Std/RVV checksum 均为 `14306181153930413671`；QEMU timing 只作日志形状，不作性能结论。 |
| production asm | done | `build/asm/riscv/bench_grabcut_rvv.full.asm` | demangled 符号 `pcl::GrabCut<pcl::PointXYZRGB>::initGraphTerminalWeightsRVV()` 内含 `vsetvli`、`vle32.v`、`vse32.v`、`vfmacc.*`、`vfadd.vv` 和 `expf_RVV_f32m2` 内联链路的 `vfcvt` / `vfnmsub` / `vfmin` / `vfmax`。 |
| production board repeated | done | `doc/phases/060-production-initgraph-terminal-evidence/repeated-board-20260827-152529/` | Milkv-Jupiter 5-run B/A 为 `3.1292, 3.1280, 3.0998, 3.0944, 3.1982`，median `3.1280x`，positive bucket。 |
| production Evidence Doctor | done | `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 |
| registry | done | `log/evidence_registry.json`、`make evidence_status` | Phase 060 summary 已登记；`evidence_status` 为 fresh。 |

## 生产补丁范围

| 文件 | 变更 | 未触碰范围 |
| --- | --- | --- |
| `segmentation/include/pcl/segmentation/grabcut_segmentation.h` | 在 `__RVV10__` 下声明私有 `initGraphTerminalWeightsRVV()`。 | public API、模板参数、外部可见接口不变。 |
| `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` | 引入 RVV math helper 和 `riscv_vector.h`；新增 `initGraphTerminalWeightsRVV()`；`initGraph()` 在 RVV 成功后跳过 unknown scalar terminal loop。 | graph node allocation、fixed-label trimap、n-link edge、max-flow solver 和非 RVV 构建路径保持原语义。 |

## 板卡 repeated summary

| run label | Std avg ms | RVV avg ms | B/A |
| --- | --- | --- | --- |
| run01 | 478.437427 | 152.892135 | 3.1292 |
| run02 | 477.683276 | 152.713604 | 3.1280 |
| run03 | 475.098146 | 153.266078 | 3.0998 |
| run04 | 473.891141 | 153.144161 | 3.0944 |
| run05 | 487.538891 | 152.444021 | 3.1982 |

统计口径：5-run、`--width 640 --height 480 --iterations 8 --warmup 2 --case production_initgraph_terminal`，
设备为 Milkv-Jupiter。Std median 为 `477.683276 ms`，RVV median 为 `152.892135 ms`，B/A median 为
`3.127968x`。板卡命令输出中存在 `script/rvv-board-run.mk` clock skew（时钟偏移）warning；该 warning
来自板卡文件时间戳，不改变当前 manifest / checksum / Doctor 结果，但保留为环境风险。

历史目录 `repeated-board-20260827-152154` 和 `repeated-board-20260827-production` 各有一次空日志文件，
只作为 failed/manual run（失败或手工 run）背景，不作为当前证据。当前恢复指针为
`doc/phases/060-production-initgraph-terminal-evidence/repeated-board-20260827-152529`。

## Evidence Doctor 和 registry

| 文件 | 角色 |
| --- | --- |
| `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-manifest.json` | production-detail manifest；记录 run_count、warmup、B/A、checksum、timer boundary 和 repeated dir。 |
| `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.md` | Evidence Doctor summary；结果为 `Errors=0, Warnings=0, Suggestions=0`。 |
| `doc/phases/060-production-initgraph-terminal-evidence/repeated-evidence-doctor.json` | 机器可读 Doctor 摘要。 |
| `log/evidence_registry.json` | summary artifact registry；登记 Phase 030 和 Phase 060 的 manifest / Doctor 文件。 |

raw board logs 保持 local-only。summary-only（只提交摘要）策略下，只有被本 result、README、evaluation、
roadmap 或 phase index 引用的 summary artifact 才进入可审查提交候选。

## diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | 当前证据是 `production-detail`，通过测试专用子类调用真实 `GrabCut<PointXYZRGB>::initGraph()`。 |
| A/B boundary | `production_detail_helper`；baseline 是非 RVV 构建的真实 `initGraph()` terminal path，candidate 是 RVV 构建的 `initGraphTerminalWeightsRVV()`。 |
| 当前决策问题 | 当前 production patch 是否足以进入 PI5 用户确认。 |
| diagnostic 是否可外推到 production | 不再依赖外推；Phase 060 直接测真实 production detail。但它仍不证明完整 `extract/refineOnce` wall time。 |
| comparison-boundary / baseline mismatch 风险 | bench 使用 prepared state，n-link 数量为 0，不覆盖 max-flow；这些边界不影响 terminal-weight helper 的局部生产证据，但限制外推范围。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；当前 production-detail evidence 是 positive。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个 RVV family；若后续新增 family，必须补同一边界 RVV-vs-RVV A/B。 |

## worker_quality_gate_check

| gate（门禁项） | status（状态） | evidence（文件 / 章节） | missing_items（缺口） |
| --- | --- | --- | --- |
| preferences_loaded / work_preferences | pass | `AGENTS.md`、`.agents/config/defaults.yaml`、本 result | local override 不存在；默认 no commit、summary-only。 |
| phase_plan_written_before_edits | pass | `060-production-initgraph-terminal-evidence/plan.zh.md` | none |
| production_direct_results_ready | pass | `make run_test_compare` | public `extract/refineOnce` wall time 未覆盖。 |
| fallback_results_ready | pass | `GrabCutProductionDirect.RvvTerminalWeightsMatchScalarAndFixedLabelsFallback` | `Scalar=double` 不适用；非 RVV 构建由 Std 4/4 覆盖。 |
| asm_attribution_ready | pass | `build/asm/riscv/bench_grabcut_rvv.full.asm` | asm 文件位于 build 输出，默认不提交。 |
| board_evidence_paths_ready | pass | `repeated-evidence-manifest.json`、`repeated-evidence-doctor.md` | raw board logs 不默认提交。 |
| evidence_doctor_result_ready | pass | `repeated-evidence-doctor.md` | none |
| evidence_registry_status_ready | pass | `log/evidence_registry.json`、`make evidence_status` | fresh |
| bench_backend_choice_ready | pass | 本 result 的板卡 repeated summary | 性能结论只来自 Milkv-Jupiter；QEMU smoke 不计性能。 |
| qemu_bench_smoke_scope_ready | pass | QEMU smoke 命令 | QEMU 只证明日志形状和可运行性。 |
| rerun_budget_decision_ready | pass | 本 result 的板卡 repeated summary | 5-run 预算已用完且 bucket 稳定为 positive。 |
| production_public_vs_family_selection_ready | pass | diagnostic-to-production mismatch audit | 当前不是 RVV-family-selection。 |
| production_topic_doc_applicability_ready | pass / adopted after user confirmation | Phase 070 result、`doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` | 用户已确认采纳；长期生产文档适用。 |
| dirty_isolation_ready | partial | path-limited `git status --short --untracked-files=all` | topic 目录仍整体 untracked；提交前需排除 build/raw log 和旧失败 run。 |
| continue_stop_decision | pass | 本 result | 停止条件是 PI5 用户确认边界。 |

## Continue / stop decision

本阶段原始 `continue_stop_decision` 是
`turn_stop_deferred with stop_condition_hit=production_adoption_requires_user_authorization`。该停止条件已解除：
用户确认保留 / 采纳后，Phase 070 已补完整 public `extract()` wall time。当前恢复入口不再是 PI5，而是
Phase 070 closeout / final verification；若未来要求回滚，仍需明确回滚授权。
