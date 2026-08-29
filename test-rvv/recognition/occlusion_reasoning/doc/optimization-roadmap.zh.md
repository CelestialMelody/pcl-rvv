# Optimization Roadmap

## 当前边界

当前 topic 已完成 `ZBuffering<ModelT, SceneT>::filter(model, indices, thres)` 的 production
接入，并把 `computeDepthMap()` 的矩形索引修复并入同一 production patch。当前 board repeated
已经 positive，`doc-rvv` 已适用。这个 roadmap 不再承载 current adopted path 的收口，只记录
如果要另起新的证据边界，还能往哪里走。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `public-inline-filter-rvv` | public inline filter/getOccludedCloud shape | `filter(scene, model, f, threshold)` / `getOccludedCloud(scene, model, f, threshold)` | 已兑现；board positive 1.250x | 当前采用实现已经足够解释收益 | correctness、asm、board repeated、doctor | adopted | none |
| `production-projection-filter-rvv` | 当前源码 shape | `ZBuffering::filter()` 的投影、bounds mask、depth compare、indices append | 已兑现；board positive 1.270x | 当前采用实现已经足够解释收益 | correctness、asm、board repeated、doctor | adopted | none |
| `depth-gather-rvv` | 进一步把 depth compare 也向量化 | 只在另开边界时适用 | 可能进一步减少 scalar tail | 需要新的 production boundary 和 RVV-vs-RVV A/B | 同一 production boundary 的 board / asm / doctor | deferred | 另开 phase / 另开 topic |
| `smooth-window-min-rvv` | `computeDepthMap(smooth=true)` 邻域 min | 只在 caller 真会走 smooth 时适用 | 仅对 smooth 主导调用可能有收益 | 调用频率不明，不属于当前 adopted path | caller profile、独立 bench、board | deferred | 需要 caller 证据 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 020 | public inline wrapper 也属于同一模块的真实生产边界，且 board 已正向 | item 6 的 public free-function 入口证据补齐后，现可与 item 7 并列 adopted | public inline correctness、asm、board、doctor | medium |
| 010 | 当前 production path 已经包含 `vcompress` staging 和 scalar tail | board positive 后，继续细拆只会扩大证据边界 | 若要继续，必须另起新的边界 | low |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `smooth-window-min-rvv` | 当前 adopted path 已足够解释收益，smooth 不是默认所有调用都走 | 有 caller profile 或独立 board 证据显示 smooth 主导 |
| `depth-gather-rvv` | 需要新的 production boundary 和 RVV-vs-RVV 证据，不是当前 topic 的 unblocked next action | 另开 phase / topic |

## 结论

当前 topic 没有继续留在本 topic 内的高优先级未阻塞候选；如果要再做，应该另起证据边界，
而不是把 current adopted path 再包装一轮。
