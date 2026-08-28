# Phase 015: evidence registry target alias 计划

## 阶段目标和授权边界

本阶段补齐当前 topic 的 Evidence Doctor（证据体检）和 evidence registry（证据登记表）Make target，让 Phase 000 的 repeated board summary（重复板卡摘要）可以通过正式 target 生成、登记和检查 freshness（新鲜度）。本阶段只修改 `test-rvv/sample_consensus/sac_model_sphere/Makefile` 和 topic-local 文档，不重新跑板卡，不修改 production。

## 当前基线

| area | 当前状态 |
| --- | --- |
| repeated manifest | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-manifest.json` 已存在，由 topic-local script 生成。 |
| repeated doctor | `doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.md` 已存在，当前报告 `Errors=3, Warnings=0, Suggestions=1`。 |
| registry | `log/evidence_registry.json` 尚不存在。 |
| target alias | Makefile 只有 `generate_board_evidence_manifest`，没有 doctor / registry / status target。 |

## 执行动作

| action | 完成判据 |
| --- | --- |
| 增加 evidence 变量和 doc refs | Makefile 中有 manifest、doctor、registry、doc ref 的稳定变量。 |
| 增加 `run_repeated_board_evidence_doctor` | 可用现有 repeated raw log 和 asm 重新生成 manifest / doctor，不跑板卡。 |
| 增加 `record_repeated_board_evidence_state` | 生成 `log/evidence_registry.json`，登记 manifest、doctor 和 doc refs。 |
| 增加 `repeated_evidence_status` | 可扫描 manifest、doctor 和引用文档，发现未登记或缺引用。 |
| 文档同步 | README、testing overview、benchmark/evidence、Phase 010 result、matrix 和 roadmap 不再把 registry alias 写成未闭合。 |

## 暂停条件

如果 evidence registry script 参数与当前 topic 需求不兼容，或 status target 无法在不登记 raw logs 的前提下通过，本阶段记录 blocked 并保留人工 freshness check。
