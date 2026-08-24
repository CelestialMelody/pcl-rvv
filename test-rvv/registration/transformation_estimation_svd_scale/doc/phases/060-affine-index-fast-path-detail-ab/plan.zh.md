# Phase 060 计划：affine index fast-path detail A/B

## 阶段意图和边界

本阶段从 Phase 045 的 row-source locality / order profile（局部性 / 顺序剖析）继续推进：当前 production row-source RVV path 对 source-indexed、dual-indexed 和 correspondence 都采用 indexed gather（索引离散加载）。当 indices / correspondences 实际是 contiguous affine order（连续等差顺序）时，理论上可以避免 gather，改用 ordered / strided load（顺序 / 跨步加载）形态。

本阶段只做 production-detail diagnostic（生产细节诊断）A/B，不修改 production 源码，不扩大 public API，不把结果直接写成 adopted production behavior。若本阶段证据为 positive，后续才进入 bounded production probe（有界生产探针）计划。

validated_scope：

- row source：source-indexed、dual-indexed、correspondence。
- 点型 / Scalar / layout：`PointXYZ -> PointXYZ`、`Scalar=float`、dense xyz AoS。
- index pattern：contiguous offset slice，source / target offset 可以不同，但步长固定为 1。
- size：64K / 256K。
- comparison boundary：current public RVV gather path vs diagnostic contiguous fast-path candidate。

unvalidated_scope：

- stride > 1、reverse、shuffle 或任意非连续 index pattern。
- custom layout / generic point type / `Scalar=double`。
- 非 dense、非法 index / correspondence、小规模输入。
- production dispatch heuristic、fallback 统计和长期 `doc-rvv` 生产行为。

## 当前状态清单

| area | current state |
| --- | --- |
| production row-source path | Phase 043 已采纳 source-indexed、dual-indexed、correspondence gather RVV path。 |
| locality evidence | Phase 045 显示 contiguous / stride / reverse 强正向，shuffle 收益下降；该结果说明 locality 影响性能，但没有验证新的 fast-path family。 |
| rejected mitigation | staged-selected-cloud、target-sorted、dual-indexed source-sorted-copy 已有负向 / 不稳定证据，本阶段不恢复这些路线。 |
| correctness | 当前 QEMU correctness 为 Std/RVV 各 21 tests passed。 |
| matrix gap | row-source mitigation 方向若继续，必须提出新的 candidate family；affine contiguous fast path 是新 family。 |

## 假设与候选族

候选族：`affine-index-fast-path-detail-ab`。

假设：

- source-indexed contiguous indices 可以把 source gather 换成 source ordered offset load，target 仍按 selected target ordered load。
- dual-indexed contiguous source/target indices 可以把两端 gather 都换成 offset ordered load。
- correspondence 中 query/match 连续且同顺序时，可以把 correspondence gather 换成两端 offset ordered load。
- fast path 的检测成本应计入 candidate；本阶段先用诊断 candidate 固定 contiguous 输入，重点确认 load shape 是否有收益。

风险：

- current gather path 在 contiguous 输入上已经足够快，新增 branch 可能只有弱收益。
- 只验证 step=1，不能外推到 stride / reverse。
- reduction order 与 current path 可能不同，正确性以同构 selected-cloud reference 为准。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `affine-index-fast-path-detail-ab` | source-indexed | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS / contiguous indices | diagnostic detail A/B | new correctness guard vs selected-cloud reference | `row-source-affine-index-fast-path-detail-ab` | required, 64K / 256K repeated | RVV detail candidate symbol / bench asm | required | `planned` |
| `affine-index-fast-path-detail-ab` | dual-indexed | 同上 | diagnostic detail A/B | 同上 | 同上 | required, 64K / 256K repeated | 同上 | required | `planned` |
| `affine-index-fast-path-detail-ab` | correspondence | 同上 | diagnostic detail A/B | 同上 | 同上 | required, 64K / 256K repeated | 同上 | required | `planned` |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 诊断 candidate | `include/impl/tesvd_scale_candidates.hpp` | RVV 构建下提供 contiguous offset detail candidate；Std 构建自然不可用或走 current baseline。 |
| A2 correctness guard | `src/test_tesvd_scale.cpp` | 新增 test 覆盖三类 row source 的 contiguous offset 输入，candidate 与 selected-cloud reference 近似一致。 |
| A3 bench case-filter | `src/bench_tesvd_scale.cpp` | 新增 `row-source-affine-index-fast-path-detail-ab`，输出 current / affine-fast-path 对应 label。 |
| A4 Make / scripts / registry | `Makefile`、summary / manifest 脚本 | 新增 QEMU smoke、board repeated、Evidence Doctor 和 registry target，不覆盖历史 phase。 |
| A5 证据执行 | QEMU / board logs | correctness、QEMU smoke、board repeated、Doctor、registry fresh。 |
| A6 文档回填 | result、matrix、roadmap、topic docs | 按 positive / weak / negative / unstable 回填，不提前接 production。 |

