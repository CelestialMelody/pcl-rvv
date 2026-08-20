# Phase 000 result: staged-window diagnostic

## 当前结论

本阶段完成 `staged-window-reduction` 接入前诊断。Correctness（正确性）通过，反汇编能归属到候选 RVV 指令，但最新 Milkv-Jupiter 板卡复跑仍稳定负向：三个 case 的 Std/RVV speedup 分别为 `0.90x`、`0.91x`、`0.90x`。因此不建议把当前 staged-window-reduction 候选接入 `surface/include/pcl/surface/impl/bilateral_upsampling.hpp`。

当前 EvidenceDecision（证据决策）：`rollback/no-production`，含义是当前候选不进入 production（生产源码）。这不是对整个 `bilateral_upsampling` 主题永久拒绝；若后续继续，应另开 `column-stride direct load` 或更少 staging 的 phase，而不是把本候选直接生产化。

## 计划回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| scaffold | done | `Makefile`、`board.mk`、`include/`、`src/`、topic docs | 已按 `src/`、`include/`、`include/impl/` 布局创建。 |
| correctness | done | `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | QEMU std/RVV 两侧各 5 个测试通过。 |
| asm | done | `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | `bench` lambda 附近可见 `vle32.v`、`vfmul.vv`、`vfredusum.vs`，归属到暂存规约候选。 |
| board | done | `make -C test-rvv/surface/bilateral_upsampling board_smoke` | 板卡 RVV 均慢于 Std，decision bucket 为 `negative`。 |
| doctor | done-with-negative-errors | `doc/phases/000-current-state-and-diagnostic-staged-window/evidence_manifest.json` -> `evidence-doctor.md` | Doctor 的 `ba_degradation_frequency` Errors 被解释为不接 production 的证据，不作为待修正 positive evidence。 |

## 证据分层

Correctness：`run_test_compare` 覆盖表生成、全 NaN fallback、dense cloud、NaN holes 和窗口边界。RVV 与标量误差在 `max_abs_xyz <= 6e-6`、`rmse_xyz <= 2e-6` 范围内。

QEMU path evidence（QEMU 路径证据）：QEMU 只证明 std/RVV 构建和测试路径可运行；没有运行 QEMU bench compare，也没有把 QEMU timing 写成性能结论。

Disassembly evidence（反汇编证据）：`build/asm/riscv/bench_bilateral_upsampling_rvv.full.asm` 中 candidate lambda 附近出现 `vsetvli e32,m2`、`vle32.v`、`vfmul.vv`、`vfredusum.vs`。该证据说明 RVV 候选被编译进二进制；它不证明性能正向。

Board performance（板卡性能）：`log/board/analyze_bench_compare.log` 来自 Milkv-Jupiter，5 iterations、2 warmup。

| case | Std avg | RVV avg | speedup | bucket |
| --- | ---: | ---: | ---: | --- |
| `80x60 w3 dense` | 4.9110 ms | 5.4789 ms | 0.90x | negative |
| `120x90 w4 holes` | 18.9948 ms | 20.9199 ms | 0.91x | negative |
| `180x120 w5 dense` | 61.9880 ms | 68.7636 ms | 0.90x | negative |

Evidence Doctor（证据体检）：脚本读取 topic-local manifest 后报告 `Errors=3`、`Warnings=9`。三个 Error 都是 `ba_degradation_frequency`，表示候选在所有 case 低于 1.0x；本阶段处理动作是降级为 `rollback/no-production`，而不是继续把它解释成 production positive。九个 Warning 来自 wrapper、timer boundary 和 reduction 口径不同；这是本阶段有意的 diagnostic A/B 设计，也进一步说明不能外推成 production evidence。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar，判断当前 staged-window-reduction 是否值得进入 production probe。 |
| diagnostic 是否可外推到 production | no，用于 positive 不能外推；用于 negative 只能说明当前 staged 形态不值得接入。 |
| comparison-boundary / baseline mismatch 风险 | yes，RVV 侧多了 staging 和 vfredusum chunk reduction。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前不建议直接 probe。只有先设计更少 staging 的 direct-load 候选并通过同边界诊断，才恢复 production probe 条件。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。 |

## 继续 / 停止决定

`continue_stop_decision`: stop_for_user_confirmation。

`stop_condition_hit`: 继续会扩大到新的候选 family 或 production integration loop，需要用户确认。当前候选已被负向证据关闭；没有理由把它直接接入 production。

`next_phase_default`: 若用户要求继续探索而不是接源码，建议 phase 010 `column-stride-direct-load-probe`，目标是减少本阶段暴露出的 staging 成本；若用户只问是否接源码，默认回答“不建议接入当前实现”。
