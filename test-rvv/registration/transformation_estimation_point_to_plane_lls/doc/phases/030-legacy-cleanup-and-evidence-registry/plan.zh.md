# Phase 030 Plan: Legacy Cleanup And Evidence Registry

## 阶段意图和边界

本阶段按当前 agent asset（代理资产）规则继续 Phase 020 后的未闭合项。当前默认配置要求：
legacy pointer（旧路径指针）和 compatibility alias（兼容别名）默认不保留，除非存在明确外部依赖、
用户限定兼容或 dirty isolation（脏工作隔离）风险。恢复时的 `rg` 检查显示，根目录 evaluation
旧指针和旧聚合头没有当前源码、Make target 或脚本依赖；仅历史 phase 文档记录了当时保留决策。

本阶段做两个低风险动作：

- 删除无依赖的根目录 evaluation legacy pointer 和旧 test support compatibility alias。
- 用现有通用 `test-rvv/script/evidence_registry.py` 登记当前文档引用的 summary-only evidence
  （摘要证据）和本机 QEMU correctness log（QEMU 正确性日志），并运行 freshness check。

本阶段不做：

- 不修改 production header、RVV hot path、dispatch gate、fallback gate、bench case 或测试语义。
- 不运行 QEMU timing、板卡 bench 或刷新 performance（性能）结论。
- 不提交 raw logs（原始日志）、build 输出、完整 asm dump 或 weighted topic 证据。
- 不修改 `.agents/` agent asset；当前 agent asset 策略为 report-only（只报告建议）。

## S0 和 Dirty Isolation

| 项 | 当前状态 |
| --- | --- |
| `preferences_loaded` | 已读取 `AGENTS.md`、`.agents/config/defaults.yaml`、`.agents/knowledge/pcl-rvv-knowledge-map.md`、`rvv-workflow`、`rvv-test`、`rvv-documentation`、phase loop、evidence registry/output policy；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 要求恢复 phase loop、检查 git status 和 dirty isolation。 |
| 注释策略 | test-rvv / diagnostic / prototype 详细中文；production 注释只解释维护边界、fallback、dispatch、数值风险和数据布局。 |
| 文档策略 | closeout 当前状态优先；长期文档不写对话流程；英文术语首次出现带中文解释；无依赖 legacy pointer / alias 默认删除。 |
| 证据策略 | summary-only；raw logs 不默认提交；QEMU 只用于 correctness、路径和日志形状；registry 只登记已被文档引用或本阶段验证的摘要/日志。 |
| dirty isolation | 当前 worktree 还混有 weighted topic 和 test-rvv `.gitignore` dirty。本阶段只允许修改当前 topic 的 phase/evaluation/roadmap/matrix、topic doc、registry 和删除当前 topic 旧入口；weighted dirty 作为 separate review 边界，不读取为当前结论。 |

当前允许路径：

```text
doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md
test-rvv/registration/transformation_estimation_point_to_plane_lls/
```

## 当前状态清单

