# 000 current-state-and-component-plan 计划

## 阶段意图和边界

本阶段开启 `ApproximateProgressiveMorphologicalFilter<PointT>::extract` 的函数级 RVV 评估。范围只覆盖 `test-rvv/segmentation/approximate_progressive_morphological_filter/` 下的测试资产和 topic-local 文档，不修改 `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` 生产源码。

本阶段要证明三件事：`grid z-min`（点云写入栅格最小 z）、`window open`（窗口最小值后最大值的开运算）和 `tail-compress`（高度阈值筛选并保持 indices 顺序）能否拆成同构标量 / RVV 诊断组件。当前不证明生产分流、泛型点类型、indices 子集入口或 OpenMP 与 RVV 的最终分工。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| 生产入口 | `extract(Indices& ground)` 直接计算 window sizes、全输入 `getMinMax3D`、grid z-min、两段 window min/max open，再对当前 `ground` 做高度阈值筛选 | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| 队列状态 | segmentation 函数评估队列把本文件列为第一条未完成主题，建议先做 grid z-min / window open component evaluation | `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md` |
| 既有 topic 资产 | 无既有 `test-rvv/segmentation/approximate_progressive_morphological_filter/` 目录 | 本阶段新建 |
| 生产状态 | 无本 topic production patch | `git status --short` |
| 板卡状态 | 当前会话说明板卡可用；需要性能结论时按有界预算运行 board target | 用户 prompt |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `z-min-index-staging` | RVV 先批量计算 row / col / flat cell id，再用标量按 cell 更新最小 z；适合判断 coordinate-to-grid 的占比 | 同一 cell 的 min 更新仍是标量随机写，收益可能被写入和冲突稀释 |
| `window-open-row-reduction` | Eigen `MatrixXf` 是 column-major（列主序），对固定列内连续 row 区间可用 RVV load + 局部规约；对固定 row 横扫 column 时 stride 较大 | 窗口边界和 NaN sentinel（哨兵）语义复杂，OpenMP 已覆盖外层并行，RVV 内层收益不确定 |
| `tail-compress-indices` | `diff < threshold` 的 mask 和保序写回可用 RVV + `vcompress` 诊断 | 需要从 `ground` 间接读取点和 `Zf(erow, ecol)`，gather 与压缩成本可能抵消收益 |

## 本阶段优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `z-min-index-staging` | full input cloud, no indices subset | `PointXYZ` / float / AoS + Eigen column-major grid | test-only component helper | `run_test_compare` 中 z-min same-chain 对拍 | `run_bench_compare` 的 z-min case，QEMU 只作 log-shape | `board_smoke` 或 repeated target，预算 5 run | `dump_bench_rvv` 检查 `vfcvt` / load 指令 | QEMU smoke 手工 doctor；board summary 走脚本或人工 doctor | planned |
| `window-open-row-reduction` | synthetic grid | float grid / Eigen column-major | test-only erosion + dilation helper | `run_test_compare` 中 open same-chain 对拍，覆盖 NaN hole 和边界窗口 | open pass case，按 half-size 分组 | 板卡可用时 5 run 决策桶 | `dump_bench_rvv` 检查 RVV load / store | 同上 | planned |
| `tail-compress-indices` | current ground vector | `PointXYZ` / float / AoS + Eigen grid lookup | test-only threshold helper | `run_test_compare` 中保序 indices 对拍 | tail-compress case | 板卡可用时 5 run 决策桶 | `dump_bench_rvv` 检查 compare / compress | 同上 | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 写 RED 测试 | `src/test_apmf.cpp` | 首次运行因缺少 candidate helper 或未实现 RVV diagnostic 而失败 |
| 实现 test-only helpers | `include/apmf.h`、`include/impl/apmf_components.hpp` | Std / RVV 两套构建的 component same-chain 测试通过 |
| 写 component bench | `src/bench_apmf.cpp` | 输出 `Dataset:`、`Iterations:`、case label、checksum，能被共享 compare 脚本解析 |
| 接入 Makefile / board.mk | `Makefile`、`board.mk` | QEMU test compare 可运行；QEMU bench compare 保持 guard；board target 可部署 |
| 补 topic-local 文档 | README、evaluation、testing overview、phase result、roadmap、matrix | 能从文档定位源码、target、证据边界和下一 phase |

## Evidence Doctor 和 registry 规则

本阶段先使用共享 `test-rvv/script/analyze_bench_compare.py` 生成 bench summary。若没有 topic-local manifest wrapper，Evidence Doctor（证据体检）先按人工检查记录：Errors / Warnings / Suggestions 必须写入 phase result 和 Handoff。若 board summary 进入当前结论，下一动作是补 `script/generate_apmf_evidence_manifest.py` 并调用 `test-rvv/script/evidence_doctor.py`。

## 板卡复跑预算和决策桶

板卡可用时，本阶段性能证据预算为 5 run，每 run 使用 bench 默认 warm-up 和 iteration。决策桶：

- `positive`：主要 component median speedup >= 1.20x，且 correctness / checksum 一致。
- `weak-positive`：1.05x 到 1.20x。
- `neutral`：0.95x 到 1.05x。
- `negative`：低于 0.95x。
- `unstable`：5 run 内方向反复或 Evidence Doctor 警告无法解释。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，只证明 test-only component helper 的局部行为 |
| A/B boundary | `test helper` |
| 当前决策问题 | `RVV-vs-scalar` 和 `implementation-shape` |
| diagnostic 是否可外推到 production | 否。生产入口还有 `initCompute`、`getMinMax3D`、OpenMP 外层、`copyPointCloud`、多轮 window loop 和真实 `ground` 更新 |
| comparison-boundary / baseline mismatch 风险 | 有。component helper 会把生产流程拆段计时，不能代表 public overload |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许条件是至少有一个同构组件在板卡上稳定正向，且 production probe 只改受控 helper / fallback，不扩大到泛型点类型 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。component positive 只能进入 `partial-production-candidate`，不能 clean-adopt |

## 继续 / 停止条件

默认继续到 QEMU correctness、反汇编和板卡 component bench。合法停止条件仅包括：测试/工具链不可用、板卡不可达、Evidence Doctor Error 无法修复、继续需要修改 production 或 public API、或者本阶段矩阵和 roadmap 均无当前授权内的未阻塞动作。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/approximate_progressive_morphological_filter-evaluation.zh.md`、`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/test-support-code-map.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 和本阶段 `result.zh.md`。`doc-rvv/segmentation/*-RVV.zh.md` 在没有 adopted production behavior 前不适用。

## roadmap 同步动作

本阶段结束后按证据回填：z-min staging、window row-reduction、tail-compress 三条候选的 adopted / attempted / rejected / deferred 状态；若出现收益但 production boundary 未闭合，下一 phase 默认进入 bounded production probe plan 或补 component ablation。
