# Phase 100 full learnGMMs production-shaped diagnostic 结果

## 当前结论

本阶段完成完整 `learnGMMs()` production-shaped diagnostic（生产形态诊断）。测试专用 helper 复刻
production `learnGMMs()` 的两段局部流程：先按 hard segmentation（硬分割状态）选择 foreground /
background GMM，再为每个像素选择概率最大的 K=5 Gaussian component（高斯分量），随后按 component
vector 重新累加 foreground / background GaussianFitter 并更新 GMM 参数。

Milkv-Jupiter 5-run repeated board（重复板卡测试）结果为 positive：median B/A 为 `3.048585x`，
Std median 为 `4.078463 ms`，RVV median 为 `1.337755 ms`，Std/RVV checksum 均为
`2296873087520052250`。Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=0`。

该结果说明 Phase 090 assignment-only（仅分量归属）收益没有被 GaussianFitter relearn（高斯拟合器重新训练）
完全稀释。它足以支持提出有界 production integration loop（生产接入闭环）计划；它仍不是
production direct（真实生产路径证据），不能直接写成 clean adoption（干净采纳）或已采用生产行为。

## 执行范围

| item | result |
| --- | --- |
| phase plan | `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/plan.zh.md` 已在实现和 board evidence 前存在。 |
| source scope | 只修改 test-rvv 测试支撑、bench、manifest target 和 topic 文档；本阶段未修改 production 源码。 |
| validated scope | organized image grid、`PointXYZRGB`-like float `Color`、K=5 GMM、foreground/background hard segmentation mask、96x72。 |
| unvalidated scope | 真实 `GrabCut<PointT>::learnGMMs()` production dispatch、public `extract()` wall time、`Scalar=double`、自定义点型、其它 layout、non-organized KNN、fixed-label trimap 以外的路径。 |

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| add full helper | done | `include/impl/grabcut_diagnostic_reference.hpp` 新增 `FullLearnGMMsResult`、GaussianFit accumulator 和 `learnGMMsReference` / `learnGMMsCandidate`。 | candidate 只替换 assignment 子阶段，relearn 共用标量语义。 |
| add correctness test | done | `src/test_grabcut.cpp` 新增 `GrabCutDiagnosticReference.LearnGMMsFullMatchesScalarReference`；`make run_test_compare` 已通过。 | components、GMM `pi`、`mu`、`determinant` 和 `inverse` 与标量参考一致。 |
| add bench case | done | `src/bench_grabcut.cpp` 新增 `--case learn_gmms_full`。 | 输出 `BENCH grabcut_component`，checksum 覆盖 components 和 GMM 参数。 |
| add manifest metadata | done | `script/generate_grabcut_board_evidence_manifest.py` 新增 `learn_gmms_full` case label。 | manifest 能声明 evidence role、A/B boundary（A/B 边界）、timer boundary（计时边界）和 checksum policy（校验策略）。 |
| QEMU smoke | done | Std/RVV `--width 32 --height 24 --iterations 1 --warmup 0 --case learn_gmms_full`，checksum 均为 `12295835468401059724`。 | QEMU 只证明可运行、路径和日志形状；QEMU timing 不进入性能结论。 |
| board repeated | done | `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-board-20260827-clean-96x72`。 | 5-run bucket 为 `full_learn_gmms_positive`。 |
| docs refresh | done | 本 result、README、optimization matrix、roadmap、evaluation、Handoff。 | 当前默认下一动作停在 production 接入授权边界。 |

## Board evidence

| field | value |
| --- | --- |
| command | `make -C test-rvv/segmentation/grabcut_segmentation collect_learn_gmms_full_repeated_board LEARN_GMMS_FULL_REPEATED_DIR=doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-board-20260827-clean-96x72 SSH_OPTS='-F /home/zoomin/.ssh/config -i /home/zoomin/.ssh/id_milkv_jupyter -o IdentitiesOnly=yes'` |
| bench args | `--width 96 --height 72 --iterations 8 --warmup 2 --case learn_gmms_full` |
| manifest | `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-manifest.json` |
| doctor | `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.md` |
| B/A values | `3.078492, 3.048585, 3.033853, 3.088773, 3.041691` |
| median B/A | `3.048585x` |
| Std median | `4.078463 ms` |
| RVV median | `1.337755 ms` |
| checksum | Std/RVV both `2296873087520052250` |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=0` |

`B/A = Std_ms / RVV_ms`，`>1` 表示 RVV candidate 更快。本阶段使用计划内一次 5-run 预算。所有 run
方向一致，decision bucket 稳定，因此没有触发追加复跑。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test-support helper boundary；baseline 是 scalar assignment + scalar relearn，candidate 是 RVV assignment + scalar relearn。 |
| 当前决策问题 | implementation-shape：完整 `learnGMMs()` 局部形态是否值得进入有界 production integration loop。 |
| 计时边界 | 包含 assignment、component vector 生成、GaussianFitter bucket accumulation（按分量桶累加）和 GMM 参数更新；不包含 `initGraph()`、max-flow 或 public `extract()` wall time。 |
| diagnostic 是否可外推到 production | partial。它覆盖 `learnGMMs()` 的局部算法顺序和 GMM 更新状态，但还没有证明真实 `GrabCut<PointT>::refineOnce()` 调用会命中 RVV dispatch。 |
| comparison-boundary / baseline mismatch 风险 | yes。输入是 synthetic organized image 和 mask；production 中还会叠加 object lifetime、trimap 更新、public wall-time 和当前已采纳 `initGraph()` RVV helper。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。本阶段实际为 positive；若结果落入弱 / 负 / 中性 / 不稳定，本计划会停止，不推进 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。后续若实现生产路径，需要同一 production boundary 下接入后测试、fallback、asm 和板卡 repeated，再在 PI5 等待用户确认。 |

## Evidence Doctor 和 registry

Evidence Doctor 输入为
`doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-manifest.json`，输出为
`doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.md/.json`。报告为
`Errors=0, Warnings=0, Suggestions=0`。

`log/evidence_registry.json` 已登记 Phase 100 summary artifact，target 为
`record_learn_gmms_full_repeated_evidence_state`。提交前仍需运行 `make evidence_status`，因为本 result
新增后会影响文档引用集合。

## 优化矩阵更新

| candidate family | result | decision | next action |
| --- | --- | --- | --- |
| full `learnGMMs` RVV assignment plus scalar relearn | Correctness、QEMU smoke、board repeated 和 Doctor 均闭合到 production-shaped diagnostic 边界。 | completed / positive production-shaped diagnostic | 等待用户明确授权后，进入有界 `learnGMMs()` production integration loop。 |
| GaussianFitter accumulation RVV | 本阶段没有单独向量化 bucket accumulation；当前 full 结果仍为 positive。 | deferred / lower priority | 只有 production direct 后仍显示 relearn 成本主导，才开独立 accumulation profile。 |

## Continue / stop decision

`continue_stop_decision`: `turn_stop_deferred with stop_condition_hit`。

停止条件是继续需要扩大到 production source files（生产源码文件）并进入新的 production integration loop。
本阶段 positive 只授权建议下一步，不授权直接修改 `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp`
或把 `learnGMMs()` 写成已采用生产行为。

`next_phase_default`: 等待用户明确授权后创建 `110-learn-gmms-production-integration-plan`，先冻结入口、
点型、`Scalar`、fallback、production direct tests、asm 和接入后 repeated board 计划；未授权时保持当前
production patch 不变。
