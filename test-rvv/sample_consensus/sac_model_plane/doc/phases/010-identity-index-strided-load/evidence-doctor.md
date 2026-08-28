# Phase 010 Evidence Doctor 摘要

本文保存 Phase 010 可提交的 Evidence Doctor（证据体检）摘要。完整机器可读输入位于
`log/board/phase-010/*/repeated/evidence_manifest.json`；raw run logs 默认本机保留，不作为默认提交内容。

| evidence batch | summary | doctor result | 结论影响 |
| --- | --- | --- | --- |
| identity-stride-select-count | `log/board/phase-010/identity-stride-select-count/repeated/summary.md` | Errors=0, Warnings=1, Suggestions=6 | Warning 只属于未采纳的 `getDistancesToModel` identity 分支；select/count 窄采纳不降级。 |
| shuffled-stride-select-count | `log/board/phase-010/shuffled-stride-select-count/repeated/summary.md` | Errors=0, Warnings=0, Suggestions=6 | shuffled fallback 无阻塞异常，支持 select/count 保留 gather fallback。 |

## Warning 处理

identity batch 的唯一 Warning 是：

- `long_tail_or_variance — sac_model_plane getDistancesToModel PointXYZ 65536`：
  min=2.0321x、median=2.1547x、max=2.6258x，max/min 约 1.29。

处理结论：`getDistancesToModelRVV` 当前没有采纳 identity strided load，生产代码只保留 gather。
该 Warning 说明 getDistances identity 分支不适合作为 clean adoption（干净采纳）证据；它不阻塞
`selectWithinDistanceRVV` 和 `countWithinDistanceRVV` 的 Phase 010 窄采纳。

## Suggestions 处理

两个 batch 都提示缺少 taskset、governor、freq、temperature 和 binary hash（等价二进制身份）。
这些字段缺失会削弱对长尾和 run-to-run 波动的解释能力，但当前 select/count 的 decision bucket
稳定为 positive。后续若准备提交 evidence logs 或重开 getDistances identity 分支，应先补环境
metadata（元数据）和二进制身份。
