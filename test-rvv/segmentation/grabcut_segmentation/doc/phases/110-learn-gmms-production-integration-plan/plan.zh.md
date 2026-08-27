# Phase 110 learnGMMs production integration plan

## 阶段意图和边界

本阶段把 Phase 100 的 `learnGMMs()` production-shaped diagnostic（生产形态诊断，尽量复用真实函数局部流程的测试专用诊断）推进到 production integration loop（生产接入闭环）。目标是在 `segmentation/src/grabcut_segmentation.cpp` 的真实 `pcl::segmentation::grabcut::learnGMMs()` free function（自由函数）内接入 RVV component assignment（分量归属选择）路径，并保留 GaussianFitter relearn（高斯拟合器重新训练）为原标量路径。

`validated_scope`：

| dimension | scope |
| --- | --- |
| production entry | `pcl::segmentation::grabcut::learnGMMs(const Image&, const Indices&, const std::vector<SegmentationValue>&, std::vector<std::size_t>&, GMM&, GMM&)` |
| row source | `Indices` 顺序扫描；`indices[idx]` 指向 `Image<Color>`。 |
| point / scalar / layout | `Image<Color>`，`Color` 为 3 个 float 字段；GMM 为 K=5 float Gaussian；不涉及模板点型 traits。 |
| RVV coverage | Step 4 component assignment；每个 hard segmentation 分组分别批量计算 K 个 Gaussian probability，并更新 `components[idx]`。 |
| scalar retained | Step 5 `GaussianFitter` bucket accumulation 和 `fit()` 参数更新保持标量。 |
| benchmark size | PI4 默认 `96x72`、`iterations=8`、`warmup=2`，与 Phase 100 同边界；必要时增加 public `extract()` `96x72` smoke。 |
| target hardware | Milkv-Jupiter board。 |

`unvalidated_scope`：

| dimension | not covered |
| --- | --- |
| public API | 不改 public API，不改变 `GrabCut<PointT>` 模板接口。 |
| other production paths | 不接入 n-link edge mutation、max-flow solver、color staging、non-organized KNN。 |
| generic point type | 本函数输入已经是 `Image<Color>`，不外推到其它点型布局；`PointXYZRGB` public 证据只证明当前 public construction shape。 |
| scalar type | 不覆盖 `Scalar=double`。 |
| RVV family selection | 本阶段新增的是第二个生产 RVV family；public Std/RVV positive 不能单独证明 family 间取舍，必要时以 production detail case 分别归属。 |

`phase_closeout_boundary`：本阶段只能关闭 `learnGMMs()` production direct 的窄范围接入证据。即使接入后板卡收益为正，也不关闭 n-link、max-flow、color staging、non-organized KNN 或泛型点型扩展。

## 当前状态清单

| item | current state |
| --- | --- |
| adopted production behavior | `GrabCut<PointT>::initGraphTerminalWeightsRVV()` 已由 Phase 060/070 证据和用户确认采纳。 |
| Phase 080 profile | public-shaped profile 显示 `learn_gmms` Std median `191.261680 ms`，占 `11.462780%`。 |
| Phase 090 diagnostic | assignment-only board median B/A `3.528303x`，Doctor `0/0/0`。 |
| Phase 100 diagnostic | full `learnGMMs()` production-shaped diagnostic board median B/A `3.048585x`，Std median `4.078463 ms`，RVV median `1.337755 ms`，checksum 一致，Doctor `0/0/0`。 |
| current missing evidence | 真实 production dispatch、fallback、production symbol asm attribution（生产符号反汇编归属）、接入后 repeated board 和 PI5 EvidenceDecision。 |
| evidence registry | `make evidence_status` 在上一阶段为 `fresh`；本阶段新增证据后需刷新 registry 和文档引用。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown | decision for this phase |
| --- | --- | --- | --- |
| `learnGMMs` RVV assignment + scalar relearn | Phase 100 已证明 full local shape 中 assignment RVV 收益不会被 scalar relearn 稀释。 | production 中 `GMM` / `Image` 对象、`indices` 间接读取和已采纳 `initGraph()` helper 叠加后，public wall-time 收益仍需重测。 | adopt as bounded production patch if PI2-PI5 evidence is positive. |
| GaussianFitter RVV accumulation | 可能进一步降低 Step 5 成本。 | 需要按 component bucket 做分散累加，数值顺序和空组件语义风险更高；Phase 100 当前已足够正向。 | defer；只有接入后 profile 显示 relearn 成本主导再开后续 phase。 |
| LMUL / ILP variants | m2 与当前 diagnostic helper 匹配，代码简单。 | m4 或额外 unroll 可能增加寄存器压力和 spill；需 RVV-vs-RVV detail A/B。 | defer；本阶段优先同 Phase 100 形状接入，降低变量。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production `learnGMMs` RVV assignment + scalar relearn | ordered `Indices` over `Image<Color>` | float `Color`, K=5 GMM | `learnGMMs()` free function | add production direct gtest comparing full function output; small input fallback test | add `production_learn_gmms` case, plus optional public profile rerun | 5-run board repeated with same `96x72` budget | `learnGMMsRVV` / production object symbol contains RVV load/store/FMA/exp helper path | repeated manifest + Doctor, registry recorded | planned |
| GaussianFitter RVV accumulation | same | same | Step 5 only | not implemented | not implemented | none | none | none | deferred |

