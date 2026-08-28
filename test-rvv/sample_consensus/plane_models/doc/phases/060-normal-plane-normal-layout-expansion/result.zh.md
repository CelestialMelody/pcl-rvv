# Normal-plane Normal Layout 扩展 Phase Result

## 执行范围

本阶段按计划只关闭 normal layout expansion（法线点类型布局扩展）的 correctness（正确性）和 fallback（回退路径）证据。未修改 production 源码；新增测试直接检验 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` 中已有的 `NormalPlaneRVVNormalAoSLayout<PointNT>` 和 `kNormalPlaneRVVLayoutCompatible<PointT, PointNT>` gate（分流准入条件）。

| 维度 | 实际覆盖 |
| --- | --- |
| public entry | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` |
| row source | ordered `indices_` gather |
| source 点型 | `PointXYZ` |
| normal 点型 | `Normal`、`PointNormal`、`PointXYZINormal` |
| fallback | non-AoS registered normal、curvature 非 float normal、non-AoS registered source |
| Scalar | `float` model coefficients；输出距离仍为 `double` |
| performance | 未新增；沿用 Phase 030/050 repeated board helper performance |

## 代码和测试变更

| 层级 | 路径 | 变更 |
| --- | --- | --- |
| test | `test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` | 新增 `NonAoSRegisteredNormal` fixture、normal cloud 生成 helper、`PointNormal` / `PointXYZINormal` normal cloud public-vs-direct RVV 测试，以及 non-AoS normal fallback 测试。 |
| test target | `test-rvv/sample_consensus/plane_models/Makefile` | `NORMAL_PLANE_PUBLIC_FILTER` 从 6 个 normal-plane public/fallback 用例扩展为 9 个，用于 QEMU 和板卡 public alias。 |
| docs | `test-rvv/sample_consensus/plane_models/doc/phases/060-normal-plane-normal-layout-expansion/plan.zh.md` | 新增本阶段计划。 |

## 验证结果

| 命令 | 结果 | 证据角色 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests` | QEMU/RVV public alias 9/9 passed。 | public dispatch / fallback correctness |
| `make -C test-rvv/sample_consensus/plane_models run_test_compare` | QEMU Std 28/28 passed，RVV 28/28 passed。QEMU 计时只作日志形状，不作性能结论。 | full correctness compare |
| `SSH_AUTH_SOCK=<injected> make -C test-rvv/sample_consensus/plane_models run_board_normal_plane_public_tests` | 板卡 public alias 9/9 passed。输出包含远端 clock skew warning，但测试结果为通过。 | board public correctness |

## Evidence Doctor 和 Registry

本阶段没有新增性能 summary，也没有新增 phase 060 Evidence Doctor manifest。现有性能结论仍引用 Phase 030 和 Phase 050：

- Phase 030 repeated board summary：`log/board/normal-plane-phase030-repeated-board/summary.md`，Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。
- Phase 050 representative source summary：`log/board/normal-plane-phase050-representative-aos-source-performance/summary.md`，Evidence Doctor 为 Errors=0、Warnings=5、Suggestions=0。

Phase 050 的 Warnings 已按点型和 helper 独立解释：`PointXYZI getDistancesToModel` 有长尾，另有 group outlier（组内收益差异）提示；所有 min speedup 仍远大于 1.0x，因此代表性 source helper performance 的 decision bucket（决策桶）保持 positive-stable。本阶段不改变该性能桶。

## Diagnostic 到 Production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-public correctness 和 fallback correctness |
| A/B boundary | public overload 对 direct RVV helper；fallback 对 Standard helper |
| 当前决策问题 | implementation-shape 和 fallback correctness |
| 可否外推到 production | 正确性可以，因为测试直接调用真实 public entry；性能不能外推，因为本阶段没有 repeated board bench。 |
| comparison-boundary / baseline mismatch 风险 | 低；同一对象内 public/direct RVV 或 public/Standard 对拍，没有跨 wrapper 计时比较。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；已有 production gate 若失败应进入 gate repair phase。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；未改变 RVV helper family。 |

## Optimization Matrix 更新

| candidate family | row source | point type / Scalar / layout | correctness / fallback | board | decision | 未覆盖范围 |
| --- | --- | --- | --- | --- | --- | --- |
| normal AoS layout expansion | ordered `indices_` | `PointXYZ + PointNormal`, float, AoS-compatible normal/curvature | public-vs-direct RVV passed | public alias passed | adopted for representative normal correctness | 不证明 source×normal 全组合或性能 |
| normal AoS layout expansion | ordered `indices_` | `PointXYZ + PointXYZINormal`, float, AoS-compatible normal/curvature | public-vs-direct RVV passed | public alias passed | adopted for representative normal correctness | 不证明 `PointXYZINormal` 同时作为 source 和 normal 的组合性能 |
| non-AoS normal fallback | ordered `indices_` | `PointXYZ + NonAoSRegisteredNormal`, registered float fields but non-standard-layout | public-vs-Standard fallback passed | public alias passed | adopted fallback | 不覆盖所有用户自定义 normal 类型 |

## Scope 和扩展队列

`validated_scope`：`PointXYZ + Normal`、`PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` 的 public RVV correctness；`NonAoSRegisteredNormal` 和 `NormalWithDoubleCurvature` fallback；Phase 040 已有 non-AoS source fallback。

`unvalidated_scope`：完整用户自定义点型全集、其它 PCL normal-like 点型、source × normal 交叉组合、`Scalar=double`、公开入口 repeated performance 和新的 RVV 实现族选择。

`point_type_expansion_queue`：

| queue item | 状态 | 恢复条件 |
| --- | --- | --- |
| more PCL normal-like point types | phase_deferred | 用户要求覆盖 `PointXYZRGBNormal`、`PointXYZLNormal` 或其它 normal 点型，或调用点显示这些类型是主要生产实例。 |
| source × normal cross product | phase_deferred | 需要证明 `PointXYZI` / `PointXYZINormal` source 与非 `Normal` normal cloud 的组合。 |
| `Scalar=double` | turn_stop_deferred with scope expansion | 需要新数值设计和 helper family，不属于当前 float `f32m2` production patch 完善。 |

## Continue / Stop Decision

`continue_stop_decision`：本阶段 closed。`stop_condition_hit`：当前矩阵内高优先级、未阻塞且不扩大范围的 normal layout correctness 动作已完成；剩余方向要么是完整泛型全集 / source×normal 组合扩展，要么是 `Scalar=double` 或公开入口性能，均需要新 phase 范围选择。

`next_phase_default`：若继续当前 topic，建议先做 `070-normal-plane-cross-point-type-layout`，只覆盖 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的交叉 correctness；若用户更关心收益，则改做 public-overload performance probe（公开入口性能探针），但该方向需要重新定义计时边界。
