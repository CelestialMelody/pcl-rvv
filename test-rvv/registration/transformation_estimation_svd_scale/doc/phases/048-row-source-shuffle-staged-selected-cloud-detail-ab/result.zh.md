# Phase 048 结果：row-source shuffle staged-selected-cloud detail A/B

## 执行摘要

本阶段把 `staged-selected-cloud-detail-ab` 作为 RVV-vs-RVV detail A/B 跑完：比较 current shuffled row-source RVV path 与 staged selected-cloud path（先把 source / target 对应点复制成顺序点云，再走 ordered public path）。

结果是明确的 negative / mixed negative：copy + staging 成本吞掉了预期收益，dual-indexed 与 correspondence 的 4K / 64K / 256K 都没有形成可接入生产的正向子边界。

## 证据结果

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| QEMU smoke | 完成 | `record_qemu_row_source_staged_selected_cloud_detail_ab_state` 通过；Doctor 只用于日志形状和 metadata 复核。 |
| board repeated detail A/B | 完成 | 5 runs 的 six cases 全部 negative；对应 4K / 64K / 256K 结果均不支持接入。 |
| board Evidence Doctor | 完成 | `Errors=6`、`Warnings=9`、`Suggestions=0`；Errors 主要来自 4K 以及高频退化。 |

### board 结果

| case | median B/A | bucket | 结论 |
| --- | ---: | --- | --- |
| correspondence 4K | `0.711x` | negative | staged-selected-cloud 在小规模显著退化。 |
| dual-indexed 4K | `0.773x` | negative | staged-selected-cloud 在小规模显著退化。 |
| correspondence 64K | `0.482x` | negative | copy + select 成本远高于当前 shuffled gather。 |
| dual-indexed 64K | `0.776x` | negative | staged-selected-cloud 不稳定且总体为负。 |
| correspondence 256K | `0.563x` | negative | copy + select 成本远高于当前 shuffled gather。 |
| dual-indexed 256K | `0.958x` | negative | 接近 1 但仍不足以接入。 |

## Evidence Doctor 解释

Board Doctor 的 6 个 Errors 都来自高频退化：correspondence 4K / 64K / 256K、dual-indexed 4K / 64K 以及 dual-indexed 256K 均出现多 run 低于 1。9 个 Warnings 主要是 long-tail / variance 和 group-outlier，处理方式是按 row source 和 size 分开报告，而不是用组内均值掩盖负向。

QEMU smoke 的数值仍只作为 build、label、manifest shape 和 correctness-adjacent 日志检查；它不参与性能结论。

## Diagnostic-to-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | `production_detail_rvv_vs_rvv`，但仍是 test-rvv detail A/B，不是 production adoption evidence。 |
| A/B boundary | current shuffled row-source public RVV path vs staged selected-cloud + ordered public RVV path。 |
| 当前决策问题 | staged selected-cloud 是否比 current gather / sorted-copy 更值得继续为 shuffled row-source implementation family。 |
| 是否可外推到 production | 不可作为接入信号外推。copy + staging 成本已计入计时，board repeated 已显示负向；真实 production 还会增加 dispatch / allocation 维护成本。 |
| comparison-boundary / baseline mismatch 风险 | 有。candidate 的 row-source 外壳与 baseline 不同，因此本阶段只支撑 family-selection negative。 |
| weak / negative / unstable 时是否允许 bounded production probe | 不允许。没有任何 64K / 256K 子边界达到 positive，dual-indexed 256K 也只是 `0.958x`。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段已提供 negative detail A/B，因此不进入 clean adoption。 |

## 文档同步

已同步或需要同步的当前状态：

- `doc/phases/README.zh.md`：Phase 047 改为 adopted，Phase 048 改为 attempted negative。
- `doc/phases/optimization-matrix.zh.md`：新增 staged-selected-cloud negative 行，并把 correspondence sorted-copy production probe 改成 adopted。
- `doc/optimization-roadmap.zh.md`：把 staged-selected-cloud 从 unblocked 改为 attempted negative；默认恢复队列不再指向它。
- `README.zh.md`、`doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/transformation_estimation_svd_scale-evaluation.zh.md`、`doc/test-support-code-map.zh.md`、`doc/correctness-tests.zh.md`：补充 Phase 047 adopted 和 Phase 048 negative 边界。
- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`：只同步 Phase 047 已采纳的 production behavior；Phase 048 保持 topic-local diagnostic 记录。

## 当前决策

`attempted_negative_or_mixed / staged-selected-cloud-detail-ab`。

这条路线不应继续写成 production candidate。它说明在当前 topic 的 row-source / size / layout 边界里，staged selected-cloud 比 sorted-copy 更重，不能替代已经 adopted 的 correspondence sorted-copy production probe。

## 下一步

如果后续还要继续探索，只能另起新的 candidate family 或新的更窄诊断边界；当前 staged-selected-cloud 路线应按 negative 结果收束。
