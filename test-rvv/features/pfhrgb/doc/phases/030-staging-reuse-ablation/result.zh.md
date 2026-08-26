# Phase 030 Staging Reuse Ablation Result

## 当前结论

Phase 030 已完成本地 correctness（正确性）、bench build / log-shape smoke（构建 / 日志形状小型验证）、
ASM（反汇编）和 Milkv-Jupiter 5-run repeated board（板卡重复测试）。`pfhrgb-staging-reuse`
现在是 positive production-shaped diagnostic（正向生产形态诊断）：`public_pfhrgb_k_with_candidate_reuse`
中位数 `1.25x`，最低 `1.22x`，0/5 低于 1。它仍不是 production direct（真实生产路径证据），因为
`features/include/pcl/features/impl/pfhrgb.hpp` 没有修改。

同一批 Evidence Doctor（证据体检）报 Errors=2 / Warnings=0 / Suggestions=6。两个 Error 都来自
component/helper 级 case 的退化频率，要求降级旧的 helper-only 收益外推；它们不直接推翻
public-shaped reuse case，但会阻止把 fixed-neighborhood helper 写成独立 production value。

本阶段没有修改 `features/include/pcl/features/impl/pfhrgb.hpp`，也没有新增 production dispatch（生产分流）。

## 计划回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| RED-030 | done | `make -B -C test-rvv/features/pfhrgb run_test_rvv` | 新增测试先因 `computePublicPFHRGBWithReusablePairBatchCandidate` 缺失失败，失败点符合预期。 |
| GREEN-030 | done | `include/impl/pfhrgb_pair_batch_candidate.hpp`、`src/test_pfhrgb.cpp` | 新增 `PairBatchWorkspace`，在 public-shaped wrapper（公开入口形态包装器）外层循环复用 staging 和 tuple buffers。 |
| TEST-030 | done | `make -B -C test-rvv/features/pfhrgb run_test_compare` | Std/RVV 两侧各 4 个 gtest 全部通过；reusable public-shaped candidate 与真实 estimator descriptor 对拍通过。 |
| BENCH-030 | done | `src/bench_pfhrgb.cpp` 新增 `public_pfhrgb_k_with_candidate_reuse`；Std/RVV QEMU smoke 均输出该 label 和相同 checksum `3.2128e+06`。 | QEMU 只证明新增 case 可编译、可运行、输出可解析；性能结论来自 board repeated。 |
| ASM-030 | done | `make -B -C test-rvv/features/pfhrgb dump_bench_rvv` | `build/asm/riscv/bench_pfhrgb_rvv.asm` 仍包含 `vsetvli`、`vle32.v`、`vfmacc.vv`、`vfdiv.vv` 等 RVV 指令。 |
| BOARD-030 | done | `make -C test-rvv/features/pfhrgb REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 板卡 SSH 已恢复；manifest 包含 `public_pfhrgb_k_with_candidate_reuse`。 |
| DOCTOR-030 | done with downgraded component claims | `log/board/repeated/evidence_doctor.md` | 当前 Errors=2 / Warnings=0 / Suggestions=6；Errors 来自 `candidate_pfhrgb_pair_batch_rvv` 与 `component_pfhrgb_signature` 的 B/A 退化频率。 |

## Repeated Board 统计

| case | role | Std mean | RVV mean | B/A values | bucket | 解释 |
| --- | --- | --- | --- | --- | --- | --- |
| `public_pfhrgb_k_with_candidate_reuse` | production-shaped diagnostic | `565.0126 ms` | `455.4444 ms` | `1.25, 1.25, 1.22, 1.24, 1.25` | positive | reusable workspace 在 public-shaped KSearch wrapper 内稳定正向。 |
| `public_pfhrgb_k_with_candidate` | production-shaped diagnostic | `568.2984 ms` | `470.2156 ms` | `1.21, 1.20, 1.21, 1.21, 1.22` | positive | 非复用 wrapper 仍稳定正向，支撑 PI1。 |
| `public_pfhrgb_k` | production-shaped diagnostic | `615.4144 ms` | `609.2912 ms` | `1.00, 1.02, 1.01, 1.01, 1.00` | neutral | production 未接入 RVV；该 case 只说明当前公开入口基本无变化。 |
| `candidate_pfhrgb_pair_batch_rvv` | diagnostic | `67.9985 ms` | `70.4262 ms` | `0.98, 0.94, 0.97, 0.97, 0.98` | negative-current-rerun | helper-only 边界本轮退化；历史正向不能继续作为当前 truth。 |
| `component_pfhrgb_signature` | production-shaped diagnostic | `68.0928 ms` | `68.5011 ms` | `1.00, 0.99, 0.99, 0.99, 0.99` | neutral-negative | component 背景不是收益证据。 |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `public_pfhrgb_k_with_candidate_reuse` 是 production-shaped diagnostic，不是 production direct。 |
| A/B boundary | 当前只有 topic-local public-shaped wrapper；没有真实 production helper 或 public overload（公开重载）分流。 |
| 当前决策问题 | implementation-shape（实现形态）和 allocation-staging ablation（分配 / 暂存消融）。 |
| diagnostic 是否可外推到 production | unknown。它复用了 KSearch 外层循环和 descriptor 输出形态，但 wrapper scalar side 与真实 `PFHRGBEstimation::compute` 不同，不能直接外推。 |
| comparison-boundary / baseline mismatch 风险 | yes。reuse case 与非复用 case 是同一 topic-local wrapper family 的 A/B 线索，但真实 production dispatch、对象状态和 fallback 仍未验证。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes。当前 component/helper 级结果为负向或中性，但 public-shaped reuse 仍稳定正向；允许条件是用户确认 PI1、production patch 保持 exact `PointXYZRGBNormal` / `float` / `nr_split=5` gate，并补 production direct correctness、fallback、ASM 和 repeated board。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。reuse 消融即便板卡正向，也只说明 topic-local wrapper 的实现形态更好；生产采纳仍需真实 production boundary。 |

## 证据分层

| 层级 | 当前证据 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `run_test_compare` Std/RVV 各 4 tests passed。 | reusable workspace 没破坏 descriptor 输出、histogram 写回和非 RVV fallback。 | 不证明 production dispatch 已接入。 |
| QEMU smoke | `log/qemu/run_bench_std_reuse_smoke.log`、`log/qemu/run_bench_rvv_reuse_smoke.log`。 | 新 bench label 可运行，Std/RVV checksum 一致。 | QEMU timing 不证明性能。 |
| ASM | `build/asm/riscv/bench_pfhrgb_rvv.asm`。 | RVV binary 仍包含候选所需的向量指令。 | 目前只证明二进制指令存在，未做更细符号级热点归属。 |
| board performance | `log/board/repeated/evidence_manifest.json`。 | reuse public-shaped case 为 positive，且比非复用 wrapper 的中位数 `1.21x` 略高。 | 不证明真实 production dispatch 或泛型点型收益。 |
| Evidence Doctor | `log/board/repeated/evidence_doctor.md`。 | 暴露 component/helper 退化 Error 和环境 metadata suggestion。 | Error 不应被忽略；helper-only 结论必须降级。 |

## Optimization Matrix 更新

`pfhrgb-staging-reuse` 从 `correctness-and-build-ready / board-blocked` 更新为
`positive production-shaped diagnostic`。关闭范围仅限 exact `pcl::PointXYZRGBNormal`、`float`、
AoS xyz / normal / rgb、synthetic dense finite cloud、`nr_split=5`、KSearch row source 的
topic-local diagnostic helper。未验证范围包括 production source、泛型点型、`Scalar=double`、
其它 row source 和真实 fallback。

`pfhrgb-color-pair-batch-rvv` 和 `pfhrgb-component-baseline` 根据本轮 Evidence Doctor 降级为
`attempted / negative-current-rerun` 与 `attempted / neutral-negative`。历史 positive 数值只作为
Phase 000/010 historical baseline，不再作为当前 production-value 证据。

## Evidence Doctor 与 Registry

`evidence_doctor_repeated` 重新生成 `log/board/repeated/evidence_manifest.json`、`evidence_doctor.md`
和 `evidence_doctor.json`。这些文件覆盖五个 case，包含 `public_pfhrgb_k_with_candidate_reuse`。
`log/qemu/run_bench_*_reuse_smoke.log` 是本阶段 QEMU smoke raw log，默认 local-only，不作为性能
summary 提交边界。当前 board raw logs 仍默认 local-only；summary / Doctor / registry 可作为摘要证据
进入 topic 产物候选。

## Continue / Stop Decision

`continue_stop_decision`: continue to Phase 040 doc-suite parity, then stop at production checkpoint if production source remains unconfirmed.

`stop_condition_hit`: Phase 030 性能证据已闭合；继续进入 Phase 020 PI1 仍需要用户明确确认修改 production source，因此当前 worker 不能自行接入生产。

`next_phase_default`: 先完成 Phase 040 topic-local doc suite parity；若用户明确确认 PI1，则切回 `020-pi1-production-integration-plan`。
