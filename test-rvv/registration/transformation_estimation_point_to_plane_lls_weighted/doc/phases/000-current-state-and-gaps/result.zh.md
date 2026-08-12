# Phase 000: 当前状态与缺口恢复结果

## 计划与实际范围

本 phase 完成了 current state / gap 恢复、optimization matrix 建档、source-indexed 与 row-source 的 machine-readable Evidence Doctor / manifest 边界补齐，并把这些边界回填到 topic docs 和 Handoff Packet。

## 已完成项

| 动作 | 状态 | 证据路径 / 说明 |
| --- | --- | --- |
| A1 建立 phase 文档入口 | done | `doc/phases/README.zh.md`、`doc/phases/000-current-state-and-gaps/plan.zh.md`。 |
| A2 source-indexed production summary 的 manifest / doctor 边界 | done | `log/board/production_source_indices_staged_gather/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`。doctor 结果 0 Errors / 7 Warnings / 6 Suggestions。 |
| A3 row-source diagnostic 的 manifest / doctor 边界 | done | `log/board/run_board_bench_row_sources/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`。doctor 结果 0 Errors / 0 Warnings / 0 Suggestions。 |
| A4 topic docs 的第二轮同步 | done | `README.zh.md`、`optimization-evidence.zh.md`、`benchmark-and-evidence.zh.md`、`testing-overview.zh.md`、`transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md`、本 phase `plan.zh.md` 已回填实际边界和 doctor 结果。 |
| A5 phase result 完整收口 | done | 当前文件。 |
| A6 Handoff Packet | done | 本轮最终回复将包含 dirty isolation、doctor 结果、optimization matrix 状态和 next action。 |

## Optimization Matrix 状态

| candidate family | row source policy | 状态 | 下一步 |
| --- | --- | --- | --- |
| block-reduction + A/B/C/N + fused-abcd-ilp | full-cloud | adopted | 只维护边界与 accepted-risk，不再外推。 |
| staged-gather / compressed-tail | source-indexed | adopted | 已有 production direct repeated board 边界；仍保留 asm / binary identity warning，不把 current family 当成 clean pass。 |
| source-indexed block-reduction / fused-formula carry-over | source-indexed | planned / audit | 先做 test-rvv 同边界 candidate / bench / board。 |
| dual-indices diagnostic family | dual-indices | diagnostic only | 先补 candidate / bench / board，再谈 production。 |
| correspondences diagnostic family | correspondences | diagnostic only | 先补 candidate / bench / board，再谈 production。 |

## Evidence Doctor 结果

| 证据 | 结果 | 备注 |
| --- | --- | --- |
| production-dispatch full-cloud | `0 Errors / 0 Warnings / 3 Suggestions` | binary identity suggestion 不阻塞当前结论。 |
| source-indexed production summary | `0 Errors / 7 Warnings / 6 Suggestions` | 主要是 source-indexed-specific asm boundary、long-tail 和 binary identity 未闭合。 |
| row-source diagnostic | `0 Errors / 0 Warnings / 0 Suggestions` | 只保留 pre-production diagnostic 边界，不升级成 production direct。 |

## 继续 / 停止决策

当前 phase 已完成，且下一步已经明确为新的 family carry-over audit。

下一阶段默认入口：

`doc/phases/000-current-state-and-gaps/plan.zh.md` 中已标注的 source-indexed family carry-over audit，或等价的新阶段 `010-source-indexed-family-carry-over`。

## Handoff 片段

- dirty isolation：工作树本来就 dirty，新增的 phase/doc/script 变更只是在现有差异上追加，不回退既有改动。
- 关键新边界：`log/board/production_source_indices_staged_gather/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`；`log/board/run_board_bench_row_sources/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json`。
- 下一步：若继续优化，不要直接改 production C++，先在 test-rvv 补 source-indexed family comparison，再决定 dual-indices / correspondences 是否进入 production path。
