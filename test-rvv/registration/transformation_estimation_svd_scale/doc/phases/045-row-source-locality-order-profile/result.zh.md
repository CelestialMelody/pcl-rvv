# Phase 045 结果：row-source locality / order profile

## 执行摘要

本阶段完成 `row-source-locality-order-profile`（row-source 局部性 / 顺序剖析）。它不修改 production 源码，也不提出新的 RVV family（RVV 实现族）；它只解释 Phase 043 / 044 中 row-source board Evidence Doctor（证据体检）的 long-tail / variance（长尾 / 方差）和 group-outlier（组内离群）warning。

结论是：

```text
profile_positive_with_locality_sensitivity / production_public_row_source_profile / PointXYZ -> PointXYZ / Scalar=float / dense xyz AoS
```

QEMU smoke 覆盖 36 个比较，Doctor clean。板卡 5-run repeated 覆盖 36 个 case，全部为 `positive`，overall decision bucket 为 `positive`；Board Doctor 为 `Errors=0`、`Warnings=29`、`Suggestions=0`。这些 warning 不阻塞 Phase 043 / 044 的采纳结论，但明确说明 index / correspondence 的 order pattern（顺序模式）会显著影响 B/A 和稳定性，尤其是 shuffle。

## 源码 / 测试 / bench / script 变化

| area | change | evidence role |
| --- | --- | --- |
| fixture helper | `include/impl/tesvd_scale_support.hpp` 新增 reverse 和 deterministic shuffle index helper。 | 构造合法 index / correspondence order pattern，不改变 production。 |
| bench wrapper | `src/bench_tesvd_scale.cpp` 新增 `row-source-locality-order-profile` case-filter。 | 生成 3 row source x 4 order pattern x 3 size = 36 个 label。 |
| Makefile target | 新增 `record_qemu_row_source_locality_order_profile_state`、`run_board_bench_row_source_locality_order_profile_repeated` 和对应 doctor / registry target。 | QEMU smoke、board repeated、Evidence Doctor 和 registry 入口。 |
| manifest / summary script | topic-local QEMU manifest 和 board summary wrapper 识别 profile evidence role。 | 生成可登记的 summary、manifest 和 doctor 输入。 |

## 证据结果

