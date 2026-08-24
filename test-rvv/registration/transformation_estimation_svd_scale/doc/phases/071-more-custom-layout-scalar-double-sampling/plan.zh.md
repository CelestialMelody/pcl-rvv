# Phase 071 Plan: more custom layout `Scalar=double` sampling

## 阶段意图和边界

本阶段在 Phase 069 和 Phase 070 都仍为 `positive_pending_user_confirmation` 的前提下，继续做 custom layout `Scalar=double`（自定义布局双精度）证据扩展。它不做 adoption closeout（采纳收尾），也不把 pending candidate（待用户确认候选）写成 adopted。

验证范围：

- ordered-cloud-pair（顺序点云对）、source-indexed、dual-indexed 和 correspondence public overload。
- 三组已经存在于测试支撑中的 registered custom xyz AoS layout（已注册自定义 xyz 结构数组布局）：
  - `LocalSVDScaleCompactXYZSource -> LocalSVDScaleCompactXYZTarget`
  - `LocalSVDScaleHugePaddingXYZSource -> LocalSVDScaleHugePaddingXYZTarget`
  - `LocalSVDScaleAligned64XYZSource -> LocalSVDScaleAligned32XYZTarget`
- `Scalar=double`、dense、64K board case、合法 index / correspondence。

不覆盖：

- Phase 069 或 Phase 070 的采纳确认。
- 全部自定义点型、packed unaligned float、异常 alignment 全集或非法 index / correspondence 语义。
- sorted-copy double 或新的 RVV-family-selection（RVV 实现族选择）。

## 当前状态清单

- Phase 069：row-source common PCL xyz AoS / `Scalar=double` production probe 已 positive，但仍等待用户确认。
- Phase 070：单个 padded/wide custom layout / `Scalar=double` public scout 已 positive，但仍等待用户确认。
- production patch 当前已把 double dispatch gate（双精度分流门控）放宽到 `std::is_same_v<Scalar, double> && SrcLayout::value && TgtLayout::value`；本阶段先按测试资产验证更多 layout 样本，不再新增 production 行为。
- custom layout float 方向已有 Phase 057 padding sensitivity 和 Phase 059 alignment sensitivity；这些结果不能自动外推到 `Scalar=double`。

## 假设与候选族

候选族：`more-custom-layout-scalar-double-sampling`。

假设：Phase 070 的 f64 widened RVV accumulation（双精度扩宽 RVV 累加）已经只依赖 `RVVXYZAoSFloatLayout` 的 offset / stride；因此 compact、huge-padding 和 aligned 三组 registered xyz AoS layout 在 `Scalar=double` public path 下应与 public double scalar fallback（公开双精度标量回退）对齐，并在 board 上保持 positive 或至少暴露明确的 padding / alignment sensitivity（填充 / 对齐敏感性）。

风险：

- huge-padding 和 aligned layout 可能显著降低收益，甚至使 row-source slice（行来源切片）变成 weak / unstable。
- 当前证据是 production-public scout（真实公开入口侦察），不是 clean adoption；positive 仍需用户确认后另开 closeout。
- 如果 board warning 增多，result 必须保留 min / median / max，不剔除异常。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `more-custom-layout-scalar-double-sampling` | ordered-cloud-pair | compact / huge-padding / aligned custom xyz AoS / `double` | 新增 correctness + detail-hit | 64K board repeated | QEMU / board Doctor | pending |
| `more-custom-layout-scalar-double-sampling` | source-indexed / dual-indexed / correspondence | 同上 | 新增 row-source correctness + fallback boundary 复用 | 64K board repeated | QEMU / board Doctor | pending |

## 实现和测试动作

1. 先做 RED：运行尚不存在的 phase target，确认 `more-custom-layout-scalar-double-sampling` 证据入口缺失。
2. 增加 correctness test（正确性测试）：三组 custom layout 的 ordered 和三类 row-source public double path 与 reference 对齐；RVV 构建下应命中 double RVV path。
3. 增加 bench case-filter、QEMU smoke target、board repeated target、Evidence Doctor 和 evidence registry 登记。
4. 运行 correctness、QEMU smoke + Evidence Doctor、board repeated + Evidence Doctor、evidence freshness。
5. 更新 phase result、optimization matrix、roadmap、topic-local docs、长期 `doc-rvv` pending 边界和 Handoff。

## Evidence Doctor 和 registry 规则

- QEMU 只证明 build / path / log-shape（构建 / 路径 / 日志形状），不写性能结论。
- board repeated（板卡重复测试）才支撑性能 bucket。
- Doctor Error 必须修正或降级；Warning 必须解释，并保留 min / median / max。
- 证据登记到 `test-rvv/registration/transformation_estimation_svd_scale/log/evidence_registry.json`；最终用 `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 检查 fresh。

## 板卡复跑预算和决策桶

- 使用既有 `TESVD_SCALE_BOARD_REPEATED_RUNS`、iterations 和 warm-up 设置。
- `positive`：planned case median B/A 大于 1，且退化频率不改变结论桶。
- `weak_positive`：median 只略大于 1 或 warning 显著，不能作为 clean adoption 依据。
- `negative`：median 小于 1 或多数 run 退化。
- `unstable`：预算内跨桶摇摆，降级或交给用户 / reviewer 判断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public custom layout double sampling（真实公开入口自定义布局双精度取样） |
| A/B boundary | public overload；Std 为 public double scalar fallback，RVV 为当前 production patch 下的 layout-gated double RVV path。 |
| 当前决策问题 | RVV-vs-scalar：更多 custom layout sample 的 public double RVV path 是否快于 public double scalar fallback。 |
| 是否可外推到 production | 只能外推到本阶段列出的三组 layout sample；不能外推到全部自定义 layout。 |
| comparison-boundary / baseline mismatch 风险 | 低；planned summary 使用同一 public case-filter。但该证据不是 RVV-family-selection。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段本身是 bounded production-public scout；若某组 layout weak / negative / unstable，只能按该 layout 降级。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段不选择新 RVV family；若要 clean-adopt 更广 custom layout double gate，仍需要用户明确确认，并写清代表性证据边界。 |

## 完成条件

- Std/RVV correctness 通过。
- QEMU smoke + Evidence Doctor 无 Error。
- board repeated 完成，或记录真实 blocker。
- result / matrix / roadmap / Handoff 明确 Phase 069 和 Phase 070 仍未采纳。

## 继续 / 停止条件

- 若全部 planned case positive：Phase 071 停在 `positive_pending_user_confirmation` 或作为 Phase 070 的 sampling reinforcement（取样增强）继续等待用户确认；不能自动采纳。
- 若出现 weak / negative / unstable：按 layout / row source 降级，并评估是否还有值得做的 custom layout 或 sorted-copy double 路线。
- 若没有新的未阻塞方向，才可把下一步写成等待用户确认或 reviewer 判断。
