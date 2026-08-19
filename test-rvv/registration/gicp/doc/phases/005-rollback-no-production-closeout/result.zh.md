# Phase 005 Result: rollback no-production closeout

## 当前状态

本阶段已按用户确认执行 no-production（不接入生产）收口。GICP 生产源码已回到标量实现：
`registration/include/pcl/registration/gicp.h` 和
`registration/include/pcl/registration/impl/gicp.hpp` 对当前基线没有 diff。此前的
`dfddfLoopRVV()`、`__RVV10__` dispatch（分流逻辑）、RVV point load include 和 reduction helper
均已从生产源码移除。

长期生产文档 `doc-rvv/registration/gicp-RVV.zh.md` 已删除，因为当前没有 adopted production
behavior（已采纳生产行为）。诊断、production probe（生产探针）和回退原因保留在
`test-rvv/registration/gicp` 的 topic-local 文档中。

## 已完成动作

| action | evidence | result |
| --- | --- | --- |
| 回退生产源码 | `git diff -- registration/include/pcl/registration/gicp.h registration/include/pcl/registration/impl/gicp.hpp` | 无输出，生产源码零 diff |
| 删除长期生产文档 | `doc-rvv/registration/gicp-RVV.zh.md` | no-production 下不再适用 |
| 同步入口文档 | `README.zh.md`、`doc/gicp-evaluation.zh.md` | 当前结论改为 no-production |
| 同步证据和路线图 | `doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | Phase 003/004 作为回退决策证据保留 |

## EvidenceDecision

最终结论是 `no-production / rollback / topic-closeout-ready`。

主要依据：

- cost-only production probe 为 neutral：1024 点 median `1.019x`，4096 点 median `1.011x`。
- `dfddfLoopRVV()` clean production probe 只有弱正向：1024 点 median `1.080x`，4096 点 median `1.058x`。
- Phase 004 的 gather width 微调没有改善：1024 点 median `1.073x`，低于 Phase 003。
- 当前收益不足以抵消生产源码维护成本和窄覆盖范围；覆盖范围仅为 `PointXYZ -> PointXYZ`、`Scalar=float`、xyz AoS layout。

## 后续状态

当前 topic 可以结束。若未来重新打开 GICP，建议只作为新的 bounded phase（有界阶段）处理：
优先重新评估 covariance post-KNN 是否能穿透完整 public entry，或在有明确阈值时重新审计
`dfddf()`，但不要从本轮弱正向结果直接恢复生产接入。
