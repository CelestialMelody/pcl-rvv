# Phase 070 Plan: Doc suite parity closeout

## 阶段意图和边界

本阶段只补齐 PPF topic-local doc suite（主题本地文档套件）的结构审计和角色文档。Phase 060 的
production path（生产路径）和板卡证据已经闭合，本阶段不修改
`features/include/pcl/features/impl/ppf.hpp`，不新增 RVV candidate（候选实现），也不重跑板卡
benchmark（性能测试）。

本阶段要证明的是：下一轮 worker 或 reviewer 不依赖对话上下文，就能从 topic-local 文档定位
correctness（正确性）、bench、board evidence（板卡证据）、Evidence Doctor（证据体检）、
test support（测试支撑代码）和 production long-term doc（生产长期文档）的职责边界。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| production | Phase 060 已采纳 traits-gated source xyz AoS + normal AoS + exact `PPFSignature` 的 `alpha_m` RVV path。 |
| tests | `src/test_ppf.cpp` 有 9 个 gtest；`run_test_compare` 历史结果为 Std 5/5、RVV 9/9 pass。 |
| bench | `src/bench_ppf.cpp` 有 6 个 case label，其中 3 个 public production case。 |
| board evidence | Phase 060 两组 production-public repeated board evidence 均为 positive，Doctor 均 `0E/0W/2S`。 |
| docs | README、evaluation、roadmap、phase index、matrix 和 `doc-rvv/features/ppf-RVV.zh.md` 已存在；独立 role 文档尚缺。 |
| evidence registry | 当前没有 `log/evidence_registry.json`；本阶段使用 manifest / Doctor 路径和 artifact tracking 做人工 freshness check。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| doc-suite-parity-closeout | not_applicable | not_applicable | topic-local docs for PPF | doc link / target inventory only | not_applicable | reuse Phase 060 summaries; no new board run | not_applicable | reuse Phase 060 Doctor summaries | planned | Create role docs, update README / phase index / roadmap / matrix / handoff, then run whitespace and freshness checks. |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 写独立 role docs | `testing-overview.zh.md`、`correctness-tests.zh.md`、`benchmark-and-evidence.zh.md`、`optimization-evidence.zh.md`、`test-support-code-map.zh.md` | 每份文档说明主职责、真实 target / case / helper 和证据边界。 |
| A2 更新导航和 phase 状态 | README、phase index、roadmap、optimization matrix | 阅读路径包含新增 role docs；Phase 070 出现在 phase index 和恢复队列中。 |
| A3 写 result 与 `doc_suite_role_inventory` | `070-doc-suite-parity-closeout/result.zh.md` | inventory 覆盖 topic_navigation、testing_overview、correctness_tests、benchmark_and_evidence、optimization_evidence、optimization_roadmap、test_support_code_map、phase suite、evaluation、production_topic_doc 和 artifact tracking。 |
| A4 更新 Handoff | `tmp/rvv-work-logs/features/ppf/current-handoff/current-handoff.zh.md` | `phase_reached`、`artifact_tracking_status`、`doc_suite_role_inventory` 和 `next_worker_action` 与 Phase 070 一致。 |
| A5 验证 | shell checks | `git diff --check` 通过；新增文档路径可由 README / phase index / Handoff 找到。 |

## Evidence Doctor 和 registry 规则

本阶段不产生新的 benchmark 数据。Phase 060 的两个 repeated board manifest / Doctor 仍是当前
production-public 性能证据：

- `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_manifest.json`
- `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_doctor.md`
- `test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_manifest.json`
- `test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_doctor.md`

没有 `log/evidence_registry.json` 时，本阶段必须在 result 中写 `evidence_registry_status`：
`not_available / manual freshness check used`，并说明 raw logs 默认不提交。

## 阶段完成条件

本阶段完成后，`doc_suite_role_inventory` 中不得有当前 topic 授权范围内的
`phase_deferred + unblocked` 文档结构项。若只剩 evidence hardening、direct-AoS pair-feature revisit
或 PPFRGB / CPPF caller evaluation，它们必须写成 separate phase / separate topic required，不作为
当前 PPF closeout 的未阻塞动作。

## 继续 / 停止条件

`next_phase_default` 只有在下列条件都成立时才能写为 `ready for review`：

- 独立 role docs 已创建并由 README / phase index 可达。
- roadmap / matrix 不再含当前 PPF topic 内高优先级未阻塞优化动作。
- Phase 060 production evidence 和 `doc-rvv/features/ppf-RVV.zh.md` 没有相互矛盾。
- artifact tracking 明确指出哪些 untracked files 属于 topic artifact boundary，哪些 generated logs 默认 local-only。

若发现文档引用不存在、证据路径缺失或 production doc stale，则停止在本阶段并写 `turn_stop_deferred`
或继续修复。
