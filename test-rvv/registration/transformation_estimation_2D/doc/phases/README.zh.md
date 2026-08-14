# transformation_estimation_2D Phase Loop

本目录记录 `test-rvv/registration/transformation_estimation_2D` 的可恢复优化阶段。阶段文档保存计划、执行事实、Evidence Doctor（证据体检）和继续 / 停止决策；函数级评估主归属是 `doc/transformation_estimation_2D-evaluation.zh.md`。

## 当前恢复入口

当前 topic 的窄范围 production loop 已完成证据闭环；Phase 030 的 row-source candidate 也完成了板卡 repeated，但仍因 64K 稳定性问题保持 test-only。默认恢复动作是 `060-production-candidate-review-and-row-source-boundaries`。

| phase | 状态 | 默认恢复动作 |
| --- | --- | --- |
| `000-current-state-and-gaps` | done | 已建立源码 shape scan、evaluation、roadmap、matrix 和初始 Handoff。 |
| `010-scaffold-and-ordered-cloud-pair-correlation-diagnostic` | done | 已创建 test / bench scaffold，完成 ordered-cloud-pair correctness、QEMU smoke、asm input 和 doc suite。 |
| `020-board-and-asm-evidence` | done | 已生成正式 asm attribution、QEMU / board Evidence Doctor、board repeated summary 和 registry 记录。 |
| `040-production-integration-plan` | done / archived | PI1 曾冻结 exact `PointXYZ -> PointXYZ`、`Scalar=float`、ordered-cloud-pair 生产候选范围；当前恢复入口由 Phase 050 决定。 |
| `050-pi2-production-patch-and-direct-evidence` | done / current evidence refreshed | PI2-PI4 production-public probe 已完成；最新板卡为 4.222x / 5.310x / 4.947x，Doctor 0/0/0，窄范围 production patch 保留待审阅。 |
| `030-row-source-family-carryover` | done / diagnostic only | 三类 materialize-to-ordered candidate 的 Std/RVV 16/16 correctness、9-case QEMU、asm 和板卡 repeated 已完成；Doctor 1/2/6，64K 稳定性不足。 |
| `060-production-candidate-review-and-row-source-boundaries` | planned | 审阅当前 production diff/fallback，并决定 row-source 是否需要新的 gather/staging 候选。 |

## 文档归属

| 信息 | 主归属 |
| --- | --- |
| 函数级评估、公开入口、标量路径、RVV 候选和生产边界 | `../transformation_estimation_2D-evaluation.zh.md` |
| 测试入口分类和覆盖矩阵 | `../testing-overview.zh.md` |
| correctness tests 逐项说明 | `../correctness-tests.zh.md` |
| bench、QEMU、board、asm 和证据提交边界 | `../benchmark-and-evidence.zh.md` |
| 跨阶段 candidate family、默认恢复队列和暂缓 / 拒绝路线 | `../optimization-roadmap.zh.md` |
| 候选 × row source × 证据状态 | `optimization-matrix.zh.md` |
| 本地 handoff（交接包） | `../../../../tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.zh.md` |

## Evidence Registry

当前 topic 已本地生成 evidence registry（证据登记表）：

```text
test-rvv/registration/transformation_estimation_2D/log/evidence_registry.json
```

该文件位于本地生成的 `log/` 下，默认不提交。恢复或提交前使用：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

## 当前早停规则

`ready_for_review` 只表示当前窄范围 production candidate 的证据可审阅，不表示 indexed / correspondence 已获准接入。

`next_phase_default` 是 `060-production-candidate-review-and-row-source-boundaries`。QEMU timing 不能替代 board performance evidence；row-source 的 board Doctor 异常也不能被 median 小幅正向掩盖。
