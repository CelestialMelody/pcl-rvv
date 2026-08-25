# Phase 090 计划：structure parity / doc suite diagnostic

## 阶段意图和边界

本阶段是 documentation / structure diagnostic（文档和结构诊断），只补齐 `test-rvv/features/shot` 的 topic-local doc suite（主题本地文档套件）、target granularity audit（测试 target 粒度审计）和 test-support code map（测试支撑代码地图）。不修改 production（生产源码），不新增 RVV candidate，不重跑板卡性能。

本阶段要解决的问题是：Phase 000-080 已经产生多个 test-only helper、bench case、board run 和 Evidence Doctor report（证据体检报告），但 README / evaluation 当前承担了过多职责。下一轮 worker 或 reviewer 应能从 role 文档直接定位 correctness、bench、asm、board、doctor、候选取舍和 production doc 适用性。

## 当前状态清单

| area | 当前形态 | 缺口 |
| --- | --- | --- |
| topic navigation | `README.zh.md` 已存在 | 需要链接新增 role docs。 |
| evaluation | `doc/shot-evaluation.zh.md` 已存在 | Traceability Map 已有，但 testing / bench / code map 过度合并。 |
| phase suite | `doc/phases/README.zh.md`、phase plan/result、matrix 已存在 | 080 已补齐；090 result 待写。 |
| testing overview | 不存在独立文档 | 需要 target 粒度审计和测试流程说明。 |
| correctness tests | 不存在独立文档 | 需要逐个 gtest 字典。 |
| benchmark/evidence | 不存在独立文档 | 需要 case-filter 字典、board / doctor / asm 边界和提交边界。 |
| optimization evidence | 不存在独立文档 | 需要把 adopted / attempted / partial-production-candidate 的候选状态和证据路径从 roadmap 中分离。 |
| test support code map | 不存在独立文档 | 需要从聚合头、impl helper、src、script 到 output 的定位表。 |
| production topic doc | `doc-rvv/features/shot-RVV.zh.md` 不存在 | 当前无 adopted production behavior，判为 not_applicable with evidence。 |

## Doc suite role inventory（计划状态）

| role | planned status | path / section |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_diagnostic | standalone | `doc/shot-evaluation.zh.md` |
| production_topic_doc | not_applicable with evidence | no adopted production behavior / no PI5 accepted patch |

## Target 粒度审计计划

| target 类别 | 当前事实 | 本阶段动作 |
| --- | --- | --- |
| correctness aggregate | `run_test_compare` 构建并运行 Std / RVV gtest | 文档化。 |
| correctness aliases | 没有细分 Make target；可用 gtest filter 手动拆分 | 记录为 phase_deferred + unblocked，不虚构 target。 |
| bench diagnostic aliases | `--case-filter` 支持单 case | 文档化 case 字典。 |
| QEMU smoke aliases | `dump_bench_rvv` 构建并 dump asm；QEMU bench compare 默认不作为性能证据 | 文档化边界。 |
| board smoke aliases | `board_smoke` 存在 | 文档化只作 smoke / quick check。 |
| board repeated aliases | `run_board_bench_compare BENCH_ARGS=...` 可定向 run，但没有 per-case Make alias | 记录为手动 targeted flow；细分 alias 暂缓。 |
| doctor / registry aliases | `run_evidence_doctor` 存在；topic-local manifest wrapper 存在；registry 未接入 | 文档化 doctor，registry 标为 phase_deferred + unblocked。 |
| historical probe guarded aliases | 没有历史 production probe target | not_applicable with evidence。 |

## 实现动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 新增 testing overview | `doc/testing-overview.zh.md` | 覆盖 target 分类、流程、输入和覆盖矩阵。 |
| 新增 correctness tests | `doc/correctness-tests.zh.md` | 12 个 gtest 均可定位到输入、被测路径和证明边界。 |
| 新增 benchmark/evidence | `doc/benchmark-and-evidence.zh.md` | bench case-filter 字典、board / doctor / asm / 提交边界清楚。 |
| 新增 optimization evidence | `doc/optimization-evidence.zh.md` | 每个 candidate family 与 phase / code / board / decision 对齐。 |
| 新增 test support code map | `doc/test-support-code-map.zh.md` | 聚合头、internal helper、test、bench、script 和 production 边界可定位。 |
| 更新入口文档 | README、evaluation、phase index、roadmap、matrix | 默认恢复入口和 role inventory 一致。 |
| Phase 090 result | `doc/phases/090.../result.zh.md` | 回填审计表、artifact tracking 和下一步。 |

## Evidence Doctor 和性能边界

本阶段不产生新的 performance evidence（性能证据），不需要新增 board run。仍需在结果中记录当前 Evidence Doctor 状态来自 Phase 080：Errors=0、Warnings=1 low-run、Suggestions=0，并说明它不覆盖三次 targeted run 的不稳定性。

## 完成条件

本阶段完成后，doc-suite inventory 中 topic-local 文档 role 应全部为 `standalone` 或 `not_applicable with evidence`；允许留下的后续项只能是明确的 `phase_deferred + unblocked`，例如 registry 接入和细分 Make alias。若这些项仍在当前授权范围内，roadmap 必须给出下一阶段默认动作。

## Continue / stop 条件

若 role docs 补齐且验证通过，默认下一步是 `100-evidence-registry-target-alias-diagnostic`：补 topic-local evidence registry / target alias 的结构缺口。若验证发现 production `shot.hpp` 被修改、dirty isolation 不安全或工具链不可用，则转为 blocked。