| evidence | result | path / notes |
| --- | --- | --- |
| correctness rerun | Std/RVV 各 12 个 gtest passed。 | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded` |
| QEMU profile smoke | 36 comparisons；`Errors=0`、`Warnings=0`、`Suggestions=0`。 | `log/qemu/row_source_locality_order_profile/evidence_doctor.md` |
| board profile repeated | 36/36 case bucket 为 `positive`；overall decision bucket 为 `positive`。 | `log/board/row_source_locality_order_profile_repeated/summary.md` |
| board Evidence Doctor | `Errors=0`、`Warnings=29`、`Suggestions=0`。 | `log/board/row_source_locality_order_profile_repeated/evidence_doctor.md` |
| registry | profile QEMU / board evidence 已登记，当前 `evidence_status` fresh。 | `log/evidence_registry.json` |

## Board Profile 摘要

| order pattern | source-indexed median B/A | dual-indexed median B/A | correspondence median B/A | interpretation |
| --- | --- | --- | --- | --- |
| contiguous 64K / 256K | `18.146x` / `17.383x` | `13.954x` / `13.412x` | `12.538x` / `11.868x` | contiguous 是最强边界，说明 row-source public RVV path 在局部性好时非常稳定。 |
| stride 64K / 256K | `14.790x` / `16.356x` | `10.386x` / `8.528x` | `9.937x` / `11.082x` | stride 仍保持强 positive，说明常规确定性跨步 index 不削弱采纳边界。 |
| reverse 64K / 256K | `14.708x` / `16.371x` | `10.497x` / `10.457x` | `9.964x` / `11.012x` | reverse 与 stride 接近，反向顺序不是主要风险。 |
| shuffle 64K / 256K | `3.931x` / `5.332x` | `2.344x` / `2.271x` | `2.363x` / `2.233x` | shuffle 仍 positive，但 dual-indexed / correspondence 的 64K / 256K 明显下降；这是后续 mitigation 的主要线索。 |

4K 也全部 positive：contiguous / stride / reverse 大多在 `10x` 到 `13x` 区间，shuffle source-indexed 为 `8.935x`，dual-indexed 为 `4.745x`，correspondence 为 `6.187x`。这说明小规模也没有出现负向或 neutral（中性）信号。

## Evidence Doctor 解释

Board Doctor 的 29 个 warning 分两类：

- long-tail / variance：主要出现在 shuffle 以及部分 stride / reverse 的中大规模 case。由于所有 case 的 min B/A 仍大于 `2.1x`，本阶段不降级为 unstable，只要求后续按 min / median / max 分开报告。
- group-outlier：Doctor 把 36 个 profile case 放在同组里比较，contiguous/source-indexed 和 shuffle/dual-indexed 等自然会成为组内离群。这个 warning 说明不能按一个 overall median 外推到所有 order pattern；它不是 correctness 或 dispatch failure。

处理结论：Phase 043 / 044 的 board warning 更可能来自 locality / order sensitivity（局部性 / 顺序敏感性）和 row source policy（行来源策略）差异，而不是 RVV 分流错误。profile 结果反而补强了已采纳 row-source path 的边界：即使 deterministic shuffle 最差组合仍为 positive。

## Diagnostic-to-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | `production_public_row_source_profile`。使用真实公开 row-source overload，但本阶段只做顺序剖析。 |
| A/B boundary | Std 构建父类标量公开入口 vs RVV 构建 scale 子类 row-source RVV 公开入口。 |
| 当前决策问题 | Phase 043 / 044 warning 是否暴露 locality 风险，以及是否需要后续 mitigation phase。 |
| 是否可外推到 production | 只能外推到本阶段 `PointXYZ -> PointXYZ` / `Scalar=float` / dense xyz AoS / 四种 order pattern；不能外推到全部点型、非法 index/correspondence 或 `Scalar=double`。 |
| comparison-boundary / baseline mismatch 风险 | 有。order pattern 同时影响标量父类路径和 RVV gather 路径的缓存行为；因此按 pattern 分开报告。 |
| weak / negative / unstable 时是否允许 bounded production probe | 本阶段没有 weak / negative / unstable；若未来 profile 出现这类结果，不自动回滚 Phase 043 adopted patch，而是单独降级对应 order pattern 或开 mitigation phase。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 本阶段没有新 RVV family，不需要。若 Phase 046 尝试 staging / sorting / chunk-order mitigation，则必须做同一 production boundary 内 RVV-vs-RVV detail A/B。 |

## 文档同步

已同步或需要同步的当前状态：

- `doc/phases/README.zh.md`：默认恢复入口从 Phase 045 pending 改为 Phase 046 mitigation / commit-boundary audit 判断。
- `doc/phases/optimization-matrix.zh.md`：`row-source-locality-order-profile` 从 `phase_deferred + unblocked` 改为 `profile_positive_with_locality_sensitivity`。
- `doc/optimization-roadmap.zh.md`：新增 `row-source-shuffle-mitigation-detail-ab` 候选。
- `README.zh.md`、`doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/transformation_estimation_svd_scale-evaluation.zh.md`、`doc/test-support-code-map.zh.md`：补充 Phase 045 证据路径和边界。
- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`：只记录已采纳 production 行为下的 locality profile 解释，不把本阶段写成新的 production behavior。

## 下一步

本阶段自身已 closed positive，但它生成了一个明确的新候选：

| candidate | why now | state | next action |
| --- | --- | --- | --- |
| `row-source-shuffle-mitigation-detail-ab` | shuffle 下 dual-indexed / correspondence 的 64K / 256K B/A 从 contiguous 的约 `12x-14x` 降到约 `2.2x-2.4x`，仍 positive 但 locality 敏感很强。 | `phase_deferred + unblocked` | 若继续优化，Phase 046 应做同一 production boundary 内 RVV-vs-RVV detail A/B，评估 staging / sorting / chunk-order 是否能改善 shuffle，而不是再跑 Std/RVV public positive。 |
| `row-source-generic-locality-profile` | Phase 044 的 representative generic row-source 有 12 个 warning；Phase 045 只覆盖 `PointXYZ -> PointXYZ`。 | `phase_deferred_after_mitigation_or_commit_audit` | 若 mitigation 不做或完成后，再决定是否扩到代表泛型点型 order profile。 |
| `invalid-index/correspondence-semantics-audit` | 当前 production gate 依赖父类入口处理非法 index / correspondence；正确性只覆盖合法 deterministic 样本。 | `phase_deferred_after_locality_work` | 提交前可补范围说明或负向语义审计；不影响当前 positive 性能结论。 |

默认下一步不是回滚，也不是扩大已采纳范围；是在 `row-source-shuffle-mitigation-detail-ab` 和提交前审计之间做选择。由于用户要求继续推进优化矩阵，下一 phase 默认指向 `046-row-source-shuffle-mitigation-detail-ab`。
