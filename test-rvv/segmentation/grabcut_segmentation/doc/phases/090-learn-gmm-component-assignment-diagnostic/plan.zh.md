# Phase 090 learn GMM component assignment diagnostic 计划

## 阶段意图和边界

本阶段只做 `learnGMMs()` 中 component assignment（分量归属选择，即每个像素选择概率最大的 Gaussian 分量）诊断，不修改 production（生产源码）。Phase 080 的 public extract profile（公开入口形态性能剖析）显示 `learn_gmms` 在 Std 构建中约占总耗时 `11.46%`，且 Std/RVV 构建几乎无差异；它是剩余未优化组件中唯一有明确公式族和非微小占比的方向。

本阶段证明：

- 测试专用 RVV candidate（候选实现）能否在 `PointXYZRGB` organized image（有组织图像网格）样本上匹配标量分量选择结果。
- 只替换 assignment 子阶段时，板卡 component bench（组件性能测试）是否有稳定正向收益。

本阶段不证明：

- `learnGMMs()` 全函数、`GaussianFitter` bucket accumulation（按分量桶累加）或 production public `extract()` 已获得额外收益。
- 新 RVV family（实现族）可直接接入 production。
- `Scalar=double`、自定义点型、non-organized KNN、其它 layout 或其它 row source policy（行来源策略）已覆盖。

## 当前状态清单

| item | current evidence |
| --- | --- |
| Phase 080 profile | `080-public-extract-component-profile/result.zh.md` 将记录 clean 96x72 5-run board profile；`learn_gmms` Std median 约 `191.26 ms`，占 `11.46%`，Std/RVV 几乎相同。 |
| existing formula helper | `include/impl/grabcut_diagnostic_reference.hpp` 已有 `computeGMMProbabilityCandidate()`，在 RVV 构建下使用 `pcl::expf_RVV_f32m2`。 |
| production boundary | 当前已采纳 production patch 只覆盖 `initGraphTerminalWeightsRVV()`；本阶段不触碰 production。 |
| evidence role | diagnostic / component A/B（组件对照），只用于判断是否值得进入后续 production-shaped diagnostic 或 production integration loop（生产接入闭环）。 |

## 候选与假设

| candidate family | 假设 | 本阶段处理 |
| --- | --- | --- |
| learn GMM component assignment RVV | 每个像素对 K=5 Gaussian 重复概率公式，和 Phase 010/020/060 已验证的公式族相同；可批量计算概率并标量更新 best component。 | 新增 test-only helper、gtest、bench case、QEMU smoke、板卡 repeated 和 Evidence Doctor。 |
| GaussianFitter accumulation | 第二段按 `components[idx]` 写入 5 个 bucket，数据相关分支和小矩阵累加较强。 | 本阶段不实现；若 assignment positive 且全 `learn_gmms` 占比仍高，再单独做 accumulation profile。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| learn GMM component assignment RVV | organized image grid | `PointXYZRGB`-like float `Color` samples / K=5 GMM | test-only helper matching first loop of `learnGMMs()` | add gtest comparing component vector against scalar reference | add `--case learn_gmm_assignment` | planned 5-run board repeated, 96x72 and same synthetic image family | `grabcut_diag::assignGMMComponentsCandidate` or inlined RVV probability helper | planned manifest / Doctor | planned |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| add assignment helper | `include/impl/grabcut_diagnostic_reference.hpp` | Std/RVV 构建均可编译；RVV 构建批量计算概率，输出 component vector。 |
| add correctness test | `src/test_grabcut.cpp` | `make run_test_compare` 通过；小样本同时覆盖 foreground/background 两侧。 |
| add bench case | `src/bench_grabcut.cpp`、`Makefile` | `--case learn_gmm_assignment` 输出 parseable `BENCH grabcut_component`，checksum 使用 component vector 指纹。 |
| add manifest metadata | `script/generate_grabcut_board_evidence_manifest.py` | Evidence Doctor 能识别 evidence role、A/B boundary、timer boundary 和 checksum policy。 |
| QEMU smoke | `make run_bench_std ... --case learn_gmm_assignment` 和 RVV 对应命令 | checksum 一致，日志形状可解析；QEMU timing 不进入性能结论。 |
| board repeated | `collect_learn_gmm_assignment_repeated_board` | 5-run board repeated、manifest、Doctor、registry 闭合。 |

## Evidence Doctor 和 registry 规则

本阶段新增 summary manifest 路径：

- `doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/090-learn-gmm-component-assignment-diagnostic/repeated-evidence-doctor.json`

异常处理：

- `Error`：checksum 不一致、Std/RVV 缺侧、metadata 缺失或 run 数不足时，先修复并重跑。
- `Warning`：B/A 跨方向、长尾或环境字段缺失时，解释并按需要最多追加一次同边界 repeated。
- `Suggestion`：补温度、governor、binary hash 时记录为后续可选项，不阻塞诊断结论。

## 板卡复跑预算和决策桶

- 初始预算：1 次 5-run repeated board，`--width 96 --height 72 --iterations 8 --warmup 2 --case learn_gmm_assignment`。
- 追加预算：只有 Evidence Doctor Warning 未能解释，或 median B/A 落在 `0.98-1.02` 且影响下一步时，最多追加 1 次同边界 repeated。
- 决策桶：
  - `assignment_positive`：median B/A >= `1.08`，且没有 checksum / Doctor Error。
  - `assignment_weak`：median B/A 在 `1.02-1.08`，可作为线索但不足以立即进入 production。
  - `assignment_neutral_or_negative`：median B/A <= `1.02` 或跨方向明显，不进入 production。
  - `unstable`：两次 repeated 的方向或桶摇摆。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic component A/B（组件诊断对照） |
| A/B boundary | test-only helper for first loop of `learnGMMs()` |
| 当前决策问题 | implementation-shape：是否值得把 `learnGMMs()` assignment 子阶段推进到 production-shaped diagnostic |
| diagnostic 是否可外推到 production | no。它不包含 `GaussianFitter` relearn、真实 hard segmentation 状态演化或 public `extract()` 总耗时。 |
| comparison-boundary / baseline mismatch 风险 | yes。test-only helper 的 staging 和 component checksum 可能与 production `learnGMMs()` 全函数不同。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。只有 `assignment_positive` 且能写出全 `learnGMMs()` 同边界计划时才考虑后续 probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若替换或补充现有 adopted family，必须先做 production boundary 内的 RVV-vs-RVV 或 public evidence。 |

## 继续 / 停止条件

默认推进到 helper、gtest、QEMU smoke、板卡 repeated、Evidence Doctor 和 phase result。只有以下情况停止：实现发现 assignment 语义无法与 `learnGMMs()` 对齐、QEMU correctness 失败且无法修复、板卡不可达、Evidence Doctor Error 未能修复，或结果落入 `assignment_neutral_or_negative` 且 roadmap 没有其它高价值未阻塞方向。
