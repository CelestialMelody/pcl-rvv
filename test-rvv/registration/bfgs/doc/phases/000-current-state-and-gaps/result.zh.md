# Phase 000: current-state-and-gaps 结果

## 执行范围

本阶段按 `plan.zh.md` 开启 `bfgs` RVV topic（主题），建立 topic-local scaffold（主题本地脚手架）和 S2 evaluation（函数级评估）。实际范围与计划一致：未修改 `registration/include/pcl/registration/bfgs.h`，未创建 production patch（生产补丁），未进入 production integration loop（生产接入闭环）。

## 计划动作回填

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| 建立 topic-local scaffold | done | `README.zh.md`、`doc/*.zh.md`、`doc/phases/README.zh.md` | 新 topic 入口已可恢复 |
| 写 S2 函数级评估 | done | `doc/bfgs-evaluation.zh.md` | 当前结论为 `diagnostic_planned` |
| 建立 roadmap / matrix | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | planned / deferred / not_applicable 状态已区分 |
| doc-suite shape scan | done | `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md` | 当前只有文档骨架，test/bench/source layout 为下一阶段 `phase_deferred + unblocked` |
| 更新 module second-pass 状态 | done | `doc-rvv/library-screening/registration/registration-module-second-pass.zh.md` | `bfgs` 从待评估更新为评估已建 / diagnostic planned |

## 源码事实

仓库检索确认 `bfgs.h` 的 production caller（生产调用方）是 GICP 的 BFGS 路径：

- `registration/include/pcl/registration/gicp.h` 中 `OptimizationFunctorWithIndices : BFGSDummyFunctor<double, 6>`。
- `registration/include/pcl/registration/impl/gicp.hpp` 中 `estimateRigidTransformationBFGS()` 构造 `BFGS<OptimizationFunctorWithIndices>` 并调用 `minimizeInit()` / `minimizeOneStep()`。
- `test/registration/test_registration.cpp` 中 `GeneralizedIterativeClosestPointBFGS` 覆盖 `useBFGS()` 上游 smoke（小型验证）形态。

本阶段没有发现 NDT 直接使用 `bfgs.h` 的源码证据，因此 NDT 不作为当前 topic 的 caller 结论。

## Optimization matrix 更新

| candidate family | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| Eigen vector expression baseline | planned | 源码使用 Eigen `dot()`、`norm()` 和向量表达式 | Phase 010 asm probe |
| Direction update fused diagnostic | planned_diagnostic | `minimizeOneStep()` direction update 是最集中向量状态更新 | Phase 010 test-only helper + correctness |
| Move-to + slope combined diagnostic | deferred | cache path 和 line-search 语义需先测试 | Direction update 后再决定 |
| Line-search scalar control | not_applicable with evidence | 标量分支、cache 和 functor 回调为主 | 只保留 boundary correctness |
| GICP caller hotspot audit | deferred_until_caller_evidence | GICP caller 已确认，但无 profile / board 证据 | 局部诊断有信号后执行 |
| Production dispatch | blocked_requires_user_authorization_and_evidence | 无 correctness、asm、board、caller hotspot 证据 | 不进入 PI1 |

## 证据分层

| evidence type | 状态 | 说明 |
| --- | --- | --- |
| correctness（正确性） | not_run | 尚未创建 test harness |
| QEMU path evidence（QEMU 路径证据） | not_run | 尚未创建 Makefile / target |
| asm attribution（反汇编归属） | not_run | 下一阶段计划执行 |
| board performance（板卡性能） | not_run | 本阶段无性能结论 |
| Evidence Doctor（证据体检） | not_applicable | 没有 benchmark、summary、checksum 或 asm evidence |
| evidence registry（证据登记表） | not_available_initial_topic | 没有生成日志 |

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | diagnostic planning only |
| A/B boundary | 未建立；下一阶段必须区分 Eigen baseline、test-only candidate 和 production public helper |
| 当前决策问题 | 是否值得继续建立 BFGS 局部诊断 |
| diagnostic 是否可外推到 production | no；caller 维度、functor 成本和 Eigen 自动向量化会改变真实边界 |
| comparison-boundary / baseline mismatch 风险 | yes；test-only fused candidate 可能不等于 production boundary |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 caller hotspot 证据和用户授权同时满足时允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes |

## Doc-suite parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已创建 | 入口导航、目录分工、证据边界 | adopted | `README.zh.md` | 后续随 target 更新 |
| testing-overview | 已创建，只有计划 | 测试类型、target 粒度审计、覆盖矩阵 | adopted for Phase 000 | `doc/testing-overview.zh.md` | Phase 010 补真实 target |
| correctness-tests | 已创建，只有测试族计划 | gtest 语义和命令 | adopted for Phase 000 | `doc/correctness-tests.zh.md` | Phase 010 创建 `src/test_bfgs.cpp` |
| benchmark-and-evidence | 已创建，只有 bench 计划 | bench label、证据边界、doctor | adopted for Phase 000 | `doc/benchmark-and-evidence.zh.md` | Phase 010 创建 bench / asm target |
| optimization-evidence | 已创建 | 候选状态索引 | adopted | `doc/optimization-evidence.zh.md` | 随证据更新 |
| test-support-code-map | 已创建，代码布局未实现 | 聚合入口、internal helper、src/script map | `phase_deferred + unblocked` | 当前没有 test support 代码 | Phase 010 创建 include / src |
| evaluation | 已创建 | S2 评估、Traceability Map、生产判断 | adopted | `doc/bfgs-evaluation.zh.md` | 随证据更新 |
| long-term doc-rvv | 不适用 | 只在 adopted production behavior 后创建 | not_applicable with evidence | 无 production patch / PI5 | 无 |
| artifact tracking | 新文件均为 untracked topic-local docs | 新增文档需列入提交边界 | partial until commit phase | 本轮不提交 | Handoff / final 明确 |

## Continue / Stop Decision

`continue_stop_decision`: 本轮可以停止在 Phase 000，因为用户请求是开启新 topic，本阶段已经完成 topic-local scaffold、evaluation、roadmap、matrix 和 second-pass 状态同步。继续到 Phase 010 会新增 Makefile、test / bench source 和运行证据，属于下一阶段实现工作。

`stop_condition_hit`: 用户本轮目标限定为开启新 topic；production 需要后续证据和明确授权。

`next_phase_default`: `010-diagnostic-scaffold-and-asm-probe`。

## 下一阶段默认动作

创建 Phase 010 plan，随后建立：

1. `include/bfgs.h` 聚合入口。
2. `include/impl/bfgs_fixtures.hpp`、`bfgs_references.hpp`、`bfgs_candidates.hpp`。
3. `src/test_bfgs.cpp` 和 `src/bench_bfgs.cpp`。
4. 最小 Makefile target：`run_test_compare`、`dump_bench_rvv`、`run_qemu_smoke_evidence_doctor`。

Phase 010 仍不修改 production；只有 correctness、asm、board 和 caller hotspot 证据都成立且用户授权，才允许进入 PI1。
