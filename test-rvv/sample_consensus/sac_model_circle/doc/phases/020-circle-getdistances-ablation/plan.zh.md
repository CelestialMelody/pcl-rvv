# Phase 020: circle2d getDistances diagnostic / ablation 计划

## 阶段意图和边界

Phase 000 已把 `selectWithinDistance` 推到 PI5 production candidate（生产候选检查点），
但还没有用户确认采纳；本阶段不修改或回滚任何 production patch（生产补丁），也不把
`selectWithinDistance` 写成 adopted production behavior（已采用生产行为）。

本阶段只做 `getDistancesToModel` 的 topic-local diagnostic（主题本地诊断）和 ablation
（消融）：在测试/bench 资产中新增 test-only RVV candidate（仅测试用 RVV 候选），让 RVV
负责 x/y gather（离散加载）和平方距离，随后仍逐 lane 用 scalar `sqrt` 和 double store
保持 public 标量语义。目标是回答这个候选是否值得后续 bounded production probe（有界生产探针），
不是直接接入 production。

## 当前状态清单

| area | 当前状态 | 证据 |
| --- | --- | --- |
| select/count production | Phase 000 证据支持 positive production candidate，但停在 PI5 用户确认点。 | `doc/phases/000-circle-select-distance-production/result.zh.md` |
| getDistances production | `getDistancesToModel` 仍是标量公开入口，无 production RVV helper。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| bench companion row | Phase 000 board bench 中 public `getDistancesToModel` 伴随行在 0.9554x..1.0067x 摇摆，4/5 低于 1。 | `log/board/repeated-20260828-phase000-production-select/run-*/run_bench_*.log` |
| evidence tooling | Phase 000 已有 board manifest、Evidence Doctor 和 registry，当前 production evidence fresh。 | `doc/phases/000-circle-select-distance-production/production-repeated-evidence-*`、`log/evidence_registry.json` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| test-only getDistances RVV sqr + scalar sqrt store | RVV x/y gather 和平方距离可能减少部分算术成本；保留 scalar `sqrt` 可维持 bit-level 语义接近。 | 每个点都需要 `sqrt` 和 double write，RVV 只能覆盖前半段；临时 float buffer 和 lane loop 可能抵消收益。 |
| public getDistances scalar companion | 继续测公开入口，作为同一输入上的实际当前 production baseline。 | RVV build 下 public 入口仍可能出现编译器自动向量化或噪声，不能当作手写 RVV production 证据。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| test-only getDistances RVV sqr + scalar sqrt store | direct indexed `indices_` | `PointXYZ` / float x-y AoS；`PointXYZI` correctness expansion optional | test-only helper vs public `getDistancesToModel` | new gtest: candidate distances equal public distances | `bench_sac_model_circle` diagnostic getDistances row | planned 5-run repeated board | `getDistancesToModelCandidateRVV` | planned diagnostic manifest | pending |
| public getDistances scalar companion | direct indexed `indices_` | `PointXYZ` / float x-y AoS | current public entry | existing compare tests keep public path usable | companion public row | reuse Phase 000 / Phase 020 board rows | public symbol only | diagnostic context only | scalar retained unless probe evidence changes |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED 测试 | 在 `src/test_sac_model_circle.cpp` 中新增 `GetDistancesCandidateMatchesPublicPath`，调用 test-only helper | RVV 构建先因 helper 缺失编译失败，证明测试抓住候选缺口。 |
| test-only helper | 在测试和 bench 内部派生类实现 `getDistancesToModelCandidate` / `getDistancesToModelCandidateRVV` | 不修改 production；public 和 candidate 输出逐项 `EXPECT_NEAR`。 |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_circle run_test_compare` | Std/RVV correctness 通过；QEMU 不写性能结论。 |
| asm attribution | `make -C test-rvv/sample_consensus/sac_model_circle dump_bench_rvv` | `getDistancesToModelCandidateRVV` 出现 RVV 指令；若被内联则记录真实承载边界。 |
| board repeated diagnostic | `SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh make -C ... collect_getdistances_repeated_board_evidence` | 5-run board；只作为 diagnostic / production-shaped evidence。 |
| Evidence Doctor | `make -C ... run_getdistances_board_evidence_doctor` | Errors / Warnings / Suggestions 写入 result；弱/负/中性不能直接推出 no-production。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断）和 implementation-shape（实现形态）证据。 |
| A/B boundary | test helper：candidate 是 test-only helper；public row 是 public overload companion。 |
| 当前决策问题 | 是否值得为 `getDistancesToModel` 申请后续 bounded production probe。 |
| diagnostic 是否可外推到 production | 不可直接外推。candidate helper 形态接近 production，但仍不是真实 public dispatch。 |
| comparison-boundary / baseline mismatch 风险 | 有。test-only helper 与 public overload 的 wrapper 不同；如果结果正向，后续仍需 production direct。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有在 asm 显示明显可优化但 bench 被测量噪声污染，或有明确替代实现族时才允许；否则保持 deferred，不直接写 no-production closeout。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 若后续接入 production，需要 public Std/RVV production direct；本阶段不能 clean-adopt。 |

## 板卡复跑预算和决策桶

默认 5-run repeated board，200 iterations，5 warmup。`B/A = public_getDistances_ms / candidate_getDistances_ms`，
`>1` 表示 test-only candidate 比 public scalar 更快。decision bucket：`positive >= 1.20x`、
`weak-positive 1.05x..1.20x`、`neutral 0.95x..1.05x`、`negative < 0.95x`、跨桶摇摆为 `unstable`。
如果 Evidence Doctor 报退化频率或长尾，最多再做一次同边界确认复跑；预算耗尽仍摇摆则写 `unstable`。

## Continue / Stop

如果 candidate 为 positive 或 weak-positive，下一步是 production probe plan，仍需用户确认是否越过
PI5 pending 的生产边界。如果 candidate 为 neutral/negative/unstable，默认不改 production，更新
roadmap/matrix，把 `getDistancesToModel` 标为 deferred 或 rejected-for-current-shape，但不能用 diagnostic
直接关闭整个 topic。若 select/count 仍未获 PI5 采纳确认，最终仍停在用户确认点。
