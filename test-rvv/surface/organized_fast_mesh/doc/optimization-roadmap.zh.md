# Optimization Roadmap

## 当前边界

当前 topic 只覆盖 `PointXYZ`、`float`、organized cloud、`triangle_pixel_size=1`、`storeShadowedFaces(true)`。
生产公开入口接入后，board evidence 当前不支持采纳。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| finite-cache scan | 当前源码 shape scan | valid mask hot path | 减少重复 `isFinite` | production fallback 未闭合 | correctness, bench, asm | partial-production-candidate | `PI1-production-integration-plan` |
| adaptive diagonal preference | 当前源码 shape scan | adaptive_cut | 将 z 差值批量化 | production fallback 未闭合 | correctness, bench, board | partial-production-candidate | `PI1-production-integration-plan` |
| production-output-shape | production direct 负向归因 | adaptive_cut public path | 消除 `push_back` 与标量预分配写回的 baseline mismatch | QEMU smoke 仍负向，板卡暂时不可达 | board public rerun, Evidence Doctor | attempted / board-blocked | `020-production-output-shape-rerun` |
| shadow gate split | 当前源码 shape scan | shadowed faces | 需要单独证据边界 | 额外状态复杂 | diagnostic, board | deferred | `shadow-gate-diagnostic` |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000-current-state-and-gaps | finite cache + adaptive cut split | 先证明规则扫描是否值得 | test, qemu, board, asm | high |
| 000-current-state-and-gaps | PI1 scoped production plan | 单次 board diagnostic 正向，但生产源码未接入 | fallback, production direct, repeated board | high |
| 020-production-output-shape-rerun | production helper 同构输出写回 | public path 负向暴露 `push_back` 与原标量 `resize` + `idx` 写回不一致 | board public rerun, doctor | medium |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| shadow edge RVV | 语义边界更宽 | 先完成单独 diagnostic |
| 泛型点类型扩展 | 当前只收窄到 PointXYZ | 先补 traits / fallback 证据 |
| 扩大 production RVV 接入到 fixed cut / quad | public path 负向或近似 1.0x，维护成本高于当前收益 | 只有 adaptive-cut 同构输出在板卡明确正向后才恢复 |
