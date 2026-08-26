# Phase 050 Plan: concave-hull-xor

## 阶段意图和边界

本阶段尝试把 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` 中的 concave hull（凹包）多 polygon XOR（异或）扫描从 scalar fallback（标量回退）扩到已采纳的 RVV production path。Phase 045 已采纳 single polygon dense / indexed RVV；本阶段只在同一公开入口、同一 `RVVXYZAoSFloatLayout<PointT>`、同一 `indices_->size() >= 64` 和合法 indices 边界内扩大 `polygons_.size() > 1` 的覆盖。

不修改 public API（公开接口），不改 `isPointIn2DPolygon` / `isXYPointIn2DXYPolygon`，不优化 `projectPoints`，不扩大到新的点类型性能结论。

## 当前状态清单

| area | 当前状态 | 证据 |
| --- | --- | --- |
| single polygon production RVV | adopted | Phase 045 result；production dense / indexed median 1.75x |
| multi polygon behavior | scalar fallback | `SegmentRvvDeclinesConcaveHullPolygons` |
| test-only candidate | 已支持多 polygon XOR | `segmentPolygonalPrismRvvFullScanCandidate` 对多个 polygon 做 XOR |
| board availability | available | 本轮已通过 `make run_board_test` |

## 假设与候选族

Phase 050 候选直接复用 single polygon 的 per-edge mask（逐边掩码）结构：每个 polygon 内部仍按 edge parity 更新 `in_poly`，polygon 之间用 XOR 合并为 `in_any_polygon`。预期好处是让 `test_concave_prism` 类 two-rings 输入也走 RVV 扫描段；主要风险是多 polygon 边数通常较小，额外循环可能让收益低于 single polygon。

## 优化矩阵

| candidate family | row source | point type / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| concave hull XOR production RVV | dense ordered | `RVVXYZAoSFloatLayout<PointT>` / `PointXYZ` | production direct nested polygons vs `segmentStd` | `--path production --polygons nested` repeated board | `segmentRvv` contains existing mask / compress RVV instructions | production nested manifest | pending |
| concave hull XOR production RVV | indexed | `RVVXYZAoSFloatLayout<PointT>` / `PointXYZ` | production direct nested polygons with indexed source | `--path production --indices indexed --polygons nested` repeated board | gather + XOR + compress | production nested indexed manifest | pending |
| single polygon existing path | dense / indexed | current adopted scope | regression via `make run_test_compare` | existing production summary remains current until code changes require rerun | asm attribution | existing doctor | must remain pass |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| RED tests | `src/test_eppd.cpp` | multi polygon `segmentRvv` expected true and equals `segmentStd`；当前代码应失败 |
| production patch | `segmentRvv` | 删除 `polygons_.size() != 1` fallback，构造多个 active polygons，并按 XOR 合并 mask |
| bench extension | `src/bench_eppd.cpp`、`Makefile` | 支持 `--polygons single|nested`；新增 production nested board repeated target |
| correctness | `make run_test_compare`、`make run_board_test` | Std/RVV tests 和板卡 tests pass |
| asm / board / doctor | `make dump_bench_rvv`、nested repeated board targets、Evidence Doctor | dense / indexed nested case 有明确 bucket；doctor 0 或异常已解释 |

## Evidence Doctor 和 registry

本阶段新增 nested production public summary，run label 使用 `eppd-full-scan-production-nested-5run` 和 `eppd-full-scan-production-nested-indexed-5run`。完成 board repeated 后更新 `log/evidence_registry.json`，并把 summary / manifest / doctor 加入 README、evaluation、formal `doc-rvv` 和本 result 的证据引用。

## 板卡复跑预算和决策桶

预算为 dense nested 5 runs、indexed nested 5 runs。判断口径沿用本 topic 现有 repeated summary：

- `positive`：median >= 1.20x 且 min > 1.05x；
- `weak-positive`：median >= 1.05x 且 min > 1.00x；
- `neutral`：median 在 0.95x 到 1.05x；
- `negative`：median < 0.95x 或 min 明显低于 1.00x；
- `unstable`：5-run 内正负方向摇摆，且 Evidence Doctor 或原始日志显示长尾无法解释。

positive 或 weak-positive 可保留当前扩展；neutral / negative 默认回退多 polygon RVV 改动，只保留测试 / bench 资产和负向记录。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标证据是 `production-public` |
| A/B boundary | public overload：Std / RVV build 都调用真实 `segment` |
| 当前决策问题 | 多 polygon XOR 走 RVV 是否快于当前 public scalar path，且 fallback 是否保持语义 |
| diagnostic 是否可外推到 production | test-only candidate 只能提供实现形状；最终以 production public nested board 为准 |
| comparison-boundary / baseline mismatch 风险 | benchmark 必须使用 `--path production --polygons nested`，避免 single polygon summary 外推 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；本阶段本身就是有界 production probe，失败则回退 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策不是替换已有 multi polygon RVV family |

## 文档更新清单

若扩展采纳，刷新 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`、README、evaluation、roadmap、matrix、Phase 050 result、evidence registry 和 Handoff。若扩展回退，则 formal `doc-rvv` 保留 Phase 045 single polygon adopted 状态，并在后续方向中记录 multi polygon 不值得接入。
