# Optimization Roadmap

## 当前边界

当前 topic 已完成 `SurfaceNormalModality<PointInT>::processInputData()` 的 production direct
闭环。`computeAndQuantizeSurfaceNormals2()`、`filterQuantizedSurfaceNormals()` 和默认
`spreading_size_=8` 的 surface-normal 专用 spread 已采纳为 adopted production behavior（已采纳生产行为）。
当前 topic 内没有未阻塞生产优化动作；跨 modality 的公共 `QuantizedMap::spreadQuantizedMap()`
RVV 化需要另开范围。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `depth-quantize-rvv` | 当前源码与队列表 | `computeAndQuantizeSurfaceNormals2()` 核心内区 | 已实现；public RVV path weak-positive | 只保留余量，不再是未闭合候选 | correctness、asm、board repeated、Evidence Doctor | adopted | closeout |
| `filter-5x5-rvv` | 当前源码 | `filterQuantizedSurfaceNormals()` | 已实现；Phase 020 public entry median `1.420x` / `1.420x` | 只覆盖 `PointXYZRGBA` organized public input | correctness、asm、board repeated、Evidence Doctor | adopted | closeout |
| `snm-spread-rvv-2pass` | 当前源码 | `SurfaceNormalModality::processInputData()` 的默认 spread 8 | 已实现；Phase 030 public entry median `1.620x` / `1.640x` | 只覆盖 surface-normal 专用调用，不修改公共 helper | correctness、asm、board repeated、Evidence Doctor | adopted | closeout |
| `generic-quantized-map-spread-rvv` | Phase 030 反思 | 公共 `QuantizedMap::spreadQuantizedMap()` | 可能惠及 color / color-gradient / surface-normal modality | 影响多个调用方，当前 topic 授权和证据不足 | 跨 modality correctness、fallback、asm、board repeated | deferred / separate topic | not in this topic |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 010 | public overload 已采纳，剩余热点转向 filter / spread | production direct 弱正向已经闭合 | 新 phase plan、correctness、asm、board、Evidence Doctor | high |
| Phase 020 | filter RVV 把 public entry median 提升到 `1.420x` / `1.420x` | filter 证据正向，spread 仍在同一 public entry 计时边界内 | spread phase plan、fallback gate、correctness、asm、board、Evidence Doctor | high |
| Phase 030 | surface-normal 专用 spread RVV 把 public entry median 提升到 `1.620x` / `1.640x` | 当前 topic 内最后一个 public-entry 预处理子链路已闭合 | 若继续，另开公共 QuantizedMap spread 或 profile-first extractFeatures | separate-topic |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `extractFeatures-rvv` | list sort、distance map 和 feature selection 状态主导，当前计时边界不包含它。 | profile 证明 feature extraction 是完整 LINEMOD depth modality 的热点。 |
| `full-chain-rewrite` | 当前 public entry 已经采纳，没必要把 filter / spread 一口气并成更大修改。 | 需要证明独立阶段难以闭合且收益足够大。 |
| `generic-quantized-map-spread-rvv` | 公共 helper 会影响 color modality、color-gradient modality 和 surface-normal modality，当前 topic 不应独自采纳。 | 另开跨 modality topic，并补所有调用方的 correctness、fallback 和 board 证据。 |
