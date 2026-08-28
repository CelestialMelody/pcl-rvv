# Phase 040 Result: linearized-map-copy-ablation

## 执行范围

本阶段按 `plan.zh.md` 建立 `EnergyMaps -> LinearizedMaps` 8x8 offset copy（偏移拷贝）的 production-shaped diagnostic（生产形态诊断）。测试输入是 synthetic single-bin `u8` energy map，输出是 64 个 step=8 的 linearized offset maps；输出顺序与 `LinearizedMaps::operator()(map_col,map_row)` 的 `map_row * step_size + map_col` 一致。

本阶段未修改 `recognition/src/linemod.cpp`，未覆盖 `EnergyMaps` / `LinearizedMaps` 类分配、`getOffsetMap` 后续读取、energy map generation（能量图生成）、score accumulation（分数累加）、threshold scan（阈值扫描）、NMS（非极大值抑制）、averaged detection（邻域加权检测）或真实 production dispatch（生产分流）。

## 动作回填

| action | status | evidence / command | conclusion |
| --- | --- | --- | --- |
| RED：新增 linearized copy correctness | done | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.LinearizedMapCopyMatchesDefaultDetectTemplatesLayout"` 先因 `linearizeEnergyMapStd/RVV` 缺失编译失败 | 测试先于 helper 实现，失败原因符合预期 |
| GREEN：实现标量 / RVV helper | done | `include/impl/linemod_template_scoring_candidates.hpp` 中 `linearizeEnergyMapStd/RVV` | RVV helper 使用 `vlse8.v` stride load（跨步加载）和 `vse8.v` contiguous store（连续写入） |
| correctness 回归 | done | `make run_test_compare TEST_ARGS="--gtest_filter=LINEMODTemplateScoring.*"` | Std/RVV 各 5 个测试通过 |
| QEMU smoke | done | `make run_bench_std EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY" BENCH_ARGS="257 17 1 0"`；`make run_bench_rvv EXTRA_CXXFLAGS="-DLINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY" BENCH_ARGS="257 17 1 0"` | 两侧 `linearized_checksum` 一致；QEMU timing 不作为性能结论 |
| bench / manifest 支撑 | done | `src/bench_linemod_template_scoring.cpp`、`Makefile`、`script/generate_linemod_template_scoring_evidence_manifest.py` | 新增 `LINEMOD linearized map copy` timing、`linearized_checksum`、Phase 040 collect / doctor / registry target |
| asm attribution（反汇编归属） | done | `make dump_linearized_map_copy_bench_rvv`；`build/asm/riscv/bench_linemod_template_scoring_linearized_map_copy_only_rvv.full.asm` | 专用二进制中出现 `vlse8.v` / `vse8.v` / `vsetvli` |
| board repeated | done | `make collect_linearized_map_copy_repeated_board BENCH_ARGS="4096 96 200 5"` | 5-run correctness 通过，checksum / energy_checksum / linearized_checksum 一致；median speedup `2.160x` |
| Evidence Doctor / registry | done | `make run_linearized_map_copy_evidence_doctor BENCH_ARGS="4096 96 200 5"`；`make record_linearized_map_copy_evidence_state BENCH_ARGS="4096 96 200 5"`；`make check_linearized_map_copy_evidence_freshness` | Doctor `Errors=0, Warnings=0, Suggestions=0`；registry fresh |

## 诊断证据链

| evidence | result | path |
| --- | --- | --- |
| correctness | Std/RVV `LINEMODTemplateScoring.*` 各 5/5 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke | linearized-only std / rvv 小规模运行均输出同一 `linearized_checksum` | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` |
| asm attribution | linearized-only RVV bench 二进制包含 `vlse8.v`、`vse8.v` 和 `vsetvli` | `build/asm/riscv/bench_linemod_template_scoring_linearized_map_copy_only_rvv.full.asm` |
| board repeated | `LINEMOD linearized map copy` median speedup `2.160x`，min `2.033x`，max `2.266x`，degrade frequency `0/5` | `doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-summary.md` |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=0` | `doc/phases/040-linearized-map-copy-ablation/linearized-map-copy-repeated-evidence-doctor.md` |
| evidence registry | Phase 040 summary / manifest / doctor 已登记且 freshness check 为 fresh | `log/evidence_registry.json` |

板卡运行期间出现远端 Makefile clock skew warning（时间戳偏差警告）。该 warning 没有导致编译、测试、bench 或日志 fetch 失败；当前 Evidence Doctor 规则没有把它建模为 finding，因此本阶段把它作为环境备注保留，不扩大性能结论边界。

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper` |
| 当前决策问题 | `RVV-vs-scalar` for `EnergyMaps -> LinearizedMaps` copy |
| diagnostic 是否可外推到 production | `unknown`；本阶段直接传入 contiguous energy map 和 contiguous output slots，不覆盖真实 `LinearizedMaps` 分配、对象生命周期、后续 `getOffsetMap` 读取或 full detection pipeline |
| comparison-boundary / baseline mismatch 风险 | `yes`；bench 隔离 copy 核，省略 modality、energy generation、score accumulation 和 detection sink |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `yes`，但本阶段结果为 positive；进入 production 前仍需 full-chain timing split 或 production eligibility audit 证明该 copy 在真实入口中值得接入 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采用 RVV family；若后续出现 transpose/table-copy/multi-bin fused copy 等实现族，需要同边界 RVV-vs-RVV A/B |

## Matrix / Roadmap 更新

Phase 040 decision 是 `attempted-positive / partial-production-candidate`。该结果支持继续做 production eligibility audit（生产接入适用性审计）或 full-chain timing split（完整链路分阶段计时），但不支持直接修改 production 源码或写成 clean adopted。原因是 Phase 010/020/030 已显示其它子核在相同硬件上为负向；若只接入 linearized copy，真实入口收益仍取决于这段 copy 占完整 `matchTemplates` 成本的比例、对象分配成本是否被保留、以及后续 score accumulation / scan 的负向是否抵消收益。

## Continue / Stop Decision

`stop_condition_hit: none`。当前 roadmap 仍有授权且未阻塞的下一动作：`050-full-chain-timing-and-production-eligibility`，用于把 Phase 040 正向信号放回完整 LINEMOD 子阶段成本模型中。板卡可用，因此不能把“需要板卡验证”作为停止理由。
