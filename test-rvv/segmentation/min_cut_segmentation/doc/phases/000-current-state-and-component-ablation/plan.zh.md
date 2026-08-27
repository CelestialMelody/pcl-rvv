# Phase 000 Plan: current state and component ablation

## 阶段意图和边界

本阶段证明 `calculateUnaryPotential` 和 `calculateBinaryPotential` 中的 potential loop 是否有 RVV component value（组件价值）。阶段范围限定为 test-only helper、topic-local test / bench / script / 文档；不修改 `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp`。

validated_scope：`PointXYZ`，`float` xyz AoS layout，dense synthetic cloud，input-indexed row 和 edge-list row。unvalidated_scope：模板 `PointT` 泛型 traits、`Scalar=double` production 语义、真实 `buildGraph()` search / graph mutation、max-flow solver、indices 子集和非 dense 输入。

## 当前状态清单

| 项 | 状态 | 证据 |
| --- | --- | --- |
| topic 资产 | 新建 | `test-rvv/segmentation/min_cut_segmentation` 原先不存在 |
| production 源码 | 标量 | `segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp` |
| 上游测试 | 存在 | `test/segmentation/test_segmentation.cpp` 的 `MinCutSegmentationTest` |
| RVV helper | planned | 尚未创建 |
| Evidence Doctor | planned | `test-rvv/script/evidence_doctor.py` |
| 板卡 | available by prompt | 本轮用户说明板卡可用 |

## 假设与候选族

| 候选 | 假设 | 风险 |
| --- | --- | --- |
| unary foreground min-distance batch | 用 RVV 跨 input points 批处理，foreground loop 保持标量外层，可以减少每点距离公式成本 | foreground 点少时收益弱；graph addEdge 可能吞掉收益 |
| binary distance + expf approximation | edge list 上 gather source/target xyz，距离公式和 `expf_RVV_f32m2` 能降低 KNN 后 edge weight 成本 | `expf_RVV_f32m2` 是 float 有限域近似，不是 double `std::exp` production 替代 |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED correctness | `make -C test-rvv/segmentation/min_cut_segmentation run_test_rvv` | 在 RVV helper 缺失时失败，证明测试能抓住未实现路径 |
| GREEN correctness | `make run_test_compare` | Std / RVV 两侧通过；RVV 侧实际调用 candidate |
| bench build smoke | `make dump_bench_rvv` | RVV bench binary 编译并出现相关 RVV 指令 |
| board repeated | `make run_board_min_cut_repeated` | 生成 repeated summary、manifest 和 Evidence Doctor |
| result 回填 | `result.zh.md`、matrix、roadmap、evaluation | EvidenceDecision 和下一 phase 明确 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar component value |
| diagnostic 是否可外推到 production | no；当前不包含 KNN search、Boost graph mutation、edge_marker 去重和 max-flow |
| comparison-boundary / baseline mismatch 风险 | yes；binary RVV 候选使用 float `expf_RVV_f32m2`，production 标量是 double `std::exp` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只在 component 占比或 Phase 010 public-shaped timing 支持时允许 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；production patch 前还需要 production direct tests、fallback、asm 和板卡 repeated |

## 板卡复跑预算和决策桶

默认执行 5-run repeated board。若 Evidence Doctor 显示方向接近 1.0、长尾明显或 `B/A < 1` 频率异常，最多追加一次同边界 5-run。桶规则：median speedup >= 1.10 且无 checksum Error 为 positive；1.03 到 1.10 为 weak-positive；0.97 到 1.03 为 neutral；低于 0.97 为 negative；跨桶摇摆为 unstable。

## 继续 / 停止条件

Phase 000 完成后，如果 unary 或 binary component 为 positive / weak-positive，默认下一 phase 是 `010-production-shaped-buildgraph-timing`。如果二者都 neutral / negative，进入 diagnostic closeout，不接 production。任何 production patch 都需要新的 PI1 计划。
