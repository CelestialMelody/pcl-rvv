# Optimization Evidence

| candidate family | status | evidence | decision |
| --- | --- | --- | --- |
| score-update 3x3 RVV | attempted-positive | QEMU correctness 通过；board gtest 通过；bench RVV asm 有关键 RVV 指令；board repeated median `2.560x`，checksum 一致；Doctor `0E/0W/1S` | `partial-production-candidate`，进入 Phase 010 |
| four-score-image update pipeline | attempted-positive | QEMU correctness 2/2 通过；board repeated median `2.210x`，checksum 一致；Doctor `0E/0W/1S` | `partial-production-candidate`，进入 Phase 020 |
| RangeImage fixture / generation plus update | attempted-neutral | `range_image_score_generation_160x120` 和 `range_image_generation_plus_update_160x120` board repeated median 均 `1.000x`，checksum 一致；Doctor `0E/2W/4S` | 不接 score-update-only production；进入 Phase 030 拆分 score-generation 子成本 |
| score-generation component split | attempted-neutral | `range_image_local_surface_160x120` 和 `range_image_border_scores_after_surface_160x120` board repeated median 均 `1.000x`，checksum 一致；Doctor `0E/2W/4S` | 暂停当前 topic，不进入 production integration loop |
| neighbor-distance score RVV | not_recommended_now | Phase 030 after-surface `extractBorderScoreImages()` 子边界 median `1.000x`，没有显示 `getNeighborDistanceChangeScore` 值得单独 RVV 化 | 只有未来真实 workload / profile 证明该子边界占比显著时恢复 |
| border classification state split | not_recommended_now | 需要 shadow / veil 状态 trace、输出 oracle 和 public profile；当前 score-update / score-generation 证据不能覆盖 | 不作为当前 topic 自动继续方向 |

Phase 000/010 正向只说明连续 float score-update helper 有明显收益，不说明完整 `RangeImageBorderExtractor::computeFeature()` 会同等受益。Phase 020/030 已证明该局部收益在当前 `RangeImage` production-shaped boundary 下被稀释到 neutral；当前没有值得接入 production 的 RVV 方向。
