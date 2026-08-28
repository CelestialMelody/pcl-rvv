# color_gradient_modality Optimization Roadmap

## 当前边界

当前 topic 来自 recognition 函数级评估队列。`recognition/include/pcl/recognition/color_gradient_modality.h`
已经接入 `rgb-stencil-production-rvv`：它只接管 `processInputData()` 中 Gaussian convolution（高斯卷积）
之后、spread 之前的 Sobel+quantize+filter 链路；public API（公开接口）、Gaussian、spread 和
`extractFeatures()` 保持标量实现。

Phase 050 production direct（真实生产路径）板卡证据为 positive，且用户已确认板卡有收益即可采纳；
当前 production patch 视为 adopted production behavior（已采纳生产行为）。正式长期主题文档为
`doc-rvv/recognition/color_gradient_modality-RVV.zh.md`，生产数据采用 Phase 050 summary。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `sobel-selected-angle-quantize-rvv` | 当前源码 `sqrt/atan2` 热点与已有 `atan2_RVV_f32m2` | organized RGB Sobel 内区 | 减少 libm `atan2` / `sqrt` 子链路成本 | 角度近 bin 边界可能跨 bin；只覆盖 test helper | correctness、asm、board repeated、Evidence Doctor | attempted-positive diagnostic | Phase 000 completed；已被生产链路吸收 |
| `dominant-3x3-filter-rvv` | `filterQuantizedColorGradients()` 3x3 histogram | quantized map | 减少 dominant filter 逐像素成本 | histogram tie-break、`>=5` 阈值和边框语义 | correctness、bench、asm、board | attempted-positive diagnostic | Phase 010 completed；已被生产链路吸收 |
| `full-sobel-quantize-filter-rvv` | Phase 000 + Phase 010 子链路组合 | organized RGB -> filtered one-hot map | 判断组合后是否仍保持正向 | 中间 `GradientCell` / quantized map 暂存可能增加内存流量；仍缺 Gaussian/spread/public entry | correctness、QEMU smoke、asm、board repeated、doctor | superseded by production direct candidate | Phase 020 completed |
| `post-gaussian-production-rvv` | Phase 020 positive diagnostic + 用户授权 PI2-PI5 | `processInputData()` 中 Gaussian 后、spread 前内部链路 | 在不直接 RVV 读取 `PointInT` RGB 字段的前提下接入 production candidate | `atan2_RVV_f32m2` 是近似 helper；`GradientXY::angle` 只能做容差一致 | production direct tests、asm、board、doctor、用户确认 | superseded historical adopted production behavior | Phase 030 completed |
| `full-sobel-rgb-stencil-rvv` | 源码 3 通道 3x3 stencil | organized RGB Sobel 内区 | 减少标量 Sobel staging 和整图 float buffer 流量 | byte stride load、widen 和 channel tie-break 复杂；需 production direct 验证 | same-chain tests、asm、board ablation、production integration | adopted via `rgb-stencil-production-rvv` | Phase 040 + Phase 050 completed |
| `rgb-stencil-production-rvv` | Phase 040 positive diagnostic + 用户确认“接入后有收益即可采纳” | `processInputData()` 中 Gaussian 后、spread 前内部链路 | 当前 public entry 相对 scalar path 稳定加速 | 只覆盖 preprocessing；不含 `extractFeatures()` 计时；缺严格 Phase 030-vs-Phase 050 RVV detail A/B | production direct correctness、asm、board、doctor | adopted production behavior | Phase 050 completed |
| `feature-prefilter-audit` | `extractFeatures()` 先扫 mask 和 gradient magnitude | feature extraction 前置筛选 | 可能减少候选 list 构造前扫描 | list sort / distance selection 仍标量主导；当前 production_process timer 不包含 feature extraction | component profile / ablation | turn_stop_deferred with stop condition: hotspot unproven | 仅在需要覆盖完整模板生成成本时创建 Phase 060 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | `atan2` bin-stability adversarial cases | `atan2_RVV_f32m2` 为近似 helper，量化边界敏感 | near-boundary dx/dy 样本和 fallback policy | high |
| `000` | `dominant-3x3-filter-rvv` 应优先于 full Sobel stencil | Phase 000 已证明 angle/quantize 子链路有正向信号，但完整 `processInputData()` 还会立刻经过 3x3 filter；先闭合 filter 可缩小 production-shaped full-chain 风险 | filter same-chain correctness、bench、asm、board repeated、doctor | completed |
| `010` | 串接 Sobel+quantize+filter，而不是马上改 production | filter 子链路 repeated board 为 positive，但 isolated helper 不能证明组合后内存暂存成本 | full-chain correctness、asm、board repeated、doctor | completed |
| `020` | 先做 post-Gaussian production integration plan | full-chain repeated board 仍为 positive，且候选可收窄到 `pcl::RGB` smoothed input 和 `QuantizedMap` 内部 map，不必直接 RVV 读取 `PointInT` RGB 字段 | PI1 fallback / dispatch / production direct evidence plan | completed |
| `030` | production bench checksum 只覆盖离散 maps，浮点 `GradientXY` 用 gtest 容差门禁 | 首轮 production direct board 显示速度正向但 checksum mismatch；定位到 `atan2_RVV_f32m2` 近似角度被千分位哈希，不影响 quantized/spreaded map 或 feature 输出 | forced scalar/RVV gtest、刷新 board summary、Evidence Doctor | completed |
| `030` | 采纳后再考虑 Sobel RGB stencil 或 feature prefilter | production direct 已有 `1.490x` / `1.540x` 正向，继续加复杂 stencil 会扩大维护面；应先完成 PI5 决策 | 用户确认采纳后，另开 candidate A/B | completed by Phase 040 |
| `040` | RGB Sobel stencil RVV 与当前 full-chain candidate 同边界 A/B | Phase 030 已采纳，当前还存在标量 Sobel staging；test-rvv helper 能判断是否值得扩大生产实现复杂度 | path-hit correctness、asm、board repeated、Evidence Doctor | completed by Phase 050 |
| `050` | `extractFeatures()` 不应直接作为下一 production RVV patch | Phase 050 已把 color-gradient preprocessing 热点压到 positive production path；剩余方向不在当前 production_process 计时边界内 | 先补完整模板生成 profile 或 feature-only component ablation | conditional |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `production-dispatch` | 已作为 `rgb-stencil-production-rvv` 完成 PI2-PI5 并采纳 | 若后续 production 证据失效或用户要求 strict angle 语义，则重开 production integration / rollback phase |
| `Phase030-vs-Phase050-rvv-detail-ab` | Phase 050 已满足本轮“接入后板卡有收益即可采纳”策略；严格 RVV family selection 不是当前采纳门槛 | 如果 reviewer 要求证明 Phase 050 相对 Phase 030 的独立增益，而不只是相对 scalar path positive，则补同一 production boundary detail A/B |
| `feature-prefilter-audit` | 当前没有证据说明 `extractFeatures()` 在完整模板生成中仍是热点；它含 list sort、distance constraint 和保序状态，直接写 production RVV 风险高 | 有完整模板生成 profile，或用户明确要求扩展到 feature extraction 计时边界 |