## 实现和测试动作

| action | dependency | artifact / command | completion criterion |
| --- | --- | --- | --- |
| PI2 production patch | this plan | `segmentation/src/grabcut_segmentation.cpp` | 原标量主体抽为 `learnGMMsStd()`；`__RVV10__` 下新增 `learnGMMsRVV()`；public function 只做短路 dispatch 后 fallback。 |
| production direct correctness | PI2 | `test-rvv/segmentation/grabcut_segmentation/src/test_grabcut.cpp` + `make run_test_compare` | Std/RVV 构建均通过；RVV 构建覆盖真实 `learnGMMs()` 输出 components 和 GMM 参数。 |
| fallback coverage | PI2 | small input test and non-RVV build | 小规模输入在 RVV helper 中返回 false 或自然走 Std；Std 构建不含 RVV symbol 依赖。 |
| bench case | PI2 | `src/bench_grabcut.cpp`、Makefile、manifest script | 新增 `production_learn_gmms` case，计时真实 production function，不再调用 test-only candidate。 |
| QEMU smoke | bench case | narrow `run_bench_std` / `run_bench_rvv` | checksum 一致；只记录为 log-shape，不写性能结论。 |
| asm attribution | PI2 | `make dump_bench_rvv` 或等价 objdump | 证明 production `learnGMMs` RVV helper 或内联区域含 RVV 指令。 |
| board repeated | QEMU smoke | `collect_production_learn_gmms_repeated_board` | 5-run B/A bucket 稳定；若 positive，进入 PI5 pause。 |
| Evidence Doctor / registry | board repeated | repeated manifest + Doctor + `make evidence_status` | Doctor `0/0/0` 或异常已解释；summary artifact 已登记。 |
| docs / Handoff | evidence | phase result、matrix、roadmap、evaluation、doc-rvv、handoff | 用接入后的生产证据刷新结论，不再沿用 Phase 100 数字作为生产收益。 |

## Evidence Doctor 和 registry 规则

新增 case label 必须进入 `script/generate_grabcut_board_evidence_manifest.py` 的 `CASE_LABELS`。新增 repeated target 写入 `Makefile`，并通过 `record_production_learn_gmms_repeated_evidence_files` 登记：

| field | value |
| --- | --- |
| manifest | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json` |
| doctor | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.md/.json` |
| registry | `log/evidence_registry.json` |
| expected Doctor | `Errors=0, Warnings=0, Suggestions=0` |
| anomaly handling | Error 先修复或降级，不关闭阶段；Warning 必须解释；Suggestion 写入 roadmap 或 result。 |

## 板卡复跑预算和决策桶

| item | value |
| --- | --- |
| run count | 5 repeated runs |
| warmup / iterations | `--warmup 2 --iterations 8` |
| default size | `--width 96 --height 72` |
| positive | median B/A >= `1.05x` 且 5 次方向一致或仅小幅波动，checksum 一致，Doctor 无 Error。 |
| weak-positive | `1.00x < median B/A < 1.05x`；只保留为 experiment path，除非 public wall-time 也正向且维护成本极低。 |
| neutral / negative | median B/A <= `1.00x` 或多数 run 退化；不采纳，除非用户要求保留实验 patch。 |
| unstable | B/A 跨正负方向且预算用完；降级证据并在 PI5 暂停。 |

