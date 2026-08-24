# Phase 057 计划：custom layout padding sensitivity

## 阶段意图和边界

本阶段接续 Phase 055 / 056，继续自定义 registered xyz AoS layout（已注册 xyz 结构数组布局）方向，但只做保守取样：新增两个测试本地 custom layout 组合，覆盖更紧凑和更宽 padding / stride（填充 / 步长）形状，检查当前 traits-gated production public path（基于 traits 门控的生产公开路径）是否对字段 offset 和 `sizeof(PointT)` 更敏感。

验证范围：

- `Scalar=float`
- dense input
- 当前 production public overload
- row source：source-indexed、dual-indexed、correspondence
- size：64K、256K selected pairs
- order pattern：沿用 Phase 055 的 stride source / alt target index 形状
- point layout：
  - compact-ish：`LocalSVDScaleCompactXYZSource -> LocalSVDScaleCompactXYZTarget`
  - huge-padding：`LocalSVDScaleHugePaddingXYZSource -> LocalSVDScaleHugePaddingXYZTarget`

不覆盖范围：

- 不修改 production 源码，不扩大 production gate
- 不声称覆盖任意自定义点型全集、异常 alignment、未注册字段、非 standard-layout POD 或非法 index / correspondence
- 不验证 `Scalar=double`，double 仍需要用户确认数值预算
- 不把 QEMU timing（QEMU 计时）写成真实性能结论

## 当前状态清单

| 输入 | 当前事实 |
| --- | --- |
| Phase 055 | 两个 custom layout 样本 ordered 和多数 row-source slice positive；256K dual-indexed / correspondence mixed。 |
| Phase 056 | 同两个样本的 256K dual-indexed / correspondence 受控 order profile 8/8 positive，但 Board Doctor 有 8 个 variance / group-outlier warnings。 |
| roadmap | `more-custom-xyz-aos-board` 需要新的 layout / padding 取样空间和 board budget；本阶段把取样空间限定为 2 个 layout 组合 × 3 row source × 2 size。 |
| matrix | 自定义 layout 不能外推到全集；`Scalar=double` 仍 not_applicable。 |
| RED 检查 | `--case-filter custom-layout-padding-sensitivity` 当前没有目标 case。 |

## 假设与候选族

| 假设 | 需要验证什么 | 可能结论 |
| --- | --- | --- |
| H1：当前 RVV xyz AoS traits 对常见 padding / stride 仍稳定 | 两个新增 layout 的 12 个 board case 都保持 positive 或 weak-positive。 | 自定义 layout 方向可升级为 sampled-positive，但仍不是全集 clean positive。 |
| H2：极宽 padding 会放大 gather / segment stride 成本 | huge-padding layout 的 row-source 64K / 256K 明显低于 compact-ish layout，或出现 long-tail。 | 把 custom layout 结论按 layout stride 降级，后续不扩大 production claim。 |
| H3：row source 比 layout padding 更关键 | 同一 layout 下 source-indexed、dual-indexed、correspondence 的差异大于 layout 间差异。 | 后续继续优先按 row source / locality 解释，而不是新建 layout-specific production path。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `custom-layout-padding-sensitivity` | source-indexed / dual-indexed / correspondence | compact-ish custom xyz AoS / `float` / 64K + 256K | production public row-source overload | QEMU smoke `max_reference_error <= 2e-3`，Std/RVV correctness freshness | `custom-layout-padding-sensitivity` case-filter | 5-run board repeated，6 comparisons | production public boundary；不新增 RVV family | Doctor 必须解释 long-tail / group-outlier | 待回填 |
| `custom-layout-padding-sensitivity` | source-indexed / dual-indexed / correspondence | huge-padding custom xyz AoS / `float` / 64K + 256K | 同上 | 同上 | 同上 | 5-run board repeated，6 comparisons | 同上 | 同上 | 待回填 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 新 layout fixture | `include/impl/tesvd_scale_support.hpp` 新增 4 个测试本地 registered xyz AoS 类型 | static layout scan 能证明 offset / sizeof 不同于 Phase 055 样本和 PCL 常见点型。 |
| A2 correctness case | `src/test_tesvd_scale.cpp` 增加 custom padding layout reference test | Std / RVV 当前 correctness aggregate 通过。 |
| A3 bench case-filter | `src/bench_tesvd_scale.cpp` 新增 `custom-layout-padding-sensitivity` | QEMU bench 输出 12 个 case：2 layout × 3 row source × 2 size。 |
| A4 QEMU / registry target | `Makefile` 和 QEMU manifest script 新增 smoke / Doctor / registry target | QEMU 12 comparisons；Doctor `Errors=0` 或异常已解释；registry fresh。 |
| A5 board repeated target | `Makefile` 和 board summary script 新增 repeated / Doctor / registry target | 5-run board repeated 12 comparisons；summary / manifest / Doctor 已生成。 |
| A6 文档同步 | result、phase README、optimization matrix、roadmap、topic evidence docs | 只写成 sampled layout padding sensitivity，不扩大 production gate。 |

