# Phase 000 Result: Current State And Gaps

## 实际执行范围

本阶段完成了 PPF topic-local scaffold（主题本地脚手架）和 same-chain scalar reference（同构标量参考链路）。
没有修改 `features/include/pcl/features/impl/ppf.hpp` production（生产源码）。

## 动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 写 RED 测试 | done | `make -C test-rvv/features/ppf run_test_std` | 按预期编译失败，缺少 `makeXYZAndNormalClouds`、`makeSequentialIndices` 和 `computePPFReference`。 |
| 实现 reference helper | done | `include/impl/ppf_reference.hpp`、`include/ppf.h` | reference 复刻当前 production 的 `computePairFeatures` helper choice 和 `alpha_m` 计算。 |
| 跑 correctness | done | `make -C test-rvv/features/ppf run_test_compare` | Std/RVV 两侧 QEMU correctness 均为 2/2 pass。QEMU 不代表性能。 |
| 更新 roadmap / matrix | partial | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | Phase 010 已列为默认下一步。 |

## 证据分层

| 证据 | 状态 | 边界 |
| --- | --- | --- |
| correctness | pass | 只证明 test-only reference 与 production 输出在小型 `PointXYZ + Normal` 顺序索引输入上一致。 |
| QEMU path | pass | Std/RVV 测试二进制可在 QEMU 运行；不说明性能。 |
| asm attribution | not_applicable | 本阶段没有 RVV candidate。 |
| board performance | not_applicable | 本阶段没有性能结论。 |
| Evidence Doctor | not_applicable | 没有 benchmark、board summary、checksum summary 或 asm attribution。 |

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | diagnostic correctness。 |
| A/B boundary | production public output vs test-only reference。 |
| 当前决策问题 | implementation-shape。 |
| diagnostic 是否可外推到 production | no，只提供后续候选对拍 oracle。 |
| comparison-boundary / baseline mismatch 风险 | helper choice 已按当前源码固定为 `computePairFeatures`；`computePPFPairFeature` 仍为未采用 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no，reference 不成立时不能进入生产探针。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes，且必须等 PI1-PI5。 |

## 阶段反思

当前源码没有 `output.push_back`，而是 resize 后 indexed store；复筛表中的 output append 风险应修正为
output staging / indexed store 审计。`alpha_m` 的 Eigen rotation 后段可能是 PPF 独有主成本，Phase 010
应先拆出 pair-feature batch RVV 与标量 `alpha_m` 的混合候选，避免一次性把两个热点混在一起。

## Continue / Stop Decision

`continue_stop_decision=continue`。未命中停止条件：构建环境可用、QEMU correctness 已通过、dirty isolation
仍限制在 `test-rvv/features/ppf/**`，且 roadmap 中存在授权且未阻塞的 Phase 010。

`next_phase_default=010-pair-feature-and-output-staging-ablation`。
