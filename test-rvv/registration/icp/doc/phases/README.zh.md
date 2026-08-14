# ICP phase 入口

当前 topic 使用 phase loop（阶段循环）推进。每个 phase 先写 `plan.zh.md`，实现和验证后再写
`result.zh.md`，跨阶段候选和默认恢复动作维护在 `../optimization-roadmap.zh.md`。

| phase | 状态 | 入口 |
| --- | --- | --- |
| `001-transform-cloud-diagnostic` | diagnostic_done | `001-transform-cloud-diagnostic/result.zh.md` |
| `002-board-repeated-diagnostic` | positive_done | `002-board-repeated-diagnostic/result.zh.md` |
| `003-production-integration` | production_direct_positive | `003-production-integration/result.zh.md` |
