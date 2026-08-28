# Phase 080 Plan: semi-scale-production-direct-probe

## 阶段意图和边界

本阶段只评估 `detectTemplatesSemiScaleInvariant()` 默认宏路径里的 `EnergyMaps -> LinearizedMaps` copy loop 是否应复用 Phase 070 已采纳的 `linearizeEnergyMap()` production helper。它仍是 `recognition/src/linemod.cpp` 当前 topic 内的公开入口扩展，但不把 Phase 070 的 `matchTemplates` / `detectTemplates` 证据外推到 semi-scale。

本阶段不触碰 `LINEMOD_USE_SEPARATE_ENERGY_MAPS`、score accumulation、threshold scan、NMS、averaged detection 或新 RVV family（实现族）。若接入后板卡结果弱、负、中性或不稳定，本阶段只降级 semi-scale production-public 边界，不撤回 Phase 070 默认入口已采纳行为。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 070 default entries | `matchTemplates` median `1.138x`，`detectTemplates` median `1.129x`，production-public positive。 |
| semi-scale源码 | `detectTemplatesSemiScaleInvariant()` 仍有独立默认单套 energy map linearized copy 三层循环，尚未调用 `linearizeEnergyMap()`。 |
| existing helper | `linearizeEnergyMapStd` / `linearizeEnergyMapRVV` / `linearizeEnergyMap` 已在 `linemod.cpp` 内部匿名命名空间。 |
| required RED | 新增 semi-scale production-direct bench 后，RVV asm gate 应先因为 `linearizeEnergyMapRVV` 调用点不足而失败。 |
| board availability | 用户说明板卡可用；若 RED/GREEN、本地 QEMU 或 asm 通过，本阶段继续跑 5-run board repeated。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标是 `production-public`。 |
| A/B boundary | `public_overload_current_source`，真实调用 `LINEMOD::detectTemplatesSemiScaleInvariant()`。 |
| 当前决策问题 | semi-scale public RVV path 是否快于当前 semi-scale public scalar path，以及是否值得把同一 copy helper 扩展到该入口。 |
| diagnostic 是否可外推到 production | 不外推；只使用本阶段 production-direct test、asm 和 board evidence。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 两侧必须链接当前源码；case-filter 必须只计时 semi-scale public entry。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段本身就是 bounded production probe；若结果不 positive，只撤回或暂缓 semi-scale 扩展，不影响 Phase 070。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只复用已采纳 helper，不做 family selection；若后续新增 semi-scale 专用 helper，需要 RVV-vs-RVV A/B。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| semi-scale production-public linearized map copy RVV | production quantized modality public entry | `unsigned char` energy maps to `LinearizedMaps`; no point type / Scalar | `detectTemplatesSemiScaleInvariant()` default macro path | `run_semiscale_production_direct_test_compare` | `collect_semiscale_production_direct_repeated_board` | pending 5-run Milkv-Jupiter | `dump_semiscale_production_direct_bench_rvv` must show an additional `linearizeEnergyMapRVV` call site | pending | planned |

## 实现和测试动作

1. 新增 semi-scale production-direct test / bench 支撑，构造稳定 synthetic quantized map、固定模板、`min_scale=1.0f`、`max_scale=2.0f`、`scale_multiplier=2.0f`，并验证 detection checksum 非零、scale 字段只来自预期 scale 集合。
2. 新增 Phase 080 Makefile target：Std/RVV production-direct test compare、RVV bench dump、5-run board repeated、manifest、Evidence Doctor 和 registry freshness。
3. RED：运行 `make dump_semiscale_production_direct_bench_rvv`，当前源码应因为 semi-scale 入口未调用 `linearizeEnergyMapRVV` 而失败。
4. GREEN：把 `detectTemplatesSemiScaleInvariant()` 默认单套 energy map copy loop 改为 `linearizeEnergyMap()`；`LINEMOD_USE_SEPARATE_ENERGY_MAPS` 分支继续使用原标量四套 map 循环。
5. 运行 correctness、asm、board repeated、summary / manifest / Evidence Doctor / registry。
6. 若板卡 repeated summary 为 positive 且 doctor clean，按用户规则采纳 semi-scale extension；否则只回收或暂缓 semi-scale extension，并在 result / roadmap / matrix 中记录。

## Evidence Doctor 和 registry 规则

Phase 080 的 manifest 必须使用 `production-public` evidence role，并只包含 `LINEMOD production semi-scale detectTemplates total` timing label。Evidence Doctor 结果进入：

- `doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.md`
- `doc/phases/080-semi-scale-production-direct-probe/semi-scale-production-direct-repeated-evidence-doctor.json`

registry 使用 `record_semiscale_production_direct_evidence_state` 登记 summary、manifest、doctor 和当前 topic 文档引用。

## 板卡复跑预算和决策桶

- run count: 5
- iterations: 默认沿用 `BENCH_ARGS` 第三个字段，当前为 `200`
- warmup_iterations: 5
- positive: median >= `1.05x` 且 `B/A < 1` 为 `0/5`
- weak-positive: median >= `1.00x` 且无退化，但收益不足以覆盖生产扩展维护成本
- neutral / negative / unstable: 按 Evidence Doctor 和 repeated values 降级，不做 clean adoption

## 继续 / 停止条件

本阶段若 semi-scale production-public 结果 positive，则同步 production `doc-rvv`、evaluation、roadmap、matrix、queue 和 Handoff 后停在 ready-for-review。若结果不 positive，只撤回或暂缓 semi-scale 变更，并说明 Phase 070 默认入口仍 adopted。`LINEMOD_USE_SEPARATE_ENERGY_MAPS` 仍是单独 follow-up phase。
