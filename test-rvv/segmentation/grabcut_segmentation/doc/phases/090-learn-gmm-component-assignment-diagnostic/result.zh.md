# Phase 090 learn GMM component assignment diagnostic 结果

## 阶段结论

本阶段完成 `learn_gmm_assignment` component diagnostic（组件诊断）。新增测试专用 helper 对齐
`learnGMMs()` 第一段 component assignment（分量归属选择）：每个像素按 hard segmentation（硬分割状态）
选择 foreground 或 background GMM（前景 / 背景高斯混合模型），再从 K=5 Gaussian component（高斯分量）中
选概率最大的分量编号。本阶段不修改 production（生产源码）。

Milkv-Jupiter 96x72、`iterations=8`、`warmup=2`、5-run repeated board 结果为 `assignment_positive`：
B/A 为 `3.564149, 3.511878, 3.528303, 3.613924, 3.522483`，median `3.528303x`。
Std median 为 `3.847615 ms`，RVV median 为 `1.092302 ms`，两侧 checksum 均为
`7343836954147025114`。Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=0, Suggestions=0`。

这个正向结果只覆盖 `learnGMMs()` 的 assignment 子阶段。它可以支撑下一阶段做完整 `learnGMMs`
production-shaped diagnostic（生产形态诊断），不能直接支撑 production adoption（生产采纳）。

## 实际执行范围

| action | 状态 | 证据 |
| --- | --- | --- |
| add assignment helper | done | `include/impl/grabcut_diagnostic_reference.hpp` 新增 `assignGMMComponentsReference()`、`assignGMMComponentsCandidate()` 和分组 candidate helper。 |
| add correctness test | done | `src/test_grabcut.cpp` 新增 `GrabCutDiagnosticReference.LearnGMMComponentAssignmentMatchesScalarReference`。 |
| add bench case | done | `src/bench_grabcut.cpp` 新增 `--case learn_gmm_assignment`，输出 component checksum。 |
| add manifest metadata | done | `script/generate_grabcut_board_evidence_manifest.py` 新增 `learn_gmm_assignment` case label。 |
| add Make targets | done | `collect_learn_gmm_assignment_repeated_board`、manifest、Doctor 和 registry target 已接入。 |
| correctness / QEMU smoke | done | `make run_test_compare` 通过；Std/RVV QEMU smoke checksum 一致，QEMU timing 不作为性能证据。 |
| board repeated / Doctor | done | `doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-board-20260827-clean-96x72`；Doctor 为 `0/0/0`。 |

## 诊断证据链

| 证据 | 结果 | 说明 |
| --- | --- | --- |
| correctness（正确性） | Std/RVV `run_test_compare` passed；component vector 与标量 reference 一致 | 证明 candidate 在测试样本上保持分量选择语义。 |
| QEMU smoke（QEMU 小型验证） | Std/RVV checksum 均为 `13736753309912082280` | 只证明可运行和日志形状；不作为性能结论。 |
| board performance（板卡性能） | median B/A `3.528303x` | 只覆盖 assignment-only helper，不包含 GaussianFitter 重新累加和 GMM 参数更新。 |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=0` | 脚本覆盖范围内没有 checksum、run count、A/B metadata 或长尾异常。 |
| registry（证据登记） | summary files 已登记 | `log/evidence_registry.json` 记录 `learn_gmm_assignment` manifest / Doctor。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic component A/B（组件诊断对照） |
| A/B boundary | test-support helper（测试支撑 helper）边界，只替换 `learnGMMs()` 第一段 assignment |
| 当前决策问题 | implementation-shape：是否值得继续到完整 `learnGMMs()` production-shaped diagnostic |
| 计时边界 | component assignment only；不包含 `GaussianFitter` bucket accumulation（按分量桶累加）、GMM 参数更新、`initGraph` 或 max-flow |
| row source / point type / layout | organized image grid、`PointXYZRGB`-like float color、foreground/background mask、96x72 |
| baseline / candidate 路径 | baseline 为 `assignGMMComponentsReference()`；candidate 为 `assignGMMComponentsCandidate()` |
| diagnostic 是否可外推到 production | no。完整 `learnGMMs()` 还要用分量编号重新训练 GMM，assignment-only 的 3.53x 可能被后续 accumulation 稀释。 |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 使用测试专用 foreground/background positions staging；production 中数据来自 `hard_segmentation_` 和 `components` vector。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive。若结果曾为弱 / 负 / 中性 / 不稳定，则不应进入 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若后续要接 production，需要先完成完整 `learnGMMs()` 生产形态诊断，再按 production boundary 做接入计划和接入后板卡测试。 |

## 实现审计

RVV candidate 按 foreground/background mask 分组收集 position，再调用已验证的 GMM probability 批量 helper。
每个 lane（向量通道）仍只在同一个 GMM 内比较 K=5 component 的概率；最终 component vector 顺序按原像素位置写回。
这个 staging（分阶段暂存）是测试专用形态，用于隔离 assignment 子阶段收益。它不代表 production 中可以直接复用同样的
buffer layout（缓冲区布局）和写回策略。

## Evidence Doctor 和 registry

- manifest: `doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-manifest.json`
- Doctor: `doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.md`
- result: `Errors=0, Warnings=0, Suggestions=0`
- registry: `log/evidence_registry.json` 记录 `learn_gmm_assignment` summary files。

## 继续 / 停止决策

`continue_stop_decision=continue`。当前阶段已经证明 assignment 子阶段有稳定板卡收益，但证据边界仍是
assignment-only。下一阶段应创建 `100-learn-gmms-full-production-shaped-diagnostic`：同一测试资产中构造完整
`learnGMMs()` 形态，对比“标量 assignment + 标量 relearn”和“RVV assignment + 标量 relearn”，确认局部收益在
完整 GMM 学习阶段是否仍可见。只有该阶段仍 positive，才有理由提出有界 production integration loop。
