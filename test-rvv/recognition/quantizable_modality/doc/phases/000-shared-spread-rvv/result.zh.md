# Phase 000 Result: shared-spread-rvv

## 当前状态

本阶段把公共 `QuantizedMap::spreadQuantizedMap()` 的默认 `spreading_size == 8` 路径推进到 production-detail（生产细节 helper）证据边界。当前 production helper 在 RVV 构建中优先尝试 `spreadQuantizedMapRVV()`，失败时自然回到 `spreadQuantizedMapStd()`。

本轮用户已授权：接入后板卡结果如果显示有收益即可采纳。当前 repeated board（重复板卡性能测试）结果为 positive，因此该 production patch 在本阶段范围内视为 adopted production behavior（已采纳生产行为）。

## 计划与实际

| action | status | evidence | note |
| --- | --- | --- | --- |
| scalar helper extraction | done | `recognition/src/quantizable_modality.cpp` | 原两遍标量 OR 主体抽成 `spreadQuantizedMapStd()`。 |
| RVV helper | done | `recognition/src/quantizable_modality.cpp` | 默认 spread 8、尺寸足够时按 VL chunk 做横向和纵向 byte OR。 |
| fallback gate | done | `test-rvv/recognition/quantizable_modality/src/test_qm.cpp` | 非默认 spread 5 回退标量；测试 hook 可强制 scalar/RVV 对拍。 |
| correctness | done | `make -C test-rvv/recognition/quantizable_modality run_test_compare` | Std/RVV gtest 3/3，checksum 和逐 byte 输出一致。 |
| asm attribution（反汇编归属） | done | `make -C test-rvv/recognition/quantizable_modality check_qm_rvv_asm` | RVV asm 命中 `vle8`、`vor`、`vse8`。 |
| board repeated | done | `log/board/repeated_phase000_shared_spread_rvv/summary.md` | 5-run，两个 helper case median `4.670x` / `4.770x`，`B/A < 1 = 0/5`。 |
| Evidence Doctor（证据体检） | done | `log/board/repeated_phase000_shared_spread_rvv/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4`。 |
| evidence registry（证据登记表） | done | `log/evidence_registry.json` | summary / manifest / doctor 已登记为 `fresh`。 |

## 证据解释

| case | 入口与规模 | median speedup | range | 退化频率 | 结论 |
| --- | --- | ---: | --- | --- | --- |
| `shared_spread_320x240` | `QuantizedMap::spreadQuantizedMap()`，320x240 byte map | `4.670x` | `4.510x` - `5.070x` | `0/5` | positive |
| `shared_spread_641x481_tail` | `QuantizedMap::spreadQuantizedMap()`，641x481 tail | `4.770x` | `4.340x` - `4.790x` | `0/5` | positive |

checksum（校验和）在 Std/RVV 两侧一致。计时边界只包含公共 helper 的两遍 spread，不包含任何 caller 的 quantize、filter、convolution（卷积）或 feature extraction（特征提取）。因此这批证据能证明公共 helper 本身的 RVV 路径显著快于标量路径，不能单独证明三个 modality 公开入口的端到端收益。

Evidence Doctor 的 4 个 Suggestion 是环境 metadata（环境元数据）和 binary identity（二进制身份）缺失：缺少 taskset、governor、freq、temperature 和 binary hash。它们不阻塞当前 positive 结论，因为两个 case 均无 Error / Warning、checksum 一致且没有 B/A 反向样本。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_detail` |
| A/B boundary | `production_detail_helper`，Std/RVV 都直接调用 `QuantizedMap::spreadQuantizedMap()` |
| 当前决策问题 | 当前 helper RVV path 是否快于当前 helper scalar path，是否保留生产补丁 |
| diagnostic 是否可外推到 production | 对该 helper 本身成立；不能外推为 caller public entry 的端到端收益 |
| comparison-boundary / baseline mismatch 风险 | helper 内部受控；caller 仍需要独立 public-entry 证据 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已完成 bounded probe，结果 positive；未来若复跑反转，应先标记 stale 再重跑同边界证据 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；当前不是 RVV-family-selection（RVV 实现族选择） |

## 覆盖范围与未覆盖范围

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| 默认 `spreading_size == 8` | adopted | correctness、asm 和 repeated board 都覆盖该路径。 |
| organized quantized byte map | adopted | 当前 helper 输入是连续 `QuantizedMap` byte 存储。 |
| 320x240 与 641x481 tail | adopted | 常规尺寸和非整齐宽度都为 positive。 |
| 非默认 spreading size | scalar fallback | 例如 `spreading_size=5` 的测试确认 RVV build 回退标量。 |
| 小尺寸 map | scalar fallback | 尺寸不足以覆盖 active window 时回退标量。 |
| `ColorModality` / `ColorGradientModality` public entry | unvalidated | 调用方端到端收益需另用 public entry bench 证明。 |
| `SurfaceNormalModality` public entry | separate adopted topic | surface-normal 已有私有 spread 证据，本阶段不重写该 topic 结论。 |
| `extractFeatures()` / `getSubMap()` | out of scope | 当前 helper bench 不覆盖这些路径。 |

## 阶段反思

`shared-spread-rvv-2pass` 的收益很强，说明固定 8-wide byte OR 是适合 RVV 的共享 helper 形态。继续尝试非默认 spread、caller end-to-end 或把 surface-normal 私有 spread 改回公共 helper，都需要扩大到其它 topic 或其它 public entry，因此不在当前 phase 继续推进。

当前没有值得在同一 topic 内继续自动优化的未阻塞方向：`variable-spread-rvv` 缺真实热点证据，caller 端到端收益属于 `color_modality` / `color_gradient_modality` / surface-normal 回访的独立范围。

## EvidenceDecision

`shared-spread-rvv-2pass` 在当前 Phase 000 边界下判定为 `adopted production behavior`。

可采纳理由：

- correctness（正确性）对拍通过，RVV build 命中 `Rvv` hook，fallback test 覆盖非默认 spread。
- asm attribution 能看到 byte load / OR / store RVV 指令。
- repeated board 两个 case 都为 positive，median `4.670x` / `4.770x`，无退化样本。
- Evidence Doctor 无 Error / Warning，Suggestion 不改变当前结论。

## 继续 / 停止决定

`continue_stop_decision`: stop；`stop_condition_hit`: 当前 phase 矩阵、optimization matrix 和 roadmap 在本 topic 授权范围内均已闭合，剩余方向会扩大到 caller topic、非默认 spread 新语义或其它 helper。

`next_phase_default`: `ready_for_review`。
