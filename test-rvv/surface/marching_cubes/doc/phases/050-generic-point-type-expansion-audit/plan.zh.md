# Phase 050 计划：generic point type expansion validation

## 阶段意图和边界

本阶段原本只审计是否值得把 `performReconstruction()` 的 exact `PointNormal` gate 放宽为 traits-gated 的 `RVVXYZAoSFloatLayout<PointNT>` 路线。当前源码已经把 gate 放宽到 `RVVXYZAoSFloatLayout<PointNT>`，所以本阶段现在改成验证这条 generic（泛型）生产路径是否站得住，以及还需要哪些代表点型 board 证据才能收束。

| question | answer |
| --- | --- |
| validated_scope | `performReconstruction()` / `RVVXYZAoSFloatLayout<PointNT>` generic production / `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` representative public probes / contiguous synthetic grid / RVV active-cell prepass + scalar `createSurface()` |
| unvalidated_scope | repeated generic board、真实 Hoppe/RBF 输入分布、`Scalar=double`、edge interpolation RVV、triangle emission RVV |
| phase_closeout_boundary | 只能关闭 generic point type expansion validation；不能把一轮代表点型 probe 直接外推成 repeated generic closeout。 |
| point_type_expansion_queue | `RVVXYZAoSFloatLayout<PointNT>` 已覆盖 public wrapper 的输入前提与输出语义；四个代表点型已完成 5-run repeated generic board。下一步若继续优化，应转入 active-z tail / table-lookup compression A/B。 |

## 当前状态清单

| area | status | evidence |
| --- | --- | --- |
| production patch | adopted / generic gate now in source | `surface/include/pcl/surface/impl/marching_cubes.hpp` 中 `if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointNT>::value)`。 |
| correctness | done | `make run_test_compare`，Std/RVV 各 6 tests passed，新增 generic PointT 和 fallback 覆盖。 |
| board | done / representative repeated | `log/board/generic_xyz_repeated/summary.md`、`generic_xyzi_repeated/summary.md`、`generic_xyzrgb_repeated/summary.md`、`generic_xyzrgba_repeated/summary.md`，四个代表点型均为 5-run positive。 |
| production-direct board | done / legacy narrow truth remains | `log/board/production_direct_repeated/summary.md`，5-run narrow truth 仍可作为历史对照。 |
| Evidence Doctor | done / clean | four generic repeated doctors 均为 `Errors=0`、`Warnings=0`、`Suggestions=0`；`production_direct_repeated` 旧 truth 为 `Errors=0`、`Warnings=1`。 |
| generic traits audit | done / representative repeated complete | 代码阅读确认 `RVVXYZAoSFloatLayout` 覆盖输入布局；四个代表点型 board repeated 已通过。 |

## 候选族和假设

| candidate family | hypothesis | risk / unknown | decision target |
| --- | --- | --- | --- |
| generic xyz AoS gate | `RVVXYZAoSFloatLayout<PointNT>` 已在 source 中启用 | repeated generic board 已完成；真实 Hoppe/RBF 分布仍未覆盖 | 收束 generic closeout |
| keep exact PointNormal gate | narrow anchor 仍是历史对照和回退锚点 | generic 点型已经能走 RVV，narrow truth 只是历史基线 | 作为对照，不再是当前 gate |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| generic xyz AoS gate | public synthetic reconstruction | `PointNT` / `float` / `RVVXYZAoSFloatLayout` | `performReconstruction()` public wrapper | representative point types correctness | repeated generic board | `PointXYZ=3.873x`, `PointXYZI=3.618x`, `PointXYZRGB=3.601x`, `PointXYZRGBA=3.639x` | `dump_bench_rvv` refreshed after generic bench compile fix | each generic repeated doctor `Errors=0, Warnings=0, Suggestions=0` | adopted / generic production | optional active-z tail A/B |
| keep exact PointNormal gate | historical narrow anchor | `PointNormal` / `float` / exact gate | historical comparison path | fallback correctness already passed | historical 5-run board truth | `production_direct_repeated` history retained | historical asm anchor | historical doctor `Errors=0, Warnings=1, Suggestions=0` | historical / anchor only | compare, not current gate |

## 实现和测试动作

1. 把 current wrapper 的输入前提拆成两层：`getBoundingBox()` / xyz traits 和 `createSurface()` / PointNT 输出语义，写清哪些条件是 `RVVXYZAoSFloatLayout` 已覆盖的，哪些还要靠点型 probe。
2. 板卡继续补四个代表点型 repeated generic board。
3. repeated board 完成后，把 generic gate 收束成 production closeout。
4. 如果后续出现语义或 performance 退化，保留 generic gate 但把失败点标成 deferred with evidence。

## Board 复跑预算和决策桶

| item | value |
| --- | --- |
| target | `mc_prod_xyz_64`、`mc_prod_xyzi_64`、`mc_prod_xyzrgb_64`、`mc_prod_xyzrgba_64` 作为 generic representative probes；narrow `production_direct_repeated` 作为历史 anchor |
| minimum runs | 代表点型 5-run repeated |
| positive | 四个 generic 代表点型已正向 |
| weak-positive | repeated generic board 仍待做 |
| stop / rerun | 5-run repeated 全部 positive 且 doctor 无 finding 后停止本阶段证据扩展 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public` / `production-detail` validation 前置 |
| A/B boundary | `public reconstruction wrapper` |
| 当前决策问题 | `implementation-shape` + `RVV-family-selection` |
| diagnostic 是否可外推到 production | 当前四个代表点型 repeated 都支持 generic gate 的 synthetic public boundary；真实 Hoppe/RBF 分布仍不能外推 |
| comparison-boundary / baseline mismatch 风险 | 有；当前 board 仍是 synthetic grid，generic 代表点型只跑了 1-run probes |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只有在代表点型和现有 generic gate 都没有语义冲突时才允许 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；generic closeout 最好有 repeated board 证据 |

## 继续 / 停止条件

- repeated generic board 已完成，可以转入文档收束。
- 若要继续追求更高性能，创建 active-z tail / table-lookup compression A/B phase。
- 若任一代表点型出现语义不清、fallback 不稳或 board 证据不足，先保留 generic gate 但把未覆盖点型写成 deferred with evidence。
