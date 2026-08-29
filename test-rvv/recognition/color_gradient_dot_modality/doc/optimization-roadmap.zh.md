# color_gradient_dot_modality Optimization Roadmap

## 当前边界

当前 topic 来自 recognition 函数级评估队列。目标源码 `color_gradient_dot_modality.h` 为 DOTMOD 输入图像
生成 bin-level dominant quantized map（主方向量化图）。本轮先覆盖 `processInputData()` 里的
`computeMaxColorGradients()` 和 `computeDominantQuantizedGradients()`，不覆盖真实 DOTMOD 滑窗匹配，
也不覆盖 `computeInvariantQuantizedMap()` 的状态恢复路径。当前 production direct 已稳定采纳，后续不再
扩展算法优化候选；本路线图只保留已验证 / 暂缓路线和恢复条件。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `cgdm-gradient-dominant-rvv` | 当前源码 RGB 差分、最大通道选择、`sqrt` / `atan2` 和 dominant map 选择 | `processInputData()` 预处理链 | 减少 organized RGB 图像逐像素预处理成本 | 近似 `atan2`、strict `>` tie-break、bin 边界、输出 bit 语义 | correctness、QEMU smoke、asm、board repeated、Evidence Doctor | adopted production behavior | closed |
| `cgdm-gradient-only-rvv` | 拆出 `computeMaxColorGradients()` 消融 | gradient map 生成 | 若 full chain 弱，可判断 dominant bin scan 是否稀释收益 | 只覆盖中间态，不证明最终 map 收益 | component bench / asm / board | superseded | not needed; production direct already positive |
| `cgdm-invariant-map-rvv` | `computeInvariantQuantizedMap()` 多次局部最大搜索 | template invariant map | 可能减少 createAndAddTemplate 阶段的局部搜索成本 | 会临时改写 `color_gradients_` 并恢复，状态语义复杂 | state restoration tests、bench、asm、board | deferred | reopen only with template creation profile |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | 先测 full preprocessing diagnostic，而不是只测 gradient-only | 队列要求本文件单独记录 dominant map correctness 与收益，full map 输出更接近 DOTMOD 输入 | same-chain correctness、bench、asm、board | high |
| `010` | 把 RVV gradient helper 接入 production public entry，dominant bin scan 继续标量 | Phase 000 helper median `3.080x` / `3.990x`，且 public entry neutral 是因为尚未接 production | production direct correctness、asm、5-run board、Evidence Doctor | done |
| `020` | doc suite closeout 与 evidence freshness 修复 | adopted production behavior 已进入提交前检查，必须把 topic-local 文档和 registry 关口补齐 | README / evaluation / role docs / freshness check | closeout |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `dotmod-template-matching-rvv` | 属于 `recognition/src/dotmod.cpp` 主题，不静默并入当前 modality 文件 | 当前 modality closeout 后按队列表第 4 项或相邻 topic 继续 |
| `cgdm-invariant-map-rvv` | 当前 production direct 已稳定 positive；该路径状态恢复风险高且没有 profile 证明仍是热点 | template creation profile 指向 `computeInvariantQuantizedMap()` |

## 关闭说明

本 topic 目前没有新的可推进算法候选；Phase 020 只处理文档套件、registry 和 commit readiness。
