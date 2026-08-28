# Phase 040 Plan: linearized-map-copy-ablation

## 阶段意图和边界

本阶段建立 `EnergyMaps -> LinearizedMaps` 的 production-shaped diagnostic（生产形态诊断）。它复刻 `recognition/src/linemod.cpp` 中 8x8 offset copy（偏移拷贝）的核心语义：对每个 bin 的 energy map，按 `map_row` / `map_col` 选出 8x8 子采样位置，把源图上固定 stride（跨步）的一行拷贝到对应 linearized map 的连续内存中。

本阶段只修改 `test-rvv/recognition/linemod_template_scoring` 下的测试资产、bench、manifest wrapper 和 topic-local 文档，不修改 `recognition/src/linemod.cpp`。本阶段不证明 `QuantizableModality`、energy map 生成、score accumulation、threshold scan、NMS、averaged detection、semi-scale 缩放偏移或真实 production dispatch。

## 当前状态清单

| item | current state |
| --- | --- |
| source shape | `matchTemplates` 默认路径中 `step_size=8`，`lin_width=width/8`，`lin_height=height/8`；每个 `(map_col,map_row)` 的目标 map 连续写入 `lin_width * lin_height` 字节 |
| previous phases | Phase 010/020/030 分别对 scan、accumulation-only、energy map generation 得到 attempted-negative diagnostic |
| topic assets | 已有 `src/`、`include/impl`、manifest wrapper、phase docs 和 evidence registry |
| board availability | 当前 prompt 明确板卡可用；本阶段正确性、QEMU smoke、asm 后继续 5-run board repeated |
| production state | production 源码未修改；正向也只进入后续 production eligibility audit，不自动接入 |

## Validated / Unvalidated Scope

| scope type | content |
| --- | --- |
| validated_scope | synthetic single-bin `u8` energy map；`width` / `height` 可被 `step_size=8` 整除；输出 layout 与 `LinearizedMaps::operator()(map_col,map_row)` 的 `map_row * step_size + map_col` 顺序一致 |
| unvalidated_scope | 多 modality 对象生命周期、`LinearizedMaps` 类分配、`getOffsetMap` 后续读取、非 8 step、非整除宽高尾部、完整 `detectTemplates` / `detectTemplatesSemiScaleInvariant` |
| point_type_expansion_queue | not applicable；本阶段输入是图像字节 map，不涉及 PCL point type（点类型）或 `Scalar` |
| phase_closeout_boundary | 只能关闭 linearized copy diagnostic helper 的正确性、asm 和 board performance；不能关闭完整 LINEMOD production 入口 |

## 假设与候选族

| hypothesis | expected signal | required evidence |
| --- | --- | --- |
| RVV stride-load + contiguous-store 可减少 8x8 offset copy 成本 | `vlse8.v` 从源行按 `step_size` 字节跨步加载，`vse8.v` 连续写入目标行；board median speedup 达到 positive 或 weak-positive | same-chain correctness、tail、asm attribution、5-run board repeated、Evidence Doctor |
| 跨步加载成本或小行宽会抵消收益 | correctness 和 asm 通过，但 board decision bucket 为 neutral / negative | repeated summary、doctor、roadmap 中记录后续 full-chain profile |
| 当前子核不是主热点 | Phase 040 负向或弱信号叠加 Phase 010-030 负向后，应提升 full-chain timing split 优先级 | evaluation、matrix、roadmap 更新 |

## 优化矩阵

| candidate | scope | correctness | bench | asm | board | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| linearized map copy RVV | `u8` energy map -> 64 个 `u8` linearized maps，step=8，tail included by row width | planned RED/GREEN | planned dedicated timing item | planned dump | planned 5-run budget | planned | planned |

## 实现和测试动作

| action | artifact | expected evidence | completion |
| --- | --- | --- | --- |
| RED：新增 linearized copy correctness | `src/test_linemod_template_scoring.cpp` | helper 缺失导致编译失败，证明测试先于实现 | failure observed before helper edit |
| GREEN：实现标量 / RVV helper | `include/impl/linemod_template_scoring_candidates.hpp` | 输出 layout、跨步读取、非整 VL row tail 与标量 reference 一致 | `run_test_compare` pass |
| 扩展 bench harness | `src/bench_linemod_template_scoring.cpp` | 输出 `LINEMOD linearized map copy` timing 和 `linearized_checksum` | parseable logs |
| 增加 Phase 040 Make targets | `Makefile` | collect / manifest / doctor / registry / freshness target 指向 Phase 040 证据路径 | target exists and runs |
| 扩展 manifest wrapper | `script/generate_linemod_template_scoring_evidence_manifest.py` | 识别 linearized copy label、checksum 和 boundary metadata | summary + manifest |
| 反汇编检查 | `make dump_linearized_map_copy_bench_rvv` | RVV 指令能归属到 helper 或 inline candidate，至少包含 stride load / byte store | asm summary |
| 板卡 repeated | 5-run budget | 判断 positive / weak_positive / neutral / negative / unstable | summary + doctor |

## Evidence Doctor 和 Registry

本阶段 summary 主归属为 `doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-summary.md`，manifest 为 `linearized-map-copy-repeated-evidence-manifest.json`，Evidence Doctor 输出为 `linearized-map-copy-repeated-evidence-doctor.md/json`。registry run label 使用 `phase040-linearized-map-copy-repeated-20260828`。若 checksum mismatch、metadata 缺失或 Evidence Doctor Error 无法降级解释，本阶段转为 blocked。

## 板卡复跑预算和决策桶

默认 5-run。`median speedup >= 1.05` 为 `positive`，`1.00-1.05` 为 `weak_positive`，`0.97-1.00` 为 `neutral`，`<0.97` 为 `negative`；若方向跨越 1 或 Evidence Doctor 指出影响决策桶的长尾，最多再做一次同边界确认复跑。若 5-run 已稳定落入一个桶，不追加复跑。

## 继续 / 停止条件

板卡可用时不因“需要板卡验证”停止。若 Phase 040 正向，下一步是 production integration eligibility audit（生产接入适用性审计）或 full-chain timing split，不自动修改 production。若 Phase 040 中性或负向，默认继续到 production-shaped full-chain timing split，判断 energy map、linearized copy、accumulation、scan 的相对成本；只有 roadmap / matrix 无授权未阻塞动作或命中工具、证据、dirty isolation 停止条件时才停止。

## 文档更新清单

本阶段完成后更新本 `result.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、`doc/linemod_template_scoring-evaluation.zh.md`、`README.zh.md`、筛选队列和 Handoff。没有 adopted production behavior 或 PI5 生产证据闭环，因此不创建 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `RVV-vs-scalar` for `EnergyMaps -> LinearizedMaps` copy |
| diagnostic 是否可外推到 production | `unknown`；本阶段直接传入 contiguous energy map 和 contiguous output slots，不覆盖真实 `LinearizedMaps` 分配、对象生命周期、后续 `getOffsetMap` 读取或 full detection pipeline |
| comparison-boundary / baseline mismatch 风险 | `yes`；bench 隔离 copy 核，省略 modality、energy generation、score accumulation 和 detection sink |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，但必须先有 full-chain timing split 证明 copy 是真实入口热点，并且 production patch 能保持 step/layout/fallback 边界 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采用 RVV family；若后续出现 transpose/table-copy/multi-bin fused copy 等实现族，需要同边界 RVV-vs-RVV A/B |
