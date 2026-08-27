# Phase 050: evidence registry target alias plan

## 阶段意图和边界

本阶段补齐 evidence registry（证据登记表）入口，让 `initgraph_no_solve` repeated board（重复板卡测试）
摘要文件可以通过 Make target 记录 size / mtime / sha256 和文档引用状态。Phase 040 已把
registry 缺口标成 `partial`，且该动作只触碰 `test-rvv/segmentation/grabcut_segmentation/**`。

本阶段不修改 production（生产源码），不重新解释 Phase 030 的性能数字，不把 diagnostic evidence
（诊断证据）升级为 production evidence（生产证据），也不登记 raw repeated board log 作为默认提交候选。

## 当前状态清单

| area | 当前事实 | 路径 / 证据 |
| --- | --- | --- |
| Phase 030 repeated evidence | 当前采用 run label 为 `repeated-board-20260827-144235`，B/A median（中位数）约 `1.5416x`。 | `doc/phases/030-initgraph-no-solve-diagnostic/result.zh.md` |
| Evidence Doctor | repeated Doctor 为 `Errors=0, Warnings=0, Suggestions=0`。 | `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md` |
| registry 状态 | topic 尚未维护 `log/evidence_registry.json`。 | `040-production-integration-plan/result.zh.md#test-support-shape-scan-和-target-粒度审计` |
| 生产状态 | PI1 完成，PI2 因 production 授权边界暂停。 | `040-production-integration-plan/result.zh.md` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| evidence registry alias | synthetic public-shaped diagnostic | GMM K=5 / float Color / `initgraph_no_solve` | topic-local Makefile target records repeated manifest and doctor files | `make run_test_compare` must remain green after Makefile edit | QEMU smoke only for log-shape if Makefile touched | no new board run; reuse Phase 030 repeated summary | no new asm required unless build target regresses | `make run_initgraph_repeated_evidence_doctor` plus registry check | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 新增 registry 变量和 record target | `Makefile` | `make record_initgraph_repeated_evidence_state` 先生成 repeated manifest / doctor，再记录 manifest、Markdown doctor、JSON doctor。 |
| 新增 registry status target | `Makefile` | `make evidence_status` 对已登记摘要文件做 hash check，并扫描 summary artifact。 |
| 运行 registry target | `log/evidence_registry.json` | registry 中出现 Phase 030 repeated manifest / doctor 文件，且不包含 raw board log 内容。 |
| 回填 Phase 050 result | `050-evidence-registry-target-alias/result.zh.md` | 记录 target、检查输出、freshness 状态和继续 / 停止决定。 |
| 同步导航和矩阵 | README、phase index、roadmap、optimization matrix、evaluation、Phase 040 result | 把 evidence registry 从 `partial` 更新为本阶段已接通的 topic-local freshness 机制。 |

## Evidence Doctor 和 registry 规则

registry 输入只包含 summary artifact（摘要产物）：

- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/030-initgraph-no-solve-diagnostic/repeated-evidence-doctor.json`

`record_initgraph_repeated_evidence_state` 使用 `test-rvv/script/evidence_registry.py record`。`evidence_status`
使用同一脚本的 `check`，并启用 `--require-doc-ref`，让 README、evaluation、roadmap、phase index 和相关 phase
result 成为当前 run label / 摘要路径的文档引用来源。raw repeated board logs 仍按 summary-only（只提交摘要）
策略保留为 local-only。

## 阶段完成条件

| 条件 | 判定 |
| --- | --- |
| registry target 可运行 | `make record_initgraph_repeated_evidence_state` exit 0。 |
| freshness check 可运行 | `make evidence_status` exit 0，或输出 issue 后在 result 中解释并修复。 |
| correctness 未受影响 | `make run_test_compare` 通过。 |
| QEMU smoke 未受影响 | 若 Makefile target 变化影响 bench 入口，运行窄范围 QEMU smoke；QEMU timing 不作为性能证据。 |
| Evidence Doctor 仍 fresh | `make run_initgraph_repeated_evidence_doctor` 仍为 `Errors=0, Warnings=0, Suggestions=0`。 |
| 生产边界保持 | production 文件无 diff。 |

## 板卡复跑预算和决策桶

本阶段不新跑板卡 benchmark（性能测试）。Phase 030 repeated board 已有 5-run 正向 bucket（决策桶），本阶段只登记摘要文件状态。若 registry check 发现摘要文件 hash 变化，先重新生成 manifest / doctor 并刷新文档；只有性能方向、Doctor 数量或 run label 改变时，才需要另开证据刷新 phase 或重跑板卡。

## 继续 / 停止条件

本阶段完成后，topic 仍停在 PI2 production patch 授权边界。默认下一动作保持为：用户明确授权 production integration loop（生产接入闭环）后，按 Phase 040 `pi2_scope` 连续推进 PI2-PI5。授权前继续修改 production 文件仍命中停止条件。

## 文档更新清单

- `doc/phases/050-evidence-registry-target-alias/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/grabcut_segmentation-evaluation.zh.md`
- `README.zh.md`
- `doc/phases/040-production-integration-plan/result.zh.md`

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` 的 freshness bookkeeping（新鲜度登记），不是新的性能证据。 |
| A/B boundary | `test_support_helper`，沿用 Phase 030 repeated manifest 表达的边界。 |
| 当前决策问题 | 证据摘要是否可恢复、可检查，避免后续覆盖日志后继续引用旧数值。 |
| diagnostic 是否可外推到 production | 不外推。本阶段只改善证据登记，不改变 production 接入判断。 |
| comparison-boundary / baseline mismatch 风险 | 无新增 A/B；仍继承 Phase 030 的 helper-level diagnostic 边界。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不改变 Phase 030 positive bucket；若后续 registry 发现 stale 数值，先刷新证据再判断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。registry 不能替代 PI2-PI5 production direct 证据。 |

## roadmap 同步动作

阶段结束后，将 `evidence registry alias` 从 `planned` 更新为 `adopted` 或记录失败原因。若成功，roadmap 的默认恢复队列仍把 PI2 授权边界作为唯一 turn-level 停止点。
