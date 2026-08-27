# Phase 010: GMM probability density diagnostic plan

## 阶段意图和边界

本阶段专门覆盖 `segmentation/src/grabcut_segmentation.cpp` 中 `GMM::probabilityDensity(i, c)` 的 per-Gaussian（单个高斯分量）概率密度公式。目标是建立 test-only same-chain（测试专用同构链路）RVV candidate（候选实现），判断这个小公式是否具备继续做 component bench（组件性能测试）和后续 public-shaped diagnostic（公开入口形态诊断）的价值。

本阶段不修改 production（生产源码），不修改 `BoykovKolmogorov::solve` max-flow（最大流）状态机，不改变 GMM build / learn 的 SVD（奇异值分解）和 component assignment（分量分配）流程，也不把 QEMU timing（QEMU 计时）写成性能结论。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| GMM probability | `GMM::probabilityDensity(i, c)` 计算 3 维二次型，再执行 `std::exp(-0.5*d)`。 | `segmentation/src/grabcut_segmentation.cpp` |
| 数学 helper | `pcl::expf_RVV_f32m2` 已存在，但它是 finite-domain fast approximation（有限输入域快速近似），不是完整 `std::expf` 替换。 | `common/include/pcl/common/impl/rvv_math.hpp`、`doc-rvv/rvv/math/expf-RVV.zh.md` |
| Phase 000 | organized n-link reference correctness 已可运行；QEMU bench 只作为 log-shape smoke。 | `test-rvv/segmentation/grabcut_segmentation/doc/phases/000-current-state-and-diagnostic-plan/` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| scalar same-chain GMM probability | 把 production 公式复刻到 topic-local helper 后，可用固定 Gaussian 和 color 样本锁定数值语义。 | 只覆盖 `probabilityDensity(i, c)`，不覆盖 `probabilityDensity(c)` 的 `pi` 加权总和，也不覆盖 `learnGMMs` 分量选择。 |
| RVV GMM probability batch | 对一批 `Color` 样本使用 stride load（跨步加载）和 `expf_RVV_f32m2`，可能减少 per-pixel terminal weight / component assignment 里的公式成本。 | `expf_RVV_f32m2` 与 `std::exp` 不是严格位级等价；需要误差预算。GMM `K=5` 小，真实收益可能被 graph / max-flow 稀释。 |

## 优化矩阵

矩阵主归属：`test-rvv/segmentation/grabcut_segmentation/doc/phases/optimization-matrix.zh.md`。

| candidate family | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| GMM probability scalar/RVV same-chain | test-only batch wrapper for `GMM::probabilityDensity(i, c)` | `make run_test_compare` 新增固定 Gaussian + sample grid | 延续 `bench_grabcut`，后续可加 `case=gmm_probability` | planned if component bench participates in EvidenceDecision | `make dump_bench_rvv` after bench case exists | manual warning until manifest exists | planned |

## 实现和测试动作

| 动作 | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| RED GMM correctness | `src/test_grabcut.cpp` 引用尚未实现的 GMM batch candidate | 编译失败，证明测试会捕获缺失 helper | `make run_test_compare` 因 helper 缺失失败。 |
| GREEN same-chain helper | `include/impl/grabcut_diagnostic_reference.hpp` | Std 构建走标量同构链路，RVV 构建走 `__RVV10__` helper | `make run_test_compare` Std/RVV 均通过，最大误差在预算内。 |
| Bench shape update | `src/bench_grabcut.cpp` 后续增加 `gmm_probability` case | QEMU log-shape smoke，板卡 A/B 前不下性能结论 | 可输出 case、avg_ms、checksum；板卡 compare 才能进入性能判断。 |

## 数值预算

本阶段 RVV candidate 使用 `pcl::expf_RVV_f32m2`。它在有限域内近似 `expf`，不完整覆盖 `std::exp` 的 NaN / Inf / overflow 语义。测试样本只使用 determinant（协方差行列式）为正、二次型有限的 Gaussian；误差预算为 absolute error（绝对误差）`2e-5` 与 relative error（相对误差）`2e-4` 二者取较大值。

## Evidence Doctor 和 registry 规则

本阶段如果只运行 correctness，不要求脚本化 Evidence Doctor。若新增 board summary 或 Std/RVV bench compare，必须先补 manifest 或在 result 中记录 `metadata_incomplete` warning。本 topic 尚无 `log/evidence_registry.json`，result 需写 `evidence_registry_status=not_available`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation（组件消融）。 |
| A/B boundary | test-only helper，不是 production public entry。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 GMM component 是否值得后续 public-shaped diagnostic。 |
| diagnostic 是否可外推到 production | unknown。公式来自 production，但不包含 `learnGMMs` 的 `K=5` 小循环、GMM fitting、terminal weight `log`、graph edge mutation 和 max-flow。 |
| comparison-boundary / baseline mismatch 风险 | yes。batch wrapper 可能高估真实收益，因为 public path 里 solver / graph 状态机成本很重。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅当后续 component timing 证明 GMM probability 在 public shape 中占比可见时允许。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。任何 production 接入前都需要真实 public entry / fallback / asm / board 证据。 |

## 继续 / 停止条件

默认继续到 GMM correctness 闭合。若 correctness 失败且无法在 test-only helper 内解释，停止为 `blocked`。若 correctness 通过，则下一步是把 bench 增加 `gmm_probability` case，并在板卡可用时执行 bounded repeated board（有界重复板卡测试）或明确记录工具阻塞。
