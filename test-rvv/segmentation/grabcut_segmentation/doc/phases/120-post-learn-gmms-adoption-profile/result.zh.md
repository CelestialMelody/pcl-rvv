# Phase 120 post-learnGMMs adoption profile 结果

## 执行范围

本阶段按计划在 `initGraph()` unknown terminal helper 和 `learnGMMs()` component assignment helper 均已采纳后，
重跑 `public_extract_profile` diagnostic-profile（诊断剖析）证据。阶段没有修改 production（生产源码），只用
接入后的 profile 判断是否还有当前 topic 内值得继续推进的新 RVV family（RVV 实现族）。

覆盖范围保持为 organized `PointXYZRGB`，96x72，`Image<Color>`，float GMM，测试专用
`GrabCutBenchAccess::runPublicExtractProfile()`。本阶段不证明泛型点型、`Scalar=double`、
non-organized KNN、n-link production expansion、max-flow solver 或新的 production patch。

## 命令和结果

| action | command / artifact | result |
| --- | --- | --- |
| correctness guard | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` | Std 8/8、RVV 10/10 passed。 |
| QEMU smoke（小型仿真验证） | Std/RVV `run_bench_* BENCH_ARGS='--width 32 --height 24 --iterations 1 --warmup 0 --case public_extract_profile'` | checksum 均为 `13677801866028019573`；只证明可运行和日志形状，不作为性能结论。 |
| board repeated（重复板卡测试） | `collect_post_learn_gmms_profile_repeated_board POST_LEARN_GMMS_PROFILE_REPEATED_DIR=doc/phases/120-post-learn-gmms-adoption-profile/repeated-board-20260827-clean-96x72` | 5-run completed，checksum 均为 `9089139176994405943`。 |
| manifest / Doctor | `make -C test-rvv/segmentation/grabcut_segmentation run_post_learn_gmms_profile_repeated_evidence_doctor POST_LEARN_GMMS_PROFILE_REPEATED_DIR=doc/phases/120-post-learn-gmms-adoption-profile/repeated-board-20260827-clean-96x72` | Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=0`，registry recorded。 |
| registry freshness（登记新鲜度） | `make -C test-rvv/segmentation/grabcut_segmentation evidence_status` | `fresh`。 |

## Board 结果

| run | Std avg ms | RVV avg ms | B/A | checksum |
| --- | ---: | ---: | ---: | --- |
| 01 | 1596.387791 | 1295.410260 | 1.232341 | `9089139176994405943` |
| 02 | 1599.363950 | 1239.190001 | 1.290653 | `9089139176994405943` |
| 03 | 1621.925804 | 1227.206763 | 1.321640 | `9089139176994405943` |
| 04 | 1600.078260 | 1237.415389 | 1.293081 | `9089139176994405943` |
| 05 | 1598.996779 | 1244.950097 | 1.284386 | `9089139176994405943` |

中位数：Std `1599.363950 ms`，RVV `1239.190001 ms`，B/A `1.290653x`。这说明两个已采纳 production
helper 同时存在时，public-shaped profile 仍保持正向；但本阶段的决策问题不是再次采纳已有 helper，而是判断是否继续写新的 helper。

## Component Profile

| component | Std median ms | Std median pct | RVV median ms | RVV median pct | Std/RVV ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| `initgraph_refine` | 1153.119320 | 72.063190 | 976.372140 | 78.791157 | 1.181024 |
| `graph_solve` | 249.449641 | 15.600384 | 176.500220 | 14.243193 | 1.413311 |
| `learn_gmms` | 170.032458 | 10.620452 | 57.534405 | 4.592618 | 2.955318 |
| `initgraph_fit` | 18.305167 | 1.142071 | 18.593278 | 1.497305 | 0.984505 |
| `compute_beta_organized` | 4.220861 | 0.260292 | 4.213458 | 0.340717 | 1.001757 |
| `build_gmms` | 2.238361 | 0.139985 | 2.273750 | 0.182638 | 0.984436 |
| `update_hard_segmentation` | 1.638600 | 0.102453 | 1.569431 | 0.126077 | 1.044073 |
| `compute_nlinks_organized` | 1.470153 | 0.091644 | 1.505236 | 0.120907 | 0.976693 |
| `output_clusters` | 0.208889 | 0.013055 | 0.156514 | 0.012651 | 1.334635 |

