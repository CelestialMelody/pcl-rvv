# Phase 020 Plan: cluster-growth-production-probe

## 阶段意图和边界

本阶段不再重复 pairwise predicate 的局部结论，而是把 `clusterGrowthCandidate()`
这类 outer growth 形态作为新的板卡探针，确认它是否只是“同一 helper 在更宽控制流里
继续正向”，还是足以导向下一条真正的 production-direct growth phase。
范围仍不包含 `std::sort`、RANSAC rejector、transformations 输出顺序或公开 API 变更。

## 当前状态清单

| 项目 | 当前事实 | 路径 |
| --- | --- | --- |
| adopted production patch | pairwise predicate 已采纳为窄范围 production RVV | `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` |
| previous growth probe | 当前会话已经跑过 growth-mode repeated board，结果正向，但还未用独立 target 正式登记 | `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/` |
| board availability | board 可达，且 `SSH_AUTH_SOCK` 注入后正常跑通 | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh` |
| production state | 仍停留在窄范围 adopted patch；没有新 production helper 进入源码 | 当前源码 |

## 假设与候选族

| candidate family | 假设 | 风险 | 本阶段状态 |
| --- | --- | --- | --- |
| `cluster-growth-rvv` | outer growth 形态在更宽控制流里仍能保持正向 | `taken_corresps`、顺序敏感和边界登记更复杂 | planned |
| `production patch` | 当前不应扩大到新的公开入口或更宽泛型结论 | 需要新的 board / bench / doctor 边界 | rejected for this phase |

## 诊断到 production 错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic / production-shaped diagnostic |
| A/B boundary | growth helper vs scalar reference |
| 当前决策问题 | RVV-vs-scalar growth probe |
| diagnostic 是否可外推到 production | unknown；当前只证明 growth 形态继续正向 |
| comparison-boundary / baseline mismatch 风险 | yes；需要独立 growth target，不能只复用 pairwise summary |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no；先把这次 growth probe 体检清楚 |
| clean adoption 是否需要同一 production boundary 内 A/B | yes |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cluster-growth-rvv` | correspondence-pair | `PointXYZ / float / AoS` growth helper probe | full cluster growth benchmark shape | scalar reference vs candidate | growth-mode `bench_gc` / board repeated | current session positive, needs dedicated registration | RVV gather / sqrt / compare + growth control flow | planned | planned | add dedicated growth board target and refresh evidence |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| growth board target | `board_repeated_growth` | 产生独立 growth summary / manifest / doctor |
| growth evidence registration | `record_evidence_state_growth` | registry 能把 growth probe 登记成 fresh |
| phase docs | `plan.zh.md` / `result.zh.md` | 结果能解释增长形态与 production boundary 的关系 |
| doc refresh | README / roadmap / matrix / evaluation / handoff | 让后续恢复的人知道这次 growth probe 已经试过 |

## 板卡复跑预算和决策桶

默认 5-run。`median >= 1.20x` 且 `B/A < 1 = 0` 视为 positive；`1.05x~1.20x`
为 weak_positive；`0.95x~1.05x` 为 neutral；`< 0.95x` 为 negative；预算内摇摆
为 unstable。

## 继续 / 停止条件

只要 growth probe 还没有自己的板卡登记和 phase 结果，就继续把它闭环。若最终仍不能
导出新的 production boundary，只把它记为 deferred，不再硬拽成 production patch。

## 文档更新清单

本阶段更新 README、testing-overview、benchmark-and-evidence、optimization-evidence、
optimization-roadmap、optimization matrix、evaluation 和 handoff。若 growth probe 最终
仍只是 diagnostic，`doc-rvv` 不扩大。
