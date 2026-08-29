# Optimization Roadmap

## 当前边界

当前 topic 已完成公共 `QuantizedMap::spreadQuantizedMap()` 的 production-detail（生产细节 helper）闭环。默认 `spreading_size == 8` 的共享 helper 已采纳为 adopted production behavior（已采纳生产行为）。非默认 spread、caller public entry（公开入口）端到端收益和其它 `QuantizedMap` helper 不属于当前 closeout 范围。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `shared-spread-rvv-2pass` | 当前源码与 surface-normal spread 经验 | `QuantizedMap::spreadQuantizedMap()` 默认 spread 8 | 已实现；helper board median `4.670x` / `4.770x` | 不证明 caller 端到端收益 | correctness、asm、board repeated、Evidence Doctor | adopted | closeout |
| `variable-spread-rvv` | 当前源码 | 非默认 `spreading_size` | 可能覆盖用户自定义 spread 半径 | 需要通用滑动窗口或多分支，维护成本更高 | caller profile、fallback tests、dedicated board | deferred | not in this topic |
| `caller-end-to-end-rvv` | Phase 000 反思 | color / color-gradient / surface-normal public entry | 可能把 shared helper 收益转成公开入口收益 | caller 还有 quantize、filter、convolution、feature extraction 成本 | public-entry correctness、asm、board repeated | separate-topic | caller topic |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | caller end-to-end 回访 | helper 本身强正向，但端到端收益仍需 caller 证据 | 公开入口 correctness、fallback、asm、board repeated | separate-topic |
| Phase 000 | 非默认 spread 泛化 | 当前 RVV gate 只覆盖 spread 8 | 真实 caller 证明非 8 是热点后再设计 | low / deferred |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `variable-spread-rvv` | 当前 production caller 主要使用默认 spread 8；非默认路径已有标量 fallback。 | profile 或用户 workload 证明非默认 `spreading_size` 是热点。 |
| `getSubMap-rvv` | 属于 `recognition/include/pcl/recognition/quantized_map.h` 容器 copy helper；DOTMOD 已通过 direct window 避免 submap 拷贝。 | 新 caller profile 证明 standalone `getSubMap()` 构造是真热点。 |
| `caller-end-to-end-rvv` | 会扩大到其它 topic 的公开入口，不能由 helper bench 直接关闭。 | 另开 `color_modality` 或 caller 回访 topic，并补 public-entry 证据。 |
