# Phase 000 Plan: shared-spread-rvv

## 阶段意图和边界

本阶段评估公共 `QuantizedMap::spreadQuantizedMap()` 的默认 spread 8 byte-map（字节图）扩散 helper 是否值得接入 RVV。该函数先做横向 8 byte OR 写入临时 map，再做纵向 8 行 OR 写入 output map。

validated_scope（本阶段准备证明的范围）：`QuantizedMap::spreadQuantizedMap()`、organized quantized byte map、`spreading_size == 8`、320x240 与 641x481 tail 规模、QEMU correctness、RVV 反汇编、板卡 repeated benchmark 和 Evidence Doctor。

unvalidated_scope（仍未验证范围）：非默认 spreading size 的 RVV 化、完整 `ColorModality` / `ColorGradientModality` / `SurfaceNormalModality` 公开入口端到端收益、`extractFeatures()`、`QuantizedMap::getSubMap()` 和其它容器 helper。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| queue | `doc-rvv/library-screening/recognition/recognition-retained-candidate-rescreen.zh.md` 把本文件列为 `Quantized map spread helper`。 |
| production source | `recognition/src/quantizable_modality.cpp` 原实现是两遍标量 OR window。 |
| callers | `color_modality.h`、`color_gradient_modality.h` 和 `surface_normal_modality.h` 都调用公共 helper；surface-normal 私有 spread 已在独立 topic 中证明正向。 |
| test assets | 本阶段创建 `test-rvv/recognition/quantizable_modality`，使用 `qm` 作为短标识。 |
| doc-rvv | 只有生产补丁和板卡证据正向后才适用。 |

## 假设与候选族

| candidate family | hypothesis | risk / unknown |
| --- | --- | --- |
| `shared-spread-rvv-2pass` | 两遍固定 8-wide byte OR 可按 VL chunk 批量处理，减少内层标量 OR 和循环控制成本。 | 临时 map 偏移、未写区域初值和 active width / height 边界必须完全匹配标量路径。 |
| `variable-spread-rvv` | 非默认 spread 也可能用滑动窗口或分段 OR 优化。 | 复杂度更高，当前 caller 默认主要使用 8；本阶段不扩大。 |
| `caller-end-to-end-rvv` | 共享 helper 正向后可能提高多个 modality 的 `processInputData()`。 | 端到端收益会被量化、filter、convolution 和 feature extraction 稀释，需要另开 caller scope。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `shared-spread-rvv-2pass` | organized quantized byte map | byte map / no point type / contiguous storage | `QuantizedMap::spreadQuantizedMap()` production helper | `run_test_compare`；强制 scalar/RVV 对拍；非默认 spread fallback | `bench_qm --case-filter shared_spread_*` | 5-run repeated board | `check_qm_rvv_asm` | `Errors=0 / Warnings=0` 才可 clean adopt | planned | 实现 RVV helper 并闭合证据 |
| `variable-spread-rvv` | organized quantized byte map | byte map | non-default spread | fallback test only | not covered | not covered | not covered | not covered | deferred | 只有 caller 证明非默认 spread 是热点后再启动 |
| `caller-end-to-end-rvv` | modality public entry | caller-specific point type / layout | color / color-gradient / surface-normal public entry | not covered | not covered | not covered | not covered | not covered | separate-topic | 另开 caller topic |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| scalar helper extraction | `recognition/src/quantizable_modality.cpp` | 原标量主体抽成 `spreadQuantizedMapStd()`，公开入口保持短路 RVV 后回退标量。 |
| RVV helper | `recognition/src/quantizable_modality.cpp` | `spreading_size == 8` 且尺寸足够时按 VL chunk 做两遍 byte OR。 |
| correctness | `make -C test-rvv/recognition/quantizable_modality run_test_compare` | Std/RVV gtest 通过，RVV build 命中 path hook。 |
| asm | `make -C test-rvv/recognition/quantizable_modality check_qm_rvv_asm` | 反汇编命中 `vle8`、`vor`、`vse8`。 |
| board | `SSH_AUTH_SOCK=<agent-forwarded-sock> make -C test-rvv/recognition/quantizable_modality board_repeated record_evidence_state_repeated` | 5-run repeated board、manifest、Evidence Doctor、registry 刷新。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_detail` |
| A/B boundary | `production_detail_helper`，直接调用公共 `QuantizedMap::spreadQuantizedMap()` |
| 当前决策问题 | 当前 helper RVV path 是否快于当前 helper scalar path，是否值得保留生产补丁 |
| diagnostic 是否可外推到 production | 可以外推到该 helper 本身；不能外推到完整 modality public entry 的端到端收益 |
| comparison-boundary / baseline mismatch 风险 | helper 内部边界受控；caller 端到端收益仍存在 mismatch 风险 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 helper 证据弱或负，本阶段不采纳公共 helper；可保留测试资产并另做 caller profile |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 不需要；当前不是 RVV-family-selection，而是标量 helper 到单一 RVV family 的接入 |

## 板卡复跑预算和决策桶

默认 5-run repeated，bench 参数为 `--case-filter shared_spread_320x240,shared_spread_641x481_tail --iterations 50 --warmup-iterations 5`。`positive` 要求 median speedup >= 1.20x 且 `B/A < 1` 为 0/5；`weak_positive` 为 median >= 1.05x 且退化频率不超过 1/5；低于 1.05x 或退化频率高时判为 neutral / negative。预算耗尽仍摇摆时标为 unstable（不稳定）。

## 继续 / 停止条件

若 correctness、asm 和板卡 repeated 都闭合，且 Evidence Doctor 无 Error / Warning，本轮按用户授权自动采纳并创建 `doc-rvv/recognition/quantizable_modality-RVV.zh.md`。若继续优化需要扩大到 caller 公开入口、非默认 spread 或其它 helper，则停止当前 topic，记录为 separate-topic 或 deferred。

## 文档更新清单

本阶段完成后更新 `doc/phases/000-shared-spread-rvv/result.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/quantizable_modality-evaluation.zh.md`、`README.zh.md`、`doc-rvv/recognition/quantizable_modality-RVV.zh.md`、复筛状态表和 current Handoff。