## Evidence Doctor 和 registry 规则

- QEMU manifest：`log/qemu/custom_layout_padding_sensitivity/evidence_manifest.json`
- QEMU Doctor：`log/qemu/custom_layout_padding_sensitivity/evidence_doctor.md`
- Board summary：`log/board/custom_layout_padding_sensitivity_repeated/summary.md`
- Board Doctor：`log/board/custom_layout_padding_sensitivity_repeated/evidence_doctor.md`
- Board manifest：`log/board/custom_layout_padding_sensitivity_repeated/evidence_manifest.json`
- evidence role：`production_public_custom_layout_padding_sensitivity`
- run budget：5 repeated runs，`iterations=20`，`warmup_iterations=5`
- decision bucket：`positive` 需要 5/5 B/A > 1.20；`weak_positive` 允许 median >= 1.05 且 min >= 0.97；若任一 case 多次 B/A < 1，降级为 `negative` 或 `unstable`。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_custom_layout_padding_sensitivity`。 |
| A/B boundary | Std build public scalar path vs RVV build public RVV path；不是 RVV-vs-RVV family selection。 |
| 当前决策问题 | 更紧凑和更宽 padding / stride 的 registered xyz AoS layout 是否仍能走当前 adopted production path，并保持正向性能。 |
| diagnostic 是否可外推到 production | 只可外推到本阶段两个新增 layout 组合、三类 row source、两个 size 和当前合法 dense 输入。 |
| comparison-boundary / baseline mismatch 风险 | 有。layout、row source、size 同时变化，result 必须按组合独立报告。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不新增 production probe；弱 / 负结果只用于降级 custom layout evidence。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要，因为本阶段没有新 implementation family；若后续提出 layout-specific mitigation，则需要。 |

## 阶段完成条件

- 计划先于 bench / Make / script 修改写入。
- 新 case-filter 在 QEMU 下输出 12 个目标 case，且 `max_reference_error <= 2e-3`。
- correctness aggregate 通过并登记 freshness。
- board repeated 完成 5-run summary、manifest、Evidence Doctor 和 registry 刷新。
- result、matrix、roadmap、phase README 与 topic evidence docs 同步，不把本阶段结果写成 custom layout 全量 clean positive。

## 继续 / 停止条件

默认继续到 QEMU 和板卡证据闭合。合法停止条件只有：板卡不可达、工具失败、Evidence Doctor Error 无法解释、registry 不 fresh、dirty isolation 不安全，或本阶段 12 个 case 已完成并回填文档。

下一阶段默认入口由本阶段结果决定：

- 若新增 layout 全部 positive：把 custom layout 证据写成 `sampled_positive_with_scope_limits`，但不扩大 production gate。
- 若 huge-padding 出现退化：把 custom layout 性能边界按 layout stride 降级，后续只在用户定义更广采样预算后继续。
- 若只有 row-source shuffle / large-size 不稳：回到 row-source locality / mitigation 方向，另开 RVV-vs-RVV detail A/B。
- `Scalar=double` 仍需用户确认数值预算后另开 phase。
