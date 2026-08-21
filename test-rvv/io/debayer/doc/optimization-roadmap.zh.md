# debayer optimization roadmap

## 当前边界

本 topic 来自 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` 中的 Bayer debayer conversion。Phase `000-current-state-and-bilinear-interior` 已证明 `debayerBilinear` full-size 内区 2x2 stencil（邻域模板）的两种 RVV（RISC-V Vector，可变向量长度向量扩展）测试专用 candidate 在板卡上稳定慢于标量，因此当前默认恢复动作不是继续 bilinear inner 微调，而是停在 no-production（不接入生产）结论。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| bilinear-inner-u8-stencil-strided-store | 当前源码主循环和队列表建议 | `debayerBilinear` full-size 内区 | 固定邻域 byte load、整数 average 和 RGB stride store 可批量化 | strided RGB store 和窄化成本可能抵消收益 | correctness、asm、板卡 bench、Evidence Doctor | rejected: board 0.94x / 0.95x | none |
| bilinear-inner-u8-stencil-segmented-store | phase 000 反思 | `debayerBilinear` full-size 内区 | 用 `vsseg6e8` 合并 RGB block 写回 | tuple setup、load/extend 成本仍可能主导 | correctness、asm、板卡 bench、Evidence Doctor | rejected: board 0.93x | none |
| edge-aware-branch-vector | 当前源码 `debayerEdgeAware` 内区 | edge-aware 内区绿色通道方向选择 | 分支可用 mask（掩码）选择表达 | `abs` 和 compare mask 成本、分支分布未知；bilinear 内区已负向 | profile 或分支分布诊断，再补 correctness / board A/B | deferred with resume condition | `010-edge-aware-profile-or-branch-distribution` |
| weighted-edge-aware | 当前源码 `debayerEdgeAwareWeighted` | weighted 内区绿色通道 | 可能复用 bilinear RGB/B 通道，单独优化 G 通道 | 除法、零梯度分支和 byte 语义风险更高；当前无正向前置证据 | component ablation（组件消融）、数值对拍、board | deferred with resume condition | `020-weighted-inner` |
| production-boundary-split | workflow phase loop | 真实 `DeBayer` 入口 | 如果诊断正向，可把边界标量 + 内区 RVV 做成 production candidate | 当前 bilinear diagnostic negative | production direct test、asm、board、用户 PI5 确认 | rejected for now | 只有新的 positive diagnostic family 才恢复。 |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| downsample-openni-bayer | 不在 `io/src/debayer.cpp`，属于 OpenNI Bayer wrapper 的下采样入口 | `debayerBilinear` / edge-aware 内区证据完成后，另开 wrapper parity phase。 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | load-count / intermediate-vector ablation | `vsseg6e8` 没有改善，说明瓶颈可能不是单纯写回 | profile、load 数量消融、减少邻域重复加载的新算法 | low; only if user wants deeper diagnosis |

## 默认恢复队列

| order | action | status | resume condition |
| ---: | --- | --- | --- |
| 1 | 维持 `debayerBilinear` bilinear inner family 的 no-production 结论。 | turn_stop_deferred with stop_condition_hit | Phase 000 correctness / asm 闭合，板卡 bucket 稳定 negative，继续 production 会扩大到证据不支持的生产补丁。 |
| 2 | 新建 `010-edge-aware-profile-or-branch-distribution`，先做 edge-aware 分支分布或 profile。 | deferred with resume condition | 用户明确要求继续 debayer topic，且接受先做 profile / component ablation（组件消融）而不是直接 production。 |
| 3 | 新建 `020-weighted-inner` weighted 候选。 | deferred with resume condition | edge-aware profile 或新候选先给出能改变收益结构的证据。 |
