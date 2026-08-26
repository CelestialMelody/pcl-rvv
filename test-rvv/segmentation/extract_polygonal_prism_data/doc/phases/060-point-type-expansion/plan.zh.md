# Phase 060 Plan: point-type-expansion

## 阶段意图和边界

本阶段验证 `segmentRvv` 的 `RVVXYZAoSFloatLayout<PointT>` 泛型 gate（点类型布局准入）是否可以从当前 `PointXYZ` 生产性能证据扩展到常见 PointXYZ-like AoS float 点型。范围限定在真实 public entry（公开入口）`ExtractPolygonalPrismData<PointT>::segment(PointIndices&)`，不修改 public API，不改 `projectPoints`，不扩大到非 float xyz layout 或 `Scalar=double`。

计划覆盖代表性点型：`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZINormal`。本函数只输出 `indices`，不构造完整 `PointT` 输出；因此额外字段不参与 RVV 计算，但每个点型仍需要独立 compile / correctness / board evidence（板卡证据）。

## 当前状态清单

| area | 当前状态 | 证据 |
| --- | --- | --- |
| `PointXYZ` single / nested production | adopted | Phase 045 / 050 result；single dense / indexed median 1.75x；nested dense 2.18x、nested indexed 2.13x |
| `PointXYZI` correctness | partially covered | `SegmentRvvMatchesSegmentStdForPointXYZI` 仅覆盖 single dense correctness |
| 更多 PointXYZ-like 点型 | not_yet_covered | roadmap 和 matrix 中仍是 `phase_deferred + unblocked` |
| board availability | available | 当前会话确认板卡可用 |

## 假设与候选族

`RVVXYZAoSFloatLayout<PointT>` 已通过 PCL traits（点类型字段特征）证明 `x/y/z` 是单个 float 字段，且 POD / standard-layout / stride / offset 满足 AoS RVV load 前提。因为 `segment` 输出只保留源 index，`PointXYZI`、RGB/RGBA 和 normal 复合点型的额外字段不会被写回或聚合，理论上可以共享同一 RVV 扫描段。

主要风险是 `sizeof(PointT)` 和字段布局改变会影响 AoS stride load、cache locality（缓存局部性）和 indexed gather 成本，性能不能从 `PointXYZ` 外推。

## 优化矩阵

| candidate family | row source | point type / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| point type expansion | dense ordered | `PointXYZI` / `RVVXYZAoSFloatLayout` | production direct vs `segmentStd` | `--path production --point-type xyzi` repeated board | `segmentRvv` contains stride load / compress | pointtype manifest | pending |
| point type expansion | dense ordered | `PointXYZRGB` / `RVVXYZAoSFloatLayout` | production direct vs `segmentStd` | `--path production --point-type xyzrgb` repeated board | `segmentRvv` contains stride load / compress | pointtype manifest | pending |
| point type expansion | dense ordered | `PointXYZRGBA` / `RVVXYZAoSFloatLayout` | production direct vs `segmentStd` | `--path production --point-type xyzrgba` repeated board | `segmentRvv` contains stride load / compress | pointtype manifest | pending |
| point type expansion | dense ordered | `PointXYZINormal` / `RVVXYZAoSFloatLayout` | production direct vs `segmentStd` | `--path production --point-type xyzinormal` repeated board | `segmentRvv` contains stride load / compress | pointtype manifest | pending |

本阶段先闭合 dense ordered row source。若 dense 点型全部 positive，再决定是否同轮扩到 indexed row source；若 dense 里出现 neutral / negative，则 indexed 点型扩展暂缓，避免把 gather 成本和点型 stride 叠加后直接扩大 production 结论。

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| production direct correctness | `src/test_eppd.cpp` | 代表性点型的 `segmentRvv` 输出等于 `segmentStd`；至少覆盖 single polygon，必要时补 nested smoke |
| bench point type 参数 | `src/bench_eppd.cpp` | 支持 `--point-type xyz|xyzi|xyzrgb|xyzrgba|xyzinormal`；非 `PointXYZ` 仅走 production mode |
| board targets | `Makefile` | 新增 pointtype repeated board target，生成 summary / manifest / doctor |
| local correctness / asm | `make run_test_compare`、`make dump_bench_rvv` | Std/RVV correctness pass；production `segmentRvv` 反汇编仍含预期 RVV 指令 |
| board evidence | pointtype repeated targets | 每个点型有 5-run summary 和 Evidence Doctor |

## Evidence Doctor 和 registry

新增 run label 使用 `eppd-full-scan-production-pointtype-<type>-5run`。完成 board repeated 后更新 `log/evidence_registry.json`，并把 summary / manifest / doctor 加入 Phase 060 result、evaluation 和正式 `doc-rvv` 的证据引用。

## 板卡复跑预算和决策桶

每个点型 dense production 5 runs，iterations=8，warmup=2。判断口径：

- `positive`：median >= 1.20x 且 min > 1.05x；
- `weak-positive`：median >= 1.05x 且 min > 1.00x；
- `neutral`：median 在 0.95x 到 1.05x；
- `negative`：median < 0.95x 或 min 明显低于 1.00x；
- `unstable`：5-run 内正负方向摇摆，且 Evidence Doctor 或 raw log 显示长尾无法解释。

positive 或 weak-positive 可把对应点型纳入 adopted production behavior；neutral / negative 不回滚已采用 `PointXYZ` 路径，只把该点型性能结论保持未采用或 fallback 解释。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标证据是 `production-public` |
| A/B boundary | public overload：Std / RVV build 都调用真实 `segment` |
| 当前决策问题 | 代表性 PointXYZ-like 点型走 RVV 是否快于当前 public scalar path |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；以 production public board 为准 |
| comparison-boundary / baseline mismatch 风险 | bench 必须使用 `--path production --point-type <type>`，避免 `PointXYZ` summary 外推 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；本阶段就是有界 production probe，失败只收窄点型结论 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前决策不是替换已有同点型 RVV family |

## 文档更新清单

若点型扩展采纳，刷新 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`、README、evaluation、roadmap、matrix、Phase 060 result、evidence registry 和 Handoff。若某些点型收益不足，长期文档只列已证明点型，未通过点型写入 roadmap 的暂缓 / 拒绝路线。
