# Phase 056 计划：custom row-source large variance profile

## 阶段意图和边界

本阶段接续 Phase 055，只复核两个测试本地 registered custom xyz AoS layout（已注册 xyz 结构数组布局）样本在 256K 大规模 row-source（行来源）路径上的方差和退化原因。目标是把 Phase 055 中 dual-indexed（双索引）和 correspondence（对应关系）256K 的 `negative / unstable` slice 拆成 order pattern（索引顺序模式）证据，而不是扩大 production gate（生产门控）或接入新的 RVV family（实现族）。

本阶段验证范围：

- source：`LocalSVDScalePaddedXYZSource`，`x/y/z` offset = `4/12/24`，`sizeof=32`
- target：`LocalSVDScaleWideXYZTarget`，`x/y/z` offset = `8/20/28`，`sizeof=40`
- row source：dual-indexed、correspondence
- order pattern：contiguous、stride、reverse、shuffle
- size：256K selected pairs；底层 source / target cloud 为 524288 点，indices 合法
- `Scalar=float`、dense、当前 production public overload

不覆盖范围：

- 不接入新 production patch，不改变 `transformation_estimation_svd_scale.hpp`
- 不扩大到任意自定义点型全集、异常 alignment、double xyz 字段、未注册字段或非 standard-layout POD
- 不覆盖非法 index / correspondence、非 dense 输入或 `Scalar=double`
- 不把 QEMU timing（QEMU 计时）写成真实性能结论

## 当前状态清单

| 输入 | 当前事实 |
| --- | --- |
| Phase 055 result | `doc/phases/055-custom-xyz-aos-layout-sampling/result.zh.md` 记录 custom layout ordered 和多数 row-source slice positive，但 256K dual-indexed / correspondence 有退化频率和 long-tail。 |
| optimization matrix | `custom-xyz-aos-layout-sampling` 已写成 mixed；dual-indexed 256K 是 `negative_or_unstable_custom_dual_indexed_256k_slice`，correspondence 256K 是 `mixed_custom_correspondence_256k_slice`。 |
| roadmap | `custom-row-source-large-variance-profile` 是 `phase_deferred + unblocked`。 |
| bench 当前形态 | 已有 `custom-xyz-aos-layout-sampling` case-filter；尚无只跑 custom layout 256K order profile 的窄 filter。 |
| RED 检查 | `--case-filter custom-row-source-large-variance-profile` 当前不输出目标 case，证明本阶段 profile target 尚未存在。 |
| evidence registry | Phase 055 已登记 QEMU / board custom layout 证据；本阶段需新增独立 run label 和 registry 记录，避免覆盖 Phase 055 结论。 |

## 假设与候选族

| 假设 | 需要验证什么 | 可能结论 |
| --- | --- | --- |
| H1：退化主要来自 shuffle-like locality | custom layout 的 contiguous / stride / reverse 256K 仍稳定 positive，shuffle 明显更差。 | 将 Phase 055 退化解释为 order/locality 敏感，不否定 custom layout gate。 |
| H2：custom stride + gather 本身导致大规模不稳 | 即便 contiguous / stride / reverse 也出现退化或长尾。 | 降级 custom layout row-source large-size 证据，后续不建议扩大 custom layout 性能结论。 |
| H3：correspondence sorted-copy heuristic 只覆盖 query disorder，不足以消除 custom layout 方差 | correspondence shuffle 仍 mixed，或与 dual-indexed 不同方向。 | 需要把 custom layout + correspondence 256K 保持独立风险，不继承 PointXYZ 结果。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `custom-row-source-large-variance-profile` | dual-indexed | `LocalPaddedXYZSource -> LocalWideXYZTarget` / `float` / custom xyz AoS | public dual-indexed overload，256K，contiguous / stride / reverse / shuffle | 复用 Phase 055 custom layout correctness；新增 QEMU smoke `max_reference_error <= 2e-3` | 新增 `custom-row-source-large-variance-profile` case-filter | 5-run board repeated，按 order pattern 分桶 | production public row-source profile boundary，asm 输入来自 RVV bench dump | Doctor 必须解释 degradation frequency / long-tail / group-outlier | 待回填 |
| `custom-row-source-large-variance-profile` | correspondence | 同上 | public correspondence overload，256K，contiguous / stride / reverse / shuffle | 同上 | 同上 | 同上 | 同上 | 同上 | 待回填 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 bench case-filter | `src/bench_tesvd_scale.cpp` 新增 `custom-row-source-large-variance-profile` | RVV bench 输出 8 个 case：2 row source × 4 order pattern × 256K。 |
| A2 QEMU / registry target | `Makefile` 新增 QEMU smoke、manifest、Doctor、registry target | QEMU smoke 8 comparisons；Doctor `Errors=0` 或异常已解释；registry fresh。 |
| A3 board repeated target | `Makefile` 新增 board repeated、summary、Doctor、registry target | 5-run board repeated 8 comparisons；Doctor finding 全部进入 result。 |
| A4 script classification | `generate_tesvd_scale_qemu_evidence_manifest.py` 和 `generate_tesvd_scale_board_repeated_summary.py` 识别新 case-filter | manifest 写入 `production_public_custom_row_source_profile`，row_source 能按 label 拆成 dual-indexed / correspondence。 |
| A5 文档同步 | phase result、phase README、optimization matrix、roadmap、topic-local evidence 文档 | 写清本阶段只解释 Phase 055 large row-source 方差，不改 production。 |

