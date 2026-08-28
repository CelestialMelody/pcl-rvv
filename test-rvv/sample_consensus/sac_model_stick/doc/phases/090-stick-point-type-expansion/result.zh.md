# Phase 090: stick point type expansion result

## 执行范围

本阶段按 `plan.zh.md` 只扩展 correctness（正确性）证据，不修改 production（生产源码）。验证范围是 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal`，row source policy（行来源策略）仍是 direct indexed `indices_`。

## 动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| add access wrapper and typed fixtures | done | `src/test_sac_model_stick.cpp` | 新增 `SampleConsensusModelStickAccess` 和 `makeStickDistanceCloudAs<PointT>`，可用同一几何输入构造常见 xyz AoS（结构数组）点型。 |
| add point-type correctness test | done | `AdditionalAoSPointTypesMatchStandardPath` | 四个点型的 public count/select/getDistances 均与 Standard helper 对拍。 |
| add target alias | done | `Makefile` 的 `run_stick_point_type_tests` | 可单独运行点型扩展 case。 |
| run focused alias | done | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_point_type_tests` | RVV/QEMU 1/1 通过。 |
| run full correctness | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建各 11/11 通过。 |
| update docs | done | README、correctness、overview、optimization evidence、roadmap、matrix、`doc-rvv`、队列表 | 文档把本阶段写成 representative point type correctness，不写 dedicated performance。 |

## Correctness 结果

新增测试 `AdditionalAoSPointTypesMatchStandardPath` 对每个点型执行同一组检查：

- public `countWithinDistance` 与 `countWithinDistanceStandard` 返回一致。
- public `selectWithinDistance` 与 `selectWithinDistanceStandard` 的 `inliers` 和 `error_sqr_dists_` 一致。
- public `getDistancesToModel` 与 `getDistancesToModelStandard` 的 dense distance（稠密距离）输出一致。

本阶段已运行 focused RVV alias：

```text
[  PASSED  ] 1 test.
```

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| point-type-expansion | direct indexed `indices_` | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal`；`Eigen::VectorXf`；xyz AoS | public count/select/getDistances | done: focused RVV alias 1/1；full Std/RVV aggregate 11/11 | not_applicable in this phase | not_applicable for performance | production helper symbols unchanged; Phase 080 asm still applies to helper family | not_applicable without new board performance manifest | adopted_for_representative_point_type_correctness | dedicated point-type performance needs new board budget / phase |

## Evidence Doctor 和 registry

本阶段没有生成新的 board performance manifest，因此 Evidence Doctor 不适用。Phase 080 production manifest / doctor / registry 仍是当前性能采纳证据。新增 correctness 不改变 Phase 080 的 speedup、checksum 或 decision bucket。

## Diagnostic 到 production 回填审计

| question | answer |
| --- | --- |
| evidence role | production correctness。 |
| A/B boundary | public entry vs Standard helper in the same typed model。 |
| 当前决策问题 | 当前 traits-gated production path 能否覆盖代表性非 exact `PointXYZ` 点型的输出语义。 |
| diagnostic 是否可外推到 production | 不需要外推；测试直接调用 production public entry。 |
| comparison-boundary / baseline mismatch 风险 | public entry 和 Standard helper 复用同一个模型状态、同一组 `indices_` 和系数。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；没有性能比较。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不适用；不是 RVV family selection（实现族选择）。 |

## Continue / stop decision

本阶段已把代表性点型 correctness 证据扩展到 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal`。仍未证明这些点型的 dedicated board performance；若维护者需要把性能结论从 `PointXYZ` 扩大到常见点型集合，应另建 point-type performance phase，先冻结板卡预算、bench label 和 Evidence Doctor 输入。

当前 topic 内没有新的同边界 production performance candidate。Phase 080 采纳的三入口实现保持当前生产行为。
