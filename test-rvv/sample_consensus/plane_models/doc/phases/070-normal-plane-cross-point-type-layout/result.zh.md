# Normal-plane Source × Normal 交叉布局 Phase Result

## 执行范围

本阶段按 `plan.zh.md` 关闭代表性 source × normal cross-product（source 与 normal 点型交叉组合）correctness（正确性）缺口。实际覆盖范围与计划一致：

| 维度 | 已验证范围 |
| --- | --- |
| public entry | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` |
| row source | ordered `indices_` gather |
| source 点型 | `PointXYZI`、`PointXYZINormal` |
| normal 点型 | `PointNormal`、`PointXYZINormal` |
| Scalar | `float` model coefficients；公开 API 仍输出 `std::vector<double>` distances |
| production 层级 | production-public correctness：真实公开入口对 direct RVV helper |
| 不覆盖 | 新性能摘要、公开入口计时、更多 normal-like 点型全集、`Scalar=double`、新 RVV family selection（RVV 实现族选择） |

## 动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| 扩 source builder | done | `src/test_sample_consensus_plane_models.cpp` 新增 `makeNormalPlaneSourceCloud<PointT>`，填充 `PointXYZI` / `PointXYZINormal` 额外字段。 |
| 增加交叉测试 | done | 新增 4 个 public-vs-direct RVV tests：`PointXYZI + PointNormal`、`PointXYZI + PointXYZINormal`、`PointXYZINormal + PointNormal`、`PointXYZINormal + PointXYZINormal`。 |
| 更新 target filter | done | `NORMAL_PLANE_PUBLIC_FILTER` 覆盖 13 个 public / fallback / buffer tests。 |
| 本地验证 | done | `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests`：13/13 passed；`make -C test-rvv/sample_consensus/plane_models run_test_compare`：Std/RVV 各 32/32 passed。 |
| 板卡验证 | done | `SSH_AUTH_SOCK=<injected> make -C test-rvv/sample_consensus/plane_models run_board_normal_plane_public_tests`：13/13 passed；仅出现远端 clock skew warning，不影响 correctness。 |
| freshness check | done | `evidence_status`、`repeated_evidence_status`、`phase050_evidence_status` 均为 fresh。 |

## 优化矩阵更新

| candidate family | row source | point type / Scalar / layout | correctness target | board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- |
| source-normal cross correctness | ordered `indices_` | `PointXYZI + PointNormal`, float, AoS-compatible | public-vs-direct RVV passed | board public alias passed | no new performance doctor | adopted for representative cross correctness |
| source-normal cross correctness | ordered `indices_` | `PointXYZI + PointXYZINormal`, float, AoS-compatible | public-vs-direct RVV passed | board public alias passed | no new performance doctor | adopted for representative cross correctness |
| source-normal cross correctness | ordered `indices_` | `PointXYZINormal + PointNormal`, float, AoS-compatible | public-vs-direct RVV passed | board public alias passed | no new performance doctor | adopted for representative cross correctness |
| source-normal cross correctness | ordered `indices_` | `PointXYZINormal + PointXYZINormal`, float, AoS-compatible | public-vs-direct RVV passed | board public alias passed | no new performance doctor | adopted for representative cross correctness |

## 证据分层

本阶段只产生 production-public correctness 证据。它证明真实公开入口在 4 个代表性交叉组合中会走现有 RVV dispatch（分流逻辑），且 `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 的输出与 direct RVV helper 一致。

本阶段不产生新的性能结论。Phase 030 的 `PointXYZ + Normal` helper hot-path 5-run summary 和 Phase 050 的 representative source helper hot-path 5-run summary 仍是当前性能主证据；本阶段只运行 registry freshness check，确认这些摘要和文档引用没有过期。

## Diagnostic 到 Production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production-public correctness |
| A/B boundary | public overload 对 direct RVV helper |
| 当前决策问题 | implementation-shape correctness |
| 是否可外推到 production | 可以外推 correctness，因为测试直接调用真实 public entry，并与同对象 direct RVV helper 对拍；不能外推性能。 |
| comparison-boundary / baseline mismatch 风险 | 低；比较双方共享同一个模型对象、同一组 `indices_`、source cloud 和 normal cloud。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；若失败，应修复或降级对应点型组合 gate。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；本阶段不选择新 RVV family。 |

## Evidence Doctor 和 Registry

本阶段没有新增 board performance summary，因此没有新增 Phase 070 Evidence Doctor manifest。结束时复核既有 evidence registry：

```text
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
make -C test-rvv/sample_consensus/plane_models phase050_evidence_status
```

三项输出均为 fresh；Phase 050 Evidence Doctor 仍为 Errors=0、Warnings=5、Suggestions=0，Warnings 已在 Phase 050 result 中按点型 / helper 边界解释。

## Phase Scope 与扩展队列

`validated_scope`：代表性 source × normal 交叉 correctness 已关闭，覆盖 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个组合。

`unvalidated_scope`：更多 PCL normal-like 点型（例如 `PointXYZRGBNormal`、`PointXYZLNormal`）、用户自定义点型全集、完整公开入口性能、`Scalar=double` 和新 RVV math approximation family 仍未验证。

| queue item | 状态 | 恢复条件 |
| --- | --- | --- |
| more normal-like PCL types | deferred | 生产调用点或用户要求指向更多 normal-like 点型时，补 traits audit、public direct/fallback tests、QEMU 和 board alias。 |
| public-overload performance probe | deferred | 需要评估公开入口对象状态、dispatch 和缓冲区 resize 是否改变 helper-level positive 结论时，另建 repeated board phase。 |
| `Scalar=double` | deferred | 需要新 double helper family、数值预算、asm 和 board 证据；属于实现族扩展。 |

## Continue / Stop Decision

Phase 070 closed。当前授权范围内的 high-priority correctness 扩展已经覆盖到代表性 source axis、normal axis 和二者交叉组合。剩余方向要么是更多 PCL / 自定义点型全集，要么是公开入口性能探针或 `Scalar=double` 新实现族；这些方向会扩大 scope 或需要新的收益问题定义。默认下一步是停在 Handoff，报告当前状态与可选扩展方向，而不是把窄范围结果外推成泛型 closeout。
