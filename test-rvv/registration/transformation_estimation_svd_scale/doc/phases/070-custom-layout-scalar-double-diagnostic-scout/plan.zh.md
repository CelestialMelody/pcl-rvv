# Phase 070 Plan: custom layout `Scalar=double` diagnostic scout

## 阶段意图和边界

本阶段在 Phase 069 仍为 `positive_pending_user_confirmation` 的前提下，单独侦察 custom registered xyz AoS layout（测试本地注册 xyz 结构数组布局）与 `Scalar=double` 的 production public path（真实公开入口路径）是否可以命中现有 D64 RVV 累加 helper。用户未确认 Phase 069 前，本阶段不把 Phase 069 写成 adopted，也不做 adoption closeout（采纳收尾）。

验证范围：

- ordered-cloud-pair、source-indexed、dual-indexed 和 correspondence public overload。
- `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget`。
- `Scalar=double`、dense、8192 correctness / 4096 row-source correctness，后续 bench 用 64K。
- 只证明这个 custom layout sample，不外推到全部自定义点型、packed unaligned float、异常 alignment 全集或 sorted-copy double。

不覆盖：

- Phase 069 adoption。
- sorted-copy double 或新的 RVV family selection（实现族选择）。
- 非 dense、小规模、非法 index / correspondence 之外的新语义。

## 当前状态清单

- Phase 069 result：`test-rvv/registration/transformation_estimation_svd_scale/doc/phases/069-row-source-generic-scalar-double-production-probe/result.zh.md`，当前 `positive_pending_user_confirmation`。
- Phase 069 board Doctor：`test-rvv/registration/transformation_estimation_svd_scale/log/board/row_source_generic_scalar_double_production_probe_repeated/evidence_doctor.md`，`Errors=0`、`Warnings=4`、`Suggestions=0`。
- production D64 helpers 已经模板化使用 `RVVXYZAoSFloatLayout` offsets，但 double dispatch 仍有 common PCL point whitelist。
- custom layout float 采样已在 Phase 055/057/059 完成，不能自动外推到 `Scalar=double`。

## 假设与候选族

候选族：`custom-layout-scalar-double-diagnostic-scout`。

假设：D64 RVV 累加已经按 `PointSource` / `PointTarget` 的 layout offset 和 stride 加载 xyz；如果去掉 common PCL whitelist，custom registered xyz AoS layout 在 double ordered 和 row-source public overload 下应与 double reference 对齐，并真实命中 RVV detail dispatch（生产细节分流）。

风险：

- whitelist 放宽会扩大 production public dispatch，需要 correctness、fallback 和 board 证据后才能成为候选。
- custom layout 的 stride / padding 会改变内存成本；性能只能来自 board repeated。
- 本阶段 positive 也只是 bounded candidate，不自动进入 adopted。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | test | bench / board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `custom-layout-scalar-double-diagnostic-scout` | ordered-cloud-pair | `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` / `double` / custom xyz AoS | 新增 correctness + RVV detail-hit | 64K board repeated | QEMU / board Doctor | pending |
| `custom-layout-scalar-double-diagnostic-scout` | source-indexed / dual-indexed / correspondence | 同上 | 新增 row-source correctness + RVV detail-hit + fallback | 64K board repeated | QEMU / board Doctor | pending |

## 实现和测试动作

1. 先新增 failing correctness tests（预期失败测试）：custom layout double ordered 和 row-source public path 与 `estimateScaleStdDouble` 对齐，并在 RVV 构建下 `detail::*D64RVV` 返回 true。
2. 最小放宽 production double dispatch gate：由 common PCL whitelist 改为 `Scalar=double && SrcLayout::value && TgtLayout::value`，保留 dense、size、合法 index / correspondence 和 gather byte-offset fallback。
3. 补 bench case-filter 和 QEMU / board evidence target：`custom-layout-scalar-double-diagnostic-scout`。
4. 运行 correctness、QEMU smoke + Evidence Doctor、board repeated + Evidence Doctor、evidence registry freshness。
5. 更新 result、optimization matrix、roadmap、topic-local docs 和 Handoff。Phase 069 状态仍保持 pending。

## Evidence Doctor 和 registry 规则

- QEMU 只证明 correctness、RVV path 和 log shape，不写性能结论。
- Board repeated 才能支撑性能 bucket。
- 任何 Doctor Error 必须修正或降级；Warning 必须保留 min / median / max 和解释。
- 证据写入 `log/evidence_registry.json`，并用 `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` 检查 fresh。

## 板卡复跑预算和决策桶

- 计划使用既有 `TESVD_SCALE_BOARD_REPEATED_RUNS` 和 warm-up 设置。
- `positive`：每个 planned case median B/A 明显大于 1，且退化频率不改变 decision bucket。
- `weak_positive`：median 仅略大于 1 或 warning 显著，不能作为 adoption 依据。
- `negative`：median 小于 1 或多数 run 退化。
- `unstable`：同边界 run 跨桶摇摆，预算耗尽后降级或交给人工判断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public candidate scout（真实公开入口候选侦察） |
| A/B boundary | public overload；detail-hit 只检查 RVV dispatch |
| 当前决策问题 | RVV-vs-scalar for custom layout double |
| 是否可外推到 production | 只可外推到本阶段列出的 custom layout sample；不能外推到全部自定义 layout |
| comparison-boundary / baseline mismatch 风险 | correctness 用 public fallback/reference；board 用 Std/RVV public binary，风险可控但不是 RVV-family-selection |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已是 bounded production-public scout；若 weak/negative/unstable，不进入 adoption，保留为 attempted |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段不是新 family selection；若后续要把 custom layout double clean-adopt，仍需用户确认和 closeout |

## 完成条件

- correctness Std/RVV 通过。
- RVV 构建下 custom layout double ordered 和三类 row-source detail dispatch 命中。
- QEMU smoke + Evidence Doctor 无 Error。
- board repeated 完成或给出真实 blocker。
- result / matrix / roadmap / Handoff 明确 Phase 069 仍未采纳。

## 继续 / 停止条件

默认下一步：

- 若 Phase 070 positive：停在 `positive_pending_user_confirmation` 或继续按用户确认进入 adoption closeout；不得自动采纳。
- 若 weak / negative / unstable：保留 attempted / blocked，并评估 sorted-copy double 或其它 roadmap 候选。
- 若 tests 暴露 gate 或 numerical blocker：先修 correctness；无法修复时写 blocked Handoff。
