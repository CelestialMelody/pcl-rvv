# Phase 000 Current State And PFH Scaffold Result

## 执行范围

本阶段完成 PFH topic 的初始 scaffold（脚手架）和 baseline evidence（基线证据）。未修改 `features/include/pcl/features/impl/pfh.hpp` production（生产源码）。

## 计划动作回填

| action | status | 证据 |
| --- | --- | --- |
| RED-1 failing test | done | `make -B -C test-rvv/features/pfh run_test_std` 先因 `PointT` / reference helper 缺失失败。 |
| GREEN-1 scalar reference | done | `include/impl/pfh_reference.hpp` 复刻 pair tuple、bin clamp 和 histogram scatter；`make -B -C test-rvv/features/pfh run_test_compare` 通过。 |
| BENCH-1 benchmark scaffold | done | `src/bench_pfh.cpp` 输出 `component_pfh_signature` 与 `public_pfh_k`；`dump_bench_rvv` 生成 `build/asm/riscv/bench_pfh_rvv.full.asm`。 |
| BOARD-1 smoke | done | `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'` 通过，board gtest 通过，component/public 都约 1.00x。 |
| BOARD-2 repeated | done | `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2'` 产出 5-run summary。 |
| DOCTOR-1 repeated doctor | done_with_errors | `make -C test-rvv/features/pfh evidence_doctor_repeated` 产出 `log/board/repeated/evidence_doctor.md`，Errors=2、Warnings=0、Suggestions=6。 |

## 证据摘要

| case | evidence role | B/A values | decision bucket | 解释 |
| --- | --- | --- | --- | --- |
| `component_pfh_signature` | production-shaped diagnostic baseline | `1.00x, 1.01x, 0.99x, 1.01x, 0.99x` | neutral | 当前没有 PFH RVV candidate，Std/RVV 差异只是编译宏 / 自动向量化噪声；2/5 退化不能写成收益。 |
| `public_pfh_k` | production-public baseline | `1.00x, 1.01x, 0.99x, 1.01x, 0.99x` | neutral | 公开形态同样中性，不支持生产接入。 |

Correctness（正确性）：Std/RVV QEMU gtest 均通过，board gtest 通过。Checksum（校验和）在 repeated board 的 Std/RVV 两侧一致。

QEMU：只运行 correctness 和 asm dump；未运行 QEMU bench compare，未把 QEMU timing 写成性能证据。

Asm（反汇编）：`dump_bench_rvv` 可生成 RVV binary asm。由于本阶段没有手写 PFH RVV helper，asm 只作为后续候选的归属基线，不作为优化归属证据。

Board（板卡）：Milkv-Jupiter 5-run repeated 已完成；缺少 taskset/governor/freq/temperature 和 binary hash，Doctor 以 Suggestion 记录。

## Evidence Doctor 处理

Doctor 的两个 Error 都是 `ba_degradation_frequency`：component 和 public 各 2/5 run 低于 1。处理方式是把当前 baseline 降级为 neutral，不作为 production evidence 或收益结论。它不表示 production bug，只说明没有可采纳加速。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `component_pfh_signature` 是 production-shaped diagnostic baseline；`public_pfh_k` 是 production-public baseline，但没有新增 RVV production path。 |
| A/B boundary | Std build vs RVV build，同一 bench wrapper；当前差异只来自 `__RVV10__` 构建宏和编译器行为，不是新实现族。 |
| 当前决策问题 | 是否已有无需手写的 RVV收益；答案是 no，bucket neutral。 |
| diagnostic 是否可外推到 production | 不能外推为 production adoption；它只证明当前无手写 PFH RVV 时没有收益。 |
| comparison-boundary / baseline mismatch 风险 | 有：component 不含 search/output，public 含 KdTree search。两者都只是后续候选的基线。 |
| bounded production probe 条件 | 需要 Phase 010 或后续 candidate 在 component 上有明确正向，并补 correctness、asm、board 和 Doctor。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | yes；若多个候选存在，必须同 production boundary 比较实现族。 |

## Optimization Matrix 更新

| candidate family | decision | unblocked next action |
| --- | --- | --- |
| `pfh-scalar-reference-scaffold` | adopted_as_test_baseline | 继续用作 Phase 010 candidate 对拍。 |
| `pfh-pair-feature-batch-diagnostic` | phase_deferred + unblocked | 新建 `010-pair-feature-batch-diagnostic`。 |
| `pfh-public-k-dilution-check` | adopted_as_baseline_check | 后续 candidate 正向时重跑。 |

## Continue / Stop Decision

`continue_stop_decision`: continue。停止条件未命中：板卡可用，dirty isolation 仍可按 `test-rvv/features/pfh/**` 隔离，当前 topic roadmap 仍有未阻塞的 pair-feature batch candidate。下一阶段默认入口是 `010-pair-feature-batch-diagnostic`。
