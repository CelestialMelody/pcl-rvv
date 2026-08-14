# ICP phase 入口

当前 topic 使用 phase loop（阶段循环）推进。每个 phase 先写 `plan.zh.md`，实现和验证后再写
`result.zh.md`，跨阶段候选和默认恢复动作维护在 `../optimization-roadmap.zh.md`。

| phase | 状态 | 入口 |
| --- | --- | --- |
| `001-transform-cloud-diagnostic` | diagnostic_done | `001-transform-cloud-diagnostic/result.zh.md` |
| `002-board-repeated-diagnostic` | positive_done | `002-board-repeated-diagnostic/result.zh.md` |
| `003-production-integration` | production_direct_positive | `003-production-integration/result.zh.md` |
| `004-structure-parity-doc-suite` | docs_adopted / artifact_tracking_pending_commit_boundary | `004-structure-parity-doc-suite/result.zh.md` |

默认恢复动作：

1. 先运行 `git status --short --untracked-files=all -- test-rvv/registration/icp doc-rvv/registration/icp-RVV.zh.md`，
   确认 README 引用的 doc-suite 文件是否仍有 untracked。
2. 若准备提交 ICP 文档补齐，必须把 Phase 004 新增的 topic-local docs 与 README 同一 topic commit
   纳入审查；否则 README 会引用提交中不存在的文档。
3. 若只复核运行证据，按 `../optimization-roadmap.zh.md` 的默认恢复动作执行，不运行 QEMU bench compare。
| `004-structure-parity-doc-suite` | doc_suite_parity_done | `004-structure-parity-doc-suite/result.zh.md` |
