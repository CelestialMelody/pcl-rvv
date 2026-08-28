# Phase 030 Result: energy-map-generation

## 执行范围

本阶段按 `plan.zh.md` 建立 energy map generation（能量图生成）的 production-shaped diagnostic（生产形态诊断）。测试输入是一段 synthetic `QuantizedMap` 等价 `u8` 字节流，输出是默认合并 energy maps：8 个 bin，每个像素按 `val0..val3` 四个 mask（掩码）命中次数得到 `0..4` 的 `u8` 值。

本阶段未修改 `recognition/src/linemod.cpp`，未覆盖 `LINEMOD_USE_SEPARATE_ENERGY_MAPS` 编译分支，也未覆盖 `EnergyMaps -> LinearizedMaps` 拷贝、score accumulation（分数累加）、threshold scan（阈值扫描）、NMS（非极大值抑制）、averaged detection（邻域加权检测）或真实 production dispatch（生产分流）。

## 动作回填

| action | status | evidence / command | conclusion |
| --- | --- | --- | --- |
| RED：新增 energy map correctness | done | 历史 RED 已观察：新增测试先引用缺失 helper，随后实现 helper 后纳入 `LINEMODTemplateScoring.*` 对拍 | 测试能约束 8-bin mask 语义和 tail |
| GREEN：实现标量 / RVV helper | done | `include/impl/linemod_template_scoring_candidates.hpp` 中 `buildEnergyMapsStd/RVV` | helper 只服务测试资产，不进入 production |
| 扩展 bench harness | done | `src/bench_linemod_template_scoring.cpp` 输出 `LINEMOD energy map generation` 与 `energy_checksum` | `LINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY` 不再输出无关 scan/index 字段 |
| 增加 Phase 030 Make targets | done | `Makefile` 中 `dump_energy_map_bench_rvv`、`collect_energy_map_repeated_board`、`generate_energy_map_evidence_manifest`、`run_energy_map_evidence_doctor`、`record_energy_map_evidence_state`、`check_energy_map_evidence_freshness` | Phase 030 证据有独立 target 和登记入口 |
| 扩展 manifest wrapper | done | `script/generate_linemod_template_scoring_evidence_manifest.py --include-label "LINEMOD energy map generation"` | summary 只包含 energy map timing，避免混入其它子阶段 |
| QEMU correctness | done | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` | Std/RVV 4 个测试均通过 |
| QEMU smoke | done | `make run_bench_std EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY" BENCH_ARGS="257 17 1 0"`；`make run_bench_rvv EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY" BENCH_ARGS="257 17 1 0"` | 只证明日志形状和 checksum 一致，不作为性能结论 |
| asm attribution（反汇编归属） | done | `make dump_energy_map_bench_rvv`；`build/asm/riscv/bench_linemod_template_scoring_energy_map_only_rvv.full.asm` | RVV 指令存在：`vle8.v`、`vand.vx/vi`、`vmsne.vi`、`vmerge.vim`、`vadd.vv`、`vse8.v` |
| board repeated | done | `make collect_energy_map_repeated_board BENCH_ARGS="4096 96 200 5"` | 5-run checksum / energy_checksum 一致，性能 5/5 退化 |
| Evidence Doctor / registry | done | `make run_energy_map_evidence_doctor BENCH_ARGS="4096 96 200 5"`；`make record_energy_map_evidence_state BENCH_ARGS="4096 96 200 5"` | Doctor 报 `Errors=1`，Error 是退化频率；registry 已登记 4 个摘要证据文件 |

## 诊断证据链

| evidence | result | path |
| --- | --- | --- |
| correctness | Std/RVV `LINEMODTemplateScoring.*` 4/4 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke | energy-only std / rvv 小规模运行均输出同一 `energy_checksum` | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` |
| asm attribution | energy-only RVV bench 二进制包含目标 byte mask / compare / add / store 指令 | `build/asm/riscv/bench_linemod_template_scoring_energy_map_only_rvv.full.asm` |
| board repeated | `LINEMOD energy map generation` median speedup `0.954x`，min `0.947x`，max `0.963x`，degrade frequency `5/5` | `doc/phases/030-energy-map-generation/energy-map-repeated-summary.md` |
| Evidence Doctor | `Errors=1, Warnings=0, Suggestions=0`；Error 为 `ba_degradation_frequency` | `doc/phases/030-energy-map-generation/energy-map-repeated-evidence-doctor.md` |
| evidence registry | Phase 030 summary / manifest / doctor 已登记 | `log/evidence_registry.json` |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `RVV-vs-scalar` for default energy map generation |
| diagnostic 是否可外推到 production | `unknown`；本阶段只覆盖 synthetic `QuantizedMap` 字节流到默认合并 energy maps，不覆盖 modality 对象、separate-energy 编译分支、LinearizedMaps 拷贝、score accumulation、NMS、averaging 或真实 production dispatch |
| comparison-boundary / baseline mismatch 风险 | `yes`；bench 直接传入字节流并写连续输出数组，省略 `QuantizableModality::getSpreadedQuantizedMap`、`EnergyMaps` 分配和后续 `LinearizedMaps` 构造 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，但当前 Phase 030 不支持直接 production probe；只有后续 full-chain timing split 证明该子阶段是主热点，且 production patch 能保持默认 / separate-energy 分支边界时才恢复 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采用 RVV family；若后续出现 table-lookup、single-pass multi-bin 或 separate-map 多实现族，需要同边界 RVV-vs-RVV A/B |

## Evidence Doctor 处理

Evidence Doctor 的 `ba_degradation_frequency` 在本阶段代表负向性能证据，而不是 correctness failure。5 次板卡复跑全部低于 1，且 decision bucket（决策桶）稳定为 `negative`，因此不追加复跑。本阶段不进入 production integration loop（生产接入闭环），也不把该负向 diagnostic 外推成完整 LINEMOD no-go。

## Matrix / Roadmap 更新

Phase 030 decision 是 `attempted-negative / continue-next-phase`。`energy map generation RVV` 在当前 diagnostic boundary 下不采纳；下一阶段默认进入 `040-linearized-map-copy-ablation`，验证 `EnergyMaps -> LinearizedMaps` 的 8x8 offset copy 是否有可独立收益或是否进一步证明 full-chain profile 更应优先。

## Continue / Stop Decision

`stop_condition_hit: none`。当前 topic 的 roadmap 仍有 `linearized map copy RVV / layout ablation` 这一授权且未阻塞的下一动作，板卡可用，因此不能在 Phase 030 后停止。默认下一阶段：`040-linearized-map-copy-ablation`。