## Evidence Doctor 和 registry 规则

- QEMU manifest：`log/qemu/custom_row_source_large_variance_profile/evidence_manifest.json`
- QEMU Doctor：`log/qemu/custom_row_source_large_variance_profile/evidence_doctor.md`
- Board summary：`log/board/custom_row_source_large_variance_profile_repeated/summary.md`
- Board Doctor：`log/board/custom_row_source_large_variance_profile_repeated/evidence_doctor.md`
- Board manifest：`log/board/custom_row_source_large_variance_profile_repeated/evidence_manifest.json`
- evidence role：`production_public_custom_row_source_profile`
- run budget：5 repeated runs，`iterations=20`，`warmup_iterations=5`
- decision bucket：`positive` 需要 5/5 B/A > 1.20；`weak_positive` 允许 median >= 1.05 且 min >= 0.97；若任一 case 出现多次 B/A < 1，降级为 `negative` 或 `unstable`，不得写 clean positive。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_row_source_profile`，真实 public overload 上的 custom layout profile。 |
| A/B boundary | Std 构建 public scalar path vs RVV 构建 public RVV path；不是 RVV-vs-RVV family selection。 |
| 当前决策问题 | Phase 055 的 256K custom layout dual-indexed / correspondence 退化是否和 order/locality 相关。 |
| diagnostic 是否可外推到 production | 只可外推到这两个测试本地 registered custom layout 样本和本阶段 order pattern；不可外推到任意自定义点型全集。 |
| comparison-boundary / baseline mismatch 风险 | 有。不同 order pattern 代表不同输入分布；result 必须分 row source / order pattern 报告。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不新增 production probe；弱 / 负结果只用于降级 custom layout large row-source 证据边界。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要，本阶段没有新 implementation family；若后续提出 mitigation，则需要独立 RVV-vs-RVV detail A/B。 |

## 阶段完成条件

- `plan.zh.md` 已先于 bench / Make / script 修改写入。
- 新 case-filter 在 QEMU 下输出 8 个目标 case，且 `max_reference_error <= 2e-3`。
- board repeated 若可用，完成 5-run summary、manifest、Evidence Doctor 和 registry 刷新。
- `result.zh.md` 逐项回填 A1-A5，并写明 Evidence Doctor Errors / Warnings / Suggestions 对结论的影响。
- optimization matrix / roadmap / phase README 同步更新，不把本阶段结果写成 production gate 扩大。

## 继续 / 停止条件

默认继续到 QEMU 和板卡证据闭合。合法停止条件只有：板卡不可达、工具失败、Evidence Doctor Error 无法解释、registry 不 fresh、dirty isolation 不安全，或本阶段 8 个 case 已完成并回填文档。

下一阶段默认入口由本阶段结果决定：

- 若 shuffle 单独退化：后续可以考虑 `custom-shuffle-mitigation-detail-ab`，但需另开 RVV-vs-RVV detail A/B。
- 若所有 order pattern 都不稳：保持 custom large row-source evidence 降级，不建议继续扩大 custom layout 性能结论。
- `Scalar=double` 仍需用户确认数值预算后另开 phase。
