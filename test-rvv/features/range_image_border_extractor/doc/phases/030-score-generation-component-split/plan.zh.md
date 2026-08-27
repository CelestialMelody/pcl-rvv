# Phase 030: score-generation component split

## 阶段意图和边界

本阶段仍不修改 production。目标是把 Phase 020 的真实 `RangeImage` score generation 边界继续拆开：分别计时 `extractLocalSurfaceStructure()` 和 local-surface 已经存在时的 `extractBorderScoreImages()`，判断 `getNeighborDistanceChangeScore()` 是否仍是值得 RVV 化的主成本。

validated_scope：160x120 全有限 row-major `RangeImage` fixture，`PointWithRange`，`Scalar=float`，`max_no_of_threads=1`，`pixel_radius_borders=3`，`pixel_radius_plane_extraction=2`。

unvalidated_scope：inf / max range / unobserved 分支、完整 `classifyBorders()`、shadow/veil 状态机、真实 PCD 输入、其它尺寸、并行 OpenMP 配置和 production dispatch。

## 当前状态清单

| evidence | state |
| --- | --- |
| single score-update | positive，Phase 000 median `2.560x` |
| four-score-image update | positive，Phase 010 median `2.210x` |
| RangeImage generation + update | neutral，Phase 020 median `1.000x` |
| production source | unchanged |

## 假设与候选族

| candidate family | hypothesis | required evidence |
| --- | --- | --- |
| local surface dominates | `extractLocalSurfaceStructure()` 里的 PCA / sort / neighbor scan 可能支配 Phase 020 | local-surface-only board timing |
| border-score after-surface candidate | local surfaces 已生成后，四方向 `getNeighborDistanceChangeScore()` 仍可能有可见成本 | after-surface score image timing |
| finite RangeImage neighbor-score RVV | 若 after-surface 成本可见，后续可对全有限连续 `PointWithRange` 做 RVV 公式候选 | correctness、asm、board repeated、Doctor |

## 动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| component bench cases | 在 `bench_range_image_border_extractor.cpp` 新增 `range_image_local_surface_160x120`、`range_image_border_scores_after_surface_160x120` | QEMU smoke 输出 checksum，board repeated 可解析 |
| QEMU smoke | `make run_bench_compare ALLOW_QEMU_BENCH_COMPARE=1 ...` | 只验证输出形状和 checksum，不写性能结论 |
| board repeated + Doctor | 5-run bounded budget | summary、manifest、Doctor 记录 decision bucket |
| EvidenceDecision | result / matrix / roadmap | 判断是否继续 neighbor-score RVV candidate 或暂停 |

## 板卡复跑预算和决策桶

预算：5 runs，`--repeat 1 --iterations 10 --warmup 2`。`median >= 1.10` 且 0/5 低于 1 为 positive；`1.03 <= median < 1.10` 为 weak-positive；`0.97 <= median < 1.03` 为 neutral；`median < 0.97` 为 negative；方向摇摆且不能归入前述桶则 unstable。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / production-shaped helper |
| 当前决策问题 | implementation-shape；判断下一步是否值得做 neighbor-score RVV |
| diagnostic 是否可外推到 production | 只能外推到全有限 RangeImage fixture 的 score generation 子边界 |
| comparison-boundary / baseline mismatch 风险 | medium；未覆盖 inf / shadow / veil 和完整 public output |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许直接 production probe；除非 after-surface 成本显著且 RVV candidate 后续正向 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

若 after-surface score generation 占比很小，或 board repeated 显示 neutral / negative，本阶段暂停并说明不建议继续当前 score-update 方向。若 after-surface 成本可见且局部公式适合 RVV，再创建下一 phase 的 neighbor-score RVV candidate。
