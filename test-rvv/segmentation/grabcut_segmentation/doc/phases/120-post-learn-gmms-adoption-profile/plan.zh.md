# Phase 120 post-learnGMMs adoption profile 计划

## 阶段意图和边界

本阶段在 Phase 110 `learnGMMs()` production patch（生产补丁）已由用户确认采纳后，重跑
`public_extract_profile` component profile（组件剖析）。目标是回答：当前两个已采纳 RVV helper
同时存在时，真实 `setBackgroundPointsIndices()` + `extract()` 形态的剩余耗时是否仍有当前 topic 内
值得继续优化的方向。

本阶段不直接修改 production（生产源码）。如果 profile 显示剩余热点仍在 `learnGMMs()`，后续才考虑
GaussianFitter accumulation（高斯拟合器累加）消融或 `learnGMMsRVV()` LMUL / ILP variants
（向量寄存器分组 / 指令级并行变体）的同边界 RVV-vs-RVV A/B。若剩余热点主要在 max-flow solver
（最大流求解器）、graph mutation（图结构写入）或低占比组件，本阶段停止当前 topic，不继续写新 helper。

## 范围

| 项 | 本阶段覆盖 |
| --- | --- |
| entry | 测试专用 `GrabCutBenchAccess::runPublicExtractProfile()`，形态模拟真实 `setBackgroundPointsIndices()` + `extract()` |
| point type / layout | organized `PointXYZRGB`，96x72，`Image<Color>`，float GMM |
| evidence role | diagnostic-profile（诊断剖析，只用于候选排序，不替代 production-public 性能证据） |
| production state | `initGraphTerminalWeightsRVV()` 和 `learnGMMsRVV()` 均为 adopted production behavior |
| 不覆盖 | 新 production patch、泛型点型、`Scalar=double`、non-organized KNN、n-link production expansion、max-flow RVV 重写 |

`validated_scope`：接入后 `public_extract_profile` 的组件耗时、checksum、repeated board（重复板卡测试）和
Evidence Doctor（证据体检）。

`unvalidated_scope`：GaussianFitter accumulation 的 RVV correctness / 数值预算、LMUL / ILP 变体选择、
更大规模 public wall-time、其它点型和其它 row source policy（行来源策略）。

`phase_closeout_boundary`：本阶段只能关闭“是否值得继续当前 topic 内下一优化 phase”的决策，不能直接采纳
新的 RVV 实现族。

## 当前状态清单

| 来源 | 当前状态 |
| --- | --- |
| Phase 110 result | `learnGMMs()` production-detail median `2.755010x`，接入后 public `extract()` median `1.207907x`，两层 Doctor `0/0/0`，用户已确认采纳。 |
| Phase 080 profile | 接入 `learnGMMs()` 前，`learn_gmms` Std median `191.261680 ms`，占 `11.462780%`；该数据已不能代表接入后的剩余热点。 |
| roadmap / matrix | Phase 120 是当前最高优先级未阻塞动作；GaussianFitter accumulation 和 LMUL / ILP variants 均需先等接入后 profile。 |
| board availability | 用户说明板卡可用；本阶段需要 5-run board repeated、manifest、Doctor 和 registry。 |

## 候选假设

