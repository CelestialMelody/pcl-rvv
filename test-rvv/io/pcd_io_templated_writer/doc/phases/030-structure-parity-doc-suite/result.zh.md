# Phase 030 结果：structure parity doc suite

## 实际执行范围

本阶段只更新 topic-local doc suite（主题本地文档套件），没有修改 production（生产源码），没有新增
板卡复跑，也没有改变 Phase 010 的证据数字或 Phase 020 PI1 production integration plan（生产接入计划）。

## Doc Suite Role Inventory

| role | status | path / section | evidence |
| --- | --- | --- | --- |
| topic_navigation | adopted | `README.zh.md` | 已写当前 EvidenceDecision、阅读路径、常用命令、证据白名单和 production doc 适用性。 |
| testing_overview | adopted | `doc/testing-overview.zh.md` | 已覆盖 target 粒度、测试流程、输入数据和覆盖矩阵。 |
| correctness_tests | adopted | `doc/correctness-tests.zh.md` | 已逐个说明 4 个 gtest 的输入、断言、证明范围和不能证明的范围。 |
| benchmark_and_evidence | adopted | `doc/benchmark-and-evidence.zh.md` | 已覆盖 case-filter、计时边界、summary、Doctor、registry 和提交边界。 |
| optimization_evidence | adopted | `doc/optimization-evidence.zh.md` | 已把 candidate family 映射到代码、test、bench、board、asm、Doctor 和 decision。 |
| optimization_roadmap | adopted | `doc/optimization-roadmap.zh.md` | 已记录 PI1 pending authorization 和后续恢复条件。 |
| test_support_code_map | adopted | `doc/test-support-code-map.zh.md` | 已定位 aggregator、internal helper、test、bench、script、output 和 production 对照。 |
| phase_index / phase_plan / phase_result / optimization_matrix | adopted | `doc/phases/**` | Phase 000/010/020/030 均有 plan/result 或 index；matrix 已同步。 |
| evaluation_diagnostic | adopted | `doc/pcd_io_templated_writer-evaluation.zh.md` | 已包含 Traceability Map（可追踪性地图）、诊断证据链和生产判断。 |
| evaluation_production | not_applicable with evidence | `doc/pcd_io_templated_writer-evaluation.zh.md#生产接入判断` | 当前没有 production patch；PI2 未授权。 |
| production_topic_doc | not_applicable with evidence | not created | 没有 adopted production behavior（已采用生产行为）、PI5 通过或用户确认采纳。 |

## Target 粒度审计

| target 类别 | current shape scan | decision | evidence / next action |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` 跑 Std/RVV gtest | adopted | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`。 |
| correctness aliases | 当前只有一个 gtest binary；未拆 filter target | not_applicable with evidence | 测试数量少，gtest 字典足够定位；PI3 production direct 后再拆。 |
| bench diagnostic aliases | `--case-filter all`、`--case-filter compressed_*` | adopted | `doc/benchmark-and-evidence.zh.md`。 |
| QEMU smoke aliases | `run_qemu_smoke`，可传 compressed filter | adopted | 明确 QEMU timing 不作为性能证据。 |
| board smoke aliases | `run_board_pcdtw_smoke` | adopted | 单次可运行，不替代 repeated summary。 |
| board repeated aliases | `run_board_pcdtw_repeated`、`run_board_pcdtw_shaped_repeated` | adopted | Phase 000 / 010 summary、manifest、Doctor、registry 已登记。 |
| doctor / registry aliases | component 和 shaped 两组 Doctor / record target | adopted | `log/evidence_registry.json` fresh。 |
| historical probe guarded aliases | no historical production probe target | not_applicable with evidence | 当前没有 rollback probe 或 legacy production target。 |
| production direct aliases | planned only | turn_stop_deferred with stop_condition_hit | 需要 PI2/PI3 production patch 授权后新增。 |

## Structure Parity 审计

| area | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| test/bench source layout | 已使用 `src/test_pcdtw.cpp`、`src/bench_pcdtw.cpp` | adopted | 符合当前 topic 复杂度。 | PI3 后按 production direct 再评估。 |
| aggregator and internal helpers | `include/pcdtw.h` + `include/impl/pcdtw_support.hpp` | adopted | 没有旧 `test_support/` 目录。 | helper 超阈值或职责继续增加时拆分。 |
| script and bench registry | topic-local `script/` 生成 summary / manifest | adopted | manifest 标注 component 与 shaped evidence role。 | production evidence 后扩展 script。 |
| topic-local docs | README + 5 个 role docs + evaluation / roadmap / phase suite | adopted | 本阶段补齐。 | none inside topic-local doc suite。 |
| long-term docs | `doc-rvv` topic doc 未创建 | not_applicable with evidence | 没有 production adopted。 | PI5 + 用户确认后再创建。 |
| legacy compatibility | 没有 root evaluation pointer 或旧路径 alias | not_applicable with evidence | 当前 topic 为新建目录。 | none。 |
| evidence freshness | Phase 000 / 010 registry fresh | adopted | `log/evidence_registry.json`。 | production evidence 后更新。 |

## 当前决策

current_decision：`doc-suite-parity-complete / pending-production-authorization`。

production_decision（生产判断）：

- 仍未修改 `io/include/pcl/io/impl/pcd_io.hpp`。
- Phase 010 支持 `partial-production-candidate`，Phase 020 已冻结 PI1 计划。
- 下一步若进入 PI2 production patch，需要用户明确授权 production integration loop（生产接入闭环）。

## Continue / Stop Decision

stop_condition_hit：继续到默认最高优先级动作 PI2 会扩大到 production 源码，需要用户明确授权。

next_phase_default：

- 用户授权 production integration loop 时：进入 PI2 production patch，范围严格限于 Phase 020 PI1。
- 未授权时：保持 pending-production-authorization；当前 topic-local doc suite parity 已闭合。
