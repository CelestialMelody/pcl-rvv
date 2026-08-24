# Phase 000 Result: current-state and caller-shaped ablation

## 当前状态

Phase 000 已完成。已创建 topic scaffold（脚手架），并完成 QEMU correctness（正确性）、
RVV bench 反汇编、板端 smoke / repeated（重复板卡测试）和 Evidence Doctor（证据体检）。
本阶段没有修改 production（生产源码），因此所有性能数据都保持为 `production_shaped_diagnostic`
（生产形态诊断）证据，不作为 production adoption（生产采纳）结论。

## 计划执行状态

| action | 状态 | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 scaffold topic harness | done | `Makefile`, `board.mk`, `include/fpfh.h`, `include/impl/fpfh_reference.hpp`, `src/test_fpfh.cpp`, `src/bench_fpfh.cpp`, `script/generate_fpfh_evidence_manifest.py` | topic-local correctness、bench、board 和 Doctor wrapper 已建立。 |
| A2 QEMU correctness | done | `make -C test-rvv/features/fpfh run_test_compare`; `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log` | Std / RVV QEMU 均 4/4 tests passed。QEMU 只证明 correctness 和日志形状，不写性能结论。 |
| A3 asm attribution | done | `make -C test-rvv/features/fpfh dump_bench_rvv`; `build/asm/riscv/bench_fpfh_rvv.asm`, `bench_fpfh_rvv.full.asm` | RVV bench 可生成反汇编；可见 `FPFHEstimation::weightPointSPFHSignature`、`computePairFeatures` 等符号，且 RVV 指令存在。当前只能写成 attribution risk：还不能证明向量指令属于 FPFH 热循环。 |
| A4 board smoke / repeated | done | `make -C test-rvv/features/fpfh board_smoke`, `board_repeated`; `log/board/*`, `log/board/repeated/run_*/analyze_bench_compare.log` | board smoke 通过 RVV gtest 4/4；5-run repeated 完成。Std/RVV checksum 一致，但 speedup 围绕 1.00 摇摆。 |
| A5 Evidence Doctor readiness | done | `make -C test-rvv/features/fpfh evidence_doctor_repeated`; `log/board/repeated/evidence_manifest.json`, `evidence_doctor.md`, `evidence_doctor.json` | Doctor 输出 Errors=3，Warnings=0，Suggestions=9；三项 error 均为 `ba_degradation_frequency`，结论必须降级。 |

## Board Repeated Summary

| bench case | Std avg ms | RVV avg ms | avg speedup | B/A values | degradation frequency | decision bucket |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `component_spfh_signature` | 4.26872 | 4.27462 | 0.999x | 0.99x, 1.00x, 1.01x, 1.00x, 0.99x | 2/5 | `neutral/unstable` |
| `component_weighted_spfh_33` | 34.78918 | 35.21640 | 0.988x | 1.00x, 1.00x, 0.94x, 1.06x, 0.94x | 2/5 | `unstable_negative` |
| `public_fpfh_k` | 144.57940 | 144.54080 | 1.000x | 1.02x, 1.00x, 0.99x, 1.01x, 0.98x | 2/5 | `neutral/unstable` |

Evidence Doctor 的三个 Errors 都来自退化频率：每个 case 都有 2/5 runs 低于 1.00。Suggestions 还指出
缺少 taskset / governor / freq / temperature 和 binary hash（板卡环境与二进制身份）metadata，并且 median
距离 1.00 阈值不足 0.05。因此本阶段不能把 RVV build 的持平或个别单轮正向写成稳定收益。

## Diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | `production_shaped_diagnostic` for public path；component cases 是 `component_ablation`。 |
| A/B boundary | Std build vs RVV build of the same topic-local wrapper；没有新增 RVV family，没有 production dispatch。 |
| 当前决策问题 | 判断当前源码在 RVV 编译下的 baseline parity（基线持平）和下一候选优先级，不判断 production adoption。 |
| diagnostic 是否可外推到 production | 只能部分外推。public case 真实走 `FPFHEstimation::compute`，但输入是 synthetic dense cloud，点型固定为 `PointNormal -> FPFHSignature33`，且没有 production RVV patch。 |
| comparison-boundary / baseline mismatch 风险 | 中等。component case 不含 search / lookup；public case 含 KSearch，但没有新 RVV helper，所以只能说明当前编译边界没有稳定收益。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不直接允许 production probe。可以继续做 test-only / diagnostic RVV component candidate；只有 candidate correctness、asm attribution 和 board repeated 稳定正向后，才重新审计 PI1。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。当前没有 adopted RVV family，也没有 production diff。 |

## EvidenceDecision

`decision`: `continue_diagnostic`

`decision_bucket`: `neutral_or_unstable_baseline`

`production_decision`: `no_production_patch_in_phase_000`

解释：

- correctness 已通过，说明 topic scaffold 可以作为后续 candidate 的对拍基础。
- 当前 Std/RVV build 对比没有稳定性能收益，且 Evidence Doctor 把三项 repeated compare 都标成 error。
- 负向或中性 diagnostic 不能直接推出 `no-production`，但足以阻止本阶段进入 production integration loop（生产接入闭环）。
- 下一阶段应优先在 topic-local test support 中做有界 `weighted-spfh-33` RVV candidate，独立证明 33-bin 累加是否真有可采纳空间。

## Continue / Stop Decision

`continue_stop_decision`: `continue`

`next_phase_default`: `001-weighted-spfh-33-diagnostic-candidate`

当前没有命中停止条件。板卡可用，dirty isolation 仍能限定在 `test-rvv/features/fpfh`，且 roadmap
仍有未阻塞的 component-level candidate（组件级候选）。不得把 Phase 000 写成 topic closeout。
