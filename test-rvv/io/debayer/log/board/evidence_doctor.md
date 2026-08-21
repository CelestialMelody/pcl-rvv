# debayer phase 000 Evidence Doctor

## 输入

| item | value |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断） |
| A/B boundary | test helper Std/RVV，`debayerBilinear` full-size 内区 |
| board | Milkv-Jupiter |
| case | `bilinear_inner_640x480` |
| run budget | 初始 1 组；因接近 1.0 追加 1 组；尝试 strided-store 和 segmented-store 两个 code shape |

## Errors / Warnings / Suggestions

| severity | signal | observed pattern | action |
| --- | --- | --- | --- |
| Error | none | checksum 一致；Std/RVV 均输出 `6572528553309442793` | 证据可用于当前 diagnostic 边界。 |
| Warning | negative speedup | strided-store RVV 为 0.94x / 0.95x；segmented-store RVV 为 0.93x | 当前 candidate family 不支持 production probe。 |
| Warning | metadata_incomplete | board logs 由远端 target 打印，当前本地没有 topic-local manifest；analyzer 也提示缺少 Dataset 行 | 本阶段采用人工 Evidence Doctor 摘要；后续若继续本 topic，应补 topic-local manifest / fetch wrapper。 |
| Suggestion | implementation-shape follow-up | `vsseg6e8` 没有改善，说明写回合并不是唯一瓶颈，load 数量和 u8->u16 扩展也可能主导 | 不继续在同一 inner bilinear family 上微调；恢复条件是 profile 或新候选能减少邻域 load / 中间向量数量。 |

## 结论边界

这份 Evidence Doctor（证据体检）只覆盖 `debayerBilinear` full-size 内区 test-only diagnostic（测试专用诊断）。它不能证明 `debayerEdgeAware`、`debayerEdgeAwareWeighted` 或未来完全不同的生产实现族没有价值，也不能替代 production direct（真实生产路径证据）。