板卡当前由用户确认可用。运行 board target 时使用：

```text
SSH_OPTS='-F /home/zoomin/.ssh/config -i /home/zoomin/.ssh/id_milkv_jupyter -o IdentitiesOnly=yes'
```

该路径只作为本地执行参数记录在 phase / Handoff，默认不进入可提交长期文档正文。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 100 是 production-shaped diagnostic；Phase 110 目标证据是 production-detail，再补 production-public。 |
| A/B boundary | Phase 100 baseline/candidate 是 test-support helper；Phase 110 baseline/candidate 是真实 `learnGMMs()` free function。 |
| 当前决策问题 | RVV-vs-scalar：接入后的真实生产路径是否快于同入口标量路径；并确认不破坏已采纳 `initGraph()` production path。 |
| diagnostic 是否可外推到 production | partial。算法顺序一致，但 production object、linkage、dispatch 和 public `extract()` loop 仍需真实证据。 |
| comparison-boundary / baseline mismatch 风险 | yes。Phase 100 不含 production symbol、非 RVV构建 fallback 和 public `refineOnce()` 叠加成本。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已由用户授权进入 probe；若接入后结果弱 / 负 / 中性 / 不稳定，PI5 暂停并报告，不自行回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 对本阶段的 Std/RVV 接入决策，production-detail 和 production-public positive 可作为采纳依据；若后续比较 LMUL / ILP / accumulation 新 family，需要同边界 RVV-vs-RVV A/B。 |

## 继续 / 停止条件

`continue_stop_decision` 默认继续 PI2-PI5。只有以下情况停止：

- production patch 需要扩大到 public API、模板点型 traits、公共 RVV helper 或其它 topic。
- `learnGMMs()` 真实生产接入无法保持原标量 fallback 边界。
- QEMU correctness、fallback test、asm、board evidence 或 Evidence Doctor 出现未解决 Error。
- dirty isolation 显示本阶段会覆盖用户无关修改。
- PI5 完成后，无论 positive 或 negative，都暂停等待用户确认采纳或回滚。

`next_phase_default`：若 PI5 positive 且用户确认采纳，进入 production closeout；若 PI5 negative 或 unstable，等待用户授权回滚或保留实验 patch；若 board / tool 阻塞，保留 patch 并记录解除阻塞命令。

## 文档更新清单

- `result.zh.md`：回填 PI2-PI5 执行事实、证据路径、Evidence Doctor 和 PI5 decision。
- `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`：更新恢复队列和候选状态。
- `doc/grabcut_segmentation-evaluation.zh.md`：把 `learnGMMs()` 从 production-shaped diagnostic 更新为 production direct 证据边界。
- `doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md`：仅在 production patch 存在且 PI5 生产证据闭合后刷新为接入后数据；PI5 前不写 clean adopted。
- `current-handoff.*`：记录 loaded instruction sources、preferences、dirty isolation、PI5 状态和下一动作。

## 写文件前门禁自查

| gate | status | evidence / decision |
| --- | --- | --- |
| preferences_loaded | pass | defaults loaded；local override absent；prompt override 授权继续，板卡可用。 |
| phase_plan_written_before_edits | pass | 本文件先于 Phase 110 production / test / bench 修改创建。 |
| pi1_production_scope_ready | pass | 只接 `learnGMMs()` free function 的 Step 4。 |
| fallback_dispatch_strategy_ready | pass | 非 RVV 构建自然走 `learnGMMsStd()`；RVV helper 小输入返回 false 后 fallback。 |
| production_direct_test_plan_ready | pass | 真实 `learnGMMs()` gtest + `production_learn_gmms` bench。 |
| board_availability_continue_ready | pass | 用户确认板卡可用；计划含 5-run repeated + Doctor。 |
| evidence_role_and_ab_boundary_ready | pass | 上方 mismatch audit 已区分 diagnostic 与 production-detail / production-public。 |
| production_public_vs_family_selection_ready | pass | 本阶段不把 public Std/RVV positive 写成 RVV family selection 结论。 |
| micro_stop_guard | pass | PI1 完成后继续 PI2-PI5，PI5 后暂停。 |
