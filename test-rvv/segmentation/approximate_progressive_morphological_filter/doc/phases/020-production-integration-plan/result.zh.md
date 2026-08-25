# 020 production-integration-plan 结果

## 实际执行范围

本阶段只完成 PI1 production integration plan（生产接入计划）。生产源码和 public API（公开接口）未修改。

## 计划动作回填

| action | status | 证据 | 结论 |
| --- | --- | --- | --- |
| 冻结候选范围 | done | `plan.zh.md` 的“候选范围” | 候选为 `extract` 的有界 production probe，优先泛型 xyz AoS traits gate，必要时收窄到 `PointXYZ` |
| 冻结 fallback 矩阵 | done | `plan.zh.md` 的“fallback 矩阵” | 非 RVV、traits 不满足、小规模、offset 溢出、non-dense 和 window-open 弱收益都必须有回退或测试 |
| 冻结源码形态选择 | blocked-at-user-authorization | `plan.zh.md` 的“生产源码形态选择” | clean split 需要触碰类声明头；impl-only exception 需要 reviewer 接受例外 |
| 进入 PI2 production patch | blocked | 本文件 | 需要用户明确确认 production scope 后才能修改 production |

## EvidenceDecision

current_decision：`partial-production-candidate / PI2-blocked-on-user-authorization`。

phase 010 的 production-shaped diagnostic 已支持进入生产接入计划；但 PI2 会修改 production 文件，且推荐的 clean split 可能同时触碰 `segmentation/include/pcl/segmentation/approximate_progressive_morphological_filter.h` 与 `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp`。当前用户只点名 impl 头优化 topic，未显式确认生产改动范围，因此本阶段在 PI1 后停止。

## next_worker_action

等待用户确认以下二选一后继续：

| 选择 | 含义 | 下一步 |
| --- | --- | --- |
| clean split | 允许同时修改类声明头和 impl 头，抽出 `extractStd` / `extractRVV` 或等价 helper | 进入 PI2 production patch，随后连续跑 PI3-PI5 并在 PI5 停下等待最终采纳 / 回滚确认 |
| impl-only exception | 只允许修改 impl 头，接受 public entry 中保留大段标量 fallback 的例外形态 | 进入更窄 PI2，但必须在 Handoff 中标出结构风险 |

未确认前，不修改 production。
