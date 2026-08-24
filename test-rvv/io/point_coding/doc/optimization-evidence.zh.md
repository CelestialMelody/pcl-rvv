# point_coding 优化证据

## 候选族状态

| candidate family | 当前状态 | 代码位置 | 正确性证据 | 性能 / asm 证据 | 决策 |
| --- | --- | --- | --- | --- | --- |
| scalar same-chain reference | adopted for test support | `include/impl/point_coding_support.hpp` | `make run_test_compare` | Std board baseline | 保留为 correctness / bench 基线。 |
| indexed gather + scalar quantize encode | attempted / diagnostic-positive | `encodePointsRVV` | encode gtest 通过。 | 默认 12-case board 中 encode median 1.17x 到 2.14x；asm 有 `vluxseg3ei32.v`。 | 保留为默认局部收益线索，但 tiny leaf 1/5 退化阻止 production 采纳。 |
| full f32 RVV quantize encode | attempted / rejected in Phase 000 | 未保留为最终 helper | clamp / boundary 对拍存在差 1 风险。 | 未进入 bench。 | 暂缓；必须先有输入域或误差合同。 |
| f64 exact RVV quantize encode | attempted / rejected in Phase 020 | 显式 probe | `make run_test_rvv POINT_CODING_ENABLE_F64_QUANTIZE=1` 通过。 | probe board 中 `encode_indexed_16` median 0.64x 且 5/5 退化，`encode_indexed_4096` 2/5 退化。 | 正确但太重；保留 probe，不作为默认 candidate。 |
| contiguous decode RVV | attempted / weak-positive diagnostic | `decodePointsRVV` | decode gtest 通过。 | Phase 040 decode-only 10-run median 为 1.15x 到 1.29x；Evidence Doctor 为 `Errors=0，Warnings=1`。 | 保留为诊断线索；可进入 test-only production-shaped context scout，但不直接接 production。 |
| decode context RVV | attempted / weak-positive production-shaped diagnostic | `decodePointsCandidateToCloud`、`decodePointsRVVToCloud` | object-state gtest 通过。 | Phase 050 10-run median 为 1.21x 到 1.40x；Evidence Doctor 为 `Errors=0，Warnings=7`。 | 中位数正向但有退化频率 warning；不进入 production。 |
| decode multileaf context RVV | attempted / weak-positive production-shaped diagnostic | `decodePointsCandidateMultiLeafToCloud`、`decodePointsRVVToCloudBytes` | multi-leaf object-state gtest 通过。 | Phase 055 10-run median 为 1.18x 到 1.28x；Evidence Doctor 为 `Errors=0，Warnings=3`。 | 多 leaf context 没吞掉所有收益，但小规模退化和长尾仍阻止 production-ready。 |
| public octree roundtrip feasibility | completed feasibility smoke | `runPublicOctreeRoundtrip` | public roundtrip gtest 通过。 | Phase 056 未运行 timing；`make run_test_compare` 只作为 correctness / entry-shape 证据。 | 公开入口可构造；性能结论以后续 Phase 080 public boundary 板卡结果为准。 |
| production-direct decode RVV | adopted production behavior | `io/include/pcl/compression/point_coding.h` 的 `decodePointsRVV` | production-direct rounding gtest 通过。 | Phase 060 production-direct 10-run median 为 1.22x / 1.23x / 1.18x / 1.16x；Evidence Doctor 为 `Errors=0，Warnings=1`；asm 有 f64 widen/narrow 和 strided store。 | 用户已确认采纳 exact `PointXYZ` patch；不覆盖所有点型或 public end-to-end。 |
| traits-gated production-direct decode RVV | adopted production behavior | `io/include/pcl/compression/point_coding.h` 的 generic `decodePoints` | PointXYZI / PointXYZRGB extra-field preservation gtest 通过。 | Phase 070 production-direct 10-run median 为 `1.27x / 1.20x / 1.27x / 1.19x`；Evidence Doctor 为 `Errors=0，Warnings=2`；asm 有 traits-gated `vssseg3e32.v`。 | 用户可按板卡收益采纳 traits gate；仍不覆盖全部自定义点型或完整 public end-to-end。 |
| public octree decode / roundtrip timing | attempted / weak-near-threshold | `src/bench_point_coding.cpp` 的 `octree_decode_public_*` / `octree_roundtrip_public_*` | public roundtrip gtest 通过；QEMU bench smoke 通过。 | Phase 080 public boundary 10-run median 为 `1.01x / 1.01x / 1.01x / 1.02x`；Evidence Doctor 为 `Errors=1，Warnings=2，Suggestions=4`。 | 不作为稳定 public-positive evidence；说明 direct helper 收益在完整 public 链路中被明显稀释。 |

## 关键实现取舍

### Encode

当前默认 RVV encode 只负责 indexed gather（离散加载）和 chunk 组织，量化仍回到 `encodeOneScalar`。这个选择看起来不“纯”，但它保住了 production 标量的 double 除法与 `static_cast<int>` 截断语义。Phase 020 尝试过 f64 exact quantize，correctness 成立但板卡退化频率过高，因此默认路径恢复为 scalar same-chain quantize。

下一步如果再尝试 encode 方向，必须先选择量化策略：

- f64 exact RVV 已拒绝为默认路线；只保留显式 probe。
- 对远离整数边界的输入用 f32 fast path，边界 lane 回退标量；但这需要先证明 production 输入域和 error bound。

### Decode

Decode 使用 `vlse8` 从 interleaved diff byte 中跨步加载 `x/y/z`，再以 `vsse32` 写回 `PointXYZ` AoS 字段。Phase 060 production path 为了保持 `referencePoint_arg` 的 double 舍入语义，使用 f64 vector arithmetic 后窄化到 float；Phase 070 在 traits-gated 紧凑 xyz layout 上继续沿用同一语义，并通过 `vssseg3e32.v` 写回 `PointXYZI` / `PointXYZRGB` 这类 layout-compatible 点型。当前生产取舍已经覆盖两段 production-direct board repeated。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | Phase 060 / 070 都是 production_direct；早期 Phase 040/050/055 是 diagnostic / production-shaped diagnostic。 |
| A/B boundary | 两阶段都是真实 production entry，对比只差 RVV 构建与 traits gate 命中情况。 |
| 当前决策问题 | production decode path 是否快于当前 scalar production decode path。 |
| diagnostic 是否可外推到 production | 旧 diagnostic 只作为 probe justification；最终取舍使用 Phase 060 / 070 production-direct 数据。 |
| comparison-boundary / baseline mismatch 风险 | Phase 060/070 已降到 production detail helper；Phase 080 进一步测 public entry，但结果为 near-threshold 并有 Doctor Error。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded production probe；当前证据为 positive / weak-positive。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no。当前不是 RVV family 选择，而是同一 family 的门控扩围。 |

## 继续价值

当前局部收益已经足够支撑 adoption。接下来如果继续，优先级应从“同类 synthetic bench”降到“更大的入口边界”：

1. Phase 030 已确认当前 production 源码不足以给出 encode 输入域合同，所以 encode 不应实现 f32 fast path。
2. Phase 040、050、055、056 已经把 decode 的 synthetic / shaped / public smoke 梯度补齐。
3. Phase 060 和 Phase 070 已把 exact `PointXYZ` 与 traits-gated PointXYZ-like decode 都推进到 adopted production behavior。
4. Phase 080 已补 full public octree end-to-end boundary；收益接近阈值且有退化频率 Error / Warning，不建议继续自动改 production。后续只有在真实 workload/profile 指向 point coder decode 主导时才值得重开。