## Evidence Doctor 和 registry

- QEMU：只验证日志形状、路径命中、asm attribution（反汇编归属）和 correctness guard；QEMU timing 不作为性能结论。
- Board：使用 repeated summary，B/A = current gather RVV ms / affine fast-path RVV ms；大于 1 表示 fast path 更快。
- Doctor：`Errors` / `Warnings` / `Suggestions` 必须记录；负向或 warning 不自动拒绝 adopted row-source path，只影响本 candidate。
- Registry：新增独立 run label 和 doc-ref，避免覆盖 Phase 045 / 046 / 058。

## 板卡复跑预算和决策桶

- 默认使用现有 `TESVD_SCALE_BOARD_REPEATED_RUNS`。
- 64K / 256K × 3 row source，共 6 个 case。
- `positive`：median B/A >= 1.20 且没有高频退化。
- `weak_positive`：1.05 <= median B/A < 1.20，且实现和 dispatch 足够简单时可考虑 production probe。
- `neutral`：0.95 <= median B/A < 1.05。
- `negative`：median B/A < 0.95 或多数 run < 1。
- `unstable`：median positive 但退化频率高、Doctor error 或 long-tail 影响决策。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail_affine_index_fast_path_diagnostic`。 |
| A/B boundary | 同一 bench binary 内 current public gather RVV path vs diagnostic affine fast-path candidate。 |
| 当前决策问题 | 是否值得为 contiguous row-source 输入设计 bounded production probe。 |
| diagnostic 是否可外推到 production | 不能直接外推；positive 只说明当前诊断边界值得进入 production probe。 |
| comparison-boundary / baseline mismatch 风险 | 有。candidate 需要保证检测成本计入；若 bench candidate 固定已知 contiguous pattern，result 必须说明检测成本是否已计入。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak positive 只在代码小、fallback 简单且退化频率低时允许；negative / unstable 不进入 production。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。本阶段正是 detail A/B；若接 production，还需 public probe 重跑。 |

## 继续 / 停止条件

- 若 board repeated 为 positive 或 strong weak-positive：下一阶段进入 affine contiguous production probe plan。
- 若 neutral / negative / unstable：candidate 记录为 attempted / rejected，不修改 production；继续寻找新的 row-source mitigation family。
- 若板卡不可用或脚本阻塞：记录为 `turn_stop_deferred with stop_condition_hit`，不伪装成完成。
- 若 correctness 或 QEMU smoke 失败：先修复诊断 candidate 或降级 rejected，不进入 board。

## 文档更新清单

- `result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `README.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_svd_scale-evaluation.zh.md`

`doc-rvv` 只有在后续 production probe 被采纳后才新增生产行为；本阶段最多记录为 topic-local diagnostic evidence。

## roadmap 同步动作

- 新增 `affine-index-fast-path-detail-ab` 为 row-source mitigation candidate。
- 保留 staged-selected-cloud、target-sorted、dual-indexed source-sorted-copy 的 rejected 状态，不恢复旧路线。
- 若本阶段负向，roadmap 继续要求新的 row-source mitigation family 或用户定义 double / custom layout 新范围。
