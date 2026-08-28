# Normal-plane Source × Normal 交叉布局 Phase Plan

## 阶段意图和边界

本阶段关闭 source × normal cross-product（source 与 normal 点型交叉组合）中的代表性 correctness（正确性）缺口。Phase 040 已验证 `PointXYZI` / `PointXYZINormal` 作为 source 且 normal cloud 为 `Normal`；Phase 060 已验证 `PointNormal` / `PointXYZINormal` 作为 normal cloud 且 source 为 `PointXYZ`。本阶段把这两条轴组合起来，确认现有 production gate（生产分流准入）在代表性交叉组合上仍命中 RVV，并且公开入口输出与 direct RVV helper 一致。

| 维度 | 本阶段验证范围 |
| --- | --- |
| public entry | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` |
| row source | ordered `indices_` gather |
| source 点型 | `PointXYZI`、`PointXYZINormal` |
| normal 点型 | `PointNormal`、`PointXYZINormal` |
| Scalar | `float` model coefficients；输出 `double` distances |
| fallback | 不新增 fallback 类型；复用 Phase 040/060 的 non-AoS source / normal fallback |
| 不证明 | 完整 PCL 点型全集、`PointXYZRGBNormal` / `PointXYZLNormal` 等更多 normal 点型、公开入口性能、`Scalar=double`、新 RVV family selection（RVV 实现族选择） |

## 当前状态清单

| 项 | 当前事实 | 路径 |
| --- | --- | --- |
| source axis | `PointXYZI + Normal`、`PointXYZINormal + Normal` public correctness 已关闭；Phase 050 有代表性 source protected helper performance。 | `040-normal-plane-aospoint-gate-expansion/result.zh.md`、`050-normal-plane-representative-aos-source-performance/result.zh.md` |
| normal axis | `PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` public correctness 已关闭；non-AoS registered normal fallback 已关闭。 | `060-normal-plane-normal-layout-expansion/result.zh.md` |
| production gate | `kNormalPlaneRVVLayoutCompatible<PointT, PointNT>` 同时检查 source `RVVXYZAoSFloatLayout` 和 normal/curvature AoS-compatible layout。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` |
| current tests | `run_normal_plane_public_tests` 当前覆盖 9 个 public/fallback/buffer 用例。 | `test-rvv/sample_consensus/plane_models/Makefile` |

## 假设与候选族

候选族是 `representative source-normal cross correctness`。source 侧只读取 `x/y/z`，normal 侧只读取 `normal_x/y/z/curvature`；两侧 point type 可以不同，并且应该分别使用自己的 byte offset 和 stride。只要两侧各自满足 AoS-compatible gate，组合后也应该走现有 RVV helper。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness target | board | doctor | decision 预期 |
| --- | --- | --- | --- | --- | --- | --- |
| source-normal cross correctness | ordered `indices_` | `PointXYZI + PointNormal`, float, AoS-compatible | public-vs-direct RVV | board public alias | no new performance doctor | adopted if pass |
| source-normal cross correctness | ordered `indices_` | `PointXYZI + PointXYZINormal`, float, AoS-compatible | public-vs-direct RVV | board public alias | no new performance doctor | adopted if pass |
| source-normal cross correctness | ordered `indices_` | `PointXYZINormal + PointNormal`, float, AoS-compatible | public-vs-direct RVV | board public alias | no new performance doctor | adopted if pass |
| source-normal cross correctness | ordered `indices_` | `PointXYZINormal + PointXYZINormal`, float, AoS-compatible | public-vs-direct RVV | board public alias | no new performance doctor | adopted if pass |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 扩 source builder | `src/test_sample_consensus_plane_models.cpp` | 增加可复用 source cloud helper，填充 `PointXYZI` / `PointXYZINormal` 额外字段。 |
| 增加交叉测试 | `src/test_sample_consensus_plane_models.cpp` | 4 个 source × normal public-vs-direct RVV tests。 |
| 更新 target filter | `Makefile` | `NORMAL_PLANE_PUBLIC_FILTER` 覆盖新增 4 个用例。 |
| 运行验证 | QEMU 和板卡 public alias | `run_normal_plane_public_tests`、`run_test_compare`、板卡 `run_board_normal_plane_public_tests` 通过。 |
| 文档同步 | phase result、README、testing/correctness/optimization/evaluation、matrix、roadmap、队列表 | 明确 Phase 070 覆盖和未覆盖范围。 |

## Evidence Doctor 和 Registry 规则

本阶段不新增性能 summary，不新增 phase 070 Evidence Doctor manifest。结束时运行 `evidence_status`、`repeated_evidence_status` 和 `phase050_evidence_status`，确保既有 Phase 000/030/050 性能 summary 与文档引用仍 fresh。

## 板卡复跑预算和决策桶

本阶段板卡预算为 1 次 public correctness alias。命令环境注入 SSH agent socket。若测试失败，先进入 gate repair；若只有远端 clock skew warning 但 GTest 通过，不降级 correctness 结论。性能 decision bucket 不变。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-public correctness |
| A/B boundary | public overload 对 direct RVV helper |
| 当前决策问题 | implementation-shape correctness |
| diagnostic 是否可外推到 production | 可以外推 correctness，因为测试直接调用真实 public entry；不外推性能。 |
| comparison-boundary / baseline mismatch 风险 | 低；同一模型对象内比较 public output 与 direct RVV output。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；失败说明当前 gate 或 helper 对交叉点型不成立，应修复或降级该组合。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；不改 RVV helper family。 |

## Phase Scope 与扩展队列

`validated_scope` 预期为 4 个 representative source × normal 组合的 public RVV correctness。`unvalidated_scope` 仍包括更多 PCL normal-like 点型、用户自定义点型、公开入口性能和 `Scalar=double`。

`point_type_expansion_queue`：

| queue item | 恢复条件 | 所需证据 |
| --- | --- | --- |
| more normal-like PCL types | 用户要求或调用点指向 `PointXYZRGBNormal`、`PointXYZLNormal` 等类型 | traits audit、public direct/fallback tests、QEMU、board alias |
| public-overload performance probe | 需要评估公开入口 dispatch / object setup 是否影响 helper-level positive 结论 | 明确计时边界、board repeated summary、Evidence Doctor |
| `Scalar=double` | 需要 double helper family | 新实现设计、数值预算、QEMU、asm、board |

## 继续 / 停止条件

本阶段通过后，代表性 source、representative normal layout 和二者交叉 correctness 均关闭。若 roadmap 剩余方向只包括更多点型全集、公开入口性能或 `Scalar=double`，它们需要新的范围选择；本轮可在 Handoff 中暂停并报告。若测试失败，进入 `normal-plane-cross-gate-repair`。

## 文档更新清单

- 新增 `doc/phases/070-normal-plane-cross-point-type-layout/result.zh.md`。
- 更新 phase README、optimization matrix、optimization roadmap。
- 更新 README、correctness-tests、testing-overview、optimization-evidence、evaluation、长期 production doc 和 sample_consensus 队列表。
