# Phase 010 计划：板卡证据与 production 边界

## 阶段意图和边界

本阶段补齐 board evidence（板卡证据）和 production boundary（生产边界）判断。Phase 000 已证明 test-only candidate（测试专用候选）在 correctness（正确性）、QEMU smoke（QEMU 小型验证）和 asm smoke（反汇编小型验证）层面可审查；它没有证明目标硬件性能，也没有证明真实 production dispatch（生产分流）命中 RVV。本阶段只在 topic-local 产物内补板卡 smoke、repeated board summary（重复板卡摘要）、Evidence Doctor（证据体检）和 S10 EvidenceDecision（证据决策）。

不可触碰路径：

- `registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp`
- `registration/include/pcl/registration/correspondence_rejection_poly.h`
- 其它 topic 的 `test-rvv`、`doc-rvv` 或 agent asset（代理资产）

允许触碰路径：

- `test-rvv/registration/correspondence_rejection_poly/**`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/**`

## 当前状态清单

| 项 | 当前状态 | 路径 / 说明 |
| --- | --- | --- |
| production 源码 | 未修改 | `git diff -- registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp registration/include/pcl/registration/correspondence_rejection_poly.h` 无输出。 |
| 标量语义 | 已重建 | `doc/correspondence_rejection_poly-evaluation.zh.md` 的标量流程与 Traceability Map。 |
| correctness | pass | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`。 |
| QEMU smoke | pass as log-shape | `log/qemu/analyze_bench_compare_edge_batch.log`、`log/qemu/analyze_bench_compare_acceptance.log`。QEMU timing 不进入性能结论。 |
| asm | partial | `build/asm/riscv/bench_correspondence_rejection_poly_rvv.asm`；helper 内联到 bench lambda，production hot symbol 未闭合。 |
| Evidence Doctor | pass for QEMU smoke | `log/qemu/edge_batch/evidence_doctor.md`、`log/qemu/acceptance_filter/evidence_doctor.md`。 |
| board | not_run in Phase 000 | 本阶段执行。 |
| EvidenceDecision | `diagnostic/no-production` | Phase 000 result 和 current Handoff。 |

## 写文件前 worker quality gate check

| gate | status | evidence | missing_items |
| --- | --- | --- | --- |
| preferences_loaded | pass | `AGENTS.md`、`.agents/config/defaults.yaml` 已读取；`.agents/local/user-preferences.yaml` absent；当前 prompt 限定 topic-local。 | none |
| frozen_policies | pass | 注释策略为测试资产 / diagnostic 详细中文，production 克制；证据策略为 summary-only；默认不 commit。 | none |
| scalar_path_ready | pass | 当前源码复核 `getRemainingCorrespondences`、`thresholdEdgeLength`、histogram / Otsu 和输出 `>` cut。 | none |
| production_to_diagnostic_mapping_ready | partial | Phase 000 只覆盖连续 squared distance 和连续 counter 数组；完整 public entry 当前是标量 public-entry-shaped smoke。 | 真实 production RVV dispatch 未建立。 |
| phase_plan_written_before_edits | pass | 本文件是 Phase 010 首个产物。 | none |
| optimization_roadmap_ready | pass | `doc/optimization-roadmap.zh.md` 默认恢复动作为 `010-board-and-production-boundary`。 | none |
| optimization_matrix_ready | pass | `doc/phases/optimization-matrix.zh.md` 已列 `edge_length_batch`、`accept_rate_filter`、`full_entry_diagnostic`。 | none |
| bench_backend_choice_ready | pass | Phase 010 的性能结论只使用 board / target hardware（目标硬件）。 | none |
| qemu_bench_smoke_scope_ready | pass | QEMU bench 只作为 Phase 000 log-shape smoke，不在本阶段生成性能结论。 | none |
| rerun_budget_decision_ready | pass | 本计划定义 5-run 起步、最多追加 1 轮同边界复核。 | none |
| evidence_doctor_result_ready | planned | 本阶段每个 repeated summary 生成 manifest 并运行 Evidence Doctor。 | 等待 board 输出。 |
| evidence_registry_status_ready | planned | 本阶段登记 board test、summary、manifest、doctor 到 `log/evidence_registry.json`。 | 等待 board 输出。 |
| dirty_isolation_ready | pass | 本阶段只触碰允许路径；无关 dirty paths 写入 Handoff。 | none |
| production_decision_ready | pass | 本计划明确不修改 production；EvidenceDecision 后再判断是否只输出候选边界。 | none |

## 候选和优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | doctor | decision rule |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | correspondences（对应关系索引路径）局部 edge 数组 | `PointXYZ` / `float` / 预构造连续 squared distance | test support candidate wrapper | Phase 000 gtest + QEMU smoke | `run_board_bench_edge_batch_repeated`，case-filter `edge-batch` | bench binary partial asm | `log/board/edge_batch_repeated/evidence_doctor.md` | board bucket 为 positive / weak_positive 时进入 production-shaped gather 计划；neutral / negative 保持 no-production。 |
| `accept_rate_filter` | contiguous counters（连续计数器） | `int` counters + `float` accept rate | test support candidate wrapper + scalar append tail | Phase 000 gtest + QEMU smoke | `run_board_bench_acceptance_repeated`，case-filter `acceptance-filter` | bench binary partial asm | `log/board/acceptance_filter_repeated/evidence_doctor.md` | board bucket 为 positive / weak_positive 时补 scalar-tail cost attribution；neutral / negative 保持 no-production。 |
| `full_entry_diagnostic` | correspondences（对应关系输入） | `PointXYZ` / `float` / fixed seed | real public entry with no RVV dispatch | Phase 000 public-entry-shaped gtest | `run_board_bench_full_entry_smoke` 可选 smoke | scalar production path | optional / manual | 只证明完整入口可运行和日志形状；不作为 RVV speedup 证据。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 board summary 支撑 | topic-local script 解析板卡 run-labelled logs，生成 `summary.md` 和 `evidence_manifest.json`。 | 能从 `run_bench_std.log` / `run_bench_rvv.log` 提取 case、checksum、iterations、warm-up 和每轮 Std/RVV timing。 |
| A2 Makefile board targets | 新增 run-labelled board smoke / repeated targets，避免覆盖裸 `log/board/analyze_bench_compare.log`。 | `edge_batch_repeated` 和 `acceptance_filter_repeated` 输出独立目录。 |
| A3 board correctness smoke | `make -C test-rvv/registration/correspondence_rejection_poly run_board_test`。 | board `run_test.log` 返回成功，并抓回本地 `log/board/run_test.log` 或 run-labelled 等价路径。 |
| A4 edge repeated board | `make -C ... run_board_bench_edge_batch_repeated`。 | 5-run summary、manifest、doctor 生成；doctor Error=0 或降级 / blocked。 |
| A5 acceptance repeated board | `make -C ... run_board_bench_acceptance_repeated`。 | 5-run summary、manifest、doctor 生成；doctor Error=0 或降级 / blocked。 |
| A6 registry / freshness | 用 `test-rvv/script/evidence_registry.py record/check` 登记本阶段 board summary、manifest、doctor、board test log。 | registry check `fresh`，或 result/Handoff 明确写 `stale_doc_pending_refresh` / blocked。 |
| A7 S10 文档 closeout | 更新 phase result、matrix、roadmap、evaluation、doc-rvv、current Handoff。 | EvidenceDecision 与 board bucket、doctor 结果、production 边界一致。 |

## Board 复跑预算和决策桶

默认参数：

- repeated run count：5。
- warm-up iterations：5。
- bench iterations：50。
- 最大追加复跑：1 轮同边界 5-run。只有 Evidence Doctor Warning、bucket 接近阈值、方向与当前文档冲突或 board 运行异常时触发。
- 统计口径：每个 case 计算 `speedup = std_ms / rvv_ms`；每轮每个 size 独立记录，summary 给出 min / median / max 和 `speedup < 1` 频率。

决策桶：

| bucket | 判断口径 | EvidenceDecision 影响 |
| --- | --- | --- |
| `positive` | 所有子 case median >= 1.20，且无 `speedup < 1`。 | 可进入 production-shaped gather / scalar-tail attribution 计划，但仍不直接改 production。 |
| `weak_positive` | 所有子 case median >= 1.05，且 `speedup < 1` 频率 <= 20%。 | 只支持局部 production-candidate 计划；必须补 production direct / fallback / asm。 |
| `neutral` | median 位于 [0.95, 1.05) 或不同子 case 方向混合。 | 保持 `diagnostic/no-production`，候选保留为诊断。 |
| `negative` | 任一关键子 case median < 0.95，或 `speedup < 1` 频率 > 20%。 | 保持 `diagnostic/no-production`，写负向归因假设。 |
| `unstable` | 追加复跑后 bucket 仍跨 positive / neutral / negative 摇摆，或 Evidence Doctor 未解决 Warning 影响方向判断。 | 降级为 blocked 或 no-production，交给 reviewer / 用户判断。 |

## Evidence Doctor 和 registry 规则

每个 repeated board output 使用 topic-local wrapper 生成：

- `log/board/<label>/summary.md`
- `log/board/<label>/evidence_manifest.json`
- `log/board/<label>/evidence_doctor.md`

Evidence Doctor 处理策略：

- Error：修正 summary / manifest 或重跑；不能用 Error 证据关闭性能结论。
- Warning：写入 `result.zh.md`、evaluation 和 Handoff，说明是否重跑、降级或保留风险。
- Suggestion：进入 `optimization-roadmap.zh.md` 或下一 phase。

Registry 处理策略：

- 本阶段 record 后运行 check。
- 如果发现 `unregistered_change` 或 `unregistered_file`，当前数值结论先标 `stale_doc_pending_refresh`。
- raw board logs 默认 local-only；summary、manifest、doctor 作为 summary artifact（摘要证据产物）可被文档引用。

## 继续 / 停止条件

继续到下一 phase 的条件：

- `edge_length_batch` 或 `accept_rate_filter` 的 board bucket 为 `positive` 或 `weak_positive`，且 Evidence Doctor 没有未处理 Error。
- 下一 phase 仍在 topic-local 诊断范围内，例如 production-shaped gather candidate、scalar-tail cost attribution 或更窄 asm attribution。

停止条件：

- 两个候选均为 `neutral` / `negative`，或追加复跑后 `unstable`。
- 继续需要修改 production、public API（公开接口）、其它 topic，或需要泛型点类型策略与 production dispatch 授权。
- board / SSH / 远端依赖不可用，且本阶段只能写 blocked Handoff。

默认下一 phase：

- 若 board bucket 正向：`020-production-shaped-gather-or-tail-attribution`，仍不改 production，先补 production-shaped diagnostic（生产形态诊断）。
- 若 board bucket 非正向：保持 `diagnostic/no-production` 并进入 no-production closeout。
- 若 board 阻塞：输出 blocked Handoff，下一动作是修复 board 配置后重跑 A3-A5。

## 文档更新清单

- `doc/phases/010-board-and-production-boundary/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/correspondence_rejection_poly-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `log/evidence_registry.json`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/current-handoff.zh.md`

## roadmap 同步动作

本阶段结束时必须把 board bucket 写回 roadmap：

- 正向：新增 production-shaped gather / scalar-tail attribution 的高优先级路线。
- 非正向：将对应 candidate 标为 attempted diagnostic，并写恢复条件。
- 阻塞：将 board evidence 标为 blocked，写解除阻塞后的命令。
