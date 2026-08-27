# range_image_border_extractor phase index

## 当前默认恢复入口

`000-current-state-and-score-update`、`010-production-shaped-score-pipeline`、`020-range-image-fixture-and-neighbor-score` 和 `030-score-generation-component-split` 已完成。当前默认恢复状态是暂停：score-update / score-generation 方向在真实 `RangeImage` production-shaped diagnostic（生产形态诊断）边界下为 neutral，不建议进入 production integration loop。

## 阶段列表

| phase | 状态 | 计划 | 结果 | 默认下一步 |
| --- | --- | --- | --- | --- |
| 000-current-state-and-score-update | completed / partial-production-candidate | `000-current-state-and-score-update/plan.zh.md` | `000-current-state-and-score-update/result.zh.md` | 进入 Phase 010 production-shaped score pipeline diagnostic |
| 010-production-shaped-score-pipeline | completed / partial-production-candidate | `010-production-shaped-score-pipeline/plan.zh.md` | `010-production-shaped-score-pipeline/result.zh.md` | 进入 Phase 020 RangeImage fixture / neighbor score diagnostic |
| 020-range-image-fixture-and-neighbor-score | completed / attempted-neutral | `020-range-image-fixture-and-neighbor-score/plan.zh.md` | `020-range-image-fixture-and-neighbor-score/result.zh.md` | 进入 Phase 030 component split，确认 score-generation 子成本 |
| 030-score-generation-component-split | completed / stop-current-topic-no-production | `030-score-generation-component-split/plan.zh.md` | `030-score-generation-component-split/result.zh.md` | 暂停当前 topic；只有未来 profile 证明 shadow/veil 状态机是主成本时再另建 phase |
