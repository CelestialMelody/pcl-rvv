# Phase 000 Result: current-state-and-snm-scaffold

## 计划与实际

本阶段的目标是先把 `computeAndQuantizeSurfaceNormals2()` 的 production-shaped diagnostic
（生产形态诊断）闭环跑通，再决定是否值得进入 production integration loop（生产接入闭环）。
实际执行范围与计划一致：只覆盖 depth-to-normal / quantize 子链路，不触碰
`filterQuantizedSurfaceNormals()`、`QuantizedMap::spreadQuantizedMap()` 或 `extractFeatures()`。

| action | status | evidence | note |
| --- | --- | --- | --- |
| RED production-shaped test | done | `test-rvv/recognition/surface_normal_modality/src/test_snm.cpp` | 生产直连测试先失败后通过，确认 path-hit 断言有效。 |
| RVV candidate scaffold | done | `test-rvv/recognition/surface_normal_modality/include/impl/snm_surface_normal.hpp` | 先在测试支撑层完成候选链路。 |
| QEMU correctness | done | `make -C test-rvv/recognition/surface_normal_modality run_test_compare` | Std/RVV 4/4。 |
| RVV asm | done | `make -C test-rvv/recognition/surface_normal_modality check_snm_rvv_asm` | `vle16/vse32/vsub/vmul/vadd/vmslt/vmerge` 命中。 |
| board repeated | done | `log/board/repeated_phase000_depth_quantize/summary.md` | 5-run，median `1.100x` / `1.110x`，`B/A < 1 = 0/5`。 |
| Evidence Doctor | done | `log/board/repeated_phase000_depth_quantize/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4`。 |
| evidence registry | done | `log/evidence_registry.json` | 记录 repeated board 证据状态。 |

## 证据解释

- `depth_quantize_320x240` median `1.100x`，`depth_quantize_641x481_tail` median `1.110x`。
- 两个 case 的 checksum 一致，说明 candidate 与 reference 在当前 checksum 口径下对齐。
- Evidence Doctor 只给出环境 metadata 和 binary identity 缺失的 Suggestions，不阻断继续。

## Diagnostic to production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `depth-quantize-rvv` 是否值得进入 production direct |
| diagnostic 是否可外推到 production | 只能作为候选信号，不能直接当 production 结论 |
| comparison-boundary / baseline mismatch 风险 | 有；test helper 排除了 public overload、filter 和 spread |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，只要生产补丁范围小、fallback 明确且用户授权存在 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；本阶段只能回答 RVV-vs-scalar 值不值得继续 |

## 继续 / 停止

Phase 000 已完成。`depth-quantize-rvv` 的候选价值已确认，下一阶段进入
`010-production-integration`，把候选接入真实公开入口并用生产直连板卡证据重新判断。
