# Phase 010 Result: production-integration

## 计划与实际

本阶段把 `computeAndQuantizeSurfaceNormals2()` 从 production-shaped diagnostic
推进到 production direct（真实生产路径）证据。实际执行范围与计划一致：
只接入 `SurfaceNormalModality<PointInT>::processInputData()` 中的
`computeAndQuantizeSurfaceNormals2()` 子链路，`filterQuantizedSurfaceNormals()`、
`QuantizedMap::spreadQuantizedMap()` 和 `extractFeatures()` 仍保持原有标量实现。

| action | status | evidence | note |
| --- | --- | --- | --- |
| production patch | done | `recognition/include/pcl/recognition/surface_normal_modality.h` | 抽出 `computeAndQuantizeSurfaceNormals2Std()`，新增 RVV helper 和入口分流。 |
| production direct correctness | done | `make -C test-rvv/recognition/surface_normal_modality run_test_compare` | Std/RVV 4/4，forced scalar/RVV 生产直连测试通过。 |
| production RVV asm | done | `make -C test-rvv/recognition/surface_normal_modality check_snm_production_rvv_asm` | full asm 命中 `computeAndQuantizeSurfaceNormals2RVV()`，精简 asm 命中 RVV 指令。 |
| board repeated | done | `log/board/repeated_phase010_production_direct/summary.md` | 5-run，median `1.060x` / `1.060x`，`B/A < 1 = 0/5`。 |
| Evidence Doctor | done | `log/board/repeated_phase010_production_direct/evidence_doctor.md` | `Errors=0 / Warnings=0 / Suggestions=4`。 |
| evidence registry | done | `log/evidence_registry.json` | 已登记 production_direct 证据。 |

## 证据解释

- `production_process_320x240` median `1.060x`，`production_process_641x481_tail` median `1.060x`。
- 两个 case 的 checksum 一致，`0/5` 退化，decision bucket 为 `weak_positive`。
- Evidence Doctor 只有环境 metadata / binary identity 的 Suggestions，没有 Error 或 Warning。

## Diagnostic to production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_direct` |
| A/B boundary | `public_overload` |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path |
| diagnostic 是否可外推到 production | 已由生产直连证据直接闭合，不再依赖 diagnostic 外推 |
| comparison-boundary / baseline mismatch 风险 | 受控；同一 public entry、同一输入、同一计时边界 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮已完成 bounded probe，且结果为 weak_positive |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前不是 family selection；只需 RVV-vs-scalar 证据 |

## 结论

`computeAndQuantizeSurfaceNormals2()` 的 production direct 证据达到用户授权的采纳门槛，
当前实现可视为 adopted production behavior（已采纳生产行为）。长期文档和队列状态已经按
production_direct 归档。下一阶段若继续优化，默认候选是 `filterQuantizedSurfaceNormals()` 的
RVV 化；它仍是独立 phase，不会被本阶段结果自动覆盖。
