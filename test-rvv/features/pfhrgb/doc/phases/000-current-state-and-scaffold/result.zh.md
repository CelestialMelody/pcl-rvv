# Phase 000 Current State And PFHRGB Scaffold Result

## 结果摘要

Phase 000 已完成。它建立了 `features/include/pcl/features/impl/pfhrgb.hpp` 的 topic-local（主题本地）测试、bench（性能测试）、manifest（证据清单）和 Evidence Doctor（证据体检）入口；没有修改 production（生产源码）。

当前 EvidenceDecision（证据决策）已由后续 Phase 030/040 刷新为
`partial-production-candidate / PI1-plan-ready`。Phase 000 的原始五轮数字只作为 historical run
（历史运行），不再是当前 truth（当前事实）。当前 summary 显示 helper-only case 退化，但
public-shaped candidate 和 reusable workspace 仍稳定正向；详见
`030-staging-reuse-ablation/result.zh.md` 和 `optimization-matrix.zh.md`。

## 计划执行回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED-1 same-chain reference test | done | `make -B -C test-rvv/features/pfhrgb run_test_std` 曾先因 include 入口和 protected helper 使用方式失败，随后用 `PFHRGBEstimation::compute` 形成正确 RED。 | 测试能捕获 reference 未实现 / 语义不一致。 |
| GREEN-1 scalar reference | done | `make -B -C test-rvv/features/pfhrgb run_test_compare` | Std/RVV 两侧各 2 个 gtest 通过；reference 复刻 production 有向 pair order、颜色 ratio、bin clamp 和 histogram scatter。 |
| BENCH-1 diagnostic bench | done | `make -B -C test-rvv/features/pfhrgb dump_bench_rvv` | bench 可构建，反汇编中存在 `vle32.v`、`vfmacc.vv`、`vfdiv.vv` 等 RVV 指令。 |
| CANDIDATE-1 pair-batch RVV | done | `make -B -C test-rvv/features/pfhrgb run_test_rvv`、`run_test_compare` | 候选与 reference 在当前 PointXYZRGBNormal / float / AoS / `nr_split=5` 边界内一致。 |
| BOARD-1 smoke | done | `make -C test-rvv/features/pfhrgb board_smoke` | board correctness 2/2 通过；候选 smoke 为 `1.24x`，public-like 为 `1.00x`。 |
| BOARD-2 repeated + Doctor | done / superseded | `make -C test-rvv/features/pfhrgb REPEATED_BOARD_RUNS=5 board_repeated evidence_doctor_repeated` | 五轮 repeated 完成；后续 Phase 030/040 rerun 已覆盖 `log/board/repeated/`，当前 manifest 含五个 case。 |

## Correctness / QEMU / ASM / Board 分层结论

