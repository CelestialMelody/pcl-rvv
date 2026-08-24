# Phase 069 Plan: row-source generic `Scalar=double` production probe

## 阶段意图和边界

本阶段把 Phase 067 已采纳的 row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` 分支，扩展到 Phase 068 已采纳的 common PCL xyz AoS whitelist（常见 PCL xyz 结构数组白名单）。目标是验证 source-indexed、dual-indexed 和 correspondence 三类公开 row-source overload（公开行来源重载）在 `Scalar=double` 下是否能复用 f64 widened accumulation（把 float xyz 扩宽到 double 后累加）。

本阶段只证明：

- row source：source-indexed、dual-indexed、correspondence。
- 点型 / 布局：common PCL xyz AoS whitelist，且 source / target 都满足 `RVVXYZAoSFloatLayout`。
- `Scalar`：`double`。
- 输入：dense、`nr_points >= 16`、indices / correspondences 已满足现有 RVV gather byte-offset gate。

本阶段不证明：

- custom layout double、任意用户自定义点型全集或异常 padding / alignment 全集。
- sorted-copy double、affine fast path double 或新的 RVV family selection（实现族选择）。
- non-dense、小规模、非法 index / correspondence 语义。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| ordered generic double | Phase 068 已采纳 ordered common PCL xyz AoS whitelist / `Scalar=double`，5 个 board case 全 positive。 | `doc/phases/068-generic-scalar-double-ordered-production-probe/result.zh.md` |
| row-source exact double | Phase 067 已采纳 exact `PointXYZ -> PointXYZ` 的三类 row-source double 分支。 | `doc/phases/067-row-source-scalar-double-production-probe/result.zh.md` |
| production helper | 当前 row-source double helper 仍是 exact `PointXYZ`；float row-source helper 已有 traits-gated generic implementation。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| 测试资产 | 已有 ordered generic double gtest / bench helper，也已有 row-source exact double gtest / bench helper。 | `src/test_tesvd_scale.cpp`、`src/bench_tesvd_scale.cpp` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `row-source-generic-scalar-double-production-probe` | 把 float generic row-source load/gather 组织复用到 double widened accumulation，能在 common PCL xyz AoS 点型上保持 Phase 067 的收益方向。 | gather 成本随 `sizeof(PointT)` 和 source/target 混合点型改变；double f64 lane 数较少；具体点型组合不能外推到任意 custom layout。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board target | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `row-source-generic-scalar-double-production-probe` | source-indexed | `PointXYZI -> PointXYZI` / `double` / common xyz AoS | 新增 public correctness + detail-hit + fallback test | 新增 case-filter `row-source-generic-scalar-double-production-probe` | QEMU + board doctor | planned |
| 同上 | dual-indexed | `PointXYZRGB -> PointXYZRGB` / `double` / common xyz AoS | 同上 | 同上 | 同上 | planned |
| 同上 | correspondence | `PointXYZI -> PointXYZRGB` / `double` / common xyz AoS | 同上 | 同上 | 同上 | planned |

## 实现和测试动作

1. 在 production header 中新增 templated D64 row-source accumulation helper：contiguous-offset、source-indexed、dual-indexed 和 correspondence 形态，复用 `RVVXYZAoSFloatLayout` offsets 和 D64 lane accumulation。
2. 将三类 row-source `Scalar=double` dispatch 从 exact `PointXYZ` 扩到 `kTransformationEstimationSVDScaleCommonDoublePair && SrcLayout::value && TgtLayout::value`，保持 dense、size、gather byte-offset 和 fallback gate 不变。
3. 在 gtest 中新增 row-source generic double correctness、detail-hit 和 fallback boundaries，覆盖 source-indexed、dual-indexed 和 correspondence。
4. 在 bench 中新增 `row-source-generic-scalar-double-production-probe` case-filter，至少覆盖三个代表组合：`PointXYZI->PointXYZI`、`PointXYZRGB->PointXYZRGB`、`PointXYZI->PointXYZRGB`。
5. 在 Makefile 中新增 QEMU smoke、Evidence Doctor、registry 和 board repeated target，doc-ref 指向本 phase result。

## Evidence Doctor 和 registry 规则

QEMU smoke 只用于 correctness/log shape/path evidence（正确性、日志形状和路径证据），不写性能结论。board repeated 采用默认 5-run budget，decision bucket 沿用当前 topic repeated summary 脚本：positive / weak / neutral / negative / unstable 由 summary 和 Evidence Doctor 共同解释。

## 完成条件

- production diff 仅触碰当前 topic 对应生产 header。
- `run_test_compare` Std/RVV correctness 通过，测试数增加后必须在 result 更新。
- QEMU smoke manifest / Evidence Doctor 为 `Errors=0`；Warnings 必须解释边界。
- board repeated 若可用，运行 `run_board_bench_row_source_generic_scalar_double_production_probe_repeated` 并记录 summary / doctor；若不可用，本 phase 停在 `production probe pending board evidence`，不得写 adopted。
- matrix / roadmap / topic docs 同步当前 phase 状态。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public（真实公开入口证据） |
| A/B boundary | public overload，Std/RVV public path 对比；QEMU smoke 只看路径和正确性 |
| 当前决策问题 | RVV-vs-scalar for row-source generic double |
| diagnostic 是否可外推到 production | not_applicable，本阶段直接接真实 public overload |
| comparison-boundary / baseline mismatch 风险 | 有：public Std/RVV positive 只证明新增 public RVV path 快于 public scalar path，不比较 double row-source generic 与其它未实现 RVV family |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已是 bounded production probe；若 board 弱或负，保留 patch 等待用户检查，不自动采纳 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 否；本阶段不是 RVV family selection，没有已有 generic double row-source RVV family |

## 继续 / 停止条件

默认下一步是执行本计划。若本阶段 board positive 且用户确认采纳，后续应另开新的 adoption closeout phase；若 board 不可用或 Evidence Doctor Error 未解，则停止在带恢复命令的 handoff。Phase 070 后来已用于独立 custom layout double scout，不能再作为本阶段 adoption closeout 名称；后续仍可独立推进 broader custom layout double 或 sorted-copy double。
