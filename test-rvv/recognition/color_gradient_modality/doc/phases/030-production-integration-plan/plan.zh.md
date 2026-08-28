# Phase 030: production integration plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划）。它只冻结将来 production patch
的候选范围、fallback（回退路径）、dispatch（分流逻辑）和证据计划；除非用户明确授权继续
production integration loop，本阶段不修改 `recognition/include/pcl/recognition/color_gradient_modality.h`。

## 候选范围

候选生产范围收窄到 `processInputData()` 中 Gaussian convolution 之后、
`QuantizedMap::spreadQuantizedMap()` 之前的内部链路：

1. `computeMaxColorGradientsSobel(smoothed_input_)`
2. `quantizeColorGradients()`
3. `filterQuantizedColorGradients()`

保守设计是抽出一个 production-local helper，在 `__RVV10__` 构建且输入 organized、宽高至少为 3、
内部 `pcl::RGB` cloud 连续可读时，尝试 RVV 子链路；其它条件继续走原标量 helper。该范围避开
`PointInT` 泛型点类型的 `r/g/b` 读取，因为 production 入口已经先复制成 `pcl::RGB` 并由 Gaussian 输出
`smoothed_input_`。

## 不覆盖范围

- 不改 public API（公开接口）。
- 不改变 Gaussian kernel、convolution border policy、spread 或 feature extraction。
- 不证明 `PointInT` 的泛型 RGB field load（字段加载）可由 RVV 直接读取。
- 不接入 `processInputDataFromFiltered()`；该入口只执行 spread，和当前候选无直接关系。
- 不创建 `doc-rvv/recognition/color_gradient_modality-RVV.zh.md`，除非 PI5 通过且用户确认采纳。

## fallback 和 dispatch 计划

| gate | RVV 条件 | fallback 行为 | 需要的测试 |
| --- | --- | --- | --- |
| compile-time | `__RVV10__` 启用 | 非 RVV 构建调用原标量 helpers | Std build production direct test |
| image shape | `width >= 3 && height >= 3` 且 organized | 小图/空图保持原标量语义 | 小图 fallback correctness |
| internal storage | `smoothed_input_` 和 quantized / filtered maps 可按 contiguous row 访问 | 若无法证明连续，保持原 helper | production direct shape smoke |
| numerical semantics | `atan2_RVV_f32m2` 量化结果与标量 same-chain 一致 | near-boundary 风险时回退或保留标量 quantize | near-boundary adversarial correctness |
| production evidence | production direct correctness、asm、board repeated 均闭合 | 保持 diagnostic-only，不采纳 | PI4/PI5 evidence |

## 证据计划

| step | command / artifact | 完成判据 |
| --- | --- | --- |
| PI2 production patch | 修改 `color_gradient_modality.h`，抽出 Std helper 并添加 RVV helper / dispatch | public API 不变，非 RVV 构建保留原路径 |
| PI3 production direct tests | 新增或扩展 topic test，真实实例化 `ColorGradientModality` 并命中 `processInputData()` | Std/RVV correctness、fallback gate 和 path-hit 都可失败 |
| PI4 asm | 新增 production asm attribution target | RVV 指令归属到 production helper 或可解释的 inlined boundary |
| PI4 board | production direct repeated board | target hardware 上 checksum 一致，decision bucket 稳定 |
| PI5 EvidenceDecision | Evidence Doctor / registry / result | 无未处理 Error；Warning 有解释；停在用户确认采纳或回滚 |

## 继续 / 停止条件

若用户明确授权“推进 production integration loop”或等价目标，下一轮可从本 plan 进入 PI2。
若 production patch 需要扩大到 public API、直接 RVV 读取 `PointInT` RGB 字段、改 Gaussian / spread
语义，或无法隔离 fallback gate，必须暂停并重写 PI1。

`next_worker_action`: 等待用户授权后，从 PI2 production patch 开始。
