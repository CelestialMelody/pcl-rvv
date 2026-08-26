# Phase 020 Result: production-integration-plan

## 实际执行范围

本阶段只完成 PI1 production integration plan（生产接入计划）。生产源码和 public API（公开接口）未修改。

## 计划动作回填

| action | status | 证据 | 结论 |
| --- | --- | --- | --- |
| 冻结候选范围 | done | `plan.zh.md#候选范围` | 候选为 `segment` 扫描段的有界 production probe，优先 `RVVXYZAoSFloatLayout<PointT>` gate |
| 冻结 fallback 矩阵 | done | `plan.zh.md#Fallback 矩阵` | 非 RVV、traits 不满足、小规模、indexed subset、concave hull 多 polygon 和不满足 setup 的路径都必须 fallback |
| 冻结生产源码形态 | done | `plan.zh.md#推荐源码形态` | 默认建议 clean split；impl-only exception 仅在用户限定 impl 文件时使用，并标结构风险 |
| 冻结生产直连证据计划 | done | `plan.zh.md#生产直连测试计划`、`plan.zh.md#生产 bench / asm / board 计划` | PI2-PI5 需要 public entry correctness、fallback、asm、board repeated 和 Evidence Doctor |
| 进入 PI2 production patch | blocked-at-authorization-boundary | 本 result | 需要用户明确授权生产源码改动范围 |

## EvidenceDecision

current_decision：`partial-production-candidate / PI2-blocked-on-user-authorization`。

Phase 010 的 full-scan production-shaped diagnostic 支持进入生产接入闭环；但 PI2 会修改 production 文件。clean split 需要同时修改类声明头 `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` 和 impl 头 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`；impl-only exception 只改 impl 头但会保留较重的公开入口主体。当前必须停在用户授权边界。

## artifact 和 evidence 状态

| area | status | 说明 |
| --- | --- | --- |
| production source | unchanged | 未触碰生产源码 |
| topic test assets | updated | full-scan reference / candidate / test / bench 已存在 |
| board evidence | current | `log/board/repeated/summary.md` median 1.98x，Evidence Doctor 0 / 0 / 0 |
| doc-rvv production topic doc | not_applicable | 无 adopted production behavior，PI5 未发生 |
| evidence registry | not_available | 当前用 manifest + manual path scan 替代 |

## next_worker_action

等待用户确认是否进入 production integration loop（生产接入闭环）：

| 选择 | 含义 | 下一步 |
| --- | --- | --- |
| clean split（推荐） | 允许同时修改类声明头和 impl 头，抽出 `segmentStd` / `segmentRvv` 或等价 helper | 进入 PI2 production patch，随后连续跑 PI3-PI5，并在 PI5 停下等待最终采纳 / 回滚确认 |
| impl-only exception | 只允许修改 impl 头，接受 public entry 中保留较重标量 fallback 的例外形态 | 进入更窄 PI2，但 Handoff 必须标出结构风险 |
| 暂不接 production | 保留当前 production-shaped diagnostic 证据 | 后续可继续做 concave hull RVV 或 arbitrary indices gather 诊断 phase |

未确认前，不修改 production。
