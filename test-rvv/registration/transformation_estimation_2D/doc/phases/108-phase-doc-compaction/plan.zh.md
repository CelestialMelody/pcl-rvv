# Phase 108 Plan: phase-doc-compaction

## 阶段意图和边界

本阶段只整理 `doc/phases` 的读者入口和提交前文档边界，降低 Phase 000-107 多阶段记录对 reviewer 的阅读压力。阶段目标是把当前恢复入口、关键决策和历史索引分层：`README.zh.md` 只保留当前状态和读者路径，新增历史压缩索引保存阶段脉络，详细事实仍以各 phase 的 `result.zh.md`、optimization matrix（优化矩阵）和 evidence summary（证据摘要）为准。

本阶段不修改 production 源码，不删除历史 phase 事实，不覆盖 Phase 103/104/106/107 的证据，不把 guarded probe（受保护探针）写成 adopted（已采纳）。

## 当前状态清单

当前 `doc/phases` 有 000-107 的多阶段目录，外加 `README.zh.md` 和 `optimization-matrix.zh.md`。Phase 107 已完成 production patch candidate（生产补丁候选）证据闭环：

- ordered-cloud-pair generic 已采纳并保留。
- source-indexed exact `PointXYZ -> PointXYZ` 已采纳并保留。
- dual-indexed exact `PointXYZ -> PointXYZ` 已接入当前 patch candidate，等待用户确认后提交。
- source-indexed generic widening 经 Phase 106 20-run variance negative，不采纳。
- correspondence exact trial 经 Phase 107 family A/B negative 后已退回。

当前 hygiene（卫生检查）状态：`run_test_compare` / `record_qemu_correctness_state` 为 Std/RVV `84/84` pass，`evidence_status` fresh，`git diff --check` pass。

## 整理动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 压缩 phase README | `doc/phases/README.zh.md` | 只保留当前恢复入口、关键结论、阅读路径、证据状态和停止规则。 |
| 新增历史压缩索引 | `doc/phases/history.zh.md` | 按阶段组总结 Phase 000-107，不替代每个 phase 的 result。 |
| 同步恢复口径 | roadmap / matrix / Handoff | 明确 Phase 108 是文档整理，不改变生产边界。 |
| 刷新 hygiene | `evidence_status`、`git diff --check` | registry fresh，空白检查通过。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 benchmark / board / asm evidence。若文档整理改动了 evidence registry（证据登记表）的引用输入，必须重跑：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

若发现 `unregistered_change`、`unregistered_file` 或 `doc_ref_missing`，先修复文档引用或 registry 状态，再进入提交。

## 阶段完成条件

- `doc/phases/README.zh.md` 成为短入口。
- 历史阶段有一个压缩索引可读。
- Phase 107 生产结论保持不变。
- Phase 103/104 仍写作 guarded probe；Phase 106 仍写作 negative；correspondence 仍写作 rolled back。
- `evidence_status` fresh。
- `git diff --check` pass。

## 继续 / 停止条件

本阶段完成后回到提交准备：先列出 topic-only commit（主题提交）候选文件，再按用户确认创建 commit。不自动提交 raw logs、本地 build 输出、私有路径或未被文档引用的临时文件。
