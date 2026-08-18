# Phase 010: diagnostic scaffold and asm probe 计划

## 阶段意图和边界

本阶段在不修改 `registration/include/pcl/registration/bfgs.h` 的前提下，建立 `bfgs` topic 的最小 test/bench scaffold（脚手架）、same-chain correctness（同构链路正确性）验证、Eigen baseline asm probe（反汇编探针）和 QEMU smoke 证据输入。

本阶段覆盖：

- `include/bfgs.h` 聚合入口。
- `include/impl/bfgs_fixtures.hpp`、`bfgs_references.hpp`、`bfgs_candidates.hpp`。
- `src/test_bfgs.cpp`、`src/bench_bfgs.cpp`。
- topic-local Makefile、QEMU smoke manifest、Evidence Doctor（证据体检）输入和 evidence registry（证据登记表）。

本阶段不覆盖：

- 不修改 production 源码 `registration/include/pcl/registration/bfgs.h`。
- 不进入 production integration loop（生产接入闭环）。
- 不做 board（板卡）性能结论。
- 不把 QEMU 计时写成性能证据。

## 当前状态清单

| 对象 | 当前状态 | 路径 / 证据 | 本阶段动作 |
| --- | --- | --- | --- |
| Phase 000 scaffold | 已完成 | `doc/phases/000-current-state-and-gaps/{plan,result}.zh.md` | 作为恢复输入 |
| 生产源码 caller | 已确认 GICP-only | `registration/include/pcl/registration/gicp.h`、`registration/include/pcl/registration/impl/gicp.hpp` | 只作为 caller-shaped smoke 约束 |
| test/bench 源码 | 未创建 | `test-rvv/registration/bfgs/src/` | 新建 |
| 聚合入口 / internal helper | 未创建 | `test-rvv/registration/bfgs/include/` | 新建 |
| Makefile | 未创建 | `test-rvv/registration/bfgs/Makefile` | 新建 |
| QEMU / asm / registry | 未创建 | `log/`、`build/asm/`、`log/evidence_registry.json` | 只在本阶段创建最小 smoke 管线 |

## 假设与候选族

| candidate family | 假设 | 风险 / 未知 | 本阶段处理 |
| --- | --- | --- | --- |
| Eigen baseline asm | `moveTo()`、`slope()` 和 direction update 的 vector expression 已足以触发 RVV 或证明其缺席 | 小维度可能完全被编译器折叠；需要 asm 探针 | 作为首要诊断 |
| Direction update fused diagnostic | `minimizeOneStep()` 中多次 dot/norm 与线性组合是最有可能出现局部 RVV 的区域 | 可能只得到 test-only helper 收益 | 作为 test-only candidate |
| Move-to + slope combined diagnostic | `moveTo(alpha)` 之后紧接 `slope()` 可能有复用空间 | cache / 依赖链路可能让收益很小 | 先实现后再决定是否 bench |
| GICP caller-shaped smoke | GICP 的 `Vector6d` BFGS 路径是当前唯一确认 caller | 真实热点可能仍在 functor / search / line-search | 只做 smoke，不做热点结论 |

## 优化矩阵

| candidate family | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Eigen baseline asm | `moveTo()`、`slope()`、`minimizeInit()` | fixed `Vector6d` reference | QEMU smoke only | missing | planned | planned for QEMU manifest | `planned` | build test harness |
| Direction update fused diagnostic | `minimizeOneStep()` direction update | same-chain correctness vs reference | direction-update microbench | missing | planned | planned for QEMU manifest | `planned_diagnostic` | implement helpers |
| Move-to + slope combined diagnostic | `moveTo(alpha)` + `slope()` | cache path correctness | deferred | missing | deferred | missing | `deferred` | revisit after direction update |
| GICP caller-shaped smoke | `GeneralizedIterativeClosestPointBFGS` | upstream smoke only | QEMU smoke only | missing | not_applicable | planned for QEMU manifest | `planned` | add caller smoke test |
| Production dispatch | `bfgs.h` production template | not run | not run | missing | not_applicable | missing | `blocked_requires_user_authorization_and_evidence` | do not touch production |

