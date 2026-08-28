# Phase 030 Plan: structure parity doc suite

## 阶段意图和边界

本阶段只补齐 `test-rvv/sample_consensus/sac_model_circle/` 的 topic-local doc suite（主题本地文档套件）和恢复入口，让 reviewer（审查者）能从仓库文件恢复 Phase 000 / Phase 020 证据、production patch（生产补丁）状态和下一步用户确认边界。

本阶段不修改 production（生产源码），不新建 `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md`，不重新跑板卡 bench（性能测试），也不把 Phase 000 patch 写成 adopted production behavior（已采用生产行为）。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| production 状态 | `selectWithinDistance` / `countWithinDistance` 有 PI5 前 positive production candidate（正向生产候选），等待用户确认。 |
| diagnostic 状态 | Phase 020 的 `getDistancesToModel` test-only candidate（仅测试使用候选）稳定退化，production 保持标量。 |
| phase 文档 | 已有 Phase 000 / Phase 020 plan/result、manifest、Evidence Doctor 和 optimization matrix。 |
| 文档缺口 | 缺少 `README.zh.md`、`doc/sac_model_circle-evaluation.zh.md`、testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map。 |
| handoff 缺口 | 配置解析出的 current handoff 路径不存在，短 prompt 恢复必须依赖对话摘要。 |

## 优化矩阵和文档套件动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| topic navigation | `README.zh.md` | 写清当前结论、阅读顺序、常用 target、可提交摘要证据和默认排除项。 |
| evaluation | `doc/sac_model_circle-evaluation.zh.md` | 承载 production patch scope、证据链、Traceability Map（可追踪性地图）和 PI5 阻塞边界。 |
| testing overview | `doc/testing-overview.zh.md` | 按 Makefile / board.mk / gtest / bench / script 抽取真实 target 粒度。 |
| correctness role | `doc/correctness-tests.zh.md` | 解释 5 个 gtest 的输入、断言、证明范围和不覆盖范围。 |
| benchmark/evidence role | `doc/benchmark-and-evidence.zh.md` | 解释 bench row、计时边界、board repeated、manifest、Evidence Doctor 和 registry。 |
| optimization evidence role | `doc/optimization-evidence.zh.md` | 把 select/count positive candidate、getDistances rejected candidate 和 deferred 路线映射到证据。 |
| code map | `doc/test-support-code-map.zh.md` | 定位 production helper、test-only helper、bench wrapper、script 和 evidence output。 |
| phase / roadmap / handoff | phase index、matrix、roadmap、current handoff | 默认下一步停在 PI5 用户确认；没有未阻塞 topic-local 文档动作。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的性能 evidence（证据），只引用已有登记项：

- Phase 000 production direct（真实生产路径证据）：`test-rvv/sample_consensus/sac_model_circle/doc/phases/000-circle-select-distance-production/production-repeated-evidence-manifest.json`
- Phase 020 production-shaped diagnostic（生产形态诊断）：`test-rvv/sample_consensus/sac_model_circle/doc/phases/020-circle-getdistances-ablation/getdistances-repeated-evidence-manifest.json`
- registry（证据登记表）：`test-rvv/sample_consensus/sac_model_circle/log/evidence_registry.json`

若 `production_evidence_status` 或 `getdistances_evidence_status` 报 stale（过期），本阶段 result 只能写 refresh pending，不能沿用旧数值结论。

## 阶段完成条件

本阶段完成后，doc-suite role inventory（文档职责清单）应全部为 `standalone:<path>` 或 `not_applicable with evidence`。合法停止条件仍是 PI5 用户确认：继续 production closeout（生产收尾）会把 patch 视为 adopted 或回滚，必须等待用户明确授权。

## 继续 / 停止条件

默认下一步：等待用户对 Phase 000 production patch 明确选择“采纳 / 保留”或“回滚”。如果用户只要求继续当前 topic 且没有给出 PI5 决策，worker 可以继续做新的非生产候选，但当前 roadmap 中没有高优先级、已具备新实现族的 `getDistancesToModel` 动作。
