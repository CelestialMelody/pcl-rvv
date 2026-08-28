# Phase 020: selectWithinDistance PI1 生产接入计划

## 阶段目标和授权边界

本阶段是 PI1 production integration plan（生产接入计划）。目标是把 Phase 000 中 positive-stable 的 `selectWithinDistance` 测试专用 candidate 转成可审查的 production probe（生产探针）计划。当前阶段只写计划，不修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp`。

本阶段只覆盖 `selectWithinDistance`。`getDistancesToModel` 当前候选 5-run board median `0.7781x`，不进入本 PI1 范围。`countWithinDistance` 只作为已有 production RVV regression（回归测试）保留。

## 候选范围

| 维度 | PI1 冻结值 |
| --- | --- |
| public entry | `SampleConsensusModelSphere<PointT>::selectWithinDistance` |
| row source | direct indexed `indices_` |
| 点类型 / layout | 初始生产探针可使用 `pcl::rvv::RVVXYZFloatLayout<PointT>` 或更强 AoS gate；不得写死 `PointXYZ` 当最终泛型结论。 |
| Scalar | `Eigen::VectorXf` model coefficients，阈值仍来自 `double threshold` 后转为 float shell bounds。 |
| 输出语义 | 按 `indices_` 顺序写 `inliers`，并写 `error_sqr_dists_` 为 `abs(sqrt(sqr_dist) - radius)`。 |
| 不覆盖 | `getDistancesToModel`、circle / normal-sphere、非 single-float xyz 字段、`Scalar=double`、32-bit byte offset 不安全的大 cloud。 |

## 生产实现草案

PI2 若获授权，应最小修改 production header：

1. 把现有 `selectWithinDistance` 标量主体抽成 `selectWithinDistanceStandard` 或等价私有 / protected helper，保持原有 PCL 空格风格。
2. 在 `__RVV10__` 下新增 `selectWithinDistanceRVV`，复用 `pcl::rvv_load::indexed_load3_fields_f32m2` 或 `indexed_load3_f32m2`，按 `indices_` 做 gather（离散加载）和 shell mask。
3. RVV helper 只接管 squared distance 和 shell predicate（球壳谓词判断）。为保持输出顺序和 error distance 语义，当前生产探针可先使用 scratch squared distance + 标量有序写回；后续 `vcompress` 作为独立 A/B。
4. public entry 在模型有效性检查和输出容器准备后，若 `__RVV10__`、traits gate、规模 gate 和 32-bit byte offset gate 成立，则调用 RVV helper；否则自然 fallback（回退）到标量 helper。

## Fallback / gate 计划

| gate | 处理 |
| --- | --- |
| `__RVV10__` 未启用 | 只编译标量 helper。 |
| 点类型不满足 xyz single-float traits | fallback 到标量 helper。 |
| 32-bit byte offset 风险 | 使用 `cloud.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` 或等价 gate；不能证明时 fallback。 |
| `indices_` 为空或规模很小 | 可以 fallback 标量，或让 RVV helper 自然处理；PI2 需冻结阈值。 |
| 非 dense / NaN / Inf | 保持原 public semantics；不新增会改变标量行为的过滤。 |
| 输出容器顺序 | RVV helper 必须保持 `indices_` 顺序。 |

## 证据计划

| 证据层 | PI2-PI5 必须动作 |
| --- | --- |
| production direct correctness | 在现有 `src/test_sac_model_sphere.cpp` 增加真实 public `selectWithinDistance` RVV dispatch 命中测试和 fallback 测试。 |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare`。 |
| asm attribution | 重新 `dump_bench_rvv`，优先让 production `selectWithinDistanceRVV` 有可归属符号；若内联，manifest 必须明确降级。 |
| board performance | 用 board repeated 跑 production public `selectWithinDistance`，不能复用 test-only candidate 行。 |
| Evidence Doctor | 生成 production direct manifest，处理 Error / Warning / Suggestion 后再做 PI5 EvidenceDecision。 |
| regression | 保持 `countWithinDistance` positive 行和 `getDistancesToModel` scalar public 行不被误改。 |

## Diagnostic 到 production mismatch audit

| question | PI1 判断 |
| --- | --- |
| evidence role | Phase 000 select 是 production-shaped diagnostic；PI2-PI5 必须生成 production direct。 |
| A/B boundary | Phase 000 是 `test_helper`；PI 后必须是 `public_overload` 或 production detail helper。 |
| 当前决策问题 | RVV-vs-scalar production probe 可行性，不是 clean adoption。 |
| diagnostic 是否可外推到 production | 只能支撑进入有界生产探针；不能直接采纳。 |
| comparison-boundary / baseline mismatch 风险 | 有。production bench 必须重跑 public entry，不得复用 diagnostic candidate speedup。 |
| 弱 / 负 / 中性时是否允许 bounded probe | 若 production direct 降为 neutral/negative，应停在 PI5 用户检查点，不能自行采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 证据 | 需要 PI5 用户确认；若新增 `vcompress` 或其它 family，还需 RVV-vs-RVV detail A/B。 |

## 暂停条件

进入 PI2 前必须有用户明确授权修改 production header。PI2-PI5 过程中若发现 fallback gate 不能隔离、需要扩大到 `getDistancesToModel`、需要 public API 变更、asm 归属无法解释、board 不可达或 Evidence Doctor Error 无法降级处理，应停在 Handoff。

## 下一步

默认下一动作：

```text
next_worker_action: 等待用户授权后，按本 PI1 计划进入 PI2 production_patch；若未授权，只保留当前测试资产和文档。
```
