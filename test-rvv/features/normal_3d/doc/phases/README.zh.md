# normal_3d 阶段索引

| phase | 状态 | 默认恢复动作 | 文档 |
| --- | --- | --- | --- |
| `000-current-state-and-common-covariance-audit` | completed | 无默认同轮下一 phase；只有真实 workload/profile 指向 `normal_3d.hpp` local loop，或用户授权 production probe 时恢复。 | `000-current-state-and-common-covariance-audit/plan.zh.md`, `000-current-state-and-common-covariance-audit/result.zh.md` |

当前停止状态：`turn_stop_deferred with stop_condition_hit`。Phase 000 已完成 QEMU correctness、反汇编、5-run 板卡 repeated 和 Evidence Doctor；component covariance RVV positive，但 public `NormalEstimation` 只有 near-threshold 弱收益，因此本 topic 不建议继续修改 `normal_3d.hpp` production 路径。