| evidence layer | 当前证据 | 证明范围 | 不证明范围 |
| --- | --- | --- | --- |
| correctness | `run_test_compare` Std/RVV 两侧通过。 | test reference、public descriptor smoke 和 pair-batch RVV 候选在当前 synthetic finite 输入上一致。 | 泛型 RGB 点型、非 dense 输入、非默认 bin、production dispatch。 |
| QEMU | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` 由 compare target 刷新。 | 构建、链接、测试路径和日志形状。 | 性能结论。 |
| ASM | `build/asm/riscv/bench_pfhrgb_rvv.asm` 中可见 RVV 指令。 | RVV build 的候选路径产生向量指令。 | 指令一定来自真实 production 入口；当前仍是 test-only candidate。 |
| board performance | `log/board/repeated/run_*/analyze_bench_compare.log` 与 manifest。 | Milkv-Jupiter 上当前 synthetic case 的重复性能趋势。 | 任意数据分布、真实 production patch、其它点型或 row source。 |

## Repeated Board 统计

| case | role | Std mean | RVV mean | B/A values | bucket | 解释 |
| --- | --- | --- | --- | --- | --- | --- |
| `candidate_pfhrgb_pair_batch_rvv` | diagnostic | `67.9985 ms` | `70.4262 ms` | `0.98, 0.94, 0.97, 0.97, 0.98` | negative-current-rerun | 当前 rerun 退化；历史 positive 已降级。 |
| `component_pfhrgb_signature` | production-shaped diagnostic | `68.0928 ms` | `68.5011 ms` | `1.00, 0.99, 0.99, 0.99, 0.99` | neutral-negative | 当前 rerun 触发退化频率 Error，不能写成稳定收益。 |
| `public_pfhrgb_k` | production-shaped diagnostic | `615.4144 ms` | `609.2912 ms` | `1.00, 1.02, 1.01, 1.01, 1.00` | neutral | 当前没有 production patch，因此只说明公开入口基本未变化。 |

## Evidence Doctor 回填

输入：`test-rvv/features/pfhrgb/log/board/repeated/evidence_manifest.json`。

当前 `log/board/repeated/` 已由 Phase 030/040 rerun 覆盖，结果：Errors=2，Warnings=0，Suggestions=6。
Phase 000 原始正向 helper 结果和 Phase 010 expanded result 均降级为历史信号。

| severity | item | 处理动作 | 对结论影响 |
| --- | --- | --- | --- |
| Suggestion | `environment_metadata_missing` 四项 | 记录缺少 taskset、governor、freq、temperature。 | 不阻塞当前 diagnostic 结论；PI1 或 production evidence 前应补 metadata 或继续写明环境边界。 |
| Suggestion | `near_threshold_ba — component_pfhrgb_signature` | component 维持 neutral，不写成收益。 | 阻止把标量背景 case 写成 clean positive。 |
| Suggestion | `near_threshold_ba — public_pfhrgb_k` | public-like 维持 neutral，不写成收益。 | 支持 Phase 010/PI1 继续分层。 |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `candidate_pfhrgb_pair_batch_rvv` 是 diagnostic；`component_pfhrgb_signature` 和 `public_pfhrgb_k` 是 production-shaped diagnostic。 |
| A/B boundary | candidate 是 test helper；component 是 production-shaped helper；public-like 是 test bench 中的 `PFHRGBEstimation::compute` wrapper，但没有 production RVV patch。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 implementation-shape（实现形态），不是 clean adoption。 |
| diagnostic 是否可外推到 production | 不能直接外推。候选正向只说明 pair math + RGB ratio + scalar scatter 在 staged test helper 中值得继续；public-like 中性说明 search/output 总成本会稀释收益。 |
| comparison-boundary / baseline mismatch 风险 | 有。Std build 的 candidate case 走 scalar fallback，RVV build 走 test-only RVV helper；public-like case 当前两侧都没有生产 RVV patch。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许 topic-local bounded probe：先在测试资产里建立 public-with-candidate 外层循环和同边界 bench。若仍中性或退化，再暂停 production 接入。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。若后续进入生产源码，必须补 production direct correctness、fallback、asm、board repeated 和 Evidence Doctor；PI5 后还要用户确认采纳或回滚。 |

## Evidence Registry 状态

`log/evidence_registry.json` 已更新为 summary-only（仅摘要）策略，当前登记 `log/board/repeated/evidence_manifest.json`、`log/board/repeated/evidence_doctor.md` 和 `log/board/repeated/evidence_doctor.json`。`log/board/repeated/run_*` raw logs 默认 local-only，不作为默认提交候选。Manifest 记录 bench binary SHA-256；环境 taskset / governor / freq / temperature 仍缺失。

当前 freshness（新鲜度）判断：Phase 000 的 repeated 结果已经是 historical evidence（历史证据）；当前 truth（当前事实）以 Phase 030/040 rerun 覆盖后的 manifest、Evidence Doctor、matrix、roadmap 和 evaluation 为准。早期 smoke 结果只作为历史信号。

## Phase Scope 与未验证范围

| area | validated_scope | unvalidated_scope | next action |
| --- | --- | --- | --- |
| point type | exact `pcl::PointXYZRGBNormal` | 泛型 RGB 点型、`PointXYZRGB + Normal` 分离输入、RGBA、非标准布局 | 进入 production 前必须读泛型点类型策略并列出 fallback。 |
| Scalar | `float` | `double` 或其它标量模板实例 | 当前 PFHRGB signature 是 float histogram；生产接入时仍需写清。 |
| row source | fixed wrapped neighborhood indices 与 synthetic public KSearch | 真实数据分布、indices 边界、OMP、不同 search policy | Phase 010 先补 public-with-candidate diagnostic。 |
| layout | AoS xyz / normal / rgb 字段读取，候选内部转 SoA staging | direct AoS RVV load、减少 staging 分配、cache reuse | Phase 010/020 比较 staging 与 public 外层成本。 |

## Continue / Stop Decision

`continue_stop_decision`: continue。

`stop_condition_hit`: none at Phase 000。后续 Phase 010 已完成并停在 production checkpoint。

`next_phase_default`: Phase 010 已完成；当前默认恢复为 `020-pi1-production-integration-plan`。

默认下一动作：Phase 010 已完成；当前恢复到 Phase 020 PI1 checkpoint，用户确认后再修改 production source。
