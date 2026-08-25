# 050 tail-vector-filter-probe 计划

## 阶段意图和边界

本阶段尝试一个窄 production probe（生产探针）：继续沿用已采纳的 `apmfExtractRVV`，只改 `thresholdGroundRVV` 内部剩余的 per-lane scalar filter（逐 lane 标量筛选）组织。当前 030/040 已证明 grid z-min + threshold tail 的 production public（真实公开入口）路径正向；050 只判断 tail 阶段里把 `diff < threshold` 和输出保序压缩进一步放入 RVV 是否值得保留。

本阶段不修改 public API（公开接口），不重开 window-open RVV，不扩大 row source，也不改变 `RVVXYZAoSFloatLayout<PointT>` gate。若板卡同边界 A/B 没有收益，必须回退 050 生产改动，保留 040 已采纳状态。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| adopted production | `extract` -> `apmfExtractRVV` -> grid z-min RVV + scalar window-open + threshold tail RVV | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| 040 correctness | Std/RVV 14/14 passed，覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` | `src/test_apmf.cpp` |
| 040 board | 8 个 production public label 5-run 均 positive，Evidence Doctor clean | `log/board/point-type-production-repeated-v1/summary.md` |
| 当前 tail 形态 | RVV gather xyz + row/col 计算后，把 row/col/z/source 写入 staging buffer，再逐 lane 查 `Zf(row,col)`、比较并 `push_back` | production helper |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `tail-vector-filter-compress` | 保留 scalar `Zf(row,col)` lookup（查表）以避免越界 masked gather 风险，但把 `diff` 比较和 source index 压缩改为 RVV，可以减少 per-lane branch / push_back 成本。 | 仍有 staging 和 scalar lookup；收益可能被 extra load/store 抵消。 |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-detail` + `production-public` |
| A/B boundary | 同一 public overload 下的 040 adopted baseline 与 050 probe candidate |
| 当前决策问题 | `RVV-family-selection`，判断新 tail 组织是否优于已采纳实现 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；最终用 production public board A/B |
| comparison-boundary / baseline mismatch 风险 | 中。需要用同一 bench wrapper、同一输入、同一板卡 run budget，并在文档中区分 040 历史 summary 与 050 probe summary。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；若不正向，回退 050 改动。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。050 是已采纳 RVV family 内的实现形态选择，不能只看 Std/RVV positive。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 记录 baseline | `log/board/point-type-production-repeated-v1/summary.md` | 使用 040 5-run 作为 adopted baseline；必要时同轮重跑可刷新。 |
| 实现 tail vector filter probe | production helper | `thresholdGroundRVV` 中比较和保序输出使用 RVV mask + `vcompress`，fallback 和边界不变。 |
| correctness | `run_test_compare` | Std/RVV 各 14/14 passed。 |
| asm attribution | `check_production_rvv_asm` | production bench RVV binary 仍可定位 `apmfExtractRVV` 和关键 RVV 指令。 |
| board A/B | `log/board/tail-vector-filter-probe-v1/` | 至少 3-run；若新形态相对 040 同 case bucket 稳定正向才采纳。 |
| Evidence Doctor | summary / manifest / doctor 或人工 doctor | Errors=0；Warnings 必须解释。 |

## 板卡复跑预算和决策桶

先跑 3-run candidate production public repeated。把 050 candidate 的每个 label speedup 与 040 `point-type-production-repeated-v1` adopted baseline 对比；若 median improvement >= 1.03 且没有 label 退化超过 2%，判为 positive；若多数 label 持平或退化，判为 neutral / negative 并回退 050 改动。若结果接近阈值，最多扩到 5-run。

## 继续 / 停止条件

若 050 相比 040 同边界正向，保留生产改动并刷新 `doc-rvv`、evaluation、matrix、roadmap 和 Handoff。若 correctness、asm 或 Evidence Doctor 失败，修复一次；仍失败则回退 050。若 board A/B 中性或负向，回退 050 并记录为 rejected with evidence。

下一阶段默认入口：取决于 050 结果。若 050 被拒绝且 roadmap 无当前授权内高优先级候选，则进入 closeout-and-next-topic audit；若 050 被采纳，再检查是否还有 worth-trying 的 unblocked micro-candidate。
