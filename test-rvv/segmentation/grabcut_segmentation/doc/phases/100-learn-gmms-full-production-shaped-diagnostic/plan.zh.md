# Phase 100 full learnGMMs production-shaped diagnostic 计划

## 阶段意图和边界

本阶段做完整 `learnGMMs()` production-shaped diagnostic（生产形态诊断），不修改 production（生产源码）。
Phase 090 的 assignment-only（仅分量归属）诊断在板卡上为 positive，但它不包含 GaussianFitter relearn
（高斯拟合器重新训练）。本阶段要回答：把 assignment 子阶段替换为 RVV candidate 后，完整 `learnGMMs()`
形态中是否仍有可见收益。

本阶段证明：

- 测试专用 full helper 按 production `learnGMMs()` 的两段流程执行：先选择 component，再按 component 重新累加并更新 background / foreground GMM。
- RVV assignment + scalar relearn 与 scalar assignment + scalar relearn 结果一致。
- 板卡 full `learnGMMs()` component bench（组件性能测试）是否仍是 positive。

本阶段不证明：

- 新 RVV family（实现族）已经可接入 production。
- public `extract()` 在当前 adopted terminal helper 之外获得额外收益。
- `Scalar=double`、自定义点型、non-organized KNN、其它 layout 或其它 row source policy（行来源策略）已覆盖。

## 当前状态清单

| item | current evidence |
| --- | --- |
| adopted production patch | 当前 production 只覆盖 `initGraph()` unknown terminal weights。 |
| Phase 080 profile | `080-public-extract-component-profile/result.zh.md`：`learn_gmms` Std median `191.261680 ms`，占 `11.462780%`。 |
| Phase 090 assignment diagnostic | `090-learn-gmm-component-assignment-diagnostic/result.zh.md`：assignment-only board median B/A `3.528303x`，Evidence Doctor `0/0/0`。 |
| current matrix | `optimization-matrix.zh.md` 将 full `learnGMMs` 标成 `phase_deferred + unblocked`。 |
| evidence registry | Phase 080/090 summary artifact 已登记；本阶段新增 summary 后必须刷新 registry。 |

## 假设与候选族

| candidate family | 假设 | 本阶段处理 |
| --- | --- | --- |
| full `learnGMMs` with RVV assignment | assignment 是可向量化公式段；后续 scalar relearn 不会完全稀释收益。 | 新增 full helper、gtest、bench case、QEMU smoke、板卡 repeated 和 Evidence Doctor。 |
| GaussianFitter accumulation RVV | 按 component bucket 累加有数据相关写入，可能是 full 阶段剩余成本。 | 本阶段不向量化；若 full 结果为 weak / neutral，可在 result 中决定是否再开 accumulation profile。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| full `learnGMMs` with RVV assignment | organized image grid | `PointXYZRGB`-like float `Color` / K=5 GMM / foreground-background mask | test-support helper matching production `learnGMMs()` assignment plus relearn | add gtest comparing component vector and updated GMM against scalar full helper | add `--case learn_gmms_full` | planned 5-run board repeated, 96x72 | test-support helper boundary; production asm not applicable | planned manifest / Doctor | planned |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| add full helper | `include/impl/grabcut_diagnostic_reference.hpp` | Std/RVV 构建均可编译；candidate 只替换 assignment，relearn 使用同一标量 fitter 语义。 |
| add correctness test | `src/test_grabcut.cpp` | `make run_test_compare` 通过；components、GMM `pi`、`mu`、`determinant` 和 `inverse` 在预算内一致。 |
| add bench case | `src/bench_grabcut.cpp`、`Makefile` | `--case learn_gmms_full` 输出 parseable `BENCH grabcut_component`，checksum 覆盖 components 和 GMM 参数。 |
| add manifest metadata | `script/generate_grabcut_board_evidence_manifest.py` | Evidence Doctor 能识别 evidence role、A/B boundary、timer boundary 和 checksum policy。 |
| QEMU smoke | Std/RVV `--case learn_gmms_full` | checksum 一致；QEMU timing 不进入性能结论。 |
| board repeated | `collect_learn_gmms_full_repeated_board` | 5-run board repeated、manifest、Doctor、registry 闭合。 |
| docs refresh | `result.zh.md`、roadmap、matrix、README、evaluation、Handoff | 写清 full 边界是否支持继续 PI1。 |

## Evidence Doctor 和 registry 规则

本阶段新增 summary manifest 路径：

- `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/100-learn-gmms-full-production-shaped-diagnostic/repeated-evidence-doctor.json`

`Error` 包括 checksum 不一致、Std/RVV 缺侧、metadata 缺失或 run 数不足。`Warning` 包括 B/A 跨方向、长尾或环境字段缺失。`Suggestion` 记录为后续可选项。

## 板卡复跑预算和决策桶

- 初始预算：1 次 5-run repeated board，`--width 96 --height 72 --iterations 8 --warmup 2 --case learn_gmms_full`。
- 追加预算：只有 Evidence Doctor Warning 未能解释，或 median B/A 落在 `0.98-1.02` 且影响下一步时，最多追加 1 次同边界 repeated。
- 决策桶：
  - `full_learn_gmms_positive`：median B/A >= `1.05`，checksum 一致，Doctor 无 Error。
  - `full_learn_gmms_weak`：median B/A 在 `1.02-1.05`，只作为生产接入前线索。
  - `full_learn_gmms_neutral_or_negative`：median B/A <= `1.02` 或方向跨越。
  - `unstable`：两次 repeated 的方向或桶摇摆。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test-support helper matching full `learnGMMs()` assignment plus relearn |
| 当前决策问题 | implementation-shape：是否值得进入有界 production integration loop |
| diagnostic 是否可外推到 production | partial。它覆盖完整 `learnGMMs()` 局部函数语义，但仍不是真实 `GrabCut<PointT>::refineOnce()` 或 public `extract()` dispatch。 |
| comparison-boundary / baseline mismatch 风险 | yes。测试 helper 使用 synthetic image 和 mask，不覆盖 production object lifetime、trimap 更新或 public wall-time。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。弱 / 负 / 中性 / 不稳定只说明当前 full diagnostic 不足以推进 production。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若本阶段 positive，只能进入 PI1 计划；production 采纳仍需 production direct tests、asm、接入后 board 和用户确认。 |

## 继续 / 停止条件

默认推进到 helper、gtest、QEMU smoke、板卡 repeated、Evidence Doctor、registry 和 phase result。只有以下情况停止：

- full helper 无法保持 production `learnGMMs()` 语义。
- correctness 或 checksum 失败且无法修复。
- 板卡不可达或 Evidence Doctor Error 未能修复。
- 结果为 `full_learn_gmms_neutral_or_negative`，且 roadmap 没有其它高价值未阻塞方向。
- 若结果为 positive，继续到 PI1 需要扩大到 production patch，必须先暂停并等待用户明确授权。
