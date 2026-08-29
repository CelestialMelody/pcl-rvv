# color_modality Optimization Roadmap

## 当前边界

当前 adopted path 只覆盖 `ColorModality<PointXYZRGB>::processInputData()` 的 organized `PointXYZRGB` 输入。RVV 接管 RGB extrema quantize 和 3x3 dominant filter，spread 使用既有 `QuantizedMap::spreadQuantizedMap()`，feature extraction 保持标量。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `cm-rgb-extrema-quantize-rvv` | 当前源码逐像素 RGB bin 计算 | `PointXYZRGB` public process input | 已把 Phase 030 约 `1.38x/1.36x` 的 handoff baseline 提升到 Phase 040 `2.490x/2.410x` | exact point type gate，不覆盖泛型 RGB traits | correctness、asm、board repeated、doctor | adopted | closed |
| `cm-filter-3x3-rvv` | 当前源码 3x3 histogram filter | quantized byte map | 已作为 Phase 040 family 的组成部分 | 单独收益被 Phase 040 替代 | 已有 correctness、asm、board | adopted | closed |
| `extractFeatures` prefilter / candidate staging | 源码中 mask、distance map、list/sort 状态机 | feature extraction | unknown | 状态机和保序语义复杂，可能不是热点 | profile 或 component ablation，再设计 correctness / board evidence | deferred with evidence | not scheduled without profile |
| `computeDistanceMap` vectorization | 源码二遍 distance transform | each bin mask map | unknown | 依赖前后行状态，简单 RVV 可能破坏递推语义或收益不稳 | profile、算法等价审计、单独 correctness | deferred with evidence | not scheduled without profile |
| generic RGB point-type traits | 模板入口可访问 `r/g/b` | non-`PointXYZRGB` RGB-like point types | unknown | PCL RGB 字段 traits、offset 和 layout gate 未审计 | traits tests、fallback tests、board per point type | rejected for current production scope | only reopen on user/caller demand |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 040 | no new high-priority candidate | Phase 040 已把 preprocess 主成本中的量化和滤波都纳入 RVV，board repeated 稳定正向；剩余热点需要 profile 证明 | `extractFeatures()` / `computeDistanceMap()` 组件消融或完整模板生成 profile | low until profile |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `extractFeatures` / `computeDistanceMap` | 当前 evidence boundary（证据边界）不包含 feature extraction；没有 profile 时继续手写 RVV 风险高、收益不可证明 | 先运行完整模板生成或 feature extraction component ablation，证明它们是主成本 |
| generic RGB traits | 当前生产代码使用 exact `PointXYZRGB` gate 保持边界清楚，其它模板实例 fallback；扩大点型会引入 traits/layout 验证成本 | 有真实 RGB-like point type 调用需求，并补 fallback/correctness/board matrix |

## 默认恢复队列

| item | status | resume condition |
| --- | --- | --- |
| production closeout docs | adopted | 本轮已完成 evaluation、phase result、doc suite 和 `doc-rvv` |
| further optimization | turn_stop_deferred with stop_condition_hit | 当前授权范围内没有无需 profile 即可继续的高价值动作 |
