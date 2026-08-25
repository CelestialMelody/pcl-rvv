# Phase 090 结果：structure parity / doc suite diagnostic

## 执行范围

本阶段只更新 topic-local 文档，不修改 production（生产源码）、test helper、bench 逻辑或板卡配置。新增 role docs（职责文档）用于让下一轮 worker / reviewer 不依赖聊天上下文即可定位测试入口、bench case、Evidence Doctor（证据体检）、候选取舍和代码边界。

## 计划动作回填

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| testing overview | done | `doc/testing-overview.zh.md` | target 分类、输入范围、覆盖矩阵和提交边界已拆出。 |
| correctness tests | done | `doc/correctness-tests.zh.md` | 12 个 gtest 的输入、被测路径、断言和不能证明的范围已列出。 |
| benchmark/evidence | done | `doc/benchmark-and-evidence.zh.md` | bench case-filter 字典、board / doctor / asm 边界已拆出。 |
| optimization evidence | done | `doc/optimization-evidence.zh.md` | candidate family（候选族）和 phase / code / board / decision 对齐。 |
| test support code map | done | `doc/test-support-code-map.zh.md` | 聚合头、internal helper、src、script、output 和 production 边界已定位。 |
| entry docs sync | done | README、evaluation、phase index、roadmap、matrix | 默认恢复入口和 role inventory 已同步。 |

## Doc suite role inventory（结果）

| role | status | path / section | evidence |
| --- | --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` | 当前结论、阅读路径、常用命令和提交边界。 |
| testing_overview | standalone | `doc/testing-overview.zh.md` | target 粒度审计和覆盖矩阵。 |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` | 12 个 gtest 字典。 |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` | case-filter、board、doctor、asm 和提交边界。 |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` | candidate 状态和证据索引。 |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` | 搜索空间和默认恢复队列。 |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` | 代码定位和拆分审计。 |
| phase_index | standalone | `doc/phases/README.zh.md` | phase 恢复入口。 |
| evaluation_diagnostic | standalone | `doc/shot-evaluation.zh.md` | S2 评估、Traceability Map 和 production doc 适用性。 |
| production_topic_doc | not_applicable with evidence | not_created | 当前无 adopted production behavior，也无 PI5 后用户确认保留的 production patch。 |

## Structure parity audit

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_shot.cpp` 546 行、`src/bench_shot.cpp` 597 行，低于 800 行软阈值。 | `src/` + `include/impl` 结构可审查。 | adopted | 已有 source split。 | none |
| aggregator and internal helpers | `include/shot.h` 聚合，`include/impl/shot_*.hpp` 按职责拆分。 | responsibility-first（职责优先）。 | adopted | 无旧 `test_support/` 目录。 | none |
| target granularity | aggregate、case-filter、board smoke、doctor 存在；细分 correctness / board repeated alias 不存在。 | 不虚构 target，缺口写下一 phase。 | phase_deferred + unblocked | `testing-overview.zh.md` 已列出。 | `100-evidence-registry-target-alias-diagnostic` |
| topic-local docs | 新增 testing / correctness / benchmark / optimization / code-map role docs。 | 复杂 topic 优先 standalone role。 | adopted | role inventory 已闭合。 | none |
| long-term docs | 无 `doc-rvv/features/shot-RVV.zh.md`。 | 未接 production 时 not_applicable。 | not_applicable with evidence | production untouched。 | PI5 后若用户采纳再创建。 |
| legacy compatibility | 无旧 doc pointer 或 compatibility alias。 | 默认不保留 legacy。 | not_applicable with evidence | 当前无旧入口。 | none |
| evidence freshness / registry | manifest wrapper 和 doctor target 已有；registry 未接入。 | registry / freshness 应可恢复检查。 | phase_deferred + unblocked | 当前靠 manifest + manual status。 | Phase 100 默认处理。 |

## Target 粒度审计结果

| target 类别 | decision | evidence | next action |
| --- | --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` | none |
| correctness aliases | phase_deferred + unblocked | 只有手动 gtest filter。 | Phase 100 可补常用 alias。 |
| bench diagnostic aliases | adopted | `--case-filter` case 字典。 | none |
| QEMU smoke aliases | adopted | `dump_bench_rvv`。 | none |
| board smoke aliases | adopted | `board_smoke`。 | none |
| board repeated aliases | phase_deferred + unblocked | `run_board_bench_compare BENCH_ARGS=...`，无 per-case alias。 | Phase 100 可补。 |
| doctor / registry aliases | partial / phase_deferred + unblocked | `run_evidence_doctor` 已有，registry 未接入。 | Phase 100 默认补 registry / freshness。 |
| historical probe guarded aliases | not_applicable with evidence | 无历史 production probe target。 | none |

## Evidence 和 production decision

本阶段没有新增性能数据。当前 Phase 080 evidence 仍为 `attempted / unstable`：0.84x、1.12x、1.17x，doctor 当前 fetched report 为 Errors=0、Warnings=1 low-run、Suggestions=0。生产源码未修改；shape-bin indexed PI1 plan 仍是唯一已写好的 production probe 输入，PI2 需要用户授权。

## Artifact tracking

新增 topic-local docs 均位于 `test-rvv/features/shot/doc/`，属于当前 topic artifact candidate（待提交候选）。`log/board`、`log/qemu`、`build` 和 asm 仍是生成产物，不默认提交。

## Continue / stop decision

- `continue_stop_decision`：继续。
- `stop_condition_hit`：none。
- `next_phase_default`：`100-evidence-registry-target-alias-diagnostic`。
- 原因：evidence registry / freshness target 和常用 correctness / board repeated alias 仍是当前 topic 测试资产范围内的 `phase_deferred + unblocked` 结构缺口。
