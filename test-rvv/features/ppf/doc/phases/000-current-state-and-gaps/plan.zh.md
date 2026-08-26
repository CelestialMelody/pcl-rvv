# Phase 000 Plan: Current State And Gaps

## 阶段意图和边界

本阶段建立 `PPFEstimation::computeFeature` 的 S2 evaluation（函数级评估）、topic-local scaffold
（主题本地脚手架）和 same-chain scalar reference（同构标量参考链路）。本阶段不修改
`features/include/pcl/features/impl/ppf.hpp`，不做 production dispatch（生产分流），也不声明性能结论。

validated_scope（本阶段准备证明的范围）：`PointXYZ` 输入点、`Normal` 法线、float 字段、
sequential indices（连续索引）和小型确定性点云上的 all-pairs output 语义。

unvalidated_scope（未验证范围）：泛型 `PointInT` / `PointNT`、非连续 indices、`Scalar=double`、
`PPFRGB` / `CPPF`、真实板卡性能、反汇编归属和 production integration loop（生产接入闭环）。

point_type_expansion_queue（点类型扩展队列）：无 production 候选前暂不展开；若 Phase 010/020
诊断 positive，再建立 `PointNormal`、`PointXYZI`、`PointXYZRGBA` 与 normal traits（法线字段特征）
扩展 phase。

phase_closeout_boundary（阶段收口边界）：只能关闭 “reference scaffold correctness” 矩阵条目。

## 当前状态清单

| area | 状态 |
| --- | --- |
| production | `features/include/pcl/features/impl/ppf.hpp` 无 RVV 分流。 |
| helper choice | 当前 production 调用 `pcl::computePairFeatures`，不是 `pcl::computePPFPairFeature`。 |
| upstream test | `test/features/test_ppf_estimation.cpp` 只覆盖 bunny PCD 上少量固定输出。 |
| topic-local assets | Phase 000 创建前不存在 `test-rvv/features/ppf`。 |
| board | 用户已说明板卡可用；本阶段不需要板卡性能。 |

## 假设与候选族

| 假设 | 验证方式 | 风险 |
| --- | --- | --- |
| same-chain reference 能复刻 production 输出，包括 identity pair NaN 和 `output.is_dense=false`。 | 先写失败测试，再实现 reference 并跑 Std/RVV correctness。 | Eigen transform 和 `alpha_m` sign 修正容易写偏。 |
| PPF 的 `output.push_back` 风险需要按当前源码修正为 indexed store 审计。 | evaluation 和 roadmap 记录 current source fact。 | 历史筛选文案不能替代当前源码。 |
| pair-feature RVV 候选应复用 PFH 的 direct-AoS 数学经验，但不复制其 production 结论。 | Phase 010 另写候选和 bench。 | PPF 后段 `alpha_m` 可能吞掉 pair math 收益。 |

## 优化矩阵

见 `../optimization-matrix.zh.md`。本阶段只推进 `same-chain scalar reference`。

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 写 RED 测试 | `src/test_ppf.cpp` 引用尚未实现的 reference helper；`make -C test-rvv/features/ppf run_test_std` | 编译失败，报缺少 `ppf_test` helper。 |
| 实现 reference helper | `include/impl/ppf_reference.hpp`、`include/ppf.h` | 测试能编译并与 production 输出对拍。 |
| 跑 correctness | `make -C test-rvv/features/ppf run_test_compare` | Std/RVV 两侧通过；QEMU 只表示正确性。 |
| 更新 phase result / roadmap / matrix | `result.zh.md`、`optimization-roadmap.zh.md`、`optimization-matrix.zh.md` | 记录 RED/GREEN 命令、证据边界和 Phase 010 默认入口。 |

## Evidence Doctor 和 registry 规则

本阶段不生成 benchmark、board summary、checksum summary 或 asm attribution，因此 Evidence Doctor
脚本不适用。若测试输出进入 Handoff，只按人工检查记录：没有性能结论、没有 board evidence、
没有 production direct 证据。

## 板卡复跑预算和决策桶

not_applicable。本阶段只跑 QEMU correctness，不需要板卡复跑。Phase 010 若进入 bench，默认
`REPEATED_BOARD_RUNS=5`，最多一次同边界确认复跑；positive / weak / neutral / negative 口径在
Phase 010 plan 冻结。

## 继续 / 停止条件

默认继续：Phase 000 通过后，创建 Phase 010 `pair-feature-and-output-staging-ablation` 计划并推进
candidate / bench。只有 reference 无法同构复刻 production、构建环境不可用、dirty isolation 不安全、
或继续需要修改 production 才停止。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/ppf-evaluation.zh.md`、`doc/phases/README.zh.md`、
`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` 和本 result。
不创建 `doc-rvv/features/ppf-RVV.zh.md`。

## Roadmap 同步动作

Phase 000 完成后，把 reference scaffold 置为 `adopted` 或 `blocked`，并把 Phase 010 的
pair-feature batch RVV、alpha_m RVV 和 output staging 审计重新排序。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic correctness（诊断正确性），不是 performance evidence（性能证据）。 |
| A/B boundary | production public output vs test-only reference。 |
| 当前决策问题 | implementation-shape：reference 是否复刻当前 production 行为。 |
| diagnostic 是否可外推到 production | no。本阶段只证明 reference 可用于后续对拍，不证明生产接入。 |
| comparison-boundary / baseline mismatch 风险 | yes。当前 helper choice 是 `computePairFeatures`，`computePPFPairFeature` 只能作为后续审计对象。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。若 reference 不成立，先修 correctness。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes，且必须等后续 production integration loop。 |