| candidate family | 假设 | 本阶段如何判断 |
| --- | --- | --- |
| GaussianFitter accumulation profile | Step 5 仍为标量，可能成为 `learn_gmms` 剩余成本。 | 若接入后 `learn_gmms` 仍有可见占比，下一 phase 才拆 Step 4 / Step 5 消融和数值预算。 |
| LMUL / ILP variants | 当前 `learnGMMsRVV()` 使用 m2 形状，可能不是最优实现族。 | 若 `learn_gmms` 剩余仍高，下一 phase 做同一 production-detail boundary 的 RVV-vs-RVV A/B。 |
| stop current topic | 已采纳 helper 后，剩余热点可能转向 solver 或 graph mutation。 | 若 profile 显示剩余可向量化组件占比不足，写明停止理由并关闭当前 topic。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / profile target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| post-adoption public component profile | organized image grid | `PointXYZRGB` / float `Color` / 96x72 | test-only public-shaped profile wrapper | `make run_test_compare` 保持通过；profile checksum Std/RVV 一致 | `bench_grabcut --case public_extract_profile` | 5-run Milkv-Jupiter repeated | 无新 RVV 符号；继承已采纳 helper 归属 | planned manifest + Doctor | planned |
| GaussianFitter accumulation RVV | ordered `Indices` over `Image<Color>` | float `Color`, K=5 GMM | future diagnostic only | not implemented | not implemented | not_applicable until profile supports | not_applicable | not_applicable | deferred pending profile |
| LMUL / ILP variants | ordered `Indices` over `Image<Color>` | float `Color`, K=5 GMM | future RVV-vs-RVV A/B | not implemented | not implemented | not_applicable until profile supports | not_applicable | not_applicable | deferred pending profile |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| add Phase 120 evidence variables / target aliases | `test-rvv/segmentation/grabcut_segmentation/Makefile` | Phase 120 profile repeated 不覆盖 Phase 080 summary；`evidence_status` 扫描 Phase 120 manifest / Doctor。 |
| correctness guard | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` | Std/RVV correctness 仍通过。 |
| QEMU smoke | Std/RVV `run_bench_* BENCH_ARGS='--width 32 --height 24 --iterations 1 --warmup 0 --case public_extract_profile'` | checksum 一致；只证明可运行和日志形状，不作为性能结论。 |
| board repeated | `collect_post_learn_gmms_profile_repeated_board` | 5-run `96x72`、`iterations=3`、`warmup=1`；保存到 Phase 120 目录。 |
| manifest / Doctor / registry | `run_post_learn_gmms_profile_repeated_evidence_doctor` | Doctor `Errors=0`，registry recorded，`evidence_status` fresh。 |
| docs refresh | Phase 120 result、roadmap、matrix、evaluation、README、Handoff | 写清 profile 结果、继续 / 停止决策和下一 phase 默认动作。 |

## Evidence Doctor 和 registry 规则

输出路径：

- repeated dir: `doc/phases/120-post-learn-gmms-adoption-profile/repeated-board-20260827-clean-96x72`
- manifest: `doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-manifest.json`
- Doctor: `doc/phases/120-post-learn-gmms-adoption-profile/repeated-evidence-doctor.md/.json`
- registry: `log/evidence_registry.json`

若 checksum 不一致、Std/RVV component 集合或顺序不一致、run count 不足，当前证据降级为 blocked，先修复或重跑。
Doctor Warning 必须在 result 中解释；Suggestion 记录到 roadmap 或 Handoff。

## 板卡复跑预算和决策桶

- 初始预算：1 次 5-run repeated board，`--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract_profile`。
- 追加预算：只有 checksum 一致但 component 占比跨越判断阈值时，最多追加 1 次 5-run。
- `profile_actionable`：某个当前可向量化剩余组件占 public profile 总耗时 >= 5%，且已有明确下一 phase 可验证。
- `profile_non_actionable`：剩余热点主要在 max-flow / graph mutation，或所有可向量化组件占比不足以支撑 production 改动。
- `unstable`：两次预算后 component 排序或 B/A 方向仍摇摆，降级并交给 reviewer / 用户判断。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic-profile |
| A/B boundary | test-only public-shaped profile wrapper；不是新的 production direct 采纳证据。 |
| 当前决策问题 | implementation-shape：是否还有值得当前 topic 继续推进的新 RVV family。 |
| diagnostic 是否可外推到 production | no。它只能决定下一阶段候选排序；任何新 production patch 仍需独立 PI1-PI5。 |
| comparison-boundary / baseline mismatch 风险 | yes。profile 拆分 protected 调用和计时边界，不能替代 `public_extract` production-public 5-run。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。只有 profile 指向明确组件并补同边界诊断 / A/B 后才允许新的 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若后续比较 LMUL / ILP 或 accumulation 新实现族，必须补同边界 A/B。 |

## 文档更新清单

本阶段完成后更新：

- `doc/phases/120-post-learn-gmms-adoption-profile/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/grabcut_segmentation-evaluation.zh.md`
- `README.zh.md`
- `tmp/rvv-work-logs/segmentation/grabcut_segmentation/current-handoff/current-handoff.*`

长期 `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` 只在 profile 改变 adopted production truth、证据状态或后续条件时更新；普通 profile 数字不替代 Phase 110 production-public 证据。

## 继续 / 停止条件

`continue_stop_decision` 默认继续到 board repeated、Doctor、registry 和 result。停止只在以下情况发生：

- board / rsync / toolchain 不可用；
- checksum 或 profile component 集合不一致且无法同轮修复；
- registry 显示无法归属的未登记变更；
- profile 结果为 `profile_non_actionable`，且 roadmap / matrix 中没有当前授权内高价值未阻塞动作。

`next_phase_default`：若 `profile_actionable`，按证据选择 GaussianFitter accumulation profile 或
`learnGMMsRVV()` LMUL / ILP RVV-vs-RVV A/B；若 `profile_non_actionable`，停止当前 topic 并整理 closeout。
