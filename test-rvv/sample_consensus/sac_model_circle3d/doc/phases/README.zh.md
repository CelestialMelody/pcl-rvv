# sac_model_circle3d Phase Index

| phase | 状态 | plan | result |
| --- | --- | --- | --- |
| `000-circle3d-projection-component-ablation` | completed / diagnostic | `000-circle3d-projection-component-ablation/plan.zh.md` | `000-circle3d-projection-component-ablation/result.zh.md` |
| `010-selectwithin-production-probe` | completed / rollback-no-production | `010-selectwithin-production-probe/plan.zh.md` | `010-selectwithin-production-probe/result.zh.md` |
| `020-select-point-type-expansion` | completed / rejected-with-evidence | `020-select-point-type-expansion/plan.zh.md` | `020-select-point-type-expansion/result.zh.md` |
| `030-rollback-no-production-closeout` | completed / no-unblocked-next-action | `030-rollback-no-production-closeout/plan.zh.md` | `030-rollback-no-production-closeout/result.zh.md` |

默认恢复动作：当前 topic 已收口为 rollback/no-production。后续若重启 `selectWithinDistance`、`countWithinDistance` 或 `getDistancesToModel` 的新实现族，应先新建 phase plan，不复用已回滚的 production patch。
