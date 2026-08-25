# approximate_progressive_morphological_filter 优化路线图

## 当前边界

当前 topic 已完成 production integration loop（生产接入闭环）并进入 adopted production behavior（已采用生产行为）。`ApproximateProgressiveMorphologicalFilter<PointT>::extract` 在 `__RVV10__` 构建中对满足 `RVVXYZAoSFloatLayout<PointT>` 的 xyz AoS 点类型尝试 RVV，失败时回到 `extractStd` 标量路径。当前生产形态是 grid z-min RVV + window-open 标量 + threshold tail 的 RVV indexed xyz gather / row-col staging + 标量 `Zf` lookup 与输出更新。

接入后的 production public board（真实公开入口板卡）证据已经覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 的 dense / non-dense label，全部为 positive，Evidence Doctor（证据体检）为 Errors=0 / Warnings=0 / Suggestions=0。050 额外尝试把 tail 剩余比较 / 输出压缩也向量化，结果相对 040 adopted RVV baseline 整体中性，已回退。

默认恢复队列：当前没有值得自动推进的高优先级生产优化候选。继续扩大到更多 `PCL_XYZ_POINT_TYPES`、OpenMP 多线程或真实业务数据分布属于更宽验证 / profiling（性能剖析）范围，需要用户或 reviewer 明确选择。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `z-min-index-staging` | segmentation 队列 + 当前源码 | full input cloud 到 grid z-min | 批量 floor、cell id 和 z 读取减少前置循环成本 | 同 cell min 更新仍是标量随机写；OpenMP 与 RVV 分工不明 | same-chain test、component bench、asm、board repeated、Evidence Doctor | adopted through production hybrid | closed |
| `window-open-row-reduction` | 当前源码窗口 min/max open | synthetic Eigen column-major grid | 对列内连续 row 区间做 RVV load，降低窗口扫描成本 | component board 中性 / 负向，可能增加内存流量和边界处理成本 | grid same-chain test、half-size bench、asm、board repeated、Evidence Doctor | rejected for production | no default; only reopen with new profile |
| `tail-compress-indices` | segmentation 队列 tail-compress 建议 | current ground vector + filtered grid lookup | indexed xyz gather 和 row/col staging 减少尾段读取 / 坐标计算成本 | `Zf` lookup 不连续，最终输出仍需保序 | same-chain test、tail case bench、asm、board repeated、Evidence Doctor | adopted as 040 production tail gather/staging | closed |
| `full-pipeline-rvv-components` | phase 000 反思 | full input cloud + 多轮 window open + current ground vector | 组合链路在 production-shaped diagnostic 中保持正向 | diagnostic 不能直接替代 public overload | full same-chain test、full bench、asm、5-run board、Evidence Doctor | closed by 030 production integration | closed |
| `production-clean-split` | phase 010 结果 + phase 030 production evidence | `extract` public entry；generic `PointT` xyz float AoS gate | production public `PointXYZ` 1.60x / 1.35x，040 measured point types 全部 positive | 未证明所有自定义点型、多线程或业务数据分布 | production direct tests、asm、5-run board、Evidence Doctor | adopted | closed |
| `point-type-production-expansion` | phase 030 范围反思 + phase 040 | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` | 减少“泛型 gate 但只量 PointXYZ”的审查风险 | 其它 PCL_XYZ_POINT_TYPES 不自动外推 | public correctness、production bench、asm、repeated board、Evidence Doctor | adopted within measured point types | no default; wider validation only by explicit choice |
| `tail-vector-filter-compress` | phase 050 反思 | production `thresholdGroundRVV` 剩余比较 / 输出更新 | 期望减少 per-lane branch / push_back | 增加 `diffs` 和 `kept_indices` staging；收益被 `Zf` lookup 和内存流量抵消 | final correctness、asm、RVV-vs-RVV board A/B、Evidence Doctor | rejected and reverted | closed |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | `full-pipeline-rvv-components` | component 证据显示 z-min 和 tail-compress 正向、window-open 中性，必须验证组合链路 | full same-chain、asm、5-run board、Evidence Doctor | completed |
| `010` | `production-clean-split` | full-pipeline production-shaped diagnostic 为 positive，值得准备生产接入计划 | 用户授权、production direct correctness、fallback、asm、board、Evidence Doctor | completed |
| `030` | `point-type-production-expansion` | production public `PointXYZ` 证据 positive，但 traits-gated production helper 可覆盖更多 xyz AoS 点型 | 每个新增点型的 public correctness、bench、asm、board 和 Evidence Doctor | completed for measured types |
| `050` | `tail-vector-filter-compress` | 当前 tail 仍有 per-lane scalar filter，值得用生产探针验证是否有剩余收益 | 同边界 040 vs 050 RVV-vs-RVV A/B | rejected |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `window-open-row-reduction` | component 和完整链路证据不支持接入；当前 production public 不依赖它也已 positive。 | 新 profile 证明 window-open 成为主瓶颈，或用户要求重开同边界 A/B。 |
| `tail-vector-filter-compress` | 5-run 板卡 A/B 相对 040 基线整体 neutral，未达到本阶段采纳阈值；已回退生产改动。 | 新输入分布显示 tail 输出比例显著不同，或能避免额外 staging 后再重开。 |
| other `PCL_XYZ_POINT_TYPES` expansion | 当前已测常用 xyz AoS 点型均 positive；继续扩展是覆盖验证，不是当前证据下新的生产优化方式。 | 用户或 reviewer 明确要求完整点型覆盖矩阵。 |
| OpenMP + RVV | 当前 production bench 使用单线程，OpenMP 组合会改变调度边界。 | 有多线程真实场景 profile 或明确要求后另建 phase。 |
