# ROPS phase index

| phase | status | role | default recovery |
| --- | --- | --- | --- |
| `000-current-state-and-component-ablation` | completed | 建立 central moments component 的 test-first scaffold；正确性成立但不作为单独 production 候选 | 已关闭；默认转入 `010-distribution-matrix-ablation` |
| `010-distribution-matrix-ablation` | completed | 验证 rotated local cloud 到 distribution matrix 的 bin index + scatter 组件；diagnostic board repeated positive | 已关闭；默认转入 `020-rotate-projection-ablation` |
| `020-rotate-projection-ablation` | completed | 验证 rotateCloud + AABB 的批量 xyz 旋转和 min/max 规约候选；diagnostic board repeated positive | 已关闭；默认转入 `030-combined-rotate-distribution-probe` |
| `030-combined-rotate-distribution-probe` | completed | 验证 rotateCloud + AABB + 三个 distribution matrix 的 production-shaped diagnostic；board repeated positive | 已关闭；默认转入 `040-production-integration-plan` |
| `040-production-integration-plan` | completed-adopted | 接入 production private helper dispatch；`PointXYZ` board repeated median `1.700x`，Doctor `0E/0W/2S` | 证据已关闭；用户确认板卡有收益即可采纳，后续由 `050-point-type-expansion` 补代表性点型 |
| `050-point-type-expansion` | completed-adopted | 扩展到 `RVVXYZAoSFloatLayout<PointInT>` traits gate；`PointXYZI` median `1.520x`、`PointNormal` median `1.590x` | 证据已关闭；默认进入 S11 production closeout |

当前默认恢复入口：S11 production closeout。用户已确认板卡正向即可采纳，production patch 视为已采纳生产行为。
`doc-rvv/features/rops_estimation-RVV.zh.md` 保存长期生产行为、fallback、证据链和后续方向。

当前 topic 内没有建议继续自动推进的高优先级算法优化 phase。完整 public workload/profile、descriptor normalization、
scatter rewrite 和 evidence metadata hardening 均需独立目标、真实输入或归档要求后再开启。
