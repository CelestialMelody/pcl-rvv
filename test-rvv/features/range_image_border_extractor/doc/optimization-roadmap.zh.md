# range_image_border_extractor Optimization Roadmap

## 当前边界

当前 topic 来自 `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` 的第 8 项。生产源码未修改。Phase 000/010 的 score-update（分数传播）局部诊断为 positive，但 Phase 020/030 的真实 `RangeImage` production-shaped diagnostic（生产形态诊断）均为 neutral；当前不建议继续 production RVV 接入。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| score-update 3x3 RVV | 当前源码 shape scan；organized edge 的 organized-grid mask 结构经验 | 四个 border score 图像的 3x3 传播 | 连续 load/store，边界可标量处理，维护成本低 | 被真实 score generation 稀释 | correctness、QEMU、asm、board bench、Evidence Doctor | attempted-positive / diagnostic-only | closed；不接 production |
| production-shaped score pipeline | Phase 000 positive 后的 mismatch audit | 四方向 score 图像 update batch | 检查单 helper 收益在四图连续处理时是否仍成立 | 仍不包含真实 RangeImage / LocalSurface score 生成 | correctness、component timing、asm、board repeated、Doctor | attempted-positive / diagnostic-only | closed；不接 production |
| RangeImage fixture / neighbor score audit | Phase 010 positive 后的 production mismatch audit | `extractBorderScoreImages` / `getNeighborDistanceChangeScore` | 判断 score-update 收益是否会被 score 生成成本稀释，并发现下一个候选热点 | `get1dPointAverage`、inf 分支、fixture 稳定性和链接库复杂 | production-shaped oracle、RangeImage fixture、component bench、board repeated、Doctor | attempted-neutral | closed；Phase 020 median `1.000x` |
| score-generation component split | Phase 020 neutral 后的瓶颈拆分 | `extractLocalSurfaceStructure()`；local surface 已存在时的 `extractBorderScoreImages()` | 判断 neighbor-score 是否仍值得单独 RVV 化 | 两个 component 都可能只剩函数调用、对象构造或缓存噪声 | QEMU smoke、board repeated、Doctor | attempted-neutral | closed；Phase 030 median 均 `1.000x` |
| border classification state split | 当前源码 `classifyBorders` / `findAndEvaluateShadowBorders` | shadow / veil / maximum state machine | 理论上可能减少分类循环开销 | 写回顺序、shadow index、指针状态和分支语义风险高；缺少 public-output oracle 和 profile 热点证据 | 先另建 profile-driven public workload / shadow-veil oracle | not_recommended_now | 无当前默认 phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000-current-state-and-score-update | production-shaped score pipeline audit | helper-level board 证据为 positive，但不能外推完整 public entry | 需要构造贴近 production 的 RangeImage fixture、四方向 score 图像和同边界 timing | high |
| 010-production-shaped-score-pipeline | RangeImage fixture / neighbor-score audit | four-score-image update 仍 positive，但 synthetic score image 不能证明真实 score 生成占比 | 需要真实 RangeImage fixture、score generation timing 和 mismatch audit | high |
| 020-range-image-fixture-and-neighbor-score | score-generation component split | RangeImage generation+update 为 neutral，需要确认是 local surface 主导还是 after-surface score generation 仍有优化空间 | local-surface-only 与 after-surface score-image timing、board repeated、Doctor | high |
| 030-score-generation-component-split | no further low-risk current-topic candidate | local surface 和 after-surface border score 两个 component median 均 `1.000x`，没有收益信号支撑生产探针 | 若未来重开，需要先有 public profile 和 shadow/veil output oracle | low / not recommended |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| production dispatch for score-update-only | Phase 020/030 production-shaped diagnostic 均为 neutral；局部 helper 收益不能穿透真实 RangeImage 边界 | 只有新的 public profile 证明 score-update 重新成为主成本，且 production-direct probe 有同边界正向证据时再恢复 |
| finite neighbor-distance score RVV | Phase 030 after-surface `extractBorderScoreImages()` median `1.000x`，说明 `getNeighborDistanceChangeScore()` 子边界没有可见收益空间 | 只有更大真实 workload 或 profile 证明该子边界占比显著时再恢复 |
| border classification state split | shadow/veil 和 maximum 状态机需要新的输出 oracle、state trace 和热点 profile；当前 score-update 证据不能授权自动扩展 | 另建 profile-driven public workload / shadow-veil oracle phase，并先证明分类状态机是主成本 |
