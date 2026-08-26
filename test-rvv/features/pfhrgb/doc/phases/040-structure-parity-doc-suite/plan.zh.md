# Phase 040 Structure Parity Doc Suite Plan

## 阶段意图和边界

本计划是 Phase 040 执行前的历史计划。当时本阶段只补齐 PFHRGB topic-local doc suite（主题本地文档套件）
和证据同步，尚未进入 production patch（生产补丁）。Phase 020 之后，`features/include/pcl/features/impl/pfhrgb.hpp`
已经接入 exact-gated production RVV path，`doc-rvv/features/pfhrgb-RVV.zh.md` 也已经适用；当前事实请以
Phase 020 result、optimization matrix、evaluation 和正式 `doc-rvv` 为准。

## 当前证据基线

| item | 当前事实 |
| --- | --- |
| Phase 030 board repeated | `public_pfhrgb_k_with_candidate_reuse` 已进入 5-run manifest，B/A 为 `1.25, 1.25, 1.22, 1.24, 1.25`，median `1.25x`。 |
| Phase 030 non-reuse public-shaped | `public_pfhrgb_k_with_candidate` B/A 为 `1.21, 1.20, 1.21, 1.21, 1.22`，median `1.21x`。 |
| Evidence Doctor | 当时历史 baseline 为 2E/0W/6S；当前 Phase 020 后结果为 1E/1W/6S，helper/component claims 仍需降级。 |
| Production | 当时还未进入 production patch；Phase 020 后已采纳 exact-gated production path。 |

## 执行动作

| action | 产物 | 验收 |
| --- | --- | --- |
| REFRESH-040 | 刷新 README、evaluation、Phase 030 result、matrix、roadmap、phase index 和 current handoff。 | 不再保留 `board-blocked` 或旧 `Errors=0` 作为当前 truth；旧 Phase 010/000 数字标为 historical baseline（历史基线）。 |
| DOCS-040 | 新增 `testing-overview.zh.md`、`correctness-tests.zh.md`、`benchmark-and-evidence.zh.md`、`optimization-evidence.zh.md`、`test-support-code-map.zh.md`。 | 每个 role（职责）有独立主归属，README 可导航，文档不把 diagnostic speedup 写成 production adoption（生产采纳）。 |
| AUDIT-040 | 在 Phase 040 result 写 doc_suite_role_inventory、target granularity audit（测试 target 粒度审计）和 artifact tracking（产物跟踪）状态。 | 每个 area 只能是 adopted、not_applicable with evidence、rejected with evidence 或合法 deferred。 |
| VERIFY-040 | 运行 `run_test_compare`、`dump_bench_rvv`、`evidence_doctor_repeated`、路径限定 `git status` 和 `git diff --check`。 | correctness / ASM / Doctor / 文档引用可复核；QEMU timing 不写性能结论。 |

## 继续 / 停止条件

本阶段完成后，若 production 修改仍未获明确确认，合法停止点是
`stop at production checkpoint`：topic-local 文档和证据已闭合，下一 phase 默认回到
`020-pi1-production-integration-plan`。该历史停止点已被后续用户授权和 Phase 020 接入闭环解除；
当前如果继续，默认方向应是新的 `050-point-type-expansion`，而不是重跑 PI1。
