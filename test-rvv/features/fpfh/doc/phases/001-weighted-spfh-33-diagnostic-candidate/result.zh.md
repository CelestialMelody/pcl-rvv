# Phase 001 Result: weighted-spfh-33 diagnostic candidate

## 当前状态

Phase 001 已完成。新增 `weighted-spfh-33` 的 test-only RVV candidate（仅测试 RVV 候选），并完成
TDD red/green、QEMU correctness（正确性）、asm attribution（反汇编归因）、board smoke / repeated
和 Evidence Doctor（证据体检）。production（生产源码）仍未修改。

## 计划执行状态

| action | 状态 | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 RED correctness test | done | `make -C test-rvv/features/fpfh run_test_compare` | 先看到 `weightPointSPFHDenseRowsRVV` 不存在的编译失败，证明新增测试能卡住未实现候选。 |
| A2 GREEN candidate | done | `include/impl/fpfh_weighted_candidate.hpp`, `include/fpfh.h` | Std 构建回退 `weightPointSPFHReference`；RVV 构建仅在 dense sequential 11+11+11 bin 边界走 `weightPointSPFHDenseRowsRvvKernel`。 |
| A3 bench label | done | `src/bench_fpfh.cpp` | 新增 `candidate_weighted_spfh_dense_rows`，独立于 `component_weighted_spfh_33` baseline。 |
| A4 QEMU correctness | done | `make -C test-rvv/features/fpfh run_test_compare`; `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log` | Std / RVV QEMU 均 5/5 tests passed。 |
| A5 asm attribution | done | `make -C test-rvv/features/fpfh dump_bench_rvv`; `addr2line` on `0x2fc4c`, `0x2fc5c`, `0x2fc60` | `vsetvli`、`vlse32.v`、`vfmacc.vf` 归到 `include/impl/fpfh_weighted_candidate.hpp:75-78`。 |
| A6 board smoke / repeated | done | `make -C test-rvv/features/fpfh board_smoke`; `make -C test-rvv/features/fpfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/phase001-weighted-candidate/repeated` | board smoke 5/5 tests passed；candidate repeated 5/5 正向。 |
| A7 Evidence Doctor | done | `make -C test-rvv/features/fpfh evidence_doctor_repeated ...phase001...`; `log/board/phase001-weighted-candidate/repeated/evidence_doctor.md` | Doctor: Errors=1, Warnings=2, Suggestions=11。唯一 Error 属于旧 `component_weighted_spfh_33` baseline；candidate label 无 error，仅 metadata / binary identity suggestions。 |

## Board Repeated Summary

| bench case | Std avg ms | RVV avg ms | avg speedup | B/A values | degradation frequency | decision bucket |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `candidate_weighted_spfh_dense_rows` | 12.45420 | 6.87842 | 1.811x | 1.77x, 1.79x, 1.84x, 1.87x, 1.78x | 0/5 | `strong_diagnostic_positive` |
| `component_weighted_spfh_33` | 34.56632 | 35.41974 | 0.976x | 0.93x, 1.01x, 1.01x, 1.00x, 0.94x | 2/5 | `unstable_baseline` |
| `component_spfh_signature` | 4.42396 | 4.41894 | 1.001x | 0.99x, 1.00x, 1.01x, 1.00x, 1.00x | 1/5 | `neutral_baseline` |
| `public_fpfh_k` | 145.14540 | 144.85180 | 1.002x | 0.99x, 1.00x, 1.01x, 1.01x, 1.00x | 1/5 | `neutral_public` |

## Diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | `candidate_weighted_spfh_dense_rows` 是 `component-candidate` diagnostic；其它 case 是 baseline / public dilution checks。 |
| A/B boundary | candidate: Std build scalar reference fallback vs RVV build test-only RVV helper；production public path 未接入。 |
| 当前决策问题 | 是否值得进入 PI1 production integration plan（生产接入计划），不是是否采纳。 |
| diagnostic 是否可外推到 production | 只可外推到 dense sequential SPFH row 的 33-bin weighted accumulation。生产中的 remapped row indices、fallback dispatch 和 public path 尚未验证。 |
| comparison-boundary / baseline mismatch 风险 | 高。candidate scalar baseline 不是当前 production helper，且 public path 不调用 candidate。 |
| weak / neutral / unstable 情况处理 | 旧 baseline 和 public path 仍是 neutral / unstable，说明 candidate 收益尚未转化为公开入口收益。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要。必须先有 production patch、fallback、public/direct correctness、board repeated 和 PI5 用户确认。 |

## EvidenceDecision

`decision`: `production_integration_checkpoint_required`

`decision_bucket`: `strong_diagnostic_positive_component_candidate`

`production_decision`: `not_adopted_no_production_diff`

解释：

- 33-bin dense-row component candidate 在板端 repeated 上稳定约 1.81x，且 candidate label 没有 Doctor error。
- asm attribution 已归到 topic-local candidate header，说明这不是 Eigen / stdlib 噪声。
- public path 仍约 1.00x，因为 production 没有接入 candidate；该结果不能否定 candidate，但阻止直接 claim public speedup。
- 进入 production integration loop 需要修改 `features/include/pcl/features/impl/fpfh.hpp`，按仓库规则必须先停在用户检查点。

## Continue / Stop Decision

`continue_stop_decision`: `stop_for_user_checkpoint`

`stop_condition_hit`: 继续推进需要 production patch（生产补丁）用户确认。

`next_phase_default`: `PI1-weighted-spfh-33-production-integration-plan`

下一步建议：若用户确认进入 PI1，只做 bounded production probe（有界生产探针），先冻结 dense sequential row gate、
fallback scalar path、public/direct correctness target 和 repeated board 证据，不把 Phase 001 直接写成 adopted。
