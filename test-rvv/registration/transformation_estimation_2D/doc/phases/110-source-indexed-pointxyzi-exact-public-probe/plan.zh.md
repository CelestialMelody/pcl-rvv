# Phase 110 Plan: source-indexed-pointxyzi-exact-public-probe

## 阶段意图和边界

本阶段从 Phase 106 的负向代表性证据中收窄一个可验证子范围：source-indexed
`PointXYZI -> PointXYZI` exact public probe。目标是判断在不恢复 source-indexed generic
widening 的前提下，是否可以把 Phase 091 exact `PointXYZ -> PointXYZ` production dispatch
扩展到另一个具体点型 `PointXYZI -> PointXYZI`。

本阶段只证明：

- row source：source-indexed-cloud-pair；
- point type：exact `PointXYZI -> PointXYZI`；
- `Scalar=float`；
- dense、finite、valid source indices、size >= 16；
- public source-indexed overload 的 Std/RVV 和同一 production helper 路径。

本阶段不证明：

- source-indexed generic PointXYZ-like widening；
- `PointNormal`、`PointXYZINormal`、mixed source/target pair；
- dual-indexed、correspondence 或 ordered-cloud-pair 新结论；
- `Scalar=double`、RGB/RGBA、自定义 traits 点型；
- 非法 index / correspondence 安全合同。

生产源码若接入只作为 bounded production probe（有界生产探针）。PI5 后必须停在用户决策点；
不自动采纳、不自动提交、不自动回滚。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| 已采纳 source-indexed | Phase 091 exact `PointXYZ -> PointXYZ` 已被用户采纳并保留。 |
| guarded generic 历史 | Phase 103/104 仍是 guarded probe，不写 adopted。 |
| Phase 106 20-run | source-indexed generic representative public variance 为 negative：12 positive、1 weak_positive、3 negative，Doctor `1/27/0`。 |
| PointXYZI 子范围 | Phase 106 中 `PointXYZI->PointXYZI` 4K/64K/256K 为 `3.900x / 3.432x / 3.275x`，`B/A<1=0/20`。 |
| 负向来源 | Phase 106 negative cases 集中在 `PointNormal->PointNormal 256K`、`PointXYZINormal->PointNormal 64K`、`PointXYZINormal->PointXYZINormal 256K`。 |
| 当前 production source | `tryTransformationEstimation2DSourceIndexedCloudPairRVV` 仍是 exact `PointXYZ -> PointXYZ` gate。 |
| Phase 109 | dual-indexed exact 20-run 已完成并保留 4K caveat；不影响本阶段。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段为 `production-public` probe；若补 RVV-vs-scalar board，则只回答 current public RVV path 是否快于 public scalar path。 |
| A/B boundary | 真实 public source-indexed overload；必要时新增专用 `source-indexed-pointxyzi-public` case-filter，避免混入 Phase 106 Normal negative cases。 |
| 当前决策问题 | 是否允许 exact `PointXYZI -> PointXYZI` 作为 source-indexed 窄 production candidate 进入用户决策点。 |
| diagnostic 是否可外推到 production | Phase 106 是 public boundary 证据，但包含历史 guarded generic probe；本阶段必须用当前 bounded production patch 重新跑 correctness、QEMU、asm、board 和 Doctor。 |
| comparison-boundary / baseline mismatch 风险 | 有。Phase 106 full generic summary 不能直接支撑 exact PointXYZI adoption；本阶段使用独立 run label / evidence dir 和 PointXYZI-only filter 降低混淆。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 probe 已是 bounded；若结果仍负向或 unstable，等待用户授权回滚，不扩大 source-indexed gate。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若只是 public scalar vs RVV 生产候选，需要 PI5 用户确认；若要声明 family 最优，另需 direct-vs-materialize family A/B。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| source-indexed PointXYZI exact public probe | source-indexed-cloud-pair | exact `PointXYZI -> PointXYZI`, `Scalar=float`, `RVVXYZAoSFloatLayout` | true public overload | `run_test_compare` plus existing public source-indexed generic correctness/fallback tests | new `source-indexed-pointxyzi-public` QEMU/board filter | 20-run board under independent label | production public source-indexed boundary or PointXYZI-specific asm attribution | QEMU and board Doctor | pending |

## 实现和测试动作

1. 新建 Phase 110 plan / result 目录。
2. 将 production source-indexed RVV gate 从 exact `PointXYZ` 扩成 exact `PointXYZ` 或 exact `PointXYZI`。
3. 新增 PointXYZI-only public bench filter、QEMU manifest / Doctor / registry target 和 board repeated target；
   run label 必须独立，例如 `source_indexed_pointxyzi_public_phase110_repeated`。
4. 运行 correctness：
   `make -C test-rvv/registration/transformation_estimation_2D run_test_compare`。
5. 运行 QEMU smoke、asm 和 Doctor；QEMU timing 不作为性能结论。
6. 板卡可用时运行 20-run board repeated、Evidence Doctor 和 registry。
7. 同步 result、README、matrix、roadmap、evaluation、`doc-rvv` 和 Handoff。

## Evidence Doctor 和 registry 规则

- QEMU target 必须登记 doc-ref 到 Phase 110 plan / result。
- board target 必须使用独立 evidence dir，不能覆盖 Phase 103/104/106。
- Doctor Error 必须先修复或降级证据；Warning 必须解释是否影响 exact PointXYZI candidate。
- `evidence_status` 必须为 fresh 后才能进入用户决策点。

## 板卡复跑预算和决策桶

- run count：20；
- warm-up：沿用 topic board target 默认 `5`；
- iterations：沿用 topic board target 默认 `20`；
- positive：4K/64K/256K median 全部 > 1.05，且每个规模 `B/A<1 <= 1/20`，Doctor 无 Error；
- weak-positive：某一规模 median > 1.0 但 <= 1.05，或 `B/A<1` 为 2-3/20，且无大规模 Error；
- negative：任一规模 median < 1.0，或 `B/A<1 >= 4/20`；
- unstable：Doctor Warning 指向长尾 / group outlier 且结论不能稳定归桶；
- gate-review：Doctor Error 或 correctness / asm / registry 无法闭合。

## 继续 / 停止条件

若 positive / weak-positive：停在 PI5 用户决策点，保留 probe patch，报告生产 diff 和证据；
不自动采纳或提交。若 negative / unstable / gate-review：同样停在用户决策点，不自动回滚。

若本阶段闭合且用户没有授权采纳，下一阶段默认回到 correspondence new bounded candidate 或
PointXYZI family A/B；不能把 PointXYZI exact 结论外推成 source-indexed generic widening。