`learn_gmms` 在 RVV profile 中的中位占比为 `4.592618%`，低于 Phase 120 计划里的 `profile_actionable`
阈值 `>= 5%`。剩余主要耗时是 `initgraph_refine` 和 `graph_solve`；前者包含图写入和已采纳 terminal helper
以外的构图成本，后者是 max-flow solver（最大流求解器）状态机。二者当前都不是低风险 RVV 生产候选。

## Evidence Doctor 和 Registry

- manifest: `test-rvv/segmentation/grabcut_segmentation/doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-manifest.json`
- Doctor: `test-rvv/segmentation/grabcut_segmentation/doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.md`
- Doctor result: `Errors=0, Warnings=0, Suggestions=0`
- registry: `test-rvv/segmentation/grabcut_segmentation/log/evidence_registry.json`
- registry state: `fresh`

本阶段没有 Evidence Doctor finding 需要降级。manifest 的 metadata 仍记录 `taskset/governor/freq/temperature/binary_hash`
为 `not_recorded`，但脚本规则没有将这些字段判为 suggestion；性能结论仍限定在 Milkv-Jupiter repeated board。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic-profile（诊断剖析） |
| A/B boundary | test-only public-shaped profile wrapper；不是新的 production direct 采纳证据。 |
| 当前决策问题 | implementation-shape：是否还有当前 topic 内值得继续推进的新 RVV family。 |
| diagnostic 是否可外推到 production | no。它只能排序下一候选；任何新 production patch 仍需独立 PI1-PI5。 |
| comparison-boundary / baseline mismatch 风险 | yes。profile 拆分 protected 调用和组件计时边界，不能替代 `public_extract` production-public 5-run。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。当前结果没有指向足够高占比的新公式型组件，不触发新的 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若未来重开 GaussianFitter accumulation 或 LMUL / ILP variants，必须先有同边界 A/B。 |

## Optimization Matrix 更新

| candidate family | decision | reason | next action |
| --- | --- | --- | --- |
| post-adoption public component profile | completed / profile_non_actionable | public-shaped B/A median `1.290653x`，但 `learn_gmms` 剩余 RVV 占比只有 `4.592618%`；主要剩余为 `initgraph_refine` 和 `graph_solve`。 | 停止当前 topic 的自动性能探索。 |
| GaussianFitter accumulation RVV | rejected for current loop / resume by new evidence | profile 未达到 `learn_gmms` 剩余占比阈值；继续做分散累加会引入浮点顺序和 bucket 语义风险。 | 只有未来 profile 显示 `learn_gmms` 或 accumulation 成为公开入口主成本时重开。 |
| `learnGMMsRVV()` LMUL / ILP variants | rejected for current loop / resume by new evidence | 当前 `learnGMMsRVV()` 不是剩余主成本，同边界 RVV-vs-RVV A/B 不太可能改变 public 入口决策。 | 只有 production-detail 或 public profile 显示该 helper 成为主瓶颈时重开。 |
| n-link / color staging / non-organized KNN | deferred / not current high-value | Phase 120 profile 中 `compute_nlinks_organized`、`compute_beta_organized`、color staging 相关组件均低于 1%；non-organized KNN 不在本输入形态内。 | 需要新的输入形态 profile 或用户明确新 scope。 |
| max-flow solver | rejected with evidence | `graph_solve` 仍占约 `14.24%`，但源码是 map/deque/parent/orphan 状态机，不是当前 RVV 直接改写目标。 | 保持标量；如要优化需另开非 RVV / 算法级 topic。 |

## Continue / Stop Decision

`continue_stop_decision`: stop current topic.

`stop_condition_hit`: roadmap 和 optimization matrix 中没有当前授权范围内、未阻塞且高价值的下一段 RVV 优化动作。已采纳的
`initGraph()` unknown terminal helper 和 `learnGMMs()` assignment helper 均保留；Phase 120 只停止继续写新 helper，
不撤销已采纳 production behavior。

`next_phase_default`: none inside current GrabCut RVV topic. 恢复条件是出现新的 public profile、输入形态或用户明确新 scope，
证明 n-link、color staging、GaussianFitter accumulation、non-organized KNN 或其它组件重新成为公开入口主成本，并能建立新的
correctness、asm、board repeated 和 Evidence Doctor 闭环。