| 领域 | 当前事实 | 路径 |
| --- | --- | --- |
| production candidate | full-cloud f32 AoS layout-gated `Scalar=float` fused-formula block dispatch；三类代表点型 5-run 板卡正向。 | `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |
| legacy pointer | 根目录 `transformation_estimation_point_to_plane_lls-evaluation.zh.md` 只指向 `doc/` 主路径；`rg` 未发现当前脚本或源码依赖。 | topic 根目录旧文件 |
| compatibility alias | `test_support_transformation_estimation_point_to_plane_lls.hpp` 只转发到 `include/teptpl.h`；新 test/bench 已 include 新聚合入口。 | topic 根目录旧头 |
| evidence registry | topic 尚无 `log/evidence_registry.json`，Phase 020 标记为未阻塞后续项。 | 应为 `log/evidence_registry.json` |
| summary evidence | 两个 board summary 已被 evaluation / topic doc 引用。 | `output/board/*_summary.md` |
| QEMU correctness logs | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` 是本机 generated logs，phase result 已引用为 correctness 证据。 | `log/qemu/` |

## 假设与候选族

| 假设 | 本阶段验证方式 |
| --- | --- |
| 删除旧 pointer / alias 不影响构建。 | `rg` 依赖检查；`run_test_std` / `run_test_rvv`。 |
| registry 可以用现有通用脚本记录当前 summary-only evidence，不需要新 topic-local parser。 | 对 summary/QEMU log 使用 `record`；随后用 `check --require-doc-ref` 检查 hash 和文档引用。 |
| 由于不改 hot path、bench case 或 dispatch gate，不需要板卡复跑。 | `git diff` 审计和本阶段 result 记录 0 次板卡预算。 |

## 本阶段优化矩阵

本阶段新增 `legacy cleanup / evidence registry adoption` 行，见 `../optimization-matrix.zh.md`。生产候选状态不变。

## 实现和测试动作

| id | 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- | --- |
| C0 | 写本 phase plan 并冻结范围。 | 本文件。 | plan 先于 Phase 030 文件修改存在。 |
| C1 | 删除无依赖旧入口。 | 删除根目录 evaluation pointer 和旧 alias 头；更新 current docs。 | `rg` 不再发现当前依赖；历史 phase 文档可保留当时事实。 |
| C2 | 接入 evidence registry。 | `log/evidence_registry.json`；`python3 test-rvv/script/evidence_registry.py record/check ...`。 | registry check fresh；doc refs 覆盖已登记文件或 run label。 |
| C3 | 同步 README、matrix、roadmap、evaluation 和 topic doc。 | phase README、optimization matrix、roadmap、evaluation、doc-rvv topic doc。 | 当前恢复入口指向 030，legacy / registry 状态不冲突。 |
| C4 | 验证。 | `git diff --check`；`rg` 旧入口检查；`make ... run_test_std`；`make ... run_test_rvv`。 | diff check 干净；std/RVV QEMU correctness 仍通过。 |
| C5 | 回填 phase result。 | `result.zh.md`。 | 每项动作有 `done / partial / deferred / blocked`、证据路径和 continue / stop decision。 |

## Evidence Doctor 和 Registry 规则

本阶段不生成新的 benchmark、board summary、checksum summary 或 asm attribution，因此不运行 JSON manifest
形式 Evidence Doctor（证据体检）。registry 采用通用脚本记录已存在的 summary/QEMU correctness 文件：

```text
test-rvv/script/evidence_registry.py record/check
```

若 `check` 发现 `unregistered_change`、`unregistered_file` 或 `doc_ref_missing`，先更新 registry 或文档引用；
若无法归属则在 result 中降级为 `blocked` 或 `stale_doc_pending_refresh`。

## 板卡复跑预算和决策桶

本阶段板卡复跑预算为 `0`。理由是删除无依赖旧入口和 registry 记录不改变 RVV hot path、bench case、
case filter、dispatch gate 或 fallback gate。既有 production-dispatch decision bucket 保持
`positive` within current production candidate boundary。

## 继续 / 停止条件

继续条件：

- registry check 或引用检查发现当前 topic 文档矛盾，且可在当前 topic 文档边界内修复。
- 删除旧入口导致构建失败，需要恢复依赖或更新 include。

停止条件：

- C0-C5 闭合，production candidate 未改变，registry fresh，旧入口无依赖删除完成。
- 剩余动作属于独立 phase：production helper shape review、test source split、`test_support/` 内部重命名、
  更多点型、`Scalar=double` 或 row-source production follow-up。

默认下一阶段：

- 若本阶段通过，`next_phase_default=ready_for_review`。
- 若 reviewer 要求继续当前 topic，优先从 roadmap 选择 `030-helper-shape-review` 或
  `030-test-source-split`，但它们会改变 hot path 或产生大搬迁 diff，需单独 plan。

## 文档更新清单

| 文档 | 本阶段动作 |
| --- | --- |
| `doc/phases/030-legacy-cleanup-and-evidence-registry/result.zh.md` | 阶段结束时新建并回填事实。 |
| `doc/phases/README.zh.md` | 增加 Phase 030 当前恢复入口和 registry 状态。 |
| `doc/phases/optimization-matrix.zh.md` | 增加 legacy cleanup / evidence registry adoption 行。 |
| `doc/optimization-roadmap.zh.md` | 将 legacy cleanup / registry adoption 标为本阶段处理或完成，重排剩余候选。 |
| `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md` | 移除旧 pointer / alias 当前状态，补 registry 状态。 |
| `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` | 如需同步 evidence registry 路径，做窄更新。 |