## 实现和测试动作

| action | 产物 | 完成判据 | 依赖 |
| --- | --- | --- | --- |
| 建立 topic-local include 结构 | `include/bfgs.h` 与 `include/impl/*.hpp` | test / bench 只 include 聚合入口，内部 helper 职责清楚 | Phase 000 |
| 实现 reference / candidate helper | 标量参考和 RVV candidate | same-chain 对拍通过；RVV build 与 Std build 都可编译 | include 结构 |
| 实现 correctness tests | `src/test_bfgs.cpp` | 至少覆盖 direction update、move-to+slope、GICP caller smoke 和边界样本 | helpers |
| 实现 bench smoke | `src/bench_bfgs.cpp` | 输出 `ms/iter`、`Total Time` 和 checksum，可供 QEMU compare / doctor 读取 | helpers |
| 建立 Makefile 和 manifest / doctor 钩子 | `Makefile` 与 qemu manifest 脚本 | `run_test_compare`、`dump_bench_rvv`、`run_qemu_smoke_evidence_doctor`、`record_qemu_smoke_evidence_state` 可用 | source files |

## Evidence Doctor 和 registry 规则

本阶段若生成 bench log 或 asm dump，必须同步：

1. 生成 topic-local manifest。
2. 运行 Evidence Doctor。
3. 记录到 `log/evidence_registry.json` 或等价 registry。

QEMU smoke 只用于 correctness 和日志形状，不用于 performance 结论。若 manifest 字段不足或 doctor 输出异常，必须在 result 中写明是 metadata 问题、数值问题还是边界问题。

## 阶段完成条件

本阶段完成需要满足：

- `include/`、`include/impl/`、`src/`、`Makefile` 已创建。
- `test_bfgs.cpp` 和 `bench_bfgs.cpp` 可编译。
- 至少一个 same-chain correctness test 可运行。
- `dump_bench_rvv` 和 QEMU smoke manifest / doctor 链路可运行。
- evidence registry 可以记录当前主题输出。
- `doc-rvv` 仍不适用，且未新建。

## 板卡复跑预算和决策桶

本阶段不要求 board 复跑。若后续进入 board，默认以 5-run repeated 作为第一轮预算，decision bucket 只做 `positive`、`weak-positive`、`neutral`、`negative`、`unstable` 分桶。

## 继续 / 停止条件

默认下一动作是创建实现文件并编译验证。只有当 Phase 010 的 correctness / asm / smoke 结果足以说明局部诊断没有价值，或者 compiler 已经在同边界生成等价 RVV 且 reviewer 可接受时，才考虑停止在诊断层；否则继续到 Phase 020 board smoke。

## 文档更新清单

- `README.zh.md`
- `doc/bfgs-evaluation.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/phases/010-diagnostic-scaffold-and-asm-probe/result.zh.md`

## Roadmap 同步动作

Phase 010 结束后，roadmap 需要根据 correctness / asm 结果决定：

- `Eigen baseline asm` 是 `adopted` 还是 `rejected with evidence`。
- `Direction update fused diagnostic` 是否保持 `planned_diagnostic` 进入 board。
- `Move-to + slope combined diagnostic` 是否继续 defer。
- `GICP caller-shaped smoke` 是否足以进入 caller hotspot audit。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic scaffold only |
| A/B boundary | baseline / candidate / caller smoke 尚未完全闭合 |
| 当前决策问题 | 是否值得把 BFGS 局部向量状态更新继续做成诊断候选 |
| diagnostic 是否可外推到 production | no；必须先过 caller 和 asm 边界 |
| comparison-boundary / baseline mismatch 风险 | yes；Eigen 自动向量化、test-only candidate 和 production helper 不在同一层 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有 caller hotspot 证据加用户授权后才允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes |
