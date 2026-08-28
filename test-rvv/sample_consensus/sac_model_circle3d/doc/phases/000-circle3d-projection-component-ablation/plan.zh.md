# Phase 000 Plan: circle3d projection component ablation

## 阶段意图和边界

本阶段只验证 `SampleConsensusModelCircle3D<PointT>` 的 count/select 投影距离核是否值得继续 RVV 化。它使用 test-only candidate（仅测试使用候选）模拟 public entry（公开入口）状态：direct indexed `indices_`、`PointXYZ`、float xyz AoS（结构数组）和 `Scalar=float` 系数。它不修改 production（生产源码），不覆盖 `getDistancesToModel`、`projectPoints`、`optimizeModelCoefficients`、泛型点类型或 `Scalar=double`。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| production source | 只有标量路径。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` |
| retained-candidate rescreen | 建议 circle3d 先做 component ablation。 | `doc-rvv/library-screening/sample_consensus/sample_consensus-retained-candidate-rescreen.zh.md` |
| sibling experience | circle2d、line、stick 证明 gather、mask、`vcompress`、full-RVV sqrt/store 可行，但不能外推 3D 投影公式。 | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` |
| topic assets | 本阶段新建 `src/`、`include/`、`include/impl/` 和 topic-local docs。 | `test-rvv/sample_consensus/sac_model_circle3d/` |

## 标量路径和候选假设

标量 count/select 对每个 `indices_` 元素读取 `x/y/z`，计算 `lambda = -(P-C).dot(N) / N.dot(N)`，把点投影到圆平面，再用 `normalize(P_proj-C)` 找到圆周点 `K`，最后用 `|P-K|^2 < threshold^2` 判断 inlier。候选先保守复刻这条链路；若投影点接近圆心，candidate 必须 fallback（回退）到标量同构链路，避免无效 lane（向量通道）污染诊断结论。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / component ablation。 |
| A/B boundary | test helper；baseline 是 public count/select 标量路径，candidate 是 test-only projection helper。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 implementation-shape（实现形态）。 |
| diagnostic 是否可外推到 production | no。它只能支持是否进入 production integration loop（生产接入闭环）的判断。 |
| comparison-boundary / baseline mismatch 风险 | yes。public baseline 调真实 production 标量入口，candidate 是测试派生类 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | unknown；只有 correctness、asm、board component bench 和 mismatch audit 都闭合后才判断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。真正采纳前需要 production direct（真实生产路径）和必要 fallback 证据。 |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| projection count/select candidate | direct indexed `indices_` | `PointXYZ` / float xyz AoS / float coefficients | planned | planned | planned | planned | planned | planned |
| getDistances full-RVV | direct indexed `indices_` | `PointXYZ` / double output | deferred | deferred | deferred | deferred | deferred | waits for Phase 000 |

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| 写 RED correctness test | `src/test_sac_model_circle3d.cpp`，`make run_circle3d_phase000_tests` | 测试因 candidate 未实现而失败。 |
| 实现 test-only candidate | `include/impl/sac_model_circle3d_candidates.hpp` | count/select 与 public path 在 `1e-5` 内一致。 |
| 补 bench scaffold | `src/bench_sac_model_circle3d.cpp` | 能输出 public/candidate timing 和 checksum。 |
| QEMU correctness | `make run_test_compare` | Std/RVV 测试通过；QEMU 不写性能结论。 |
| asm attribution | `make dump_bench_rvv` 后检查 candidate 符号 | 确认 RVV 指令能归属到 test-only helper；若未实现 RVV，标为 partial。 |
| board repeated | `collect_*` target 后续补齐 | 5-run，预算最多 1 次同边界确认复跑；性能桶使用 positive / weak_positive / neutral / negative / unstable。 |
| Evidence Doctor / registry | `projection-repeated-evidence-*`、`log/evidence_registry.json` | benchmark 或 EvidenceDecision 前必须有 doctor 摘要，或人工说明缺口。 |

## 文档更新清单

本阶段更新 README、evaluation、phase result、optimization roadmap、optimization matrix 和 Handoff。当前没有 adopted production behavior，因此不创建 `doc-rvv/sample_consensus/sac_model_circle3d-RVV.zh.md`。

## 继续 / 停止条件

若 correctness 无法闭合，停止在 diagnostic blocked。若 correctness、asm 和 board component bench 都正向，进入 S10 `partial-production-candidate`，再由用户确认是否进入 production integration loop。若性能为 weak、negative、neutral 或 unstable，本阶段只能拒绝或暂缓当前 diagnostic boundary，不能直接推出所有 production probe 不可行。
